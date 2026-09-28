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
// pmove.h -- player movement, run by SV_ClientThink on sv_player

#ifndef PMOVE_H
#define PMOVE_H

// state of the player being moved (defined in engine_globals.c)
extern edict_t	*g_pmove;
extern vec3_t	*g_velocity;		// &g_pmove->v.velocity
extern vec3_t	*g_origin;			// &g_pmove->v.origin
extern int		g_onground;

extern vec3_t	g_forward, g_right, g_up;
extern float	g_forwardmove, g_sidemove, g_upmove;

extern vec3_t	g_wishvel;			// normalized wish direction
extern float	g_wishspeed;

void PM_DropPunchAngle(void);
void PM_WaterMove(void);
void PM_WaterJump(void);
void PM_AirMove(void);

#endif // PMOVE_H
