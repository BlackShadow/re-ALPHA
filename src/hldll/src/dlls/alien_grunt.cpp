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
// Alien grunt - dominant, warlike alien that fires homing hornets
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

#define AGRUNT_THINK_INTERVAL		0.1f
#define AGRUNT_MIN_HEALTH			70.0f
#define AGRUNT_EXTRA_HEALTH			30.0f	// random extra health, up to this much
#define AGRUNT_YAW_SPEED			8.0f
#define AGRUNT_CHASE_DIST			512.0f	// chases an enemy that is further away
#define AGRUNT_MELEE_DIST			64.0f
#define AGRUNT_RANGE_DIST			1024.0f
#define AGRUNT_MIN_HORNETS			4
#define AGRUNT_MAX_HORNETS			6

#define HORNET_HEALTH				1.0f
#define HORNET_DAMAGE				10.0f
#define HORNET_LAUNCH_SPEED			150.0f
#define HORNET_SPEED				300.0f	// flying straight at the enemy
#define HORNET_HOMING_DELAY			0.6f	// flies straight this long before homing in
#define HORNET_BUZZ_VOLUME			0.6f
#define HORNET_BLOOD_COLOR			54

// GetAnimationEventFlags() bits: any of the event types 1 to 4 launches a hornet
#define AGRUNT_AE_HORNET			((1<<1) | (1<<2) | (1<<3) | (1<<4))

// agrunt.mdl sequences, named by what SetActivity uses them for
enum
{
	AGRUNT_SEQ_IDLE = 0,			// also MONSTERSTATE_IDLE2 and MONSTERSTATE_COMBAT
	AGRUNT_SEQ_IDLE3,
	AGRUNT_SEQ_UNUSED1,				// accepted by SetActivity, but no state plays it
	AGRUNT_SEQ_WALK,
	AGRUNT_SEQ_RUN,					// MONSTERSTATE_CHASE
	AGRUNT_SEQ_UNUSED2 = 7,			// accepted by SetActivity, but no state plays it
	AGRUNT_SEQ_MELEE_ATTACK = 10,
	AGRUNT_SEQ_DIE1 = 12,
	AGRUNT_SEQ_DIE3,
	AGRUNT_SEQ_RANGE_ATTACK,		// fires the hornets
	AGRUNT_SEQ_SPAWN,				// matches no state, so the first SetActivity always switches
};

static const char szAGruntModel[] = "models/agrunt.mdl";
static const char szHornetModel[] = "models/hornet.mdl";

static const char *pHornetBuzzSounds[] =
{
	"hornet/ag_buzz1.wav",
	"hornet/ag_buzz2.wav",
	"hornet/ag_buzz3.wav",
};

static const char *pAlertSounds[] =
{
	"agrunt/ag_alert1.wav",
	"agrunt/ag_alert2.wav",
	"agrunt/ag_alert3.wav",
};

static const char *pFireSounds[] =
{
	"agrunt/ag_fire1.wav",
	"agrunt/ag_fire2.wav",
	"agrunt/ag_fire3.wav",
};

static const char *pDieSounds[] =
{
	"agrunt/ag_die1.wav",
	"agrunt/ag_die2.wav",
	"agrunt/ag_die3.wav",
};

static const char *pIdleSounds[] =
{
	"agrunt/ag_idle1.wav",
	"agrunt/ag_idle2.wav",
	"agrunt/ag_idle3.wav",
	"agrunt/ag_idle4.wav",
	"agrunt/ag_idle5.wav",
	"agrunt/ag_idle6.wav",
	"agrunt/ag_idle7.wav",
};

static const char *pPainSounds[] =
{
	"agrunt/ag_pain1.wav",
	"agrunt/ag_pain2.wav",
	"agrunt/ag_pain3.wav",
	"agrunt/ag_pain4.wav",
	"agrunt/ag_pain5.wav",
};

//=========================================================
// NormalizeVector - scales vec to unit length. Returns FALSE,
// leaving vec alone, when it has no length.
//=========================================================
static BOOL NormalizeVector(Vector &vec)
{
	float flLength = vec.Length();
	if (flLength <= 0.0f)
		return FALSE;

	vec = vec * (1.0f / flLength);
	return TRUE;
}

