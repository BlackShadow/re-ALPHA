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
// Cycler - model display entities. Using one spins it
// around, shooting it steps through its sequences.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

#define CYCLER_THINK_INTERVAL	0.1f
#define CYCLER_SPIN_TIME		4.5f		// seconds a use keeps the model spinning
#define CYCLER_SPIN_RATE		4.0f		// yaw degrees per think while spinning

class CCycler : public CBaseMonster
{
public:
	void GenericCyclerSpawn(const char *szModel, Vector vecMin, Vector vecMax, float flRaise);
	void Think(CBaseEntity *pOther);
	void Use(CBaseEntity *pOther);
	void KeyValue(KeyValueData *pkvd);
	int Classify();
	void Pain(float flDamage);

	float			m_flUnused;			// cleared on spawn, never read
	float			m_flSpinEndTime;	// the model spins until this time
};

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
// KeyValue - lets a plain "cycler" take its model from
// the map
//=========================================================
void CCycler::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "model"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseEntity::KeyValue(pkvd);
	}
}

//=========================================================
// GenericCyclerSpawn - shared spawn of the cycler_* entities
//=========================================================
void CCycler::GenericCyclerSpawn(const char *szModel, Vector vecMin, Vector vecMax, float flRaise)
{
	pev->origin.z += flRaise;

	PRECACHE_MODEL(szModel);
	SET_MODEL(ENT(pev), szModel);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_AIM;
	pev->effects = 0.0f;
	pev->health = CYCLER_HEALTH;
	pev->yaw_speed = 5.0f;

	UTIL_SetSize(pev, vecMin, vecMax);

	m_flGroundSpeed = 0.0f;
	m_flFrameRate = 75.0f;
	m_flUnused = 0.0f;

	pev->classname = ALLOC_STRING("cycler");
	pev->sequence = 0;
	pev->frame = 0.0f;
	pev->nextthink += 1.0f;

	ResetSequenceInfo(CYCLER_THINK_INTERVAL);

	SetUse(&CCycler::Use);
}

void CGenericCycler::Spawn()
{
	if (FStringNull(pev->model))
		return;

	GenericCyclerSpawn(STRING(pev->model), Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerScientist::Spawn()
{
	GenericCyclerSpawn("models/scientist.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerHeadcrab::Spawn()
{
	GenericCyclerSpawn("models/headcrab.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerPanther::Spawn()
{
	GenericCyclerSpawn("models/panther.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerHoundeye::Spawn()
{
	GenericCyclerSpawn("models/houndeye.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerSecure::Spawn()
{
	GenericCyclerSpawn("models/barney.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerBullchicken::Spawn()
{
	GenericCyclerSpawn("models/bullchik.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerDoctor::Spawn()
{
	GenericCyclerSpawn("models/doctor.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerRedDoc::Spawn()
{
	GenericCyclerSpawn("models/reddoc.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerGreenDoc::Spawn()
{
	GenericCyclerSpawn("models/greendoc.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerBlueDoc::Spawn()
{
	GenericCyclerSpawn("models/bluedoc.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerTurret::Spawn()
{
	GenericCyclerSpawn("models/turret.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerHumanAssault::Spawn()
{
	GenericCyclerSpawn("models/hassault.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerHumanGrunt::Spawn()
{
	GenericCyclerSpawn("models/hgrunt.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerDesert::Spawn()
{
	GenericCyclerSpawn("models/desert.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerOlive::Spawn()
{
	GenericCyclerSpawn("models/olive.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerAlienGrunt::Spawn()
{
	GenericCyclerSpawn("models/agrunt.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerAlienSlave::Spawn()
{
	GenericCyclerSpawn("models/islave.mdl", Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f), 0.0f);
}

void CCyclerPrdroid::Spawn()
{
	// raised so the box rests on the placed origin
	GenericCyclerSpawn("models/prdroid.mdl", Vector(-16.0f, -16.0f, -16.0f), Vector(16.0f, 16.0f, 16.0f), 16.0f);
}

//=========================================================
// Think - spins the model while the time given by Use
// lasts
//=========================================================
void CCycler::Think(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + CYCLER_THINK_INTERVAL;

	if (gpGlobals->time < m_flSpinEndTime)
		pev->angles.y += CYCLER_SPIN_RATE;

	AdvanceAnimation(CYCLER_THINK_INTERVAL);
}

void CCycler::Use(CBaseEntity *pOther)
{
	m_flSpinEndTime = gpGlobals->time + CYCLER_SPIN_TIME;
}

int CCycler::Classify()
{
	return CLASS_NONE;
}

//=========================================================
// Pain - gives the damage back and steps to the next
// sequence
//=========================================================
void CCycler::Pain(float flDamage)
{
	pev->health += flDamage;
	pev->sequence++;

	ResetSequenceInfo(CYCLER_THINK_INTERVAL);

	// past the last sequence, start over
	if (m_flFrameRate == 0.0f)
	{
		pev->sequence = 0;
		ResetSequenceInfo(CYCLER_THINK_INTERVAL);
	}

	pev->frame = 0.0f;
}

// plain "cycler" with the model set by the map, not listed in hl.def
LINK_ENTITY_TO_CLASS(cycler, CGenericCycler);

LINK_ENTITY_TO_CLASS(cycler_scientist, CCyclerScientist);
LINK_ENTITY_TO_CLASS(cycler_headcrab, CCyclerHeadcrab);
LINK_ENTITY_TO_CLASS(cycler_panther, CCyclerPanther);
LINK_ENTITY_TO_CLASS(cycler_houndeye, CCyclerHoundeye);
LINK_ENTITY_TO_CLASS(cycler_secure, CCyclerSecure);
LINK_ENTITY_TO_CLASS(cycler_bullchicken, CCyclerBullchicken);
LINK_ENTITY_TO_CLASS(cycler_doctor, CCyclerDoctor);
LINK_ENTITY_TO_CLASS(cycler_reddoc, CCyclerRedDoc);
LINK_ENTITY_TO_CLASS(cycler_greendoc, CCyclerGreenDoc);
LINK_ENTITY_TO_CLASS(cycler_bluedoc, CCyclerBlueDoc);
LINK_ENTITY_TO_CLASS(cycler_turret, CCyclerTurret);
LINK_ENTITY_TO_CLASS(cycler_human_assault, CCyclerHumanAssault);
LINK_ENTITY_TO_CLASS(cycler_human_grunt, CCyclerHumanGrunt);
LINK_ENTITY_TO_CLASS(cycler_desert, CCyclerDesert);
LINK_ENTITY_TO_CLASS(cycler_olive, CCyclerOlive);
LINK_ENTITY_TO_CLASS(cycler_alien_grunt, CCyclerAlienGrunt);
LINK_ENTITY_TO_CLASS(cycler_alien_slave, CCyclerAlienSlave);
LINK_ENTITY_TO_CLASS(cycler_prdroid, CCyclerPrdroid);
