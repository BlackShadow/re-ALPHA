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
// Items - item entities
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"

//=========================================================
// item - gives the player the item of its "type"
//=========================================================
class CItem : public CBaseEntity
{
public:
	void KeyValue(KeyValueData *pkvd);
	void Touch(CBaseEntity *pOther);

private:
	int		m_iItemType;
};

LINK_ENTITY_TO_CLASS(item, CItem);

void CItem::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_iItemType = atol(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

void CItem::Touch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);

	// only the player picks up items
	if (!FClassnameIs(pevOther, "player"))
		return;

	if (m_iItemType >= MAX_WEAPONS)
		pevOther->items |= 1 << (m_iItemType - MAX_WEAPONS);
	else
		pevOther->weapons |= 1 << m_iItemType;

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1f;
}
