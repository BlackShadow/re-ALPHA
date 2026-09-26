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
// CBaseEntity - base entity behavior
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

CBaseEntity::CBaseEntity()
{
	pev = NULL;
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
}

int CBaseEntity::ObjectCaps()
{
	return 0;
}

int CBaseEntity::Save(SAVERESTOREDATA *pSaveData)
{
	ALERT(at_console, "Saving %s\n", STRING(pev->classname));
	return 0;
}

int CBaseEntity::Restore(SAVERESTOREDATA *pSaveData)
{
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
}

void CBaseEntity::Death(int iDeathType)
{
}

void CBaseEntity::IdleSound()
{
}

int CBaseEntity::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	return 0;
}

int CBaseEntity::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage)
{
	return 0;
}

void CBaseEntity::SUB_Remove(CBaseEntity *pOther)
{
	REMOVE_ENTITY(ENT(pev));
}

void CBaseEntity::SUB_DoNothing(CBaseEntity *pOther)
{
}
