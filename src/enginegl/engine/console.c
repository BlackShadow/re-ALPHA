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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "quakedef.h"

#define CON_TEXTSIZE     0x4000
#define CON_X            8

static char		*con_text;
static int		con_linewidth;
int				con_totallines;
static int		con_current;
static int		con_x;
int				con_backscroll;
static int		con_vislines;
static int		con_notifylines;
static float	con_times[4];

int				con_initialized;
static int		con_debuglog;
static int		con_cr_pending;
static int		con_update_lock;

int				con_forcedup;
static float	con_cursorspeed = 4.0f;
static cvar_t	con_notifytime = { "con_notifytime", "3" };
static char		con_debug_buffer[4096];

extern char		key_lines[32][256];
extern int		key_linepos;
extern int		edit_line;

extern char		chat_buffer[32];
extern int		chat_bufferlen;
extern qboolean	team_message;

static void Con_DebugLog(const char *fileName, const char *fmt, ...)
{
	va_list argptr;
	int fileHandle;

	va_start(argptr, fmt);
	vsprintf(con_debug_buffer, fmt, argptr);
	va_end(argptr);

	fileHandle = _open(fileName, _O_WRONLY | _O_CREAT | _O_APPEND, _S_IREAD | _S_IWRITE);
	if (fileHandle != -1)
	{
		_write(fileHandle, con_debug_buffer, (unsigned int)strlen(con_debug_buffer));
		_close(fileHandle);
	}
}

static void Con_Linefeed(void)
{
	++con_current;
	con_x = 0;

	Q_memset(con_text + con_linewidth * (con_current % con_totallines), ' ', con_linewidth);
}

static void Con_DrawInput(void)
{
	int i;
	int x;
	int y;
	char *text;
	int cursorChar;
	const int rowHeight = draw_chars->rowHeight;

	if (key_dest != key_console && !con_forcedup)
		return;

	text = key_lines[edit_line];

	cursorChar = (((int)(con_cursorspeed * realtime)) & 1) + 10;
	text[key_linepos] = (char)cursorChar;

	if (key_linepos + 1 < con_linewidth)
		memset(&text[key_linepos + 1], ' ', (unsigned int)(con_linewidth - (key_linepos + 1)));

	if (con_linewidth <= key_linepos)
		text += key_linepos - con_linewidth + 1;

	x = CON_X;
	y = con_vislines - rowHeight - 2;

	for (i = 0; i < con_linewidth; i++)
		x += Draw_Character(x, y, (unsigned char)text[i]);

	text[key_linepos] = 0;
}

static void Con_MessageMode_f(void)
{
	key_dest = key_message;
	team_message = false;
}

static void Con_MessageMode2_f(void)
{
	key_dest = key_message;
	team_message = true;
}

void Con_ToggleConsole_f(void)
{
	if (key_dest == key_console)
	{
		if (cls.state == ca_connected)
		{
			key_dest = key_game;
			key_linepos = 1;
			key_lines[edit_line][1] = 0;
		}
		else
		{
			M_Menu_Main_f();
		}
	}
	else
	{
		key_dest = key_console;
	}

	SCR_EndLoadingPlaque();
	Con_ClearNotify();
}

void Con_Clear_f(void)
{
	if (con_text)
		Q_memset(con_text, ' ', CON_TEXTSIZE);
}

void Con_ClearNotify(void)
{
	con_times[0] = 0.0f;
	con_times[1] = 0.0f;
	con_times[2] = 0.0f;
	con_times[3] = 0.0f;
}

