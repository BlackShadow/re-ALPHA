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
// func_breakable, func_glass and func_pushable
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

typedef enum
{
	matGlass = 0,
	matWood,
	matMetal,
	matFlesh,
	matCinderBlock,
	matCeilingTile,
	matLastMaterial,
} Materials;

typedef enum
{
	expRandom,
	expDirected,
} Explosions;

#define SF_BREAK_TRIGGER_ONLY	1		// can only be broken by a trigger

// TE_BREAKMODEL flags. The client makes glass gibs translucent, gives
// metal gibs smoke trails and throws four times as many wood gibs.
#define BREAK_GLASS				0x01
#define BREAK_METAL				0x02
#define BREAK_FLESH				0x04
#define BREAK_WOOD				0x08
#define BREAK_SMOKE				0x10	// smoke trails

#define BREAK_SHARDS			6		// default of the unused "shards" key

#define BREAK_GIB_COUNT_AUTO	0		// the client picks the number of gibs from the size
#define BREAK_GIB_LIFE			15		// tenths of a second
#define GLASS_SHARD_COUNT		12

// damage done by a player running into it, per unit of speed
#define BREAK_TOUCH_DAMAGE		0.01f
#define GLASS_TOUCH_DAMAGE		0.1f

// func_pushable "size" key
#define PUSHABLE_SIZE_POINT		0
#define PUSHABLE_SIZE_BIG		2
#define PUSHABLE_SIZE_DUCK		3		// any other value is the player size

// a pushable's top speed is this minus its friction
#define PUSHABLE_MAX_SPEED		400.0f

// the player pushes with half its speed
#define PUSHABLE_PUSH_SCALE		0.5f

//=========================================================
// func_breakable
//=========================================================
class CBreakable : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Touch(CBaseEntity *pOther);
	void Use(CBaseEntity *pOther);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage);

	void Die();

	Materials		m_Material;
	Explosions		m_Explosion;
	int				m_iShards;			// "shards" key, not used
	int				m_idShard;			// model index of the gibs
	float			m_angle;			// spawn yaw, gives g_vecAttackDir when used
};

void CBreakable::Spawn()
{
	PRECACHE_SOUND("common/glass.wav");

	if (!((int)pev->spawnflags & SF_BREAK_TRIGGER_ONLY))
		pev->takedamage = DAMAGE_AIM;
	else
		pev->takedamage = DAMAGE_NO;

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	// the brush itself is not turned by its yaw
	m_angle = pev->angles.y;
	pev->angles.y = 0.0f;

	m_iShards = BREAK_SHARDS;

	switch (m_Material)
	{
	case matGlass:
		m_idShard = PRECACHE_MODEL("sprites/shard.spr");
		PRECACHE_SOUND("debris/bustglass1.wav");
		PRECACHE_SOUND("debris/bustglass2.wav");
		PRECACHE_SOUND("debris/bustglass3.wav");
		break;
	case matWood:
		m_idShard = PRECACHE_MODEL("models/woodgibs.mdl");
		PRECACHE_SOUND("debris/bustcrate1.wav");
		PRECACHE_SOUND("debris/bustcrate3.wav");
		break;
	case matMetal:
		m_idShard = PRECACHE_MODEL("models/shrapnel.mdl");
		break;
	case matFlesh:
		m_idShard = PRECACHE_MODEL("models/gib_b_gib.mdl");
		break;
	case matCinderBlock:
		m_idShard = PRECACHE_MODEL("models/cindergibs.mdl");
		break;
	case matCeilingTile:
		m_idShard = PRECACHE_MODEL("models/ceilinggibs.mdl");
		PRECACHE_SOUND("debris/bustceiling.wav");
		break;
	default:
		break;
	}

	SET_MODEL(ENT(pev), STRING(pev->model));
}

