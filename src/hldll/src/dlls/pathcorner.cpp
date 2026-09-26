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
// path_corner - a stop on a monster's patrol path
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

// func_train reads CBaseToggle::m_flWait of its corners, which stays 0
class CPathCorner : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Touch(CBaseEntity *pOther);

private:
	float	m_flCornerWait;		// "wait" key, pausing at a corner is not implemented
};

LINK_ENTITY_TO_CLASS(path_corner, CPathCorner);

void CPathCorner::Spawn()
{
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-8.0f, -8.0f, -8.0f), Vector(8.0f, 8.0f, 8.0f));
}

void CPathCorner::KeyValue(KeyValueData *pkvd)
{
	if (!pkvd)
		return;

	if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flCornerWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// Touch - a monster walking its path reached this corner,
// send it on to the next one
//=========================================================
void CPathCorner::Touch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);

	// only the monster heading for this corner, and only while it has no enemy
	if (gpGlobals->self != pevOther->goalentity)
		return;

	if (!FNullEnt(pevOther->enemy))
		return;

	if (m_flCornerWait != 0.0f)
		ALERT(at_warning, "Non-zero path-cornder waits NYI");

	if (FStringNull(pev->target))
		ALERT(at_warning, "PathCornerTouch: no next stop specified");

	const char *pszNextName = STRING(pev->target);
	edict_t *pentNext = FIND_ENTITY_BY_STRING(NULL, "targetname", pszNextName);
	EOFFSET eoffsetNext = OFFSET(pentNext);

	pevOther->goalentity = eoffsetNext;

	if (eoffsetNext)
	{
		// face the next corner
		entvars_t *pevGoal = VARS(eoffsetNext);
		pevOther->ideal_yaw = UTIL_VecToYaw(pevGoal->origin - pevOther->origin);
	}
	else
	{
		const char *pszTarget = STRING(pev->target);
		const char *pszClassname = STRING(pev->classname);
		ALERT(at_error, "PathCornerTouch--%s couldn't find next stop in path: %s", pszClassname, pszTarget);
	}
}
