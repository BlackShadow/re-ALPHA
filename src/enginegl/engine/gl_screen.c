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
// gl_screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "nullsubs.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full


*/

float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;
int			scr_drawdialog;
int			scr_initialized;		// ready to draw
int			scr_loading;
int			scr_disabled_for_loading;
int			scr_fullupdate;
int			scr_sb_lines;
double		scr_disabled_time;
char		*scr_centerstring_ptr;	// dialog message
int			scr_drawloading;
float		scr_con_current;
int			scr_copyeverything;

int			scr_disk_state;
int			scr_disk_counter;
int			scr_disk_offset;
float		scr_disk_time;

static qpic_t	*scr_pause_pic;
static qpic_t	*scr_net_pic;
static qpic_t	*scr_turtle_pic;
static int		scr_turtle_count;

char		scr_centerstring[1024];

cvar_t		scr_viewsize = {"viewsize", "120", true, false};
cvar_t		scr_fov = {"fov", "90"};	// 10 - 170
cvar_t		scr_conspeed = {"scr_conspeed", "300"};
cvar_t		scr_centertime = {"scr_centertime", "2"};
cvar_t		scr_printspeed = {"scr_printspeed", "8"};
cvar_t		scr_showram = {"scr_showram", "1"};
cvar_t		scr_showturtle = {"scr_showturtle", "0"};
cvar_t		scr_showpause = {"scr_showpause", "1"};

vrect_t		scr_vrect;

void SCR_ScreenShot_f(void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint(char *str)
{
	char	*s;

	strncpy(scr_centerstring, str, sizeof(scr_centerstring) - 1);
	scr_centerstring[sizeof(scr_centerstring) - 1] = 0;
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl_time;

	// count the number of lines for centering
	scr_center_lines = 1;
	if (scr_centerstring[0])
	{
		for (s = scr_centerstring; *s; s++)
		{
			if (*s == '\n')
				scr_center_lines++;
		}
	}
}

/*
==============
SCR_DrawCenterString
==============
*/
void SCR_DrawCenterString(void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

	// the finale prints the characters one at a time
	if (cl_intermission)
		remaining = (cl_time - scr_centertime_start) * scr_printspeed.value;
	else
		remaining = 9999;

	if (scr_centertime_off <= 0 && !cl_intermission)
		return;

	start = scr_centerstring;

	if (scr_center_lines > 4)
		y = 48;
	else
		y = vid.height * 0.35f;

	while (*start)
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
		{
			if (start[l] == '\n' || !start[l])
				break;
		}
		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
		{
			Draw_Character(x, y, start[j]);
			if (cl_intermission && --remaining <= 0)
				return;
		}

		y += 8;

		start += l;
		if (*start == '\n')
			start++;	// skip the \n
	}
}

