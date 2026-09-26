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
// Cine - inert actors that play a single sequence for
// in-engine cinematics, and a blood spraying effect
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"
#include "decals.h"

#define CINE_THINK_INTERVAL		1.0f	// actors advance their sequence once a second
#define CINE_BLOOD_INTERVAL		0.1f
#define CINE_BLOOD_GUSHES		20		// start of the gush counter kept in pev->health

//=========================================================
// CCineMonster - shared base of the monster_cine* actors
//=========================================================
class CCineMonster : public CBaseMonster
{
public:
	void CineSpawn(const char *szModel);
	void Use(CBaseEntity *pOther);
	void CineThink(CBaseEntity *pOther);
	void Death(int iDeathType);
};

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
// CineSpawn - sets up the actor with the given model
//=========================================================
void CCineMonster::CineSpawn(const char *szModel)
{
	PRECACHE_MODEL(szModel);
	SET_MODEL(ENT(pev), szModel);
	UTIL_SetSize(pev, Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0.0f;
	pev->health = 1.0f;
	pev->yaw_speed = 10.0f;

	// maps set the sequence through the impulse key
	pev->sequence = (int)pev->impulse;

	ResetSequenceInfo(MONSTER_THINK_INTERVAL);

	pev->framerate = 0.0f;

	m_flDistTooFar = 999999.0f;		// never chase
	m_bloodColor = BLOOD_COLOR_RED;

	// actors without a targetname start on their own
	if (FStringNull(pev->targetname))
	{
		SetThink(&CCineMonster::CineThink);
		pev->nextthink += 1.0f;
	}
}

void CCineScientist::Spawn()
{
	CineSpawn("models/cine-scientist.mdl");
}

void CCinePanther::Spawn()
{
	CineSpawn("models/cine-panther.mdl");
}

void CCineBarney::Spawn()
{
	CineSpawn("models/cine-barney.mdl");
}

void CCine2Scientist::Spawn()
{
	CineSpawn("models/cine2-scientist.mdl");
}

void CCine2HvyWeapons::Spawn()
{
	CineSpawn("models/cine2_hvyweapons.mdl");
}

void CCine2Slave::Spawn()
{
	CineSpawn("models/cine2_slave.mdl");
}

void CCine3Scientist::Spawn()
{
	CineSpawn("models/cine3-scientist.mdl");
}

void CCine3Barney::Spawn()
{
	CineSpawn("models/cine3-barney.mdl");
}

//=========================================================
// Use - restarts the sequence
//=========================================================
void CCineMonster::Use(CBaseEntity *pOther)
{
	pev->animtime = 0.0f;
	SetThink(&CCineMonster::CineThink);
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// CineThink - plays the sequence. Actors with spawnflags
// set remove themselves once it has finished.
//=========================================================
void CCineMonster::CineThink(CBaseEntity *pOther)
{
	if (pev->animtime == 0.0f)
		ResetSequenceInfo(MONSTER_THINK_INTERVAL);

	pev->nextthink = gpGlobals->time + CINE_THINK_INTERVAL;

	if (pev->spawnflags != 0.0f && m_fSequenceFinished)
		Death(DEATH_NORMAL);
	else
		AdvanceAnimation(CINE_THINK_INTERVAL);
}

void CCineMonster::Death(int iDeathType)
{
	SetThink(&CBaseEntity::SUB_Remove);
}

//=========================================================
// CCineBlood - sprays blood for a while once used
//=========================================================
class CCineBlood : public CBaseEntity
{
public:
	void Spawn();
	void BloodStart(CBaseEntity *pOther);
	void BloodGush(CBaseEntity *pOther);
};

void CCineBlood::Spawn()
{
	pev->solid = SOLID_NOT;
	SetUse(&CCineBlood::BloodStart);
	pev->health = CINE_BLOOD_GUSHES;
}

void CCineBlood::BloodStart(CBaseEntity *pOther)
{
	SetThink(&CCineBlood::BloodGush);
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// BloodGush - sprays blood from the origin and now and then
// splats a blood decal on the floor
//=========================================================
void CCineBlood::BloodGush(CBaseEntity *pOther)
{
	const Vector &vecOrigin = pev->origin;

	pev->nextthink = gpGlobals->time + CINE_BLOOD_INTERVAL;

	UTIL_MakeVectors(pev->angles);

	// health counts the gushes down
	if (pev->health-- < 0.0f)
		REMOVE_ENTITY(ENT(pev));

	int iSpeed;
	if (RANDOM_FLOAT(0.0f, 1.0f) >= 0.7f)
	{
		// now and then a stream
		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_BLOODSTREAM);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.x);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.y);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.z);
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(0.0f, 1.0f));
		WRITE_BYTE(MSG_BROADCAST, BLOOD_COLOR_RED);
		iSpeed = RANDOM_LONG(50, 150);
	}
	else
	{
		// mostly globs
		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_BLOOD);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.x);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.y);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.z);
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(0.0f, 1.0f));
		WRITE_BYTE(MSG_BROADCAST, BLOOD_COLOR_RED);
		iSpeed = 10;
	}
	WRITE_BYTE(MSG_BROADCAST, iSpeed);

	if (RANDOM_FLOAT(0.0f, 1.0f) < 0.75f)
	{
		// splat the floor below, randomized a bit
		Vector vecSpread = gpGlobals->v_right * (RANDOM_FLOAT(-1.0f, 1.0f) * 0.6f);
		Vector vecSplatDir = gpGlobals->v_forward * (RANDOM_FLOAT(-1.0f, 1.0f) * 0.6f) + Vector(0.0f, 0.0f, -1.0f) + vecSpread;

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		UTIL_TraceLine(vecOrigin + Vector(0.0f, 0.0f, 64.0f), vecOrigin + vecSplatDir * 256.0f, ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0f)
		{
			WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(MSG_BROADCAST, TE_DECAL);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.x);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.y);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.z);
			WRITE_SHORT(MSG_BROADCAST, ENTINDEX(tr.pHit));
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_BLOOD1, DECAL_BLOOD6));
		}
	}
}

LINK_ENTITY_TO_CLASS(cine_blood, CCineBlood);

LINK_ENTITY_TO_CLASS(monster_cine_scientist, CCineScientist);
LINK_ENTITY_TO_CLASS(monster_cine_panther, CCinePanther);
LINK_ENTITY_TO_CLASS(monster_cine_barney, CCineBarney);
LINK_ENTITY_TO_CLASS(monster_cine2_scientist, CCine2Scientist);
LINK_ENTITY_TO_CLASS(monster_cine2_hvyweapons, CCine2HvyWeapons);
LINK_ENTITY_TO_CLASS(monster_cine2_slave, CCine2Slave);
LINK_ENTITY_TO_CLASS(monster_cine3_scientist, CCine3Scientist);
LINK_ENTITY_TO_CLASS(monster_cine3_barney, CCine3Barney);
