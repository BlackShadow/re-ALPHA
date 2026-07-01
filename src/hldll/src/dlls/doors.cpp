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
// Doors - brush-model movers (func_door / func_water /
//         func_door_rotating / momentary_door).
//
// func_door is the canonical CBaseToggle mover (Spawn, Use,
// Blocked).
// func_water shares the func_door class and vtable; it is just a
// func_door whose brush carries water contents (non-zero skin).
// func_door_rotating (Spawn) swings on its angles
// using AngularMove instead of LinearMove.  momentary_door
// (Spawn) is a continuous, position-driven door.
//
// The linear/angular move framework, KeyValue, SUB_UseTargets and
// SetMovedir all live in the CBaseToggle foundation - this file
// only adds the door-specific Spawn / Use / Blocked / move state.
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "cbase_toggle.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// Engine field constants (verified against the door decompiles
// and /hl.dll).
//   solid    : SOLID_NOT(0), SOLID_BSP(4)
//   movetype : MOVETYPE_PUSH(7)
//=========================================================
#define SOLID_NOT			0.0f
#define SOLID_BSP			4.0f
#define MOVETYPE_PUSH		7.0f

//=========================================================
// func_door spawn flags (pev->spawnflags bits used by the door
// family, matched to the decompiled bit masks).
//=========================================================
#define SF_DOOR_START_OPEN		1		// 0x01  swap open/closed at spawn
#define SF_DOOR_PASSABLE		8		// 0x08  brush is non-solid
#define SF_DOOR_ONEWAY			16		// 0x10  rotating: never reverse swing
#define SF_DOOR_NO_AUTO_RETURN	32		// 0x20  toggle (does not auto-close)
#define SF_DOOR_NO_DAMAGE_PUSH	256		// 0x100 do not bump-open on touch

extern int g_CounterActivatorIndex;	// last trigger activator index

//=========================================================
// Door move state (m_toggle_state).  The alpha door uses its own
// integer encoding, distinct from the CBaseToggle TOGGLE_STATE
// enum, so the raw values are spelled out here:
//   0 - fully open (settled at the open endpoint)
//   1 - fully closed (settled at the closed endpoint / spawn)
//   2 - opening   (set while moving toward the open endpoint)
//   3 - closing   (set while moving toward the closed endpoint)
//=========================================================
#define DOOR_STATE_OPEN		0
#define DOOR_STATE_CLOSED	1
#define DOOR_STATE_OPENING	2
#define DOOR_STATE_CLOSING	3

//=========================================================
// pev field offsets that are private to the door family.
//   132 - pev->skin: re-used by func_door as the water/contents
//         selector (0 => normal door, non-zero => func_water).
//   248 - pev->absmin (used to spell out the toggle-door spawn
//         sound origin).
//   496 - pev->dmgtime: re-used by the door as the move speed.
//   500 - pev->dmg: brush blocking damage.
//=========================================================
enum
{
	PEV_DOOR_SKIN		= 132,
	PEV_DOOR_ABSMIN		= 248,
	PEV_DOOR_SPEED		= 496,	// pev->dmgtime aliased as the door speed
	PEV_DOOR_DMG		= 500,	// pev->dmg (brush blocking damage)
};

//=========================================================
// Door move sounds (movesnd 1..8) and stop sounds (stopsnd 1..8).
// movesnd/stopsnd index 0 (or out of range) selects common/null.wav.
//=========================================================
static const char* const kDoorMoveSounds[8] =
{
	"doors/doormove1.wav",
	"doors/doormove2.wav",
	"doors/doormove3.wav",
	"doors/doormove4.wav",
	"doors/doormove5.wav",
	"doors/doormove6.wav",
	"doors/doormove7.wav",
	"doors/doormove8.wav",
};

static const char* const kDoorStopSounds[8] =
{
	"doors/doorstop1.wav",
	"doors/doorstop2.wav",
	"doors/doorstop3.wav",
	"doors/doorstop4.wav",
	"doors/doorstop5.wav",
	"doors/doorstop6.wav",
	"doors/doorstop7.wav",
	"doors/doorstop8.wav",
};

