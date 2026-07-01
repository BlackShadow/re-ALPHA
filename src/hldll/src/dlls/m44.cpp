/***
*
*Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*This product contains software technology licensed from Id
*Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

//=========================================================
// M44 - M44 weapon display model
//=========================================================

#include <new>
#include <string.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// monster-specific constants
//=========================================================

#define M44_HEALTH		80.0f

static const char kM44Model[] = "models/m44.mdl";

//=========================================================
// CM44
//
// The M44 is a static, non-damageable display model.  Spawn
// installs the model and bounding box, marks the entity as
// invulnerable (DAMAGE_NO), and schedules its first think a
// short random delay out.  It has no AI of its own; every
// vtable slot past Spawn falls through to the CBaseEntity
// defaults.
//=========================================================

class CM44 : public CBaseEntity
{
public:
	void Spawn();
};

HL_COMPILE_TIME_ASSERT(sizeof(CM44) <= 28, CM44_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CM44::Spawn()
{
	EnginePrecacheModel(kM44Model);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kM44Model);

	PevFloat(pev, PEV_SOLID) = 2.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevInt(pev, PEV_TAKEDAMAGE) = 0;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = M44_HEALTH;

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 16.0f, 16.0f, 32.0f);
		VecSet(mins, -16.0f, -16.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevInt(pev, PEV_SEQUENCE) = 0;
	PevFloat(pev, PEV_FRAME) = 0.0f;

	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + RandomFloat(0.0f, 0.5f);
}

//=========================================================
// monster_m44
//=========================================================
DLLEXPORT void monster_m44(entvars_t* pev)
{
	entvars_t* entvars = pev;
	if (!entvars)
	{
		edict_t* created = EngineCreateEntity();
		entvars = created ? EngineGetVarsOfEnt(created) : NULL;
	}

	edict_t* edict = EdictFromEntvars(entvars);
	if (!edict)
		return;

	void* privateData = EngineGetPrivateData(edict);
	if (!privateData)
	{
		privateData = EngineAllocPrivateData(edict, 28);
		if (!privateData)
			return;

		memset(privateData, 0, 28);

		CM44* monster = new (privateData) CM44();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
