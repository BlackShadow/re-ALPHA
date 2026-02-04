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
#include "quakedef.h"
#include "nullsubs.h"
#include "model.h"

float		scr_centertime_start;
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;
int			scr_drawdialog;
int			scr_initialized;
int			scr_loading;
int			scr_disabled_for_loading;
int			scr_fullupdate;
int			scr_sb_lines;
double		scr_disabled_time;
char		*scr_centerstring_ptr;
int			scr_drawloading;
float		scr_con_current;
int			scr_copyeverything;

int			scr_disk_state;
int			scr_disk_counter;
int			scr_disk_offset;
float		scr_disk_time;
static qpic_t *scr_pause_pic;
static qpic_t *scr_net_pic;
static qpic_t *scr_turtle_pic;
static int scr_turtle_count;

char		scr_centerstring[1024];

cvar_t		scr_viewsize = {"viewsize", "120", true, false};
cvar_t		scr_fov = {"fov", "90"};
cvar_t		scr_conspeed = {"scr_conspeed", "300"};
cvar_t		scr_centertime = {"scr_centertime", "2"};
cvar_t		scr_printspeed = {"scr_printspeed", "8"};
cvar_t		scr_showram = {"scr_showram", "1"};
cvar_t		scr_showturtle = {"scr_showturtle", "0"};
cvar_t		scr_showpause = {"scr_showpause", "1"};

extern int			vid_skip_swap;
extern int			sb_lines;
extern int			glx;
extern int			gly;
extern int			glwidth;
extern int			glheight;
vrect_t				scr_vrect;
extern refdef_t		r_refdef;
extern int			r_refdef_vrect_x;
extern int			r_refdef_vrect_y;
extern int			r_refdef_vrect_width;
extern int			r_refdef_vrect_height;
extern int			key_count;
extern int			key_lastpress;
extern double		realtime;
extern cvar_t		crosshair;

void SCR_ScreenShot_f(void);
void SCR_SizeUp_f(void);
void SCR_SizeDown_f(void);
void SCR_DrawRam(void);
void SCR_DrawNet(void);
void SCR_DrawTurtle(void);
void SCR_DrawPause(void);
void SCR_DrawLoading(void);
void SCR_DrawConsole(void);
void SCR_SetUpToDrawConsole(void);
void SCR_CalcRefdef(void);
void SCR_UpdateScreen(void);

void VID_GetWindowSize(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);
void Sys_SendKeyEvents(void);

void SCR_CenterPrint(char *text)
{
	char *p;
	qboolean empty;

	strncpy(scr_centerstring, text, 1023);
	scr_centerstring[1023] = 0;

	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = (float)cl_time;

	empty = (scr_centerstring[0] == 0);
	scr_center_lines = 1;

	if (!empty)
	{
		for (p = scr_centerstring; *p; p++)
		{
			if (*p == '\n')
				scr_center_lines++;
		}
	}
}

void SCR_DrawCenterString(void)
{
	char *p;
	int i, j;
	char c;
	int x_pos, y_pos;
	int lines_to_draw;

	if (cl_intermission)
		lines_to_draw = (int)((cl_time - scr_centertime_start) * scr_printspeed.value);
	else
		lines_to_draw = 9999;

	if (scr_centertime_off <= 0 && !cl_intermission)
		return;

	p = scr_centerstring;

	if (scr_center_lines > 4)
		y_pos = 48;
	else
		y_pos = (int)(vid.height * 0.35f);

	while (*p)
	{
		for (i = 0; i < 40; i++)
		{
			c = p[i];
			if (c == '\n' || c == '\0')
				break;
		}

		x_pos = (vid.width - 8 * i) / 2;

		for (j = 0; j < i; j++)
		{
			Draw_Character(x_pos, y_pos, p[j]);
			x_pos += 8;

			if (cl_intermission && --lines_to_draw <= 0)
				return;
		}

		y_pos += 8;
		p += i;
		if (*p == '\n')
			p++;
	}
}

