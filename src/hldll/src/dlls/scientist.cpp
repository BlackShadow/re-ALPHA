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
// Scientist - never attacks, follows the player
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

#define SCIENTIST_THINK_INTERVAL	0.1f
#define SCIENTIST_HEALTH			10.0f
#define SCIENTIST_YAW_SPEED			10.0f
#define SCIENTIST_CHASE_DIST		999999.0f	// never chases
#define SCIENTIST_FOLLOW_DIST		128.0f		// distance kept to the player he follows
#define SCIENTIST_DEATH_HOP_SPEED	200.0f		// a violent death throws the body up
#define SCIENTIST_BODY_RANDOM		-1.0f		// pev->body set by the level designer: pick one
#define SCIENTIST_NUM_BODIES		3

// SetDeathActivity types that no code passes, each plays MONSTERSTATE_DIE1 + type
#define DEATH_TYPE3					2
#define DEATH_TYPE4					3
#define DEATH_TYPE5					4

// scientist.mdl sequences, named by what SetActivity uses them for
enum
{
	SCIENTIST_SEQ_IDLE = 0,			// also MONSTERSTATE_COMBAT_IDLE
	SCIENTIST_SEQ_IDLE2,
	SCIENTIST_SEQ_IDLE3,
	SCIENTIST_SEQ_WALK,
	SCIENTIST_SEQ_RUN = 5,			// MONSTERSTATE_CHASE and MONSTERSTATE_HUNT
	SCIENTIST_SEQ_MELEE_ATTACK = 7,
	SCIENTIST_SEQ_DIE2,
	SCIENTIST_SEQ_DIE3,
	SCIENTIST_SEQ_DIE1,				// also MONSTERSTATE_DIE4
	SCIENTIST_SEQ_SPAWN,			// matches no state, so the first SetActivity always switches
};

static const char szScientistModel[] = "models/scientist.mdl";

static const char *pDieSounds[] =
{
	"barney/ba_die1.wav",
	"barney/ba_die2.wav",
	"barney/ba_die3.wav",
	"barney/ba_die4.wav",
};

static const char *pPainSounds[] =
{
	"barney/ba_pain1.wav",
};

class CScientist : public CBaseMonster
{
public:
	CScientist();

	void Spawn();
	int Classify();
	void SetActivity(int activity);
	void Pain(float flDamage);
	void Death(int iDeathType);
	void SetDeathActivity(int iDeathType);

	float	m_flFollowDist;
};

LINK_ENTITY_TO_CLASS(monster_scientist, CScientist);

CScientist::CScientist()
{
	m_flFollowDist = 0.0f;
}

//=========================================================
// Spawn
//=========================================================
void CScientist::Spawn()
{
	int i;

	for (i = 0; i < ARRAYSIZE(pDieSounds); i++)
		PRECACHE_SOUND(pDieSounds[i]);

	for (i = 0; i < ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND(pPainSounds[i]);

	PRECACHE_MODEL(szScientistModel);

	SET_MODEL(ENT(pev), szScientistModel);
	UTIL_SetSize(pev, Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 64.0f));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = SCIENTIST_HEALTH;
	pev->yaw_speed = SCIENTIST_YAW_SPEED;
	pev->sequence = SCIENTIST_SEQ_SPAWN;

	m_flDistTooFar = SCIENTIST_CHASE_DIST;
	m_flFollowDist = SCIENTIST_FOLLOW_DIST;
	m_bloodColor = BLOOD_COLOR_RED;

	pev->nextthink += 1.0f;

	if (pev->body == SCIENTIST_BODY_RANDOM)
		pev->body = RANDOM_LONG(0, SCIENTIST_NUM_BODIES - 1);

	SetThink(&CBaseMonster::WalkMonsterStart);
	pev->nextthink = gpGlobals->time + 0.5f;
}

int CScientist::Classify()
{
	return CLASS_PLAYER_ALLY;
}