static const char kNullSound[] = "common/null.wav";
static const char kClassFuncDoor[] = "func_door";
static const char kClassFuncDoorRotating[] = "func_door_rotating";
static const char kClassPlayer[] = "player";

//=========================================================
// Local helpers
//=========================================================

//=========================================================
// DoorClassName - the brush model's classname string.
//=========================================================
static const char* DoorClassName(entvars_t* pev)
{
	return EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME));
}

//=========================================================
// CBaseDoor
//
// func_door / func_water.  Linear (or, for the rotating subclass,
// angular) brush mover built on the CBaseToggle move framework.
// Adds no new fields - everything lives in the CBaseToggle layout.
//=========================================================
class CBaseDoor : public CBaseToggle
{
public:
	void Spawn();
	void Use(CBaseEntity* pOther);
	void Blocked(CBaseEntity* pOther);

	void DoorTouch(CBaseEntity* pOther);
	void DoorActivate();
	void DoorGoUp(CBaseEntity* pOther);
	void DoorGoDown(CBaseEntity* pOther);
	void DoorHitTop(CBaseEntity* pOther);
	void DoorHitBottom(CBaseEntity* pOther);

protected:
	// configure the looping move / arrival sounds from movesnd/stopsnd
	void SetDoorSounds();

	// water-content think parked by Spawn when pev->skin is set
	void WaterThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CBaseDoor) <= 132, CBaseDoor_size);

//=========================================================
// CBaseDoor::SetDoorSounds
//
// Precache and bind the looping move sound (pev->noiseMoving) and
// the arrival sound (pev->noiseArrived) selected by movesnd/stopsnd.
//=========================================================
void CBaseDoor::SetDoorSounds()
{
	int moveIndex = (int)m_bMoveSnd;
	const char* moveSound;
	if (moveIndex >= 1 && moveIndex <= 8)
	{
		moveSound = kDoorMoveSounds[moveIndex - 1];
		EnginePrecacheSound(moveSound);
	}
	else
	{
		moveSound = kNullSound;
	}
	PevInt(pev, PEV_NOISE_MOVING) = EngineAllocString(moveSound);

	int stopIndex = (int)m_bStopSnd;
	const char* stopSound;
	if (stopIndex >= 1 && stopIndex <= 8)
	{
		stopSound = kDoorStopSounds[stopIndex - 1];
		EnginePrecacheSound(stopSound);
	}
	else
	{
		stopSound = kNullSound;
	}
	PevInt(pev, PEV_NOISE_ARRIVED) = EngineAllocString(stopSound);
}

//=========================================================
// CBaseDoor::WaterThink
//
// One-shot think installed by Spawn for func_water: announce the
// brush's water volume to the client (msg 23 = a temp-entity /
// brush volume), then clear the think slot.
//=========================================================
void CBaseDoor::WaterThink(CBaseEntity* pOther)
{
	EngineWriteByte(0, 23);
	EngineWriteByte(0, 5);
	EngineWriteByte(0, (int)PevFloat(pev, PEV_DOOR_ABSMIN));
	EngineWriteByte(0, (int)PevFloat(pev, PEV_DOOR_ABSMIN + 4));
	EngineWriteByte(0, (int)PevFloat(pev, PEV_DOOR_ABSMIN + 8));
	EngineWriteByte(0, 1);

	SetThink(NULL);
}

