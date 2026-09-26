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
// Barney - security guard
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"

#define BARNEY_THINK_INTERVAL	0.1f
#define BARNEY_HEALTH			7.0f
#define BARNEY_YAW_SPEED		8.0f
#define BARNEY_CHASE_DIST		384.0f	// chases an enemy that is further away
#define BARNEY_FOLLOW_DIST		128.0f	// distance kept to the player he follows
#define BARNEY_MELEE_DIST		64.0f
#define BARNEY_RANGE_DIST		1024.0f	// also the range of his bullets
#define BARNEY_BULLET_SPREAD	0.05f

// GetAnimationEventFlags() bits
#define BARNEY_AE_SHOOT			(1<<1)

// barney.mdl sequences, named by the monster state that plays them
enum
{
	BARNEY_SEQ_IDLE = 0,
	BARNEY_SEQ_IDLE2,
	BARNEY_SEQ_IDLE3,
	BARNEY_SEQ_WALK,
	BARNEY_SEQ_RUN,					// also MONSTERSTATE_HUNT and MONSTERSTATE_MELEE_ATTACK
	BARNEY_SEQ_COMBAT_IDLE = 9,		// also MONSTERSTATE_COMBAT
	BARNEY_SEQ_SHOOT,				// MONSTERSTATE_RANGE_ATTACK
	BARNEY_SEQ_UNUSED = 12,			// accepted by SetActivity, but no state plays it
	BARNEY_SEQ_DIE3,
	BARNEY_SEQ_DIE1,				// also MONSTERSTATE_DIE4
	BARNEY_SEQ_DIE2,
	BARNEY_SEQ_SPAWN = 17,			// matches no state, so the first SetActivity always switches
};

static const char szBarneyModel[] = "models/barney.mdl";

static const char *pAttackSounds[] =
{
	"barney/ba_attack1.wav",
	"barney/ba_attack2.wav",
};

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

class CBarney : public CBaseMonster
{
public:
	CBarney();

	void Spawn();
	int Classify();
	void SetActivity(int activity);
	int CheckAttacks(entvars_t *pevEnemy, float flDist);
	void AlertSound();
	void Pain(float flDamage);
	void Death(int iDeathType);

	void ShootThink(CBaseEntity *pOther);

	float	m_flFollowDist;
};

LINK_ENTITY_TO_CLASS(monster_barney, CBarney);

CBarney::CBarney()
{
	m_flFollowDist = 0.0f;
}

