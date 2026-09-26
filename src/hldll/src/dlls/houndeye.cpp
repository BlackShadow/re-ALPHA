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
// Houndeye - squad monster with a sonic area attack
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define HOUNDEYE_HEALTH				15.0f
#define HOUNDEYE_THINK_INTERVAL		0.1f
#define HOUNDEYE_MAX_ATTACK_DIST	1024.0f
#define HOUNDEYE_SQUAD_RADIUS		512		// AlertSound recruits the houndeyes this close
#define HOUNDEYE_ATTACK_DELAY		9.0f	// between two sonic attacks

#define HOUNDEYE_SONIC_RADIUS		384.0f
#define HOUNDEYE_SONIC_DAMAGE		30		// per squad member, at the blast center
#define HOUNDEYE_SONIC_DAMAGE_MAX	50		// per squad member, only printed
#define HOUNDEYE_SONIC_FALLOFF		0.2f	// damage lost per unit of distance

// houndeye.mdl sequences
enum
{
	HOUND_SEQ_IDLE2 = 0,
	HOUND_SEQ_IDLE,
	HOUND_SEQ_IDLE3,
	HOUND_SEQ_ATTACK,
	HOUND_SEQ_RUN,
	HOUND_SEQ_DIE,
	HOUND_SEQ_SPAWN,			// set by Spawn, until the first think
};

static const char *pAlertSounds[] =
{
	"houndeye/he_alert1.wav",
};

static const char *pDieSounds[] =
{
	"houndeye/he_die1.wav",
	"houndeye/he_die2.wav",
	"houndeye/he_die3.wav",
};

static const char *pIdleSounds[] =
{
	"houndeye/he_idle1.wav",
	"houndeye/he_idle2.wav",
	"houndeye/he_idle3.wav",
	"houndeye/he_idle4.wav",
};

static const char *pAttackSounds[] =
{
	"houndeye/he_attack1.wav",
	"houndeye/he_attack2.wav",
	"houndeye/he_attack3.wav",
};

static const char *pPainSounds[] =
{
	"houndeye/he_pain1.wav",
	"houndeye/he_pain2.wav",
	"houndeye/he_pain4.wav",
	"houndeye/he_pain5.wav",
};

class CHoundeye : public CBaseMonster
{
public:
	CHoundeye();

	void Spawn();
	int Classify();
	void SetActivity(int activity);
	int CheckAttacks(entvars_t *pevEnemy, float flDist);
	void AlertSound();
	void IdleSound();
	void Pain(float flDamage);
	void Death(int iDeathType);

	void SonicAttackThink(CBaseEntity *pOther);
	void SonicFollowThink(CBaseEntity *pOther);

	int		m_fSonicActive;		// a squad was formed, the sonic attack is enabled
};

CHoundeye::CHoundeye()
{
	m_fSonicActive = FALSE;
}