//=========================================================
// CBaseDoor::Spawn
//
// Precache the door sounds, derive movedir from the spawn angles,
// then either set the brush solid (normal door) or schedule the
// water volume announce (func_water, pev->skin != 0).  Computes the
// open/closed endpoints (m_vecPosition1/2) from movedir, size and
// lip, swaps them when SF_DOOR_START_OPEN is set, and installs the
// touch handler unless SF_DOOR_NO_DAMAGE_PUSH is set.
//=========================================================
void CBaseDoor::Spawn()
{
	SetDoorSounds();
	SetMovedir(pev);

	if (((*(int*)&PevFloat(pev, PEV_DOOR_SKIN)) & 0x7FFFFFFF) == 0)
	{
		// normal (non-water) door
		PevFloat(pev, PEV_SOLID) = SOLID_NOT;
		if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & SF_DOOR_PASSABLE) == 0)
			PevFloat(pev, PEV_SOLID) = SOLID_BSP;

		PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_PUSH;
	}
	else
	{
		// func_water: stay non-solid and announce the water volume
		// after one frame.
		PevFloat(pev, PEV_SOLID) = SOLID_NOT;
		SetThink(&CBaseDoor::WaterThink);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(m_pGlobals) + 1.0f;
	}

	// bind the brush model and copy its origin
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetOrigin(edict, VecPtr(PevVector(pev, PEV_ORIGIN)));

	EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	// default speed / blocking damage
	if (((*(int*)&PevFloat(pev, PEV_DOOR_SPEED)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DOOR_SPEED) = 100.0f;
	if (((*(int*)&PevFloat(pev, PEV_DOOR_DMG)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DOOR_DMG) = 2.0f;

	// position1 = spawned (closed) origin
	VecCopy(VecPtr(m_vecPosition1), VecPtr(PevVector(pev, PEV_ORIGIN)));

	// position2 = position1 + movedir * (|size . movedir| - lip)
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	Vector& size = PevVector(pev, PEV_SIZE);
	float travel = fabsf(movedir.y * size.y + movedir.z * size.z + size.x * movedir.x) - m_flLip;
	m_vecPosition2.x = movedir.x * travel + m_vecPosition1.x;
	m_vecPosition2.y = movedir.y * travel + m_vecPosition1.y;
	m_vecPosition2.z = movedir.z * travel + m_vecPosition1.z;

	// SF_DOOR_START_OPEN: drop the brush at the open endpoint, then swap
	// the endpoints (through pev->origin, which now holds the open pos)
	// so position1 becomes the open pos and position2 the closed pos.
	if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & SF_DOOR_START_OPEN) != 0)
	{
		if (edict)
			EngineSetOrigin(edict, VecPtr(m_vecPosition2));

		VecCopy(VecPtr(m_vecPosition2), VecPtr(m_vecPosition1));
		VecCopy(VecPtr(m_vecPosition1), VecPtr(PevVector(pev, PEV_ORIGIN)));
	}

	m_toggle_state = DOOR_STATE_CLOSED;

	// install the bump-open touch handler unless suppressed
	if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & SF_DOOR_NO_DAMAGE_PUSH) != 0)
		SetDoNothingTouch();
	else
		SetTouch(&CBaseDoor::DoorTouch);
}

//=========================================================
// CBaseDoor::DoorTouch
//
// A player bumping a targetname-less door opens it.  Doors with a
// targetname only respond to Use(), so they ignore the touch.
//=========================================================
void CBaseDoor::DoorTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = m_pGlobals;
	edict_t* pToucher = EnginePEntityOfEntIndex(*GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX));
	entvars_t* pevToucher = EngineGetVarsOfEnt(pToucher);

	if (strcmp(EngineStringFromIndex(PevInt(pevToucher, PEV_CLASSNAME)), kClassPlayer) != 0)
		return;

	// a named door is operated only through Use()
	if (PevInt(pev, PEV_TARGETNAME) != 0)
		return;

	SetTouch(NULL);
	m_hActivator = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);
	DoorActivate();
}

//=========================================================
// CBaseDoor::Use
//
// Toggle the door.  Fires when the door is fully closed, or, for a
// no-auto-return door, when it is fully open.
//=========================================================
void CBaseDoor::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	int state = m_toggle_state;
	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	if (state == DOOR_STATE_CLOSED ||
		((flags & SF_DOOR_NO_AUTO_RETURN) != 0 && state == DOOR_STATE_OPEN))
		DoorActivate();
}

