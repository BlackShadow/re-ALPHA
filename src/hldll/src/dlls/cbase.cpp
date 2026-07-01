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
// CBaseEntity - Base entity implementation
//=========================================================

#include "cbase.h"
#include "utils.h"
#include "enginefuncs.h"

CBaseEntity::CBaseEntity()
{
	pev = NULL;
	m_pGlobals = NULL;
	m_pfnThink = NULL;
	m_pfnTouch = NULL;
	m_pfnUse = NULL;
	m_pfnBlocked = NULL;
}

void CBaseEntity::Spawn()
{
}

void CBaseEntity::KeyValue(KeyValueData *pkvd)
{
	HL_UNUSED(pkvd);
}

int CBaseEntity::ObjectCaps()
{
	return 0;
}

int CBaseEntity::Save(SAVERESTOREDATA *pSaveData)
{
	HL_UNUSED(pSaveData);

	EngineAlertMessage(1, "Saving %s\n", EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME)));
	return 0;
}

int CBaseEntity::Restore(SAVERESTOREDATA *pSaveData)
{
	HL_UNUSED(pSaveData);
	return 0;
}

void CBaseEntity::Think(CBaseEntity *pOther)
{
	if (m_pfnThink)
		(this->*m_pfnThink)(pOther);
}

void CBaseEntity::Touch(CBaseEntity *pOther)
{
	if (m_pfnTouch)
		(this->*m_pfnTouch)(pOther);
}

void CBaseEntity::Use(CBaseEntity *pOther)
{
	if (m_pfnUse)
		(this->*m_pfnUse)(pOther);
}

void CBaseEntity::Blocked(CBaseEntity *pOther)
{
	if (m_pfnBlocked)
		(this->*m_pfnBlocked)(pOther);
}

int CBaseEntity::Classify()
{
	return 0;
}

void CBaseEntity::SetActivity(int activity)
{
	HL_UNUSED(activity);
}

int CBaseEntity::BloodColor()
{
	return 0;
}

void CBaseEntity::AlertSound()
{
}

void CBaseEntity::Pain(float flDamage)
{
	HL_UNUSED(flDamage);
}

void CBaseEntity::Death(int gibType)
{
	HL_UNUSED(gibType);
}

void CBaseEntity::IdleSound()
{
}

int CBaseEntity::CheckAttacks(entvars_t* pevEnemy, float flDist)
{
	HL_UNUSED(pevEnemy);
	HL_UNUSED(flDist);
	return 0;
}

int CBaseEntity::TakeDamage(entvars_t *inflictor, entvars_t *attacker, float damage)
{
	HL_UNUSED(inflictor);
	HL_UNUSED(attacker);
	HL_UNUSED(damage);

	return 0;
}

void CBaseEntity::DoNothingThink(CBaseEntity *pOther)
{
	HL_UNUSED(pOther);
}

void CBaseEntity::DoNothingUse(CBaseEntity *pOther)
{
	HL_UNUSED(pOther);
}

void CBaseEntity::DoNothingTouch(CBaseEntity *pOther)
{
	HL_UNUSED(pOther);
}

float CBaseEntity::GlobalTime() const
{
	void *globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	return GlobalsTime(globals);
}

void CBaseEntity::SetNextThink(float delay)
{
	if (!pev)
		return;

	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + delay;
}

void CBaseEntity::SetRemoveThink()
{
	SetThink(&CBaseEntity::RemoveThink);
}

void CBaseEntity::RemoveThink(CBaseEntity *pOther)
{
	HL_UNUSED(pOther);

	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

CBaseEntity *GetEntity(edict_t *pent)
{
	if (!pent)
		return NULL;

	// The original DLL never reads edict->pvPrivateData directly; it always goes
	// through the engine GET_PRIVATE (pfnPvEntPrivateData, table slot 0xD0). The
	// DLL must not assume the engine's private edict_t layout, so mirror that.
	return (CBaseEntity *)EngineGetPrivateData(pent);
}
