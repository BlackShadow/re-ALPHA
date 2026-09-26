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
// Human grunt - squad soldier with an MP5 and grenades
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "ggrenade.h"

#define HGRUNT_THINK_INTERVAL		0.1f
#define HGRUNT_HEALTH				50.0f	// plus up to 25
#define HGRUNT_YAW_SPEED			14.0f
#define HGRUNT_CHASE_DIST			512.0f	// chases an enemy that is further away
#define HGRUNT_MELEE_DIST			64.0f
#define HGRUNT_RANGE_DIST			1024.0f	// also the range of his bullets
#define HGRUNT_BULLET_SPREAD		0.035f
#define HGRUNT_CLIP_SIZE			45
#define HGRUNT_SQUAD_RADIUS			512
#define HGRUNT_GRENADE_DELAY		5.0f	// time between grenade throws
#define HGRUNT_GRENADE_MIN_SPEED	400.0f	// too close to throw a grenade
#define HGRUNT_KICK_DAMAGE			5.0f
#define HGRUNT_HEAVY_PAIN_DAMAGE	10.0f

// GetAnimationEventFlags() bits
#define HGRUNT_AE_RELOAD			(1<<1)
#define HGRUNT_AE_KICK				(1<<3)

// hgrunt.mdl sequences, named by the monster state that plays them
enum
{
	HGRUNT_SEQ_WALK = 0,
	HGRUNT_SEQ_RUN = 2,				// also MONSTERSTATE_HUNT and MONSTERSTATE_RETREAT
	HGRUNT_SEQ_DIE1,
	HGRUNT_SEQ_DIE3,
	HGRUNT_SEQ_PAIN = 9,			// also MONSTERSTATE_HEAVY_PAIN
	HGRUNT_SEQ_IDLE = 11,			// also the combat face and combat idle states
	HGRUNT_SEQ_UNUSED,				// accepted by SetActivity, but no state plays it
	HGRUNT_SEQ_COMBAT,
	HGRUNT_SEQ_RELOAD,
	HGRUNT_SEQ_SHOOT,				// MONSTERSTATE_RANGE_ATTACK
	HGRUNT_SEQ_KICK,				// MONSTERSTATE_MELEE_ATTACK
	HGRUNT_SEQ_MOVE_LEFT = 21,
	HGRUNT_SEQ_MOVE_RIGHT,
	HGRUNT_SEQ_SPAWN,				// matches no state, so the first SetActivity always switches
};

static const char *pPainSounds[] =
{
	"hgrunt/gr_pain1.wav",
	"hgrunt/gr_pain2.wav",
	"hgrunt/gr_pain3.wav",
	"hgrunt/gr_pain4.wav",
	"hgrunt/gr_pain5.wav",
};

static const char *pIdleSounds[] =
{
	"hgrunt/gr_idle1.wav",
	"hgrunt/gr_idle2.wav",
	"hgrunt/gr_idle3.wav",
};

static const char *pAlertSounds[] =
{
	"hgrunt/gr_alert1.wav",
};

static const char *pMgunSounds[] =
{
	"hgrunt/gr_mgun1.wav",
	"hgrunt/gr_mgun2.wav",
	"hgrunt/gr_mgun3.wav",
};

static const char *pDieSounds[] =
{
	"hgrunt/gr_die1.wav",
	"hgrunt/gr_die2.wav",
	"hgrunt/gr_die3.wav",
};

static const char *pReloadSounds[] =
{
	"hgrunt/gr_reload1.wav",
};

static const char *pLoadTalkSounds[] =
{
	"hgrunt/gr_loadtalk.wav",
};

class CHGrunt : public CBaseMonster
{
public:
	CHGrunt();

	void Spawn();
	int Classify();
	void SetActivity(int activity);
	void IdleSound();
	void AlertSound();
	int CheckAttacks(entvars_t *pevEnemy, float flDist);
	void Pain(float flDamage);
	void Death(int iDeathType);

	void ShootThink(CBaseEntity *pOther);
	void MeleeAttackThink(CBaseEntity *pOther);
	void ReloadThink(CBaseEntity *pOther);
	void HeavyPainThink(CBaseEntity *pOther);

	void TossGrenade(entvars_t *pevEnemy);

	float	m_flNextGrenadeCheck;
	float	m_flNextPainSound;		// the loud pain sounds play at most this often
};

