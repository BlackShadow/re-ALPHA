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
// ISlave - Alien slave monster
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

static const char kISlaveModel[] = "models/islave.mdl";

//=========================================================
// CISlave
//=========================================================

class CISlave : public CBaseMonster
{
public:
	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);
	int CheckAttacks(entvars_t* pevEnemy, float flDist);
	void Death(int gibType);		// vtable slot 14
	void RemoveThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CISlave) <= 336, CISlave_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CISlave::Spawn()
{
	EnginePrecacheModel(kISlaveModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kISlaveModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 18.0f, 18.0f, 72.0f);
		VecSet(mins, -18.0f, -18.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;		// 0x40400000
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;		// 0x40800000
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 30.0f;		// 0x41F00000
	PevFloat(pev, PEV_YAWSPEED) = 8.0f;		// 0x41000000
	PevInt(pev, PEV_SEQUENCE) = 13;

	m_Activity = 1;

	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + 1.0f;
	WalkMonsterStart(NULL);
}

//=========================================================
// Classify (vtable slot 9 = shared base, returns 0)
//=========================================================
int CISlave::Classify()
{
	return 0;
}

//=========================================================
// SetActivity
//=========================================================
void CISlave::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 1:
	case 2:
		sequence = 0;
		break;

	case 3:
		sequence = 1;
		break;

	case 4:
		sequence = 3;
		break;

	case 8:
		sequence = 4;
		break;

	default:
		EngineAlertMessage(1, "ISlave's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	PevFloat(pev, PEV_FRAME) = 0.0f;

	// The binary validates the resolved sequence against {0,1,3,4}; the outer
	// switch only ever produces those, so the error arm is unreachable in
	// practice. Reproduced for byte-fidelity: on an out-of-range sequence it
	// emits "Bogus ISlave anim: %d" (engine ALERT level 1) and zeroes the
	// cached animation rates.
	switch (sequence)
	{
	case 0:
	case 1:
	case 3:
	case 4:
		break;

	default:
		EngineAlertMessage(1, "Bogus ISlave anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// Death (vtable slot 14) - the death handler the shared
// Killed/gib dispatch invokes when HP runs out.
//=========================================================
void CISlave::Death(int gibType)
{
	HL_UNUSED(gibType);

	// this+12 = think = the binary (shared "remove entity" think)
	SetRemoveThink();
	// pev->nextthink = *(*(pev+524) + 124) = globals time (read straight off pev, not m_pGlobals)
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(GlobalsFromEntvars(pev));
}

//=========================================================
// CheckAttacks (vtable slot 16 = shared base, returns 0)
//=========================================================
int CISlave::CheckAttacks(entvars_t* pevEnemy, float flDist)
{
	HL_UNUSED(pevEnemy);
	HL_UNUSED(flDist);
	return 0;
}

//=========================================================
// RemoveThink
//=========================================================
void CISlave::RemoveThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// TakeDamage - alien_slave does NOT override the damage slot; the shared
// CBaseMonster::TakeDamage (vtable slot 17) handles it and
// on HP <= 0, routes through Killed to the Death virtual
// (slot 14). The pain virtual (slot 13) is a nullsub for the slave, so it
// inherits the empty CBaseMonster::Pain default.
//=========================================================

//=========================================================
// monster_alien_slave
//=========================================================
DLLEXPORT void monster_alien_slave(entvars_t* pev)
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

		CISlave* monster = new (privateData) CISlave();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
