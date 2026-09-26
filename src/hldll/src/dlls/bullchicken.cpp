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
// Bullchicken - spits at its enemies
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define BULLCHICKEN_HEALTH			50.0f
#define BULLCHICKEN_BLOOD_COLOR		22
#define BULLCHICKEN_THINK_INTERVAL	0.1f
#define BULLCHICKEN_MAX_ATTACK_DIST	1024.0f
#define BULLCHICKEN_ATTACK_DELAY	4.0f	// between two spits

// where the spit leaves the mouth, from the origin
#define BULLCHICKEN_MOUTH_DIST		64.0f
#define BULLCHICKEN_MOUTH_HEIGHT	48.0f

#define BULLCHICKEN_SPIT_SPEED		600.0f
#define BULLCHICKEN_SPIT_LIFT		200.0f	// upward speed added to the spit
#define BULLCHICKEN_SPIT_DAMAGE		15.0f

// the spray of goo that goes with the spit
#define BULLCHICKEN_SPRAY_STREAMS	4
#define BULLCHICKEN_SPRAY_SPREAD	0.6f
#define BULLCHICKEN_SPRAY_COLOR		22
#define BULLCHICKEN_SPRAY_SPEED		400		// WRITE_BYTE sends the low byte: 144

// anim events
#define BC_AE_SPIT					1

// bullchik.mdl sequences
enum
{
	BC_SEQ_WALK = 0,			// also the other idles
	BC_SEQ_RUN,
	BC_SEQ_FLINCH,
	BC_SEQ_IDLE,
	BC_SEQ_SPIT,
	BC_SEQ_DIE = 6,
	BC_SEQ_SPAWN,				// set by Spawn, until the first think
};

static const char *pPainSounds[] =
{
	"bullchicken/bc_pain1.wav",
	"bullchicken/bc_pain2.wav",
	"bullchicken/bc_pain3.wav",
	"bullchicken/bc_pain4.wav",
};

static const char *pIdleSounds[] =
{
	"bullchicken/bc_idle1.wav",
	"bullchicken/bc_idle2.wav",
	"bullchicken/bc_idle3.wav",
	"bullchicken/bc_idle4.wav",
	"bullchicken/bc_idle5.wav",
};

static const char *pAttackSounds[] =
{
	"bullchicken/bc_attack1.wav",
	"bullchicken/bc_attack2.wav",
	"bullchicken/bc_attack3.wav",
};

static const char *pDieSounds[] =
{
	"bullchicken/bc_die1.wav",
	"bullchicken/bc_die2.wav",
	"bullchicken/bc_die3.wav",
};

//=========================================================
// Bullchicken's spit projectile
//=========================================================
class CSquidSpit : public CBaseMonster
{
public:
	void Init(entvars_t *pevOwner);
	void Touch(CBaseEntity *pOther);

	void SpitThink(CBaseEntity *pOther);
};

//=========================================================
// Init - launches the spit from pevOwner's mouth
//=========================================================
void CSquidSpit::Init(entvars_t *pevOwner)
{
	if (!pevOwner)
		return;

	UTIL_MakeVectors(pevOwner->angles);

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/spit.mdl");
	UTIL_SetSize(pev, Vector(-6, -6, -6), Vector(6, 6, 6));

	pev->owner = OFFSET(pevOwner);

	Vector vecOrigin = pevOwner->origin;
	vecOrigin.z += BULLCHICKEN_MOUTH_HEIGHT;
	vecOrigin += gpGlobals->v_forward * BULLCHICKEN_MOUTH_DIST;
	pev->origin = vecOrigin;

	pev->velocity = gpGlobals->v_forward * BULLCHICKEN_SPIT_SPEED + gpGlobals->v_up * BULLCHICKEN_SPIT_LIFT;
	pev->angles = UTIL_VecToAngles(pev->velocity);

	SetThink(&CSquidSpit::SpitThink);
	pev->sequence = 0;
	ResetSequenceInfo(BULLCHICKEN_THINK_INTERVAL);
	pev->nextthink = gpGlobals->time + BULLCHICKEN_THINK_INTERVAL;

	SetTouch(&CSquidSpit::Touch);
}