//=========================================================
// Hornet - homing projectile of the alien grunt
//=========================================================
class CHornet : public CBaseMonster
{
public:
	void Init(entvars_t *pevOwner);
	void Touch(CBaseEntity *pOther);
	void HornetThink(CBaseEntity *pOther);
	void Death(int iDeathType);

	unsigned char	m_bBloodColor;		// set on launch, never read
	int				m_iUnused;			// cleared on launch, never read
	float			m_flLaunchTime;		// never read
};

//=========================================================
// Init - launches the hornet from pevOwner at its enemy
//=========================================================
void CHornet::Init(entvars_t *pevOwner)
{
	UTIL_MakeVectors(pevOwner->angles);

	pev->movetype = MOVETYPE_FLYMISSILE;
	pev->solid = SOLID_BBOX;
	pev->health = HORNET_HEALTH;
	pev->takedamage = DAMAGE_AIM;

	SET_MODEL(ENT(pev), szHornetModel);
	UTIL_SetSize(pev, Vector(-6.0f, -6.0f, -6.0f), Vector(6.0f, 6.0f, 6.0f));

	pev->owner = OFFSET(pevOwner);

	// start in front of the owner, off to its right
	pev->origin = Vector(pevOwner->origin.x, pevOwner->origin.y, pevOwner->origin.z + 48.0f)
		+ gpGlobals->v_forward * 64.0f + gpGlobals->v_right * 24.0f;

	float flSpread = RANDOM_FLOAT(-0.5f, 0.5f);
	pev->velocity = (gpGlobals->v_right * flSpread + gpGlobals->v_forward) * HORNET_LAUNCH_SPEED;
	pev->angles = UTIL_VecToAngles(pev->velocity);

	pev->enemy = pevOwner->enemy;

	m_bBloodColor = HORNET_BLOOD_COLOR;
	m_iUnused = 0;
	m_flLaunchTime = gpGlobals->time;

	SetTouch(&CHornet::Touch);
	SetThink(&CHornet::HornetThink);

	{
		const char *pszSample;
		int iSound = RANDOM_LONG(0, 2);
		if (iSound == 1)
			pszSample = pFireSounds[1];
		else if (iSound == 2)
			pszSample = pFireSounds[2];
		else
			pszSample = pFireSounds[0];

		EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSample, VOL_NORM, ATTN_NORM);
	}

	pev->nextthink = gpGlobals->time + HORNET_HOMING_DELAY;
}

//=========================================================
// Death
//=========================================================
void CHornet::Death(int iDeathType)
{
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// Touch - stings whatever it hits, then disappears
//=========================================================
void CHornet::Touch(CBaseEntity *pOther)
{
	EOFFSET eoffsetOther = gpGlobals->other;
	edict_t *pentOther = ENT(eoffsetOther);
	entvars_t *pevOther = VARS(pentOther);

	// don't sting the agrunt that fired us
	if (eoffsetOther == pev->owner)
		return;

	// don't sting another hornet
	if (pevOther->modelindex == pev->modelindex)
		return;

	if (pevOther->takedamage != DAMAGE_NO)
	{
		const char *pszSample;
		int iSound = RANDOM_LONG(0, 2);
		if (iSound == 1)
			pszSample = pHornetBuzzSounds[1];
		else if (iSound == 2)
			pszSample = pHornetBuzzSounds[2];
		else
			pszSample = pHornetBuzzSounds[0];

		EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSample, HORNET_BUZZ_VOLUME, ATTN_NORM);

		CBaseEntity *pHit = CBaseEntity::Instance(pentOther);
		if (pHit)
			pHit->TakeDamage(pev, pev, HORNET_DAMAGE);
	}

	pev->modelindex = 0;
	pev->solid = SOLID_NOT;

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + AGRUNT_THINK_INTERVAL;
}

