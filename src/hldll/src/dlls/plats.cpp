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
// Plats - moving brush entities
//
//   CFuncPlat     (func_plat)  - elevator that rises / lowers between
//                                two heights via LinearMove.  Spawns a
//                                CPlatTrigger field that catches a player
//                                standing on it.
//   CFuncTrain    (func_train) - path_corner follower: glides from one
//                                target brush to the next.
//   CTrainInfo    (train_info) - alpha cinematic spawn marker.
//   (CJohnTrain / john_train now lives in john_train.cpp.)
//
// CFuncPlat and CFuncTrain derive from the shared CBaseToggle
// foundation (LinearMove / move-done callbacks / KeyValue); the
// plat's auto-trigger field and train_info are bare
// CBaseEntity helpers.
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include "cbase_toggle.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// move sounds / models
//=========================================================
static const char kPlatMoveSound[] = "plats/platmove1.wav";
static const char kPlatStopSound[] = "plats/platstop1.wav";
static const char kTrainMoveSound[] = "plats/train1.wav";
static const char kTrainStartSound[] = "plats/train2.wav";
static const char kScientistModel[] = "models/scientist.mdl";

//=========================================================
// brush mover constants
//=========================================================
#define SOLID_BSP			4.0f	// pev->solid
#define MOVETYPE_PUSH		7.0f	// pev->movetype
#define SOLID_NOT			0.0f
#define SOLID_TRIGGER		1.0f
#define MOVETYPE_NONE		0.0f
#define AT_CONSOLE			3		// ALERT() message level

#define PLAT_MOVE_VOLUME	1.0f
#define PLAT_MOVE_ATTN		0.8f	// 0.80000001f

//=========================================================
// func_plat move state (m_toggle_state).  The alpha plat uses its own
// encoding, distinct from the shared TOGGLE_STATE enum:
//   0 = parked at top       2 = travelling up
//   1 = parked at bottom    3 = travelling down
//=========================================================
enum
{
	PLAT_AT_TOP			= 0,
	PLAT_AT_BOTTOM		= 1,
	PLAT_GOING_UP		= 2,
	PLAT_GOING_DOWN		= 3,
};

//=========================================================
// pev field offsets used by the movers that are not named
// PEV_* constants in utils.h.
//=========================================================
enum
{
	PEV_AVEL_Y			= 92,	// pev->avelocity.y (john_train spin / train_info bump)

	// The plat/train movers use the base pev->noise (480) and pev->noise1
	// (484) sound fields, NOT the door's pev->noise1 (484)/noise2 (488).
	// (utils.h's PEV_NOISE_MOVING/ARRIVED are the door offsets; the alpha
	// plat/train code in the binary reads/writes 480 and 484 instead.)
	PEV_NOISE			= 480,	// looping/move sound (plat move; train "start" slot)
	PEV_NOISE1			= 484,	// stop/arrive sound (plat stop; train "move" slot)
};

class CFuncPlat;

//=========================================================
// CFuncPlat - func_plat elevator (alpha private data = 144 bytes:
// CBaseToggle + a saved copy of the original brush movedir).
//=========================================================
class CFuncPlat : public CBaseToggle
{
	friend class CPlatTrigger;	// the field entity drives the plat

public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Blocked(CBaseEntity* pOther);

	void PlatUse(CBaseEntity* pOther);

	void GoUp();
	void GoDown(CBaseEntity* pOther);
	void HitTop(CBaseEntity* pOther);
	void HitBottom(CBaseEntity* pOther);

	// Build and wire the auto-trigger field around the platform.
	void SetupTrigger();

private:
	// [132] saved copy of the brush movedir before it is zeroed.
	Vector m_vecOriginalMoveDir;
};

