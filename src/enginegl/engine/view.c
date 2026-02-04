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

int v_screenshake_enabled = 0;
float v_screenshake_angles[3] = { 0.0f, 0.0f, 0.0f };
float v_screenshake_scale = 0.0f;

cvar_t cv_cl_bob = { "cl_bob", "0.01" };
cvar_t cv_cl_bobcycle = { "cl_bobcycle", "0.8" };
cvar_t cv_cl_bobup = { "cl_bobup", "0.5" };
cvar_t cv_cl_rollangle = { "cl_rollangle", "2.0" };
cvar_t cv_cl_rollspeed = { "cl_rollspeed", "200" };

cvar_t cv_v_idlescale = { "v_idlescale", "0" };
cvar_t cv_v_ipitch_cycle = { "v_ipitch_cycle", "1" };
cvar_t cv_v_ipitch_level = { "v_ipitch_level", "0.3" };
cvar_t cv_v_iroll_cycle = { "v_iroll_cycle", "0.5" };
cvar_t cv_v_iroll_level = { "v_iroll_level", "0.1" };
cvar_t cv_v_iyaw_cycle = { "v_iyaw_cycle", "2" };
cvar_t cv_v_iyaw_level = { "v_iyaw_level", "0.3" };

cvar_t cv_v_kicktime = { "v_kicktime", "0.5" };
cvar_t cv_v_kickroll = { "v_kickroll", "0.6" };
cvar_t cv_v_kickpitch = { "v_kickpitch", "0.6" };

cvar_t cv_v_centermove = { "v_centermove", "0.15" };
cvar_t cv_v_centerspeed = { "v_centerspeed", "500" };

cvar_t cv_scr_ofsx = { "scr_ofsx", "0" };
cvar_t cv_scr_ofsy = { "scr_ofsy", "0" };
cvar_t cv_scr_ofsz = { "scr_ofsz", "0" };

cvar_t crosshair = { "crosshair", "1" };
cvar_t cv_gl_cshiftpercent = { "gl_cshiftpercent", "100" };
cvar_t cv_v_contentblend = { "v_contentblend", "1" };

cvar_t cv_v_dmg_pitch = { "v_dmg_pitch", "0.6" };
cvar_t cv_v_dmg_roll = { "v_dmg_roll", "1.0" };
cvar_t cv_v_dmg_time = { "v_dmg_time", "0.5" };
cvar_t cv_v_gamma = { "gamma", "2.5", true };
cvar_t cv_brightness = { "brightness", "0.0", true };
cvar_t cv_lightgamma = { "lightgamma", "2.5" };
cvar_t cv_texgamma = { "texgamma", "1.8" };
cvar_t cv_lambert = { "lambert", "1.7" };
cvar_t cv_direct = { "direct", "0.9" };

static qboolean v_nodrift = true;
static float v_pitchvel;
static float v_driftmove;
static double v_laststop;

extern char *Cmd_Argv(int arg);
extern float anglemod(float a);
extern double cos(double x);
extern entity_t viewent_entity;
extern int cl_viewent_valid;

typedef struct
{
	int destcolor[3];
	int percent;
} cshift_t;

enum
{
	CSHIFT_CONTENTS = 0,
	CSHIFT_DAMAGE = 1,
	CSHIFT_BONUS = 2,
	CSHIFT_POWERUP = 3,
	NUM_CSHIFTS = 4
};

static cshift_t cshift_empty = { { 0, 0, 0 }, 0 };
static const cshift_t cshift_water = { { 130, 80, 50 }, 128 };
static const cshift_t cshift_slime = { { 0, 25, 5 }, 150 };
static const cshift_t cshift_lava = { { 255, 80, 0 }, 150 };

static cshift_t cshifts[NUM_CSHIFTS];
static cshift_t prev_cshifts[NUM_CSHIFTS];
extern float	v_blend[4];

