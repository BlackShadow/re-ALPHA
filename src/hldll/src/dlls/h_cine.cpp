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
// Cine - Scripted/cinematic placeholder monsters
//
// The monster_cineX entities are inert "stand-in" actors
// used during in-engine cinematics.  Each variant has its own
// Spawn() slot that supplies the model path before tail-calling
// the shared spawn body.  cine_blood is a separate, tiny entity
// that sprays blood for a short time.
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// cine constants
//=========================================================

#define CINE_THINK_INTERVAL		0.1f
#define CINE_PRIVATE_SIZE		336

//=========================================================
// CCineMonster - shared base for every monster_cineX
//=========================================================

class CCineMonster : public CBaseMonster
{
public:
	void SpawnWithModel(const char* pszModel);
	void Use(CBaseEntity* pOther);

private:
	void PlayThink(CBaseEntity* pOther);
	void RemoveThink(CBaseEntity* pOther);
	void Death(int gibType);		// vtable slot 14
};

HL_COMPILE_TIME_ASSERT(sizeof(CCineMonster) <= CINE_PRIVATE_SIZE, CCineMonster_private_data_size);

//=========================================================
// cine variant classes
//
// The original DLL gives each monster_cineX export a distinct
// vtable whose Spawn slot only supplies the hard-coded model
// path, then runs the shared body at the binary.
//=========================================================
class CCineScientist : public CCineMonster
{
public:
	void Spawn();
};

class CCinePanther : public CCineMonster
{
public:
	void Spawn();
};

class CCineBarney : public CCineMonster
{
public:
	void Spawn();
};

class CCine2Scientist : public CCineMonster
{
public:
	void Spawn();
};

class CCine2HvyWeapons : public CCineMonster
{
public:
	void Spawn();
};

class CCine2Slave : public CCineMonster
{
public:
	void Spawn();
};

class CCine3Scientist : public CCineMonster
{
public:
	void Spawn();
};

class CCine3Barney : public CCineMonster
{
public:
	void Spawn();
};

//=========================================================
// SpawnWithModel (.. + shared)
//
// Each per-variant Spawn slot (the binary cine_scientist
// The binary cine2_hvyweapons, etc.) only tail-calls the
// shared spawn body the binary with the variant's model
// path.
//=========================================================
void CCineMonster::SpawnWithModel(const char* pszModel)
{
	EnginePrecacheModel(pszModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, pszModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 16.0f, 16.0f, 64.0f);
		VecSet(mins, -16.0f, -16.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 1.0f;
	PevFloat(pev, PEV_YAWSPEED) = 10.0f;

	// The cinematic sequence index is taken from the float
	// field at pev+0x158 (alpha entvars, no named PEV_ yet)
	// and truncated to an int.  See SHARED_NEEDS.
	PevInt(pev, PEV_SEQUENCE) = (int)PevFloat(pev, 0x158);

	ResetSequenceInfo(CINE_THINK_INTERVAL);

	PevFloat(pev, PEV_FRAMERATE) = 0.0f;

	m_flDistTooFar = 999999.0f;
	m_bloodColor = 70;

	// When the cine actor has no targetname it begins playing
	// its sequence on its own, one second after spawning.
	if (PevInt(pev, PEV_TARGETNAME) == 0)
	{
		SetThink(&CCineMonster::PlayThink);
		PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + 1.0f;
	}
}

void CCineScientist::Spawn()
{
	SpawnWithModel("models/cine-scientist.mdl");
}

void CCinePanther::Spawn()
{
	SpawnWithModel("models/cine-panther.mdl");
}

void CCineBarney::Spawn()
{
	SpawnWithModel("models/cine-barney.mdl");
}

void CCine2Scientist::Spawn()
{
	SpawnWithModel("models/cine2-scientist.mdl");
}

void CCine2HvyWeapons::Spawn()
{
	SpawnWithModel("models/cine2_hvyweapons.mdl");
}

void CCine2Slave::Spawn()
{
	SpawnWithModel("models/cine2_slave.mdl");
}

void CCine3Scientist::Spawn()
{
	SpawnWithModel("models/cine3-scientist.mdl");
}

void CCine3Barney::Spawn()
{
	SpawnWithModel("models/cine3-barney.mdl");
}

//=========================================================
// Use
//
// (Re)start the cinematic sequence: rewind the animation
// time and queue PlayThink for the current frame.
//=========================================================
void CCineMonster::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	PevFloat(pev, PEV_ANIMTIME) = 0.0f;
	SetThink(&CCineMonster::PlayThink);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime();
}