//=========================================================
// SpitThink - animates and points along the flight path
//=========================================================
void CSquidSpit::SpitThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + BULLCHICKEN_THINK_INTERVAL;
	AdvanceAnimation(BULLCHICKEN_THINK_INTERVAL);

	pev->angles = UTIL_VecToAngles(pev->velocity);
}

//=========================================================
// Touch - hurts what it hits, then goes away
//=========================================================
void CSquidSpit::Touch(CBaseEntity *pOther)
{
	// gpGlobals->other is the entity we ran into
	EOFFSET eoffsetOther = gpGlobals->other;
	edict_t *pentOther = ENT(eoffsetOther);
	entvars_t *pevOther = VARS(pentOther);

	// passes through the bullchicken that spat it
	if (eoffsetOther == pev->owner)
		return;

	if (pevOther->takedamage != DAMAGE_NO)
	{
		CBaseEntity *pHit = CBaseEntity::Instance(pentOther);
		if (pHit)
			pHit->TakeDamage(pev, pev, BULLCHICKEN_SPIT_DAMAGE);
	}

	// hide it, remove it on the next think
	pev->modelindex = 0;
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + BULLCHICKEN_THINK_INTERVAL;
}

class CBullchicken : public CBaseMonster
{
public:
	void Spawn();
	int Classify();
	void SetActivity(int activity);
	void AlertSound();
	void IdleSound();
	int CheckAttacks(entvars_t *pevEnemy, float flDist);
	void Pain(float flDamage);
	void Death(int iDeathType);

	void BigFlinchThink(CBaseEntity *pOther);
	void SpitAttackThink(CBaseEntity *pOther);
};

//=========================================================
// Spawn
//=========================================================
void CBullchicken::Spawn()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); i++)
		PRECACHE_SOUND(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pAttackSounds); i++)
		PRECACHE_SOUND(pAttackSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pIdleSounds); i++)
		PRECACHE_SOUND(pIdleSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND(pPainSounds[i]);

	PRECACHE_MODEL("models/bullchik.mdl");
	PRECACHE_MODEL("models/spit.mdl");

	SET_MODEL(ENT(pev), "models/bullchik.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 64));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = BULLCHICKEN_HEALTH;
	pev->yaw_speed = 5.0f;
	pev->sequence = BC_SEQ_SPAWN;

	m_flDistTooFar = 512.0f;
	m_bloodColor = BULLCHICKEN_BLOOD_COLOR;

	pev->nextthink = RANDOM_FLOAT(0.0f, 0.5f) + pev->nextthink + 0.5f;
	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// Classify
//=========================================================
int CBullchicken::Classify()
{
	return CLASS_BULLCHICKEN;
}

//=========================================================
// SetActivity - plays the sequence for a monster state
//=========================================================
void CBullchicken::SetActivity(int activity)
{
	int iSequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_COMBAT_IDLE:
	case MONSTERSTATE_COMBAT:
		iSequence = BC_SEQ_IDLE;
		break;
	case MONSTERSTATE_IDLE2:
	case MONSTERSTATE_IDLE3:
	case MONSTERSTATE_WALK:
	case MONSTERSTATE_MELEE_ATTACK:
		iSequence = BC_SEQ_WALK;
		break;
	case MONSTERSTATE_CHASE:
		iSequence = BC_SEQ_RUN;
		break;
	case MONSTERSTATE_RANGE_ATTACK:
		iSequence = BC_SEQ_SPIT;
		break;
	case MONSTERSTATE_FLINCH:
		iSequence = BC_SEQ_FLINCH;
		break;
	case MONSTERSTATE_DIE1:
		iSequence = BC_SEQ_DIE;
		break;
	default:
		ALERT(at_console, "BullChicken's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == iSequence)
		return;

	pev->sequence = iSequence;
	pev->frame = 0;
	ResetSequenceInfo(BULLCHICKEN_THINK_INTERVAL);

	if (pev->sequence < 0 || pev->sequence > BC_SEQ_DIE)
	{
		ALERT(at_console, "Bogus BullChicken anim: %d", pev->sequence);
		m_flFrameRate = 0;
		m_flGroundSpeed = 0;
	}
}

//=========================================================
// AlertSound - no sound, just gets ready to fight
//=========================================================
void CBullchicken::AlertSound()
{
	m_MonsterState = MONSTERSTATE_COMBAT_IDLE;
}

//=========================================================
// IdleSound
//=========================================================
void CBullchicken::IdleSound()
{
	switch (RANDOM_LONG(0, 4))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pIdleSounds[0], VOL_NORM, ATTN_IDLE);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pIdleSounds[1], VOL_NORM, ATTN_IDLE);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pIdleSounds[2], VOL_NORM, ATTN_IDLE);
		break;
	case 3:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pIdleSounds[3], VOL_NORM, ATTN_IDLE);
		break;
	case 4:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pIdleSounds[4], VOL_NORM, ATTN_IDLE);
		break;
	}

	m_flNextSoundTime = RANDOM_FLOAT(0.0f, 2.0f) + gpGlobals->time + 3.0f;
}

