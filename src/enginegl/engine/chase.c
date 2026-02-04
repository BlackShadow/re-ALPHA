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

extern vec3_t	cl_viewangles;
extern float	r_refdef_vieworg[3];
extern float	r_refdef_viewangles[3];

vec3_t chase_dest;

cvar_t chase_active = { "chase_active", "0" };
cvar_t chase_back = { "chase_back", "100" };
cvar_t chase_up = { "chase_up", "16" };
cvar_t chase_right = { "chase_right", "0" };

void TraceLine(vec_t *start, vec_t *end, vec_t *impact)
{
	trace_t trace;

	memset(&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck((hull_t *)(cl_worldmodel + 236), 0, 0.0, 1.0, (float *)start, (float *)end, &trace);

	impact[0] = trace.endpos[0];
	impact[1] = trace.endpos[1];
	impact[2] = trace.endpos[2];
}

void Chase_Update(void)
{
	int i;
	double pitch_calc;
	double pitch_dot;
	double final_pitch;
	double dot_result;
	vec3_t forward, right, up;
	vec3_t dest;
	vec3_t impact;
	float dist;

	AngleVectors(cl_viewangles, forward, right, up);

	for (i = 0; i < 3; i++)
	{
		pitch_calc = forward[i] * chase_back.value;
		pitch_dot = r_refdef_vieworg[i] - right[i] * chase_right.value;
		final_pitch = pitch_dot - pitch_calc;
		chase_dest[i] = final_pitch;
	}

	chase_dest[2] = chase_up.value + r_refdef_vieworg[2];

	VectorMA(r_refdef_vieworg, 4096.0, forward, dest);

	TraceLine((vec_t *)r_refdef_vieworg, (vec_t *)dest, impact);

	impact[0] = impact[0] - r_refdef_vieworg[0];
	impact[1] = impact[1] - r_refdef_vieworg[1];
	impact[2] = impact[2] - r_refdef_vieworg[2];

	dist = impact[2] * forward[2] + impact[1] * forward[1] + impact[0] * forward[0];
	if (dist < 1.0)
		dist = 1.0;

	dot_result = atan(impact[2] / dist);

	r_refdef_vieworg[0] = chase_dest[0];
	r_refdef_vieworg[1] = chase_dest[1];

	dist = dot_result / -3.141592653589793 * 180.0;
	r_refdef_viewangles[PITCH] = dist;
	r_refdef_vieworg[2] = chase_dest[2];
}

void Chase_Init(void)
{
	Cvar_RegisterVariable(&chase_active);
	Cvar_RegisterVariable(&chase_back);
	Cvar_RegisterVariable(&chase_up);
	Cvar_RegisterVariable(&chase_right);
}
