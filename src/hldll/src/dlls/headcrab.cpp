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
// Headcrab - leaps at the enemy, the bite is lethal
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define HEADCRAB_HEALTH				12.0f
#define HEADCRAB_BLOOD_COLOR		54
#define HEADCRAB_THINK_INTERVAL		0.1f
#define HEADCRAB_MAX_ATTACK_DIST	1024.0f
#define HEADCRAB_LEAP_SPEED			250.0f
#define HEADCRAB_ATTACK_DELAY		4.0f	// between two leaps
#define HEADCRAB_PAIN_DAMAGE		5.0f	// lighter hits interrupt the current action
#define HEADCRAB_BITE_STREAMS		4		// blood streams sprayed by a bite

// headcrab.mdl sequences
enum
{
	HC_SEQ_IDLE = 0,
	HC_SEQ_WALK,
	HC_SEQ_RUN,
	HC_SEQ_LEAP = 4,
	HC_SEQ_PAIN,
	HC_SEQ_DIE,
	HC_SEQ_LAST,				// highest sequence SetActivity accepts
	HC_SEQ_SPAWN,				// set by Spawn, until the first think
};

static const char *pAttackSounds[] =
{
	"headcrab/hc_attack1.wav",
};

static const char *pAlertSounds[] =
{
	"headcrab/hc_alert1.wav",
};

static const char *pDieSounds[] =
{
	"headcrab/hc_die1.wav",
	"headcrab/hc_die2.wav",
};

static const char *pPainSounds[] =
{
	"headcrab/hc_pain1.wav",
	"headcrab/hc_pain2.wav",
	"headcrab/hc_pain3.wav",
};

static const char *pIdleSounds[] =
{
	"headcrab/hc_idle1.wav",
	"headcrab/hc_idle2.wav",
	"headcrab/hc_idle3.wav",
};

static const char *pBiteSounds[] =
{
	"headcrab/hc_headbite.wav",
};

class CHCHeadcrab : public CBaseMonster
{
public:
	CHCHeadcrab();

	void Spawn();
	int Classify();
	void SetActivity(int activity);
	void Pain(float flDamage);
	void Death(int iDeathType);
	void AlertSound();
	void IdleSound();
	int CheckAttacks(entvars_t *pevEnemy, float flDist);

	void LeapAttackThink(CBaseEntity *pOther);
	void LeapAttackTouch(CBaseEntity *pOther);
};

CHCHeadcrab::CHCHeadcrab()
{
}

//=========================================================
// Spawn
//=========================================================
void CHCHeadcrab::Spawn()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pAttackSounds); i++)
		PRECACHE_SOUND(pAttackSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pAlertSounds); i++)
		PRECACHE_SOUND(pAlertSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); i++)
		PRECACHE_SOUND(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND(pPainSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pIdleSounds); i++)
		PRECACHE_SOUND(pIdleSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pBiteSounds); i++)
		PRECACHE_SOUND(pBiteSounds[i]);

	PRECACHE_MODEL("models/headcrab.mdl");

	SET_MODEL(ENT(pev), "models/headcrab.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = HEADCRAB_HEALTH;
	pev->yaw_speed = 10.0f;
	pev->sequence = HC_SEQ_SPAWN;

	m_bloodColor = HEADCRAB_BLOOD_COLOR;
	m_flDistTooFar = 256.0f;

	pev->nextthink = pev->nextthink + RANDOM_FLOAT(0.0f, 0.5f) + 0.5f;
	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// Classify
//=========================================================
int CHCHeadcrab::Classify()
{
	return CLASS_HEADCRAB;
}

//=========================================================
// SetActivity - plays the sequence for a monster state
//=========================================================
void CHCHeadcrab::SetActivity(int activity)
{
	int iSequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_IDLE2:
	case MONSTERSTATE_IDLE3:
	case MONSTERSTATE_COMBAT_IDLE:
	case MONSTERSTATE_COMBAT:
		iSequence = HC_SEQ_IDLE;
		break;

	case MONSTERSTATE_WALK:
		iSequence = HC_SEQ_WALK;
		break;

	case MONSTERSTATE_CHASE:
	case MONSTERSTATE_HUNT:
		iSequence = HC_SEQ_RUN;
		break;

	case MONSTERSTATE_PAIN:
	case MONSTERSTATE_HEAVY_PAIN:
		iSequence = HC_SEQ_PAIN;
		break;

	case MONSTERSTATE_RANGE_ATTACK:
		iSequence = HC_SEQ_LEAP;
		break;

	case MONSTERSTATE_ATTACK:
		return;

	case MONSTERSTATE_DIE1:
		iSequence = HC_SEQ_DIE;
		break;

	default:
		ALERT(at_console, "Headcrab's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == iSequence)
		return;

	pev->sequence = iSequence;
	pev->frame = 0;
	ResetSequenceInfo(HEADCRAB_THINK_INTERVAL);

	if (pev->sequence < 0 || pev->sequence > HC_SEQ_LAST)
	{
		ALERT(at_console, "Bogus headcrab anim: %d", pev->sequence);
		m_flFrameRate = 0;
		m_flGroundSpeed = 0;
	}
}

//=========================================================
// Pain
//=========================================================
void CHCHeadcrab::Pain(float flDamage)
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[RANDOM_LONG(0, ARRAYSIZE(pPainSounds) - 1)], VOL_NORM, ATTN_NORM);

	if (flDamage < HEADCRAB_PAIN_DAMAGE)
	{
		SetThink(&CBaseMonster::MonsterThink);
		if (!FNullEnt(pev->enemy))
			m_MonsterState = MONSTERSTATE_COMBAT_IDLE;
	}
}

