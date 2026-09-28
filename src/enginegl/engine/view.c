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

// view.c -- player eye positioning

#include "quakedef.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

// cl_items powerup bits
#define IT_INVISIBILITY		(1<<3)
#define IT_INVULNERABILITY	(1<<4)
#define IT_SUIT				(1<<5)
#define IT_QUAD				(1<<6)

int		v_screenshake_enabled = 0;
float	v_screenshake_angles[3] = { 0, 0, 0 };
float	v_screenshake_scale = 0;

cvar_t	cv_cl_bob = { "cl_bob", "0.01" };
cvar_t	cv_cl_bobcycle = { "cl_bobcycle", "0.8" };
cvar_t	cv_cl_bobup = { "cl_bobup", "0.5" };
cvar_t	cv_cl_rollangle = { "cl_rollangle", "2.0" };
cvar_t	cv_cl_rollspeed = { "cl_rollspeed", "200" };

cvar_t	cv_v_idlescale = { "v_idlescale", "0" };
cvar_t	cv_v_ipitch_cycle = { "v_ipitch_cycle", "1" };
cvar_t	cv_v_ipitch_level = { "v_ipitch_level", "0.3" };
cvar_t	cv_v_iroll_cycle = { "v_iroll_cycle", "0.5" };
cvar_t	cv_v_iroll_level = { "v_iroll_level", "0.1" };
cvar_t	cv_v_iyaw_cycle = { "v_iyaw_cycle", "2" };
cvar_t	cv_v_iyaw_level = { "v_iyaw_level", "0.3" };

cvar_t	cv_v_kicktime = { "v_kicktime", "0.5" };
cvar_t	cv_v_kickroll = { "v_kickroll", "0.6" };
cvar_t	cv_v_kickpitch = { "v_kickpitch", "0.6" };

cvar_t	cv_v_centermove = { "v_centermove", "0.15" };
cvar_t	cv_v_centerspeed = { "v_centerspeed", "500" };

cvar_t	cv_scr_ofsx = { "scr_ofsx", "0" };
cvar_t	cv_scr_ofsy = { "scr_ofsy", "0" };
cvar_t	cv_scr_ofsz = { "scr_ofsz", "0" };

cvar_t	crosshair = { "crosshair", "1" };
cvar_t	cv_gl_cshiftpercent = { "gl_cshiftpercent", "100" };
cvar_t	cv_v_contentblend = { "v_contentblend", "1" };

cvar_t	cv_v_dmg_pitch = { "v_dmg_pitch", "0.6" };
cvar_t	cv_v_dmg_roll = { "v_dmg_roll", "1.0" };
cvar_t	cv_v_dmg_time = { "v_dmg_time", "0.5" };
cvar_t	cv_v_gamma = { "gamma", "2.5", true };
cvar_t	cv_brightness = { "brightness", "0.0", true };
cvar_t	cv_lightgamma = { "lightgamma", "2.5" };
cvar_t	cv_texgamma = { "texgamma", "1.8" };
cvar_t	cv_lambert = { "lambert", "1.7" };
cvar_t	cv_direct = { "direct", "0.9" };

static qboolean	v_nodrift = true;
static float	v_pitchvel;
static float	v_driftmove;
static double	v_laststop;