//=========================================================
// HornetThink - steers towards the enemy
//=========================================================
void CHornet::HornetThink(CBaseEntity *pOther)
{
	entvars_t *pevEnemy = VARS(pev->enemy);

	// the enemy is dead, go away soon
	if (pevEnemy->health <= 0.0f)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + AGRUNT_THINK_INTERVAL;
	}

	Vector vecDirToEnemy = pevEnemy->origin - pev->origin;
	NormalizeVector(vecDirToEnemy);

	Vector vecFlightDir = pev->velocity;
	NormalizeVector(vecFlightDir);

	// buzz when turning by more than 60 degrees
	float flDot = DotProduct(vecDirToEnemy, vecFlightDir);
	if (flDot < 0.5f)
	{
		const char *pszSample;
		int iSound = RANDOM_LONG(0, 2);
		if (iSound == 1)
			pszSample = pHornetBuzzSounds[1];
		else if (iSound == 2)
			pszSample = pHornetBuzzSounds[2];
		else
			pszSample = pHornetBuzzSounds[0];

		EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSample, HORNET_BUZZ_VOLUME, ATTN_NORM);
	}

	// slow down in turns, to a quarter of the speed when the enemy is behind
	if (flDot <= 0.0f)
		flDot = 0.25f;

	Vector vecNewDir = vecDirToEnemy + vecFlightDir;
	if (!NormalizeVector(vecNewDir))
		vecNewDir = g_vecZero;

	pev->velocity = vecNewDir;
	pev->velocity.x += RANDOM_FLOAT(-0.1f, 0.1f);
	pev->velocity.y += RANDOM_FLOAT(-0.1f, 0.1f);
	pev->velocity.z += RANDOM_FLOAT(-0.1f, 0.1f);

	pev->velocity = pev->velocity * (flDot * HORNET_SPEED);
	pev->angles = UTIL_VecToAngles(pev->velocity);

	AdvanceAnimation(AGRUNT_THINK_INTERVAL);
	pev->nextthink = gpGlobals->time + AGRUNT_THINK_INTERVAL;
}

//=========================================================
// Alien grunt
//=========================================================
class CAGrunt : public CBaseMonster
{
public:
	void Spawn();
	int Classify();
	void SetActivity(int activity);
	void IdleSound();
	void AlertSound();
	int CheckAttacks(entvars_t *pevEnemy, float flDist);
	void Pain(float flDamage);
	void Death(int iDeathType);

	void MeleeAttack(CBaseEntity *pOther);
	void HornetAttack(CBaseEntity *pOther);

	CHornet *CreateHornet();

	int		m_iHornetCount;			// set for each volley, never read
};

