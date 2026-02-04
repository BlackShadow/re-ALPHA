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
#include "mathlib.h"
#include <math.h>

extern int		sv_movement_count;
extern int		sv_movement_failed;

extern int		cl_slotmask;
extern int		PF_setspawnparms(edict_t *ent);

extern int		scr_disk_state;
extern int		scr_disk_counter;
extern int		scr_disk_offset;
extern float	scr_disk_time;

extern int		scr_vrect_width;
extern int		scr_vrect_height;

void			*hud_buffers[13];

qpic_t			*hud_icon1_tex;
qpic_t			*hud_icon2_tex;
qpic_t			*hud_icon3_tex;
qpic_t			*hud_icon4_tex;
qpic_t			*hud_icon1_alpha;
qpic_t			*hud_icon2_alpha;
qpic_t			*hud_icon3_alpha;
qpic_t			*hud_icon4_alpha;

int				viewsize_level;
int				hud_pos_x;
int				hud_pos_y;
int				hud_pos_z;
int				hud_pos_w;

extern cvar_t	scr_viewsize;
extern float	scr_con_current;

extern int		trace_buffer;
extern int		reserved_buffer;

extern int		window_offset_x;
extern int		window_offset_y;
extern int		window_width;
extern int		window_height;
extern HWND		main_window;

extern int		display_mode_flag;
extern int		render_mode;
extern int		render_scale_w;
extern int		render_scale_h;
extern int		fullscreen_offset_w;
extern int		fullscreen_offset_h;
extern int		is_fullscreen;
extern int		*vid_mode_table;
extern HINSTANCE g_hInstance;
extern HICON	g_hIcon;

int SV_GetCosine(void)
{
	float angle = *(float *)(pr_globals + 16);

	float result = (float)cos((double)angle);
	*(float *)(pr_globals + 4) = result;

	return *(int *)&result;
}

int SV_GetSine(void)
{
	float angle = *(float *)(pr_globals + 16);

	float result = (float)sin((double)angle);
	*(float *)(pr_globals + 4) = result;

	return *(int *)&result;
}

int SV_GetTangent(void)
{
	float angle = *(float *)((byte *)pr_globals + 16);

	float result = (float)tan((double)angle);
	*(float *)((byte *)pr_globals + 4) = result;

	return *(int *)&result;
}