//=========================================================
// CPlatTrigger - the invisible field a CFuncPlat builds around itself
// so that a player who walks into it activates the plat.  In the alpha
// it is a bare 32-byte CBaseEntity with a single extra field: a back
// pointer to its owning platform.
//=========================================================
class CPlatTrigger : public CBaseEntity
{
public:
	void SpawnInsideTrigger(CFuncPlat* pPlatform);
	void Touch(CBaseEntity* pOther);

private:
	// [28] back-pointer to the owning CFuncPlat
	CFuncPlat* m_pPlatform;
};

HL_COMPILE_TIME_ASSERT(sizeof(CFuncPlat) <= 144, CFuncPlat_size);
HL_COMPILE_TIME_ASSERT(sizeof(CPlatTrigger) <= 32, CPlatTrigger_size);

//=========================================================
// CPlatTrigger::SpawnInsideTrigger
//
// Make the field a SOLID_TRIGGER box sized to the platform brush so a
// player standing on the plat trips it.  The box spans the platform's
// travel in z and is shifted +25 on the horizontal axes (matches the
// alpha verbatim); very thin brushes get a 1-unit centred slab.
//=========================================================
void CPlatTrigger::SpawnInsideTrigger(CFuncPlat* pPlatform)
{
	m_pPlatform = pPlatform;

	PevFloat(pev, PEV_SOLID) = SOLID_TRIGGER;
	PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_NONE;

	entvars_t* pevPlat = pPlatform->pev;
	Vector& vecMins = PevVector(pevPlat, PEV_MINS);
	Vector& vecMaxs = PevVector(pevPlat, PEV_MAXS);

	Vector vecTMin;
	Vector vecTMax;

	vecTMin.x = vecMins.x + 25.0f;
	vecTMin.y = vecMins.y + 25.0f;

	vecTMax.x = vecMaxs.x + 25.0f;
	vecTMax.y = vecMaxs.y + 25.0f;
	vecTMax.z = vecMaxs.z + 8.0f;

	// box bottom sits one plat-travel below its raised top face
	vecTMin.z = vecTMax.z - (pPlatform->m_vecPosition1.z - pPlatform->m_vecPosition2.z + 8.0f);

	// very thin brushes get a 1-unit slab centred on the brush
	if (PevFloat(pevPlat, PEV_SIZE) <= 50.0f)
	{
		float flMid = (vecMins.x + vecMaxs.x) * 0.5f;
		vecTMin.x = flMid;
		vecTMax.x = flMid + 1.0f;
	}

	if (PevFloat(pevPlat, PEV_SIZE + 4) <= 50.0f)
	{
		float flMid = (vecMaxs.y + vecMins.y) * 0.5f;
		vecTMin.y = flMid;
		vecTMax.y = flMid + 1.0f;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetSize(edict, VecPtr(vecTMin), VecPtr(vecTMax));
}

//=========================================================
// CPlatTrigger::Touch
//
// When a live player enters the field, drive the platform: send it
// down when it is at the top, or keep it parked (refresh nextthink)
// when it is already down.
//=========================================================
void CPlatTrigger::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	if (!globals)
		return;

	int otherIndex = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);
	edict_t* pOtherEdict = otherIndex ? EnginePEntityOfEntIndex(otherIndex) : NULL;
	entvars_t* pevToucher = pOtherEdict ? EngineGetVarsOfEnt(pOtherEdict) : NULL;

	if (!pevToucher)
		return;

	// only react to a living player
	if (strcmp(EngineStringFromIndex(PevInt(pevToucher, PEV_CLASSNAME)), "player") != 0)
		return;

	if (PevFloat(pevToucher, PEV_HEALTH) <= 0.0f)
		return;

	CFuncPlat* pPlatform = m_pPlatform;

	if (pPlatform->m_toggle_state == PLAT_AT_BOTTOM)
	{
		// a player rode it down - send it back up
		pPlatform->GoUp();
	}
	else if (pPlatform->m_toggle_state == PLAT_AT_TOP)
	{
		// at the top with a player aboard - hold it there a moment longer
		PevFloat(pPlatform->pev, PEV_NEXTTHINK) = PevFloat(pPlatform->pev, PEV_LTIME) + 1.0f;
	}
}

