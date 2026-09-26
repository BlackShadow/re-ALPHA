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
// Plats - func_plat elevators, func_train path followers
// and the train_info test marker
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define PLAT_DEFAULT_SPEED		150.0f
#define PLAT_DEFAULT_TLENGTH	80.0f
#define PLAT_DEFAULT_TWIDTH		10.0f
#define PLAT_LOWERED_LIP		8.0f	// a lowered plat stays this far above its spawn bottom
#define PLAT_TOP_WAIT			3.0f	// time at the top before going back down
#define PLAT_RIDER_WAIT			1.0f	// a player at the top keeps it there this much longer
#define PLAT_CRUSH_DAMAGE		1.0f

#define PLAT_TRIGGER_BORDER		25.0f	// the trigger field is moved this far along x and y
#define PLAT_TRIGGER_HEIGHT		8.0f	// and reaches this far above the plat
#define PLAT_TRIGGER_MIN_SIZE	50.0f	// on a plat this narrow the field is 1 unit thin

#define TRAIN_DEFAULT_SPEED		100.0f
#define TRAIN_DEFAULT_DMG		2.0f
#define TRAIN_DMG_INTERVAL		0.5f	// minimum time between two crush damages
#define TRAIN_START_DELAY		0.1
#define TRAIN_PARKED_WAIT		999999.0f	// a parked train waits for Use

#define SF_CORNER_WAITFORTRIG	1		// path_corner: the train parks here until used again

#define TRAIN_INFO_SPIN			150.0f	// yaw speed given to whatever touches a train_info

//=========================================================
// func_plat - an elevator that rises and lowers between
// two heights
//=========================================================
class CFuncPlat : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Blocked(CBaseEntity *pOther);

	void PlatUse(CBaseEntity *pOther);

	void GoUp();
	void GoDown(CBaseEntity *pOther);
	void HitTop(CBaseEntity *pOther);
	void HitBottom(CBaseEntity *pOther);

	void SetupTrigger();

	Vector	m_vecSavedAngles;	// pev->angles from the map, the plat itself never turns
};

LINK_ENTITY_TO_CLASS(func_plat, CFuncPlat);

//=========================================================
// The invisible field around a plat that moves it when a
// player steps on
//=========================================================
class CPlatTrigger : public CBaseEntity
{
public:
	void SpawnInsideTrigger(CFuncPlat *pPlatform);
	void Touch(CBaseEntity *pOther);

	CFuncPlat	*m_pPlatform;
};

//=========================================================
// SpawnInsideTrigger - makes the field a trigger box that
// covers the plat's whole travel
//=========================================================
void CPlatTrigger::SpawnInsideTrigger(CFuncPlat *pPlatform)
{
	m_pPlatform = pPlatform;

	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;

	entvars_t *pevPlat = pPlatform->pev;

	Vector vecTMin = pevPlat->mins + Vector(PLAT_TRIGGER_BORDER, PLAT_TRIGGER_BORDER, 0.0f);
	Vector vecTMax = pevPlat->maxs + Vector(PLAT_TRIGGER_BORDER, PLAT_TRIGGER_BORDER, PLAT_TRIGGER_HEIGHT);
	vecTMin.z = vecTMax.z - (pPlatform->m_vecPosition1.z - pPlatform->m_vecPosition2.z + PLAT_TRIGGER_HEIGHT);

	if (pevPlat->size.x <= PLAT_TRIGGER_MIN_SIZE)
	{
		vecTMin.x = (pevPlat->mins.x + pevPlat->maxs.x) * 0.5f;
		vecTMax.x = vecTMin.x + 1.0f;
	}

	if (pevPlat->size.y <= PLAT_TRIGGER_MIN_SIZE)
	{
		vecTMin.y = (pevPlat->maxs.y + pevPlat->mins.y) * 0.5f;
		vecTMax.y = vecTMin.y + 1.0f;
	}

	UTIL_SetSize(pev, vecTMin, vecTMax);
}

//=========================================================
// Touch - a live player on the plat sends it up, or keeps
// it at the top a little longer
//=========================================================
void CPlatTrigger::Touch(CBaseEntity *pOther)
{
	EOFFSET eoffsetOther = gpGlobals->other;
	if (FNullEnt(eoffsetOther))
		return;

	entvars_t *pevOther = VARS(eoffsetOther);
	if (!FClassnameIs(pevOther, "player"))
		return;

	if (pevOther->health <= 0.0f)
		return;

	CFuncPlat *pPlatform = m_pPlatform;

	if (pPlatform->m_toggle_state == TS_AT_BOTTOM)
		pPlatform->GoUp();
	else if (pPlatform->m_toggle_state == TS_AT_TOP)
		pPlatform->pev->nextthink = pPlatform->pev->ltime + PLAT_RIDER_WAIT;
}

void CFuncPlat::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_flHeight = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// SetupTrigger - creates the CPlatTrigger field around
// the plat
//=========================================================
void CFuncPlat::SetupTrigger()
{
	CPlatTrigger *pTrigger = GetClassPtr((CPlatTrigger *)NULL);
	pTrigger->SpawnInsideTrigger(this);
}

