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
// Panther - Panther melee predator monster
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// monster-specific constants
//=========================================================

#define PANTHER_THINK_INTERVAL	0.1f

static const char kPantherModel[] = "models/panther.mdl";

//=========================================================
// CPanther
//=========================================================

class CPanther : public CBaseMonster
{
public:
	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);
	void Death(int gibType);		// vtable slot 14

	void RemoveThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CPanther) <= 336, CPanther_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CPanther::Spawn()
{
	EnginePrecacheModel(kPantherModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kPantherModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 32.0f, 32.0f, 64.0f);
		VecSet(mins, -32.0f, -32.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 50.0f;
	PevFloat(pev, PEV_YAWSPEED) = 10.0f;
	PevInt(pev, PEV_SEQUENCE) = 2;

	m_bloodColor = 146;

	// nextthink = nextthink + RandomFloat(0,0.5) + 0.5 (additive on the
	// engine-seeded nextthink, NOT GlobalTime()+delay).
	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + RandomFloat(0.0f, 0.5f) + 0.5f;
	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// Classify
//=========================================================
int CPanther::Classify()
{
	return 9;
}

//=========================================================
// SetActivity
//=========================================================
void CPanther::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 1:
	case 2:
	case 3:
	case 4:
	case 8:
	case 29:
		sequence = 0;
		break;

	default:
		EngineAlertMessage(3, "Panther's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(PANTHER_THINK_INTERVAL);

	volatile int validatedSequence = sequence;
	if (validatedSequence != 0)
	{
		EngineAlertMessage(3, "Bogus Panther anim: %d", validatedSequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
	}
}

//=========================================================
// Death - vtable slot 14
// The panther has no death animation; it just removes itself.
//=========================================================
void CPanther::Death(int gibType)
{
	HL_UNUSED(gibType);

	SetRemoveThink();
}

//=========================================================
// RemoveThink
//=========================================================
void CPanther::RemoveThink(CBaseEntity* pOther)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// monster_panther
//=========================================================
DLLEXPORT void monster_panther(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 336);
		if (!privateData)
			return;

		memset(privateData, 0, 336);

		CPanther* monster = new (privateData) CPanther();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
