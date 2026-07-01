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
#pragma once

//=========================================================
// CBaseToggle - shared brush-entity foundation
//
// The mover / button / trigger / breakable families (Wave 2)
// derive from these three base classes:
//
//   CBaseDelay  : CBaseEntity   - delayed / killtarget firing
//   CBaseToggle : CBaseDelay    - linear + angular move framework
//   CBaseTrigger: CBaseToggle   - brush trigger setup
//
//=========================================================

#include "cbase.h"
#include "utils.h"		// PEV_* offsets (incl. brush/mover fields), Pev accessors

//=========================================================
// move state (m_toggle_state)
//=========================================================
enum TOGGLE_STATE
{
	TS_AT_TOP = 0,	// binary toggle_state is 0-based (e.g. DoorGoUp sets state = 2 = TS_GOING_UP)
	TS_AT_BOTTOM,	// 1
	TS_GOING_UP,	// 2
	TS_GOING_DOWN	// 3
};

//=========================================================
// CBaseDelay
//
// CBaseEntity plus a fire delay and a killtarget.  Extends the
// 28-byte CBaseEntity layout with two fields:
//   [28] m_flDelay        [32] m_iszKillTarget
//=========================================================
class CBaseDelay : public CBaseEntity
{
public:
	CBaseDelay();

	void KeyValue(KeyValueData* pkvd);

	// SUB_UseTargets - fire pev->target / killtarget after m_flDelay.
	void SUB_UseTargets(CBaseEntity* pActivator, int useType, float value);

protected:
	float m_flDelay;		// [28]
	int m_iszKillTarget;	// [32]
};

//=========================================================
// CBaseToggle
//
// CBaseDelay plus the linear / angular move framework shared by
// every brush mover.  Field byte offsets (derived from the alpha
// func_door decompiles):
//
//   [28]  m_flDelay              (CBaseDelay)
//   [32]  m_iszKillTarget        (CBaseDelay)
//   [36]  m_toggle_state
//   [40]  m_flActivateFinished
//   [44]  m_flMoveDistance       ("distance")
//   [48]  m_flWait               ("wait")
//   [52]  m_flLip                ("lip")
//   [56]  m_flTWidth
//   [60]  m_flTLength
//   [64]  m_vecPosition1 / m_vecAngle1
//   [76]  m_vecPosition2 / m_vecAngle2
//   [88]  m_cTriggersLeft        ("count")
//   [92]  m_flHeight
//   [96]  m_hActivator           (ent index of the firing player)
//   [100] m_pfnCallWhenMoveDone
//   [104] m_vecFinalDest
//   [116] m_vecFinalAngle
//   [128] m_bHealthValue         ("healthvalue")
//   [129] m_bMoveSnd             ("movesnd")
//   [130] m_bStopSnd             ("stopsnd")
//=========================================================
class CBaseToggle : public CBaseDelay
{
public:
	CBaseToggle();

	void KeyValue(KeyValueData* pkvd);

	// Linear movement: glide pev->origin to vecDest at flSpeed, then
	// invoke the move-done callback installed by SetMoveDone.
	void LinearMove(const Vector& vecDest, float flSpeed);
	void LinearMoveDone(CBaseEntity* pOther);

	// Angular movement: glide pev->angles to vecDestAngle at flSpeed.
	void AngularMove(const Vector& vecDestAngle, float flSpeed);
	void AngularMoveDone(CBaseEntity* pOther);

	void SetActivator(int activator);
	void SetMoveDoneNoop();

protected:
	typedef void (CBaseEntity::*MoveDoneFunc)(CBaseEntity* pOther);

	template <typename T>
	void SetMoveDone(void (T::*pfn)(CBaseEntity*))
	{
		m_pfnCallWhenMoveDone = (MoveDoneFunc)pfn;
	}

	void SetMoveDone(MoveDoneFunc pfn) { m_pfnCallWhenMoveDone = pfn; }
	void MoveDoneNoop(CBaseEntity* pOther);

	int m_toggle_state;				// [36]
	float m_flActivateFinished;		// [40]
	float m_flMoveDistance;			// [44]
	float m_flWait;					// [48]
	float m_flLip;					// [52]
	float m_flTWidth;				// [56]
	float m_flTLength;				// [60]
	Vector m_vecPosition1;			// [64]  (m_vecAngle1)
	Vector m_vecPosition2;			// [76]  (m_vecAngle2)
	int m_cTriggersLeft;			// [88]
	float m_flHeight;				// [92]
	int m_hActivator;				// [96]
	MoveDoneFunc m_pfnCallWhenMoveDone;	// [100]
	Vector m_vecFinalDest;			// [104]
	Vector m_vecFinalAngle;			// [116]
	unsigned char m_bHealthValue;	// [128]
	unsigned char m_bMoveSnd;		// [129]
	unsigned char m_bStopSnd;		// [130]
};

//=========================================================
// CBaseTrigger
//
// CBaseToggle specialised for brush triggers.  Adds no fields of
// its own; InitTrigger wires the solid/model/movetype state shared
// by every trigger_* entity.
//=========================================================
class CBaseTrigger : public CBaseToggle
{
public:
	CBaseTrigger();

	void InitTrigger();
};

//=========================================================
// Binary private-data sizes (entmap):
//   CBaseDelay  : 36   (DelayedUse helper entity)
//   CBaseToggle : 132  (func_door)
//   CBaseTrigger: 132  (shares the CBaseToggle layout; trigger_*
//                       entities allocate 128 because they never
//                       touch the sound bytes at [128..130])
//=========================================================
HL_COMPILE_TIME_ASSERT(sizeof(CBaseDelay) <= 36, CBaseDelay_size);
HL_COMPILE_TIME_ASSERT(sizeof(CBaseToggle) <= 132, CBaseToggle_size);
HL_COMPILE_TIME_ASSERT(sizeof(CBaseTrigger) <= 132, CBaseTrigger_size);

//=========================================================
// SetMovedir - derive pev->movedir from pev->angles (file-internal
// helper shared by movers and triggers; matches the binary).
//=========================================================
void SetMovedir(entvars_t* pev);
