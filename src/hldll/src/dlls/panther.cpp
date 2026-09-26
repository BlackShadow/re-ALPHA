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
// Panther - walks around, has no attacks and no death
// animation
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define PANTHER_HEALTH				50.0f
#define PANTHER_BLOOD_COLOR			146
#define PANTHER_THINK_INTERVAL		0.1f

// panther.mdl sequences
enum
{
	PANTHER_SEQ_WALK = 0,		// the only one SetActivity uses
	PANTHER_SEQ_SPAWN = 2,		// set by Spawn, until the first think
};

class CPanther : public CBaseMonster
{
public:
	void Spawn();
	int Classify();
	void SetActivity(int activity);
	void Death(int iDeathType);

	void RemoveThink(CBaseEntity *pOther);
};

//=========================================================
// Spawn
//=========================================================
void CPanther::Spawn()
{
	PRECACHE_MODEL("models/panther.mdl");

	SET_MODEL(ENT(pev), "models/panther.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 64));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = PANTHER_HEALTH;
	pev->yaw_speed = 10.0f;
	pev->sequence = PANTHER_SEQ_SPAWN;

	m_bloodColor = PANTHER_BLOOD_COLOR;

	pev->nextthink = pev->nextthink + RANDOM_FLOAT(0.0f, 0.5f) + 0.5f;
	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// Classify
//=========================================================
int CPanther::Classify()
{
	return CLASS_PANTHER;
}

//=========================================================
// SetActivity - plays the sequence for a monster state
//=========================================================
void CPanther::SetActivity(int activity)
{
	int iSequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_IDLE2:
	case MONSTERSTATE_IDLE3:
	case MONSTERSTATE_WALK:
	case MONSTERSTATE_CHASE:
	case MONSTERSTATE_MELEE_ATTACK:
		iSequence = PANTHER_SEQ_WALK;
		break;

	default:
		ALERT(at_error, "Panther's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == iSequence)
		return;

	pev->sequence = iSequence;
	pev->frame = 0;
	ResetSequenceInfo(PANTHER_THINK_INTERVAL);

	if (pev->sequence != PANTHER_SEQ_WALK)
	{
		ALERT(at_error, "Bogus Panther anim: %d", pev->sequence);
		m_flFrameRate = 0;
		m_flGroundSpeed = 0;
	}
}

//=========================================================
// Death - no death animation, removed on the next think
//=========================================================
void CPanther::Death(int iDeathType)
{
	SetThink(&CBaseEntity::SUB_Remove);
}

//=========================================================
// RemoveThink - not used
//=========================================================
void CPanther::RemoveThink(CBaseEntity *pOther)
{
	REMOVE_ENTITY(ENT(pev));
}

LINK_ENTITY_TO_CLASS(monster_panther, CPanther);