void Con_CheckResize(void)
{
	int newWidth;
	int oldWidth;
	int oldTotalLines;
	int numLines;
	int numChars;
	int i, j;
	char temp[CON_TEXTSIZE];
	char *dst;

	newWidth = ((int)vid.conwidth >> 3) - 2;
	if (newWidth == con_linewidth)
		return;

	if (newWidth < 1)
	{
		con_linewidth = 38;
		con_totallines = 431;
		Q_memset(con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldTotalLines = con_totallines;
		oldWidth = con_linewidth;

		con_linewidth = newWidth;
		con_totallines = CON_TEXTSIZE / newWidth;

		numLines = oldTotalLines;
		if (numLines > con_totallines)
			numLines = con_totallines;

		numChars = oldWidth;
		if (numChars > con_linewidth)
			numChars = con_linewidth;

		Q_memcpy(temp, con_text, CON_TEXTSIZE);
		Q_memset(con_text, ' ', CON_TEXTSIZE);

		if (numLines > 0)
		{
			dst = con_text + con_linewidth * (con_totallines - 1);
			for (i = 0; i < numLines; i++)
			{
				if (numChars > 0)
				{
					const int srcOffset = oldWidth * ((oldTotalLines - i + con_current) % oldTotalLines);
					for (j = 0; j < numChars; j++)
						dst[j] = temp[srcOffset + j];
				}
				dst -= con_linewidth;
			}
		}

		Con_ClearNotify();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

void Con_Init(void)
{
	char fileName[1004];

	con_debuglog = COM_CheckParm("-condebug");
	if (con_debuglog && (1000 - (int)strlen("qconsole.log") > (int)strlen(com_gamedir)))
	{
		sprintf(fileName, "%s%s", com_gamedir, "qconsole.log");
		_unlink(fileName);
	}

	con_text = (char *)Hunk_AllocName(CON_TEXTSIZE, "context");
	Q_memset(con_text, ' ', CON_TEXTSIZE);

	con_linewidth = -1;
	Con_CheckResize();

	Con_Printf("Console initialized.\n");

	Cvar_RegisterVariable(&con_notifytime);
	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand("messagemode", Con_MessageMode_f);
	Cmd_AddCommand("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand("clear", Con_Clear_f);

	con_initialized = 1;
}

void Con_Print(const char *txt)
{
	unsigned char *text;
	int mask;
	int c;
	int i;
	int l;

	con_backscroll = 0;
	text = (unsigned char *)txt;

	if (*text == 1)
	{
		mask = 0x80;
		S_LocalSound("common/talk.wav");
		text++;
	}
	else if (*text == 2)
	{
		mask = 0x80;
		text++;
	}
	else
	{
		mask = 0;
	}

	c = (signed char)*text;
	if (!*text)
		return;

	while (*text)
	{
		for (i = 0; i < con_linewidth; i++)
		{
			if ((signed char)text[i] <= ' ')
				break;
		}

		if (i != con_linewidth && i + con_x > con_linewidth)
			con_x = 0;

		text++;

		if (con_cr_pending)
		{
			--con_current;
			con_cr_pending = 0;
		}

		if (!con_x)
		{
			Con_Linefeed();
			if (con_current >= 0)
				con_times[con_current & 3] = (float)realtime;
		}

		if (c == '\n')
		{
			con_x = 0;
		}
		else if (c == '\r')
		{
			con_cr_pending = 1;
			con_x = 0;
		}
		else
		{
			l = con_x;
			c = (unsigned char)(c | mask);
			con_x++;
			con_text[con_linewidth * (con_current % con_totallines) + l] = (char)c;
			if (con_x >= con_linewidth)
				con_x = 0;
		}

		c = (signed char)*text;
	}
}

void Con_Printf(const char *fmt, ...)
{
	va_list argptr;
	char msg[4096];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	msg[sizeof(msg) - 1] = 0;
	va_end(argptr);

	Sys_Printf("%s", msg);

	if (con_debuglog)
		Con_DebugLog(va("%s/qconsole.log", com_gamedir), "%s", msg);

	if (con_initialized && cls.state)
	{
		Con_Print(msg);

		if (cls.signon != SIGNONS && !scr_disabled_for_loading && !con_update_lock)
		{
			con_update_lock = 1;
			SCR_UpdateScreen();
			con_update_lock = 0;
		}
	}
}

void Con_DPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[4096];

	if (developer.value == 0.0f)
		return;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	msg[sizeof(msg) - 1] = 0;
	va_end(argptr);

	Con_Printf("%s", msg);
}

void Con_SafePrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[1024];
	const int saved = scr_disabled_for_loading;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	msg[sizeof(msg) - 1] = 0;
	va_end(argptr);

	scr_disabled_for_loading = 1;
	Con_Printf("%s", msg);
	scr_disabled_for_loading = saved;
}

void Con_DrawNotify(void)
{
	int i;
	int y;
	const int rowHeight = draw_chars->rowHeight;

	y = 0;

	for (i = con_current - 3; i <= con_current; i++)
	{
		if (i >= 0)
		{
			const float time = con_times[i & 3];
			if (time != 0.0f)
			{
				const float age = (float)realtime - time;
				if ((double)con_notifytime.value >= (double)age)
				{
					int x = CON_X;
					const char *line = con_text + con_linewidth * ((i % con_totallines + con_totallines) % con_totallines);
					int j;

					for (j = 0; j < con_linewidth; j++)
						x += Draw_Character(x, y, (unsigned char)line[j]);

					y += rowHeight;
				}
			}
		}
	}

	if (key_dest == key_message)
	{
		int x;
		int iChar;

		x = Draw_String(CON_X, y, (unsigned char *)"say:");
		for (iChar = 0; chat_buffer[iChar]; iChar++)
			x += Draw_Character(x, y, (unsigned char)chat_buffer[iChar]);

		Draw_Character(8 * iChar + 40, y, (unsigned char)((((int)(con_cursorspeed * realtime)) & 1) + 10));
		y += rowHeight;
	}

	if (con_notifylines < y)
		con_notifylines = y;
}

void Con_DrawConsole(int lines, qboolean drawinput)
{
	int rows;
	int y;
	int i;
	const int rowHeight = draw_chars->rowHeight;

	if (lines <= 0)
		return;

	Draw_ConsoleBackground(lines);
	con_vislines = lines;

	rows = (lines - 16) / rowHeight;
	y = lines - rows * rowHeight - 16;

	for (i = con_current - rows + 1; i <= con_current; i++, y += rowHeight)
	{
		int line = i - con_backscroll;
		const char *src;
		int x;
		int j;

		if (line < 0)
			line = 0;

		src = con_text + con_linewidth * (line % con_totallines);

		x = CON_X;
		for (j = 0; j < con_linewidth; j++)
			x += Draw_Character(x, y, (unsigned char)src[j]);
	}

	if (drawinput)
		Con_DrawInput();
}
