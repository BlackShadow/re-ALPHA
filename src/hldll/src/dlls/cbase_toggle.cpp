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
// CBaseToggle - shared brush-entity foundation
//
// CBaseDelay (SUB_UseTargets, KeyValue)
// CBaseToggle (KeyValue, SetMovedir, LinearMove, AngularMove)
// CBaseTrigger (InitTrigger)
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include "cbase_toggle.h"
#include "enginefuncs.h"
#include "utils.h"

//=========================================================
// globalvars_t byte offsets (engine threads the activator /
// touched entity through these before a Touch/Use dispatch).
//=========================================================
enum
{
	GLOBALS_TOUCH_SELF		= 112,	// index of the entity currently being fired
	GLOBALS_ACTIVATOR_ENT	= 116	// index of the activator (== GLOBALS_OTHER_ENTINDEX)
};

//=========================================================
// CBaseDelay
//=========================================================
CBaseDelay::CBaseDelay()
{
	m_flDelay = 0.0f;
	m_iszKillTarget = 0;
}

//=========================================================
// CBaseDelay::KeyValue
//
// Handles "delay" (stored as m_flDelay) and "killtarget"
// (stored as the m_iszKillTarget string index).
//=========================================================
void CBaseDelay::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "delay") == 0)
	{
		m_flDelay = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "killtarget") == 0)
	{
		m_iszKillTarget = EngineAllocString(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else
	{
		CBaseEntity::KeyValue(pkvd);
	}
}

//=========================================================
// CDelayedUse - the helper entity spawned by SUB_UseTargets
// when m_flDelay is non-zero.  It is a bare CBaseDelay whose
// Think (DelayedUseThink) re-fires the targets after the delay
// elapses, then removes itself. (think)
//=========================================================
class CDelayedUse : public CBaseDelay
{
public:
	void DelayedUseThink(CBaseEntity* pOther);
	void InstallThink();
};

//=========================================================
// CDelayedUse::InstallThink - park DelayedUseThink in the think
// slot (kept out-of-line so the protected SetThink is reached
// through the entity's own type).
//=========================================================
void CDelayedUse::InstallThink()
{
	SetThink(&CDelayedUse::DelayedUseThink);
}

//=========================================================
// CDelayedUse::DelayedUseThink
//=========================================================
void CDelayedUse::DelayedUseThink(CBaseEntity* pOther)
{
	// m_flDelay is 0 on the delayed entity, so this fires immediately.
	SUB_UseTargets(NULL, 0, 0.0f);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// CBaseDelay::SUB_UseTargets
//
//   if m_flDelay != 0   -> spawn a CDelayedUse to fire later
//   else if killtarget  -> remove every entity named killtarget
//   else                -> fire pev->target (call Use on each match)
//=========================================================
void CBaseDelay::SUB_UseTargets(CBaseEntity* pActivator, int useType, float value)
{
	HL_UNUSED(useType);
	HL_UNUSED(value);

	//
	// Delayed firing: clone the firing parameters onto a CDelayedUse
	// entity that thinks after m_flDelay seconds.
	//
	if (((*(int*)&m_flDelay) & 0x7FFFFFFF) != 0)
	{
		edict_t* edict = EngineCreateEntity();
		entvars_t* pevDelay = edict ? EngineGetVarsOfEnt(edict) : NULL;
		if (!pevDelay)
			return;

		edict_t* delayEdict = EdictFromEntvars(pevDelay);
		if (!delayEdict)
			return;

		void* priv = EngineGetPrivateData(delayEdict);
		CDelayedUse* pTemp;
		if (priv)
		{
			pTemp = (CDelayedUse*)priv;
		}
		else
		{
			priv = EngineAllocPrivateData(delayEdict, sizeof(CDelayedUse));
			if (!priv)
				return;

			memset(priv, 0, sizeof(CDelayedUse));
			pTemp = new (priv) CDelayedUse();
			pTemp->pev = pevDelay;
			pTemp->m_pGlobals = GlobalsFromEntvars(pevDelay);
		}

		PevInt(pevDelay, PEV_CLASSNAME) = EngineAllocString("DelayedUse");
		PevFloat(pevDelay, PEV_NEXTTHINK) = GlobalTime() + m_flDelay;

		pTemp->InstallThink();
		pTemp->m_iszKillTarget = m_iszKillTarget;
		pTemp->m_flDelay = 0.0f;

		PevInt(pevDelay, PEV_TARGET) = PevInt(pev, PEV_TARGET);
		return;
	}

	//
	// Kill any entities named by our killtarget.
	//
	// m_iszKillTarget is stored as this translation's string_t index, so
	// de-index it before handing the name to the engine string search.
	if (m_iszKillTarget != 0)
	{
		const char* pszKillName = EngineStringFromIndex(m_iszKillTarget);
		edict_t* pKill = NULL;

		if (!pszKillName)
			return;

		while ((pKill = EngineFindEntityByString(pKill, "targetname", pszKillName)) != NULL)
		{
			// never remove the world edict (index 0) that terminates the chain
			if (EngineIndexOfEdict(pKill) == 0)
				break;

			EngineRemoveEntity(pKill);
		}
		return;
	}

	//
	// Fire pev->target: call Use on every matching entity.
	//
	if (PevInt(pev, PEV_TARGET) == 0)
		return;

	void* globals = GlobalsFromEntvars(pev);
	if (!globals)
		return;

	// Saved before the loop and restored after each Use so the dispatch
	// sees the original globals.  Mirrors the binary exactly: during the
	// Use, GLOBALS_TOUCH_SELF holds the target's index and
	// GLOBALS_ACTIVATOR_ENT holds the *original* self index.
	int oldSelfIndex = *GlobalsInt(globals, GLOBALS_TOUCH_SELF);
	int oldActivatorIndex = *GlobalsInt(globals, GLOBALS_ACTIVATOR_ENT);

	const char* pszTargetName = EngineStringFromIndex(PevInt(pev, PEV_TARGET));
	edict_t* pTarget = NULL;

	while ((pTarget = EngineFindEntityByString(pTarget, "targetname", pszTargetName)) != NULL)
	{
		// the engine returns the world edict (index 0) at the end of the
		// search chain; stop without firing it
		if (EngineIndexOfEdict(pTarget) == 0)
			break;

		// thread the activator through globals exactly like the engine
		// does for a normal Use dispatch
		*GlobalsInt(globals, GLOBALS_TOUCH_SELF) = EngineIndexOfEdict(pTarget);
		*GlobalsInt(globals, GLOBALS_ACTIVATOR_ENT) = oldSelfIndex;

		entvars_t* pevTarget = EngineGetVarsOfEnt(pTarget);
		edict_t* pUseEdict = EdictFromEntvars(pevTarget);
		if (!pUseEdict)
		{
			pUseEdict = EngineCreateEntity();
			pevTarget = pUseEdict ? EngineGetVarsOfEnt(pUseEdict) : NULL;
			pUseEdict = pevTarget ? EdictFromEntvars(pevTarget) : NULL;
		}

		void* priv = pUseEdict ? EngineGetPrivateData(pUseEdict) : NULL;

		CBaseEntity* pEntity = NULL;
		int stopDispatch = 0;
		if (priv)
		{
			pEntity = (CBaseEntity*)priv;
		}
		else
		{
			if (!pevTarget || !pUseEdict)
			{
				stopDispatch = 1;
			}
			else
			{
				priv = EngineAllocPrivateData(pUseEdict, sizeof(CBaseEntity));
				if (priv)
				{
					pEntity = new (priv) CBaseEntity();
					pEntity->pev = pevTarget;
					pEntity->m_pGlobals = GlobalsFromEntvars(pevTarget);
				}
				else
				{
					stopDispatch = 1;
				}
			}
		}

		// The binary dispatches the target's Use (slot +28) with a hardcoded
		// NULL activator (push 0); the real activator is conveyed
		// via globals+116 (set above), which the Use handlers read.
		if (pEntity)
			pEntity->Use(NULL);

		*GlobalsInt(globals, GLOBALS_TOUCH_SELF) = oldSelfIndex;
		*GlobalsInt(globals, GLOBALS_ACTIVATOR_ENT) = oldActivatorIndex;

		if (stopDispatch)
			break;
	}
}

//=========================================================
// SetMovedir
//
// Convert pev->angles into a unit pev->movedir.  The two sentinel
// angles select the vertical axes; anything else uses the forward
// vector of the angles.  pev->angles is then cleared to (0,0,0).
//=========================================================
void SetMovedir(entvars_t* pev)
{
	Vector& angles = PevVector(pev, PEV_ANGLES);
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);

	if (angles.x == 0.0f && angles.y == -1.0f && angles.z == 0.0f)
	{
		// straight up
		VecSet(VecPtr(movedir), 0.0f, 0.0f, 1.0f);
	}
	else if (angles.x == 0.0f && angles.y == -2.0f && angles.z == 0.0f)
	{
		// straight down
		VecSet(VecPtr(movedir), 0.0f, 0.0f, -1.0f);
	}
	else
	{
		EngineMakeVectors(VecPtr(angles));

		void* globals = GlobalsFromEntvars(pev);
		VecCopy(VecPtr(movedir), GlobalsForward(globals));
	}

	VecSet(VecPtr(angles), 0.0f, 0.0f, 0.0f);
}

//=========================================================
// CBaseToggle
//=========================================================
CBaseToggle::CBaseToggle()
{
	m_toggle_state = 0;
	m_flActivateFinished = 0.0f;
	m_flMoveDistance = 0.0f;
	m_flWait = 0.0f;
	m_flLip = 0.0f;
	m_flTWidth = 0.0f;
	m_flTLength = 0.0f;
	VecSet(VecPtr(m_vecPosition1), 0.0f, 0.0f, 0.0f);
	VecSet(VecPtr(m_vecPosition2), 0.0f, 0.0f, 0.0f);
	m_cTriggersLeft = 0;
	m_flHeight = 0.0f;
	m_hActivator = 0;
	m_pfnCallWhenMoveDone = (MoveDoneFunc)&CBaseToggle::MoveDoneNoop;
	VecSet(VecPtr(m_vecFinalDest), 0.0f, 0.0f, 0.0f);
	VecSet(VecPtr(m_vecFinalAngle), 0.0f, 0.0f, 0.0f);
}

//=========================================================
// SetActivator
//=========================================================
void CBaseToggle::SetActivator(int activator)
{
	m_hActivator = activator;
}

//=========================================================
// MoveDoneNoop
//=========================================================
void CBaseToggle::MoveDoneNoop(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);
}