//=========================================================
// Pain - a pain sound, then maybe a flinch
//=========================================================
void CBullchicken::Pain(float flDamage)
{
	pev->nextthink = gpGlobals->time + BULLCHICKEN_THINK_INTERVAL;

	if (m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_WALK)
		AlertSound();

	if (m_MonsterState != MONSTERSTATE_PAIN)
	{
		switch (RANDOM_LONG(0, 3))
		{
		case 0:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[0], VOL_NORM, ATTN_NORM);
			break;
		case 1:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[1], VOL_NORM, ATTN_NORM);
			break;
		case 2:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[2], VOL_NORM, ATTN_NORM);
			break;
		case 3:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[3], VOL_NORM, ATTN_NORM);
			break;
		}

		if (RANDOM_LONG(0, 1))
		{
			m_MonsterState = MONSTERSTATE_COMBAT;
			SetThink(&CBaseMonster::MonsterThink);
			return;
		}

		SetThink(&CBullchicken::BigFlinchThink);
	}

	AdvanceAnimation(BULLCHICKEN_THINK_INTERVAL);

	if (m_fSequenceFinished)
		SetThink(&CBaseMonster::MonsterThink);
}

//=========================================================
// BigFlinchThink - maybe plays the flinch animation, then
// goes back to the normal AI
//=========================================================
void CBullchicken::BigFlinchThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + BULLCHICKEN_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_FLINCH)
	{
		switch (RANDOM_LONG(0, 3))
		{
		case 0:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[0], VOL_NORM, ATTN_NORM);
			break;
		case 1:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[1], VOL_NORM, ATTN_NORM);
			break;
		case 2:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[2], VOL_NORM, ATTN_NORM);
			break;
		case 3:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pPainSounds[3], VOL_NORM, ATTN_NORM);
			break;
		}

		if (RANDOM_LONG(0, 1))
		{
			SetThink(&CBaseMonster::MonsterThink);
			return;
		}

		m_MonsterState = MONSTERSTATE_FLINCH;
		SetActivity(MONSTERSTATE_FLINCH);
	}

	GetAnimationEventFlags(BULLCHICKEN_THINK_INTERVAL);
	AdvanceAnimation(BULLCHICKEN_THINK_INTERVAL);

	if (m_fSequenceFinished)
	{
		m_MonsterState = MONSTERSTATE_COMBAT;
		SetActivity(MONSTERSTATE_COMBAT);
		SetThink(&CBaseMonster::MonsterThink);
	}
}