LINK_ENTITY_TO_CLASS(monster_human_grunt, CHGrunt);

CHGrunt::CHGrunt()
{
	m_flNextGrenadeCheck = 0.0f;
	m_flNextPainSound = 0.0f;
}

// from our eyes to the enemy's eyes
static Vector ShootDirection(entvars_t *pev, entvars_t *pevEnemy)
{
	return ((pevEnemy->origin + pevEnemy->view_ofs) - (pev->origin + pev->view_ofs)).Normalize();
}

//=========================================================
// Spawn
//=========================================================
void CHGrunt::Spawn()
{
	int i;
	const char *szModel = FClassnameIs(pev, "monster_human_assault") ? "models/hassault.mdl" : "models/hgrunt.mdl";

	PRECACHE_MODEL(szModel);

	PRECACHE_SOUND("player/hoot1.wav");
	PRECACHE_SOUND(pReloadSounds[0]);
	PRECACHE_SOUND(pLoadTalkSounds[0]);

	for (i = 0; i < ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND(pPainSounds[i]);

	PRECACHE_SOUND(pAlertSounds[0]);

	for (i = 0; i < ARRAYSIZE(pIdleSounds); i++)
		PRECACHE_SOUND(pIdleSounds[i]);

	for (i = 0; i < ARRAYSIZE(pMgunSounds); i++)
		PRECACHE_SOUND(pMgunSounds[i]);

	for (i = 0; i < ARRAYSIZE(pDieSounds); i++)
		PRECACHE_SOUND(pDieSounds[i]);

	SET_MODEL(ENT(pev), szModel);
	UTIL_SetSize(pev, Vector(-18.0f, -18.0f, 0.0f), Vector(18.0f, 18.0f, 72.0f));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = RANDOM_FLOAT(0.0f, 25.0f) + HGRUNT_HEALTH;
	pev->yaw_speed = HGRUNT_YAW_SPEED;
	pev->sequence = HGRUNT_SEQ_SPAWN;

	m_afEnemyFlags = 0;
	m_iRouteGoal = 0;
	m_iRouteIndex = 0;
	m_iSquadSize = 1;
	m_pSquadLeader = NULL;
	m_pSquadNext = NULL;

	m_iAmmo = HGRUNT_CLIP_SIZE;
	m_flDistTooFar = HGRUNT_CHASE_DIST;
	m_bloodColor = BLOOD_COLOR_RED;

	pev->weapon = WEAPON_MP5;

	m_flNextGrenadeCheck = 0.0f;
	m_flNextPainSound = 0.0f;

	SetThink(&CBaseMonster::WalkMonsterStart);
	pev->nextthink = RANDOM_FLOAT(0.0f, 0.5f) + pev->nextthink + 0.5f;
}

int CHGrunt::Classify()
{
	return CLASS_HUMAN_MILITARY;
}

//=========================================================
// SetActivity
//=========================================================
void CHGrunt::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_IDLE2:
	case MONSTERSTATE_IDLE3:
	case MONSTERSTATE_COMBAT_FACE:
	case MONSTERSTATE_COMBAT_IDLE:
		sequence = HGRUNT_SEQ_IDLE;
		break;

	case MONSTERSTATE_WALK:
		sequence = HGRUNT_SEQ_WALK;
		break;

	case MONSTERSTATE_COMBAT:
		sequence = HGRUNT_SEQ_COMBAT;
		break;

	case MONSTERSTATE_CHASE:
	case MONSTERSTATE_HUNT:
	case MONSTERSTATE_RETREAT:
		sequence = HGRUNT_SEQ_RUN;
		break;

	case MONSTERSTATE_MOVE_LEFT:
		sequence = HGRUNT_SEQ_MOVE_LEFT;
		break;

	case MONSTERSTATE_MOVE_RIGHT:
		sequence = HGRUNT_SEQ_MOVE_RIGHT;
		break;

	case MONSTERSTATE_PAIN:
	case MONSTERSTATE_HEAVY_PAIN:
		sequence = HGRUNT_SEQ_PAIN;
		break;

	case MONSTERSTATE_MELEE_ATTACK:
		sequence = HGRUNT_SEQ_KICK;
		break;

	case MONSTERSTATE_RANGE_ATTACK:
		sequence = HGRUNT_SEQ_SHOOT;
		break;

	case MONSTERSTATE_RELOAD:
		sequence = HGRUNT_SEQ_RELOAD;
		break;

	case MONSTERSTATE_DIE1:
		sequence = HGRUNT_SEQ_DIE1;
		break;

	case MONSTERSTATE_DIE3:
		sequence = HGRUNT_SEQ_DIE3;
		break;

	// keep the current animation
	case MONSTERSTATE_ALERT:
	case MONSTERSTATE_FIND_COVER:
	case MONSTERSTATE_FIND_RETREAT:
	case MONSTERSTATE_FIND_SHOOT_POSITION:
	case MONSTERSTATE_ATTACK:
		return;

	default:
		ALERT(at_console, "HGrunt's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == sequence)
		return;

	pev->sequence = sequence;
	pev->frame = 0;
	ResetSequenceInfo(HGRUNT_THINK_INTERVAL);

	switch (sequence)
	{
	case HGRUNT_SEQ_WALK:
	case HGRUNT_SEQ_RUN:
	case HGRUNT_SEQ_DIE1:
	case HGRUNT_SEQ_DIE3:
	case HGRUNT_SEQ_PAIN:
	case HGRUNT_SEQ_RELOAD:
	case HGRUNT_SEQ_SHOOT:
	case HGRUNT_SEQ_KICK:
	case HGRUNT_SEQ_MOVE_LEFT:
	case HGRUNT_SEQ_MOVE_RIGHT:
		break;

	// looping animations start at a random frame
	case HGRUNT_SEQ_IDLE:
	case HGRUNT_SEQ_UNUSED:
	case HGRUNT_SEQ_COMBAT:
		pev->frame = RANDOM_FLOAT(0.0f, 1.0f) * 256.0f;
		break;

	default:
		ALERT(at_console, "Bogus HGrunt anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// IdleSound
//=========================================================
void CHGrunt::IdleSound()
{
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);
	float flVolume = RANDOM_FLOAT(0.5f, 1.0f);
	const char *pszSound;

	if (flRand <= 0.33f)
		pszSound = pIdleSounds[0];
	else if (flRand <= 0.66f)
		pszSound = pIdleSounds[1];
	else
		pszSound = pIdleSounds[2];

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, flVolume, ATTN_IDLE);

	m_flNextSoundTime = gpGlobals->time + RANDOM_FLOAT(0.0f, 5.0f) + 3.0f;
}

//=========================================================
// AlertSound - the first grunt to see the enemy forms a
// squad and leads it
//=========================================================
void CHGrunt::AlertSound()
{
	if (m_iSquadSize > 1)
		return;

	int iRecruits = SquadRecruit(HGRUNT_SQUAD_RADIUS);
	if (iRecruits == 0)
	{
		ALERT(at_console, "No Squad");
		m_MonsterState = MONSTERSTATE_CHASE;
		return;
	}

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pAlertSounds[0], VOL_NORM, ATTN_NORM);

	unsigned int iSquadSize = (unsigned int)iRecruits + m_iSquadSize;
	m_iSquadSize = iSquadSize;

	// grunts take cover, assault grunts attack
	entvars_t *pevMember = pev;
	for (unsigned int i = 0; i < iSquadSize; i++)
	{
		CBaseMonster *pMember = (CBaseMonster *)CBaseEntity::Instance(pevMember);
		if (!pMember)
			break;

		if (FClassnameIs(pevMember, "monster_human_grunt"))
		{
			pMember->m_MonsterState = MONSTERSTATE_FIND_COVER;
			pMember->m_IdealMonsterState = MONSTERSTATE_COMBAT_FACE;
		}
		else if (FClassnameIs(pevMember, "monster_human_assault"))
		{
			pMember->m_MonsterState = MONSTERSTATE_ALERT;
		}

		pMember->m_iSquadSize = iSquadSize;
		ALERT(at_console, "%d\n", pMember->m_iSquadSize);

		pevMember = pMember->m_pSquadNext;
	}

	ALERT(at_console, "group of: %d\n", m_iSquadSize);

	m_iSquadSize = (unsigned int)iRecruits;
	m_MonsterState = MONSTERSTATE_ALERT;
	m_IdealMonsterState = MONSTERSTATE_FIND_RETREAT;
	m_fSquadLeader = TRUE;
}

//=========================================================
// CheckAttacks - kicks, shoots, throws a grenade at an
// enemy out of sight, or reloads
//=========================================================
int CHGrunt::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	if (!pevEnemy)
		return FALSE;

	if (m_iAmmo > 0)
	{
		if (flDist <= HGRUNT_MELEE_DIST && CheckMeleeAttack(pevEnemy))
		{
			m_IdealMonsterState = MONSTERSTATE_COMBAT;
			SetThink(&CHGrunt::MeleeAttackThink);
			return TRUE;
		}

		if (CheckRangeAttack(pevEnemy) && flDist <= HGRUNT_RANGE_DIST)
		{
			m_IdealMonsterState = MONSTERSTATE_COMBAT;
			SetThink(&CHGrunt::ShootThink);
			return TRUE;
		}

		if (gpGlobals->time <= m_flNextGrenadeCheck)
			return FALSE;

		if (FVisible(pev, pevEnemy))
			return FALSE;

		TossGrenade(pevEnemy);
		m_flNextGrenadeCheck = gpGlobals->time + HGRUNT_GRENADE_DELAY;
		return TRUE;
	}

	if (FVisible(pev, pevEnemy))
		m_IdealMonsterState = MONSTERSTATE_ATTACK;
	else
		m_IdealMonsterState = MONSTERSTATE_FIND_SHOOT_POSITION;

	SetThink(&CHGrunt::ReloadThink);
	return TRUE;
}