//=========================================================
// SetMoveDoneNoop
//=========================================================
void CBaseToggle::SetMoveDoneNoop()
{
	m_pfnCallWhenMoveDone = (MoveDoneFunc)&CBaseToggle::MoveDoneNoop;
}

//=========================================================
// CBaseToggle::KeyValue
//
// "lip","skin","wait","distance","movesnd","stopsnd","healthvalue"
//=========================================================
void CBaseToggle::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "lip") == 0)
	{
		m_flLip = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "skin") == 0)
	{
		PevFloat(pev, PEV_SKIN) = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "wait") == 0)
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "distance") == 0)
	{
		m_flMoveDistance = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "movesnd") == 0)
	{
		m_bMoveSnd = (unsigned char)(int)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "stopsnd") == 0)
	{
		m_bStopSnd = (unsigned char)(int)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "healthvalue") == 0)
	{
		m_bHealthValue = (unsigned char)(int)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	// The binary is a leaf: an unrecognised key is silently ignored. It
	// does NOT chain to CBaseDelay::KeyValue (the alpha never parent-chains
	// KeyValue), so there is no trailing else here.
}

//=========================================================
// CBaseToggle::LinearMove
//
// Glide pev->origin toward vecDest at flSpeed units/sec, parking
// LinearMoveDone in the think slot to fire on arrival.
//=========================================================
void CBaseToggle::LinearMove(const Vector& vecDest, float flSpeed)
{
	VecCopy(VecPtr(m_vecFinalDest), VecPtr(vecDest));

	Vector& origin = PevVector(pev, PEV_ORIGIN);

	// already there - finish immediately
	if (origin.x == vecDest.x && origin.y == vecDest.y && origin.z == vecDest.z)
	{
		LinearMoveDone(NULL);
		return;
	}

	float vecDestDelta[3];
	Vec3Sub(vecDestDelta, VecPtr(vecDest), VecPtr(origin));
	float flTravelTime = VecLength(vecDestDelta) / flSpeed;

	// too short a move to bother gliding - snap there now
	if (flTravelTime < 0.1f)
	{
		LinearMoveDone(NULL);
		return;
	}

	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + flTravelTime;
	SetThink(&CBaseToggle::LinearMoveDone);

	Vector& velocity = PevVector(pev, PEV_VELOCITY);
	VecSet(VecPtr(velocity), vecDestDelta[0] / flTravelTime, vecDestDelta[1] / flTravelTime, vecDestDelta[2] / flTravelTime);
}

