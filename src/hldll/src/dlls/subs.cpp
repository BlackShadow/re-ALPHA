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
// Subs - frequently used global functions and the
// delay / toggle base entities
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

// Landmark class
class CPointEntity : public CBaseEntity
{
};

LINK_ENTITY_TO_CLASS(info_player_start, CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_deathmatch, CPointEntity);

// Null Entity, remove on startup
class CNullEntity : public CBaseEntity
{
public:
	void Spawn();
};

void CNullEntity::Spawn()
{
	REMOVE_ENTITY(ENT(pev));
}

LINK_ENTITY_TO_CLASS(info_null, CNullEntity);

// Level transition landmark, a point found by name
class CInfoLandmark : public CBaseEntity
{
public:
	void Spawn();
};

void CInfoLandmark::Spawn()
{
	pev->solid = SOLID_BBOX;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}

LINK_ENTITY_TO_CLASS(info_landmark, CInfoLandmark);

// Stair hints for monster navigation
class CHintStairs : public CBaseEntity
{
public:
	void Spawn();
	void Think(CBaseEntity *pOther);
	void HintThink(CBaseEntity *pOther);
};

void CHintStairs::Spawn()
{
	pev->solid = SOLID_NOT;
	SetThink(&CHintStairs::HintThink);
}

void CHintStairs::Think(CBaseEntity *pOther)
{
}

void CHintStairs::HintThink(CBaseEntity *pOther)
{
	Think(pOther);
}

LINK_ENTITY_TO_CLASS(hint_stairsup, CHintStairs);
LINK_ENTITY_TO_CLASS(hint_stairsdown, CHintStairs);

//=========================================================
// CBaseDelay
//=========================================================
CBaseDelay::CBaseDelay()
{
	m_flDelay = 0.0f;
	m_iszKillTarget = 0;
}

void CBaseDelay::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "delay"))
	{
		m_flDelay = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget"))
	{
		m_iszKillTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseEntity::KeyValue(pkvd);
	}
}

//=========================================================
// SUB_UseTargets - fires pev->target, or removes the entities
// named by m_iszKillTarget. With a delay, a temporary entity
// does it once the delay has passed.
//
// The targets' Use() find the activator in gpGlobals->other.
//=========================================================
void CBaseDelay::SUB_UseTargets()
{
	if (m_flDelay != 0.0f)
	{
		CBaseDelay *pTemp = GetClassPtr((CBaseDelay *)NULL);

		pTemp->pev->classname = ALLOC_STRING("DelayedUse");
		pTemp->pev->nextthink = gpGlobals->time + m_flDelay;
		pTemp->SetThink(&CBaseDelay::DelayThink);
		pTemp->m_iszKillTarget = m_iszKillTarget;
		pTemp->m_flDelay = 0.0f;	// prevent "recursion"
		pTemp->pev->target = pev->target;
		return;
	}

	// kill the killtargets
	if (m_iszKillTarget)
	{
		const char *pszKillTarget = STRING(m_iszKillTarget);
		edict_t *pentKillTarget = NULL;

		while ((pentKillTarget = FIND_ENTITY_BY_STRING(pentKillTarget, "targetname", pszKillTarget)) != NULL)
		{
			// the search ends at the world
			if (FNullEnt(pentKillTarget))
				break;

			REMOVE_ENTITY(pentKillTarget);
		}
		return;
	}

	// fire targets
	if (FStringNull(pev->target))
		return;

	EOFFSET eoffsetSelf = gpGlobals->self;
	EOFFSET eoffsetOther = gpGlobals->other;

	const char *pszTarget = STRING(pev->target);
	edict_t *pentTarget = NULL;

	while ((pentTarget = FIND_ENTITY_BY_STRING(pentTarget, "targetname", pszTarget)) != NULL)
	{
		// the search ends at the world
		if (FNullEnt(pentTarget))
			break;

		gpGlobals->self = OFFSET(pentTarget);
		gpGlobals->other = eoffsetSelf;

		CBaseEntity *pTarget = GetClassPtr((CBaseEntity *)VARS(pentTarget));
		pTarget->Use(NULL);

		gpGlobals->self = eoffsetSelf;
		gpGlobals->other = eoffsetOther;
	}
}

void CBaseDelay::DelayThink(CBaseEntity *pOther)
{
	// m_flDelay is zero now, so this fires right away
	SUB_UseTargets();
	REMOVE_ENTITY(ENT(pev));
}