//=========================================================
// Spawn
//=========================================================
void CAGrunt::Spawn()
{
	int i;

	PRECACHE_MODEL(szAGruntModel);
	PRECACHE_MODEL(szHornetModel);

	for (i = 0; i < ARRAYSIZE(pHornetBuzzSounds); i++)
		PRECACHE_SOUND(pHornetBuzzSounds[i]);

	for (i = 0; i < ARRAYSIZE(pAlertSounds); i++)
		PRECACHE_SOUND(pAlertSounds[i]);

	for (i = 0; i < ARRAYSIZE(pFireSounds); i++)
		PRECACHE_SOUND(pFireSounds[i]);

	for (i = 0; i < ARRAYSIZE(pDieSounds); i++)
		PRECACHE_SOUND(pDieSounds[i]);

	for (i = 0; i < ARRAYSIZE(pIdleSounds); i++)
		PRECACHE_SOUND(pIdleSounds[i]);

	for (i = 0; i < ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND(pPainSounds[i]);

	SET_MODEL(ENT(pev), szAGruntModel);
	UTIL_SetSize(pev, Vector(-32.0f, -32.0f, 0.0f), Vector(32.0f, 32.0f, 64.0f));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = RANDOM_FLOAT(0.0f, AGRUNT_EXTRA_HEALTH) + AGRUNT_MIN_HEALTH;
	pev->yaw_speed = AGRUNT_YAW_SPEED;
	pev->sequence = AGRUNT_SEQ_SPAWN;

	m_flDistTooFar = AGRUNT_CHASE_DIST;
	m_bloodColor = (signed char)BLOOD_COLOR_YELLOW;	// sign-extended, only its low byte is used

	SetThink(&CBaseMonster::WalkMonsterStart);
	pev->nextthink = RANDOM_FLOAT(0.0f, 0.5f) + pev->nextthink + 0.5f;
}

int CAGrunt::Classify()
{
	return CLASS_ALIEN_MILITARY;
}

//=========================================================
// SetActivity
//=========================================================
void CAGrunt::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_IDLE2:
	case MONSTERSTATE_COMBAT:
		sequence = AGRUNT_SEQ_IDLE;
		break;
	case MONSTERSTATE_IDLE3:
		sequence = AGRUNT_SEQ_IDLE3;
		break;
	case MONSTERSTATE_WALK:
		sequence = AGRUNT_SEQ_WALK;
		break;
	case MONSTERSTATE_CHASE:
		sequence = AGRUNT_SEQ_RUN;
		break;
	case MONSTERSTATE_MELEE_ATTACK:
		sequence = AGRUNT_SEQ_MELEE_ATTACK;
		break;
	case MONSTERSTATE_RANGE_ATTACK:
		sequence = AGRUNT_SEQ_RANGE_ATTACK;
		break;
	case MONSTERSTATE_DIE1:
		sequence = AGRUNT_SEQ_DIE1;
		break;
	case MONSTERSTATE_DIE3:
		sequence = AGRUNT_SEQ_DIE3;
		break;
	default:
		ALERT(at_console, "AGrunt's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == sequence)
		return;

	pev->sequence = sequence;
	pev->frame = 0;
	ResetSequenceInfo(AGRUNT_THINK_INTERVAL);

	switch (sequence)
	{
	case AGRUNT_SEQ_IDLE:
	case AGRUNT_SEQ_IDLE3:
	case AGRUNT_SEQ_UNUSED1:
	case AGRUNT_SEQ_WALK:
	case AGRUNT_SEQ_RUN:
	case AGRUNT_SEQ_UNUSED2:
	case AGRUNT_SEQ_MELEE_ATTACK:
	case AGRUNT_SEQ_DIE1:
	case AGRUNT_SEQ_DIE3:
	case AGRUNT_SEQ_RANGE_ATTACK:
		break;

	default:
		ALERT(at_console, "Bogus AGrunt anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// IdleSound
//=========================================================
void CAGrunt::IdleSound()
{
	const char *pszSample;
	switch (RANDOM_LONG(0, 6))
	{
	case 1:
		pszSample = pIdleSounds[1];
		break;
	case 2:
		pszSample = pIdleSounds[2];
		break;
	case 3:
		pszSample = pIdleSounds[3];
		break;
	case 4:
		pszSample = pIdleSounds[4];
		break;
	case 5:
		pszSample = pIdleSounds[5];
		break;
	case 6:
		pszSample = pIdleSounds[6];
		break;
	default:
		pszSample = pIdleSounds[0];
		break;
	}

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSample, VOL_NORM, ATTN_IDLE);

	if (RANDOM_LONG(0, 6) >= 4)
		m_flNextSoundTime = gpGlobals->time + 5.0f;
	else
		m_flNextSoundTime = gpGlobals->time + 1.0f;
}

//=========================================================
// AlertSound
//=========================================================
void CAGrunt::AlertSound()
{
	m_MonsterState = MONSTERSTATE_CHASE;

	const char *pszSample;
	int iSound = RANDOM_LONG(0, 2);
	if (iSound == 1)
		pszSample = pAlertSounds[1];
	else if (iSound == 2)
		pszSample = pAlertSounds[2];
	else
		pszSample = pAlertSounds[0];

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSample, VOL_NORM, ATTN_NORM);

	m_flNextAttack = gpGlobals->time + 1.0f;
}

//=========================================================
// Pain
//=========================================================
void CAGrunt::Pain(float flDamage)
{
	const char *pszSample;
	switch (RANDOM_LONG(0, 4))
	{
	case 1:
		pszSample = pPainSounds[1];
		break;
	case 2:
		pszSample = pPainSounds[2];
		break;
	case 3:
		pszSample = pPainSounds[3];
		break;
	case 4:
		pszSample = pPainSounds[4];
		break;
	default:
		pszSample = pPainSounds[0];
		break;
	}

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSample, VOL_NORM, ATTN_NORM);

	pev->pain_finished = gpGlobals->time + 1.0f;

	if (m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_WALK)
		AlertSound();
}

//=========================================================
// Death
//=========================================================
void CAGrunt::Death(int iDeathType)
{
	const char *pszSample;
	int iSound = RANDOM_LONG(0, 2);
	if (iSound == 1)
		pszSample = pDieSounds[1];
	else if (iSound == 2)
		pszSample = pDieSounds[2];
	else
		pszSample = pDieSounds[0];

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSample, VOL_NORM, ATTN_NORM);

	// same as SetDeathActivity(DEATH_NORMAL)
	m_MonsterState = MONSTERSTATE_DIE1;
	pev->ideal_yaw = pev->angles.y;
	SetActivity(m_MonsterState);
	SetThink(&CBaseMonster::MonsterThink);
	pev->nextthink = gpGlobals->time + AGRUNT_THINK_INTERVAL;
}

