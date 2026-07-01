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
// Explode - delayed/triggered particle burst entity
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// pev field offsets used by explode that are not yet named
// in utils.h.
//   440 - pev->target (the targetname the activator points at)
//   28  - pev->ltime (local time, mirrored game time)
//=========================================================
// pev offset 440 (targetname) and 28 (ltime) are PEV_TARGETNAME / PEV_LTIME in utils.h.

//=========================================================
// Engine globalvars_t offset read straight from the globals
// block the engine fills before the call.
//   116 - index of the entity that activated this entity
//=========================================================
enum
{
	GLOBALS_ACTIVATOR_ENT	= 116,
};

//=========================================================
// Particle burst colours / counts.
//=========================================================
#define EXPLODE_PARTICLE_COLOR	65280		// 0xFF00

//=========================================================
// CExplode
//
// Original private-data block is 56 bytes. CBaseEntity occupies
// the first 28 bytes (this[0..6]); the entity-specific dwords
// map exactly onto these members:
//   this[7]  m_fUseHeight     (read only; never written -> always 0)
//   this[8]  m_iLowTime       (read only; never written -> always 0)
//   this[9]  m_iExplodeValue  (explodeMin/Mod/Max)
//   this[10] m_flStartTime    (read both as int bits and float)
//   this[11] m_fArmed
//   this[12] m_iszTriggers
//   this[13] m_iszTimers
//
// this[7]/this[8] are never assigned anywhere in the binary; they
// stay zero from the zero-filled allocation. They are kept as real
// members (not constant-folded away) so every following member sits
// at its exact original byte offset and the object fills the whole
// 56-byte private-data block (28 + 7*4).
//=========================================================

class CExplode : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pOther);

	void DetonateThink(CBaseEntity* pOther);

private:
	int m_fUseHeight;		// this[7]  (always 0)
	int m_iLowTime;			// this[8]  (always 0)
	int m_iExplodeValue;	// this[9]
	int m_flStartTime;		// this[10] (float bits)
	int m_fArmed;			// this[11]
	int m_iszTriggers;		// this[12]
	int m_iszTimers;		// this[13]
};

HL_COMPILE_TIME_ASSERT(sizeof(CExplode) <= 56, CExplode_private_data_size);

//=========================================================
// Particle burst (engine ParticleEffect).
// The original passes the colour and count through twice as
// IDA artefacts; only origin/direction/colour/count reach the
// engine call.
//=========================================================
static void ExplodeParticleEffect(const float* origin, const float* direction, int color, int count)
{
	EngineParticleEffect(origin, direction, (float)color, (float)count);
}

//=========================================================
// Spawn
//=========================================================
void CExplode::Spawn()
{
	if (m_iszTriggers == 0)
		m_fArmed = 1;

	if (m_iszTimers == 0)
	{
		// 0x47C35000 == 100000.0f
		float bigTime = 100000.0f;
		m_flStartTime = *(int*)&bigTime;
	}

	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + 1.0f;

	// The binary stores the detonate thunk in the m_pfnUse
	// member (object offset 0x14), NOT m_pfnThink (0x0C). The scheduled
	// think dispatches through the (default) Think virtual, which reads
	// m_pfnThink and therefore never fires this thunk - matching the
	// original's dormant behaviour. (This is why explode never detonates
	// on its own in the alpha.)
	SetUse(&CExplode::DetonateThink);
}

//=========================================================
// DetonateThink (vtable thunk to Use)
//
// The binary is `MOV EAX,[ECX]; JMP [EAX+0x1C]`, i.e. a tail-call to
// the entity's Use vtable slot. It is installed into the m_pfnUse
// member by Spawn but is never invoked at runtime (Use is dispatched
// directly through the vtable, and the scheduled think looks at
// m_pfnThink). It is kept here for fidelity to the stored value.
//=========================================================
void CExplode::DetonateThink(CBaseEntity* pOther)
{
	Use(pOther);
}