void CBreakable::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "explosion"))
	{
		if (!_stricmp(pkvd->szValue, "directed"))
			m_Explosion = expDirected;
		else
			m_Explosion = expRandom;

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "material"))
	{
		int i = atoi(pkvd->szValue);
		if (i < 0 || i >= matLastMaterial)
			m_Material = matWood;
		else
			m_Material = (Materials)i;

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deadmodel"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shards"))
	{
		m_iShards = (int)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "lip") || FStrEq(pkvd->szKeyName, "wait"))
	{
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// Touch - a player running into it hard enough breaks it
//=========================================================
void CBreakable::Touch(CBaseEntity *pOther)
{
	entvars_t *pevToucher = VARS(gpGlobals->other);

	if (((int)pevToucher->flags & FL_CLIENT) && !((int)pev->spawnflags & SF_BREAK_TRIGGER_ONLY))
	{
		float flDamage = pevToucher->velocity.Length() * BREAK_TOUCH_DAMAGE;

		if (flDamage >= pev->health)
			TakeDamage(pevToucher, pevToucher, flDamage);
	}
}

//=========================================================
// Use - breaks it, g_vecAttackDir points along the spawn yaw
//=========================================================
void CBreakable::Use(CBaseEntity *pOther)
{
	pev->angles.y = m_angle;
	UTIL_MakeVectors(pev->angles);

	g_vecAttackDir = gpGlobals->v_forward;

	Die();
}

int CBreakable::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage)
{
	if (pev->takedamage == DAMAGE_NO)
		return 0;

	pev->health -= (float)ceil(flDamage);

	if (pev->health <= 0.0f)
		Die();

	return 1;
}

//=========================================================
// Die - plays the break sound, throws the gibs and lets
// whatever stands on it fall
//=========================================================
void CBreakable::Die()
{
	unsigned char cFlag = 0;

	switch (m_Material)
	{
	case matGlass:
	{
		cFlag = BREAK_GLASS;
		int iPick = RANDOM_LONG(0, 2);
		if (iPick == 0)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustglass1.wav", VOL_NORM, ATTN_NORM);
		else if (iPick == 1)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustglass2.wav", VOL_NORM, ATTN_NORM);
		else
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustglass3.wav", VOL_NORM, ATTN_NORM);
		break;
	}
	case matWood:
	{
		cFlag = BREAK_WOOD;
		int iPick = RANDOM_LONG(0, 1);
		if (iPick == 0)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustcrate1.wav", VOL_NORM, ATTN_NORM);
		else
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustcrate3.wav", VOL_NORM, ATTN_NORM);
		break;
	}
	case matMetal:
		cFlag = BREAK_METAL | BREAK_SMOKE;
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/glass.wav", VOL_NORM, ATTN_NORM);
		break;
	case matFlesh:
		cFlag = BREAK_FLESH;
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/glass.wav", VOL_NORM, ATTN_NORM);
		break;
	case matCinderBlock:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/glass.wav", VOL_NORM, ATTN_NORM);
		break;
	case matCeilingTile:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustceiling.wav", VOL_NORM, ATTN_NORM);
		break;
	default:
		break;
	}

	// a random direction is picked, but not used
	if (m_Explosion != expDirected)
	{
		RANDOM_FLOAT(-1.0f, 1.0f);
		RANDOM_FLOAT(-1.0f, 1.0f);
		RANDOM_FLOAT(-1.0f, 1.0f);
	}

	Vector vecSpot = (pev->mins + pev->maxs) * 0.5f;

	// the client throws the gibs in random directions, scaled by this
	Vector vecVelocity = Vector(1.0f, 1.0f, 1.0f);

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_BREAKMODEL);

	// position
	WRITE_COORD(MSG_BROADCAST, vecSpot.x);
	WRITE_COORD(MSG_BROADCAST, vecSpot.y);
	WRITE_COORD(MSG_BROADCAST, vecSpot.z);

	// size
	WRITE_COORD(MSG_BROADCAST, pev->size.x);
	WRITE_COORD(MSG_BROADCAST, pev->size.y);
	WRITE_COORD(MSG_BROADCAST, pev->size.z);

	// velocity
	WRITE_COORD(MSG_BROADCAST, vecVelocity.x);
	WRITE_COORD(MSG_BROADCAST, vecVelocity.y);
	WRITE_COORD(MSG_BROADCAST, vecVelocity.z);

	WRITE_SHORT(MSG_BROADCAST, m_idShard);
	WRITE_BYTE(MSG_BROADCAST, BREAK_GIB_COUNT_AUTO);
	WRITE_BYTE(MSG_BROADCAST, BREAK_GIB_LIFE);
	WRITE_BYTE(MSG_BROADCAST, cFlag);

	// whatever stands on it is no longer on the ground
	float flRadius = pev->size.x;
	if (flRadius < pev->size.y)
		flRadius = pev->size.y;
	if (flRadius < pev->size.z)
		flRadius = pev->size.z;

	edict_t *pentFound = FIND_ENTITY_IN_SPHERE(vecSpot, flRadius);
	while (pentFound)
	{
		// the chain of found entities ends at the world
		if (FNullEnt(pentFound))
			break;

		entvars_t *pevFound = VARS(pentFound);
		int iFlags = (int)pevFound->flags;
		if (iFlags & FL_ONGROUND)
		{
			pevFound->flags = (float)(iFlags & ~FL_ONGROUND);
			pevFound->groundentity = 0;
		}

		pentFound = ENT(pevFound->chain);
	}

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = (float)(pev->ltime + 0.1);
}

