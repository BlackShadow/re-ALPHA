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
// Cycler - Model display cycler test entities
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// cycler constants
//=========================================================

#define CYCLER_THINK_INTERVAL	0.1f
#define CYCLER_SPIN_TIME		4.5f
#define CYCLER_SPIN_RATE		4.0f
#define CYCLER_PRIVATE_SIZE		344

static const char kCyclerClassname[] = "cycler";

//=========================================================
// CCycler - generic model cycler
//
// A cycler displays a single model.  When "used" it spins
// in place for a few seconds so the model can be inspected
// from every angle, and when shot it steps through its
// animation frames one sequence at a time.
//=========================================================

// CCycler derives from CBaseMonster: the shared CBaseMonster::TakeDamage
// (binary vtable slot 17) handles all damage and dispatches
// the Pain virtual (slot 13). Cyclers carry 80000 health so they survive every
// hit; their Pain override just steps the display animation.
class CCycler : public CBaseMonster
{
public:
	void SpawnWithInfo(const char* pszModel, float minX, float minY, float minZ, float maxX, float maxY, float maxZ, float originZAdjust);
	void Think(CBaseEntity* pOther);
	void Use(CBaseEntity* pOther);
	void KeyValue(KeyValueData* pkvd);
	int Classify();

private:
	float& SpawnClearedTime();
	float& SpinEndTime();
	void Pain(float flDamage);		// vtable slot 13
};

HL_COMPILE_TIME_ASSERT(sizeof(CCycler) <= 344, CCycler_private_data_size);

//=========================================================
// cycler variant classes
//
// The original DLL gives each cycler_* export a distinct
// vtable.  Its Spawn slot supplies the hard-coded model and
// bounds, then runs the shared body at the binary.
//=========================================================
class CGenericCycler : public CCycler
{
public:
	void Spawn();
};

class CCyclerScientist : public CCycler
{
public:
	void Spawn();
};

class CCyclerHeadcrab : public CCycler
{
public:
	void Spawn();
};

class CCyclerPanther : public CCycler
{
public:
	void Spawn();
};

class CCyclerHoundeye : public CCycler
{
public:
	void Spawn();
};

class CCyclerSecure : public CCycler
{
public:
	void Spawn();
};

class CCyclerBullchicken : public CCycler
{
public:
	void Spawn();
};

class CCyclerDoctor : public CCycler
{
public:
	void Spawn();
};

class CCyclerRedDoc : public CCycler
{
public:
	void Spawn();
};

class CCyclerGreenDoc : public CCycler
{
public:
	void Spawn();
};

class CCyclerBlueDoc : public CCycler
{
public:
	void Spawn();
};

class CCyclerTurret : public CCycler
{
public:
	void Spawn();
};

class CCyclerHumanAssault : public CCycler
{
public:
	void Spawn();
};

class CCyclerHumanGrunt : public CCycler
{
public:
	void Spawn();
};

class CCyclerDesert : public CCycler
{
public:
	void Spawn();
};

class CCyclerOlive : public CCycler
{
public:
	void Spawn();
};

class CCyclerAlienGrunt : public CCycler
{
public:
	void Spawn();
};

class CCyclerAlienSlave : public CCycler
{
public:
	void Spawn();
};

class CCyclerPrdroid : public CCycler
{
public:
	void Spawn();
};