//=========================================================
// ShootThink
//=========================================================
void CHGrunt::ShootThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + HGRUNT_THINK_INTERVAL;

	if (FNullEnt(pev->enemy))
		return;

	entvars_t *pevEnemy = VARS(pev->enemy);

	if (m_MonsterState != MONSTERSTATE_RANGE_ATTACK)
	{
		Vector vecDir = ShootDirection(pev, pevEnemy);

		// a friend is in the way, move
		if (CheckFriendlyFire(pev, vecDir, HGRUNT_RANGE_DIST))
		{
			m_MonsterState = MONSTERSTATE_FIND_SHOOT_POSITION;
			m_IdealMonsterState = MONSTERSTATE_ATTACK;
			SetThink(&CBaseMonster::MonsterThink);
			return;
		}

		m_MonsterState = MONSTERSTATE_RANGE_ATTACK;
		SetActivity(MONSTERSTATE_RANGE_ATTACK);
	}

	AdvanceAnimation(HGRUNT_THINK_INTERVAL);

	float flDist = UpdateEnemyInfo(pevEnemy);
	if (flDist <= HGRUNT_MELEE_DIST)
	{
		SetThink(&CHGrunt::MeleeAttackThink);
		return;
	}

	CHANGE_YAW(ENT(pev));

	if (!(m_afEnemyFlags & ENEMY_VISIBLE))
	{
		m_MonsterState = MONSTERSTATE_COMBAT_IDLE;
		SetThink(&CBaseMonster::MonsterThink);
		return;
	}

	Vector vecDir = ShootDirection(pev, pevEnemy);

	if (CheckFriendlyFire(pev, vecDir, HGRUNT_RANGE_DIST))
	{
		m_MonsterState = MONSTERSTATE_FIND_SHOOT_POSITION;
		m_IdealMonsterState = MONSTERSTATE_ATTACK;
		SetThink(&CBaseMonster::MonsterThink);
		return;
	}

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, pMgunSounds[rand() % ARRAYSIZE(pMgunSounds)], VOL_NORM, ATTN_NORM);

	FireBullets(1, vecDir, HGRUNT_BULLET_SPREAD, HGRUNT_BULLET_SPREAD, BULLET_NONE, HGRUNT_RANGE_DIST);
	m_iAmmo--;

	if (m_fSequenceFinished)
	{
		if (m_iAmmo > 0)
		{
			pev->frame = 0;
		}
		else
		{
			// out of ammo, take cover to reload
			m_MonsterState = MONSTERSTATE_FIND_COVER;
			m_IdealMonsterState = MONSTERSTATE_ATTACK;
			SetThink(&CBaseMonster::MonsterThink);
			m_flNextAttack = gpGlobals->time + 1.0f;
		}
	}
}