extern byte	gammatable[256];
int lightgammatable[1024];
byte ramps_r[256];
byte ramps_g[256];
byte ramps_b[256];

extern float r_refdef_viewangles[3];
float v_dmg_kick_pitch;
float v_dmg_kick_roll;
float v_dmg_time;
float v_dmg_roll;
float v_dmg_pitch;

float v_dmg_pitch_kick;
float v_dmg_roll_kick;
float v_punchangle_decay;
float v_punch_forward;
float v_punch_right;
float v_punch_up;

float v_lateral_punch;
float v_lateral_scale;
float v_vertical_punch;
float v_vertical_scale;

float v_iroll_cycle;
float v_ipitch_cycle;
float v_iyaw_cycle;
float v_iroll_level;
float v_ipitch_level;
float v_iyaw_level;
float v_idlescale;

void V_CheckDamageCshift(void);
int V_CheckBonusCshift(void);
void VID_ShiftPalette(void);
float V_CalcBob(void);
void V_ParseDamage(void);

int V_cshift_f(void)
{
	const char *rStr;
	const char *gStr;
	const char *bStr;
	const char *percentStr;
	int result;

	rStr = Cmd_Argv(1);
	cshift_empty.destcolor[0] = atoi(rStr);
	gStr = Cmd_Argv(2);
	cshift_empty.destcolor[1] = atoi(gStr);
	bStr = Cmd_Argv(3);
	cshift_empty.destcolor[2] = atoi(bStr);
	percentStr = Cmd_Argv(4);
	result = atoi(percentStr);
	cshift_empty.percent = result;
	return result;
}

void V_BonusFlash_f(void)
{
	cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cshifts[CSHIFT_BONUS].percent = 50;
}

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

int V_CalcPowerupCshift(void)
{

	if ((cl_items & 0x40) != 0)
	{
		cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cshifts[CSHIFT_POWERUP].percent = 30;
		return 0;
	}

	if ((cl_items & 0x20) != 0)
	{
		cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cshifts[CSHIFT_POWERUP].percent = 20;
		return 0;
	}

	if ((cl_items & 8) != 0)
	{
		cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cshifts[CSHIFT_POWERUP].percent = 100;
		return 100;
	}

	if ((cl_items & 0x10) != 0)
	{
		cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cshifts[CSHIFT_POWERUP].percent = 30;
		return 255;
	}

	cshifts[CSHIFT_POWERUP].percent = 0;
	return 0;
}

void V_CalcBlend(void)
{
	float r, g, b, a;
	int i;

	r = 0.0f;
	g = 0.0f;
	b = 0.0f;
	a = 0.0f;

	for (i = 0; i < NUM_CSHIFTS; ++i)
	{
		float percent = (float)cshifts[i].percent / 255.0f;
		if (percent != 0.0f)
		{
			float newAlpha = (1.0f - a) * percent + a;
			float blend = percent / newAlpha;
			float invBlend = 1.0f - blend;

			r = (float)cshifts[i].destcolor[0] * blend + invBlend * r;
			g = (float)cshifts[i].destcolor[1] * blend + invBlend * g;
			b = (float)cshifts[i].destcolor[2] * blend + invBlend * b;
			a = newAlpha;
		}
	}

	v_blend[0] = r / 255.0f;
	v_blend[1] = g / 255.0f;
	v_blend[2] = b / 255.0f;
	v_blend[3] = a;

	if (v_blend[3] > 1.0f)
		v_blend[3] = 1.0f;
	if (v_blend[3] < 0.0f)
		v_blend[3] = 0.0f;
}

