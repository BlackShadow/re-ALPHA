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

// chase.c -- chase camera code

#include "quakedef.h"

vec3_t	chase_dest;

cvar_t	chase_active = { "chase_active", "0" };
cvar_t	chase_back = { "chase_back", "100" };
cvar_t	chase_up = { "chase_up", "16" };
cvar_t	chase_right = { "chase_right", "0" };

/*
==============
TraceLine
==============
*/
void TraceLine(vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t	trace;

	memset(&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck(cl_worldmodel->hulls, 0, 0, 1, start, end, &trace);

	VectorCopy(trace.endpos, impact);
}

/*
==============
Chase_Update
==============
*/
void Chase_Update(void)
{
	int		i;
	float	dist, back;
	double	pitch;
	vec3_t	forward, up, right;
	vec3_t	dest, stop;

	AngleVectors(cl_viewangles, forward, right, up);

	// calc exact destination
	for (i = 0; i < 3; i++)
	{
		back = forward[i] * chase_back.value;
		chase_dest[i] = r_refdef_vieworg[i] - right[i] * chase_right.value - back;
	}
	chase_dest[2] = chase_up.value + r_refdef_vieworg[2];

	// find the spot the player is looking at
	VectorMA(r_refdef_vieworg, 4096.0, forward, dest);
	TraceLine(r_refdef_vieworg, dest, stop);

	// calculate pitch to look at the same spot from camera
	VectorSubtract(stop, r_refdef_vieworg, stop);
	dist = stop[2] * forward[2] + stop[1] * forward[1] + stop[0] * forward[0];
	if (dist < 1)
		dist = 1;
	pitch = atan(stop[2] / dist);

	// move towards destination
	r_refdef_vieworg[0] = chase_dest[0];
	r_refdef_vieworg[1] = chase_dest[1];
	r_refdef_viewangles[PITCH] = pitch / -M_PI * 180;
	r_refdef_vieworg[2] = chase_dest[2];
}

/*
==============
Chase_Init
==============
*/
void Chase_Init(void)
{
	Cvar_RegisterVariable(&chase_active);
	Cvar_RegisterVariable(&chase_back);
	Cvar_RegisterVariable(&chase_up);
	Cvar_RegisterVariable(&chase_right);
}