//=========================================================
// CFuncPlat::KeyValue
//
// "lip","wait","height".
//=========================================================
void CFuncPlat::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "lip") == 0)
	{
		m_flLip = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "wait") == 0)
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "height") == 0)
	{
		m_flHeight = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CFuncPlat::SetupTrigger
//
// Allocate the CPlatTrigger field entity and wire it onto the plat.
//=========================================================
void CFuncPlat::SetupTrigger()
{
	// the platform itself (passed through as the binary's a1 == this->pev)
	entvars_t* pevPlat = pev;
	edict_t* platEdict = EdictFromEntvars(pevPlat);

	CFuncPlat* pPlatform;
	{
		void* priv = platEdict ? EngineGetPrivateData(platEdict) : NULL;
		if (priv)
		{
			pPlatform = (CFuncPlat*)priv;
		}
		else
		{
			priv = EngineAllocPrivateData(platEdict, 144);
			pPlatform = priv ? new (priv) CFuncPlat() : NULL;
			if (pPlatform)
			{
				pPlatform->pev = pevPlat;
				pPlatform->m_pGlobals = GlobalsFromEntvars(pevPlat);
			}
		}
	}

	// create the field entity that surrounds the platform
	edict_t* fieldEdict = EngineCreateEntity();
	entvars_t* pevField = fieldEdict ? EngineGetVarsOfEnt(fieldEdict) : NULL;

	CPlatTrigger* pField = NULL;
	if (pevField)
	{
		void* priv = EngineGetPrivateData(fieldEdict);
		if (!priv)
		{
			priv = EngineAllocPrivateData(fieldEdict, 32);
			pField = priv ? new (priv) CPlatTrigger() : NULL;
			if (pField)
			{
				pField->pev = pevField;
				pField->m_pGlobals = GlobalsFromEntvars(pevField);
			}
		}
		else
		{
			pField = (CPlatTrigger*)priv;
		}
	}

	if (pField)
		pField->SpawnInsideTrigger(pPlatform);
}

//=========================================================
// CFuncPlat::Spawn
//=========================================================
void CFuncPlat::Spawn()
{
	EnginePrecacheSound(kPlatMoveSound);
	EnginePrecacheSound(kPlatStopSound);

	PevInt(pev, PEV_NOISE) = EngineAllocString(kPlatMoveSound);
	PevInt(pev, PEV_NOISE1) = EngineAllocString(kPlatStopSound);

	// defaults
	if (((*(int*)&m_flTLength) & 0x7FFFFFFF) == 0)
		m_flTLength = 80.0f;

	if (((*(int*)&m_flTWidth) & 0x7FFFFFFF) == 0)
		m_flTWidth = 10.0f;

	// The binary stashes pev->angles (offset 0x4C=76) into
	// m_vecOriginalMoveDir and zeroes pev->angles (g_vecZero), so the plat only
	// ever travels straight along its programmed height -- NOT pev->movedir.
	VecCopy(VecPtr(m_vecOriginalMoveDir), VecPtr(PevVector(pev, PEV_ANGLES)));
	VecSet(VecPtr(PevVector(pev, PEV_ANGLES)), 0.0f, 0.0f, 0.0f);

	PevFloat(pev, PEV_SOLID) = SOLID_BSP;
	PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_PUSH;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
	{
		EngineSetOrigin(edict, VecPtr(PevVector(pev, PEV_ORIGIN)));
		EngineSetSize(edict, VecPtr(PevVector(pev, PEV_MINS)), VecPtr(PevVector(pev, PEV_MAXS)));
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));
	}

	// plat moves at speed stored in pev->dmgtime (default 150)
	if (((*(int*)&PevFloat(pev, PEV_DMGTIME)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DMGTIME) = 150.0f;

	// raised (top) position is the spawn origin; lowered (bottom)
	// position drops by m_flHeight, or by the brush height minus the
	// alpha's hardcoded 8-unit clearance when no height is supplied.
	VecCopy(VecPtr(m_vecPosition1), VecPtr(PevVector(pev, PEV_ORIGIN)));
	VecCopy(VecPtr(m_vecPosition2), VecPtr(PevVector(pev, PEV_ORIGIN)));

	if (((*(int*)&m_flHeight) & 0x7FFFFFFF) != 0)
		m_vecPosition2.z = PevVector(pev, PEV_ORIGIN).z - m_flHeight;
	else
		m_vecPosition2.z = PevVector(pev, PEV_ORIGIN).z - PevVector(pev, PEV_SIZE).z + 8.0f;

	SetupTrigger();

	if (PevInt(pev, PEV_TARGETNAME) != 0)
	{
		// a named plat waits at the top until used
		EngineSetOrigin(edict, VecPtr(m_vecPosition1));
		m_toggle_state = PLAT_AT_TOP;
		SetUse(&CFuncPlat::PlatUse);
	}
	else
	{
		// an unnamed plat starts lowered and runs on touch
		EngineSetOrigin(edict, VecPtr(m_vecPosition2));
		m_toggle_state = PLAT_AT_BOTTOM;
	}
}

//=========================================================
// CFuncPlat::PlatUse
//
// One-shot use: disarm itself, then send the plat down if it is
// resting at the top.
//=========================================================
void CFuncPlat::PlatUse(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	SetUse(NULL);

	if (m_toggle_state == PLAT_AT_TOP)
		GoDown(NULL);
}

//=========================================================
// CFuncPlat::GoDown
//
// Play the move sound, mark PLAT_GOING_DOWN, then glide to the lowered
// position with HitBottom queued as the move-done callback.
//=========================================================
void CFuncPlat::GoDown(CBaseEntity* pOther)
{
	EngineEmitSound(EdictFromEntvars(pev), 1, EngineStringFromIndex(PevInt(pev, PEV_NOISE)),
		PLAT_MOVE_VOLUME, PLAT_MOVE_ATTN);

	m_toggle_state = PLAT_GOING_DOWN;
	SetMoveDone(&CFuncPlat::HitBottom);
	LinearMove(m_vecPosition2, PevFloat(pev, PEV_DMGTIME));
}

//=========================================================
// CFuncPlat::GoUp
//
// Mirror of GoDown for the upward leg, with HitTop queued.
//=========================================================
void CFuncPlat::GoUp()
{
	EngineEmitSound(EdictFromEntvars(pev), 1, EngineStringFromIndex(PevInt(pev, PEV_NOISE)),
		PLAT_MOVE_VOLUME, PLAT_MOVE_ATTN);

	m_toggle_state = PLAT_GOING_UP;
	SetMoveDone(&CFuncPlat::HitTop);
	LinearMove(m_vecPosition1, PevFloat(pev, PEV_DMGTIME));
}

//=========================================================
// CFuncPlat::HitTop (move-done callback of GoUp)
//
// Stop sound, settle at PLAT_AT_TOP, then queue an automatic descent
// three seconds later through GoDown.
//=========================================================
void CFuncPlat::HitTop(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	EngineEmitSound(EdictFromEntvars(pev), 1, EngineStringFromIndex(PevInt(pev, PEV_NOISE1)),
		PLAT_MOVE_VOLUME, PLAT_MOVE_ATTN);

	m_toggle_state = PLAT_AT_TOP;
	SetThink(&CFuncPlat::GoDown);
	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + 3.0f;
}

//=========================================================
// CFuncPlat::HitBottom (move-done callback of GoDown)
//
// Stop sound, settle at PLAT_AT_BOTTOM.
//=========================================================
void CFuncPlat::HitBottom(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	EngineEmitSound(EdictFromEntvars(pev), 1, EngineStringFromIndex(PevInt(pev, PEV_NOISE1)),
		PLAT_MOVE_VOLUME, PLAT_MOVE_ATTN);

	m_toggle_state = PLAT_AT_BOTTOM;
}

//=========================================================
// CFuncPlat::Blocked
//
// Damage whatever blocked us (1 point), then reverse direction.
//=========================================================
void CFuncPlat::Blocked(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	// resolve the entity the engine threaded through globals (the blocker)
	int blockerIndex = *GlobalsInt(m_pGlobals, GLOBALS_OTHER_ENTINDEX);
	edict_t* pBlockerEdict = EnginePEntityOfEntIndex(blockerIndex);
	entvars_t* pevBlocker = pBlockerEdict ? EngineGetVarsOfEnt(pBlockerEdict) : NULL;

	CBaseEntity* pBlocker = pevBlocker ?
		(CBaseEntity*)EngineGetPrivateData(pBlockerEdict) : NULL;

	// the plat deals 1 point of crush damage to the blocker, with the
	// plat itself as both inflictor and attacker
	if (pBlocker)
		pBlocker->TakeDamage(pev, pev, PLAT_MOVE_VOLUME);

	// reverse: if we were heading down, go up, and vice versa
	if (m_toggle_state == PLAT_GOING_DOWN)
		GoUp();
	else if (m_toggle_state == PLAT_GOING_UP)
		GoDown(NULL);
}

//=========================================================
// CFuncTrain - func_train path follower (alpha private data 144).
//=========================================================
class CFuncTrain : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);	// shares with func_plat
	void Blocked(CBaseEntity* pOther);
	void Use(CBaseEntity* pOther);

	void Wait(CBaseEntity* pOther);		// settle on a path corner, line up the next leg
	void Next(CBaseEntity* pOther);		// glide to the next path corner
	void Arrived(CBaseEntity* pOther);		// move-done: wait, then loop back to Next (or park)
};

