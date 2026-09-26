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
#ifndef MONSTERS_H
#define MONSTERS_H

#include "basemonster.h"

BOOL FVisible(entvars_t *pevLooker, entvars_t *pevTarget);
BOOL FInViewCone(entvars_t *pevLooker, entvars_t *pevTarget, float flDot);
BOOL CheckFriendlyFire(entvars_t *pevShooter, float dirX, float dirY, float dirZ, float flDistance);

// AI debugging lines, toggled by impulse 200
extern int g_fDrawLines;
void DrawDebugLine(const Vector &vecStart, const Vector &vecEnd);

#endif // MONSTERS_H
