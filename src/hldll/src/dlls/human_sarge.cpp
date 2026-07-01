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
// HSarge - Human sarge monster
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "ggrenade.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// Monster-specific constants
//=========================================================

#define HSARGE_THINK_INTERVAL 0.1f

static const char kHSargeModel[] = "models/hsarge.mdl";
static const char kShardSprite[] = "sprites/shard.spr";
static const char kPain3Sound[] = "player/pain3.wav";

//=========================================================
// Helpers
//=========================================================

static entvars_t* EnemyVars(entvars_t* pevSelf)
{
	if (!pevSelf)
		return NULL;

	int enemyIndex = PevInt(pevSelf, PEV_ENEMY);
	if (!enemyIndex)
		return NULL;

	edict_t* pEdict = EnginePEntityOfEntIndex(enemyIndex);
	return pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
}

//=========================================================
// CHSarge
//=========================================================

class CHSarge : public CBaseMonster
{
public:
	void Spawn();

private:
	void SetActivity(int activity);
	void SargeThink(CBaseEntity* pOther);
	void LaunchShard(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CHSarge) <= 336, CHSarge_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CHSarge::Spawn()
{
	EnginePrecacheModel(kHSargeModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kHSargeModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 18.0f, 18.0f, 72.0f);
		VecSet(mins, -18.0f, -18.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 30.0f;
	PevFloat(pev, PEV_YAWSPEED) = 10.0f;
	PevInt(pev, PEV_SEQUENCE) = 2;

	PevVector(pev, PEV_VIEWOFS).x = 0.0f;
	PevVector(pev, PEV_VIEWOFS).y = 0.0f;
	PevVector(pev, PEV_VIEWOFS).z = 64.0f;

	SetThink(&CHSarge::SargeThink);

	// The original adds the interval to the existing nextthink value (the
	// engine has already seeded pev->nextthink), it does NOT recompute it
	// from the current map time the way SetNextThink would.
	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + HSARGE_THINK_INTERVAL;
}

//=========================================================
// SetActivity
//=========================================================
void CHSarge::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 4:
	case 8:
	case 29:
		sequence = 0;
		break;

	case 35:
		sequence = 1;
		break;

	default:
		EngineAlertMessage(1, "HSarge's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(HSARGE_THINK_INTERVAL);

	volatile int validatedSequence = sequence;
	if (validatedSequence < 0 || validatedSequence > 1)
	{
		EngineAlertMessage(1, "Bogus HSarge anim: %d", validatedSequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
	}
}

//=========================================================
// SargeThink
//=========================================================
void CHSarge::SargeThink(CBaseEntity* pOther)
{
	SetNextThink(HSARGE_THINK_INTERVAL);

	edict_t* pClient = EngineFindClientInPVS();
	if (!pClient)
		return;

	if (!EngineIndexOfEdict(pClient))
		return;

	entvars_t* pevClient = EngineGetVarsOfEnt(pClient);
	if (!pevClient)
		return;

	// pev->flags is stored as a float; the original converts it to an int
	// (ftol) before testing FL_NOTARGET (0x80).
	if (((int)PevFloat(pevClient, PEV_FLAGS) & 0x80) != 0)
		return;

	if (!FInViewCone(pev, pevClient, 0.1f))
		return;

	if (!FVisible(pev, pevClient))
		return;

	PevInt(pev, PEV_ENEMY) = EngineIndexOfEdict(pClient);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, kPain3Sound, 1.0f, 0.8f);

	// The original runs the cover/retreat searches here (their result
	// vectors are discarded; FindCover sets m_Activity as a side effect).
	FindCover(pevClient);
	FindRetreat(pevClient);

	SetThink(&CHSarge::LaunchShard);
	SetNextThink(2.0f);
}

//=========================================================
// LaunchShard
//=========================================================
void CHSarge::LaunchShard(CBaseEntity* pOther)
{
	entvars_t* pevEnemy = EnemyVars(pev);
	if (!pevEnemy)
		return;

	float vecTarget[3];
	vecTarget[0] = PevVector(pevEnemy, PEV_ORIGIN).x;
	vecTarget[1] = PevVector(pevEnemy, PEV_ORIGIN).y;
	vecTarget[2] = PevVector(pevEnemy, PEV_ORIGIN).z + 32.0f;

	float vecStart[3];
	vecStart[0] = PevVector(pev, PEV_ORIGIN).x;
	vecStart[1] = PevVector(pev, PEV_ORIGIN).y;
	vecStart[2] = PevVector(pev, PEV_ORIGIN).z + 32.0f;

	float vecTossTarget[3];
	GetTossTarget(vecTossTarget, pev, vecStart, vecTarget);

	if (vecTossTarget[0] == 0.0f && vecTossTarget[1] == 0.0f && vecTossTarget[2] == 0.0f)
	{
		m_pfnThink = NULL;
		return;
	}

	edict_t* pEdict = EngineCreateEntity();
	entvars_t* pevShard = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
	if (!pevShard)
		return;

	PevFloat(pevShard, PEV_MOVETYPE) = 10.0f;
	PevFloat(pevShard, PEV_SOLID) = 2.0f;
	EngineSetModel(pEdict, kShardSprite);

	{
		float vecZero[3] = {0.0f, 0.0f, 0.0f};
		EngineSetSize(pEdict, vecZero, vecZero);
	}

	// The original writes the shard's origin fields directly (it does
	// not call SetOrigin), so the entity is not relinked here.
	PevVector(pevShard, PEV_ORIGIN).x = PevVector(pev, PEV_ORIGIN).x;
	PevVector(pevShard, PEV_ORIGIN).y = PevVector(pev, PEV_ORIGIN).y;
	PevVector(pevShard, PEV_ORIGIN).z = PevVector(pev, PEV_ORIGIN).z + 32.0f;

	float distVec[3];
	distVec[0] = vecTossTarget[0] - PevVector(pev, PEV_ORIGIN).x;
	distVec[1] = vecTossTarget[1] - PevVector(pev, PEV_ORIGIN).y;
	distVec[2] = vecTossTarget[2] - PevVector(pev, PEV_ORIGIN).z;

	float flDist = VecLength(distVec);

	float vel[3];
	vel[0] = vecTossTarget[0] - PevVector(pevShard, PEV_ORIGIN).x;
	vel[1] = vecTossTarget[1] - PevVector(pevShard, PEV_ORIGIN).y;
	vel[2] = vecTossTarget[2] - PevVector(pevShard, PEV_ORIGIN).z;

	if (!VecNormalize(vel))
		VecSet(vel, 0.0f, 0.0f, 0.0f);

	// The original multiplies the distance by a DOUBLE-precision 1.9
	// (FMUL double ptr at), so keep the scale in double.
	double scale = (double)flDist * 1.9;
	vel[0] = (float)(vel[0] * scale);
	vel[1] = (float)(vel[1] * scale);
	vel[2] = (float)(vel[2] * scale);

	PevVector(pevShard, PEV_VELOCITY).x = vel[0];
	PevVector(pevShard, PEV_VELOCITY).y = vel[1];
	PevVector(pevShard, PEV_VELOCITY).z = vel[2];
}

//=========================================================
// monster_human_sarge
//=========================================================
DLLEXPORT void monster_human_sarge(entvars_t* pev)
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

		CHSarge* monster = new (privateData) CHSarge();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
