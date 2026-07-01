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
// BoidFlock - spawns a flock of boids then removes itself
//=========================================================

#include <new>
#include <stdlib.h>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// flock-specific constants
//=========================================================

#define BOIDFLOCK_PRIVATE_DATA_SIZE	344

static const char kBoidModel[] = "models/boid.mdl";
static const char kSonarSound[] = "boid/sonar.wav";

//=========================================================
// boid.cpp creates the individual flock members
//=========================================================

edict_t* BoidFlockCreateMember(const float* origin, const float* angles);

//=========================================================
// CFlockingFlyerFlock
//=========================================================

class CFlockingFlyerFlock : public CBaseMonster
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void SpawnFlock();

private:
	int m_cFlockSize;		// 336: flock size upper bound ("value1")
	float m_flFlockRadius;	// 340: spawn scatter radius ("value2")
};

HL_COMPILE_TIME_ASSERT(sizeof(CFlockingFlyerFlock) <= BOIDFLOCK_PRIVATE_DATA_SIZE, CFlockingFlyerFlock_private_data_size);

//=========================================================
// KeyValue
//=========================================================
void CFlockingFlyerFlock::KeyValue(KeyValueData* pkvd)
{
	if (!pkvd)
		return;

	if (strcmp(pkvd->szKeyName, "value1") == 0)
	{
		m_cFlockSize = atoi(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "value2") == 0)
	{
		m_flFlockRadius = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// SpawnFlock
// Scatters boids around the flock origin within m_flFlockRadius,
// then removes the flock entity itself.
//=========================================================
void CFlockingFlyerFlock::SpawnFlock()
{
	EnginePrecacheSound(kSonarSound);
	EnginePrecacheModel(kBoidModel);

	float flRadius = m_flFlockRadius;

	// The binary: counter starts at 1, pre-increments at the top of
	// the body, and loops while m_cFlockSize >= counter -> the body runs with the
	// counter at 2,3,...,m_cFlockSize+1, i.e. exactly m_cFlockSize members.
	for (int i = 2; i <= m_cFlockSize + 1; ++i)
	{
		// RNG draw order matches the binary: the vertical (z) offset is drawn
		// first, then the two horizontal offsets
		float vecOffset[3];
		vecOffset[2] = RandomFloat(0.0f, flRadius);
		vecOffset[1] = RandomFloat(-flRadius, flRadius);
		vecOffset[0] = RandomFloat(-flRadius, flRadius);

		float vecAngles[3];
		vecAngles[0] = PevVector(pev, PEV_ANGLES).x;
		vecAngles[1] = PevVector(pev, PEV_ANGLES).y;
		vecAngles[2] = PevVector(pev, PEV_ANGLES).z;

		float vecOrigin[3];
		vecOrigin[0] = PevVector(pev, PEV_ORIGIN).x + vecOffset[0];
		vecOrigin[1] = PevVector(pev, PEV_ORIGIN).y + vecOffset[1];
		vecOrigin[2] = PevVector(pev, PEV_ORIGIN).z + vecOffset[2];

		BoidFlockCreateMember(vecOrigin, vecAngles);
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// Spawn (vtable slot 0 -> SpawnFlock)
//=========================================================
void CFlockingFlyerFlock::Spawn()
{
	SpawnFlock();
}

//=========================================================
// monster_boid_flock
//=========================================================
DLLEXPORT void monster_boid_flock(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, BOIDFLOCK_PRIVATE_DATA_SIZE);
		if (!privateData)
			return;

		memset(privateData, 0, BOIDFLOCK_PRIVATE_DATA_SIZE);

		CFlockingFlyerFlock* monster = new (privateData) CFlockingFlyerFlock();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
