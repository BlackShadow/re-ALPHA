/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

// console.c -- the drop down console and the notify lines

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "quakedef.h"

#define CON_TEXTSIZE		16384
#define CON_DEFAULTWIDTH	38		// line width until the video mode is known
#define CON_MARGIN			8		// left edge of the text, in pixels
#define NUM_CON_TIMES		4		// notify lines
#define CON_CURSOR_CHAR		10		// 10 and 11 alternate to blink the cursor
#define MAXGAMEDIRLEN		1000
#define MAXPRINTMSG			4096

static char		*con_text;
static int		con_linewidth;		// characters across screen
int				con_totallines;		// total lines in console scrollback
static int		con_current;		// where next message will be printed
static int		con_x;				// column in the current line for the next print
int				con_backscroll;		// lines up from bottom to display
static int		con_vislines;
static int		con_notifylines;	// scan lines to clear for notify lines
static float	con_times[NUM_CON_TIMES];	// realtime time the line was generated
											// for transparent notify lines

int				con_initialized;
static int		con_debuglog;
static int		con_cr;				// a \r is pending: overwrite the current line
static int		con_printing;		// guards against SCR_UpdateScreen recursion

int				con_forcedup;
static float	con_cursorspeed = 4.0f;
static cvar_t	con_notifytime = { "con_notifytime", "3" };	// seconds

static char		con_logbuffer[MAXPRINTMSG];

/*
================
Con_DebugLog

Appends to the -condebug log file.
================
*/
static void Con_DebugLog(const char *file, const char *fmt, ...)
{
	va_list	argptr;
	int		fd;

	va_start(argptr, fmt);
	vsprintf(con_logbuffer, fmt, argptr);
	va_end(argptr);

	fd = _open(file, _O_WRONLY | _O_CREAT | _O_APPEND, _S_IREAD | _S_IWRITE);
	if (fd != -1)
	{
		_write(fd, con_logbuffer, strlen(con_logbuffer));
		_close(fd);
	}
}

/*
===============
Con_Linefeed
===============
*/
static void Con_Linefeed(void)
{
	con_current++;
	con_x = 0;
	Q_memset(&con_text[(con_current % con_totallines) * con_linewidth], ' ', con_linewidth);
}

/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
static void Con_DrawInput(void)
{
	int		i;
	int		x, y;
	char	*text;
	const int	rowHeight = draw_chars->rowHeight;

	if (key_dest != key_console && !con_forcedup)
		return;		// don't draw anything

	text = key_lines[edit_line];

	// add the cursor frame
	text[key_linepos] = (((int)(con_cursorspeed * realtime)) & 1) + CON_CURSOR_CHAR;

	// fill out remainder with spaces
	if (key_linepos + 1 < con_linewidth)
		memset(&text[key_linepos + 1], ' ', con_linewidth - (key_linepos + 1));

	// prestep if horizontally scrolling
	if (con_linewidth <= key_linepos)
		text += key_linepos - con_linewidth + 1;

	// draw it
	x = CON_MARGIN;
	y = con_vislines - rowHeight - 2;

	for (i = 0; i < con_linewidth; i++)
		x += Draw_Character(x, y, text[i]);

	// remove cursor
	// FIXME: text has been advanced when scrolling, so this misses the cursor
	text[key_linepos] = 0;
}

/*
================
Con_MessageMode_f
================
*/
static void Con_MessageMode_f(void)
{
	key_dest = key_message;
	team_message = false;
}

