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
// Weapons - decal sprayers and dropped items, made by the
// player
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "decals.h"
#include "weapons.h"

#define SPRAY_INTERVAL		0.1f

//=========================================================
// SprayDecal - puts a decal on the wall in front of the
// sprayer
//=========================================================
static void SprayDecal(entvars_t *pev, int decalNumber)
{
	UTIL_MakeVectors(pev->angles);

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 128.0f, ignore_monsters, ENT(pev->owner), &tr);

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_DECAL);
	WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.x);
	WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.y);
	WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.z);
	WRITE_SHORT(MSG_BROADCAST, ENTINDEX(tr.pHit));
	WRITE_BYTE(MSG_BROADCAST, decalNumber);
}

// the sprayer starts above the owner and aims where the owner aims
static void SprayerInit(entvars_t *pev, entvars_t *pevOwner)
{
	pev->origin.x = pevOwner->origin.x;
	pev->origin.y = pevOwner->origin.y;
	pev->origin.z = pevOwner->origin.z + 32.0f;

	pev->angles = pevOwner->v_angle;
	pev->owner = OFFSET(pevOwner);
}

//=========================================================
// Lambda sprayer - sprays the six lambda decals
//=========================================================
class CLambdaSprayer : public CBaseEntity
{
public:
	void Init(entvars_t *pevOwner);
	void SprayThink(CBaseEntity *pOther);

	unsigned char	m_iSprayCount;
};

void CLambdaSprayer::Init(entvars_t *pevOwner)
{
	SprayerInit(pev, pevOwner);

	m_iSprayCount = 0;
	SetThink(&CLambdaSprayer::SprayThink);
	pev->nextthink = gpGlobals->time + SPRAY_INTERVAL;

	EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/sprayer.wav", VOL_NORM, ATTN_NORM);
}

void CLambdaSprayer::SprayThink(CBaseEntity *pOther)
{
	SprayDecal(pev, DECAL_LAMBDA1 + m_iSprayCount);

	// that was the last one
	if (DECAL_LAMBDA1 + m_iSprayCount >= DECAL_LAMBDA6)
		SetThink(&CBaseEntity::SUB_Remove);

	m_iSprayCount++;
	pev->nextthink = gpGlobals->time + SPRAY_INTERVAL;
}

void SprayLambdas(entvars_t *pevOwner)
{
	CLambdaSprayer *pSprayer = GetClassPtr((CLambdaSprayer *)NULL);
	pSprayer->Init(pevOwner);
}

//=========================================================
// Blood sprayer - sprays one random blood decal
//=========================================================
class CBloodSprayer : public CBaseEntity
{
public:
	void Init(entvars_t *pevOwner);
	void SprayThink(CBaseEntity *pOther);
};

void CBloodSprayer::Init(entvars_t *pevOwner)
{
	SprayerInit(pev, pevOwner);

	SetThink(&CBloodSprayer::SprayThink);
	pev->nextthink = gpGlobals->time + SPRAY_INTERVAL;
}

void CBloodSprayer::SprayThink(CBaseEntity *pOther)
{
	SprayDecal(pev, RANDOM_LONG(DECAL_BLOOD1, DECAL_BLOOD6));

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + SPRAY_INTERVAL;
}

void SprayBlood(entvars_t *pevOwner)
{
	CBloodSprayer *pSprayer = GetClassPtr((CBloodSprayer *)NULL);
	pSprayer->Init(pevOwner);
}

//=========================================================
// Dropped item - gives its item back to the player that
// picks it up
//=========================================================
class CDroppedItem : public CBaseEntity
{
public:
	void Init(int iItem, const Vector &vecOrigin);
	void KeyValue(KeyValueData *pkvd);
	void Touch(CBaseEntity *pOther);

	int		m_iItem;
};

void CDroppedItem::Init(int iItem, const Vector &vecOrigin)
{
	m_iItem = iItem;

	pev->movetype = MOVETYPE_TOSS;
	pev->classname = ALLOC_STRING("Item");
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/shell.mdl");
	UTIL_SetSize(pev, Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 16.0f));

	pev->origin = vecOrigin;
}

void CDroppedItem::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_iItem = atol(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

void CDroppedItem::Touch(CBaseEntity *pOther)
{
	if (FNullEnt(gpGlobals->other))
		return;

	entvars_t *pevOther = VARS(gpGlobals->other);

	// only the player picks up items
	if (!FClassnameIs(pevOther, "player"))
		return;

	if (m_iItem >= MAX_WEAPONS)
		pevOther->items |= 1 << (m_iItem - MAX_WEAPONS);
	else
		pevOther->weapons |= 1 << m_iItem;

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1f;
}

//=========================================================
// DropItem - drops the item 64 units in front of the owner,
// or behind when there is a wall
//=========================================================
CBaseEntity *DropItem(entvars_t *pevOwner, int iItem)
{
	CDroppedItem *pItem = GetClassPtr((CDroppedItem *)NULL);

	UTIL_MakeVectors(pevOwner->v_angle);

	Vector vecEye = pevOwner->origin + pevOwner->view_ofs;
	Vector vecDrop = vecEye + gpGlobals->v_forward * 64.0f;

	if (POINT_CONTENTS(vecDrop) == CONTENTS_SOLID)
	{
		vecDrop = vecEye - gpGlobals->v_forward * 64.0f;

		if (POINT_CONTENTS(vecDrop) == CONTENTS_SOLID)
			return NULL;
	}

	pItem->Init(iItem, vecDrop);
	return pItem;
}
