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
#ifndef GGRENADE_H
#define GGRENADE_H

// The top of an arc from vecStart to near vecTarget, g_vecZero when there is none
Vector GetTossTarget(entvars_t *pevOwner, const Vector &vecStart, const Vector &vecTarget);

// Player grenades explode on contact, the others bounce and explode after a while
void ShootTimedGrenade(entvars_t *pevOwner, const Vector &vecStart, const Vector &vecVelocity);

#endif // GGRENADE_H