//=========================================================
// CBaseToggle::LinearMoveDone
//
// Snap to the final destination, kill velocity, then run the
// move-done callback installed by SetMoveDone.
//=========================================================
void CBaseToggle::LinearMoveDone(CBaseEntity* pOther)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetOrigin(edict, VecPtr(m_vecFinalDest));

	VecSet(VecPtr(PevVector(pev, PEV_VELOCITY)), 0.0f, 0.0f, 0.0f);

	PevFloat(pev, PEV_NEXTTHINK) = -1.0f;

	(this->*m_pfnCallWhenMoveDone)(NULL);
}

//=========================================================
// CBaseToggle::AngularMove
//
// Glide pev->angles toward vecDestAngle at flSpeed deg/sec, parking
// AngularMoveDone in the think slot to fire on arrival.
//=========================================================
void CBaseToggle::AngularMove(const Vector& vecDestAngle, float flSpeed)
{
	VecCopy(VecPtr(m_vecFinalAngle), VecPtr(vecDestAngle));

	Vector& angles = PevVector(pev, PEV_ANGLES);

	if (angles.x == vecDestAngle.x && angles.y == vecDestAngle.y && angles.z == vecDestAngle.z)
	{
		AngularMoveDone(NULL);
		return;
	}

	float vecDestDelta[3];
	Vec3Sub(vecDestDelta, VecPtr(vecDestAngle), VecPtr(angles));
	float flTravelTime = VecLength(vecDestDelta) / flSpeed;

	// too short a move to bother gliding - snap there now
	if (flTravelTime < 0.1f)
	{
		AngularMoveDone(NULL);
		return;
	}

	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + flTravelTime;
	SetThink(&CBaseToggle::AngularMoveDone);

	Vector& avelocity = PevVector(pev, PEV_AVELOCITY);
	VecSet(VecPtr(avelocity), vecDestDelta[0] / flTravelTime, vecDestDelta[1] / flTravelTime, vecDestDelta[2] / flTravelTime);
}

