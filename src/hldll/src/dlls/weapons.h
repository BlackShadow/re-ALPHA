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
#ifndef WEAPONS_H
#define WEAPONS_H

// player weapon ids (pev->weapon), also the bit numbers in pev->weapons
#define WEAPON_NONE			0
#define WEAPON_CROWBAR		1
#define WEAPON_GLOCK		2
#define WEAPON_MP5			4

// item types below MAX_WEAPONS are pev->weapons bits, the others pev->items bits
#define MAX_WEAPONS			32

#define BULLET_NONE			0		// FireBullets ignores the bullet type

// impulse 201: sprays the six lambda decals along the owner's aim
void SprayLambdas(entvars_t *pevOwner);

// impulse 202: sprays one random blood decal along the owner's aim
void SprayBlood(entvars_t *pevOwner);

// drops item iItem in front of the owner, NULL when there is no room for it
CBaseEntity *DropItem(entvars_t *pevOwner, int iItem);

#endif // WEAPONS_H