//=========================================================
// Spawn
//=========================================================
void CBarney::Spawn()
{
	int i;

	for (i = 0; i < ARRAYSIZE(pAttackSounds); i++)
		PRECACHE_SOUND(pAttackSounds[i]);

	for (i = 0; i < ARRAYSIZE(pDieSounds); i++)
		PRECACHE_SOUND(pDieSounds[i]);

	for (i = 0; i < ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND(pPainSounds[i]);

	PRECACHE_MODEL(szBarneyModel);

	SET_MODEL(ENT(pev), szBarneyModel);
	UTIL_SetSize(pev, Vector(-18.0f, -18.0f, 0.0f), Vector(18.0f, 18.0f, 72.0f));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = BARNEY_HEALTH;
	pev->yaw_speed = BARNEY_YAW_SPEED;
	pev->sequence = BARNEY_SEQ_SPAWN;

	m_iSquadSize = 1;
	m_bloodColor = BLOOD_COLOR_RED;
	m_flDistTooFar = BARNEY_CHASE_DIST;
	m_flFollowDist = BARNEY_FOLLOW_DIST;

	pev->nextthink = RANDOM_FLOAT(0.0f, 0.5f) + pev->nextthink + 0.5f;

	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// SetActivity
//=========================================================
void CBarney::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
		sequence = BARNEY_SEQ_IDLE;
		break;

	case MONSTERSTATE_IDLE2:
		sequence = BARNEY_SEQ_IDLE2;
		break;

	case MONSTERSTATE_IDLE3:
		sequence = BARNEY_SEQ_IDLE3;
		break;

	case MONSTERSTATE_WALK:
		sequence = BARNEY_SEQ_WALK;
		break;

	case MONSTERSTATE_COMBAT_IDLE:
	case MONSTERSTATE_COMBAT:
		sequence = BARNEY_SEQ_COMBAT_IDLE;
		break;

	case MONSTERSTATE_CHASE:
	case MONSTERSTATE_HUNT:
	case MONSTERSTATE_MELEE_ATTACK:
		sequence = BARNEY_SEQ_RUN;
		break;

	case MONSTERSTATE_FOLLOW:
		{
			// stand close to the player, walk to keep up, run when far behind
			Vector vecDelta = pev->origin - m_pMoveTarget->origin;
			float flDist = (float)sqrt(DotProduct(vecDelta, vecDelta));

			if (m_flFollowDist * 2.0f >= flDist)
			{
				if (m_flFollowDist < flDist)
					sequence = BARNEY_SEQ_WALK;
				else
					sequence = BARNEY_SEQ_IDLE;
			}
			else
			{
				sequence = BARNEY_SEQ_RUN;
			}
		}
		break;

	case MONSTERSTATE_RANGE_ATTACK:
		sequence = BARNEY_SEQ_SHOOT;
		break;

	case MONSTERSTATE_DIE1:
	case MONSTERSTATE_DIE4:
		sequence = BARNEY_SEQ_DIE1;
		break;

	case MONSTERSTATE_DIE2:
		sequence = BARNEY_SEQ_DIE2;
		break;

	case MONSTERSTATE_DIE3:
		sequence = BARNEY_SEQ_DIE3;
		break;

	default:
		ALERT(at_console, "Barney's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == sequence)
		return;

	pev->sequence = sequence;

	// walking and running blend into each other
	if (sequence != BARNEY_SEQ_RUN && sequence != BARNEY_SEQ_WALK)
		pev->frame = 0;

	ResetSequenceInfo(BARNEY_THINK_INTERVAL);

	switch (sequence)
	{
	case BARNEY_SEQ_IDLE:
	case BARNEY_SEQ_IDLE2:
	case BARNEY_SEQ_IDLE3:
	case BARNEY_SEQ_WALK:
	case BARNEY_SEQ_RUN:
	case BARNEY_SEQ_COMBAT_IDLE:
	case BARNEY_SEQ_SHOOT:
	case BARNEY_SEQ_UNUSED:
	case BARNEY_SEQ_DIE3:
	case BARNEY_SEQ_DIE1:
	case BARNEY_SEQ_DIE2:
		break;

	default:
		ALERT(at_console, "Bogus Barney anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

int CBarney::Classify()
{
	return CLASS_PLAYER_ALLY;
}

//=========================================================
// CheckAttacks - he shoots at close range too
//=========================================================
int CBarney::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	if (flDist <= BARNEY_MELEE_DIST && CheckMeleeAttack(pevEnemy))
	{
		m_IdealMonsterState = MONSTERSTATE_COMBAT;
		SetThink(&CBarney::ShootThink);
		return TRUE;
	}

	if (CheckRangeAttack(pevEnemy) && flDist <= BARNEY_RANGE_DIST)
	{
		m_IdealMonsterState = MONSTERSTATE_COMBAT;
		SetThink(&CBarney::ShootThink);
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// AlertSound
//=========================================================
void CBarney::AlertSound()
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, pAttackSounds[0], VOL_NORM, ATTN_NORM);

	m_MonsterState = MONSTERSTATE_COMBAT_IDLE;
	m_flNextAttack = gpGlobals->time + 1.0f;
}

//=========================================================
// Pain
//=========================================================
void CBarney::Pain(float flDamage)
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[0], VOL_NORM, ATTN_NORM);

	AlertSound();
}

//=========================================================
// Death
//=========================================================
void CBarney::Death(int iDeathType)
{
	if (pev->health <= GIB_HEALTH)
	{
		SetDeathActivity(DEATH_VIOLENT);
		return;
	}

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

	SetDeathActivity(DEATH_NORMAL);
}

//=========================================================
// ShootThink - fires one shot per shoot event of the animation
//=========================================================
void CBarney::ShootThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + BARNEY_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_RANGE_ATTACK)
	{
		m_MonsterState = MONSTERSTATE_RANGE_ATTACK;
		SetActivity(MONSTERSTATE_RANGE_ATTACK);
		m_flNextAttack = RANDOM_FLOAT(0.5f, 1.5f) + gpGlobals->time;
	}

	int iEvents = GetAnimationEventFlags(BARNEY_THINK_INTERVAL);
	AdvanceAnimation(BARNEY_THINK_INTERVAL);

	CHANGE_YAW(ENT(pev));

	if (iEvents & BARNEY_AE_SHOOT)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pAttackSounds[1], VOL_NORM, ATTN_NORM);

		UTIL_MakeVectors(pev->angles);

		// FireBullets makes its own vectors, so pass a copy
		Vector vecDir = gpGlobals->v_forward;

		FireBullets(1, vecDir, BARNEY_BULLET_SPREAD, BARNEY_BULLET_SPREAD, BULLET_NONE, BARNEY_RANGE_DIST);

		pev->effects = (int)pev->effects | EF_MUZZLEFLASH;
	}

	if (m_fSequenceFinished)
	{
		SetThink(&CBaseMonster::MonsterThink);
		m_MonsterState = m_IdealMonsterState;
	}
}