typedef struct
{
	int		destcolor[3];
	int		percent;		// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4

static cshift_t			cshift_empty = { { 0, 0, 0 }, 0 };
static const cshift_t	cshift_water = { { 130, 80, 50 }, 128 };
static const cshift_t	cshift_slime = { { 0, 25, 5 }, 150 };
static const cshift_t	cshift_lava = { { 255, 80, 0 }, 150 };

static cshift_t	cshifts[NUM_CSHIFTS];
static cshift_t	prev_cshifts[NUM_CSHIFTS];

int		lightgammatable[1024];
byte	ramps_r[256];
byte	ramps_g[256];
byte	ramps_b[256];

float	v_gunyaw;		// the weapon model lags behind the view
float	v_gunpitch;
float	v_dmg_time;
float	v_dmg_roll;
float	v_dmg_pitch;

float	v_punchangle_decay[4];
float	v_punch_forward;
float	v_punch_right;
float	v_punch_up;

// directions of the last damage, faded by V_CheckDamageCshift
float	v_lateral_punch;
float	v_lateral_scale;
float	v_vertical_punch;
float	v_vertical_scale;

void V_CheckDamageCshift(void);
int V_CheckBonusCshift(void);

/*
==============================================================================

						PALETTE FLASHES

==============================================================================
*/

void V_cshift_f(void)
{
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}

/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f(void)
{
	cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor(int contents)
{
	switch (contents)
	{
	case CONTENTS_EMPTY:
	case CONTENTS_SOLID:
		cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	case CONTENTS_LAVA:
		cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;
	case CONTENTS_SLIME:
		cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;
	case CONTENTS_WATER:
		cshifts[CSHIFT_CONTENTS] = cshift_water;
		break;
	default:
		cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift(void)
{
	if (cl_items & IT_QUAD)
	{
		cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else if (cl_items & IT_SUIT)
	{
		cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cshifts[CSHIFT_POWERUP].percent = 20;
	}
	else if (cl_items & IT_INVISIBILITY)
	{
		cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cshifts[CSHIFT_POWERUP].percent = 100;
	}
	else if (cl_items & IT_INVULNERABILITY)
	{
		cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else
	{
		cshifts[CSHIFT_POWERUP].percent = 0;
	}
}

/*
=============
V_CalcBlend
=============
*/
void V_CalcBlend(void)
{
	float	r, g, b, a, a2;
	float	alpha;
	int		j;

	r = 0;
	g = 0;
	b = 0;
	a = 0;

	for (j = 0; j < NUM_CSHIFTS; j++)
	{
		a2 = cshifts[j].percent / 255.0f;
		if (a2 != 0)
		{
			alpha = (1.0f - a) * a2 + a;
			a2 = a2 / alpha;

			r = cshifts[j].destcolor[0] * a2 + (1.0f - a2) * r;
			g = cshifts[j].destcolor[1] * a2 + (1.0f - a2) * g;
			b = cshifts[j].destcolor[2] * a2 + (1.0f - a2) * b;
			a = alpha;
		}
	}

	v_blend[0] = r / 255.0f;
	v_blend[1] = g / 255.0f;
	v_blend[2] = b / 255.0f;
	v_blend[3] = a;
	if (v_blend[3] > 1)
		v_blend[3] = 1;
	if (v_blend[3] < 0)
		v_blend[3] = 0;
}

/*
=============
V_UpdatePalette
=============
*/
void V_UpdatePalette(void)
{
	int		i;
	qboolean	new;
	byte	*basepal;
	byte	pal[768], *newpal;
	int		r, g, b;
	float	a, frac;
	float	ir, ig, ib;
	qboolean	force;

	V_CalcPowerupCshift();

	new = false;

	for (i = 0; i < NUM_CSHIFTS; i++)
	{
		if (prev_cshifts[i].percent != cshifts[i].percent
			|| prev_cshifts[i].destcolor[0] != cshifts[i].destcolor[0]
			|| prev_cshifts[i].destcolor[1] != cshifts[i].destcolor[1]
			|| prev_cshifts[i].destcolor[2] != cshifts[i].destcolor[2])
		{
			new = true;
			prev_cshifts[i] = cshifts[i];
		}
	}

// drop the damage value
	cshifts[CSHIFT_DAMAGE].percent -= host_frametime * 150;
	if (cshifts[CSHIFT_DAMAGE].percent < 0)
		cshifts[CSHIFT_DAMAGE].percent = 0;

	V_CheckDamageCshift();

// drop the bonus value
	cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
	if (cshifts[CSHIFT_BONUS].percent < 0)
		cshifts[CSHIFT_BONUS].percent = 0;

	force = V_CheckBonusCshift();
	if (!new && !force)
		return;

	V_CalcBlend();

	a = 1.0f - v_blend[3];
	ir = v_blend[0] * v_blend[3] * 255.0f;
	ig = v_blend[1] * v_blend[3] * 255.0f;
	ib = v_blend[2] * v_blend[3] * 255.0f;

	for (i = 0; i < 256; i++)
	{
		frac = i * a;
		r = frac + ir;
		g = frac + ig;
		b = frac + ib;

		if (r > 255)
			r = 255;
		if (g > 255)
			g = 255;
		if (b > 255)
			b = 255;
		if (r < 0)
			r = 0;
		if (g < 0)
			g = 0;
		if (b < 0)
			b = 0;

		ramps_r[i] = gammatable[r];
		ramps_g[i] = gammatable[g];
		ramps_b[i] = gammatable[b];
	}

	basepal = host_basepal;
	newpal = pal;

	for (i = 0; i < 256; i++)
	{
		r = basepal[0];
		g = basepal[1];
		b = basepal[2];
		basepal += 3;

		newpal[0] = ramps_r[r];
		newpal[1] = ramps_g[g];
		newpal[2] = ramps_b[b];
		newpal += 3;
	}

	VID_ShiftPalette();
}

/*
=============
V_CheckDamageCshift

Fades the damage direction values
=============
*/
void V_CheckDamageCshift(void)
{
	v_lateral_scale -= (float)host_frametime;
	if (v_lateral_scale < 0)
		v_lateral_scale = 0;

	v_lateral_punch -= (float)host_frametime;
	if (v_lateral_punch < 0)
		v_lateral_punch = 0;

	v_vertical_punch -= (float)host_frametime;
	if (v_vertical_punch < 0)
		v_vertical_punch = 0;

	v_vertical_scale -= (float)host_frametime;
	if (v_vertical_scale < 0)
		v_vertical_scale = 0;
}

/*
=============
V_CheckBonusCshift

Rebuilds the gamma tables when a gamma cvar changed
=============
*/
int V_CheckBonusCshift(void)
{
	if (old_gamma != cv_v_gamma.value || old_lightgamma != cv_lightgamma.value || old_brightness != cv_brightness.value)
	{
		old_gamma = cv_v_gamma.value;
		old_lightgamma = cv_lightgamma.value;
		old_brightness = cv_brightness.value;

		V_BuildGammaTables(cv_v_gamma.value);
		S_AmbientOn();
		vid_gamma_changed = 1;
		return 1;
	}

	return 0;
}

/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage(void)
{
	int			armor, blood;
	vec3_t		from;
	float		count;
	entity_t	*ent;
	vec3_t		forward, right, up;
	float		side;
	float		upward;
	float		len;

	armor = MSG_ReadByte();
	blood = MSG_ReadByte();
	from[0] = MSG_ReadCoord();
	from[1] = MSG_ReadCoord();
	from[2] = MSG_ReadCoord();

	count = (armor + blood) * 0.5f;
	if (count < 10)
		count = 10;

	cshifts[CSHIFT_DAMAGE].percent = (int)(count * 4) + cshifts[CSHIFT_DAMAGE].percent;
	if (cshifts[CSHIFT_DAMAGE].percent < 0)
		cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cshifts[CSHIFT_DAMAGE].percent > 150)
		cshifts[CSHIFT_DAMAGE].percent = 150;

	if (blood >= armor)
	{
		if (armor)
		{
			cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
			cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
			cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
		}
		else
		{
			cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
			cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
			cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
		}
	}
	else
	{
		cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}

//
// calculate view angle kicks
//
	ent = &cl_entities[cl_viewentity];

	VectorSubtract(from, ent->origin, from);
	len = VectorLength(from);
	VectorNormalize(from);

	AngleVectors(ent->angles, forward, right, up);

	side = DotProduct(from, right);
	upward = DotProduct(from, up);

	v_dmg_roll = cv_v_kickroll.value * count * side;
	v_dmg_pitch = cv_v_kickpitch.value * count * upward;
	v_dmg_time = cv_v_kicktime.value;

//
// remember the direction for the damage indicators
//
	if (len > 50)
	{
		float	f;

		if (upward <= 0)
		{
			f = fabs(upward);
			if (f > 0.3f && f > v_vertical_scale)
				v_vertical_scale = f;
		}
		else if (upward > 0.3f && upward > v_vertical_punch)
		{
			v_vertical_punch = upward;
		}

		if (side <= 0)
		{
			f = fabs(side);
			if (f > 0.3f && f > v_lateral_scale)
				v_lateral_scale = f;
		}
		else if (side > 0.3f && side > v_lateral_punch)
		{
			v_lateral_punch = side;
		}
	}
	else
	{	// close by, from all directions
		v_lateral_scale = 1;
		v_lateral_punch = 1;
		v_vertical_scale = 1;
		v_vertical_punch = 1;
	}
}

/*
==============================================================================

						VIEW RENDERING

==============================================================================
*/

/*
===============
V_CalcBob
===============
*/
float V_CalcBob(void)
{
	float	bob;
	float	cycle;
	float	speed;

	cycle = cl_time / cv_cl_bobcycle.value;
	cycle = cl_time - (double)(int)cycle * cv_cl_bobcycle.value;
	cycle /= cv_cl_bobcycle.value;

	if (cycle < cv_cl_bobup.value)
		cycle = cycle * (float)M_PI / cv_cl_bobup.value;
	else
		cycle = (cycle - cv_cl_bobup.value) * (float)M_PI / (1.0f - cv_cl_bobup.value) + (float)M_PI;

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)
	speed = sqrt(cl_velocity[0] * cl_velocity[0] + cl_velocity[1] * cl_velocity[1]);
	bob = (sin(cycle) * 0.7 + 0.3) * speed * cv_cl_bob.value;

	if (bob > 4)
		return 4;
	if (bob < -7)
		return -7;
	return bob;
}

/*
===============
V_NormalizeAngles

Returns the angle in -180 to 180
===============
*/
double V_NormalizeAngles(float angle)
{
	float	a;

	a = anglemod(angle);
	if (a > 180)
		return (float)(a - 360);
	return a;
}

/*
==================
V_CalcGunAngle

The weapon model follows the view angles with a lag
==================
*/
void V_CalcGunAngle(void)
{
	float	yaw, pitch, move;

	pitch = -r_refdef_viewangles[PITCH];

	yaw = V_NormalizeAngles(r_refdef_viewangles[YAW] - r_refdef_viewangles[YAW]) * 0.4f;
	if (yaw > 10)
		yaw = 10;
	if (yaw < -10)
		yaw = -10;

	pitch = V_NormalizeAngles(-(r_refdef_viewangles[PITCH] + pitch)) * 0.4f;
	if (pitch > 10)
		pitch = 10;
	if (pitch < -10)
		pitch = -10;

	move = host_frametime * 20.0f;

	if (v_gunyaw >= yaw)
	{
		if (v_gunyaw - move > yaw)
			yaw = v_gunyaw - move;
	}
	else
	{
		if (v_gunyaw + move < yaw)
			yaw = v_gunyaw + move;
	}

	if (v_gunpitch >= pitch)
	{
		if (v_gunpitch - move <= pitch)
			pitch = v_gunpitch - move;
	}
	else
	{
		if (v_gunpitch + move < pitch)
			pitch = v_gunpitch + move;
	}

	viewent_entity.angles[YAW] = r_refdef_viewangles[YAW] + yaw;
	viewent_entity.angles[PITCH] = -(r_refdef_viewangles[PITCH] + pitch);
	v_gunyaw = yaw;
	v_gunpitch = pitch;

	viewent_entity.angles[ROLL] = viewent_entity.angles[ROLL] - (float)sin(cl_time * cv_v_iroll_cycle.value) * cv_v_iroll_level.value * cv_v_idlescale.value;
	viewent_entity.angles[PITCH] = viewent_entity.angles[PITCH] - (float)sin(cl_time * cv_v_ipitch_cycle.value) * cv_v_idlescale.value * cv_v_ipitch_level.value;
	viewent_entity.angles[YAW] = viewent_entity.angles[YAW] - (float)sin(cl_time * cv_v_iyaw_cycle.value) * cv_v_iyaw_level.value * cv_v_idlescale.value;
}

/*
==============
V_CalcViewOrigin
==============
*/
void V_CalcViewOrigin(void)
{
	entity_t	*ent;

	ent = &cl_entities[cl_viewentity];

	r_refdef_vieworg[0] = ent->origin[0] + 14;
	r_refdef_vieworg[1] = ent->origin[1] + 14;
	r_refdef_vieworg[2] = ent->origin[2] + 30;
}

/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
float V_CalcRoll(vec3_t angles, vec3_t velocity)
{
	vec3_t	forward, right, up;
	float	sign;
	float	side;
	float	value;

	AngleVectors(angles, forward, right, up);

	side = DotProduct(velocity, right);
	sign = 1;
	if (side < 0)
		sign = -1;
	side = fabs(side);

	if (side >= cv_cl_rollspeed.value)
		value = cv_cl_rollangle.value;
	else
		value = side * cv_cl_rollangle.value / cv_cl_rollspeed.value;

	return value * sign;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when
===============
*/
void V_DriftPitch(void)
{
	float	delta, move;

	if (cl_intermission || !cl_onground || cl_dead)
	{
		v_driftmove = 0;
		v_pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (v_nodrift)
	{
		if ((float)fabs(cl_forwardmove) >= cl_forwardspeed.value)
			v_driftmove += (float)host_frametime;
		else
			v_driftmove = 0;

		if (v_driftmove > cv_v_centermove.value)
			V_StartPitchDrift();
		return;
	}

	delta = cl_idealpitch - cl_viewangles[PITCH];
	if (delta == 0)
	{
		v_pitchvel = 0;
		return;
	}

	move = (float)host_frametime * v_pitchvel;
	v_pitchvel = (float)host_frametime * cv_v_centerspeed.value + v_pitchvel;

	if (delta <= 0)
	{
		if (delta < 0)
		{
			if (-delta < move)
			{
				v_pitchvel = 0;
				move = -delta;
			}
			cl_viewangles[PITCH] -= move;
		}
	}
	else
	{
		if (move > delta)
		{
			v_pitchvel = 0;
			move = cl_idealpitch - cl_viewangles[PITCH];
		}
		cl_viewangles[PITCH] += move;
	}
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle(void)
{
	r_refdef_viewangles[ROLL] = (float)sin(cl_time * cv_v_iroll_cycle.value) * cv_v_iroll_level.value * cv_v_idlescale.value + r_refdef_viewangles[ROLL];
	r_refdef_viewangles[PITCH] = (float)sin(cl_time * cv_v_ipitch_cycle.value) * cv_v_idlescale.value * cv_v_ipitch_level.value + r_refdef_viewangles[PITCH];
	r_refdef_viewangles[YAW] = (float)sin(cl_time * cv_v_iyaw_cycle.value) * cv_v_iyaw_level.value * cv_v_idlescale.value + r_refdef_viewangles[YAW];
}

/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll(void)
{
	float	side;
	float	frac;

	side = V_CalcRoll(cl_entities[cl_viewentity].angles, cl_velocity);
	r_refdef_viewangles[ROLL] += side;

	if (v_dmg_time > 0)
	{
		frac = v_dmg_time / cv_v_kicktime.value;
		r_refdef_viewangles[ROLL] = v_dmg_roll * frac + r_refdef_viewangles[ROLL];
		r_refdef_viewangles[PITCH] = v_dmg_pitch * frac + r_refdef_viewangles[PITCH];
		v_dmg_time -= (float)host_frametime;
	}

	if (cl_health <= 0)
		r_refdef_viewangles[ROLL] = 80;	// dead view angle
}

/*
==================
V_CalcIntermissionRefdef
==================
*/
void V_CalcIntermissionRefdef(void)
{
	entity_t	*ent;
	float		old;

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl_viewentity];

	VectorCopy(ent->origin, r_refdef_vieworg);
	VectorCopy(ent->angles, r_refdef_viewangles);
	viewent_entity.model = NULL;
	cl_viewent_valid = 0;

// always idle in intermission
	old = cv_v_idlescale.value;
	cv_v_idlescale.value = 1;
	V_AddIdle();
	cv_v_idlescale.value = old;
}

/*
==================
V_CalcRefdef
==================
*/
void V_CalcRefdef(void)
{
	entity_t	*ent;
	int			i;
	vec3_t		forward, right, up;
	vec3_t		angles;
	float		bob;

	V_DriftPitch();

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl_viewentity];

	bob = V_CalcBob();

// refresh position
	// never let it sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	r_refdef_vieworg[0] = ent->origin[0] + 1.0f / 32;
	r_refdef_vieworg[1] = ent->origin[1] + 1.0f / 32;
	r_refdef_vieworg[2] = ent->origin[2] + cl_viewheight + bob + 1.0f / 32;

	VectorCopy(cl_viewangles, r_refdef_viewangles);
	V_CalcViewRoll();
	V_AddIdle();

// offsets
	angles[PITCH] = cl_viewangles[PITCH];
	angles[YAW] = cl_viewangles[YAW];
	angles[ROLL] = ent->angles[ROLL];

	AngleVectors(angles, forward, right, up);

	for (i = 0; i < 3; i++)
		r_refdef_vieworg[i] += forward[i] * v_punch_forward + up[i] * v_punch_up + right[i] * v_punch_right;

	if (v_screenshake_enabled)
	{
		vec3_t	shakeangles;
		vec3_t	shakeforward, shakeright, shakeup;

		shakeangles[PITCH] = v_screenshake_angles[0];
		shakeangles[YAW] = v_screenshake_angles[1];
		shakeangles[ROLL] = 0;

		AngleVectors(shakeangles, shakeforward, shakeright, shakeup);
		for (i = 0; i < 3; i++)
			r_refdef_vieworg[i] -= shakeforward[i] * v_screenshake_scale;
	}

// set up gun position
	VectorCopy(cl_viewangles, viewent_entity.angles);

	V_CalcGunAngle();

	VectorCopy(ent->origin, viewent_entity.origin);
	viewent_entity.origin[2] += cl_viewheight;

	for (i = 0; i < 3; i++)
		viewent_entity.origin[i] += forward[i] * bob * 0.4f;
	viewent_entity.origin[2] += bob;

	viewent_entity.angles[YAW] += bob * -0.5f;
	viewent_entity.angles[ROLL] -= bob;
	viewent_entity.angles[PITCH] += bob * -0.3f;

// fudge position around to keep amount of weapon visible
// roughly equal with different FOV
	viewent_entity.origin[2] -= 1;
	if (scr_viewsize.value == 110)
		viewent_entity.origin[2] += 1;
	else if (scr_viewsize.value == 100)
		viewent_entity.origin[2] += 2;
	else if (scr_viewsize.value == 90)
		viewent_entity.origin[2] += 1;
	else if (scr_viewsize.value == 80)
		viewent_entity.origin[2] += 0.5f;

	viewent_entity.model = cl_model_precache[cl_weaponmodel];
	viewent_entity.frame = cl_weaponframe;
	viewent_entity.colormap = vid.colormap;
	cl_viewent_valid = (viewent_entity.model != NULL);

// set up the refresh position
	VectorAdd(r_refdef_viewangles, cl_punchangle, r_refdef_viewangles);

// smooth out stair step ups
	if (cl_onground && ent->origin[2] - cl_oldz > 0)
	{
		float	steptime;

		steptime = cl_time - cl_oldtime;
		if (steptime < 0)
			steptime = 0;

		cl_oldz = steptime * 80 + cl_oldz;
		if (cl_oldz > ent->origin[2])
			cl_oldz = ent->origin[2];
		if (ent->origin[2] - cl_oldz > 12)
			cl_oldz = ent->origin[2] - 12;

		r_refdef_vieworg[2] = r_refdef_vieworg[2] - ent->origin[2] + cl_oldz;
		viewent_entity.origin[2] = viewent_entity.origin[2] - ent->origin[2] + cl_oldz;
	}
	else
	{
		cl_oldz = ent->origin[2];
	}

	if (v_screenshake_enabled)
	{
		r_refdef_viewangles[PITCH] = v_screenshake_angles[0];
		r_refdef_viewangles[YAW] = v_screenshake_angles[1];
		r_refdef_viewangles[ROLL] = 0;
	}
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void V_RenderView(void)
{
	int		i;
	float	halffov;

	if (con_forcedup)
		return;

// don't allow cheats in multiplayer
	if (cl_maxclients > 1)
	{
		Cvar_Set("scr_ofsx", "0");
		Cvar_Set("scr_ofsy", "0");
		Cvar_Set("scr_ofsz", "0");
	}

	if (cl_intermission)
	{	// intermission / finale rendering
		V_CalcIntermissionRefdef();
	}
	else if (!cl_paused)
	{
		V_CalcRefdef();
	}

	R_PushDlights();

	if (cl_waterwarp == 0)
	{
		R_RenderView();
	}
	else
	{	// render the two halves of the view from both sides of the eye
		halffov = r_fov * 0.5f;

		r_refdef_viewangles[YAW] -= cl_yawoffset;
		r_refdef_vrect_width *= 2;
		r_fov = halffov;

		for (i = 0; i < 3; i++)
			r_refdef_vieworg[i] -= vpn[i] * cl_waterwarp;

		R_RenderView();

		r_refdef_vrect_x += r_refdef_vrect_width / 2;

		R_PushDlights();
		r_refdef_viewangles[YAW] += cl_yawoffset * 2.0f;

		for (i = 0; i < 3; i++)
			r_refdef_vieworg[i] += vpn[i] * cl_waterwarp * 2.0f;

		R_RenderView();

		r_refdef_vrect_height *= 2;
		r_fov *= 2.0f;
		r_refdef_vrect_x -= r_refdef_vrect_width / 2;
		r_refdef_vrect_width /= 2;
	}

	if (crosshair.value)
		Draw_Character(scr_vrect.width / 2 + scr_vrect.x, scr_vrect.height / 2 + scr_vrect.y, '+');
}

//============================================================================

/*
==============
V_StartPitchDrift
==============
*/
void V_StartPitchDrift(void)
{
	// something else may be keeping it from drifting
	if (v_laststop != cl_time && (v_nodrift || v_pitchvel == 0))
	{
		v_pitchvel = cv_v_centerspeed.value;
		v_nodrift = false;
		v_driftmove = 0;
	}
}

/*
==============
V_StopPitchDrift
==============
*/
void V_StopPitchDrift(void)
{
	v_nodrift = true;
	v_pitchvel = 0;
	v_laststop = cl_time;
}

/*
=============
V_Init
=============
*/
void V_Init(void)
{
	Cmd_AddCommand("v_cshift", V_cshift_f);
	Cmd_AddCommand("bf", V_BonusFlash_f);
	Cmd_AddCommand("centerview", V_StartPitchDrift);

	Cvar_RegisterVariable(&cv_cl_bob);
	Cvar_RegisterVariable(&cv_cl_bobcycle);
	Cvar_RegisterVariable(&cv_cl_bobup);
	Cvar_RegisterVariable(&cv_cl_rollangle);
	Cvar_RegisterVariable(&cv_cl_rollspeed);
	Cvar_RegisterVariable(&cv_v_idlescale);
	Cvar_RegisterVariable(&cv_v_ipitch_cycle);
	Cvar_RegisterVariable(&cv_v_ipitch_level);
	Cvar_RegisterVariable(&cv_v_iroll_cycle);
	Cvar_RegisterVariable(&cv_v_iroll_level);
	Cvar_RegisterVariable(&cv_v_iyaw_cycle);
	Cvar_RegisterVariable(&cv_v_iyaw_level);
	Cvar_RegisterVariable(&cv_v_kicktime);
	Cvar_RegisterVariable(&cv_v_kickroll);
	Cvar_RegisterVariable(&cv_v_kickpitch);
	Cvar_RegisterVariable(&cv_v_centermove);
	Cvar_RegisterVariable(&cv_v_centerspeed);
	Cvar_RegisterVariable(&cv_scr_ofsx);
	Cvar_RegisterVariable(&cv_scr_ofsy);
	Cvar_RegisterVariable(&cv_scr_ofsz);
	Cvar_RegisterVariable(&crosshair);
	Cvar_RegisterVariable(&cv_gl_cshiftpercent);
	Cvar_RegisterVariable(&cv_v_contentblend);
	Cvar_RegisterVariable(&cv_v_dmg_pitch);
	Cvar_RegisterVariable(&cv_v_dmg_roll);
	Cvar_RegisterVariable(&cv_v_dmg_time);

	V_BuildGammaTables(2.5f);

	Cvar_RegisterVariable(&cv_v_gamma);
	Cvar_RegisterVariable(&cv_lightgamma);
	Cvar_RegisterVariable(&cv_texgamma);
	Cvar_RegisterVariable(&cv_brightness);
	Cvar_RegisterVariable(&cv_lambert);
	Cvar_RegisterVariable(&cv_direct);
}