void V_UpdatePalette(void)
{
	int changed;
	int gammaChanged;
	int i;
	byte *palSrc;
	char localPal[768];
	char *palDest;

	changed = 0;
	V_CalcPowerupCshift();

	for (i = 0; i < NUM_CSHIFTS; ++i)
	{
		if (prev_cshifts[i].percent != cshifts[i].percent
			|| prev_cshifts[i].destcolor[0] != cshifts[i].destcolor[0]
			|| prev_cshifts[i].destcolor[1] != cshifts[i].destcolor[1]
			|| prev_cshifts[i].destcolor[2] != cshifts[i].destcolor[2])
		{
			changed = 1;
			prev_cshifts[i] = cshifts[i];
		}
	}

	cshifts[CSHIFT_DAMAGE].percent = (int)((double)cshifts[CSHIFT_DAMAGE].percent - host_frametime * 150.0);
	if (cshifts[CSHIFT_DAMAGE].percent < 0)
		cshifts[CSHIFT_DAMAGE].percent = 0;

	V_CheckDamageCshift();

	cshifts[CSHIFT_BONUS].percent = (int)((double)cshifts[CSHIFT_BONUS].percent - host_frametime * 100.0);
	if (cshifts[CSHIFT_BONUS].percent < 0)
		cshifts[CSHIFT_BONUS].percent = 0;

	gammaChanged = V_CheckBonusCshift();

	if (changed || gammaChanged)
	{
		float blendInv;
		float blendR, blendG, blendB;

		V_CalcBlend();

		blendInv = 1.0f - v_blend[3];
		blendR = v_blend[0] * v_blend[3] * 255.0f;
		blendG = v_blend[1] * v_blend[3] * 255.0f;
		blendB = v_blend[2] * v_blend[3] * 255.0f;

		for (i = 0; i < 256; ++i)
		{
			float base = (float)i * blendInv;
			int rOut = (int)(base + blendR);
			int gOut = (int)(base + blendG);
			int bOut = (int)(base + blendB);

			if (rOut > 255) rOut = 255;
			if (gOut > 255) gOut = 255;
			if (bOut > 255) bOut = 255;
			if (rOut < 0) rOut = 0;
			if (gOut < 0) gOut = 0;
			if (bOut < 0) bOut = 0;

			ramps_r[i] = gammatable[rOut];
			ramps_g[i] = gammatable[gOut];
			ramps_b[i] = gammatable[bOut];
		}

		palSrc = host_basepal;
		palDest = localPal;
		for (i = 0; i < 256; ++i)
		{
			int rVal = palSrc[0];
			int gVal = palSrc[1];
			int bVal = palSrc[2];
			palSrc += 3;

			palDest[0] = ramps_r[rVal];
			palDest[1] = ramps_g[gVal];
			palDest[2] = ramps_b[bVal];
			palDest += 3;
		}

		VID_ShiftPalette();
	}
}

void V_CheckDamageCshift(void)
{

	v_lateral_scale -= (float)host_frametime;
	if (v_lateral_scale < 0.0f)
		v_lateral_scale = 0.0f;

	v_lateral_punch -= (float)host_frametime;
	if (v_lateral_punch < 0.0f)
		v_lateral_punch = 0.0f;

	v_vertical_punch -= (float)host_frametime;
	if (v_vertical_punch < 0.0f)
		v_vertical_punch = 0.0f;

	v_vertical_scale -= (float)host_frametime;
	if (v_vertical_scale < 0.0f)
		v_vertical_scale = 0.0f;
}