//=========================================================
// Spawn
//=========================================================
void CHoundeye::Spawn()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pAlertSounds); i++)
		PRECACHE_SOUND(pAlertSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); i++)
		PRECACHE_SOUND(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pIdleSounds); i++)
		PRECACHE_SOUND(pIdleSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pAttackSounds); i++)
		PRECACHE_SOUND(pAttackSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND(pPainSounds[i]);

	PRECACHE_MODEL("models/houndeye.mdl");

	SET_MODEL(ENT(pev), "models/houndeye.mdl");
	UTIL_SetSize(pev, Vector(-18, -18, 0), Vector(18, 18, 36));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = HOUNDEYE_HEALTH;
	pev->yaw_speed = 10.0f;
	pev->sequence = HOUND_SEQ_SPAWN;

	m_flDistTooFar = 128.0f;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	m_iSquadSize = 1;

	pev->nextthink += 1.0f;
	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// Classify
//=========================================================
int CHoundeye::Classify()
{
	return CLASS_HOUNDEYE;
}

//=========================================================
// SetActivity - plays the sequence for a monster state
//=========================================================
void CHoundeye::SetActivity(int activity)
{
	int iSequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_COMBAT_IDLE:
	case MONSTERSTATE_COMBAT:
		iSequence = HOUND_SEQ_IDLE;
		break;
	case MONSTERSTATE_IDLE2:
		iSequence = HOUND_SEQ_IDLE2;
		break;
	case MONSTERSTATE_IDLE3:
		iSequence = HOUND_SEQ_IDLE3;
		break;
	case MONSTERSTATE_WALK:
	case MONSTERSTATE_CHASE:
	case MONSTERSTATE_HUNT:
		iSequence = HOUND_SEQ_RUN;
		break;
	case MONSTERSTATE_MELEE_ATTACK:
	case MONSTERSTATE_RANGE_ATTACK:
		iSequence = HOUND_SEQ_ATTACK;
		break;
	case MONSTERSTATE_DIE1:
		iSequence = HOUND_SEQ_DIE;
		break;
	default:
		ALERT(at_console, "Houndeye's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == iSequence)
		return;

	pev->sequence = iSequence;
	pev->frame = 0;
	ResetSequenceInfo(HOUNDEYE_THINK_INTERVAL);

	if (pev->sequence < 0 || pev->sequence > HOUND_SEQ_DIE)
	{
		ALERT(at_console, "Bogus Houndeye anim: %d", pev->sequence);
		m_flFrameRate = 0;
		m_flGroundSpeed = 0;
	}
}

//=========================================================
// CheckAttacks - starts the sonic attack when the enemy
// is in range
//=========================================================
int CHoundeye::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	if (!CheckRangeAttack(pevEnemy) || flDist > HOUNDEYE_MAX_ATTACK_DIST)
		return FALSE;

	m_IdealMonsterState = MONSTERSTATE_COMBAT;
	SetThink(&CHoundeye::SonicAttackThink);
	return TRUE;
}

//=========================================================
// AlertSound - the first houndeye to see the enemy
// recruits a squad and sends it after the enemy
//=========================================================
void CHoundeye::AlertSound()
{
	if (m_iSquadSize > 1)
		return;

	int iRecruits = SquadRecruit(HOUNDEYE_SQUAD_RADIUS);
	if (!iRecruits)
	{
		ALERT(at_console, "No Squad\n");
		m_MonsterState = MONSTERSTATE_COMBAT;
		return;
	}

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pAlertSounds[0], VOL_NORM, ATTN_NORM);

	m_iSquadSize += iRecruits;

	// send the whole squad after the enemy
	entvars_t *pevMember = m_pSquadNext;
	while (pevMember && pevMember != pev)
	{
		CBaseMonster *pMember = (CBaseMonster *)CBaseEntity::Instance(pevMember);
		if (!pMember)
			break;

		pMember->m_MonsterState = MONSTERSTATE_CHASE;
		pMember->m_iSquadSize = m_iSquadSize;

		pevMember = pMember->m_pSquadNext;
	}

	ALERT(at_console, "group of: %d\n", m_iSquadSize);

	m_MonsterState = MONSTERSTATE_CHASE;
	m_fSonicActive = TRUE;
	m_iSquadSize = iRecruits;
}

//=========================================================
// IdleSound
//=========================================================
void CHoundeye::IdleSound()
{
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);
	const char *pszSound;

	if (flRand <= 0.25f)
		pszSound = pIdleSounds[0];
	else if (flRand <= 0.5f)
		pszSound = pIdleSounds[1];
	else if (flRand <= 0.75f)
		pszSound = pIdleSounds[2];
	else
		pszSound = pIdleSounds[3];

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_IDLE);

	m_flNextSoundTime = (float)RANDOM_LONG(0, 3) + gpGlobals->time + 1.0f;
}

//=========================================================
// Pain
//=========================================================
void CHoundeye::Pain(float flDamage)
{
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);
	const char *pszSound;

	if (flRand <= 0.25f)
		pszSound = pPainSounds[0];
	else if (flRand <= 0.5f)
		pszSound = pPainSounds[1];
	else if (flRand <= 0.75f)
		pszSound = pPainSounds[2];
	else
		pszSound = pPainSounds[3];

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_NORM);

	if (m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_WALK)
		AlertSound();
}

//=========================================================
// Death
//=========================================================
void CHoundeye::Death(int iDeathType)
{
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);

	// no death cry when gibbed
	if (pev->health > GIB_HEALTH)
	{
		if (flRand <= 0.33f)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pDieSounds[0], VOL_NORM, ATTN_NORM);

		const char *pszSound;
		if (flRand <= 0.66f)
			pszSound = pDieSounds[1];
		else
			pszSound = pDieSounds[2];

		EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_NORM);
	}

	SetDeathActivity(DEATH_NORMAL);
}