//=========================================================
// func_glass - a pane that shatters into glass shards
//=========================================================
class CGlass : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pOther);
	void Touch(CBaseEntity *pOther);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage);

	void Die();

	int				m_idShard;			// model index of the shards
};

// func_glass takes no keys, not even the CBaseToggle ones
void CGlass::KeyValue(KeyValueData *pkvd)
{
}

int CGlass::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage)
{
	if (pev->takedamage == DAMAGE_NO)
		return 0;

	pev->health -= (float)ceil(flDamage);

	if (pev->health <= 0.0f)
		Die();

	return 1;
}

//=========================================================
// Die - shatters and removes the pane
//=========================================================
void CGlass::Die()
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/glass.wav", VOL_NORM, ATTN_NORM);

	UTIL_MakeVectors(pev->angles);

	Vector vecSpot = (pev->mins + pev->maxs) * 0.5f;
	Vector vecVelocity = Vector(1.0f, 1.0f, 1.0f);

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_BREAKMODEL);

	// position
	WRITE_COORD(MSG_BROADCAST, vecSpot.x);
	WRITE_COORD(MSG_BROADCAST, vecSpot.y);
	WRITE_COORD(MSG_BROADCAST, vecSpot.z);

	// size
	WRITE_COORD(MSG_BROADCAST, pev->size.x);
	WRITE_COORD(MSG_BROADCAST, pev->size.y);
	WRITE_COORD(MSG_BROADCAST, pev->size.z);

	// velocity
	WRITE_COORD(MSG_BROADCAST, vecVelocity.x);
	WRITE_COORD(MSG_BROADCAST, vecVelocity.y);
	WRITE_COORD(MSG_BROADCAST, vecVelocity.z);

	WRITE_SHORT(MSG_BROADCAST, m_idShard);
	WRITE_BYTE(MSG_BROADCAST, GLASS_SHARD_COUNT);
	WRITE_BYTE(MSG_BROADCAST, BREAK_GIB_LIFE);
	WRITE_BYTE(MSG_BROADCAST, BREAK_GLASS);

	pev->solid = SOLID_NOT;
	pev->model = 0;		// no longer sent to the clients

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = pev->ltime;
}

void CGlass::Spawn()
{
	PRECACHE_SOUND("common/glass.wav");
	m_idShard = PRECACHE_MODEL("sprites/shard.spr");

	if (!((int)pev->spawnflags & SF_BREAK_TRIGGER_ONLY))
		pev->takedamage = DAMAGE_YES;
	else
		pev->takedamage = DAMAGE_NO;

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL(ENT(pev), STRING(pev->model));
}