//=========================================================
// CBaseDoor::DoorActivate
//
// Drive the door toward its next state: close a no-auto-return door
// that is already open, otherwise open it (crediting health to a
// touching player when m_bHealthValue is set).
//=========================================================
void CBaseDoor::DoorActivate()
{
	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	// a no-auto-return door that is fully open closes again
	if ((flags & SF_DOOR_NO_AUTO_RETURN) != 0 && m_toggle_state == DOOR_STATE_OPEN)
	{
		DoorGoUp(NULL);
		return;
	}

	// award the door's health bonus to a player that opened it
	edict_t* pActEdict = EnginePEntityOfEntIndex(m_hActivator);
	entvars_t* pevAct = EngineGetVarsOfEnt(pActEdict);
	if (strcmp(EngineStringFromIndex(PevInt(pevAct, PEV_CLASSNAME)), kClassPlayer) == 0)
	{
		PevFloat(pevAct, PEV_HEALTH) = (float)m_bHealthValue + PevFloat(pevAct, PEV_HEALTH);
	}

	DoorGoDown(NULL);
}

//=========================================================
// CBaseDoor::DoorGoDown
//
// Start the door moving to its open endpoint (position2 / open
// angle).  Plays the looping move sound, sets the opening state,
// parks DoorHitTop as the move-done callback, then issues the
// linear or angular move.  For a rotating door it also chooses the
// swing direction based on which side the player is on (unless
// one-way).
//=========================================================
void CBaseDoor::DoorGoDown(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = m_pGlobals;
	edict_t* pActEdict = EnginePEntityOfEntIndex(*GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX));
	entvars_t* pevAct = EngineGetVarsOfEnt(pActEdict);

	edict_t* edict = EdictFromEntvars(pev);
	EngineEmitSound(edict, 2, EngineStringFromIndex(PevInt(pev, PEV_NOISE_MOVING)), 1.0f, 0.8f);

	m_toggle_state = DOOR_STATE_OPENING;
	SetMoveDone(&CBaseDoor::DoorHitTop);

	const char* className = DoorClassName(pev);
	if (strcmp(className, kClassFuncDoor) == 0)
	{
		LinearMove(m_vecPosition2, PevFloat(pev, PEV_DOOR_SPEED));
	}
	else if (strcmp(className, kClassFuncDoorRotating) == 0)
	{
		float direction = 1.0f;

		// Fall back to the last trigger activator when current gpGlobals->other
		// is not a player. The binary reads the binary here.
		if (strcmp(EngineStringFromIndex(PevInt(pevAct, PEV_CLASSNAME)), kClassPlayer) != 0)
		{
			edict_t* pFallback = EnginePEntityOfEntIndex(g_CounterActivatorIndex);
			pevAct = pFallback ? EngineGetVarsOfEnt(pFallback) : NULL;
		}

		// non-one-way doors swing away from the player
		if (pevAct &&
			(((int)PevFloat(pev, PEV_SPAWNFLAGS)) & SF_DOOR_ONEWAY) == 0 &&
			strcmp(EngineStringFromIndex(PevInt(pevAct, PEV_CLASSNAME)), kClassPlayer) == 0)
		{
			// only meaningful when the door swings about a Y-bearing axis
			if (((*(int*)&PevVector(pev, PEV_MOVEDIR).y) & 0x7FFFFFFF) != 0)
			{
				Vector& actAngles = PevVector(pevAct, PEV_ANGLES);
				Vector& actOrigin = PevVector(pevAct, PEV_ORIGIN);
				Vector& doorOrigin = PevVector(pev, PEV_ORIGIN);

				// build the forward vector from the player's flattened yaw
				Vector facing;
				facing.x = 0.0f;
				facing.y = actAngles.y;
				facing.z = 0.0f;
				EngineMakeVectors(VecPtr(facing));

				// cross product of (player - door) against the player's
				// in-plane velocity decides which side opens away
				Vector& vel = PevVector(pevAct, PEV_VELOCITY);
				float cross =
					((vel.y * 10.0f + actOrigin.y) - doorOrigin.y) * (actOrigin.x - doorOrigin.x) -
					((vel.x * 10.0f + actOrigin.x) - doorOrigin.x) * (actOrigin.y - doorOrigin.y);
				if (cross < 0.0f)
					direction = -1.0f;
			}
		}

		Vector destAngle;
		destAngle.x = m_vecPosition2.x * direction;
		destAngle.y = m_vecPosition2.y * direction;
		destAngle.z = m_vecPosition2.z * direction;
		AngularMove(destAngle, PevFloat(pev, PEV_DOOR_SPEED));
	}

	SUB_UseTargets(NULL, 0, 0.0f);
}

