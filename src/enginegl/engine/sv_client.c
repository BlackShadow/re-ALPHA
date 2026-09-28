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
// sv_client.c -- progs builtins for the player, the HUD and level changes

#include "quakedef.h"
#include "eiface.h"
#include "nullsubs.h"

#define AIR_TIME			12		// seconds of air after the head goes under
#define GASP_TIME			9		// short gasp when surfacing with less air than this left
#define DROWN_DMG_START		2
#define DROWN_DMG_MAX		15
#define DROWN_DMG_RESET		10
#define ATTN_WATER			0.8f

#define NUM_HUD_BUFFERS		13
#define HUD_WIDTH			640		// the HUD is drawn on a 640x480 screen
#define HUD_HEIGHT			480

// health bar in the bottom left panel
#define HEALTH_BAR_X		88
#define HEALTH_BAR_Y		84		// from the top of the panel
#define HEALTH_BAR_LENGTH	150		// at 100 health
#define HEALTH_BLOCK_SIZE	8
#define HEALTH_BLOCK_STEP	10

#define MAX_SLOTS			32		// bits in cl_slotmask

#define SFL_PLAIN_CHANGELEVEL	0x30	// with these serverflags a landmark changelevel doesn't continue the unit

void			*hud_buffers[NUM_HUD_BUFFERS];

qpic_t			*hud_bl;		// panel pictures
qpic_t			*hud_br;
qpic_t			*hud_mr;
qpic_t			*hud_ul;
qpic_t			*hud_bl_alpha;	// additive overlays
qpic_t			*hud_br_alpha;
qpic_t			*hud_mr_alpha;
qpic_t			*hud_ul_alpha;

int				hud_level;		// 0 = no HUD, 1..3 panels drawn
int				hud_pos_x;
int				hud_pos_y;
int				hud_pos_z;
int				hud_pos_w;

/*
==============
SV_GetCosine

Reads OFS_PARM4 and writes OFS_PARM0, unlike SV_GetTangent
==============
*/
void SV_GetCosine(void)
{
	float	angle;
	float	result;

	angle = G_FLOAT(OFS_PARM4);
	result = cos(angle);
	G_FLOAT(OFS_PARM0) = result;
}

/*
==============
SV_GetSine

Reads OFS_PARM4 and writes OFS_PARM0, unlike SV_GetTangent
==============
*/
void SV_GetSine(void)
{
	float	angle;
	float	result;

	angle = G_FLOAT(OFS_PARM4);
	result = sin(angle);
	G_FLOAT(OFS_PARM0) = result;
}

/*
==============
SV_GetTangent
==============
*/
void SV_GetTangent(void)
{
	float	angle;
	float	result;

	angle = G_FLOAT(OFS_PARM0);
	result = tan(angle);
	G_FLOAT(OFS_RETURN) = result;
}