//=========================================================
// PlayThink
//
// Advance the cinematic sequence.  The first tick restarts
// the sequence; thereafter the animation is advanced until
// it finishes.  When the actor carries a spawnflag and the
// sequence has finished, the slot-14 death handler removes it.
//=========================================================
void CCineMonster::PlayThink(CBaseEntity* pOther)
{
	if ((*(int*)&PevFloat(pev, PEV_ANIMTIME) & 0x7FFFFFFF) == 0)
		ResetSequenceInfo(CINE_THINK_INTERVAL);

	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 1.0f;

	if ((PevInt(pev, PEV_SPAWNFLAGS) & 0x7FFFFFFF) != 0 && m_fSequenceFinished)
		Death(0);
	else
		AdvanceAnimation(1.0f);
}

//=========================================================
// Death (slot 14)
//
// Hand the actor over to the removal think so
// it deletes itself on the next tick.
//=========================================================
void CCineMonster::Death(int gibType)
{
	HL_UNUSED(gibType);
	SetRemoveThink();
}

//=========================================================
// RemoveThink
//
// Remove the cinematic prop from the world.
//=========================================================
void CCineMonster::RemoveThink(CBaseEntity* pOther)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// CCineBlood - blood-spray cinematic effect
//
// A trivial entity (size 28) that, once used, sprays
// streams of blood every tick while its short life ticks
// down, occasionally leaving a gunshot decal.
//=========================================================

class CCineBlood : public CBaseEntity
{
public:
	void Spawn();

private:
	void SpurtUse(CBaseEntity* pOther);
	void SpurtThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CCineBlood) <= 28, CCineBlood_private_data_size);

//=========================================================
// Spawn
//
// Non-solid; arm the blood spray to begin when the entity is
// used, and seed the spray's lifetime (stored in health).
// The binary installs the handler at the m_pfnUse slot
// (this+0x14), so it fires from Use, not Touch.
//=========================================================
void CCineBlood::Spawn()
{
	PevFloat(pev, PEV_SOLID) = 0.0f;
	SetUse(&CCineBlood::SpurtUse);
	PevFloat(pev, PEV_HEALTH) = 20.0f;
}

//=========================================================
// SpurtUse
//
// Begin spraying blood immediately when used.
//=========================================================
void CCineBlood::SpurtUse(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	SetThink(&CCineBlood::SpurtThink);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime();
}

//=========================================================
// SpurtThink
//
// Emit a blood stream temp entity in the facing direction
// each tick, count down the spray lifetime (health), and
// 75% of the time trace a short ray and stamp a gunshot
// temp entity where it lands.
//=========================================================
void CCineBlood::SpurtThink(CBaseEntity* pOther)
{
	Vector& origin = PevVector(pev, PEV_ORIGIN);

	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + CINE_THINK_INTERVAL;

	// Build forward/right/up from our angles for the trace.
	EngineMakeVectors(VecPtr(PevVector(pev, PEV_ANGLES)));

	// Tick the lifetime down; remove when it runs out.
	float health = PevFloat(pev, PEV_HEALTH);
	PevFloat(pev, PEV_HEALTH) = health - 1.0f;
	if (health < 0.0f)
	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineRemoveEntity(edict);
	}

	// Blood stream temp entity (svc_temp_entity == 23).
	int count;
	if (RandomFloat(0.0f, 1.0f) >= 0.7f)
	{
		EngineWriteByte(0, 23);
		EngineWriteByte(0, 101);
		EngineWriteCoord(0, origin.x);
		EngineWriteCoord(0, origin.y);
		EngineWriteCoord(0, origin.z);
		EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
		EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
		EngineWriteCoord(0, RandomFloat(0.0f, 1.0f));
		EngineWriteByte(0, 70);
		count = RandomLong(50, 150);
	}
	else
	{
		EngineWriteByte(0, 23);
		EngineWriteByte(0, 103);
		EngineWriteCoord(0, origin.x);
		EngineWriteCoord(0, origin.y);
		EngineWriteCoord(0, origin.z);
		EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
		EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
		EngineWriteCoord(0, RandomFloat(0.0f, 1.0f));
		EngineWriteByte(0, 70);
		count = 10;
	}
	EngineWriteByte(0, count);

	// Three quarters of the time, splatter a gunshot decal.
	if (RandomFloat(0.0f, 1.0f) < 0.75f)
	{
		void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);

		// Random downward-biased ray: a (0,0,-1) base plus
		// jittered forward and right spread, each scaled by a
		// random factor in [-0.6, 0.6].
		const float* right = GlobalsRight(globals);
		float scaleRight = RandomFloat(-1.0f, 1.0f) * 0.6f;
		float vecRight[3];
		vecRight[0] = right[0] * scaleRight;
		vecRight[1] = right[1] * scaleRight;
		vecRight[2] = right[2] * scaleRight;

		const float* forward = GlobalsForward(globals);
		float scaleFwd = RandomFloat(-1.0f, 1.0f) * 0.6f;
		float vecDir[3];
		vecDir[0] = forward[0] * scaleFwd + 0.0f + vecRight[0];
		vecDir[1] = forward[1] * scaleFwd + 0.0f + vecRight[1];
		vecDir[2] = forward[2] * scaleFwd + -1.0f + vecRight[2];

		float vecSrc[3];
		float vecEnd[3];
		float vecScaled[3];
		Vec3Scale(vecScaled, vecDir, 256.0f);
		Vec3Add(vecEnd, &origin.x, vecScaled);

		float vecOfs[3];
		VecSet(vecOfs, 0.0f, 0.0f, 64.0f);
		Vec3Add(vecSrc, &origin.x, vecOfs);

		edict_t* edict = EdictFromEntvars(pev);

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine(vecSrc, vecEnd, 0, edict, &tr);

		if (tr.flFraction != 1.0f)
		{
			EngineWriteByte(0, 23);
			EngineWriteByte(0, 104);
			EngineWriteCoord(0, tr.vecEndPos[0]);
			EngineWriteCoord(0, tr.vecEndPos[1]);
			EngineWriteCoord(0, tr.vecEndPos[2]);
			EngineWriteShort(0, EngineModelIndex(TraceHitIndex(&tr)));
			EngineWriteByte(0, rand() % 6 + 14);
		}
	}
}

