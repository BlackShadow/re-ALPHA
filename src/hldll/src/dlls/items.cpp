/***
*
*Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*This product contains software technology licensed from Id
*Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

//=========================================================
// Items - Item entities
//=========================================================

#include <new>
#include <stdlib.h>
#include <string.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// item-specific constants
//=========================================================

#define ITEM_RESPAWN_DELAY		0.1f

// entvars_t weapon ownership bitfields (player). These are distinct from
// PEV_WEAPON (272); they store the player's owned-item flags as two 32-bit
// words: items 0..31 in the low word, items 32..63 in the high word.
#define PEV_ITEMS_LOW			276
#define PEV_ITEMS_HIGH			308

// globalvars_t: index of the "other" entity supplied to a Touch callback.
#define GLOBALS_OTHER_ENTINDEX	116

static const char kItemKeyType[] = "type";
static const char kPlayerClassname[] = "player";

//=========================================================
// CItem - generic pickup that grants an item bit to the player
//=========================================================

class CItem : public CBaseEntity
{
public:
	void KeyValue(KeyValueData* pkvd);
	void Touch(CBaseEntity* pOther);

private:
	void RemoveThink(CBaseEntity* pOther);

	int m_iItemType;
};

HL_COMPILE_TIME_ASSERT(sizeof(CItem) <= 32, CItem_private_data_size);

//=========================================================
// ToucherVars - resolve the entvars of the entity that is
// currently touching us (the engine stashes its entity index
// in globalvars_t at GLOBALS_OTHER_ENTINDEX before the call).
//=========================================================
static entvars_t* ToucherVars(void* globals)
{
	if (!globals)
		return NULL;

	int entIndex = *(int*)((uint8_t*)globals + GLOBALS_OTHER_ENTINDEX);
	edict_t* pEdict = EnginePEntityOfEntIndex(entIndex);
	return pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
}

//=========================================================
// KeyValue
//=========================================================
void CItem::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, kItemKeyType) == 0)
	{
		m_iItemType = atol(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// Touch
//=========================================================
void CItem::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	entvars_t* pevPlayer = ToucherVars(globals);
	if (!pevPlayer)
		return;

	const char* pszClassname = EngineStringFromIndex(PevInt(pevPlayer, PEV_CLASSNAME));
	if (!pszClassname || strcmp(pszClassname, kPlayerClassname) != 0)
		return;

	int itemType = m_iItemType;
	if (itemType >= 32)
		PevInt(pevPlayer, PEV_ITEMS_HIGH) |= 1 << (itemType - 32);
	else
		PevInt(pevPlayer, PEV_ITEMS_LOW) |= 1 << itemType;

	SetRemoveThink();
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + ITEM_RESPAWN_DELAY;
}

//=========================================================
// RemoveThink
//=========================================================
void CItem::RemoveThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// item
//=========================================================
DLLEXPORT void item(entvars_t* pev)
{
	entvars_t* entvars = pev;
	if (!entvars)
	{
		edict_t* created = EngineCreateEntity();
		entvars = created ? EngineGetVarsOfEnt(created) : NULL;
	}

	edict_t* edict = EdictFromEntvars(entvars);
	if (!edict)
		return;

	void* privateData = EngineGetPrivateData(edict);
	if (!privateData)
	{
		privateData = EngineAllocPrivateData(edict, 32);
		if (!privateData)
			return;

		memset(privateData, 0, 32);

		CItem* self = new (privateData) CItem();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