//=========================================================
// CBaseDoor::DoorGoUp
//
// Start the door moving back to its closed endpoint (position1 /
// closed angle).  Plays the move sound, sets the closing state,
// parks DoorHitBottom as the move-done callback, then issues the
// move.
//=========================================================
void CBaseDoor::DoorGoUp(CBaseEntity* pOther)
{
	edict_t* edict = EdictFromEntvars(pev);
	EngineEmitSound(edict, 2, EngineStringFromIndex(PevInt(pev, PEV_NOISE_MOVING)), 1.0f, 0.8f);

	m_toggle_state = DOOR_STATE_CLOSING;
	SetMoveDone(&CBaseDoor::DoorHitBottom);

	const char* className = DoorClassName(pev);
	if (strcmp(className, kClassFuncDoor) == 0)
	{
		LinearMove(m_vecPosition1, PevFloat(pev, PEV_DOOR_SPEED));
	}
	else if (strcmp(className, kClassFuncDoorRotating) == 0)
	{
		AngularMove(m_vecPosition1, PevFloat(pev, PEV_DOOR_SPEED));
	}
}

//=========================================================
// CBaseDoor::DoorHitTop
//
// Arrived at the open endpoint: play the stop sound and clear the
// move state.  A no-auto-return door just re-arms its bump-open
// touch handler; otherwise it schedules DoorGoUp after m_flWait
// seconds (or stays open forever when m_flWait == -1).
//=========================================================
void CBaseDoor::DoorHitTop(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	edict_t* edict = EdictFromEntvars(pev);
	EngineEmitSound(edict, 2, EngineStringFromIndex(PevInt(pev, PEV_NOISE_ARRIVED)), 1.0f, 0.8f);

	m_toggle_state = DOOR_STATE_OPEN;

	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);
	if ((flags & SF_DOOR_NO_AUTO_RETURN) != 0)
	{
		// re-arm the bump-open touch unless the door ignores pushes
		if ((flags & SF_DOOR_NO_DAMAGE_PUSH) == 0)
			SetTouch(&CBaseDoor::DoorTouch);
	}
	else
	{
		PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + m_flWait;
		SetThink(&CBaseDoor::DoorGoUp);

		// m_flWait == -1 : stay open (cancel the auto-close think)
		if ((*(int*)&m_flWait) == (int)0xBF800000)
			PevFloat(pev, PEV_NEXTTHINK) = -1.0f;
	}
}

//=========================================================
// CBaseDoor::DoorHitBottom
//
// Arrived back at the closed endpoint: play the stop sound, mark the
// door closed and re-arm the bump-open touch unless the door ignores
// pushes.
//=========================================================
void CBaseDoor::DoorHitBottom(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	edict_t* edict = EdictFromEntvars(pev);
	EngineEmitSound(edict, 2, EngineStringFromIndex(PevInt(pev, PEV_NOISE_ARRIVED)), 1.0f, 0.8f);

	m_toggle_state = DOOR_STATE_CLOSED;

	if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & SF_DOOR_NO_DAMAGE_PUSH) != 0)
		SetDoNothingTouch();
	else
		SetTouch(&CBaseDoor::DoorTouch);
}

