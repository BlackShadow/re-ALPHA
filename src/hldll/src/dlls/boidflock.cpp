/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// monster_boid_flock - spawns a flock of boids, then
// removes itself
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"

// boid.cpp
edict_t *BoidFlockCreateMember(const Vector &vecOrigin, const Vector &vecAngles);

class CFlockingFlyerFlock : public CBaseMonster
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void SpawnFlock();

	int		m_cFlockSize;
	float	m_flFlockRadius;	// the boids spawn this far from the flock origin at most
};

void CFlockingFlyerFlock::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "value1"))
	{
		m_cFlockSize = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "value2"))
	{
		m_flFlockRadius = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// SpawnFlock - scatters the boids around the flock origin
//=========================================================
void CFlockingFlyerFlock::SpawnFlock()
{
	PRECACHE_SOUND("boid/sonar.wav");
	PRECACHE_MODEL("models/boid.mdl");

	float flRadius = m_flFlockRadius;

	for (int iCount = 0; iCount < m_cFlockSize; iCount++)
	{
		// z first, then y and x
		Vector vecOffset;
		vecOffset.z = RANDOM_FLOAT(0.0f, flRadius);
		vecOffset.y = RANDOM_FLOAT(-flRadius, flRadius);
		vecOffset.x = RANDOM_FLOAT(-flRadius, flRadius);

		Vector vecAngles = pev->angles;
		Vector vecOrigin = pev->origin + vecOffset;

		BoidFlockCreateMember(vecOrigin, vecAngles);
	}

	REMOVE_ENTITY(ENT(pev));
}

void CFlockingFlyerFlock::Spawn()
{
	SpawnFlock();
}

LINK_ENTITY_TO_CLASS(monster_boid_flock, CFlockingFlyerFlock);