void SV_PlayerDrowning(void)
{
	edict_t *self;
	float damage;
	float drownlevel;
	int flags;
	int waterlevel;
	int watertype;

	self = PROG_TO_EDICT(pr_global_struct->self);

	damage = 0.0f;
	if (self->v.movetype == MOVETYPE_NOCLIP)
	{
		self->v.air_finished = (float)(sv_time + 12.0);
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	if (self->v.health < 0.0f)
	{
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	drownlevel = 3.0f;
	if (self->v.deadflag != 0)
		drownlevel = 1.0f;

	flags = self->v.flags;
	waterlevel = self->v.waterlevel;
	watertype = self->v.watertype;

	if ((flags & (FL_IMMUNE_WATER | FL_GODMODE)) == 0)
	{
		if (((flags & FL_SWIM) && (float)waterlevel < drownlevel) || (float)waterlevel >= drownlevel)
		{
			if (self->v.air_finished < sv_time && self->v.pain_finished < sv_time)
			{
				self->v.dmg = self->v.dmg + 2.0f;
				if (self->v.dmg > 15.0f)
					self->v.dmg = 10.0f;
				damage = self->v.dmg;
				self->v.pain_finished = (float)(sv_time + 1.0);
			}
		}
		else
		{
			if (self->v.air_finished < sv_time)
				SV_StartSound(self, 2, "player/gasp2.wav", 255, 0.8f);
			else if (self->v.air_finished < (float)(sv_time + 9.0))
				SV_StartSound(self, 2, "player/gasp1.wav", 255, 0.8f);

			self->v.air_finished = (float)(sv_time + 12.0);
			self->v.dmg = 2.0f;
		}
	}

	if (!waterlevel)
	{
		if (flags & FL_INWATER)
		{
			SV_StartSound(self, 4, "common/outwater.wav", 255, 0.8f);
			self->v.flags = flags & ~FL_INWATER;
		}

		self->v.air_finished = (float)(sv_time + 12.0);
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	if (watertype == CONTENTS_LAVA)
	{
		if ((flags & (FL_IMMUNE_LAVA | FL_GODMODE)) == 0 && self->v.dmgtime < sv_time)
		{
			self->v.dmgtime = (self->v.radsuit_finished < sv_time) ? (float)(sv_time + 0.2) : (float)(sv_time + 1.0);
			damage = (float)(10 * waterlevel);
		}
	}
	else if (watertype == CONTENTS_SLIME)
	{
		if ((flags & (FL_IMMUNE_SLIME | FL_GODMODE)) == 0 && self->v.dmgtime < sv_time && self->v.radsuit_finished < sv_time)
		{
			self->v.dmgtime = (float)(sv_time + 1.0);
			damage = (float)(4 * waterlevel);
		}
	}

	if ((flags & FL_INWATER) == 0)
	{
		if (watertype == CONTENTS_LAVA)
			SV_StartSound(self, 4, "player/inlava.wav", 255, 0.8f);
		if (watertype == CONTENTS_WATER)
			SV_StartSound(self, 4, "player/inh2o.wav", 255, 0.8f);
		if (watertype == CONTENTS_SLIME)
			SV_StartSound(self, 4, "player/slimbrn2.wav", 255, 0.8f);

		self->v.flags = flags | FL_INWATER;
		self->v.dmgtime = 0.0f;
	}

	if ((flags & FL_WATERJUMP) == 0)
		VectorMA(self->v.velocity, -0.8f * (float)self->v.waterlevel * (float)host_frametime, self->v.velocity, self->v.velocity);

	G_FLOAT(OFS_RETURN) = damage;
}

qpic_t *HUD_InitPanels(void)
{

	hud_pos_x = 8;
	hud_pos_y = 52;
	hud_pos_z = 44;
	hud_pos_w = 10;

	if (vid.width >= 640)
	{
		hud_pos_y = 84;
		hud_pos_z = 88;
	}

	for (int i = 0; i < (int)(sizeof(hud_buffers) / sizeof(hud_buffers[0])); ++i)
		free(hud_buffers[i]);

	hud_icon1_tex = Draw_PicFromWad_NoScrap("640_BL");
	hud_icon2_tex = Draw_PicFromWad_NoScrap("640_BR");
	hud_icon3_tex = Draw_PicFromWad_NoScrap("640_MR");
	hud_icon4_tex = Draw_PicFromWad_NoScrap("640_UL");
	hud_icon1_alpha = Draw_PicFromWad_NoScrap("640_BL_T");
	hud_icon2_alpha = Draw_PicFromWad_NoScrap("640_BR_T");
	hud_icon3_alpha = Draw_PicFromWad_NoScrap("640_MR_T");
	hud_icon4_alpha = Draw_PicFromWad_NoScrap("640_UL_T");

	return hud_icon4_alpha;
}

qpic_t *HUD_Init(void)
{

	memset(hud_buffers, 0, sizeof(hud_buffers));

	viewsize_level = 1;

	Cmd_AddCommand("+showscores", VID_ForceLockState);
	Cmd_AddCommand("-showscores", VID_LockBuffer);

	return HUD_InitPanels();
}

//=========================================================
// Sbar_IntermissionOverlay - intermission overlay stub
//=========================================================
void Sbar_IntermissionOverlay(void)
{
}

//=========================================================
// Sbar_FinaleOverlay - finale overlay stub
//=========================================================
void Sbar_FinaleOverlay(void)
{
}

void HUD_Draw(void)
{
	if (!viewsize_level)
		return;

	if ((float)(unsigned int)vid.height == scr_con_current)
		return;

	scr_copyeverything = 1;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glViewport(glx, gly, glwidth, glheight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 640.0, 480.0, 0.0, -99999.0, 99999.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	const unsigned oldWidth = vid.width;
	const unsigned oldHeight = vid.height;

	vid.width = 640;
	vid.height = 480;

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	Draw_StretchPic(0, (int)vid.height - hud_icon1_tex->height, hud_icon1_tex->width, hud_icon1_tex->height, hud_icon1_tex);

	if (viewsize_level >= 3)
		Draw_StretchPic(0, 0, hud_icon4_tex->width, hud_icon4_tex->height, hud_icon4_tex);

	if (viewsize_level >= 2)
	{
		Draw_StretchPic(
			(int)vid.width - hud_icon2_tex->width,
			(int)vid.height - hud_icon2_tex->height,
			hud_icon2_tex->width,
			hud_icon2_tex->height,
			hud_icon2_tex);

		Draw_StretchPic(
			(int)vid.width - hud_icon3_tex->width,
			(int)vid.height - hud_icon2_tex->height - hud_icon3_tex->height + 1,
			hud_icon3_tex->width,
			hud_icon3_tex->height,
			hud_icon3_tex);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
	glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	Draw_StretchPic(0, (int)vid.height - hud_icon1_tex->height, hud_icon1_tex->width, hud_icon1_tex->height, hud_icon1_alpha);

	if (viewsize_level >= 3)
		Draw_StretchPic(0, 0, hud_icon4_tex->width, hud_icon4_tex->height, hud_icon4_alpha);

	if (viewsize_level >= 2)
	{
		Draw_StretchPic(
			(int)vid.width - hud_icon2_tex->width,
			(int)vid.height - hud_icon2_tex->height,
			hud_icon2_tex->width,
			hud_icon2_tex->height,
			hud_icon2_alpha);

		Draw_StretchPic(
			(int)vid.width - hud_icon3_tex->width,
			(int)vid.height - hud_icon2_tex->height - hud_icon3_tex->height + 1,
			hud_icon3_tex->width,
			hud_icon3_tex->height,
			hud_icon3_alpha);
	}

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	float healthFrac = (float)cl_health / 100.0f;
	if (healthFrac > 1.0f)
		healthFrac = 1.0f;

	const int healthWidth = (int)(healthFrac * 150.0f);
	const int healthY = (int)vid.height - hud_icon1_tex->height + 84;

	for (int healthX = 0; healthX < healthWidth; healthX += 10)
		Draw_FillRGB(healthX + 88, healthY, 8, 8, 255, 156, 39);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	vid.width = oldWidth;
	vid.height = oldHeight;
}

int HUD_ViewZoomIncrease(void)
{
	int result = 0;

	if (++viewsize_level > 3)
	{
		viewsize_level = 3;
		float new_size = scr_viewsize.value - 10.0f;
		Cvar_SetValue("viewsize", new_size);
	}

	return result;
}

void HUD_ViewZoomDecrease(void)
{
	if (scr_viewsize.value < 120.0f)
	{
		float new_size = scr_viewsize.value + 10.0f;
		Cvar_SetValue("viewsize", new_size);
	}
	else if (--viewsize_level < 0)
	{
		viewsize_level = 0;
	}
}

int SV_NextPlayerSlot(int start_slot)
{
	int next = start_slot + 1;

	if (next >= 32)
	{
		for (int result = 0; result <= start_slot; ++result)
		{
			if (((1 << result) & cl_slotmask) != 0)
				return result;
		}
		return start_slot;
	}

	while (((1 << next) & cl_slotmask) == 0)
	{
		if (++next >= 32)
		{

			for (int result = 0; result <= start_slot; ++result)
			{
				if (((1 << result) & cl_slotmask) != 0)
					return result;
			}
			return start_slot;
		}
	}

	return next;
}

int SV_PrevPlayerSlot(int start_slot)
{
	int prev = start_slot - 1;

	if (prev < 0)
	{
		for (int result = 31; result >= start_slot; --result)
		{
			if (((1 << result) & cl_slotmask) != 0)
				return result;
		}
		return start_slot;
	}

	while (((1 << prev) & cl_slotmask) == 0)
	{
		if (--prev < 0)
		{

			for (int result = 31; result >= start_slot; --result)
			{
				if (((1 << result) & cl_slotmask) != 0)
					return result;
			}
			return start_slot;
		}
	}

	return prev;
}

int SV_EntityChangeLevelCallback(const char *level, const char *startspot)
{
	if (svs.changelevel_issued)
		return 0;

	scr_con_current = 0.0f;
	svs.changelevel_issued = 1;
	scr_drawloading = 0;
	scr_disabled_for_loading = 1;
	scr_disabled_time = realtime;

	Draw_BeginDisc();

	if (((int)pr_global_struct->serverflags & 0x30) != 0)
		Cbuf_AddText(va("changelevel %s %s\n", (char *)level, (char *)startspot));
	else
		Cbuf_AddText(va("changelevel2 %s %s\n", (char *)level, (char *)startspot));

	return 0;
}

int SV_EntitySecondChangeLevelCallback(void)
{
	return SV_EntityChangeLevelCallback(G_STRING(OFS_PARM0), G_STRING(OFS_PARM1));
}

int SV_EntityFirstChangeLevelCallback(void)
{
	return PF_setspawnparms(G_EDICT(OFS_PARM0));
}

void Builtin_Unimplemented(void)
{

	Sys_Error("unimplemented builtin");
}