//=========================================================
// CBaseDoor::Blocked
//
// Damage whatever is blocking the door, then reverse a mid-move
// door.  Finally propagate the reversal to every other func_door
// sharing this door's targetname so linked door panels stay in sync.
//=========================================================
void CBaseDoor::Blocked(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	// hurt the blocking entity
	void* globals = m_pGlobals;
	edict_t* pBlockerEdict = EnginePEntityOfEntIndex(*GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX));
	void* blockerPriv = EngineGetPrivateData(pBlockerEdict);
	if (blockerPriv)
	{
		CBaseEntity* pBlocker = (CBaseEntity*)blockerPriv;
		pBlocker->TakeDamage(pev, pev, PevFloat(pev, PEV_DOOR_DMG));
	}

	// reverse our own swing if we were mid-move (a "no reverse"
	// door, m_flWait bit pattern > 0x80000000, is skipped)
	if ((unsigned int)(*(int*)&m_flWait) < 0x80000001u)
	{
		if (m_toggle_state == DOOR_STATE_CLOSING)
			DoorGoDown(NULL);
		else
			DoorGoUp(NULL);
	}

	// reverse every other func_door that shares our targetname
	const char* pszTargetName = EngineStringFromIndex(PevInt(pev, PEV_TARGETNAME));
	edict_t* pTarget = NULL;
	while ((pTarget = EngineFindEntityByString(pTarget, "targetname", pszTargetName)) != NULL)
	{
		if (EngineIndexOfEdict(pTarget) == 0)
			break;

		entvars_t* pevTarget = EngineGetVarsOfEnt(pTarget);
		if (strcmp(EngineStringFromIndex(PevInt(pevTarget, PEV_CLASSNAME)), kClassFuncDoor) != 0)
			continue;

		// resolve the linked door's entvars; spawn a fresh entity if the
		// found edict somehow has none (defensive, matches the binary)
		if (!pevTarget)
		{
			edict_t* created = EngineCreateEntity();
			pevTarget = created ? EngineGetVarsOfEnt(created) : NULL;
		}

		edict_t* pTargetEdict = EdictFromEntvars(pevTarget);
		if (!pTargetEdict)
			continue;

		void* priv = EngineGetPrivateData(pTargetEdict);
		CBaseDoor* pDoor;
		if (priv)
		{
			pDoor = (CBaseDoor*)priv;
		}
		else
		{
			priv = EngineAllocPrivateData(pTargetEdict, 132);
			if (!priv)
				continue;

			// install the func_door vtable on the freshly bound entity
			pDoor = new (priv) CBaseDoor();
			pDoor->pev = pevTarget;
			pDoor->m_pGlobals = GlobalsFromEntvars(pevTarget);
		}

		if ((unsigned int)(*(int*)&pDoor->m_flWait) < 0x80000001u)
		{
			// copy our origin onto the linked door before reversing it
			VecCopy(VecPtr(PevVector(pevTarget, PEV_ORIGIN)), VecPtr(PevVector(pev, PEV_ORIGIN)));

			if (pDoor->m_toggle_state == DOOR_STATE_CLOSING)
				pDoor->DoorGoDown(NULL);
			else
				pDoor->DoorGoUp(NULL);
		}
	}
}

//=========================================================
// CRotDoor
//
// func_door_rotating.  Shares all of CBaseDoor's move logic; only
// Spawn differs (it swings on its angles instead of sliding).
//=========================================================
class CRotDoor : public CBaseDoor
{
public:
	void Spawn();
};

HL_COMPILE_TIME_ASSERT(sizeof(CRotDoor) <= 132, CRotDoor_size);

//=========================================================
// SetMovedirRotate
//
// Pick the rotation axis (pev->movedir) for a rotating door from
// the rotate-axis spawnflags:
//   bit 0x40 -> Z axis (0,0,1)   bit 0x80 -> X axis (1,0,0)
//   default  -> Y axis (0,1,0)
//=========================================================
static void SetMovedirRotate(entvars_t* pev)
{
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	if ((flags & 0x40) != 0)
	{
		VecSet(VecPtr(movedir), 0.0f, 0.0f, 1.0f);
	}
	else if ((flags & 0x80) != 0)
	{
		VecSet(VecPtr(movedir), 1.0f, 0.0f, 0.0f);
	}
	else
	{
		VecSet(VecPtr(movedir), 0.0f, 1.0f, 0.0f);
	}
}