//=========================================================
// SetActivity
//=========================================================
void CScientist::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
		sequence = SCIENTIST_SEQ_IDLE;
		break;

	case MONSTERSTATE_IDLE2:
		sequence = SCIENTIST_SEQ_IDLE2;
		break;

	case MONSTERSTATE_IDLE3:
		sequence = SCIENTIST_SEQ_IDLE3;
		break;

	case MONSTERSTATE_WALK:
		sequence = SCIENTIST_SEQ_WALK;
		break;

	case MONSTERSTATE_COMBAT_IDLE:
		sequence = SCIENTIST_SEQ_IDLE;
		break;

	case MONSTERSTATE_CHASE:
	case MONSTERSTATE_HUNT:
		sequence = SCIENTIST_SEQ_RUN;
		break;

	case MONSTERSTATE_FOLLOW:
		{
			// stand close to the player, walk to keep up, run when far behind
			Vector vecDelta = pev->origin - m_pMoveTarget->origin;
			float flDist = (float)sqrt(DotProduct(vecDelta, vecDelta));

			if (m_flFollowDist * 2.0f > flDist)
			{
				if (m_flFollowDist < flDist)
					sequence = SCIENTIST_SEQ_WALK;
				else
					sequence = SCIENTIST_SEQ_IDLE;
			}
			else
			{
				sequence = SCIENTIST_SEQ_RUN;
			}
		}
		break;

	case MONSTERSTATE_MELEE_ATTACK:
		sequence = SCIENTIST_SEQ_MELEE_ATTACK;
		break;

	case MONSTERSTATE_DIE1:
	case MONSTERSTATE_DIE4:
		sequence = SCIENTIST_SEQ_DIE1;
		break;

	case MONSTERSTATE_DIE2:
		sequence = SCIENTIST_SEQ_DIE2;
		break;

	case MONSTERSTATE_DIE3:
		sequence = SCIENTIST_SEQ_DIE3;
		break;

	default:
		ALERT(at_error, "Scientist's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == sequence)
		return;

	{
		int oldSequence = pev->sequence;

		// keep the walk/run cycle going when switching between the two
		if ((sequence == SCIENTIST_SEQ_RUN || sequence == SCIENTIST_SEQ_WALK)
			&& (oldSequence == SCIENTIST_SEQ_RUN || oldSequence == SCIENTIST_SEQ_WALK))
			pev->frame += 1.0f;
		else
			pev->frame = 0;
	}

	pev->sequence = sequence;
	ResetSequenceInfo(SCIENTIST_THINK_INTERVAL);

	switch (sequence)
	{
	case SCIENTIST_SEQ_IDLE:
	case SCIENTIST_SEQ_IDLE2:
	case SCIENTIST_SEQ_IDLE3:
	case SCIENTIST_SEQ_WALK:
	case SCIENTIST_SEQ_RUN:
	case SCIENTIST_SEQ_MELEE_ATTACK:
	case SCIENTIST_SEQ_DIE2:
	case SCIENTIST_SEQ_DIE3:
	case SCIENTIST_SEQ_DIE1:
		break;

	default:
		ALERT(at_error, "Bogus scientist anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// Pain
//=========================================================
void CScientist::Pain(float flDamage)
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[0], VOL_NORM, ATTN_NORM);
}

//=========================================================
// SetDeathActivity
//=========================================================
void CScientist::SetDeathActivity(int iDeathType)
{
	switch (iDeathType)
	{
	case DEATH_NORMAL:
		m_MonsterState = MONSTERSTATE_DIE1;
		break;
	case DEATH_VIOLENT:
		m_MonsterState = MONSTERSTATE_DIE2;
		break;
	case DEATH_TYPE3:
		m_MonsterState = MONSTERSTATE_DIE3;
		break;
	case DEATH_TYPE4:
		m_MonsterState = MONSTERSTATE_DIE4;
		break;
	case DEATH_TYPE5:
		m_MonsterState = MONSTERSTATE_DIE5;
		break;
	default:
		ALERT(at_console, "Unknown death type!\n");
		break;
	}

	pev->ideal_yaw = pev->angles.y;
	SetActivity(m_MonsterState);
	SetThink(&CBaseMonster::MonsterThink);
	pev->nextthink = gpGlobals->time + SCIENTIST_THINK_INTERVAL;
}

//=========================================================
// Death
//=========================================================
void CScientist::Death(int iDeathType)
{
	switch (RANDOM_LONG(0, 3))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pDieSounds[0], VOL_NORM, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pDieSounds[1], VOL_NORM, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pDieSounds[2], VOL_NORM, ATTN_NORM);
		break;
	case 3:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pDieSounds[3], VOL_NORM, ATTN_NORM);
		break;
	}

	if (pev->health > GIB_HEALTH)
	{
		SetDeathActivity(DEATH_NORMAL);
		return;
	}

	pev->origin.z += 1.0f;

	if ((int)pev->flags & FL_ONGROUND)
	{
		pev->flags -= FL_ONGROUND;
		pev->velocity = Vector(0.0f, 0.0f, SCIENTIST_DEATH_HOP_SPEED);
	}

	SetDeathActivity(DEATH_VIOLENT);
}