//=========================================================
// Death
//=========================================================
void CBullchicken::Death(int iDeathType)
{
	switch (RANDOM_LONG(0, 2))
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
	}

	// stop, then play the death animation
	pev->velocity = g_vecZero;

	m_MonsterState = MONSTERSTATE_DIE1;
	pev->ideal_yaw = pev->angles.y;
	SetActivity(m_MonsterState);
	SetThink(&CBaseMonster::MonsterThink);

	pev->nextthink = gpGlobals->time + BULLCHICKEN_THINK_INTERVAL;
}

//=========================================================
// CheckAttacks - spits when the enemy is in range
//=========================================================
int CBullchicken::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	if (!CheckRangeAttack(pevEnemy) || flDist > BULLCHICKEN_MAX_ATTACK_DIST)
		return FALSE;

	m_IdealMonsterState = MONSTERSTATE_COMBAT;
	SetThink(&CBullchicken::SpitAttackThink);
	return TRUE;
}

//=========================================================
// SpitAttackThink - plays the spit animation and launches
// the spit on its anim event
//=========================================================
void CBullchicken::SpitAttackThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + BULLCHICKEN_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_RANGE_ATTACK)
	{
		m_MonsterState = MONSTERSTATE_RANGE_ATTACK;
		SetActivity(MONSTERSTATE_RANGE_ATTACK);
	}

	int iEvents = GetAnimationEventFlags(BULLCHICKEN_THINK_INTERVAL);
	AdvanceAnimation(BULLCHICKEN_THINK_INTERVAL);

	CHANGE_YAW(ENT(pev));

	if (iEvents & (1 << BC_AE_SPIT))
	{
		switch (RANDOM_LONG(0, 2))
		{
		case 0:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pAttackSounds[0], VOL_NORM, ATTN_NORM);
			break;
		case 1:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pAttackSounds[1], VOL_NORM, ATTN_NORM);
			break;
		case 2:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pAttackSounds[2], VOL_NORM, ATTN_NORM);
			break;
		}

		CSquidSpit *pSpit = GetClassPtr((CSquidSpit *)NULL);
		pSpit->Init(pev);

		// spray some goo from the mouth, halfway between forward and up
		UTIL_MakeVectors(pev->angles);

		Vector vecDir = gpGlobals->v_forward * 400.0f + gpGlobals->v_up * 400.0f;
		float flLength = vecDir.Length();
		if (flLength != 0.0f)
			vecDir = vecDir * (1.0f / flLength);

		Vector vecSrc = pev->origin;
		vecSrc.z += BULLCHICKEN_MOUTH_HEIGHT;
		vecSrc += gpGlobals->v_forward * BULLCHICKEN_MOUTH_DIST;

		for (int i = 0; i < BULLCHICKEN_SPRAY_STREAMS; i++)
		{
			WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(MSG_BROADCAST, TE_BLOODSTREAM);
			WRITE_COORD(MSG_BROADCAST, vecSrc.x);
			WRITE_COORD(MSG_BROADCAST, vecSrc.y);
			WRITE_COORD(MSG_BROADCAST, vecSrc.z);
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-BULLCHICKEN_SPRAY_SPREAD, BULLCHICKEN_SPRAY_SPREAD) + vecDir.x);
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-BULLCHICKEN_SPRAY_SPREAD, BULLCHICKEN_SPRAY_SPREAD) + vecDir.y);
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-BULLCHICKEN_SPRAY_SPREAD, BULLCHICKEN_SPRAY_SPREAD) + vecDir.z);
			WRITE_BYTE(MSG_BROADCAST, BULLCHICKEN_SPRAY_COLOR);
			WRITE_BYTE(MSG_BROADCAST, BULLCHICKEN_SPRAY_SPEED);
		}
	}

	if (m_fSequenceFinished)
	{
		m_MonsterState = m_IdealMonsterState;
		SetThink(&CBaseMonster::MonsterThink);
		m_flNextAttack = gpGlobals->time + BULLCHICKEN_ATTACK_DELAY;
	}
}

LINK_ENTITY_TO_CLASS(monster_bullchicken, CBullchicken);
