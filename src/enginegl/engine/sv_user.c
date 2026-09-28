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
// sv_user.c -- server code for moving users

#include "quakedef.h"
#include "pmove.h"

#define MAX_FORWARD		6
#define ON_EPSILON		0.1f

// floor samples for the ideal pitch, starting 3 * 12 units in front of the player
#define PITCH_SAMPLE_START	3
#define PITCH_SAMPLE_STEP	12
#define PITCH_SAMPLE_DEPTH	160

float V_CalcRoll(vec3_t angles, vec3_t velocity);

static usercmd_t	cmd;

/*
===============
SV_SetIdealPitch
===============
*/
void SV_SetIdealPitch(void)
{
	float	angleval, sinval, cosval;
	trace_t	tr;
	vec3_t	top, bottom;
	float	z[MAX_FORWARD];
	int		i, j;
	int		step, dir, steps;

	if (!((int)sv_player->v.flags & FL_ONGROUND))
		return;

	angleval = sv_player->v.angles[YAW] * (float)(M_PI * 2 / 360);
	sinval = sin(angleval);
	cosval = cos(angleval);

	for (i = 0; i < MAX_FORWARD; i++)
	{
		top[0] = sv_player->v.origin[0] + cosval * (i + PITCH_SAMPLE_START) * PITCH_SAMPLE_STEP;
		top[1] = sv_player->v.origin[1] + sinval * (i + PITCH_SAMPLE_START) * PITCH_SAMPLE_STEP;
		top[2] = sv_player->v.origin[2] + sv_player->v.view_ofs[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - PITCH_SAMPLE_DEPTH;

		tr = SV_Move(top, vec3_origin, vec3_origin, bottom, MOVE_NOMONSTERS, sv_player);
		if (tr.allsolid)
			return;	// looking at a wall, leave ideal the way is was

		if (tr.fraction == 1)
			return;	// near a dropoff

		z[i] = top[2] + tr.fraction * (bottom[2] - top[2]);
	}

	dir = 0;
	steps = 0;
	for (j = 1; j < i; j++)
	{
		step = z[j] - z[j - 1];
		if (step > -ON_EPSILON && step < ON_EPSILON)
			continue;

		if (dir && (step - dir > ON_EPSILON || step - dir < -ON_EPSILON))
			return;	// mixed changes

		steps++;
		dir = step;
	}

	if (!dir)
	{
		sv_player->v.idealpitch = 0;
		return;
	}

	if (steps < 2)
		return;

	sv_player->v.idealpitch = -dir * sv_idealpitchscale.value;
}

/*
===================
SV_ClientThink

the move fields specify an intended velocity in pix/sec
the angle fields specify an exact angular motion in degrees
===================
*/
void SV_ClientThink(void)
{
	edict_t	*ent;
	vec3_t	v_angle;

	ent = g_pmove = sv_player;

	if (ent->v.movetype == MOVETYPE_NONE)
		return;

	g_velocity = &ent->v.velocity;
	g_origin = &ent->v.origin;
	g_onground = (int)ent->v.flags & FL_ONGROUND;

	PM_DropPunchAngle();

	// if dead, behave differently
	if (ent->v.health <= 0)
		return;

	cmd = host_client->cmd;
	g_forwardmove = cmd.forwardmove;
	g_sidemove = cmd.sidemove;
	g_upmove = cmd.upmove;

	// show 1/3 the pitch angle and all the roll angle
	VectorAdd(ent->v.v_angle, ent->v.punchangle, v_angle);
	ent->v.angles[ROLL] = V_CalcRoll(ent->v.angles, ent->v.velocity) * 4;
	if (!ent->v.fixangle)
	{
		ent->v.angles[YAW] = v_angle[YAW];
		ent->v.angles[PITCH] = v_angle[PITCH] / -3;
	}

	if ((int)ent->v.flags & FL_WATERJUMP)
	{
		PM_WaterJump();
		return;
	}

	// walk
	if (ent->v.waterlevel < 2 || ent->v.movetype == MOVETYPE_NOCLIP)
	{
		PM_AirMove();
		ent->v.friction = 1;
		return;
	}

	PM_WaterMove();
}