HL_COMPILE_TIME_ASSERT(sizeof(CFuncTrain) <= 144, CFuncTrain_size);

//=========================================================
// path-corner private-data offset: a corner's per-corner wait time
// lives at the same byte offset CBaseToggle keeps m_flWait (48).
//=========================================================
#define CORNER_WAIT_OFFSET	48

//=========================================================
// CFuncTrain::KeyValue (shared with func_plat)
//
// "lip","wait","height".
//=========================================================
void CFuncTrain::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "lip") == 0)
	{
		m_flLip = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "wait") == 0)
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "height") == 0)
	{
		m_flHeight = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CFuncTrain::Wait
//
// Snap the train onto the corner it just reached, advance pev->target
// to that corner's target, and (unless the train is named, i.e. waits
// to be retriggered) queue Next.
//=========================================================
void CFuncTrain::Wait(CBaseEntity* pOther)
{
	// find the path corner we are aimed at
	edict_t* pTarget = EngineFindEntityByString(NULL, "targetname",
		EngineStringFromIndex(PevInt(pev, PEV_TARGET)));
	entvars_t* pevTarget = EngineGetVarsOfEnt(pTarget);

	// inherit the corner's target as our next destination
	PevInt(pev, PEV_TARGET) = PevInt(pevTarget, PEV_TARGET);

	// position ourselves centred on the corner
	Vector vecCenter;
	Vector vecHalf;
	Vec3Add(VecPtr(vecCenter), VecPtr(PevVector(pev, PEV_MINS)), VecPtr(PevVector(pev, PEV_MAXS)));
	Vec3Scale(VecPtr(vecHalf), VecPtr(vecCenter), 0.5f);

	Vector vecDest;
	Vec3Sub(VecPtr(vecDest), VecPtr(PevVector(pevTarget, PEV_ORIGIN)), VecPtr(vecHalf));

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetOrigin(edict, VecPtr(vecDest));

	// an unnamed (free-running) train keeps rolling: schedule the next
	// leg immediately.  A named train parks here until it is Used again.
	if (PevInt(pev, PEV_TARGETNAME) == 0)
	{
		PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);
		SetThink(&CFuncTrain::Next);
	}
}