//=========================================================
// CheckAttacks
//=========================================================
int CAGrunt::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	if (flDist <= AGRUNT_MELEE_DIST && CheckMeleeAttack(pevEnemy))
	{
		m_IdealMonsterState = MONSTERSTATE_COMBAT;
		SetThink(&CAGrunt::MeleeAttack);
		return TRUE;
	}

	if (CheckRangeAttack(pevEnemy) && flDist <= AGRUNT_RANGE_DIST)
	{
		m_IdealMonsterState = MONSTERSTATE_COMBAT;
		m_iHornetCount = RANDOM_LONG(AGRUNT_MIN_HORNETS, AGRUNT_MAX_HORNETS);
		SetThink(&CAGrunt::HornetAttack);
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// MeleeAttack - plays the melee animation, it does no damage
//=========================================================
void CAGrunt::MeleeAttack(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + AGRUNT_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_MELEE_ATTACK)
	{
		m_MonsterState = MONSTERSTATE_MELEE_ATTACK;
		SetActivity(MONSTERSTATE_MELEE_ATTACK);
		m_flNextAttack = gpGlobals->time + 2.0f;
	}

	AdvanceAnimation(AGRUNT_THINK_INTERVAL);

	UpdateEnemyInfo(VARS(pev->enemy));
	CHANGE_YAW(ENT(pev));

	// the animation has played through
	if (pev->frame >= 255.0f)
	{
		SetThink(&CBaseMonster::MonsterThink);
		m_MonsterState = m_IdealMonsterState;
		m_flNextAttack = gpGlobals->time + 1.0f;
	}
}

//=========================================================
// CreateHornet
//=========================================================
CHornet *CAGrunt::CreateHornet()
{
	return GetClassPtr((CHornet *)NULL);
}

//=========================================================
// HornetAttack - launches a hornet whenever the animation
// reaches a launch event
//=========================================================
void CAGrunt::HornetAttack(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + AGRUNT_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_RANGE_ATTACK)
	{
		float flNextAttack = gpGlobals->time + 2.0f;
		m_MonsterState = MONSTERSTATE_RANGE_ATTACK;
		m_flNextAttack = flNextAttack;
		SetActivity(MONSTERSTATE_RANGE_ATTACK);
	}

	UpdateEnemyInfo(VARS(pev->enemy));
	CHANGE_YAW(ENT(pev));

	int iEvents = GetAnimationEventFlags(AGRUNT_THINK_INTERVAL);
	AdvanceAnimation(AGRUNT_THINK_INTERVAL);

	if (iEvents & AGRUNT_AE_HORNET)
	{
		pev->effects = EF_MUZZLEFLASH;

		CHornet *pHornet = CreateHornet();
		pHornet->Init(pev);
	}

	if (m_fSequenceFinished)
	{
		m_MonsterState = m_IdealMonsterState;
		m_flNextAttack = RANDOM_FLOAT(0.0f, 2.0f) + gpGlobals->time + 1.0f;
		SetThink(&CBaseMonster::MonsterThink);
	}
}

LINK_ENTITY_TO_CLASS(monster_alien_grunt, CAGrunt);