void SCR_CheckDrawCenterString(void)
{
	scr_erase_center = 1;
	if (scr_erase_lines < scr_center_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if ((scr_centertime_off > 0.0f || cl_intermission) && !scr_drawdialog)
		SCR_DrawCenterString();
}

void SCR_SizeUp_f(void)
{
	HUD_ViewZoomDecrease();
	vid.recalc_refdef = 1;
}

void SCR_SizeDown_f(void)
{
	HUD_ViewZoomIncrease();
	vid.recalc_refdef = 1;
}

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

	Cmd_AddCommand("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);

	scr_pause_pic = Draw_PicFromWad("lambda");
	scr_net_pic = Draw_PicFromWad("lambda");
	scr_turtle_pic = Draw_PicFromWad("lambda");

	scr_initialized = true;
}

void SCR_CalcRefdef(void)
{
	int sb_lines;
	float vsize;
	float w, h;
	float scale;

	scr_fullupdate = 0;
	vid.recalc_refdef = 0;

	scr_viewsize.value = 120.0f;

	if (scr_fov.value < 10.0f)
		Cvar_SetValue("fov", 10.0f);
	if (scr_fov.value > 170.0f)
		Cvar_SetValue("fov", 170.0f);

	if (cl_intermission)
		vsize = 120.0f;
	else
		vsize = scr_viewsize.value;

	if (vsize < 120.0f)
	{
		sb_lines = 24;
		if (vsize < 110.0f)
			sb_lines = 48;
	}
	else
	{
		sb_lines = 0;
	}

	scr_sb_lines = sb_lines;

	if (vsize >= 100.0f)
		vsize = 100.0f;

	scale = vsize / 100.0f;
	w = (float)vid.width;
	scr_vrect.width = (int)(w * scale);

	if (scr_vrect.width < 96)
	{
		float aspect = (float)scr_vrect.width;
		scr_vrect.width = 96;
		scale = 96.0f / aspect;
	}

	h = (float)vid.height;
	scr_vrect.height = (int)(h * scale);

	if ((int)vid.height - sb_lines < scr_vrect.height)
		scr_vrect.height = (int)vid.height - sb_lines;

	scr_vrect.x = ((int)vid.width - scr_vrect.width) / 2;
	scr_vrect.y = ((int)vid.height - sb_lines - scr_vrect.height) / 2;

	r_refdef_vrect_x = scr_vrect.x;
	r_refdef_vrect_y = scr_vrect.y;
	r_refdef_vrect_width = scr_vrect.width;
	r_refdef_vrect_height = scr_vrect.height;

	r_refdef.vrect = scr_vrect;
}

void SCR_ScreenShot_f(void)
{
	int i;
	byte *buffer;
	char filename[80];
	char checkname[128];
	FILE *f;
	byte temp;
	int bufsize;

	strcpy(filename, "quake00.tga");
	for (i = 0; i <= 99; i++)
	{
		filename[5] = (i / 10) + '0';
		filename[6] = (i % 10) + '0';
		sprintf(checkname, "%s/%s", com_gamedir, filename);
		f = fopen(checkname, "rb");
		if (!f)
			break;
		fclose(f);
	}

	if (i == 100)
	{
		Con_Printf("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
	}

	buffer = (byte*)malloc(3 * (glwidth * glheight + 6));

	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 2;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 0;
	buffer[8] = 0;
	buffer[9] = 0;
	buffer[10] = 0;
	buffer[11] = 0;
	buffer[12] = glwidth & 0xFF;
	buffer[13] = (glwidth >> 8) & 0xFF;
	buffer[14] = glheight & 0xFF;
	buffer[15] = (glheight >> 8) & 0xFF;
	buffer[16] = 24;
	buffer[17] = 0;

	glReadPixels(glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18);

	bufsize = 3 * (glwidth * glheight + 6);
	for (i = 18; i < bufsize; i += 3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}

	COM_WriteFile(filename, buffer, bufsize);
	free(buffer);

	Con_Printf("Wrote %s\n", filename);
}

void SCR_BeginLoadingPlaque(void)
{
	S_StopAllSounds(true);

	if (cls.state == ca_connected && cls.signon == SIGNONS)
	{
		Con_ClearNotify();
		scr_centertime_start = 0.0f;
		scr_con_current = 0.0f;
		scr_drawloading = 1;
		scr_loading = 1;
		scr_erase_center = 0;
		SCR_UpdateScreen();
		scr_disabled_for_loading = 1;
		scr_erase_center = 0;
		scr_disabled_time = realtime;
	}
}

void SCR_EndLoadingPlaque(void)
{
	scr_copyeverything = 0;
	scr_drawloading = 0;
	scr_loading = 0;
	scr_disabled_for_loading = 0;
	Con_ClearNotify();
}

void SCR_DrawCenterStringLines(void)
{
	char *text;
	int line_count;
	int x_pos;
	int y_pos;
	int char_index;
	char c;
	int line_length;
	unsigned char ch_byte;

	text = scr_centerstring_ptr;
	y_pos = (int)((double)vid.height * 0.35);

	while (*text)
	{
		for (line_count = 0; line_count < 40; ++line_count)
		{
			c = text[line_count];
			if (c == '\n')
				break;
			if (!c)
				break;
		}

		line_length = 0;
		x_pos = (vid.width - 8 * line_count) / 2;

		for (char_index = x_pos; line_length < line_count; char_index += 8)
		{
			ch_byte = text[line_length++];
			Draw_Character(char_index, y_pos, ch_byte);
		}

		y_pos += 8;

		if (!*text)
			break;

		do
		{
			if (*text == '\n')
				break;
			++text;
		}
		while (*text);

		if (!*text)
			break;

		++text;
	}
}

qboolean SCR_DrawDialog(char *message_ptr)
{
	if (!cls.state)
		return 1;

	scr_centerstring_ptr = message_ptr;
	scr_erase_center = 0;
	scr_drawdialog = 1;
	SCR_UpdateScreen();
	scr_drawdialog = 0;

	S_ClearBuffer();

	do
	{
		key_count = -1;
		Sys_SendKeyEvents();
	} while (key_lastpress != 121 && key_lastpress != 110 && key_lastpress != 27);

	scr_erase_center = 0;
	SCR_UpdateScreen();
	return key_lastpress == 121;
}

void SCR_FillRect(void)
{
	if (scr_vrect.x > 0)
	{
		Draw_Fill(0, 0, scr_vrect.x, 152, 0);
		Draw_Fill(scr_vrect.x + scr_vrect.width, 0, scr_vrect.width - scr_vrect.x + 320, 152, 0);
	}

	if (scr_vrect.height < 152)
	{
		Draw_Fill(scr_vrect.x, 0, scr_vrect.width, scr_vrect.y, 0);
		Draw_Fill(scr_vrect.x, scr_vrect.height + scr_vrect.y, scr_vrect.width, 152 - scr_vrect.y - scr_vrect.height, 0);
	}
}

void SCR_SetUpToDrawConsole(void)
{
	Con_CheckResize();

	if (scr_drawloading)
		return;

	con_forcedup = (!cl_worldmodel || cls.signon != SIGNONS);

	if (con_forcedup)
	{
		scr_con_current = (float)vid.height;
	}
	else if (key_dest == key_console)
	{
		const float halfHeight = (float)((unsigned int)vid.height >> 1);
		if (scr_con_current < halfHeight)
		{
			scr_con_current += (float)(scr_conspeed.value * host_frametime);
			if (scr_con_current > halfHeight)
				scr_con_current = halfHeight;
		}
		else if (scr_con_current > halfHeight)
		{
			scr_con_current -= (float)(scr_conspeed.value * host_frametime);
			if (scr_con_current < halfHeight)
				scr_con_current = halfHeight;
		}
	}
	else
	{
		if (scr_con_current > 0.0f)
		{
			scr_con_current -= (float)(scr_conspeed.value * host_frametime);
			if (scr_con_current < 0.0f)
				scr_con_current = 0.0f;
		}
	}

	if (scr_con_current > 0.0f)
		scr_fullupdate = 0;
}

void SCR_DrawLoading(void)
{
	if (scr_drawloading)
		Draw_BeginDisc();
}

void SCR_DrawRam(void)
{
	if (scr_showram.value != 0.0f && cl_paused)
		Draw_BeginDisc();
}

void SCR_DrawNet(void)
{
	if (!scr_net_pic)
		return;

	if (realtime - cl.last_received_message >= 0.3 && !cls.demoplayback)
		Draw_Pic(scr_vrect.x + 64, scr_vrect.y, scr_net_pic);
}

void SCR_DrawTurtle(void)
{
	if (!scr_turtle_pic || scr_showturtle.value == 0.0f)
		return;

	if (host_frametime >= 0.1)
	{
		if (++scr_turtle_count >= 3)
			Draw_Pic(scr_vrect.x, scr_vrect.y, scr_turtle_pic);
	}
	else
	{
		scr_turtle_count = 0;
	}
}

void SCR_DrawPause(void)
{
	if (!scr_pause_pic)
		return;

	if (scr_showpause.value != 0.0f && sv.paused)
		Draw_Pic(scr_vrect.x + 32, scr_vrect.y, scr_pause_pic);
}

void SCR_DrawConsole(void)
{
	if (scr_con_current == 0.0f)
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify();
	}
	else
	{
		scr_copyeverything = 1;
		Con_DrawConsole((int)scr_con_current, true);
		scr_fullupdate = 0;
	}
}

void SCR_UpdateScreen(void)
{
	if (!vid_skip_swap)
	{
		scr_erase_center = 0;
		scr_copyeverything = 0;

		if (scr_disabled_for_loading)
		{
			if (realtime - scr_disabled_time <= 60.0)
				return;

			scr_disabled_for_loading = 0;
			Con_Printf("load failed.\n");
		}

		if (cls.state && scr_initialized && con_initialized)
		{
			VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);

			if (vid.recalc_refdef)
				SCR_CalcRefdef();

			SCR_SetUpToDrawConsole();

			V_RenderView();

			GL_Set2D();

			SCR_FillRect();

			if (scr_drawdialog)
			{
				HUD_Draw();
				Draw_FadeScreen();
				SCR_DrawCenterStringLines();
				scr_copyeverything = 1;
			}
			else if (scr_loading)
			{
				SCR_DrawLoading();
				HUD_Draw();
			}
			else if (cl_intermission != 1 || scr_con_current)
			{
				if (cl_intermission != 2 || scr_con_current)
				{
					if (crosshair.value != 0.0f)
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
				else
				{
					Sbar_FinaleOverlay();
					SCR_CheckDrawCenterString();
				}
			}
			else
			{
				Sbar_IntermissionOverlay();
			}

			V_UpdatePalette();

			GL_EndRendering();
		}
	}
}
