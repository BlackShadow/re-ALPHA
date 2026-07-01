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
// Subs - Trivial info-marker entities
//=========================================================

#include <new>
#include <string.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// info markers all use the bare CBaseEntity private-data size
//=========================================================
#define INFO_PRIVATE_DATA_SIZE	28

//=========================================================
// CPointEntity - info_player_start / info_player_deathmatch
//
// These markers have an empty Spawn (vtable slot 0)
// and inherit every other slot from CBaseEntity, so they need no
// behavior of their own beyond the construction wrapper.
//=========================================================
class CPointEntity : public CBaseEntity
{
};

HL_COMPILE_TIME_ASSERT(sizeof(CPointEntity) <= INFO_PRIVATE_DATA_SIZE, CPointEntity_size);

//=========================================================
// CInfoLandmark - info_landmark
//
// Spawn makes the entity a zero-size point that
// is solid (SOLID_BBOX) so it can be located by name at runtime.
//=========================================================
class CInfoLandmark : public CBaseEntity
{
public:
	void Spawn();
};

HL_COMPILE_TIME_ASSERT(sizeof(CInfoLandmark) <= INFO_PRIVATE_DATA_SIZE, CInfoLandmark_size);

//=========================================================
// CInfoNull - info_null
//
// Spawn removes the entity immediately; it only
// exists as a placeholder target during map authoring.
//=========================================================
class CInfoNull : public CBaseEntity
{
public:
	void Spawn();
};

HL_COMPILE_TIME_ASSERT(sizeof(CInfoNull) <= INFO_PRIVATE_DATA_SIZE, CInfoNull_size);

//=========================================================
// CHintStairs - hint_stairsup / hint_stairsdown
//
// Spawn makes the entity non-solid and installs a
// think that invokes the entity's own Think slot
// which is empty for these hints.
//=========================================================
class CHintStairs : public CBaseEntity
{
public:
	void Spawn();
	void Think(CBaseEntity* pOther);

private:
	void HintThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CHintStairs) <= INFO_PRIVATE_DATA_SIZE, CHintStairs_size);

//=========================================================
// CInfoLandmark::Spawn
//=========================================================
void CInfoLandmark::Spawn()
{
	if (!pev)
		return;

	PevFloat(pev, PEV_SOLID) = 2.0f;	// SOLID_BBOX

	edict_t* edict = EdictFromEntvars(pev);
	if (!edict)
		return;

	{
		float mins[3];
		float maxs[3];
		VecSet(mins, 0.0f, 0.0f, 0.0f);
		VecSet(maxs, 0.0f, 0.0f, 0.0f);
		EngineSetSize(edict, mins, maxs);
	}
}

//=========================================================
// CInfoNull::Spawn
//=========================================================
void CInfoNull::Spawn()
{
	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// CHintStairs::Spawn
//=========================================================
void CHintStairs::Spawn()
{
	if (!pev)
		return;

	PevFloat(pev, PEV_SOLID) = 0.0f;	// SOLID_NOT

	SetThink(&CHintStairs::HintThink);
}

//=========================================================
// CHintStairs::Think
//
// The hint's Think vtable slot is empty.
//=========================================================
void CHintStairs::Think(CBaseEntity* pOther)
{
}

//=========================================================
// CHintStairs::HintThink
//
// Invokes the entity's own (empty) Think slot.
//=========================================================
void CHintStairs::HintThink(CBaseEntity* pOther)
{
	Think(pOther);
}

//=========================================================
// construction wrappers
//=========================================================

DLLEXPORT void info_player_start(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, INFO_PRIVATE_DATA_SIZE);
		if (!privateData)
			return;

		memset(privateData, 0, INFO_PRIVATE_DATA_SIZE);

		CPointEntity* self = new (privateData) CPointEntity();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

DLLEXPORT void info_player_deathmatch(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, INFO_PRIVATE_DATA_SIZE);
		if (!privateData)
			return;

		memset(privateData, 0, INFO_PRIVATE_DATA_SIZE);

		CPointEntity* self = new (privateData) CPointEntity();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

DLLEXPORT void info_null(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, INFO_PRIVATE_DATA_SIZE);
		if (!privateData)
			return;

		memset(privateData, 0, INFO_PRIVATE_DATA_SIZE);

		CInfoNull* self = new (privateData) CInfoNull();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

DLLEXPORT void info_landmark(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, INFO_PRIVATE_DATA_SIZE);
		if (!privateData)
			return;

		memset(privateData, 0, INFO_PRIVATE_DATA_SIZE);

		CInfoLandmark* self = new (privateData) CInfoLandmark();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

DLLEXPORT void hint_stairsup(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, INFO_PRIVATE_DATA_SIZE);
		if (!privateData)
			return;

		memset(privateData, 0, INFO_PRIVATE_DATA_SIZE);

		CHintStairs* self = new (privateData) CHintStairs();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

DLLEXPORT void hint_stairsdown(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, INFO_PRIVATE_DATA_SIZE);
		if (!privateData)
			return;

		memset(privateData, 0, INFO_PRIVATE_DATA_SIZE);

		CHintStairs* self = new (privateData) CHintStairs();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