//=========================================================
// SonicAttackThink - the squad plays the attack animation
// together, then the blast hurts everything in range that
// is not a houndeye. The damage grows with the squad size.
//=========================================================
void CHoundeye::SonicAttackThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + HOUNDEYE_THINK_INTERVAL;

	// a houndeye without a squad has no sonic attack
	if (!m_fSonicActive)
	{
		m_flNextAttack = gpGlobals->time + 99999.0f;	// never
		return;
	}

	if (m_MonsterState != MONSTERSTATE_RANGE_ATTACK)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pAttackSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackSounds) - 1)], VOL_NORM, ATTN_NORM);

		// the rest of the squad joins in
		entvars_t *pevMember = m_pSquadNext;
		while (pevMember && pevMember != pev)
		{
			CBaseMonster *pMember = (CBaseMonster *)CBaseEntity::Instance(pevMember);
			if (!pMember)
				break;

			if (pevMember->health > 0)
			{
				CHoundeye *pHoundeye = (CHoundeye *)pMember;
				pHoundeye->SetThink(&CHoundeye::SonicFollowThink);
				pHoundeye->m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}

			pevMember = pMember->m_pSquadNext;
		}

		m_MonsterState = MONSTERSTATE_RANGE_ATTACK;
		SetActivity(MONSTERSTATE_RANGE_ATTACK);
	}

	if (!FNullEnt(pev->enemy))
		UpdateEnemyInfo(VARS(pev->enemy));

	CHANGE_YAW(ENT(pev));

	GetAnimationEventFlags(HOUNDEYE_THINK_INTERVAL);
	AdvanceAnimation(HOUNDEYE_THINK_INTERVAL);

	if (!m_fSequenceFinished)
		return;

	int iSquadSize = (int)m_iSquadSize;
	Vector vecSrc = pev->origin;

	// the engine chains the entities in range through pev->chain, ending at the world
	edict_t *pentHit = FIND_ENTITY_IN_SPHERE(vecSrc, HOUNDEYE_SONIC_RADIUS);
	while (!FNullEnt(pentHit))
	{
		entvars_t *pevHit = VARS(pentHit);

		if (pevHit->takedamage != DAMAGE_NO)
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(pentHit);
			if (!pEntity || pEntity->Classify() != CLASS_HOUNDEYE)
			{
				// aim at the middle of the target
				Vector vecEnd = pevHit->origin;
				vecEnd.z += pevHit->size.z * 0.5f;

				TraceResult tr;
				memset(&tr, 0, sizeof(tr));
				UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, ENT(pev), &tr);

				if (tr.flFraction == 1.0f)
				{
					float flDamage = (float)(HOUNDEYE_SONIC_DAMAGE * iSquadSize) - (pev->origin - pevHit->origin).Length() * HOUNDEYE_SONIC_FALLOFF;
					if (flDamage < 0.0f)
						flDamage = 0.0f;

					ALERT(at_console, "%f/%f\n", flDamage, (float)(HOUNDEYE_SONIC_DAMAGE_MAX * iSquadSize));

					if (pEntity)
						pEntity->TakeDamage(pev, pev, flDamage);
				}
			}
		}

		if (FNullEnt(pevHit->chain))
			break;

		pentHit = ENT(pevHit->chain);
	}

	m_MonsterState = m_IdealMonsterState;
	SetThink(&CBaseMonster::MonsterThink);
	m_flNextAttack = gpGlobals->time + HOUNDEYE_ATTACK_DELAY;
}

//=========================================================
// SonicFollowThink - the other squad members play the
// attack animation while facing the leader
//=========================================================
void CHoundeye::SonicFollowThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + HOUNDEYE_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_RANGE_ATTACK)
	{
		m_MonsterState = MONSTERSTATE_RANGE_ATTACK;
		SetActivity(MONSTERSTATE_RANGE_ATTACK);
	}

	GetAnimationEventFlags(HOUNDEYE_THINK_INTERVAL);
	AdvanceAnimation(HOUNDEYE_THINK_INTERVAL);

	if (!FNullEnt(pev->enemy))
		UpdateEnemyInfo(VARS(pev->enemy));

	// face the squad leader
	if (m_pSquadLeader)
		pev->ideal_yaw = UTIL_VecToYaw(m_pSquadLeader->origin - pev->origin);

	CHANGE_YAW(ENT(pev));

	if (!m_fSequenceFinished)
		return;

	m_MonsterState = m_IdealMonsterState;
	SetThink(&CBaseMonster::MonsterThink);
	m_flNextAttack = gpGlobals->time + HOUNDEYE_ATTACK_DELAY;
}

LINK_ENTITY_TO_CLASS(monster_houndeye, CHoundeye);
