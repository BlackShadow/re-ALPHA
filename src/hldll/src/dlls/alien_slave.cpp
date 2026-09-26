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
// Alien slave - wanders around, has no attacks
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

#define ISLAVE_HEALTH			30.0f
#define ISLAVE_YAW_SPEED		8.0f

// islave.mdl sequences, named by the monster state that plays them
enum
{
	ISLAVE_SEQ_IDLE = 0,		// also MONSTERSTATE_IDLE2
	ISLAVE_SEQ_IDLE3,
	ISLAVE_SEQ_WALK = 3,
	ISLAVE_SEQ_RUN,				// MONSTERSTATE_CHASE
	ISLAVE_SEQ_SPAWN = 13,		// matches no state, so the first SetActivity always switches
};

static const char szISlaveModel[] = "models/islave.mdl";

class CISlave : public CBaseMonster
{
public:
	void Spawn();
	int Classify();
	void SetActivity(int activity);
	int CheckAttacks(entvars_t *pevEnemy, float flDist);
	void Death(int iDeathType);
	void RemoveThink(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(monster_alien_slave, CISlave);

//=========================================================
// Spawn
//=========================================================
void CISlave::Spawn()
{
	PRECACHE_MODEL(szISlaveModel);

	SET_MODEL(ENT(pev), szISlaveModel);
	UTIL_SetSize(pev, Vector(-18.0f, -18.0f, 0.0f), Vector(18.0f, 18.0f, 72.0f));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = ISLAVE_HEALTH;
	pev->yaw_speed = ISLAVE_YAW_SPEED;
	pev->sequence = ISLAVE_SEQ_SPAWN;

	m_MonsterState = MONSTERSTATE_IDLE;

	pev->nextthink += 1.0f;
	WalkMonsterStart(NULL);
}

int CISlave::Classify()
{
	return CLASS_NONE;
}

//=========================================================
// SetActivity
//=========================================================
void CISlave::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_IDLE2:
		sequence = ISLAVE_SEQ_IDLE;
		break;

	case MONSTERSTATE_IDLE3:
		sequence = ISLAVE_SEQ_IDLE3;
		break;

	case MONSTERSTATE_WALK:
		sequence = ISLAVE_SEQ_WALK;
		break;

	case MONSTERSTATE_CHASE:
		sequence = ISLAVE_SEQ_RUN;
		break;

	default:
		ALERT(at_console, "ISlave's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == sequence)
		return;

	pev->sequence = sequence;
	pev->frame = 0;

	switch (sequence)
	{
	case ISLAVE_SEQ_IDLE:
	case ISLAVE_SEQ_IDLE3:
	case ISLAVE_SEQ_WALK:
	case ISLAVE_SEQ_RUN:
		break;

	default:
		ALERT(at_console, "Bogus ISlave anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// Death - the body is removed right away
//=========================================================
void CISlave::Death(int iDeathType)
{
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time;
}

int CISlave::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	return FALSE;
}

void CISlave::RemoveThink(CBaseEntity *pOther)
{
	REMOVE_ENTITY(ENT(pev));
}
