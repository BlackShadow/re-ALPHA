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
//=========================================================
// M44 - a model that just stands there, it takes no damage
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define M44_HEALTH		80.0f

class CM44 : public CBaseEntity
{
public:
	void Spawn();
};

//=========================================================
// Spawn
//=========================================================
void CM44::Spawn()
{
	PRECACHE_MODEL("models/m44.mdl");

	SET_MODEL(ENT(pev), "models/m44.mdl");

	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_NO;
	pev->effects = 0;
	pev->health = M44_HEALTH;

	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 32));

	pev->sequence = 0;
	pev->frame = 0;

	pev->nextthink = pev->nextthink + RANDOM_FLOAT(0.0f, 0.5f);
}

LINK_ENTITY_TO_CLASS(monster_m44, CM44);