//=========================================================
// CFuncTrain::Next
//
// Look up the corner pev->target names, remember it as our enemy,
// inherit its wait/target, play the move sound, and glide there with
// Arrived queued as the move-done callback.  Picks up a per-corner
// speed override for the following leg.
//=========================================================
void CFuncTrain::Next(CBaseEntity* pOther)
{
	edict_t* pTarget = EngineFindEntityByString(NULL, "targetname",
		EngineStringFromIndex(PevInt(pev, PEV_TARGET)));
	entvars_t* pevTarget = pTarget ? EngineGetVarsOfEnt(pTarget) : NULL;

	if (!pevTarget || EngineIndexOfEdict(pTarget) == 0)
	{
		EngineAlertMessage(AT_CONSOLE, "TrainNext--cannot find next target '%s'",
			EngineStringFromIndex(PevInt(pev, PEV_TARGET)));
		return;
	}

	// ensure the corner has a class instance; the binary allocates a
	// bare wrapper (trigger-size 128) when one is missing.
	edict_t* cornerEdict = EdictFromEntvars(pevTarget);
	void* cornerPriv = cornerEdict ? EngineGetPrivateData(cornerEdict) : NULL;
	if (!cornerPriv)
	{
		cornerPriv = EngineAllocPrivateData(cornerEdict, 128);
		if (cornerPriv)
		{
			CBaseEntity* pCorner = new (cornerPriv) CBaseEntity();
			pCorner->pev = pevTarget;
			pCorner->m_pGlobals = GlobalsFromEntvars(pevTarget);
		}
	}

	// advance the target chain to the corner's own target
	PevInt(pev, PEV_TARGET) = PevInt(pevTarget, PEV_TARGET);

	if (PevInt(pev, PEV_TARGET) == 0)
	{
		EngineAlertMessage(AT_CONSOLE, "TrainNext: No next target", NULL);
		return;
	}

	// inherit the corner's wait time (private-data byte 48)
	if (cornerPriv && ((*(int*)((char*)cornerPriv + CORNER_WAIT_OFFSET)) & 0x7FFFFFFF) != 0)
		m_flWait = *(float*)((char*)cornerPriv + CORNER_WAIT_OFFSET);
	else
		m_flWait = 0.0f;

	// looping move sound for this leg (pev->noise1 [484] holds train1)
	EngineEmitSound(EdictFromEntvars(pev), 2, EngineStringFromIndex(PevInt(pev, PEV_NOISE1)),
		PLAT_MOVE_VOLUME, PLAT_MOVE_ATTN);

	// remember the corner we are heading to (used by Arrived to read its
	// "wait for retrigger" spawnflag)
	PevInt(pev, PEV_ENEMY) = EngineIndexOfEdict(cornerEdict);

	SetMoveDone(&CFuncTrain::Arrived);

	// destination = corner origin minus our box centre
	Vector vecCenter;
	Vector vecHalf;
	Vec3Add(VecPtr(vecCenter), VecPtr(PevVector(pev, PEV_MINS)), VecPtr(PevVector(pev, PEV_MAXS)));
	Vec3Scale(VecPtr(vecHalf), VecPtr(vecCenter), 0.5f);

	Vector vecDest;
	Vec3Sub(VecPtr(vecDest), VecPtr(PevVector(pevTarget, PEV_ORIGIN)), VecPtr(vecHalf));

	LinearMove(vecDest, PevFloat(pev, PEV_DMGTIME));

	// pick up a per-corner speed override for the next leg
	if (((*(int*)&PevFloat(pevTarget, PEV_DMGTIME)) & 0x7FFFFFFF) != 0)
		PevFloat(pev, PEV_DMGTIME) = PevFloat(pevTarget, PEV_DMGTIME);
}