//=========================================================
// MeleeAttackThink - kicks the enemy away
//=========================================================
void CHGrunt::MeleeAttackThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + HGRUNT_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_MELEE_ATTACK)
	{
		m_MonsterState = MONSTERSTATE_MELEE_ATTACK;
		SetActivity(MONSTERSTATE_MELEE_ATTACK);
		m_flNextAttack = gpGlobals->time + 2.0f;
	}

	int iEvents = GetAnimationEventFlags(HGRUNT_THINK_INTERVAL);
	AdvanceAnimation(HGRUNT_THINK_INTERVAL);

	if (!FNullEnt(pev->enemy))
		UpdateEnemyInfo(VARS(pev->enemy));

	CHANGE_YAW(ENT(pev));

	if (iEvents & HGRUNT_AE_KICK)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/hoot1.wav", VOL_NORM, ATTN_NORM);

		UTIL_MakeVectors(pev->angles);

		Vector vecStart = pev->origin;
		vecStart.z += 50.0f;
		Vector vecEnd = vecStart + gpGlobals->v_forward * 64.0f;
		vecEnd.z -= 16.0f;

		TraceResult tr;
		UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

		edict_t *pentHit = ENT(tr.pHit);
		entvars_t *pevHit = VARS(pentHit);

		if (pevHit->takedamage != DAMAGE_NO)
		{
			CBaseEntity *pHit = CBaseEntity::Instance(pentHit);
			if (pHit)
				pHit->TakeDamage(pev, pev, HGRUNT_KICK_DAMAGE);

			pevHit->punchangle.x = 15.0f;
			pevHit->velocity = pevHit->velocity + gpGlobals->v_forward * 250.0f + gpGlobals->v_up * 200.0f;

			m_flNextAttack = gpGlobals->time;
			m_MonsterState = m_IdealMonsterState;
			SetThink(&CBaseMonster::MonsterThink);
			return;
		}
	}

	if (m_fSequenceFinished)
	{
		SetThink(&CBaseMonster::MonsterThink);
		m_MonsterState = m_IdealMonsterState;
	}
}

