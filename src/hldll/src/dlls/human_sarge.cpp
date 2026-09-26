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
// Human sergeant - throws a shard at the player it spots
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "ggrenade.h"

#define HSARGE_THINK_INTERVAL		0.1f
#define HSARGE_HEALTH				30.0f
#define HSARGE_YAW_SPEED			10.0f
#define HSARGE_FIELD_OF_VIEW		0.1f	// FInViewCone dot product
#define HSARGE_THROW_DELAY			2.0f	// from spotting the player to throwing
#define HSARGE_THROW_HEIGHT			32.0f	// above the sergeant's and the target's origins
#define HSARGE_SHARD_SPEED_SCALE	1.9		// shard speed per unit of distance to the toss target

// hsarge.mdl sequences, named by the monster state that plays them
enum
{
	HSARGE_SEQ_WALK = 0,		// also MONSTERSTATE_CHASE and MONSTERSTATE_MELEE_ATTACK
	HSARGE_SEQ_DIE1,
	HSARGE_SEQ_SPAWN,			// matches no state, so the first SetActivity always switches
};

static const char szHSargeModel[] = "models/hsarge.mdl";
static const char szShardSprite[] = "sprites/shard.spr";
static const char szSpotSound[] = "player/pain3.wav";

// Returns the entity variables of the current enemy, or NULL
static entvars_t *EnemyVars(entvars_t *pev)
{
	if (FNullEnt(pev->enemy))
		return NULL;

	return VARS(pev->enemy);
}

class CHSarge : public CBaseMonster
{
public:
	void Spawn();
	void SetActivity(int activity);
	void SargeThink(CBaseEntity *pOther);
	void LaunchShard(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(monster_human_sarge, CHSarge);

//=========================================================
// Spawn
//=========================================================
void CHSarge::Spawn()
{
	PRECACHE_MODEL(szHSargeModel);

	SET_MODEL(ENT(pev), szHSargeModel);
	UTIL_SetSize(pev, Vector(-18.0f, -18.0f, 0.0f), Vector(18.0f, 18.0f, 72.0f));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = HSARGE_HEALTH;
	pev->yaw_speed = HSARGE_YAW_SPEED;
	pev->sequence = HSARGE_SEQ_SPAWN;

	pev->view_ofs = Vector(0.0f, 0.0f, 64.0f);	// eye position

	SetThink(&CHSarge::SargeThink);
	pev->nextthink = pev->nextthink + HSARGE_THINK_INTERVAL;
}

//=========================================================
// SetActivity
//=========================================================
void CHSarge::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case MONSTERSTATE_WALK:
	case MONSTERSTATE_CHASE:
	case MONSTERSTATE_MELEE_ATTACK:
		sequence = HSARGE_SEQ_WALK;
		break;

	case MONSTERSTATE_DIE1:
		sequence = HSARGE_SEQ_DIE1;
		break;

	default:
		ALERT(at_console, "HSarge's monster state is bogus: %d", activity);
		return;
	}

	if (pev->sequence == sequence)
		return;

	pev->sequence = sequence;
	pev->frame = 0;
	ResetSequenceInfo(HSARGE_THINK_INTERVAL);

	switch (sequence)
	{
	case HSARGE_SEQ_WALK:
	case HSARGE_SEQ_DIE1:
		break;

	default:
		ALERT(at_console, "Bogus HSarge anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// SargeThink - waits for the player to come into view
//=========================================================
void CHSarge::SargeThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + HSARGE_THINK_INTERVAL;

	edict_t *pentClient = FIND_CLIENT_IN_PVS();
	if (FNullEnt(pentClient))
		return;

	entvars_t *pevClient = VARS(pentClient);

	if ((int)pevClient->flags & FL_NOTARGET)
		return;

	if (!FInViewCone(pev, pevClient, HSARGE_FIELD_OF_VIEW))
		return;

	if (!FVisible(pev, pevClient))
		return;

	pev->enemy = OFFSET(pentClient);

	EMIT_SOUND(ENT(pev), CHAN_VOICE, szSpotSound, VOL_NORM, ATTN_NORM);

	// the positions found are not used
	FindCover(pevClient);
	FindRetreat(pevClient);

	SetThink(&CHSarge::LaunchShard);
	pev->nextthink = gpGlobals->time + HSARGE_THROW_DELAY;
}

//=========================================================
// LaunchShard - lobs a bouncing shard at the enemy
//=========================================================
void CHSarge::LaunchShard(CBaseEntity *pOther)
{
	entvars_t *pevEnemy = EnemyVars(pev);
	if (!pevEnemy)
		return;

	Vector vecTarget(pevEnemy->origin.x, pevEnemy->origin.y, pevEnemy->origin.z + HSARGE_THROW_HEIGHT);
	Vector vecStart(pev->origin.x, pev->origin.y, pev->origin.z + HSARGE_THROW_HEIGHT);

	Vector vecTossTarget = GetTossTarget(pev, vecStart, vecTarget);

	if (vecTossTarget == g_vecZero)
	{
		SetThink(NULL);
		return;
	}

	edict_t *pentShard = CREATE_ENTITY();
	entvars_t *pevShard = VARS(pentShard);

	pevShard->movetype = MOVETYPE_BOUNCE;
	pevShard->solid = SOLID_BBOX;
	SET_MODEL(pentShard, szShardSprite);
	UTIL_SetSize(pevShard, g_vecZero, g_vecZero);

	pevShard->origin = Vector(pev->origin.x, pev->origin.y, pev->origin.z + HSARGE_THROW_HEIGHT);

	float flDist = (vecTossTarget - pev->origin).Length();

	Vector vecDir = vecTossTarget - pevShard->origin;
	float flLength = vecDir.Length();
	if (flLength <= 0.0f)
		vecDir = g_vecZero;
	else
		vecDir = vecDir * (1.0f / flLength);

	// the speed is scaled in double precision
	double flSpeed = (double)flDist * HSARGE_SHARD_SPEED_SCALE;
	pevShard->velocity.x = (float)(vecDir.x * flSpeed);
	pevShard->velocity.y = (float)(vecDir.y * flSpeed);
	pevShard->velocity.z = (float)(vecDir.z * flSpeed);
}