//=========================================================
// CFuncTrain::Arrived
//
// Move-done callback.  If the corner we reached is not flagged "wait
// for retrigger", replay the move sound and loop to Next after the
// corner's wait time (immediately when the wait is zero).  Otherwise
// the train parks indefinitely until Used again.
//=========================================================
void CFuncTrain::Arrived(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	// read the spawnflags of the corner we just reached (pev->enemy)
	edict_t* pCorner = EnginePEntityOfEntIndex(PevInt(pev, PEV_ENEMY));
	entvars_t* pevCorner = pCorner ? EngineGetVarsOfEnt(pCorner) : NULL;
	int cornerFlags = pevCorner ? (int)PevFloat(pevCorner, PEV_SPAWNFLAGS) : 0;

	if ((cornerFlags & 1) == 0)
	{
		if (m_flWait > 0.0f)
		{
			// pause m_flWait seconds, then continue to the next corner
			PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + m_flWait;
			EngineEmitSound(EdictFromEntvars(pev), 2,
				EngineStringFromIndex(PevInt(pev, PEV_NOISE)),
				PLAT_MOVE_VOLUME, PLAT_MOVE_ATTN);
			SetThink(&CFuncTrain::Next);
			return;
		}

		if (((*(int*)&m_flWait) & 0x7FFFFFFF) == 0)
		{
			// no wait at all - go straight on
			Next(NULL);
			return;
		}
	}
	else
	{
		// "wait here" corner: replay the move sound and park forever
		EngineEmitSound(EdictFromEntvars(pev), 2,
			EngineStringFromIndex(PevInt(pev, PEV_NOISE)),
			PLAT_MOVE_VOLUME, PLAT_MOVE_ATTN);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(m_pGlobals) + 999999.0f;
		SetThink(&CFuncTrain::Wait);
	}
}