//=========================================================
// SetMovedir - turns pev->angles into the pev->movedir unit
// vector. Angles of (0 -1 0) and (0 -2 0) mean straight up
// and straight down.
//=========================================================
void SetMovedir(entvars_t *pev)
{
	if (pev->angles == Vector(0.0f, -1.0f, 0.0f))
	{
		pev->movedir = Vector(0.0f, 0.0f, 1.0f);
	}
	else if (pev->angles == Vector(0.0f, -2.0f, 0.0f))
	{
		pev->movedir = Vector(0.0f, 0.0f, -1.0f);
	}
	else
	{
		UTIL_MakeVectors(pev->angles);
		pev->movedir = gpGlobals->v_forward;
	}

	pev->angles = g_vecZero;
}

//=========================================================
// CBaseToggle
//=========================================================
CBaseToggle::CBaseToggle()
{
	m_toggle_state = TS_AT_TOP;
	m_flActivateFinished = 0.0f;
	m_flMoveDistance = 0.0f;
	m_flWait = 0.0f;
	m_flLip = 0.0f;
	m_flTWidth = 0.0f;
	m_flTLength = 0.0f;
	m_vecPosition1 = g_vecZero;
	m_vecPosition2 = g_vecZero;
	m_cTriggersLeft = 0;
	m_flHeight = 0.0f;
	m_hActivator = 0;
	m_pfnCallWhenMoveDone = &CBaseEntity::SUB_DoNothing;
	m_vecFinalDest = g_vecZero;
	m_vecFinalAngle = g_vecZero;
}

void CBaseToggle::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "skin"))
	{
		pev->skin = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_bMoveSnd = (unsigned char)(int)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsnd"))
	{
		m_bStopSnd = (unsigned char)(int)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
		m_bHealthValue = (unsigned char)(int)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// LinearMove - calculate pev->velocity and pev->nextthink to
// reach vecDest from pev->origin traveling at flSpeed
//=========================================================
void CBaseToggle::LinearMove(const Vector &vecDest, float flSpeed)
{
	m_vecFinalDest = vecDest;

	// Already there?
	if (pev->origin == vecDest)
	{
		LinearMoveDone(NULL);
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - pev->origin;

	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// too short a move to bother gliding
	if (flTravelTime < 0.1f)
	{
		LinearMoveDone(NULL);
		return;
	}

	// set nextthink to trigger a call to LinearMoveDone when dest is reached
	pev->nextthink = pev->ltime + flTravelTime;
	SetThink(&CBaseToggle::LinearMoveDone);

	// scale the destdelta vector by the time spent traveling to get velocity
	pev->velocity = vecDestDelta / flTravelTime;
}

//=========================================================
// After moving, set origin to exact final destination,
// call "move done" function
//=========================================================
void CBaseToggle::LinearMoveDone(CBaseEntity *pOther)
{
	UTIL_SetOrigin(pev, m_vecFinalDest);
	pev->velocity = g_vecZero;
	pev->nextthink = -1.0f;

	(this->*m_pfnCallWhenMoveDone)(NULL);
}

//=========================================================
// AngularMove - calculate pev->avelocity and pev->nextthink
// to reach vecDestAngle from pev->angles rotating at flSpeed
//=========================================================
void CBaseToggle::AngularMove(const Vector &vecDestAngle, float flSpeed)
{
	m_vecFinalAngle = vecDestAngle;

	// Already there?
	if (pev->angles == vecDestAngle)
	{
		AngularMoveDone(NULL);
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDestAngle - pev->angles;

	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// too short a move to bother rotating
	if (flTravelTime < 0.1f)
	{
		AngularMoveDone(NULL);
		return;
	}

	// set nextthink to trigger a call to AngularMoveDone when dest is reached
	pev->nextthink = pev->ltime + flTravelTime;
	SetThink(&CBaseToggle::AngularMoveDone);

	// scale the destdelta vector by the time spent traveling to get velocity
	pev->avelocity = vecDestDelta / flTravelTime;
}

//=========================================================
// After rotating, set angle to exact final angle, call
// "move done" function
//=========================================================
void CBaseToggle::AngularMoveDone(CBaseEntity *pOther)
{
	pev->angles = m_vecFinalAngle;
	pev->avelocity = g_vecZero;
	pev->nextthink = -1.0f;

	(this->*m_pfnCallWhenMoveDone)(NULL);
}

//=========================================================
// InitTrigger - sets up a brush entity as an invisible
// trigger volume
//=========================================================
void CBaseTrigger::InitTrigger()
{
	// trigger angles are used for one-way touches
	if (pev->angles != g_vecZero)
		SetMovedir(pev);

	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), STRING(pev->model));	// set size and link into world
	pev->movetype = MOVETYPE_NONE;
	pev->modelindex = 0;
	pev->model = 0;
}
