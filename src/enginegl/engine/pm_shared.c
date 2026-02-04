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
#include "pmove.h"

extern edict_t *g_pmove;
extern void *g_physents;
extern vec3_t g_forward;
extern vec3_t g_right;
extern vec3_t g_up;
extern float g_forwardmove;
extern float g_sidemove;
extern float g_upmove;
extern int g_onground;
extern vec3_t g_wishvel;
extern float g_wishspeed;
extern vec3_t *g_velocity;
extern vec3_t *g_origin;

void PM_PreventMegaBunnyJumping(void)
{
	float punch_len;
	float new_len;

	punch_len = VectorNormalize(g_pmove->v.punchangle);
	new_len = punch_len - (float)(host_frametime * 10.0);
	if (new_len < 0.0f)
		new_len = 0.0f;

	VectorScale(g_pmove->v.punchangle, new_len, g_pmove->v.punchangle);
}

void PM_Friction(void)
{
	float speed_xy_sq;
	float speed_xy;
	vec3_t start;
	vec3_t end;
	trace_t trace;
	float friction;
	float control;
	float newspeed;
	float scale;

	speed_xy_sq = (*g_velocity)[0] * (*g_velocity)[0] + (*g_velocity)[1] * (*g_velocity)[1];
	speed_xy = sqrt(speed_xy_sq);
	if (speed_xy == 0.0f)
		return;

	start[0] = (*g_velocity)[0] / speed_xy * 16.0f + (*g_origin)[0];
	start[1] = (*g_velocity)[1] / speed_xy * 16.0f + (*g_origin)[1];
	start[2] = g_pmove->v.mins[2] + (*g_origin)[2];

	VectorCopy(start, end);
	end[2] = start[2] - 34.0f;

	trace = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, g_pmove);
	if (trace.fraction == 1.0f)
		friction = sv_edgefriction.value * sv_friction.value;
	else
		friction = sv_friction.value;

	friction *= g_pmove->v.friction;

	control = (speed_xy >= sv_stopspeed.value) ? speed_xy : sv_stopspeed.value;
	newspeed = speed_xy - (float)(host_frametime * friction * control);
	if (newspeed < 0.0f)
		newspeed = 0.0f;

	scale = newspeed / speed_xy;
	(*g_velocity)[0] = (*g_velocity)[0] * scale;
	(*g_velocity)[1] = (*g_velocity)[1] * scale;
	(*g_velocity)[2] = (*g_velocity)[2] * scale;
}

void PM_Accelerate(void)
{
	float current_speed;
	float add_speed;
	float accel_speed;

	current_speed = (*g_velocity)[0] * g_wishvel[0]
		+ (*g_velocity)[1] * g_wishvel[1]
		+ (*g_velocity)[2] * g_wishvel[2];
	add_speed = g_wishspeed - current_speed;

	if (add_speed > 0.0f)
	{
		accel_speed = (float)(g_pmove->v.friction * host_frametime * sv_accelerate.value * g_wishspeed);
		if (accel_speed > add_speed)
			accel_speed = add_speed;

		(*g_velocity)[0] = (*g_velocity)[0] + g_wishvel[0] * accel_speed;
		(*g_velocity)[1] = (*g_velocity)[1] + g_wishvel[1] * accel_speed;
		(*g_velocity)[2] = (*g_velocity)[2] + g_wishvel[2] * accel_speed;
	}
}

void PM_AirAccelerate(vec3_t *wishdir)
{
	float wishspeed;
	float current_speed;
	float add_speed;
	float accel_speed;

	wishspeed = VectorNormalize(*wishdir);

	if (wishspeed > 30.0f)
		wishspeed = 30.0f;

	current_speed = (*wishdir)[0] * (*g_velocity)[0]
		+ (*wishdir)[1] * (*g_velocity)[1]
		+ (*wishdir)[2] * (*g_velocity)[2];
	add_speed = wishspeed - current_speed;

	if (add_speed > 0.0f)
	{
		accel_speed = (float)(sv_accelerate.value * g_wishspeed * g_pmove->v.friction * host_frametime);
		if (accel_speed > add_speed)
			accel_speed = add_speed;

		(*g_velocity)[0] = (*g_velocity)[0] + (*wishdir)[0] * accel_speed;
		(*g_velocity)[1] = (*g_velocity)[1] + (*wishdir)[1] * accel_speed;
		(*g_velocity)[2] = (*g_velocity)[2] + (*wishdir)[2] * accel_speed;
	}
}