int V_CheckBonusCshift(void)
{
	extern float old_gamma;
	extern float old_lightgamma;
	extern float old_brightness;
	extern cvar_t cv_v_gamma;
	extern cvar_t cv_lightgamma;
	extern cvar_t cv_brightness;
	extern int vid_gamma_changed;
	extern int V_BuildGammaTables(float gamma);
	extern void S_AmbientOn(void);

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

void V_ParseDamage(void)
{
	int armor;
	int blood;
	vec3_t from;
	float count;
	entity_t *ent;
	vec3_t dir;
	vec3_t forward, right, up;
	float side;
	float upDot;
	float len;

	armor = MSG_ReadByte();
	blood = MSG_ReadByte();
	from[0] = MSG_ReadCoord();
	from[1] = MSG_ReadCoord();
	from[2] = MSG_ReadCoord();

	count = (float)(armor + blood) * 0.5f;
	if (count < 10.0f)
		count = 10.0f;

	cshifts[CSHIFT_DAMAGE].percent = (int)(count * 4.0f) + cshifts[CSHIFT_DAMAGE].percent;
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

	ent = &cl_entities[cl_viewentity];
	VectorSubtract(from, ent->origin, dir);
	len = VectorLength(dir);
	VectorNormalize(dir);

	AngleVectors(ent->angles, forward, right, up);
	side = DotProduct(dir, right);
	upDot = DotProduct(dir, up);

	v_dmg_roll = cv_v_kickroll.value * count * side;
	v_dmg_pitch = cv_v_kickpitch.value * count * upDot;
	v_dmg_time = cv_v_kicktime.value;

	if (len > 50.0f)
	{
		float absVal;

		if (upDot <= 0.0f)
		{
			absVal = (float)fabs(upDot);
			if (absVal > 0.3f && absVal > v_vertical_scale)
				v_vertical_scale = absVal;
		}
		else if (upDot > 0.3f && upDot > v_vertical_punch)
		{
			v_vertical_punch = upDot;
		}

		if (side <= 0.0f)
		{
			absVal = (float)fabs(side);
			if (absVal > 0.3f && absVal > v_lateral_scale)
				v_lateral_scale = absVal;
		}
		else if (side > 0.3f && side > v_lateral_punch)
		{
			v_lateral_punch = side;
		}
	}
	else
	{
		v_lateral_scale = 1.0f;
		v_lateral_punch = 1.0f;
		v_vertical_scale = 1.0f;
		v_vertical_punch = 1.0f;
	}
}

float V_CalcBob(void)
{
	float cycle;
	float bob;
	float speed;
	float angle;

	cycle = (float)(cl_time / cv_cl_bobcycle.value);
	cycle = (float)(cl_time - (double)(int)cycle * cv_cl_bobcycle.value);
	cycle = cycle / cv_cl_bobcycle.value;

	if (cycle < cv_cl_bobup.value)
		angle = cycle * 3.141592653589793f / cv_cl_bobup.value;
	else
		angle = (cycle - cv_cl_bobup.value) * 3.141592653589793f / (1.0f - cv_cl_bobup.value) + 3.141592653589793f;

	speed = (float)sqrt(cl_velocity[0] * cl_velocity[0] + cl_velocity[1] * cl_velocity[1]);
	bob = (float)((sin(angle) * 0.7 + 0.3) * speed * cv_cl_bob.value);

	if (bob > 4.0f)
		return 4.0f;
	if (bob < -7.0f)
		return -7.0f;
	return bob;
}

double V_NormalizeAngles(float angle)
{
	float normalized;

	normalized = anglemod(angle);
	if (normalized > 180.0f)
		return (float)(normalized - 360.0f);
	return normalized;
}

void V_CalcViewAngles(void)
{
	float pitchTmp;
	float pitch1;
	float rollTmp;
	float roll1;
	float pitchDelta, rollDelta;
	float kickSpeed;
	float pitch2, roll2;
	float pitchTarget, rollTarget;

	rollTarget = -r_refdef_viewangles[0];
	pitchDelta = r_refdef_viewangles[1] - r_refdef_viewangles[1];
	pitchTarget = V_NormalizeAngles(pitchDelta) * 0.4f;
	if (pitchTarget > 10.0f)
		pitchTarget = 10.0f;
	if (pitchTarget < -10.0f)
		pitchTarget = -10.0f;

	rollDelta = -(r_refdef_viewangles[0] + rollTarget);
	rollTarget = V_NormalizeAngles(rollDelta) * 0.4f;
	if (rollTarget > 10.0f)
		rollTarget = 10.0f;
	if (rollTarget < -10.0f)
		rollTarget = -10.0f;

	kickSpeed = host_frametime * 20.0f;
	pitchTmp = v_dmg_kick_pitch;
	if (v_dmg_kick_pitch >= pitchTarget)
	{
		pitch2 = pitchTmp - kickSpeed;
		if (pitch2 <= pitchTarget)
			goto LABEL_15;
		pitch1 = pitchTmp - kickSpeed;
	}
	else
	{
		pitch2 = pitchTmp + kickSpeed;
		if (pitch2 >= pitchTarget)
			goto LABEL_15;
		pitch1 = pitchTmp + kickSpeed;
	}
	pitchTarget = pitch1;

LABEL_15:

	rollTmp = v_dmg_kick_roll;
	if (v_dmg_kick_roll >= rollTarget)
	{
		roll2 = rollTmp - kickSpeed;
		if (roll2 <= rollTarget)
		{
			roll1 = roll2;
			goto LABEL_20;
		}
	}
	else
	{
		roll2 = rollTmp + kickSpeed;
		if (roll2 < rollTarget)
		{
			roll1 = roll2;
LABEL_20:
			rollTarget = roll1;
		}
	}

	viewent_entity.angles[1] = r_refdef_viewangles[1] + pitchTarget;
	viewent_entity.angles[0] = -(r_refdef_viewangles[0] + rollTarget);
	v_dmg_kick_pitch = pitchTarget;
	v_dmg_kick_roll = rollTarget;

	viewent_entity.angles[2] = viewent_entity.angles[2]
		- (float)sin(cl_time * cv_v_iroll_cycle.value)
		* cv_v_iroll_level.value
		* cv_v_idlescale.value;

	viewent_entity.angles[0] = viewent_entity.angles[0]
		- (float)sin(cl_time * cv_v_ipitch_cycle.value)
		* cv_v_idlescale.value
		* cv_v_ipitch_level.value;

	viewent_entity.angles[1] = viewent_entity.angles[1]
		- (float)sin(cl_time * cv_v_iyaw_cycle.value)
		* cv_v_iyaw_level.value
		* cv_v_idlescale.value;
}

void V_CalcViewOrigin(void)
{
	float *playerState;
	float x1, y1, z1;
	float x2, y2, z2;
	float xOut, yOut, zOut;

	playerState = (float *)cl_entities + 76 * cl_viewentity;

	x1 = playerState[26] - 14.0f;
	xOut = x1;

	x2 = playerState[26] + 14.0f;
	xOut = x2;

	r_refdef_vieworg[0] = xOut;

	y1 = playerState[27] - 14.0f;
	yOut = y1;

	y2 = playerState[27] + 14.0f;
	yOut = y2;

	r_refdef_vieworg[1] = yOut;

	z1 = playerState[28] - 22.0f;
	zOut = z1;

	z2 = playerState[28] + 30.0f;
	zOut = z2;

	r_refdef_vieworg[2] = zOut;
}

float V_CalcRoll(vec3_t angles, vec3_t velocity)
{
	vec3_t forward;
	vec3_t right;
	vec3_t up;
	float side;
	float absSide;
	float roll;
	float sign;

	AngleVectors(angles, forward, right, up);

	side = velocity[0] * right[0] + velocity[1] * right[1] + velocity[2] * right[2];
	sign = 1.0f;
	if (side < 0.0f)
		sign = -1.0f;

	absSide = (float)fabs(side);
	if (absSide >= cv_cl_rollspeed.value)
		roll = cv_cl_rollangle.value;
	else
		roll = absSide * cv_cl_rollangle.value / cv_cl_rollspeed.value;

	return roll * sign;
}

void V_DriftPitch(void)
{
	float delta;
	float move;
	float absDelta;

	extern void V_StartPitchDrift(void);
	extern int cl_dead;
	extern float cl_forwardmove;

	if (cl_intermission || !cl_onground || cl_dead)
	{
		v_driftmove = 0.0f;
		v_pitchvel = 0.0f;
		return;
	}

	if (v_nodrift)
	{
		if ((float)fabs(cl_forwardmove) >= cl_forwardspeed.value)
			v_driftmove = v_driftmove + (float)host_frametime;
		else
			v_driftmove = 0.0f;

		if (v_driftmove > cv_v_centermove.value)
			V_StartPitchDrift();

		return;
	}

	delta = cl_idealpitch - cl_viewangles[0];
	if (delta == 0.0f)
	{
		v_pitchvel = 0.0f;
		return;
	}

	move = (float)host_frametime * v_pitchvel;
	v_pitchvel = (float)host_frametime * cv_v_centerspeed.value + v_pitchvel;

	if (delta <= 0.0f)
	{
		if (delta < 0.0f)
		{
			absDelta = -delta;
			if (absDelta < move)
			{
				v_pitchvel = 0.0f;
				move = absDelta;
			}
			cl_viewangles[0] = cl_viewangles[0] - move;
		}
	}
	else
	{
		if (move > delta)
		{
			v_pitchvel = 0.0f;
			move = cl_idealpitch - cl_viewangles[0];
		}
		cl_viewangles[0] = cl_viewangles[0] + move;
	}
}

void V_AddIdle(void)
{
	r_refdef_viewangles[2] = (float)sin(cl_time * cv_v_iroll_cycle.value)
		* cv_v_iroll_level.value
		* cv_v_idlescale.value
		+ r_refdef_viewangles[2];

	r_refdef_viewangles[0] = (float)sin(cl_time * cv_v_ipitch_cycle.value)
		* cv_v_idlescale.value
		* cv_v_ipitch_level.value
		+ r_refdef_viewangles[0];

	r_refdef_viewangles[1] = (float)sin(cl_time * cv_v_iyaw_cycle.value)
		* cv_v_iyaw_level.value
		* cv_v_idlescale.value
		+ r_refdef_viewangles[1];
}

void V_CalcViewRoll(void)
{
	float v0;
	float v1;

	v0 = V_CalcRoll(cl_entities[cl_viewentity].angles, cl_velocity);
	r_refdef_viewangles[2] = r_refdef_viewangles[2] + v0;

	if (v_dmg_time > 0.0f)
	{
		v1 = v_dmg_time / cv_v_kicktime.value;
		r_refdef_viewangles[2] = v_dmg_roll * v1 + r_refdef_viewangles[2];
		r_refdef_viewangles[0] = v_dmg_pitch * v1 + r_refdef_viewangles[0];
		v_dmg_time -= (float)host_frametime;
	}

	if (cl_health <= 0)
		r_refdef_viewangles[2] = 80.0f;
}

void V_CalcIntermissionRefdef(void)
{
	float oldIdleScale;
	entity_t *ent;

	ent = &cl_entities[cl_viewentity];

	VectorCopy(ent->origin, r_refdef_vieworg);
	VectorCopy(ent->angles, r_refdef_viewangles);

	viewent_entity.model = NULL;
	cl_viewent_valid = 0;
	oldIdleScale = cv_v_idlescale.value;
	cv_v_idlescale.value = 1.0f;

	V_AddIdle();
	cv_v_idlescale.value = oldIdleScale;
}

void V_CalcRefdef(void)
{
	entity_t *ent;
	vec3_t forward, right, up;
	vec3_t angles;
	float bob;
	int i;

	extern cvar_t scr_viewsize;

	V_DriftPitch();

	ent = &cl_entities[cl_viewentity];
	bob = V_CalcBob();

	r_refdef_vieworg[0] = ent->origin[0] + 0.03125f;
	r_refdef_vieworg[1] = ent->origin[1] + 0.03125f;
	r_refdef_vieworg[2] = ent->origin[2] + cl_viewheight + bob + 0.03125f;

	VectorCopy(cl_viewangles, r_refdef_viewangles);
	V_CalcViewRoll();
	V_AddIdle();

	angles[0] = cl_viewangles[0];
	angles[1] = cl_viewangles[1];
	angles[2] = ent->angles[2];
	AngleVectors(angles, forward, right, up);

	for (i = 0; i < 3; ++i)
		r_refdef_vieworg[i] += forward[i] * v_punch_forward + up[i] * v_punch_up + right[i] * v_punch_right;

	if (v_screenshake_enabled)
	{
		vec3_t shakeAngles;
		vec3_t shakeForward, shakeRight, shakeUp;

		shakeAngles[0] = v_screenshake_angles[0];
		shakeAngles[1] = v_screenshake_angles[1];
		shakeAngles[2] = 0.0f;

		AngleVectors(shakeAngles, shakeForward, shakeRight, shakeUp);
		for (i = 0; i < 3; ++i)
			r_refdef_vieworg[i] -= shakeForward[i] * v_screenshake_scale;
	}

	VectorCopy(cl_viewangles, viewent_entity.angles);
	V_CalcViewAngles();

	VectorCopy(ent->origin, viewent_entity.origin);
	viewent_entity.origin[2] += cl_viewheight;
	for (i = 0; i < 3; ++i)
		viewent_entity.origin[i] += forward[i] * bob * 0.4f;
	viewent_entity.origin[2] += bob;

	viewent_entity.angles[1] += bob * -0.5f;
	viewent_entity.angles[2] -= bob;
	viewent_entity.angles[0] += bob * -0.3f;

	viewent_entity.origin[2] -= 1.0f;
	if (scr_viewsize.value == 110.0f)
	{
		viewent_entity.origin[2] += 1.0f;
	}
	else if (scr_viewsize.value == 100.0f)
	{
		viewent_entity.origin[2] += 2.0f;
	}
	else if (scr_viewsize.value == 90.0f)
	{
		viewent_entity.origin[2] += 1.0f;
	}
	else if (scr_viewsize.value == 80.0f)
	{
		viewent_entity.origin[2] += 0.5f;
	}

	viewent_entity.model = cl_model_precache[cl_weaponmodel];
	viewent_entity.frame = cl_weaponframe;
	viewent_entity.colormap = (byte *)vid.colormap;
	cl_viewent_valid = (viewent_entity.model != NULL);

	r_refdef_viewangles[0] += cl_punchangle[0];
	r_refdef_viewangles[1] += cl_punchangle[1];
	r_refdef_viewangles[2] += cl_punchangle[2];

	if (cl_onground && ent->origin[2] - cl_oldz > 0.0f)
	{
		float stairDelta = (float)(cl_time - cl_oldtime);
		if (stairDelta < 0.0f)
			stairDelta = 0.0f;

		cl_oldz = stairDelta * 80.0f + cl_oldz;
		if (cl_oldz > ent->origin[2])
			cl_oldz = ent->origin[2];
		if (ent->origin[2] - cl_oldz > 12.0f)
			cl_oldz = ent->origin[2] - 12.0f;

		r_refdef_vieworg[2] = r_refdef_vieworg[2] - ent->origin[2] + cl_oldz;
		viewent_entity.origin[2] = viewent_entity.origin[2] - ent->origin[2] + cl_oldz;
	}
	else
	{
		cl_oldz = ent->origin[2];
	}

	if (v_screenshake_enabled)
	{
		r_refdef_viewangles[0] = v_screenshake_angles[0];
		r_refdef_viewangles[1] = v_screenshake_angles[1];
		r_refdef_viewangles[2] = 0.0f;
	}
}

void V_RenderView(void)
{
	extern int con_forcedup;
	extern int cl_paused;
	extern float cl_waterwarp;
	extern float cl_yawoffset;
	extern float r_fov;
	extern int r_refdef_vrect_x;
	extern int r_refdef_vrect_width;
	extern int r_refdef_vrect_height;
	extern cvar_t crosshair;

	if (con_forcedup)
		return;

	if (cl_maxclients > 1)
	{
		Cvar_Set("scr_ofsx", "0");
		Cvar_Set("scr_ofsy", "0");
		Cvar_Set("scr_ofsz", "0");
	}

	if (cl_intermission)
	{
		V_CalcIntermissionRefdef();
	}
	else if (!cl_paused)
	{
		V_CalcRefdef();
	}

	R_PushDlights();

	if (cl_waterwarp == 0.0f)
	{
		R_RenderView();
	}
	else
	{
		int i;
		const float halfFov = r_fov * 0.5f;

		r_refdef_viewangles[1] -= cl_yawoffset;
		r_refdef_vrect_width *= 2;
		r_fov = halfFov;

		for (i = 0; i < 3; ++i)
			r_refdef_vieworg[i] -= vpn[i] * cl_waterwarp;

		R_RenderView();
		r_refdef_vrect_x += (unsigned int)r_refdef_vrect_width >> 1;

		R_PushDlights();
		r_refdef_viewangles[1] += cl_yawoffset * 2.0f;

		for (i = 0; i < 3; ++i)
			r_refdef_vieworg[i] += vpn[i] * cl_waterwarp * 2.0f;

		R_RenderView();

		r_refdef_vrect_height *= 2;
		r_fov *= 2.0f;
		r_refdef_vrect_x -= (unsigned int)r_refdef_vrect_width >> 1;
		r_refdef_vrect_width = (unsigned int)r_refdef_vrect_width >> 1;
	}

	if (crosshair.value != 0.0f)
		Draw_Character(scr_vrect.width / 2 + scr_vrect.x, scr_vrect.height / 2 + scr_vrect.y, '+');
}

void V_StartPitchDrift(void)
{
	if (v_laststop != cl_time && (v_nodrift || v_pitchvel == 0.0f))
	{
		v_pitchvel = cv_v_centerspeed.value;
		v_nodrift = false;
		v_driftmove = 0.0f;
	}
}

void V_StopPitchDrift(void)
{
	v_nodrift = true;
	v_pitchvel = 0.0f;
	v_laststop = cl_time;
}

void V_Init(void)
{
	extern void Cmd_AddCommand(const char *name, void (*func)(void));
	extern void Cvar_RegisterVariable(cvar_t *cvar);
	extern int V_BuildGammaTables(float gamma);
	extern void V_StartPitchDrift(void);
	extern void V_BonusFlash_f(void);

	extern cvar_t cv_cl_bob;
	extern cvar_t cv_cl_bobcycle;
	extern cvar_t cv_cl_bobup;
	extern cvar_t cv_cl_rollangle;
	extern cvar_t cv_cl_rollspeed;
	extern cvar_t cv_v_idlescale;
	extern cvar_t cv_v_ipitch_cycle;
	extern cvar_t cv_v_ipitch_level;
	extern cvar_t cv_v_iroll_cycle;
	extern cvar_t cv_v_iroll_level;
	extern cvar_t cv_v_iyaw_cycle;
	extern cvar_t cv_v_iyaw_level;
	extern cvar_t cv_v_kicktime;
	extern cvar_t cv_v_kickroll;
	extern cvar_t cv_v_kickpitch;
	extern cvar_t cv_v_centermove;
	extern cvar_t cv_v_centerspeed;
	extern cvar_t cv_scr_ofsx;
	extern cvar_t cv_scr_ofsy;
	extern cvar_t cv_scr_ofsz;
	extern cvar_t crosshair;
	extern cvar_t cv_gl_cshiftpercent;
	extern cvar_t cv_v_contentblend;
	extern cvar_t cv_v_dmg_pitch;
	extern cvar_t cv_v_dmg_roll;
	extern cvar_t cv_v_dmg_time;
	extern cvar_t cv_v_gamma;
	extern cvar_t cv_brightness;
	extern cvar_t cv_lightgamma;
	extern cvar_t cv_texgamma;
	extern cvar_t cv_lambert;
	extern cvar_t cv_direct;

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

