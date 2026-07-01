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
// Polyrobo - Polyrobo monster (showcase monster)
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

#define POLYROBO_THINK_INTERVAL		0.1f

// pev byte offset of the bone-controller value bytes (4 packed bytes).
// Not yet a named PEV_ constant -> recorded in shared_needs.
#define PEV_CONTROLLER				172

static const char kPolyroboModel[] = "models/polyrobo.mdl";

//=========================================================
// Helpers
//=========================================================

//=========================================================
// SetBoneController
// Maps an angle value onto a packed bone-controller byte for the
// monster's studio model.  Mirrors the engine model helper exactly.
//=========================================================
static void SetBoneController(entvars_t* pev, int controller, float value)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (!edict)
		return;

	unsigned char* pStudioHdr = (unsigned char*)EngineGetModelPtr(edict);
	if (!pStudioHdr)
		return;

	// studiohdr_t: numbonecontrollers at +84, bonecontrollerindex at +88.
	int numControllers = *(int*)(pStudioHdr + 84);
	if (controller >= numControllers)
		return;

	// each mstudiobonecontroller_t is 16 bytes: +4 type flags, +8 start, +12 end.
	unsigned char* pController = pStudioHdr + 16 * controller + *(int*)(pStudioHdr + 88);

	float flStart = *(float*)(pController + 8);
	float flEnd = *(float*)(pController + 12);

	if ((pController[4] & 0x38) != 0
		&& flStart + 359.0f >= flEnd
		&& (flStart + flEnd) * 0.5f + 180.0f < value)
	{
		value = value - 360.0f;
	}

	int setting = (int)((value - flStart) / (flEnd - flStart) * 255.0f);
	if (setting < 0)
		setting = 0;
	if (setting > 255)
		setting = 255;

	int* pControllerBytes = (int*)((unsigned char*)pev + PEV_CONTROLLER);
	*pControllerBytes = (*pControllerBytes & ~(255 << (8 * controller))) | (setting << (8 * controller));
}

//=========================================================
// CPolyRobo
//=========================================================

class CPolyRobo : public CBaseMonster
{
public:
	void Spawn();
	void Think(CBaseEntity* pOther);

private:
	void Pain(float flDamage);		// vtable slot 13
};

HL_COMPILE_TIME_ASSERT(sizeof(CPolyRobo) <= 336, CPolyRobo_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CPolyRobo::Spawn()
{
	EnginePrecacheModel(kPolyroboModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kPolyroboModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 48.0f, 48.0f, 212.0f);
		VecSet(mins, -48.0f, -48.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 99999.0f;
	PevFloat(pev, PEV_YAWSPEED) = 10.0f;
	PevInt(pev, PEV_SEQUENCE) = 3;
	PevFloat(pev, PEV_TAKEDAMAGE) = 2.0f;

	// pev->flags is a float field: the binary loads it, __ftol to int, ORs the
	// flag bit, then stores it back as a float (FL_FLY = 0x20).
	PevFloat(pev, PEV_FLAGS) = (float)((int)PevFloat(pev, PEV_FLAGS) | 0x20);

	m_flFrameRate = 10000.0f;

	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + RandomFloat(0.0f, 0.5f);
}

//=========================================================
// Pain - vtable slot 13
//=========================================================
void CPolyRobo::Pain(float flDamage)
{
	HL_UNUSED(flDamage);

	if (PevInt(pev, PEV_SEQUENCE) == 0)
	{
		PevInt(pev, PEV_SEQUENCE) = 1;
		PevFloat(pev, PEV_FRAME) = 0.0f;
		ResetSequenceInfo(POLYROBO_THINK_INTERVAL);
	}
}

//=========================================================
// Think
//=========================================================
void CPolyRobo::Think(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) =
		GlobalsTime(m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev)) + POLYROBO_THINK_INTERVAL;

	if (m_fSequenceFinished)
	{
		int sequence = PevInt(pev, PEV_SEQUENCE);
		if (sequence >= 1 && sequence <= 3)
		{
			PevInt(pev, PEV_SEQUENCE) = 0;
			PevFloat(pev, PEV_FRAME) = 0.0f;
		}

		ResetSequenceInfo(POLYROBO_THINK_INTERVAL);
	}

	GetAnimationEventFlags(POLYROBO_THINK_INTERVAL);
	AdvanceAnimation(POLYROBO_THINK_INTERVAL);

	{
		float flTime = GlobalsTime(m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev));
		int frac = (int)(flTime * 10.0f) % 45;
		SetBoneController(pev, 0, (float)frac);
	}

	// aim the turret bone controller at the current enemy (if any)
	float flYaw = PevVector(pev, PEV_ANGLES).y;

	int enemyIndex = PevInt(pev, PEV_ENEMY);
	if (enemyIndex)
	{
		edict_t* pEnemyEdict = EnginePEntityOfEntIndex(enemyIndex);
		entvars_t* pevEnemy = pEnemyEdict ? EngineGetVarsOfEnt(pEnemyEdict) : NULL;
		if (pevEnemy)
		{
			float vecDir[3];
			vecDir[0] = PevVector(pevEnemy, PEV_ORIGIN).x - PevVector(pev, PEV_ORIGIN).x;
			vecDir[1] = PevVector(pevEnemy, PEV_ORIGIN).y - PevVector(pev, PEV_ORIGIN).y;
			vecDir[2] = PevVector(pevEnemy, PEV_ORIGIN).z - PevVector(pev, PEV_ORIGIN).z;
			flYaw = EngineVecToYaw(vecDir);
		}
	}

	SetBoneController(pev, 0, flYaw - PevVector(pev, PEV_ANGLES).y);
}

//=========================================================
// monster_polyrobo
//=========================================================
DLLEXPORT void monster_polyrobo(entvars_t* pev)
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

		CPolyRobo* monster = new (privateData) CPolyRobo();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