//=========================================================
// CFuncTrain::Use
//
// Kick a parked train (one whose think is still Wait) into motion.
//=========================================================
void CFuncTrain::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	if (m_pfnThink == (ThinkFunc)&CFuncTrain::Wait)
		Next(NULL);
}

//=========================================================
// CFuncTrain::Blocked
//
// Throttled crush damage on whoever blocks us - damages the blocker
// at most twice a second (pev->dmg points each time).
//=========================================================
void CFuncTrain::Blocked(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = m_pGlobals;
	float now = GlobalsTime(globals);

	// m_flActivateFinished gates how often we re-damage
	if (now >= m_flActivateFinished)
	{
		m_flActivateFinished = now + 0.5f;

		int blockerIndex = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);
		edict_t* pBlockerEdict = EnginePEntityOfEntIndex(blockerIndex);
		entvars_t* pevBlocker = pBlockerEdict ? EngineGetVarsOfEnt(pBlockerEdict) : NULL;

		CBaseEntity* pBlocker = pevBlocker ?
			(CBaseEntity*)EngineGetPrivateData(pBlockerEdict) : NULL;

		// crush the blocker for pev->dmg, with the train as inflictor/attacker
		if (pBlocker)
			pBlocker->TakeDamage(pev, pev, PevFloat(pev, PEV_DMG_TOGGLE));
	}
}