void CGlass::Use(CBaseEntity *pOther)
{
	Die();
}

//=========================================================
// Touch - a player running into it hard enough breaks it
//=========================================================
void CGlass::Touch(CBaseEntity *pOther)
{
	entvars_t *pevToucher = VARS(gpGlobals->other);

	if (!((int)pevToucher->flags & FL_CLIENT))
		return;

	if (pev->takedamage == DAMAGE_NO)
		return;

	float flDamage = pevToucher->velocity.Length() * GLASS_TOUCH_DAMAGE;
	if (flDamage > pev->health)
		TakeDamage(pevToucher, pevToucher, pev->health);
}

//=========================================================
// func_pushable - a crate the player pushes around
//=========================================================
class CPushable : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Touch(CBaseEntity *pOther);

private:
	float	m_maxSpeed;
};

void CPushable::Spawn()
{
	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), STRING(pev->model));

	// the friction leaves it at least 1 unit of speed
	if (pev->friction > PUSHABLE_MAX_SPEED - 1.0f)
		pev->friction = PUSHABLE_MAX_SPEED - 1.0f;

	m_maxSpeed = PUSHABLE_MAX_SPEED - pev->friction;
	pev->friction = 0.0f;
}

//=========================================================
// KeyValue - "size" picks the bounding box
//=========================================================
void CPushable::KeyValue(KeyValueData *pkvd)
{
	if (!FStrEq(pkvd->szKeyName, "size"))
		return;

	int iSize = atoi(pkvd->szValue);
	pkvd->fHandled = TRUE;

	Vector vecMins;
	Vector vecMaxs;

	if (iSize == PUSHABLE_SIZE_BIG)
	{
		vecMaxs = VEC_DUCK_HULL_MAX * 2.0f;
		vecMins = VEC_DUCK_HULL_MIN * 2.0f;
	}
	else if (iSize == PUSHABLE_SIZE_DUCK)
	{
		vecMaxs = VEC_DUCK_HULL_MAX;
		vecMins = VEC_DUCK_HULL_MIN;
	}
	else if (iSize != PUSHABLE_SIZE_POINT)
	{
		vecMaxs = VEC_HULL_MAX;
		vecMins = VEC_HULL_MIN;
	}
	else
	{
		vecMaxs = Vector(8.0f, 8.0f, 8.0f);
		vecMins = Vector(-8.0f, -8.0f, -8.0f);
	}

	UTIL_SetSize(pev, vecMins, vecMaxs);
}

//=========================================================
// Touch - a player walking into it pushes it along the
// ground, always at m_maxSpeed
//=========================================================
void CPushable::Touch(CBaseEntity *pOther)
{
	entvars_t *pevToucher = VARS(gpGlobals->other);

	if (!FClassnameIs(pevToucher, "player"))
		return;

	// not when the player stands on it
	if ((int)pevToucher->flags & FL_ONGROUND)
	{
		if (VARS(pevToucher->groundentity) == pev)
			return;
	}

	pev->velocity.x += pevToucher->velocity.x * PUSHABLE_PUSH_SCALE;
	pev->velocity.y += pevToucher->velocity.y * PUSHABLE_PUSH_SCALE;

	float flSpeed = sqrtf(pev->velocity.y * pev->velocity.y + pev->velocity.x * pev->velocity.x);
	if (flSpeed != 0.0f)
	{
		pev->velocity.x = pev->velocity.x / flSpeed * m_maxSpeed;
		pev->velocity.y = pev->velocity.y / flSpeed * m_maxSpeed;
	}
}

LINK_ENTITY_TO_CLASS(func_breakable, CBreakable);
LINK_ENTITY_TO_CLASS(func_glass, CGlass);
LINK_ENTITY_TO_CLASS(func_pushable, CPushable);