/*
==============
SCR_CheckDrawCenterString
==============
*/
void SCR_CheckDrawCenterString(void)
{
	scr_erase_center = 1;
	if (scr_erase_lines < scr_center_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if ((scr_centertime_off > 0 || cl_intermission) && !scr_drawdialog)
		SCR_DrawCenterString();
}

//=============================================================================

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f(void)
{
	HUD_ViewZoomDecrease();
	vid.recalc_refdef = 1;
}

/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f(void)
{
	HUD_ViewZoomIncrease();
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init(void)
{
	Cvar_RegisterVariable(&scr_viewsize);
	Cvar_RegisterVariable(&scr_fov);
	Cvar_RegisterVariable(&scr_conspeed);
	Cvar_RegisterVariable(&scr_centertime);
	Cvar_RegisterVariable(&scr_printspeed);
	Cvar_RegisterVariable(&scr_showram);
	Cvar_RegisterVariable(&scr_showturtle);
	Cvar_RegisterVariable(&scr_showpause);

	//
	// register our commands
	//
	Cmd_AddCommand("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);

	scr_pause_pic = Draw_PicFromWad("lambda");
	scr_net_pic = Draw_PicFromWad("lambda");
	scr_turtle_pic = Draw_PicFromWad("lambda");

	scr_initialized = true;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
void SCR_CalcRefdef(void)
{
	float	size;
	int		sb_lines;
	float	scale;

	scr_fullupdate = 0;	// force a background redraw
	vid.recalc_refdef = 0;

	scr_viewsize.value = 120;

	// bound field of view
	if (scr_fov.value < 10)
		Cvar_SetValue("fov", 10);
	if (scr_fov.value > 170)
		Cvar_SetValue("fov", 170);

	// intermission is always full screen
	if (cl_intermission)
		size = 120;
	else
		size = scr_viewsize.value;

	if (size < 120)
	{
		sb_lines = 24;	// no inventory
		if (size < 110)
			sb_lines = 24 + 16 + 8;
	}
	else
		sb_lines = 0;	// no status bar at all

	scr_sb_lines = sb_lines;

	if (size >= 100)
		size = 100;

	scale = size / 100;
	scr_vrect.width = vid.width * scale;
	if (scr_vrect.width < 96)
	{
		size = scr_vrect.width;
		scr_vrect.width = 96;	// min for icons
		scale = 96 / size;
	}

	scr_vrect.height = vid.height * scale;
	if ((int)vid.height - sb_lines < scr_vrect.height)
		scr_vrect.height = vid.height - sb_lines;

	scr_vrect.x = ((int)vid.width - scr_vrect.width) / 2;
	scr_vrect.y = ((int)vid.height - sb_lines - scr_vrect.height) / 2;

	r_refdef_vrect_x = scr_vrect.x;
	r_refdef_vrect_y = scr_vrect.y;
	r_refdef_vrect_width = scr_vrect.width;
	r_refdef_vrect_height = scr_vrect.height;

	r_refdef.vrect = scr_vrect;
}

/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f(void)
{
	byte	*buffer;
	char	tganame[80];
	char	checkname[128];
	int		i, c, temp;
	FILE	*f;

	//
	// find a file name to save it to
	//
	strcpy(tganame, "quake00.tga");

	for (i = 0; i <= 99; i++)
	{
		tganame[5] = i / 10 + '0';
		tganame[6] = i % 10 + '0';
		sprintf(checkname, "%s/%s", com_gamedir, tganame);
		f = fopen(checkname, "rb");
		if (!f)
			break;	// file doesn't exist
		fclose(f);
	}
	if (i == 100)
	{
		Con_Printf("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
	}

	buffer = malloc(3 * (glwidth * glheight + 6));	// 18 byte header and 24 bit pixels
	memset(buffer, 0, 18);
	buffer[2] = 2;	// uncompressed type
	buffer[12] = glwidth & 255;
	buffer[13] = glwidth >> 8;
	buffer[14] = glheight & 255;
	buffer[15] = glheight >> 8;
	buffer[16] = 24;	// pixel size

	glReadPixels(glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18);

	// swap rgb to bgr
	c = 3 * (glwidth * glheight + 6);
	for (i = 18; i < c; i += 3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}
	COM_WriteFile(tganame, buffer, c);

	free(buffer);
	Con_Printf("Wrote %s\n", tganame);
}

//=============================================================================

/*
===============
SCR_BeginLoadingPlaque
===============
*/
void SCR_BeginLoadingPlaque(void)
{
	S_StopAllSounds(true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

	// redraw with no console and the loading plaque
	Con_ClearNotify();
	scr_centertime_start = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	scr_loading = true;
	scr_erase_center = 0;
	SCR_UpdateScreen();
	scr_disabled_for_loading = true;
	scr_erase_center = 0;
	scr_disabled_time = realtime;
}

/*
===============
SCR_EndLoadingPlaque
===============
*/
void SCR_EndLoadingPlaque(void)
{
	scr_copyeverything = 0;
	scr_drawloading = false;
	scr_loading = false;
	scr_disabled_for_loading = false;
	Con_ClearNotify();
}

//=============================================================================

/*
===============
SCR_DrawCenterStringLines

Draws the dialog message centered on the screen
===============
*/
void SCR_DrawCenterStringLines(void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;

	start = scr_centerstring_ptr;

	y = vid.height * 0.35;

	while (*start)
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
		{
			if (start[l] == '\n' || !start[l])
				break;
		}
		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
			Draw_Character(x, y, start[j]);

		y += 8;

		if (!*start)
			break;
		do
		{
			if (*start == '\n')
				break;
			start++;
		} while (*start);

		if (!*start)
			break;
		start++;	// skip the \n
	}
}

/*
==================
SCR_DrawDialog

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
qboolean SCR_DrawDialog(char *text)
{
	if (cls.state == ca_dedicated)
		return true;

	scr_centerstring_ptr = text;

	// draw a fresh screen
	scr_erase_center = 0;
	scr_drawdialog = true;
	SCR_UpdateScreen();
	scr_drawdialog = false;

	S_ClearBuffer();	// so dma doesn't loop current sound

	do
	{
		key_count = -1;	// wait for a key down and up
		Sys_SendKeyEvents();
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE);

	scr_erase_center = 0;
	SCR_UpdateScreen();

	return key_lastpress == 'y';
}

/*
==============
SCR_FillRect

Clears the area around the view above a full status bar
==============
*/
void SCR_FillRect(void)
{
	if (scr_vrect.x > 0)
	{
		// left
		Draw_Fill(0, 0, scr_vrect.x, VIRTUAL_HEIGHT - 48, 0);
		// right
		Draw_Fill(scr_vrect.x + scr_vrect.width, 0, scr_vrect.width - scr_vrect.x + VIRTUAL_WIDTH, VIRTUAL_HEIGHT - 48, 0);
	}
	if (scr_vrect.height < VIRTUAL_HEIGHT - 48)
	{
		// top
		Draw_Fill(scr_vrect.x, 0, scr_vrect.width, scr_vrect.y, 0);
		// bottom
		Draw_Fill(scr_vrect.x, scr_vrect.y + scr_vrect.height, scr_vrect.width, VIRTUAL_HEIGHT - 48 - scr_vrect.y - scr_vrect.height, 0);
	}
}

/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole(void)
{
	float	half;

	Con_CheckResize();

	if (scr_drawloading)
		return;	// never a console with loading plaque

	// decide on the height of the console
	con_forcedup = !cl_worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_con_current = vid.height;	// full screen
	}
	else if (key_dest == key_console)
	{
		half = vid.height >> 1;	// half screen
		if (scr_con_current < half)
		{
			scr_con_current += (float)(scr_conspeed.value * host_frametime);
			if (scr_con_current > half)
				scr_con_current = half;
		}
		else if (scr_con_current > half)
		{
			scr_con_current -= (float)(scr_conspeed.value * host_frametime);
			if (scr_con_current < half)
				scr_con_current = half;
		}
	}
	else if (scr_con_current > 0)
	{
		scr_con_current -= (float)(scr_conspeed.value * host_frametime);
		if (scr_con_current < 0)
			scr_con_current = 0;
	}

	if (scr_con_current > 0)
		scr_fullupdate = 0;
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading(void)
{
	if (scr_drawloading)
		Draw_BeginDisc();
}

/*
==============
SCR_DrawRam
==============
*/
void SCR_DrawRam(void)
{
	if (scr_showram.value && cl_paused)
		Draw_BeginDisc();
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet(void)
{
	if (!scr_net_pic)
		return;

	if (realtime - cl.last_received_message >= 0.3 && !cls.demoplayback)
		Draw_Pic(scr_vrect.x + 64, scr_vrect.y, scr_net_pic);
}

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle(void)
{
	if (!scr_turtle_pic || !scr_showturtle.value)
		return;

	if (host_frametime >= 0.1)
	{
		if (++scr_turtle_count >= 3)
			Draw_Pic(scr_vrect.x, scr_vrect.y, scr_turtle_pic);
	}
	else
		scr_turtle_count = 0;
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause(void)
{
	if (!scr_pause_pic)
		return;

	if (!scr_showpause.value)	// turn off for screenshots
		return;

	if (!sv.paused)
		return;

	Draw_Pic(scr_vrect.x + 32, scr_vrect.y, scr_pause_pic);
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole(void)
{
	if (scr_con_current == 0)
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify();	// only draw notify in game
	}
	else
	{
		scr_copyeverything = 1;
		Con_DrawConsole(scr_con_current, true);
		scr_fullupdate = 0;
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen(void)
{
	if (vid_skip_swap)
		return;	// not initialized yet

	scr_erase_center = 0;
	scr_copyeverything = 0;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time <= 60)
			return;
		scr_disabled_for_loading = false;
		Con_Printf("load failed.\n");
	}

	if (!cls.state)
		return;
	if (!scr_initialized || !con_initialized)
		return;	// not initialized yet

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);

	//
	// determine size of refresh window
	//
	if (vid.recalc_refdef)
		SCR_CalcRefdef();

	//
	// do 3D refresh drawing, and then update the screen
	//
	SCR_SetUpToDrawConsole();

	V_RenderView();

	GL_Set2D();

	//
	// draw any areas not covered by the refresh
	//
	SCR_FillRect();

	if (scr_drawdialog)
	{
		HUD_Draw();
		Draw_FadeScreen();
		SCR_DrawCenterStringLines();
		scr_copyeverything = true;
	}
	else if (scr_loading)
	{
		SCR_DrawLoading();
		HUD_Draw();
	}
	else if (cl_intermission == 1 && !scr_con_current)
	{
		Sbar_IntermissionOverlay();
	}
	else if (cl_intermission == 2 && !scr_con_current)
	{
		Sbar_FinaleOverlay();
		SCR_CheckDrawCenterString();
	}
	else
	{
		if (crosshair.value)
			Draw_Character(scr_vrect.width / 2 + scr_vrect.x, scr_vrect.height / 2 + scr_vrect.y, '+');

		SCR_DrawRam();
		SCR_DrawNet();
		SCR_DrawTurtle();
		SCR_DrawPause();
		SCR_CheckDrawCenterString();
		HUD_Draw();
		SCR_DrawConsole();
		M_Draw();
	}

	V_UpdatePalette();

	GL_EndRendering();
}