//=========================================================
// ReloadThink
//=========================================================
void CHGrunt::ReloadThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + HGRUNT_THINK_INTERVAL;

	if (m_MonsterState != MONSTERSTATE_RELOAD)
	{
		m_MonsterState = MONSTERSTATE_RELOAD;
		SetActivity(MONSTERSTATE_RELOAD);
	}

	int iEvents = GetAnimationEventFlags(HGRUNT_THINK_INTERVAL);
	AdvanceAnimation(HGRUNT_THINK_INTERVAL);

	if (!FNullEnt(pev->enemy))
		UpdateEnemyInfo(VARS(pev->enemy));

	CHANGE_YAW(ENT(pev));

	if (iEvents & HGRUNT_AE_RELOAD)
	{
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, pReloadSounds[0], VOL_NORM, ATTN_NORM);

		if (RANDOM_FLOAT(0.0f, 1.0f) < 0.25f)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, pLoadTalkSounds[0], VOL_NORM, ATTN_NORM);
	}

	if (m_fSequenceFinished)
	{
		m_iAmmo = HGRUNT_CLIP_SIZE;
		SetThink(&CBaseMonster::MonsterThink);
		m_MonsterState = m_IdealMonsterState;
	}
}

//=========================================================
// HeavyPainThink
//=========================================================
void CHGrunt::HeavyPainThink(CBaseEntity *pOther)
{
	if (m_MonsterState != MONSTERSTATE_HEAVY_PAIN)
	{
		m_MonsterState = MONSTERSTATE_HEAVY_PAIN;
		SetActivity(MONSTERSTATE_HEAVY_PAIN);
	}

	pev->nextthink = gpGlobals->time + HGRUNT_THINK_INTERVAL;
	AdvanceAnimation(HGRUNT_THINK_INTERVAL);

	entvars_t *pevEnemy = NULL;
	if (!FNullEnt(pev->enemy))
	{
		pevEnemy = VARS(pev->enemy);
		UpdateEnemyInfo(pevEnemy);
	}

	CHANGE_YAW(ENT(pev));

	if (!m_fSequenceFinished)
		return;

	SetThink(&CBaseMonster::MonsterThink);

	if (pevEnemy && FInViewCone(pev, pevEnemy, 0.1f))
	{
		m_MonsterState = MONSTERSTATE_FIND_COVER;
		m_IdealMonsterState = MONSTERSTATE_COMBAT_FACE;
	}
	else
	{
		m_MonsterState = MONSTERSTATE_COMBAT_IDLE;
	}
}