void CFuncPlat::Spawn()
{
	PRECACHE_SOUND("plats/platmove1.wav");
	PRECACHE_SOUND("plats/platstop1.wav");

	pev->noise = ALLOC_STRING("plats/platmove1.wav");
	pev->noise1 = ALLOC_STRING("plats/platstop1.wav");

	if (m_flTLength == 0.0f)
		m_flTLength = PLAT_DEFAULT_TLENGTH;

	if (m_flTWidth == 0.0f)
		m_flTWidth = PLAT_DEFAULT_TWIDTH;

	m_vecSavedAngles = pev->angles;
	pev->angles = g_vecZero;

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	SET_MODEL(ENT(pev), STRING(pev->model));

	if (pev->speed == 0.0f)
		pev->speed = PLAT_DEFAULT_SPEED;

	// m_vecPosition1 is the top position, m_vecPosition2 is the bottom
	m_vecPosition1 = pev->origin;
	m_vecPosition2 = pev->origin;

	if (m_flHeight != 0.0f)
		m_vecPosition2.z = pev->origin.z - m_flHeight;
	else
		m_vecPosition2.z = pev->origin.z - pev->size.z + PLAT_LOWERED_LIP;

	SetupTrigger();

	if (!FStringNull(pev->targetname))
	{
		// a named plat waits at the top until it is used
		UTIL_SetOrigin(pev, m_vecPosition1);
		m_toggle_state = TS_AT_TOP;
		SetUse(&CFuncPlat::PlatUse);
	}
	else
	{
		UTIL_SetOrigin(pev, m_vecPosition2);
		m_toggle_state = TS_AT_BOTTOM;
	}
}

//=========================================================
// PlatUse - the first use lowers a plat waiting at the top
//=========================================================
void CFuncPlat::PlatUse(CBaseEntity *pOther)
{
	SetUse(NULL);

	if (m_toggle_state == TS_AT_TOP)
		GoDown(NULL);
}

//=========================================================
// GoDown - start moving to the bottom position
//=========================================================
void CFuncPlat::GoDown(CBaseEntity *pOther)
{
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING(pev->noise), VOL_NORM, ATTN_NORM);

	m_toggle_state = TS_GOING_DOWN;
	SetMoveDone(&CFuncPlat::HitBottom);
	LinearMove(m_vecPosition2, pev->speed);
}

//=========================================================
// GoUp - start moving to the top position
//=========================================================
void CFuncPlat::GoUp()
{
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING(pev->noise), VOL_NORM, ATTN_NORM);

	m_toggle_state = TS_GOING_UP;
	SetMoveDone(&CFuncPlat::HitTop);
	LinearMove(m_vecPosition1, pev->speed);
}

//=========================================================
// HitTop - reached the top, go back down after a while
//=========================================================
void CFuncPlat::HitTop(CBaseEntity *pOther)
{
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING(pev->noise1), VOL_NORM, ATTN_NORM);

	m_toggle_state = TS_AT_TOP;
	SetThink(&CFuncPlat::GoDown);
	pev->nextthink = pev->ltime + PLAT_TOP_WAIT;
}

//=========================================================
// HitBottom - reached the bottom
//=========================================================
void CFuncPlat::HitBottom(CBaseEntity *pOther)
{
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING(pev->noise1), VOL_NORM, ATTN_NORM);

	m_toggle_state = TS_AT_BOTTOM;
}

//=========================================================
// Blocked - hurt the blocker and go back the other way
//=========================================================
void CFuncPlat::Blocked(CBaseEntity *pOther)
{
	CBaseEntity *pBlocker = CBaseEntity::Instance(gpGlobals->other);
	if (pBlocker)
		pBlocker->TakeDamage(pev, pev, PLAT_CRUSH_DAMAGE);

	if (m_toggle_state == TS_GOING_DOWN)
		GoUp();
	else if (m_toggle_state == TS_GOING_UP)
		GoDown(NULL);
}