//=========================================================
// CFuncTrain::Spawn
//=========================================================
void CFuncTrain::Spawn()
{
	// default speed (pev->dmgtime, 100) and blocking damage (pev->dmg, 2)
	if (((*(int*)&PevFloat(pev, PEV_DMGTIME)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DMGTIME) = 100.0f;

	if (PevInt(pev, PEV_TARGET) == 0)
		EngineAlertMessage(AT_CONSOLE, "FuncTrain with no target", NULL);

	if (((*(int*)&PevFloat(pev, PEV_DMG_TOGGLE)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DMG_TOGGLE) = 2.0f;

	EnginePrecacheSound(kTrainStartSound);
	EnginePrecacheSound(kTrainMoveSound);

	PevInt(pev, PEV_NOISE) = EngineAllocString(kTrainStartSound);	// pev->noise [480]
	PevInt(pev, PEV_NOISE1) = EngineAllocString(kTrainMoveSound);	// pev->noise1 [484]

	PevFloat(pev, PEV_SOLID) = SOLID_BSP;
	PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_PUSH;
	PevInt(pev, PEV_CLASSNAME) = EngineAllocString("train");

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
	{
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));
		EngineSetSize(edict, VecPtr(PevVector(pev, PEV_MINS)), VecPtr(PevVector(pev, PEV_MAXS)));
		EngineSetOrigin(edict, VecPtr(PevVector(pev, PEV_ORIGIN)));
	}

	// start moving almost immediately
	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);
	SetThink(&CFuncTrain::Wait);
}

// CJohnTrain (john_train) was extracted into its own translation unit: john_train.cpp.

//=========================================================
// CTrainInfo - alpha cinematic spawn marker (train_info, data 28).
// A bare CBaseEntity that becomes a SOLID_TRIGGER box, swaps in the
// scientist model on touch, and clears itself.
//=========================================================
class CTrainInfo : public CBaseEntity
{
public:
	void Spawn();
	void Think(CBaseEntity* pOther);
	void Touch(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CTrainInfo) <= 28, CTrainInfo_size);

//=========================================================
// CTrainInfo::Think
//
// Keep the marker solid.
//=========================================================
void CTrainInfo::Think(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_SOLID) = SOLID_TRIGGER;
}

//=========================================================
// CTrainInfo::Touch
//
// Swap the touched scientist over to the standard model, clear its
// dmgtime, and bump its movement field.
//=========================================================
void CTrainInfo::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	// resolve the entity the engine threaded through globals
	int otherIndex = *GlobalsInt(m_pGlobals, GLOBALS_OTHER_ENTINDEX);
	edict_t* pOtherEdict = EnginePEntityOfEntIndex(otherIndex);
	entvars_t* pevOther = pOtherEdict ? EngineGetVarsOfEnt(pOtherEdict) : NULL;

	edict_t* selfEdict = EdictFromEntvars(pev);
	if (selfEdict)
		EngineSetModel(selfEdict, kScientistModel);

	if (pevOther)
	{
		PevFloat(pevOther, PEV_DMGTIME) = 0.0f;
		PevFloat(pevOther, PEV_AVEL_Y) = 150.0f;	// byte 92 on the touched entity
	}
}

//=========================================================
// CTrainInfo::Spawn
//
// Become a SOLID_TRIGGER scientist-sized box, MOVETYPE_NONE.
//=========================================================
void CTrainInfo::Spawn()
{
	PevFloat(pev, PEV_SOLID) = SOLID_TRIGGER;
	PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_NONE;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
	{
		Vector vecMins;
		Vector vecMaxs;
		VecSet(VecPtr(vecMins), -32.0f, -32.0f, -32.0f);
		VecSet(VecPtr(vecMaxs), 32.0f, 32.0f, 32.0f);
		EngineSetSize(edict, VecPtr(vecMins), VecPtr(vecMaxs));
	}
}

//=========================================================
// func_plat
//=========================================================
DLLEXPORT void func_plat(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 144);
		if (!privateData)
			return;

		memset(privateData, 0, 144);

		CFuncPlat* self = new (privateData) CFuncPlat();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_train
//=========================================================
DLLEXPORT void func_train(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 144);
		if (!privateData)
			return;

		memset(privateData, 0, 144);

		CFuncTrain* self = new (privateData) CFuncTrain();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

// john_train export was extracted into john_train.cpp.

//=========================================================
// train_info
//=========================================================
DLLEXPORT void train_info(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 28);
		if (!privateData)
			return;

		memset(privateData, 0, 28);

		CTrainInfo* self = new (privateData) CTrainInfo();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