/*
==============
SV_PlayerDrowning

Air, drowning and lava / slime damage for self.
Returns the damage to apply.
==============
*/
void SV_PlayerDrowning(void)
{
	edict_t	*self;
	float	damage;
	float	drownlevel;
	int		flags;
	int		waterlevel;
	int		watertype;

	self = PROG_TO_EDICT(pr_global_struct->self);

	damage = 0;
	if (self->v.movetype == MOVETYPE_NOCLIP)
	{
		self->v.air_finished = sv.time + AIR_TIME;
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	if (self->v.health < 0)
	{
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	drownlevel = WATERLEVEL_HEAD;
	if (self->v.deadflag != 0)
		drownlevel = WATERLEVEL_FEET;

	flags = self->v.flags;
	waterlevel = self->v.waterlevel;
	watertype = self->v.watertype;

	if (!(flags & (FL_IMMUNE_WATER | FL_GODMODE)))
	{
		if (((flags & FL_SWIM) && waterlevel < drownlevel) || waterlevel >= drownlevel)
		{
			// drown!
			if (self->v.air_finished < sv.time && self->v.pain_finished < sv.time)
			{
				self->v.dmg = self->v.dmg + DROWN_DMG_START;
				if (self->v.dmg > DROWN_DMG_MAX)
					self->v.dmg = DROWN_DMG_RESET;
				damage = self->v.dmg;
				self->v.pain_finished = sv.time + 1;
			}
		}
		else
		{
			if (self->v.air_finished < sv.time)
				SV_StartSound(self, CHAN_VOICE, "player/gasp2.wav", 255, ATTN_WATER);
			else if (self->v.air_finished < (float)(sv.time + GASP_TIME))
				SV_StartSound(self, CHAN_VOICE, "player/gasp1.wav", 255, ATTN_WATER);

			self->v.air_finished = sv.time + AIR_TIME;
			self->v.dmg = DROWN_DMG_START;
		}
	}

	if (!waterlevel)
	{
		if (flags & FL_INWATER)
		{
			SV_StartSound(self, CHAN_BODY, "common/outwater.wav", 255, ATTN_WATER);
			self->v.flags = flags & ~FL_INWATER;
		}

		self->v.air_finished = sv.time + AIR_TIME;
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	if (watertype == CONTENTS_LAVA)
	{
		if (!(flags & (FL_IMMUNE_LAVA | FL_GODMODE)) && self->v.dmgtime < sv.time)
		{
			if (self->v.radsuit_finished < sv.time)
				self->v.dmgtime = sv.time + 0.2;
			else
				self->v.dmgtime = sv.time + 1;
			damage = 10 * waterlevel;
		}
	}
	else if (watertype == CONTENTS_SLIME)
	{
		if (!(flags & (FL_IMMUNE_SLIME | FL_GODMODE)) && self->v.dmgtime < sv.time && self->v.radsuit_finished < sv.time)
		{
			self->v.dmgtime = sv.time + 1;
			damage = 4 * waterlevel;
		}
	}

	if (!(flags & FL_INWATER))
	{
		// player enter water sound
		if (watertype == CONTENTS_LAVA)
			SV_StartSound(self, CHAN_BODY, "player/inlava.wav", 255, ATTN_WATER);
		if (watertype == CONTENTS_WATER)
			SV_StartSound(self, CHAN_BODY, "player/inh2o.wav", 255, ATTN_WATER);
		if (watertype == CONTENTS_SLIME)
			SV_StartSound(self, CHAN_BODY, "player/slimbrn2.wav", 255, ATTN_WATER);

		self->v.flags = flags | FL_INWATER;
		self->v.dmgtime = 0;
	}

	// water friction
	if (!(flags & FL_WATERJUMP))
		VectorMA(self->v.velocity, -0.8f * self->v.waterlevel * (float)host_frametime, self->v.velocity, self->v.velocity);

	G_FLOAT(OFS_RETURN) = damage;
}

/*
===============
HUD_InitPanels
===============
*/
static qpic_t *HUD_InitPanels(void)
{
	int		i;

	hud_pos_x = 8;
	hud_pos_y = 52;
	hud_pos_z = 44;
	hud_pos_w = 10;

	if (vid.width >= HUD_WIDTH)
	{
		hud_pos_y = 84;
		hud_pos_z = 88;
	}

	for (i = 0; i < NUM_HUD_BUFFERS; i++)
		free(hud_buffers[i]);

	hud_bl = Draw_PicFromWad_NoScrap("640_BL");
	hud_br = Draw_PicFromWad_NoScrap("640_BR");
	hud_mr = Draw_PicFromWad_NoScrap("640_MR");
	hud_ul = Draw_PicFromWad_NoScrap("640_UL");
	hud_bl_alpha = Draw_PicFromWad_NoScrap("640_BL_T");
	hud_br_alpha = Draw_PicFromWad_NoScrap("640_BR_T");
	hud_mr_alpha = Draw_PicFromWad_NoScrap("640_MR_T");
	hud_ul_alpha = Draw_PicFromWad_NoScrap("640_UL_T");

	return hud_ul_alpha;
}

/*
===============
HUD_Init
===============
*/
qpic_t *HUD_Init(void)
{
	memset(hud_buffers, 0, sizeof(hud_buffers));

	hud_level = 1;

	Cmd_AddCommand("+showscores", VID_ForceLockState);
	Cmd_AddCommand("-showscores", VID_LockBuffer);

	return HUD_InitPanels();
}

/*
===============
Sbar_IntermissionOverlay
===============
*/
void Sbar_IntermissionOverlay(void)
{
}

/*
===============
Sbar_FinaleOverlay
===============
*/
void Sbar_FinaleOverlay(void)
{
}

/*
===============
HUD_Draw
===============
*/
void HUD_Draw(void)
{
	unsigned	oldwidth, oldheight;
	float		frac;
	int			length, x, y;

	if (!hud_level)
		return;

	if ((float)vid.height == scr_con_current)
		return;		// console is full screen

	scr_copyeverything = 1;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glViewport(glx, gly, glwidth, glheight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, HUD_WIDTH, HUD_HEIGHT, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	oldwidth = vid.width;
	oldheight = vid.height;

	vid.width = HUD_WIDTH;
	vid.height = HUD_HEIGHT;

	// solid panels
	glColor4f(1, 1, 1, 1);
	Draw_StretchPic(0, (int)vid.height - hud_bl->height, hud_bl->width, hud_bl->height, hud_bl);

	if (hud_level >= 3)
		Draw_StretchPic(0, 0, hud_ul->width, hud_ul->height, hud_ul);

	if (hud_level >= 2)
	{
		Draw_StretchPic((int)vid.width - hud_br->width, (int)vid.height - hud_br->height,
			hud_br->width, hud_br->height, hud_br);
		Draw_StretchPic((int)vid.width - hud_mr->width, (int)vid.height - hud_br->height - hud_mr->height + 1,
			hud_mr->width, hud_mr->height, hud_mr);
	}

	// additive overlays
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
	glColor4f(1, 1, 1, 0.8f);
	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	Draw_StretchPic(0, (int)vid.height - hud_bl->height, hud_bl->width, hud_bl->height, hud_bl_alpha);

	if (hud_level >= 3)
		Draw_StretchPic(0, 0, hud_ul->width, hud_ul->height, hud_ul_alpha);

	if (hud_level >= 2)
	{
		Draw_StretchPic((int)vid.width - hud_br->width, (int)vid.height - hud_br->height,
			hud_br->width, hud_br->height, hud_br_alpha);
		Draw_StretchPic((int)vid.width - hud_mr->width, (int)vid.height - hud_br->height - hud_mr->height + 1,
			hud_mr->width, hud_mr->height, hud_mr_alpha);
	}

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	// health bar
	frac = (float)cl_health / 100;
	if (frac > 1)
		frac = 1;

	length = (int)(frac * HEALTH_BAR_LENGTH);
	y = (int)vid.height - hud_bl->height + HEALTH_BAR_Y;

	for (x = 0; x < length; x += HEALTH_BLOCK_STEP)
		Draw_FillRGB(x + HEALTH_BAR_X, y, HEALTH_BLOCK_SIZE, HEALTH_BLOCK_SIZE, 255, 156, 39);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	vid.width = oldwidth;
	vid.height = oldheight;
}

/*
===============
HUD_ViewZoomIncrease

Shows one more HUD panel, then grows the view
===============
*/
int HUD_ViewZoomIncrease(void)
{
	if (++hud_level > 3)
	{
		hud_level = 3;
		Cvar_SetValue("viewsize", scr_viewsize.value - 10);
	}

	return 0;
}

/*
===============
HUD_ViewZoomDecrease

Shrinks the view, then hides one HUD panel
===============
*/
void HUD_ViewZoomDecrease(void)
{
	if (scr_viewsize.value < 120)
		Cvar_SetValue("viewsize", scr_viewsize.value + 10);
	else if (--hud_level < 0)
		hud_level = 0;
}

/*
===============
SV_NextPlayerSlot

Next set bit in cl_slotmask after start, wrapping around
===============
*/
int SV_NextPlayerSlot(int start)
{
	int		slot;

	slot = start + 1;
	if (slot >= MAX_SLOTS)
	{
		// wrap around
		for (int i = 0; i <= start; i++)
		{
			if ((1 << i) & cl_slotmask)
				return i;
		}
		return start;
	}

	while (!((1 << slot) & cl_slotmask))
	{
		if (++slot >= MAX_SLOTS)
		{
			// wrap around
			for (int i = 0; i <= start; i++)
			{
				if ((1 << i) & cl_slotmask)
					return i;
			}
			return start;
		}
	}

	return slot;
}

/*
===============
SV_PrevPlayerSlot

Previous set bit in cl_slotmask before start, wrapping around
===============
*/
int SV_PrevPlayerSlot(int start)
{
	int		slot;

	slot = start - 1;
	if (slot < 0)
	{
		// wrap around
		for (int i = MAX_SLOTS - 1; i >= start; i--)
		{
			if ((1 << i) & cl_slotmask)
				return i;
		}
		return start;
	}

	while (!((1 << slot) & cl_slotmask))
	{
		if (--slot < 0)
		{
			// wrap around
			for (int i = MAX_SLOTS - 1; i >= start; i--)
			{
				if ((1 << i) & cl_slotmask)
					return i;
			}
			return start;
		}
	}

	return slot;
}

/*
===============
SV_EntityChangeLevelCallback

Game DLL request to change the level
===============
*/
int SV_EntityChangeLevelCallback(const char *level, const char *startspot)
{
	const qboolean	has_startspot = (startspot && startspot[0]);

	if (svs.changelevel_issued)
		return 0;	// already done

	if (!level || !level[0])
		return 0;

	scr_con_current = 0;
	svs.changelevel_issued = true;
	scr_drawloading = false;
	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;

	Draw_BeginDisc();

	if (!has_startspot)
		Cbuf_AddText(va("changelevel %s\n", level));
	else if ((int)pr_global_struct->serverflags & SFL_PLAIN_CHANGELEVEL)
		Cbuf_AddText(va("changelevel %s %s\n", level, startspot));
	else
		Cbuf_AddText(va("changelevel2 %s %s\n", level, startspot));

	return 0;
}

/*
===============
SV_EntitySecondChangeLevelCallback
===============
*/
int SV_EntitySecondChangeLevelCallback(void)
{
	return SV_EntityChangeLevelCallback(G_STRING(OFS_PARM0), G_STRING(OFS_PARM1));
}

/*
===============
SV_EntityFirstChangeLevelCallback
===============
*/
int SV_EntityFirstChangeLevelCallback(void)
{
	return PF_setspawnparms(G_EDICT(OFS_PARM0));
}

/*
===============
Builtin_Unimplemented
===============
*/
void Builtin_Unimplemented(void)
{
	Sys_Error("unimplemented builtin");
}