/*
================
Con_MessageMode2_f
================
*/
static void Con_MessageMode2_f(void)
{
	key_dest = key_message;
	team_message = true;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f(void)
{
	if (key_dest == key_console)
	{
		if (cls.state == ca_connected)
		{
			key_dest = key_game;
			key_linepos = 1;
			key_lines[edit_line][1] = 0;	// clear any typing
		}
		else
		{
			M_Menu_Main_f();
		}
	}
	else
		key_dest = key_console;

	SCR_EndLoadingPlaque();
	Con_ClearNotify();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f(void)
{
	if (con_text)
		Q_memset(con_text, ' ', CON_TEXTSIZE);
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify(void)
{
	int		i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize(void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];
	char	*dst;

	width = ((int)vid.conwidth >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)	// video hasn't been initialized yet
	{
		con_linewidth = CON_DEFAULTWIDTH;
		con_totallines = CON_TEXTSIZE / CON_DEFAULTWIDTH;
		Q_memset(con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldtotallines = con_totallines;
		oldwidth = con_linewidth;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / width;

		numlines = oldtotallines;
		if (numlines > con_totallines)
			numlines = con_totallines;

		numchars = oldwidth;
		if (numchars > con_linewidth)
			numchars = con_linewidth;

		Q_memcpy(tbuf, con_text, CON_TEXTSIZE);
		Q_memset(con_text, ' ', CON_TEXTSIZE);

		// copy the newest lines to the bottom of the new buffer
		if (numlines > 0)
		{
			dst = con_text + con_linewidth * (con_totallines - 1);
			for (i = 0; i < numlines; i++)
			{
				if (numchars > 0)
				{
					const int	src = oldwidth * ((oldtotallines - i + con_current) % oldtotallines);

					for (j = 0; j < numchars; j++)
						dst[j] = tbuf[src + j];
				}
				dst -= con_linewidth;
			}
		}

		Con_ClearNotify();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

/*
================
Con_Init
================
*/
void Con_Init(void)
{
	char	temp[MAXGAMEDIRLEN + 1];

	con_debuglog = COM_CheckParm("-condebug");

	if (con_debuglog && MAXGAMEDIRLEN - (int)strlen("qconsole.log") > (int)strlen(com_gamedir))
	{
		// FIXME: no path separator, so this deletes the wrong file
		sprintf(temp, "%s%s", com_gamedir, "qconsole.log");
		_unlink(temp);
	}

	con_text = Hunk_AllocName(CON_TEXTSIZE, "context");
	Q_memset(con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize();

	Con_Printf("Console initialized.\n");

	//
	// register our commands
	//
	Cvar_RegisterVariable(&con_notifytime);

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand("messagemode", Con_MessageMode_f);
	Cmd_AddCommand("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand("clear", Con_Clear_f);
	con_initialized = true;
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print(const char *txt)
{
	const char	*text;
	int			x;
	int			c, l;
	int			mask;

	con_backscroll = 0;
	text = txt;

	// 1 and 2 mark a chat message: printed in the alternate color, 1 also beeps
	if (text[0] == 1)
	{
		mask = 128;
		S_LocalSound("common/talk.wav");
		text++;
	}
	else if (text[0] == 2)
	{
		mask = 128;
		text++;
	}
	else
		mask = 0;

	c = *text;
	if (!c)
		return;

	while (*text)
	{
		// count word length
		for (l = 0; l < con_linewidth; l++)
		{
			if (text[l] <= ' ')
				break;
		}

		// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth))
			con_x = 0;

		text++;

		if (con_cr)
		{
			con_current--;
			con_cr = false;
		}

		if (!con_x)
		{
			Con_Linefeed();
			// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current & (NUM_CON_TIMES - 1)] = realtime;
		}

		if (c == '\n')
		{
			con_x = 0;
		}
		else if (c == '\r')
		{
			con_cr = true;
			con_x = 0;
		}
		else
		{
			// display character and advance
			x = con_x;
			con_x++;
			con_text[(con_current % con_totallines) * con_linewidth + x] = c | mask;
			if (con_x >= con_linewidth)
				con_x = 0;
		}

		c = *text;
	}
}

/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
void Con_Printf(const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	msg[sizeof(msg) - 1] = 0;
	va_end(argptr);

	// also echo to debugging console
	Sys_Printf("%s", msg);

	// log all messages to file
	if (con_debuglog)
		Con_DebugLog(va("%s/qconsole.log", com_gamedir), "%s", msg);

	if (!con_initialized || cls.state == ca_dedicated)
		return;		// no graphics mode

	// write it to the scrollable buffer
	Con_Print(msg);

	// update the screen if the console is displayed
	if (cls.signon != SIGNONS && !scr_disabled_for_loading)
	{
		// protect against infinite loop if something in SCR_UpdateScreen calls
		// Con_Printf
		if (!con_printing)
		{
			con_printing = true;
			SCR_UpdateScreen();
			con_printing = false;
		}
	}
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf(const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if (developer.value == 0)
		return;		// don't confuse non-developers with techie stuff...

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	msg[sizeof(msg) - 1] = 0;
	va_end(argptr);

	Con_Printf("%s", msg);
}

/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf(const char *fmt, ...)
{
	va_list		argptr;
	char		msg[1024];
	int			temp;

	temp = scr_disabled_for_loading;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	msg[sizeof(msg) - 1] = 0;
	va_end(argptr);

	scr_disabled_for_loading = true;
	Con_Printf("%s", msg);
	scr_disabled_for_loading = temp;
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify(void)
{
	int		x, v;
	int		i;
	float	time;
	const int	rowHeight = draw_chars->rowHeight;

	v = 0;
	for (i = con_current - NUM_CON_TIMES + 1; i <= con_current; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i & (NUM_CON_TIMES - 1)];
		if (time == 0)
			continue;
		time = (float)realtime - time;
		if (con_notifytime.value >= time)
		{
			const char	*text = con_text + con_linewidth * ((i % con_totallines + con_totallines) % con_totallines);
			int			j;

			x = CON_MARGIN;
			for (j = 0; j < con_linewidth; j++)
				x += Draw_Character(x, v, text[j]);

			v += rowHeight;
		}
	}

	if (key_dest == key_message)
	{
		x = Draw_String(CON_MARGIN, v, (unsigned char *)"say:");
		for (i = 0; chat_buffer[i]; i++)
			x += Draw_Character(x, v, chat_buffer[i]);
		Draw_Character((i + 5) * 8, v, (((int)(con_cursorspeed * realtime)) & 1) + CON_CURSOR_CHAR);
		v += rowHeight;
	}

	if (con_notifylines < v)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if asked
================
*/
void Con_DrawConsole(int lines, qboolean drawinput)
{
	int		i, y;
	int		rows;
	const int	rowHeight = draw_chars->rowHeight;

	if (lines <= 0)
		return;

	// draw the background
	Draw_ConsoleBackground(lines);

	// draw the text
	con_vislines = lines;

	rows = (lines - 16) / rowHeight;		// rows of text to draw
	y = lines - rows * rowHeight - 16;		// may start slightly negative

	for (i = con_current - rows + 1; i <= con_current; i++, y += rowHeight)
	{
		int			line = i - con_backscroll;
		const char	*text;
		int			x;
		int			j;

		if (line < 0)
			line = 0;
		text = con_text + con_linewidth * (line % con_totallines);

		x = CON_MARGIN;
		for (j = 0; j < con_linewidth; j++)
			x += Draw_Character(x, y, text[j]);
	}

	// draw the input prompt, user text, and cursor if desired
	if (drawinput)
		Con_DrawInput();
}