//=========================================================
// Death
//=========================================================
void CHCHeadcrab::Death(int iDeathType)
{
	// no death cry when gibbed
	if (pev->health > GIB_HEALTH)
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pDieSounds[RANDOM_LONG(0, ARRAYSIZE(pDieSounds) - 1)], VOL_NORM, ATTN_NORM);

	SetDeathActivity(DEATH_NORMAL);
}

//=========================================================
// AlertSound
//=========================================================
void CHCHeadcrab::AlertSound()
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, pAlertSounds[0], VOL_NORM, ATTN_NORM);

	// leap after a second
	m_MonsterState = MONSTERSTATE_ATTACK;
	m_flNextAttack = gpGlobals->time + 1.0f;
}

//=========================================================
// IdleSound
//=========================================================
void CHCHeadcrab::IdleSound()
{
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);
	const char *pszSound;

	if (flRand <= 0.33f)
		pszSound = pIdleSounds[0];
	else if (flRand <= 0.66f)
		pszSound = pIdleSounds[1];
	else
		pszSound = pIdleSounds[2];

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_IDLE);

	m_flNextSoundTime = gpGlobals->time + RANDOM_FLOAT(0.0f, 2.0f) + 3.0f;
}

//=========================================================
// CheckAttacks - leaps when the enemy is in range
//=========================================================
int CHCHeadcrab::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	if (!pevEnemy)
		return FALSE;

	if (!CheckRangeAttack(pevEnemy) || flDist > HEADCRAB_MAX_ATTACK_DIST)
		return FALSE;

	m_IdealMonsterState = MONSTERSTATE_COMBAT;
	SetThink(&CHCHeadcrab::LeapAttackThink);
	return TRUE;
}

//=========================================================
// LeapAttackTouch - bites whatever the leap hits
//=========================================================
void CHCHeadcrab::LeapAttackTouch(CBaseEntity *pOther)
{
	// gpGlobals->other is the entity we ran into
	if (FNullEnt(gpGlobals->other))
		return;

	edict_t *pentOther = ENT(gpGlobals->other);
	entvars_t *pevOther = VARS(pentOther);
	if (pevOther->takedamage == DAMAGE_NO)
		return;

	CBaseEntity *pHit = CBaseEntity::Instance(pentOther);
	if (!pHit)
		return;

	if (pHit->Classify() == Classify())
		return;

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pBiteSounds[0], VOL_NORM, ATTN_NORM);

	// the bite takes all the victim's health
	pHit->TakeDamage(pev, pev, pevOther->health);

	const Vector &vecOrigin = pev->origin;

	for (int i = 0; i < HEADCRAB_BITE_STREAMS; i++)
	{
		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_BLOODSTREAM);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.x);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.y);
		WRITE_COORD(MSG_BROADCAST, vecOrigin.z);
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));	// random upward direction
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
		WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(0.0f, 1.0f));
		WRITE_BYTE(MSG_BROADCAST, pHit->BloodColor());
		WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(80, 150));		// speed
	}
}

//=========================================================
// LeapAttackThink - jumps at the enemy and flies until it
// lands, biting on touch
//=========================================================
void CHCHeadcrab::LeapAttackThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + HEADCRAB_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_RANGE_ATTACK)
	{
		m_MonsterState = MONSTERSTATE_RANGE_ATTACK;
		SetActivity(MONSTERSTATE_RANGE_ATTACK);

		EMIT_SOUND(ENT(pev), CHAN_WEAPON, pAttackSounds[0], VOL_NORM, ATTN_NORM);

		UTIL_MakeVectors(pev->angles);

		// take off
		pev->flags -= FL_ONGROUND;

		Vector vecOrigin = pev->origin;
		vecOrigin.z += 1.0f;
		UTIL_SetOrigin(pev, vecOrigin);

		// jump up and towards the enemy
		Vector vecJumpDir = g_vecZero;
		if (!FNullEnt(pev->enemy))
		{
			vecJumpDir = VARS(pev->enemy)->origin - pev->origin;

			float flLength = vecJumpDir.Length();
			if (flLength != 0.0f)
				vecJumpDir = vecJumpDir * (1.0f / flLength);
		}

		pev->velocity = (gpGlobals->v_up + vecJumpDir) * HEADCRAB_LEAP_SPEED;

		SetTouch(&CHCHeadcrab::LeapAttackTouch);
	}

	AdvanceAnimation(HEADCRAB_THINK_INTERVAL);

	if (!FNullEnt(pev->enemy))
		UpdateEnemyInfo(VARS(pev->enemy));

	// landed
	if (((int)pev->flags & FL_ONGROUND) != 0)
	{
		SetTouch(&CBaseEntity::SUB_DoNothing);
		SetThink(&CBaseMonster::MonsterThink);
		m_flNextAttack = gpGlobals->time + HEADCRAB_ATTACK_DELAY;
		m_MonsterState = m_IdealMonsterState;
	}
}

LINK_ENTITY_TO_CLASS(monster_headcrab, CHCHeadcrab);