//=========================================================
// cine_blood
//=========================================================
DLLEXPORT void cine_blood(entvars_t* pev)
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

		CCineBlood* blood = new (privateData) CCineBlood();
		blood->pev = entvars;
		blood->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// monster_cineX construction wrappers
//
// Every monster_cineX export allocates 336 bytes and installs
// that variant's vtable.  The engine then dispatches Spawn()
// through that vtable to finish setup.
//=========================================================

template <class T>
static T* CineAlloc(entvars_t* pev)
{
	entvars_t* entvars = pev;
	if (!entvars)
	{
		edict_t* created = EngineCreateEntity();
		entvars = created ? EngineGetVarsOfEnt(created) : NULL;
	}

	edict_t* edict = EdictFromEntvars(entvars);
	if (!edict)
		return NULL;

	void* privateData = EngineGetPrivateData(edict);
	if (privateData)
		return (T*)privateData;

	privateData = EngineAllocPrivateData(edict, CINE_PRIVATE_SIZE);
	if (!privateData)
		return NULL;

	memset(privateData, 0, CINE_PRIVATE_SIZE);

	T* cine = new (privateData) T();
	cine->pev = entvars;
	cine->m_pGlobals = GlobalsFromEntvars(entvars);
	return cine;
}

//=========================================================
// monster_cine_scientist (Spawn)
//=========================================================
DLLEXPORT void monster_cine_scientist(entvars_t* pev)
{
	CineAlloc<CCineScientist>(pev);
}

//=========================================================
// monster_cine_panther (Spawn)
//=========================================================
DLLEXPORT void monster_cine_panther(entvars_t* pev)
{
	CineAlloc<CCinePanther>(pev);
}

//=========================================================
// monster_cine_barney (Spawn)
//=========================================================
DLLEXPORT void monster_cine_barney(entvars_t* pev)
{
	CineAlloc<CCineBarney>(pev);
}

//=========================================================
// monster_cine2_scientist (Spawn)
//=========================================================
DLLEXPORT void monster_cine2_scientist(entvars_t* pev)
{
	CineAlloc<CCine2Scientist>(pev);
}

//=========================================================
// monster_cine2_hvyweapons (Spawn)
//=========================================================
DLLEXPORT void monster_cine2_hvyweapons(entvars_t* pev)
{
	CineAlloc<CCine2HvyWeapons>(pev);
}

//=========================================================
// monster_cine2_slave (Spawn)
//=========================================================
DLLEXPORT void monster_cine2_slave(entvars_t* pev)
{
	CineAlloc<CCine2Slave>(pev);
}

//=========================================================
// monster_cine3_scientist (Spawn)
//=========================================================
DLLEXPORT void monster_cine3_scientist(entvars_t* pev)
{
	CineAlloc<CCine3Scientist>(pev);
}

//=========================================================
// monster_cine3_barney (Spawn)
//=========================================================
DLLEXPORT void monster_cine3_barney(entvars_t* pev)
{
	CineAlloc<CCine3Barney>(pev);
}