//=========================================================
// TossGrenade - throws a grenade at an enemy that is out
// of sight, unless one of us is close to it
//=========================================================
void CHGrunt::TossGrenade(entvars_t *pevEnemy)
{
	Vector vecTarget = pevEnemy->origin;
	vecTarget.z += 32.0f;

	Vector vecStart = pev->origin;
	vecStart.z += 32.0f;

	Vector vecTossTarget = GetTossTarget(pev, vecStart, vecTarget);

	if (vecTossTarget == g_vecZero)
		return;

	edict_t *pentEnt = FIND_ENTITY_IN_SPHERE(pevEnemy->origin, 72.0f);
	while (!FNullEnt(pentEnt))
	{
		entvars_t *pevEnt = VARS(pentEnt);
		if (FStrEq(STRING(pevEnt->classname), STRING(pev->classname)))
			return;

		pentEnt = ENT(pevEnt->chain);
	}

	Vector vecDir = vecTossTarget - pev->origin;
	float flDist = vecDir.Length();
	if (flDist <= 0.0f)
		return;

	vecDir = vecDir * (1.0f / flDist);

	// throw harder the further away and the higher the target is
	float flHorizontal = (pev->origin - vecTossTarget).Length2D();
	float flSpeed = (flHorizontal * 2.0f + vecTossTarget.z - pev->origin.z) * 0.5f;

	Vector vecVelocity = vecDir * flSpeed * 2.0f;
	if (vecVelocity.Length() < HGRUNT_GRENADE_MIN_SPEED)
		return;

	ShootTimedGrenade(pev, vecStart, vecVelocity);
}

//=========================================================
// Pain
//=========================================================
void CHGrunt::Pain(float flDamage)
{
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);
	pev->pain_finished = gpGlobals->time + 1.0f;

	const char *pszSound;

	if (gpGlobals->time <= m_flNextPainSound)
	{
		if (flRand <= 0.33f)
			pszSound = pPainSounds[2];
		else if (flRand <= 0.66f)
			pszSound = pPainSounds[3];
		else
			pszSound = pPainSounds[4];
	}
	else
	{
		if (flRand <= 0.2f)
			pszSound = pPainSounds[0];
		else if (flRand <= 0.4f)
			pszSound = pPainSounds[1];
		else if (flRand <= 0.6f)
			pszSound = pPainSounds[2];
		else if (flRand <= 0.8f)
			pszSound = pPainSounds[3];
		else
			pszSound = pPainSounds[4];

		m_flNextPainSound = gpGlobals->time + 15.0f;
	}

	EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_NORM);

	// hurt while not fighting yet, rally the squad
	if (m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_WALK)
	{
		AlertSound();
		SetThink(&CBaseMonster::MonsterThink);
		return;
	}

	if (flDamage >= HGRUNT_HEAVY_PAIN_DAMAGE)
	{
		SetThink(&CHGrunt::HeavyPainThink);
		pev->nextthink = gpGlobals->time;
		return;
	}

	SetThink(&CBaseMonster::MonsterThink);

	if (FNullEnt(pev->enemy))
		return;

	if (FVisible(pev, VARS(pev->enemy)))
	{
		if (m_MonsterState != MONSTERSTATE_MOVE_LEFT && m_MonsterState != MONSTERSTATE_MOVE_RIGHT)
		{
			if (RANDOM_FLOAT(0.0f, 1.0f) >= 0.75f)
			{
				m_MonsterState = MONSTERSTATE_ATTACK;
			}
			else
			{
				m_MonsterState = MONSTERSTATE_FIND_COVER;
				m_IdealMonsterState = MONSTERSTATE_COMBAT_FACE;
			}
		}
	}
	else
	{
		m_MonsterState = MONSTERSTATE_FIND_SHOOT_POSITION;
		m_IdealMonsterState = MONSTERSTATE_COMBAT_IDLE;
	}
}

//=========================================================
// Death
//=========================================================
void CHGrunt::Death(int iDeathType)
{
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);

	// no scream when blown apart
	if (pev->health > GIB_HEALTH)
	{
		const char *pszSound;

		if (flRand <= 0.33f)
			pszSound = pDieSounds[0];
		else if (flRand <= 0.66f)
			pszSound = pDieSounds[1];
		else
			pszSound = pDieSounds[2];

		EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_NORM);
	}

	SetDeathActivity(DEATH_NORMAL);
}