//=========================================================
// CRotDoor::Spawn
//
// Set up a rotating door: precache sounds, pick the rotation axis,
// reverse it for the rotate-backwards flag, compute the open angle
// (position2 = angles + movedir * distance), make the brush a solid
// pusher, then (when starting open) display it at the open angle and
// fold the endpoints so position2 becomes the closed angle.
//=========================================================
void CRotDoor::Spawn()
{
	SetDoorSounds();
	SetMovedirRotate(pev);

	Vector& movedir = PevVector(pev, PEV_MOVEDIR);

	// rotate-backwards flag (0x2): swing the door the other way
	if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & 0x2) != 0)
	{
		movedir.x = -movedir.x;
		movedir.y = -movedir.y;
		movedir.z = -movedir.z;
	}

	m_flWait = 2.0f;	// [48] default wait slot value set by the alpha
	float distance = m_flMoveDistance;

	Vector& angles = PevVector(pev, PEV_ANGLES);
	VecCopy(VecPtr(m_vecPosition1), VecPtr(angles));

	m_vecPosition2.x = movedir.x * distance + angles.x;
	m_vecPosition2.y = movedir.y * distance + angles.y;
	m_vecPosition2.z = movedir.z * distance + angles.z;

	PevFloat(pev, PEV_SOLID) = SOLID_BSP;
	PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_PUSH;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetOrigin(edict, VecPtr(PevVector(pev, PEV_ORIGIN)));

	EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	if (((*(int*)&PevFloat(pev, PEV_DOOR_SPEED)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DOOR_SPEED) = 100.0f;
	if (((*(int*)&PevFloat(pev, PEV_DOOR_DMG)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DOOR_DMG) = 2.0f;

	if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & SF_DOOR_START_OPEN) != 0)
	{
		// spawn already open: render at the open angle, then fold the
		// endpoints (position2 := position1) and flip movedir back.
		VecCopy(VecPtr(angles), VecPtr(m_vecPosition2));
		VecCopy(VecPtr(m_vecPosition2), VecPtr(m_vecPosition1));

		movedir.x = -movedir.x;
		movedir.y = -movedir.y;
		movedir.z = -movedir.z;
	}

	m_toggle_state = DOOR_STATE_CLOSED;

	if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & SF_DOOR_NO_DAMAGE_PUSH) != 0)
		SetDoNothingTouch();
	else
		SetTouch(&CBaseDoor::DoorTouch);
}

//=========================================================
// CMomentaryDoor
//
// momentary_door.  A continuously positioned brush mover whose open
// fraction is driven directly by the activator's Use value rather
// than by a toggle.  Built on CBaseToggle but uses only the linear
// move framework and its own KeyValue/Use.
//=========================================================
class CMomentaryDoor : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pOther);
};

// momentary_door allocates 128 bytes (it shares the CBaseToggle
// layout but, like the trigger_* family, never touches the sound
// bytes at [128..130]); the C++ object still spans the full 132.
HL_COMPILE_TIME_ASSERT(sizeof(CMomentaryDoor) <= 132, CMomentaryDoor_size);