//=========================================================
// KeyValue
//=========================================================
void CExplode::KeyValue(KeyValueData* pkvd)
{
	if (!pkvd)
		return;

	if (strcmp(pkvd->szKeyName, "triggers") == 0)
	{
		m_iszTriggers = EngineAllocString(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "timers") == 0)
	{
		m_iszTimers = EngineAllocString(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "explodeMin") == 0
		|| strcmp(pkvd->szKeyName, "explodeMod") == 0
		|| strcmp(pkvd->szKeyName, "explodeMax") == 0)
	{
		// All three keys write the same slot in the original.
		m_iExplodeValue = atol(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "style") == 0
		|| strcmp(pkvd->szKeyName, "height") == 0
		|| strcmp(pkvd->szKeyName, "killtarget") == 0
		|| strcmp(pkvd->szKeyName, "value1") == 0
		|| strcmp(pkvd->szKeyName, "value2") == 0
		|| strcmp(pkvd->szKeyName, "value3") == 0)
	{
		pkvd->fHandled = 1;
	}
}

//=========================================================
// Use
//
// The activator is taken from gpGlobals (the entity that
// fired the +use), so the passed-in arguments are ignored.
// "triggers"/"timers" are matched against the activator's
// target field; once both gates pass the entity emits a
// particle burst whose density scales with elapsed time.
//=========================================================
void CExplode::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = GlobalsFromEntvars(pev);
	if (!globals)
		return;

	int activatorIndex = *GlobalsInt(globals, GLOBALS_ACTIVATOR_ENT);

	// "triggers" gate: match the activator's target against our trigger name.
	if (m_iszTriggers != 0)
	{
		edict_t* pActivatorEdict = EnginePEntityOfEntIndex(activatorIndex);
		const char* pszTrigger = EngineStringFromIndex(m_iszTriggers);
		entvars_t* pevActivator = pActivatorEdict ? EngineGetVarsOfEnt(pActivatorEdict) : NULL;
		const char* pszTarget = pevActivator ? EngineStringFromIndex(PevInt(pevActivator, PEV_TARGETNAME)) : NULL;

		if (strcmp(pszTarget, pszTrigger) == 0)
			m_fArmed = 1;
	}

	// "timers" gate: when matched, latch the start time once.
	int timerMatched = 0;
	if (m_iszTimers != 0)
	{
		edict_t* pActivatorEdict = EnginePEntityOfEntIndex(activatorIndex);
		const char* pszTimer = EngineStringFromIndex(m_iszTimers);
		entvars_t* pevActivator = pActivatorEdict ? EngineGetVarsOfEnt(pActivatorEdict) : NULL;
		const char* pszTarget = pevActivator ? EngineStringFromIndex(PevInt(pevActivator, PEV_TARGETNAME)) : NULL;

		if (strcmp(pszTarget, pszTimer) == 0)
			timerMatched = 1;
	}

	if (timerMatched)
	{
		if ((m_flStartTime & 0x7FFFFFFF) == 0)
			m_flStartTime = *GlobalsInt(globals, GLOBALS_TIME);
	}

	if ((m_flStartTime & 0x7FFFFFFF) == 0)
		return;

	if (!m_fArmed)
		return;

	float flNow = *(float*)GlobalsInt(globals, GLOBALS_TIME);
	float flElapsed = flNow - *(float*)&m_flStartTime;

	int count = 100;

	if ((float)m_iExplodeValue < flElapsed)
		count = 800;

	if ((float)m_iLowTime > flElapsed)
	{
		if (m_fUseHeight)
			count = 225;
	}
	else
	{
		count = 400;
	}

	float vecDir[3];
	float vecOrigin[3];
	VecCopy(vecDir, VecPtr(PevVector(pev, PEV_ANGLES)));
	VecCopy(vecOrigin, VecPtr(PevVector(pev, PEV_ORIGIN)));

	ExplodeParticleEffect(vecOrigin, vecDir, EXPLODE_PARTICLE_COLOR, count);
}

//=========================================================
// explode
//=========================================================
DLLEXPORT void explode(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 56);
		if (!privateData)
			return;

		memset(privateData, 0, 56);

		CExplode* self = new (privateData) CExplode();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