//=========================================================
// CBaseToggle::AngularMoveDone
//
// Snap to the final angle, kill angular velocity, then run the
// move-done callback installed by SetMoveDone.
//=========================================================
void CBaseToggle::AngularMoveDone(CBaseEntity* pOther)
{
	VecCopy(VecPtr(PevVector(pev, PEV_ANGLES)), VecPtr(m_vecFinalAngle));

	VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), 0.0f, 0.0f, 0.0f);

	PevFloat(pev, PEV_NEXTTHINK) = -1.0f;

	(this->*m_pfnCallWhenMoveDone)(NULL);
}

//=========================================================
// CBaseTrigger
//=========================================================
CBaseTrigger::CBaseTrigger()
{
}

//=========================================================
// CBaseTrigger::InitTrigger
//
// Derive movedir from non-zero spawn angles, make the brush a
// SOLID_TRIGGER, bind its model, drop movetype, then hide it by
// clearing the model index.
//=========================================================
void CBaseTrigger::InitTrigger()
{
	Vector& angles = PevVector(pev, PEV_ANGLES);
	if (angles.x != 0.0f || angles.y != 0.0f || angles.z != 0.0f)
		SetMovedir(pev);

	PevFloat(pev, PEV_SOLID) = 1.0f;	// SOLID_TRIGGER

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	PevFloat(pev, PEV_MOVETYPE) = 0.0f;	// MOVETYPE_NONE
	PevInt(pev, PEV_MODELINDEX) = 0;	// hide the brush
	PevInt(pev, PEV_MODEL) = 0;
}