//=========================================================
// func_train - moves along a chain of path_corners
//=========================================================
class CFuncTrain : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Blocked(CBaseEntity *pOther);
	void Use(CBaseEntity *pOther);

	void Wait(CBaseEntity *pOther);
	void Next(CBaseEntity *pOther);
	void Arrived(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(func_train, CFuncTrain);

void CFuncTrain::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_flHeight = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// Wait - the first think: jump to the first path_corner and
// start, unless the train is named and waits to be used
//=========================================================
void CFuncTrain::Wait(CBaseEntity *pOther)
{
	edict_t *pentTarg = FIND_ENTITY_BY_STRING(NULL, "targetname", STRING(pev->target));
	entvars_t *pevTarg = VARS(pentTarg);

	pev->target = pevTarg->target;
	UTIL_SetOrigin(pev, pevTarg->origin - (pev->mins + pev->maxs) * 0.5f);

	if (FStringNull(pev->targetname))
	{
		pev->nextthink = pev->ltime + TRAIN_START_DELAY;
		SetThink(&CFuncTrain::Next);
	}
}

//=========================================================
// Next - move on to the path_corner named by pev->target
//=========================================================
void CFuncTrain::Next(CBaseEntity *pOther)
{
	edict_t *pentTarg = FIND_ENTITY_BY_STRING(NULL, "targetname", STRING(pev->target));
	if (FNullEnt(pentTarg))
	{
		ALERT(at_error, "TrainNext--cannot find next target '%s'", STRING(pev->target));
		return;
	}

	entvars_t *pevTarg = VARS(pentTarg);

	CBaseToggle *pTarg = GetClassPtr((CBaseToggle *)pevTarg);

	pev->target = pevTarg->target;

	if (FStringNull(pev->target))
	{
		ALERT(at_error, "TrainNext: No next target");
		return;
	}

	// take over the target's wait
	m_flWait = pTarg->m_flWait;

	EMIT_SOUND(ENT(pev), CHAN_VOICE, STRING(pev->noise1), VOL_NORM, ATTN_NORM);

	// remember the corner for Arrived
	pev->enemy = OFFSET(pevTarg);

	SetMoveDone(&CFuncTrain::Arrived);
	LinearMove(pevTarg->origin - (pev->mins + pev->maxs) * 0.5f, pev->speed);

	// a corner with a speed sets the speed of the following legs
	if (pevTarg->speed != 0.0f)
		pev->speed = pevTarg->speed;
}

//=========================================================
// Arrived - reached the corner: wait there, go straight on,
// or park until used again
//=========================================================
void CFuncTrain::Arrived(CBaseEntity *pOther)
{
	entvars_t *pevCorner = VARS(pev->enemy);

	if (!((int)pevCorner->spawnflags & SF_CORNER_WAITFORTRIG))
	{
		if (m_flWait > 0.0f)
		{
			pev->nextthink = pev->ltime + m_flWait;
			EMIT_SOUND(ENT(pev), CHAN_VOICE, STRING(pev->noise), VOL_NORM, ATTN_NORM);
			SetThink(&CFuncTrain::Next);
			return;
		}

		if (m_flWait == 0.0f)
		{
			Next(NULL);
			return;
		}
	}
	else
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, STRING(pev->noise), VOL_NORM, ATTN_NORM);
		pev->nextthink = gpGlobals->time + TRAIN_PARKED_WAIT;
		SetThink(&CFuncTrain::Wait);
	}
}

//=========================================================
// Use - starts a parked train
//=========================================================
void CFuncTrain::Use(CBaseEntity *pOther)
{
	if (m_pfnThink == &CFuncTrain::Wait)
		Next(NULL);
}

//=========================================================
// Blocked - hurt the blocker, at most every
// TRAIN_DMG_INTERVAL seconds
//=========================================================
void CFuncTrain::Blocked(CBaseEntity *pOther)
{
	float flTime = gpGlobals->time;

	if (flTime >= m_flActivateFinished)
	{
		m_flActivateFinished = flTime + TRAIN_DMG_INTERVAL;

		CBaseEntity *pBlocker = CBaseEntity::Instance(gpGlobals->other);
		if (pBlocker)
			pBlocker->TakeDamage(pev, pev, pev->dmg);
	}
}

void CFuncTrain::Spawn()
{
	if (pev->speed == 0.0f)
		pev->speed = TRAIN_DEFAULT_SPEED;

	if (FStringNull(pev->target))
		ALERT(at_error, "FuncTrain with no target");

	if (pev->dmg == 0.0f)
		pev->dmg = TRAIN_DEFAULT_DMG;

	PRECACHE_SOUND("plats/train2.wav");
	PRECACHE_SOUND("plats/train1.wav");

	pev->noise = ALLOC_STRING("plats/train2.wav");		// stop sound
	pev->noise1 = ALLOC_STRING("plats/train1.wav");		// move sound

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	pev->classname = ALLOC_STRING("train");

	SET_MODEL(ENT(pev), STRING(pev->model));
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	UTIL_SetOrigin(pev, pev->origin);

	pev->nextthink = pev->ltime + TRAIN_START_DELAY;
	SetThink(&CFuncTrain::Wait);
}

//=========================================================
// train_info - a test marker: whatever touches it stops and
// spins, and the marker turns into a scientist
//=========================================================
class CTrainInfo : public CBaseEntity
{
public:
	void Spawn();
	void Think(CBaseEntity *pOther);
	void Touch(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(train_info, CTrainInfo);

void CTrainInfo::Think(CBaseEntity *pOther)
{
	pev->solid = SOLID_TRIGGER;
}

void CTrainInfo::Touch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);

	SET_MODEL(ENT(pev), "models/scientist.mdl");

	pevOther->speed = 0.0f;
	pevOther->avelocity.y = TRAIN_INFO_SPIN;
}

void CTrainInfo::Spawn()
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;

	UTIL_SetSize(pev, Vector(-32.0f, -32.0f, -32.0f), Vector(32.0f, 32.0f, 32.0f));
}