//=========================================================
// CMomentaryDoor::Spawn
//
// Solid pusher set up like a func_door, with position1/position2
// computed from movedir, size and lip.  No move-done callback runs
// (the move is continuous), so the callback slot is cleared.
//=========================================================
void CMomentaryDoor::Spawn()
{
	SetMovedir(pev);

	PevFloat(pev, PEV_SOLID) = SOLID_BSP;
	PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_PUSH;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetOrigin(edict, VecPtr(PevVector(pev, PEV_ORIGIN)));

	EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	if (((*(int*)&PevFloat(pev, PEV_DOOR_SPEED)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DOOR_SPEED) = 100.0f;
	if (((*(int*)&PevFloat(pev, PEV_DOOR_DMG)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DOOR_DMG) = 2.0f;

	VecCopy(VecPtr(m_vecPosition1), VecPtr(PevVector(pev, PEV_ORIGIN)));

	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	Vector& size = PevVector(pev, PEV_SIZE);
	float travel = fabsf(movedir.y * size.y + movedir.z * size.z + size.x * movedir.x) - m_flLip;
	m_vecPosition2.x = movedir.x * travel + m_vecPosition1.x;
	m_vecPosition2.y = movedir.y * travel + m_vecPosition1.y;
	m_vecPosition2.z = movedir.z * travel + m_vecPosition1.z;

	// momentary doors install the original null move-done callback
	SetMoveDoneNoop();

	if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & SF_DOOR_START_OPEN) != 0)
	{
		if (edict)
			EngineSetOrigin(edict, VecPtr(m_vecPosition2));

		VecCopy(VecPtr(m_vecPosition2), VecPtr(m_vecPosition1));
		VecCopy(VecPtr(m_vecPosition1), VecPtr(PevVector(pev, PEV_ORIGIN)));
	}
}

//=========================================================
// CMomentaryDoor::KeyValue
//
// Only "lip" and "distance" carry data; movesnd / stopsnd /
// healthvalue are accepted (marked handled) but ignored.
//=========================================================
void CMomentaryDoor::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "lip") == 0)
	{
		m_flLip = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "distance") == 0)
	{
		m_flMoveDistance = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "movesnd") == 0)
	{
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "stopsnd") == 0)
	{
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "healthvalue") == 0)
	{
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CMomentaryDoor::Use
//
// Drive the door to a fraction of its travel:
//   dest = position1 + (position2 - position1) * clamp(fraction, .., 1)
// then LinearMove there at the door speed.  The original receives the
// momentary fraction through the first Use argument as a float pointer;
// a null argument means an ordinary target fire and is ignored.
//=========================================================
void CMomentaryDoor::Use(CBaseEntity* pOther)
{
	if (!pOther)
		return;

	float fraction = *(float*)pOther;
	if ((*(int*)&fraction) > 0x3F800000)	// > 1.0f
		fraction = 1.0f;

	float speed = PevFloat(pev, PEV_DOOR_SPEED);

	// dest = position1 + (position2 - position1) * fraction
	Vector delta;
	delta.x = m_vecPosition2.x - m_vecPosition1.x;
	delta.y = m_vecPosition2.y - m_vecPosition1.y;
	delta.z = m_vecPosition2.z - m_vecPosition1.z;

	Vector scaled;
	scaled.x = delta.x * fraction;
	scaled.y = delta.y * fraction;
	scaled.z = delta.z * fraction;

	Vector dest;
	dest.x = m_vecPosition1.x + scaled.x;
	dest.y = m_vecPosition1.y + scaled.y;
	dest.z = m_vecPosition1.z + scaled.z;

	LinearMove(dest, speed);
}

//=========================================================
// Entity construction wrappers
//=========================================================

//=========================================================
// func_door (export) - SIZE 132
//=========================================================
DLLEXPORT void func_door(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 132);
		if (!privateData)
			return;

		memset(privateData, 0, 132);

		CBaseDoor* self = new (privateData) CBaseDoor();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_water (export) - SIZE 132
//
// Shares the CBaseDoor class and vtable with func_door.
//=========================================================
DLLEXPORT void func_water(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 132);
		if (!privateData)
			return;

		memset(privateData, 0, 132);

		CBaseDoor* self = new (privateData) CBaseDoor();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_door_rotating (export) - SIZE 132
//=========================================================
DLLEXPORT void func_door_rotating(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 132);
		if (!privateData)
			return;

		memset(privateData, 0, 132);

		CRotDoor* self = new (privateData) CRotDoor();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// momentary_door (export) - SIZE 128
//=========================================================
DLLEXPORT void momentary_door(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, sizeof(CMomentaryDoor));
		if (!privateData)
			return;

		memset(privateData, 0, sizeof(CMomentaryDoor));

		CMomentaryDoor* self = new (privateData) CMomentaryDoor();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