void PM_WaterMove(void)
{
	int i;
	vec3_t wishvel;
	float wishspeed;
	float speed;
	float newspeed;
	float addspeed;
	float accelspeed;
	float scale;

	AngleVectors(g_pmove->v.v_angle, g_forward, g_right, g_up);

	for (i = 0; i < 3; i++)
	{
		wishvel[i] = g_forward[i] * g_forwardmove + g_right[i] * g_sidemove;
	}

	if (g_forwardmove == 0.0f && g_sidemove == 0.0f && g_upmove == 0.0f)
		wishvel[2] = wishvel[2] - 60.0f;
	else
		wishvel[2] = wishvel[2] + g_upmove;

	wishspeed = Length(wishvel);
	if (wishspeed > sv_maxspeed.value)
	{
		scale = sv_maxspeed.value / wishspeed;
		VectorScale(wishvel, scale, wishvel);
		wishspeed = sv_maxspeed.value;
	}

	wishspeed *= 0.7f;

	speed = Length(*g_velocity);
	if (speed == 0.0f)
	{
		newspeed = 0.0f;
	}
	else
	{
		newspeed = (1.0f - (float)(g_pmove->v.friction * host_frametime * sv_friction.value)) * speed;
		if (newspeed < 0.0f)
			newspeed = 0.0f;

		scale = newspeed / speed;
		VectorScale(*g_velocity, scale, *g_velocity);
	}

	if (wishspeed != 0.0f)
	{
		addspeed = wishspeed - newspeed;

		if (addspeed > 0.0f)
		{
			(void)VectorNormalize(wishvel);
			accelspeed = (float)(wishspeed * sv_accelerate.value * g_pmove->v.friction * host_frametime);

			if (accelspeed > addspeed)
				accelspeed = addspeed;

			for (i = 0; i < 3; i++)
			{
				(*g_velocity)[i] = (*g_velocity)[i] + accelspeed * wishvel[i];
			}
		}
	}
}

int PM_WalkMove(void)
{
	int result;
	int y;
	int flags;

	if (g_pmove->v.teleport_time < sv.time || g_pmove->v.waterlevel == 0.0f)
	{
		flags = (int)g_pmove->v.flags;
		flags &= ~FL_WATERJUMP;
		g_pmove->v.teleport_time = 0.0f;
		g_pmove->v.flags = (float)flags;
	}

	result = *(int *)((byte *)g_pmove + 580);
	y = *(int *)((byte *)g_pmove + 584);
	*(int *)((byte *)g_pmove + 184) = result;
	*(int *)((byte *)g_pmove + 188) = y;

	return result;
}

void PM_AirMove(void)
{
	int i;
	vec3_t wishvel;
	float wishspeed;
	float scale;
	float fvel;

	AngleVectors(g_pmove->v.angles, g_forward, g_right, g_up);

	fvel = g_forwardmove;
	if (g_pmove->v.teleport_time > sv.time && fvel < 0.0f)
		fvel = 0.0f;

	for (i = 0; i < 3; i++)
	{
		wishvel[i] = g_forward[i] * fvel + g_right[i] * g_sidemove;
	}

	if ((int)g_pmove->v.movetype == MOVETYPE_WALK)
		wishvel[2] = 0.0f;
	else
		wishvel[2] = g_upmove;

	VectorCopy(wishvel, g_wishvel);
	wishspeed = VectorNormalize(g_wishvel);
	g_wishspeed = wishspeed;

	if (wishspeed > sv_maxspeed.value)
	{
		scale = sv_maxspeed.value / wishspeed;
		VectorScale(wishvel, scale, wishvel);
		g_wishspeed = sv_maxspeed.value;
	}

	if (g_pmove->v.movetype == MOVETYPE_NOCLIP)
	{
		(*g_velocity)[0] = wishvel[0];
		(*g_velocity)[1] = wishvel[1];
		(*g_velocity)[2] = wishvel[2];
	}
	else if (g_onground)
	{
		PM_Friction();
		PM_Accelerate();
	}
	else
	{
		PM_AirAccelerate(&wishvel);
	}
}
