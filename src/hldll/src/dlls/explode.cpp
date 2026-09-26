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
// Explode - particle burst, armed and timed by the
// entities that fire it
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

// particle counts of the burst
#define EXPLODE_COUNT			100
#define EXPLODE_COUNT_LATE		800		// after m_iExplodeValue seconds
#define EXPLODE_COUNT_HEIGHT	225		// before m_iLowTime, with m_fUseHeight
#define EXPLODE_COUNT_LOW		400		// from m_iLowTime on

// only the low byte reaches the client, which makes it palette color 0
#define EXPLODE_COLOR			65280

// start time without "timers": started, but too late for the timed counts
#define EXPLODE_NOTIMER_START	100000.0f

class CExplode : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pOther);

	void ExplodeUse(CBaseEntity *pOther);

private:
	BOOL		m_fUseHeight;		// always FALSE, nothing sets it
	int			m_iLowTime;			// always 0, nothing sets it
	int			m_iExplodeValue;	// seconds, from "explodeMin", "explodeMod" or "explodeMax"
	float		m_flStartTime;		// set by the "timers" entity, 0 until then
	BOOL		m_fArmed;			// set by the "triggers" entity
	string_t	m_iszTriggers;
	string_t	m_iszTimers;
};

void CExplode::Spawn()
{
	if (FStringNull(m_iszTriggers))
		m_fArmed = TRUE;

	if (FStringNull(m_iszTimers))
		m_flStartTime = EXPLODE_NOTIMER_START;

	pev->nextthink = pev->ltime + 1.0f;

	// never called: CExplode::Use replaces the use dispatch
	SetUse(&CExplode::ExplodeUse);
}

void CExplode::ExplodeUse(CBaseEntity *pOther)
{
	Use(pOther);
}

void CExplode::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "triggers"))
	{
		m_iszTriggers = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "timers"))
	{
		m_iszTimers = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "explodeMin")
		|| FStrEq(pkvd->szKeyName, "explodeMod")
		|| FStrEq(pkvd->szKeyName, "explodeMax"))
	{
		m_iExplodeValue = atol(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "style")
		|| FStrEq(pkvd->szKeyName, "height")
		|| FStrEq(pkvd->szKeyName, "killtarget")
		|| FStrEq(pkvd->szKeyName, "value1")
		|| FStrEq(pkvd->szKeyName, "value2")
		|| FStrEq(pkvd->szKeyName, "value3"))
	{
		// accepted, but not used
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// Use - the entity that fires us (gpGlobals->other) arms
// the burst when it is named by "triggers" and starts the
// timer when it is named by "timers". Once both are done,
// every use makes a burst that grows with the time elapsed.
//=========================================================
void CExplode::Use(CBaseEntity *pOther)
{
	EOFFSET eoffsetActivator = gpGlobals->other;

	if (!FStringNull(m_iszTriggers))
	{
		edict_t *pentActivator = ENT(eoffsetActivator);
		const char *pszTrigger = STRING(m_iszTriggers);
		entvars_t *pevActivator = VARS(pentActivator);

		if (FStrEq(STRING(pevActivator->targetname), pszTrigger))
			m_fArmed = TRUE;
	}

	BOOL fStartTimer = FALSE;
	if (!FStringNull(m_iszTimers))
	{
		edict_t *pentActivator = ENT(eoffsetActivator);
		const char *pszTimer = STRING(m_iszTimers);
		entvars_t *pevActivator = VARS(pentActivator);

		if (FStrEq(STRING(pevActivator->targetname), pszTimer))
			fStartTimer = TRUE;
	}

	if (fStartTimer)
	{
		if (m_flStartTime == 0.0f)
			m_flStartTime = gpGlobals->time;
	}

	if (m_flStartTime == 0.0f)
		return;

	if (!m_fArmed)
		return;

	float flElapsed = gpGlobals->time - m_flStartTime;

	int iCount = EXPLODE_COUNT;

	if ((float)m_iExplodeValue < flElapsed)
		iCount = EXPLODE_COUNT_LATE;

	if ((float)m_iLowTime > flElapsed)
	{
		if (m_fUseHeight)
			iCount = EXPLODE_COUNT_HEIGHT;
	}
	else
	{
		iCount = EXPLODE_COUNT_LOW;
	}

	PARTICLE_EFFECT(pev->origin, pev->angles, (float)EXPLODE_COLOR, (float)iCount);
}

LINK_ENTITY_TO_CLASS(explode, CExplode);