//=========================================================
// KeyValue
//
// Compatibility-only support for a plain "cycler" map entity:
// the original alpha export table only binds cycler_* variants,
// but surrounding maps can carry a generic cycler with a model key.
// Store the key in entvars, not in private data.
//=========================================================
void CCycler::KeyValue(KeyValueData* pkvd)
{
	if (pkvd && pkvd->szKeyName && pkvd->szValue
		&& strcmp(pkvd->szKeyName, "model") == 0)
	{
		PevInt(pev, PEV_MODEL) = EngineAllocString(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else
	{
		CBaseEntity::KeyValue(pkvd);
	}
}

//=========================================================
// SpawnWithInfo (shared)
//
// Per-variant Spawn slots only supply
// the model path and bounding box before tail-calling the
// shared spawn body.
//=========================================================
void CCycler::SpawnWithInfo(const char* pszModel, float minX, float minY, float minZ, float maxX, float maxY, float maxZ, float originZAdjust)
{
	if (!pszModel)
		return;

	// cycler_prdroid raises origin.z by 16 before the shared
	// spawn body runs; every other variant leaves the origin untouched.
	PevVector(pev, PEV_ORIGIN).z += originZAdjust;

	EnginePrecacheModel(pszModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, pszModel);

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 0.0f;
	PevFloat(pev, PEV_TAKEDAMAGE) = 2.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 80000.0f;
	PevFloat(pev, PEV_YAWSPEED) = 5.0f;

	if (edict)
	{
		float mins[3];
		float maxs[3];
		VecSet(mins, minX, minY, minZ);
		VecSet(maxs, maxX, maxY, maxZ);
		EngineSetSize(edict, mins, maxs);
	}

	// The binary raises origin.z by a DIRECT entvars store (line 225) and does
	// NOT call the engine SetOrigin slot; SetModel/SetSize above already relink the
	// entity with the bumped origin, so no explicit SetOrigin is issued.

	m_flGroundSpeed = 0.0f;
	m_flFrameRate = 75.0f;
	SpawnClearedTime() = 0.0f;

	PevInt(pev, PEV_CLASSNAME) = EngineAllocString(kCyclerClassname);
	PevInt(pev, PEV_SEQUENCE) = 0;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + 1.0f;

	ResetSequenceInfo(CYCLER_THINK_INTERVAL);

	// The binary installs ONLY m_pfnUse; it leaves m_pfnThink NULL. CCycler
	// overrides the virtual Think (slot 5), so DispatchThink -> CCycler::Think()
	// dispatches through the vtable without a stored think pointer.
	SetUse(&CCycler::Use);
}

void CGenericCycler::Spawn()
{
	if (PevInt(pev, PEV_MODEL) == 0)
		return;

	SpawnWithInfo(EngineStringFromIndex(PevInt(pev, PEV_MODEL)), -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerScientist::Spawn()
{
	SpawnWithInfo("models/scientist.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerHeadcrab::Spawn()
{
	SpawnWithInfo("models/headcrab.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerPanther::Spawn()
{
	SpawnWithInfo("models/panther.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerHoundeye::Spawn()
{
	SpawnWithInfo("models/houndeye.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerSecure::Spawn()
{
	SpawnWithInfo("models/barney.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerBullchicken::Spawn()
{
	SpawnWithInfo("models/bullchik.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerDoctor::Spawn()
{
	SpawnWithInfo("models/doctor.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerRedDoc::Spawn()
{
	SpawnWithInfo("models/reddoc.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerGreenDoc::Spawn()
{
	SpawnWithInfo("models/greendoc.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerBlueDoc::Spawn()
{
	SpawnWithInfo("models/bluedoc.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerTurret::Spawn()
{
	SpawnWithInfo("models/turret.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerHumanAssault::Spawn()
{
	SpawnWithInfo("models/hassault.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerHumanGrunt::Spawn()
{
	SpawnWithInfo("models/hgrunt.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerDesert::Spawn()
{
	SpawnWithInfo("models/desert.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerOlive::Spawn()
{
	SpawnWithInfo("models/olive.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerAlienGrunt::Spawn()
{
	SpawnWithInfo("models/agrunt.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerAlienSlave::Spawn()
{
	SpawnWithInfo("models/islave.mdl", -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 64.0f, 0.0f);
}

void CCyclerPrdroid::Spawn()
{
	SpawnWithInfo("models/prdroid.mdl", -16.0f, -16.0f, -16.0f, 16.0f, 16.0f, 16.0f, 16.0f);
}

//=========================================================
// SpawnClearedTime
//
// The shared spawn body clears this+336,
// just before the end of the 344-byte private-data block.
//=========================================================
float& CCycler::SpawnClearedTime()
{
	return *(float*)((unsigned char*)this + 336);
}

//=========================================================
// SpinEndTime
//
// Use writes this+340; Think compares global time against the same slot.
//=========================================================
float& CCycler::SpinEndTime()
{
	return *(float*)((unsigned char*)this + 340);
}

//=========================================================
// Think
//
// While inside the spin window opened by Use(), rotate the
// model about its yaw so the player can see every side.
//=========================================================
void CCycler::Think(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + CYCLER_THINK_INTERVAL;

	if (GlobalTime() < SpinEndTime())
		PevVector(pev, PEV_ANGLES).y += CYCLER_SPIN_RATE;

	AdvanceAnimation(CYCLER_THINK_INTERVAL);
}

//=========================================================
// Use
//
// Open a spin window so the model rotates for a few seconds.
//=========================================================
void CCycler::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	SpinEndTime() = GlobalTime() + CYCLER_SPIN_TIME;
}

//=========================================================
// Classify
//=========================================================
int CCycler::Classify()
{
	return 0;
}

//=========================================================
// Pain - vtable slot 13
//
// Dispatched by the shared CBaseMonster::TakeDamage (slot 17)
// each time the cycler survives a hit. It restores the damage
// just subtracted (keeping the cycler effectively invulnerable)
// and steps the display animation one sequence per shot,
// wrapping back to the first sequence when the model has no
// further frame rate.
//=========================================================
void CCycler::Pain(float flDamage)
{
	PevFloat(pev, PEV_HEALTH) += flDamage;
	PevInt(pev, PEV_SEQUENCE) += 1;

	ResetSequenceInfo(CYCLER_THINK_INTERVAL);

	if ((*(int*)&m_flFrameRate & 0x7FFFFFFF) == 0)
	{
		PevInt(pev, PEV_SEQUENCE) = 0;
		ResetSequenceInfo(CYCLER_THINK_INTERVAL);
	}

	PevFloat(pev, PEV_FRAME) = 0.0f;
}

//=========================================================
// Cycler construction wrappers
//
// Every cycler_X export allocates the shared CCycler private
// data and installs the matching variant vtable.  The engine
// then dispatches Spawn() through that vtable to finish
// initialisation.
//=========================================================

template <class T>
static T* CyclerAlloc(entvars_t* pev)
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

	privateData = EngineAllocPrivateData(edict, CYCLER_PRIVATE_SIZE);
	if (!privateData)
		return NULL;

	memset(privateData, 0, CYCLER_PRIVATE_SIZE);

	T* cycler = new (privateData) T();
	cycler->pev = entvars;
	cycler->m_pGlobals = GlobalsFromEntvars(entvars);
	return cycler;
}

//=========================================================
// cycler
//
// Compatibility helper for local experiments with a generic model cycler.
// Not exported by hl.def because the original alpha DLL does not expose a
// plain "cycler" factory.
//=========================================================
DLLEXPORT void cycler(entvars_t* pev)
{
	CyclerAlloc<CGenericCycler>(pev);
}

//=========================================================
// cycler_scientist
//=========================================================
DLLEXPORT void cycler_scientist(entvars_t* pev)
{
	CyclerAlloc<CCyclerScientist>(pev);
}

//=========================================================
// cycler_headcrab
//=========================================================
DLLEXPORT void cycler_headcrab(entvars_t* pev)
{
	CyclerAlloc<CCyclerHeadcrab>(pev);
}

//=========================================================
// cycler_panther
//=========================================================
DLLEXPORT void cycler_panther(entvars_t* pev)
{
	CyclerAlloc<CCyclerPanther>(pev);
}

//=========================================================
// cycler_houndeye
//=========================================================
DLLEXPORT void cycler_houndeye(entvars_t* pev)
{
	CyclerAlloc<CCyclerHoundeye>(pev);
}

//=========================================================
// cycler_secure
//=========================================================
DLLEXPORT void cycler_secure(entvars_t* pev)
{
	CyclerAlloc<CCyclerSecure>(pev);
}

//=========================================================
// cycler_bullchicken
//=========================================================
DLLEXPORT void cycler_bullchicken(entvars_t* pev)
{
	CyclerAlloc<CCyclerBullchicken>(pev);
}

//=========================================================
// cycler_doctor
//=========================================================
DLLEXPORT void cycler_doctor(entvars_t* pev)
{
	CyclerAlloc<CCyclerDoctor>(pev);
}

//=========================================================
// cycler_reddoc
//=========================================================
DLLEXPORT void cycler_reddoc(entvars_t* pev)
{
	CyclerAlloc<CCyclerRedDoc>(pev);
}

//=========================================================
// cycler_greendoc
//=========================================================
DLLEXPORT void cycler_greendoc(entvars_t* pev)
{
	CyclerAlloc<CCyclerGreenDoc>(pev);
}

//=========================================================
// cycler_bluedoc
//=========================================================
DLLEXPORT void cycler_bluedoc(entvars_t* pev)
{
	CyclerAlloc<CCyclerBlueDoc>(pev);
}

//=========================================================
// cycler_turret
//=========================================================
DLLEXPORT void cycler_turret(entvars_t* pev)
{
	CyclerAlloc<CCyclerTurret>(pev);
}

//=========================================================
// cycler_human_assault
//=========================================================
DLLEXPORT void cycler_human_assault(entvars_t* pev)
{
	CyclerAlloc<CCyclerHumanAssault>(pev);
}

//=========================================================
// cycler_human_grunt
//=========================================================
DLLEXPORT void cycler_human_grunt(entvars_t* pev)
{
	CyclerAlloc<CCyclerHumanGrunt>(pev);
}

//=========================================================
// cycler_desert
//=========================================================
DLLEXPORT void cycler_desert(entvars_t* pev)
{
	CyclerAlloc<CCyclerDesert>(pev);
}

//=========================================================
// cycler_olive
//=========================================================
DLLEXPORT void cycler_olive(entvars_t* pev)
{
	CyclerAlloc<CCyclerOlive>(pev);
}

//=========================================================
// cycler_alien_grunt
//=========================================================
DLLEXPORT void cycler_alien_grunt(entvars_t* pev)
{
	CyclerAlloc<CCyclerAlienGrunt>(pev);
}

//=========================================================
// cycler_alien_slave
//=========================================================
DLLEXPORT void cycler_alien_slave(entvars_t* pev)
{
	CyclerAlloc<CCyclerAlienSlave>(pev);
}

//=========================================================
// cycler_prdroid
//=========================================================
DLLEXPORT void cycler_prdroid(entvars_t* pev)
{
	CyclerAlloc<CCyclerPrdroid>(pev);
}
