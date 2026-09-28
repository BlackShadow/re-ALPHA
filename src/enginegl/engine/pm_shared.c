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
// pm_shared.c -- player movement

#include "quakedef.h"
#include "pmove.h"

#define PUNCH_DECAY			10		// punch angle recovery, degrees per second

#define EDGE_LOOKAHEAD		16		// test for a dropoff this far ahead of the player
#define EDGE_DROP			34		// how deep the test trace goes

#define AIR_WISHSPEED_MAX	30		// air control only up to this speed

#define WATER_SINK_SPEED	60		// sink when no buttons are held
#define WATER_SPEED_SCALE	0.7f

/*
==================
PM_DropPunchAngle
==================
*/
void PM_DropPunchAngle(void)
{
	float	len;

	len = VectorNormalize(g_pmove->v.punchangle);
	len -= (float)(host_frametime * PUNCH_DECAY);
	if (len < 0)
		len = 0;

	VectorScale(g_pmove->v.punchangle, len, g_pmove->v.punchangle);
}

/*
==================
PM_Friction
==================
*/
static void PM_Friction(void)
{
	float	speed, newspeed, control;
	vec3_t	start, stop;
	float	friction;
	trace_t	trace;

	speed = sqrt((*g_velocity)[0] * (*g_velocity)[0] + (*g_velocity)[1] * (*g_velocity)[1]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = (*g_velocity)[0] / speed * EDGE_LOOKAHEAD + (*g_origin)[0];
	start[1] = (*g_velocity)[1] / speed * EDGE_LOOKAHEAD + (*g_origin)[1];
	start[2] = g_pmove->v.mins[2] + (*g_origin)[2];

	VectorCopy(start, stop);
	stop[2] = start[2] - EDGE_DROP;

	trace = SV_Move(start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, g_pmove);
	if (trace.fraction == 1)
		friction = sv_edgefriction.value * sv_friction.value;
	else
		friction = sv_friction.value;

	friction *= g_pmove->v.friction;

	// apply friction
	control = speed >= sv_stopspeed.value ? speed : sv_stopspeed.value;
	newspeed = speed - (float)(host_frametime * friction * control);
	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	VectorScale(*g_velocity, newspeed, *g_velocity);
}

/*
==================
PM_Accelerate
==================
*/
static void PM_Accelerate(void)
{
	float	addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct(*g_velocity, g_wishvel);
	addspeed = g_wishspeed - currentspeed;
	if (addspeed > 0)
	{
		accelspeed = (float)(g_pmove->v.friction * host_frametime * sv_accelerate.value * g_wishspeed);
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		VectorMA(*g_velocity, accelspeed, g_wishvel, *g_velocity);
	}
}

/*
==================
PM_AirAccelerate
==================
*/
static void PM_AirAccelerate(vec3_t wishveloc)
{
	float	addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalize(wishveloc);
	if (wishspd > AIR_WISHSPEED_MAX)
		wishspd = AIR_WISHSPEED_MAX;

	currentspeed = DotProduct(wishveloc, *g_velocity);
	addspeed = wishspd - currentspeed;
	if (addspeed > 0)
	{
		accelspeed = (float)(sv_accelerate.value * g_wishspeed * g_pmove->v.friction * host_frametime);
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		VectorMA(*g_velocity, accelspeed, wishveloc, *g_velocity);
	}
}

/*
==================
PM_WaterMove
==================
*/
void PM_WaterMove(void)
{
	int		i;
	vec3_t	wishvel;
	float	speed, newspeed, wishspeed, addspeed, accelspeed;
	float	scale;

	// user intentions
	AngleVectors(g_pmove->v.v_angle, g_forward, g_right, g_up);

	for (i = 0; i < 3; i++)
		wishvel[i] = g_forward[i] * g_forwardmove + g_right[i] * g_sidemove;

	if (g_forwardmove == 0 && g_sidemove == 0 && g_upmove == 0)
		wishvel[2] -= WATER_SINK_SPEED;
	else
		wishvel[2] += g_upmove;

	wishspeed = Length(wishvel);
	if (wishspeed > sv_maxspeed.value)
	{
		scale = sv_maxspeed.value / wishspeed;
		VectorScale(wishvel, scale, wishvel);
		wishspeed = sv_maxspeed.value;
	}
	wishspeed *= WATER_SPEED_SCALE;

	// water friction
	speed = Length(*g_velocity);
	if (speed == 0)
		newspeed = 0;
	else
	{
		newspeed = (1 - (float)(g_pmove->v.friction * host_frametime * sv_friction.value)) * speed;
		if (newspeed < 0)
			newspeed = 0;
		scale = newspeed / speed;
		VectorScale(*g_velocity, scale, *g_velocity);
	}

	// water acceleration
	if (wishspeed == 0)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed > 0)
	{
		VectorNormalize(wishvel);
		accelspeed = (float)(wishspeed * sv_accelerate.value * g_pmove->v.friction * host_frametime);
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		VectorMA(*g_velocity, accelspeed, wishvel, *g_velocity);
	}
}

/*
==================
PM_WaterJump
==================
*/
void PM_WaterJump(void)
{
	if (g_pmove->v.teleport_time < sv.time || !g_pmove->v.waterlevel)
	{
		g_pmove->v.teleport_time = 0;
		g_pmove->v.flags = (int)g_pmove->v.flags & ~FL_WATERJUMP;
	}

	g_pmove->v.velocity[0] = g_pmove->v.movedir[0];
	g_pmove->v.velocity[1] = g_pmove->v.movedir[1];
}

/*
==================
PM_AirMove
==================
*/
void PM_AirMove(void)
{
	int		i;
	vec3_t	wishvel;
	float	fmove, wishspeed;

	AngleVectors(g_pmove->v.angles, g_forward, g_right, g_up);

	fmove = g_forwardmove;

	// hack to not let you back into teleporter
	if (g_pmove->v.teleport_time > sv.time && fmove < 0)
		fmove = 0;

	for (i = 0; i < 3; i++)
		wishvel[i] = g_forward[i] * fmove + g_right[i] * g_sidemove;

	if ((int)g_pmove->v.movetype == MOVETYPE_WALK)
		wishvel[2] = 0;
	else
		wishvel[2] = g_upmove;

	VectorCopy(wishvel, g_wishvel);
	wishspeed = VectorNormalize(g_wishvel);
	g_wishspeed = wishspeed;
	if (wishspeed > sv_maxspeed.value)
	{
		VectorScale(wishvel, sv_maxspeed.value / wishspeed, wishvel);
		g_wishspeed = sv_maxspeed.value;
	}

	if (g_pmove->v.movetype == MOVETYPE_NOCLIP)
	{
		// noclip
		VectorCopy(wishvel, *g_velocity);
	}
	else if (g_onground)
	{
		PM_Friction();
		PM_Accelerate();
	}
	else
	{
		// not on ground, so little effect on velocity
		PM_AirAccelerate(wishvel);
	}
}
