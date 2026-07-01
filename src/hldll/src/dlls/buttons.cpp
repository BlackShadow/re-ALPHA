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
// Buttons - func_button, func_rot_button, momentary_rot_button
//           and the multisource master/counter entity.
//
// CMultiSource (multisource: CBaseEntity) - Spawn, KeyValue, Use,
//   Register, RegisterThink.
// CBaseButton (func_button: CBaseToggle) - Spawn, KeyValue, ButtonUse,
//   ButtonTouch, TakeDamage, ButtonActivate, TriggerAndWait,
//   ButtonReturn, ButtonBackHome, ButtonSpark.
// CRotButton (func_rot_button) - Spawn (AngularMove).
// CMomentaryRotButton (momentary_rot_button: CBaseEntity) - Spawn,
//   KeyValue, Use, think + helper routines.
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include "cbase_toggle.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// entvars_t byte offsets used by the brush movers that are not
// already named in utils.h.
//   476 - pev->sounds   : button sound index (KeyValue "sounds")
//   480 - pev->noise    : the bound press-sound string index
//   496 - pev->speed    : mover speed (aliases the door's 496 field)
//=========================================================
enum
{
	PEV_SOUNDS	= 476,
	PEV_NOISE	= 480,
	PEV_SPEED	= 496,
};

//=========================================================
// globalvars_t byte offsets the engine fills before a Use/Touch
// dispatch (self and activator entity indices).
//=========================================================
enum
{
	BUTTON_GLOBALS_SELF_ENTINDEX		= 112,
	BUTTON_GLOBALS_ACTIVATOR_ENTINDEX	= 116,
};

//=========================================================
// Mover constants (decoded from the IDA float-bit literals).
//=========================================================
#define BUTTON_MOVETYPE_PUSH	7.0f	// MOVETYPE_PUSH
#define BUTTON_SOLID_BSP		4.0f	// SOLID_BSP
#define BUTTON_DEFAULT_SPEED	40.0f	// func_button default pev->speed
#define MOMENTARY_DEFAULT_SPEED	100.0f	// momentary_rot_button default speed
#define BUTTON_SHOT_HEALTH		9999.0f	// re-armed health after a shot trigger
#define BUTTON_SND_VOLUME		1.0f
#define BUTTON_SND_ATTN			0.8f	// ATTN_NORM

//=========================================================
// Spawnflag bits (read from pev->spawnflags).
//=========================================================
#define SF_ROTBUTTON_BACKWARDS	0x0002	// func_rot_button: spin the other way
#define SF_BUTTON_TOGGLE		0x0020	// stay in the down state until used again
#define SF_BUTTON_SPARK			0x0040	// emit sparks while idle
#define SF_BUTTON_TOUCH_ACTIVATE 0x0100	// touch (not +use) presses the button

//=========================================================
// Button move state (pev-private [36]).  The alpha buttons use raw
// values that do NOT match the foundation TOGGLE_STATE enum:
//   0 = pressed / at the top      1 = home / at the bottom
//   2 = moving down (pressing)    3 = moving up (returning)
//=========================================================
#define BS_PRESSED		0
#define BS_HOME			1
#define BS_GOING_DOWN	2
#define BS_GOING_UP		3

//=========================================================
// Button press sounds. Index 0 is silence; 1..11
// map to buttons/buttonN.wav, anything else falls back to button9.
//=========================================================
static const char* ButtonSound(int sound)
{
	switch (sound)
	{
	case 0:		return "common/null.wav";
	case 1:		return "buttons/button1.wav";
	case 2:		return "buttons/button2.wav";
	case 3:		return "buttons/button3.wav";
	case 4:		return "buttons/button4.wav";
	case 5:		return "buttons/button5.wav";
	case 6:		return "buttons/button6.wav";
	case 7:		return "buttons/button7.wav";
	case 8:		return "buttons/button8.wav";
	case 9:		return "buttons/button9.wav";
	case 10:	return "buttons/button10.wav";
	case 11:	return "buttons/button11.wav";
	default:	return "buttons/button9.wav";
	}
}

//=========================================================
// Idle spark sounds.
//=========================================================
static const char* kSparkSounds[6] =
{
	"buttons/spark1.wav",
	"buttons/spark2.wav",
	"buttons/spark3.wav",
	"buttons/spark4.wav",
	"buttons/spark5.wav",
	"buttons/spark6.wav",
};

//=========================================================
// AxisDir - derive a rotation axis movedir from the
// pitch/roll spawnflag bits (shared by every rotating brush).
//   bit 0x40 -> Z axis    bit 0x80 -> X axis    else -> Y axis
//=========================================================
static void AxisDir(entvars_t* pev)
{
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	if ((flags & 0x40) != 0)
		VecSet(VecPtr(movedir), 0.0f, 0.0f, 1.0f);
	else if ((flags & 0x80) != 0)
		VecSet(VecPtr(movedir), 1.0f, 0.0f, 0.0f);
	else
		VecSet(VecPtr(movedir), 0.0f, 1.0f, 0.0f);
}

//=========================================================
// AxisDelta - component of (a - b) on the rotation
// axis selected by the same pitch/roll spawnflag bits.
//=========================================================
static float AxisDelta(int spawnflags, const float* a, const float* b)
{
	if ((spawnflags & 0x40) != 0)
		return a[2] - b[2];
	if ((spawnflags & 0x80) != 0)
		return a[0] - b[0];
	return a[1] - b[1];
}

//=========================================================
// FindOrMakeEntity - resolve an edict's CBaseEntity private data,
// allocating a bare CBaseEntity (28 bytes) when none exists yet.
// Mirrors the inline get-or-create the IDA repeats for every
// targetname dispatch.
//=========================================================
static CBaseEntity* FindOrMakeEntity(edict_t* pEdict)
{
	entvars_t* pevTarget = EngineGetVarsOfEnt(pEdict);
	edict_t* edict = EdictFromEntvars(pevTarget);
	if (!edict)
	{
		edict = EngineCreateEntity();
		pevTarget = edict ? EngineGetVarsOfEnt(edict) : NULL;
		if (!edict)
			return NULL;
	}

	void* priv = EngineGetPrivateData(edict);
	if (priv)
		return (CBaseEntity*)priv;

	priv = EngineAllocPrivateData(edict, 28);
	if (!priv)
		return NULL;

	CBaseEntity* pEntity = new (priv) CBaseEntity();
	pEntity->pev = pevTarget;
	pEntity->m_pGlobals = GlobalsFromEntvars(pevTarget);
	return pEntity;
}

//=========================================================
// CMultiSource (multisource)
//
// CBaseEntity (28 bytes) plus a counter pair (size 36):
//   [28] m_iCount   - triggers not yet satisfied (counts down to 0)
//   [32] m_iTotal   - total number of triggers feeding us
//
// Each trigger that fires us decrements m_iCount; when it hits 0 (all
// triggers set) we fire pev->target.  A trigger that resets calls
// Register, bumping m_iCount back up toward m_iTotal.
//=========================================================
class CMultiSource : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pOther);

	void Register();
	void RegisterThink(CBaseEntity* pOther);
	int IsTriggered();

	int m_iCount;	// [28]
	int m_iTotal;	// [32]
};

HL_COMPILE_TIME_ASSERT(sizeof(CMultiSource) <= 36, CMultiSource_size);

//=========================================================
// ResolveMultiSource - resolve the multisource private data behind an
// edict, allocating (and installing the CMultiSource vtable) when it
// has not spawned yet.  Used by buttons to talk to their master.
//=========================================================
static CMultiSource* ResolveMultiSource(entvars_t* pevSource)
{
	edict_t* edict = EdictFromEntvars(pevSource);
	if (!edict)
	{
		edict = EngineCreateEntity();
		pevSource = edict ? EngineGetVarsOfEnt(edict) : NULL;
		if (!edict)
			return NULL;
	}

	void* priv = EngineGetPrivateData(edict);
	if (priv)
		return (CMultiSource*)priv;

	priv = EngineAllocPrivateData(edict, 36);
	if (!priv)
		return NULL;

	CMultiSource* pSource = new (priv) CMultiSource();
	pSource->pev = pevSource;
	pSource->m_pGlobals = GlobalsFromEntvars(pevSource);
	return pSource;
}

//=========================================================
// CMultiSource::KeyValue
//
// Silently consumes the editor keys that have no runtime meaning.
//=========================================================
void CMultiSource::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "style") == 0
		|| strcmp(pkvd->szKeyName, "height") == 0
		|| strcmp(pkvd->szKeyName, "killtarget") == 0
		|| strcmp(pkvd->szKeyName, "value1") == 0
		|| strcmp(pkvd->szKeyName, "value2") == 0
		|| strcmp(pkvd->szKeyName, "value3") == 0)
	{
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CMultiSource::RegisterThink
//
// Deferred one-shot: count how many entities target us (capped at 10)
// so IsTriggered knows when "all" of them are set.
//=========================================================
void CMultiSource::RegisterThink(CBaseEntity* pOther)
{
	m_iTotal = 0;
	m_iCount = 0;
	SetDoNothingThink();	// SUB_DoNothing

	const char* pszName = EngineStringFromIndex(PevInt(pev, PEV_TARGETNAME));
	edict_t* pTarget = EngineFindEntityByString(NULL, "target", pszName);

	while (pTarget && EngineIndexOfEdict(pTarget) != 0 && m_iTotal < 10)
	{
		m_iTotal++;

		// Walk the engine-built match chain (pev->chain) to the next
		// entity targeting us, exactly as the binary does.
		entvars_t* pevTarget = EngineGetVarsOfEnt(pTarget);
		int chainIndex = pevTarget ? PevInt(pevTarget, PEV_CHAIN) : 0;
		pTarget = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
	}

	m_iCount = m_iTotal;
}

//=========================================================
// CMultiSource::Spawn
//
// Defer the trigger count by one frame (so every targeting entity has
// spawned), and accept Use() calls.
//=========================================================
void CMultiSource::Spawn()
{
	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + 1.0f;
	SetThink(&CMultiSource::RegisterThink);
	SetUse(&CMultiSource::Use);
}

//=========================================================
// CMultiSource::Use
//
// One of our triggers reset.  Decrement the satisfied count; when it
// hits zero, fire pev->target.
//=========================================================
void CMultiSource::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	if (m_iCount <= 0)
		return;

	m_iCount--;
	if (m_iCount != 0)
		return;

	if (PevInt(pev, PEV_TARGET) == 0)
		return;

	void* globals = m_pGlobals;
	int oldSelf = *GlobalsInt(globals, BUTTON_GLOBALS_SELF_ENTINDEX);
	int oldActivator = *GlobalsInt(globals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX);

	const char* pszTarget = EngineStringFromIndex(PevInt(pev, PEV_TARGET));
	edict_t* pTarget = NULL;

	while ((pTarget = EngineFindEntityByString(pTarget, "targetname", pszTarget)) != NULL)
	{
		if (EngineIndexOfEdict(pTarget) == 0)
			break;

		*GlobalsInt(globals, BUTTON_GLOBALS_SELF_ENTINDEX) = EngineIndexOfEdict(pTarget);
		*GlobalsInt(globals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX) = oldSelf;

		CBaseEntity* pEntity = FindOrMakeEntity(pTarget);
		if (pEntity)
			pEntity->Use(NULL);

		*GlobalsInt(globals, BUTTON_GLOBALS_SELF_ENTINDEX) = oldSelf;
		*GlobalsInt(globals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX) = oldActivator;
	}
}

//=========================================================
// CMultiSource::Register
//
// One of our triggers became set.  Bump the satisfied count toward the
// total registered at spawn.
//=========================================================
void CMultiSource::Register()
{
	if (m_iTotal > m_iCount)
		m_iCount++;
}

//=========================================================
// CMultiSource::IsTriggered - the multisource is "on" once every
// trigger has fired in (the unsatisfied count has reached 0).
//=========================================================
int CMultiSource::IsTriggered()
{
	return (m_iCount == 0) ? 1 : 0;
}

//=========================================================
// CBaseButton (func_button / func_rot_button)
//
// CBaseToggle (132 bytes) plus three fields:
//   [336] m_fStayPushed   (pev->wait == -1)
//   [340] m_fRotating     (0 func_button, 1 func_rot_button)
//   [344] m_sMaster       (KeyValue "master")
//=========================================================
class CBaseButton : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	int TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage);

	void ButtonUse(CBaseEntity* pOther);
	void ButtonTouch(CBaseEntity* pOther);
	void ButtonSpark(CBaseEntity* pOther);

	void ButtonActivate();
	void TriggerAndWait(CBaseEntity* pOther);
	void ButtonReturn(CBaseEntity* pOther);
	void ButtonBackHome(CBaseEntity* pOther);

	void PlayPressSound();

	int m_fStayPushed;		// [336]
	int m_fRotating;		// [340]
	int m_sMaster;			// [344]
};

HL_COMPILE_TIME_ASSERT(sizeof(CBaseButton) <= 348, CBaseButton_size);

//=========================================================
// CRotButton - func_rot_button.  Only the spawn differs (rotation
// axis + AngularMove); every other method is shared with CBaseButton.
//=========================================================
class CRotButton : public CBaseButton
{
public:
	void Spawn();
};

HL_COMPILE_TIME_ASSERT(sizeof(CRotButton) <= 348, CRotButton_size);

//=========================================================
// CBaseButton::PlayPressSound - the bound press wav on CHAN_VOICE.
//=========================================================
void CBaseButton::PlayPressSound()
{
	edict_t* edict = EdictFromEntvars(pev);
	EngineEmitSound(edict, 2, EngineStringFromIndex(PevInt(pev, PEV_NOISE)), BUTTON_SND_VOLUME, BUTTON_SND_ATTN);
}

//=========================================================
// CBaseButton::KeyValue
//
// "lip","wait","delay","distance" go straight into the toggle
// fields; "master" stores the multisource name index.
//=========================================================
void CBaseButton::KeyValue(KeyValueData* pkvd)
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
	else if (strcmp(pkvd->szKeyName, "delay") == 0)
	{
		m_flDelay = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "distance") == 0)
	{
		m_flMoveDistance = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "master") == 0)
	{
		m_sMaster = EngineAllocString(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CBaseButton::ButtonSpark
//
// Idle think that scatters a sparks temp-entity around the button
// centre and plays a random spark sound, then re-schedules itself.
//=========================================================
void CBaseButton::ButtonSpark(CBaseEntity* pOther)
{
	SetThink(&CBaseButton::ButtonSpark);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + RandomFloat(0.0f, 1.5f) + 0.1f;

	Vector& mins = PevVector(pev, PEV_MINS);
	Vector& size = PevVector(pev, PEV_SIZE);

	// TE_SPARKS (svc_temp_entity=23, type=9) at a random point inside
	// the brush bounds.
	EngineWriteByte(0, 23);
	EngineWriteByte(0, 9);
	EngineWriteCoord(0, size.x * 0.5f + mins.x);
	EngineWriteCoord(0, size.y * 0.5f + mins.y);
	EngineWriteCoord(0, size.z * 0.5f + mins.z);

	float volume = RandomFloat(0.25f, 0.75f);
	int index = (int)(RandomFloat(0.0f, 1.0f) * 6.0f);
	if (index < 0 || index > 5)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	EngineEmitSound(edict, 2, kSparkSounds[index], volume, BUTTON_SND_ATTN);
}

//=========================================================
// CBaseButton::ButtonActivate
//
// Begin pressing: play the press sound, enter the going-down state, install
// TriggerAndWait as the move-done callback and glide to position2
// (linear for func_button, angular for func_rot_button).
//=========================================================
void CBaseButton::ButtonActivate()
{
	PlayPressSound();

	m_toggle_state = BS_GOING_DOWN;
	SetMoveDone(&CBaseButton::TriggerAndWait);

	float speed = PevFloat(pev, PEV_SPEED);

	if (m_fRotating == 0)
		LinearMove(m_vecPosition2, speed);
	else
		AngularMove(m_vecPosition2, speed);
}

//=========================================================
// CBaseButton::ButtonReturn
//
// Enter the going-up state and glide back to position1, installing
// ButtonBackHome as the move-done callback.  Clears pev->frame.
//=========================================================
void CBaseButton::ButtonReturn(CBaseEntity* pOther)
{
	m_toggle_state = BS_GOING_UP;
	SetMoveDone(&CBaseButton::ButtonBackHome);

	float speed = PevFloat(pev, PEV_SPEED);

	if (m_fRotating == 0)
		LinearMove(m_vecPosition1, speed);
	else
		AngularMove(m_vecPosition1, speed);

	PevFloat(pev, PEV_FRAME) = 0.0f;
}

//=========================================================
// CBaseButton::TriggerAndWait
//
// Move-done callback at the bottom: settle into the pressed state, fire
// targets, then either hold pushed (toggle / stay-pushed) or wait
// pev->wait seconds before returning home.
//=========================================================
void CBaseButton::TriggerAndWait(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	m_toggle_state = BS_PRESSED;

	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	if (m_fStayPushed != 0 || (flags & SF_BUTTON_TOGGLE) != 0)
	{
		// Stays pushed: drop the touch handler (re-arm touch presses if
		// SF_BUTTON_TOUCH_ACTIVATE is set).
		SetDoNothingTouch();
		if ((flags & SF_BUTTON_TOUCH_ACTIVATE) != 0)
			SetTouch(&CBaseButton::ButtonTouch);
	}
	else
	{
		// Schedule the automatic return.
		PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + m_flWait;
		SetThink(&CBaseButton::ButtonReturn);
	}

	PevFloat(pev, PEV_FRAME) = 1.0f;
	SUB_UseTargets(NULL, 0, 0.0f);
}

//=========================================================
// CBaseButton::ButtonBackHome
//
// Move-done callback back at the top: enter the home state, notify every
// multisource named by pev->target that this trigger is now set, then
// re-arm the touch / spark handlers.
//=========================================================
void CBaseButton::ButtonBackHome(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	m_toggle_state = BS_HOME;

	if (PevInt(pev, PEV_TARGET) != 0)
	{
		const char* pszTarget = EngineStringFromIndex(PevInt(pev, PEV_TARGET));
		edict_t* pTarget = NULL;

		while ((pTarget = EngineFindEntityByString(pTarget, "targetname", pszTarget)) != NULL)
		{
			if (EngineIndexOfEdict(pTarget) == 0)
				break;

			entvars_t* pevTarget = EngineGetVarsOfEnt(pTarget);
			if (strcmp(EngineStringFromIndex(PevInt(pevTarget, PEV_CLASSNAME)), "multisource") != 0)
				continue;

			CMultiSource* pSource = ResolveMultiSource(pevTarget);
			if (pSource)
				pSource->Register();
		}
	}

	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	SetDoNothingTouch();
	if ((flags & SF_BUTTON_TOUCH_ACTIVATE) != 0)
		SetTouch(&CBaseButton::ButtonTouch);

	if ((flags & SF_BUTTON_SPARK) != 0)
	{
		SetThink(&CBaseButton::ButtonSpark);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.5f;
	}
}

//=========================================================
// CBaseButton::ButtonUse
//
// +use handler.  Ignored while the button is moving; otherwise stash
// the activator, then either start pressing (ButtonActivate) or, for a
// toggle button already up, fire targets and return home.
//=========================================================
void CBaseButton::ButtonUse(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	// Leftover alpha debug spew (head).
	EngineAlertMessage(1, "%d\n", m_fStayPushed);

	if (m_toggle_state == BS_GOING_DOWN || m_toggle_state == BS_GOING_UP)
		return;

	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	// A pressed, stay-pushed, non-toggle button ignores further presses.
	if (m_toggle_state == BS_PRESSED && m_fStayPushed != 0 && (flags & SF_BUTTON_TOGGLE) == 0)
		return;

	m_hActivator = *GlobalsInt(m_pGlobals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX);

	// At home (or a plain button) -> press; otherwise (pressed toggle /
	// stay-pushed) fire targets and return.
	if ((m_fStayPushed == 0 && (flags & SF_BUTTON_TOGGLE) == 0) || m_toggle_state != BS_PRESSED)
	{
		ButtonActivate();
	}
	else
	{
		PlayPressSound();
		SUB_UseTargets(NULL, 0, 0.0f);
		ButtonReturn(NULL);
	}
}

//=========================================================
// CBaseButton::ButtonTouch
//
// Touch handler (SF_BUTTON_TOUCH_ACTIVATE).  Only a touching player
// presses the button, and only when its master (multisource) allows.
//=========================================================
void CBaseButton::ButtonTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	if (!globals)
		return;

	int otherIndex = *GlobalsInt(globals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX);
	edict_t* pOtherEdict = otherIndex ? EnginePEntityOfEntIndex(otherIndex) : NULL;
	entvars_t* pevOther = pOtherEdict ? EngineGetVarsOfEnt(pOtherEdict) : NULL;

	if (!pevOther)
		return;

	if (strcmp(EngineStringFromIndex(PevInt(pevOther, PEV_CLASSNAME)), "player") != 0)
		return;

	if (m_toggle_state == BS_GOING_DOWN || m_toggle_state == BS_GOING_UP)
		return;

	int flags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	// A pressed, non-stay-pushed, non-toggle button ignores further touches.
	if (m_toggle_state == BS_PRESSED && m_fStayPushed == 0 && (flags & SF_BUTTON_TOGGLE) == 0)
		return;

	// Master check: a multisource named by m_sMaster must be fully
	// triggered (its unsatisfied count at 0) before a touch counts.
	if (m_sMaster != 0)
	{
		edict_t* pMaster = EngineFindEntityByString(NULL, "targetname", EngineStringFromIndex(m_sMaster));
		if (pMaster && EngineIndexOfEdict(pMaster) != 0)
		{
			entvars_t* pevMaster = EngineGetVarsOfEnt(pMaster);
			if (strcmp(EngineStringFromIndex(PevInt(pevMaster, PEV_CLASSNAME)), "multisource") == 0)
			{
				CMultiSource* pSource = ResolveMultiSource(pevMaster);
				if (pSource && pSource->m_iCount != 0)
					return;	// not all triggers set yet -> blocked
			}
		}
	}

	// Begin acting.
	SetTouch((EntityFunc)NULL);
	m_hActivator = *GlobalsInt(globals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX);

	if ((m_fStayPushed != 0 || (flags & SF_BUTTON_TOGGLE) != 0) && m_toggle_state == BS_PRESSED)
	{
		PlayPressSound();
		SUB_UseTargets(NULL, 0, 0.0f);
		ButtonReturn(NULL);
	}
	else
	{
		ButtonActivate();
	}
}

//=========================================================
// CBaseButton::TakeDamage
//
// "Shoot to press" path.  Re-arm health, stash the activator, then run
// the same press/return logic as ButtonUse.
//=========================================================
int CBaseButton::TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage)
{
	HL_UNUSED(inflictor);
	HL_UNUSED(attacker);
	HL_UNUSED(damage);

	PevFloat(pev, PEV_HEALTH) = BUTTON_SHOT_HEALTH;

	// Ignore the hit while moving, or when there is nothing to do
	// (pressed but not stay-pushed).
	if (m_toggle_state == BS_GOING_DOWN || m_toggle_state == BS_GOING_UP)
		return 0;

	if (m_toggle_state == BS_PRESSED && m_fStayPushed == 0)
		return 0;

	SetTouch((EntityFunc)NULL);
	m_hActivator = *GlobalsInt(m_pGlobals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX);

	// At home (or a plain button) -> press; a pressed stay-pushed button
	// fires targets and returns.
	if (m_fStayPushed == 0 || m_toggle_state != BS_PRESSED)
	{
		ButtonActivate();
	}
	else
	{
		PlayPressSound();
		SUB_UseTargets(NULL, 0, 0.0f);
		ButtonReturn(NULL);
	}

	return 0;
}

//=========================================================
// CBaseButton::Spawn - func_button
//
// Precache the press sound (and spark sounds when SF_BUTTON_SPARK),
// derive movedir, make the brush a MOVETYPE_PUSH / SOLID_BSP mover,
// default speed/lip, compute the pressed position, then install the
// touch or use handler.
//=========================================================
void CBaseButton::Spawn()
{
	int sound = (int)PevFloat(pev, PEV_SOUNDS);
	const char* pszSound = ButtonSound(sound);
	EnginePrecacheSound(pszSound);
	PevInt(pev, PEV_NOISE) = EngineAllocString(pszSound);

	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & SF_BUTTON_SPARK) != 0)
	{
		EnginePrecacheSound(kSparkSounds[0]);
		EnginePrecacheSound(kSparkSounds[1]);
		EnginePrecacheSound(kSparkSounds[2]);
		EnginePrecacheSound(kSparkSounds[3]);
		EnginePrecacheSound(kSparkSounds[4]);
		EnginePrecacheSound(kSparkSounds[5]);

		SetThink(&CBaseButton::ButtonSpark);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.5f;
	}

	SetMovedir(pev);

	PevFloat(pev, PEV_MOVETYPE) = BUTTON_MOVETYPE_PUSH;
	PevFloat(pev, PEV_SOLID) = BUTTON_SOLID_BSP;

	edict_t* edict = EdictFromEntvars(pev);
	EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	if ((PevInt(pev, PEV_SPEED) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_SPEED) = BUTTON_DEFAULT_SPEED;

	if (PevInt(pev, PEV_HEALTH) > 0)
		PevFloat(pev, PEV_TAKEDAMAGE) = 1.0f;	// DAMAGE_YES

	if (((*(int*)&m_flWait) & 0x7FFFFFFF) == 0)
		m_flWait = 1.0f;

	if (((*(int*)&m_flLip) & 0x7FFFFFFF) == 0)
		m_flLip = 4.0f;

	m_toggle_state = BS_HOME;

	// position1 =origin; position2 = origin pushed in along movedir by
	// (|movedir . size| - lip).
	Vector& origin = PevVector(pev, PEV_ORIGIN);
	Vector& size = PevVector(pev, PEV_SIZE);
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);

	VecCopy(VecPtr(m_vecPosition1), VecPtr(origin));

	float travel = (float)fabs(movedir.x * size.x + movedir.y * size.y + movedir.z * size.z) - m_flLip;

	m_vecPosition2.x = movedir.x * travel + m_vecPosition1.x;
	m_vecPosition2.y = movedir.y * travel + m_vecPosition1.y;
	m_vecPosition2.z = movedir.z * travel + m_vecPosition1.z;

	m_fRotating = 0;
	m_fStayPushed = (m_flWait == -1.0f) ? 1 : 0;

	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & SF_BUTTON_TOUCH_ACTIVATE) != 0)
	{
		SetTouch(&CBaseButton::ButtonTouch);
	}
	else
	{
		SetDoNothingTouch();	// SUB_DoNothing
		SetUse(&CBaseButton::ButtonUse);
	}
}

//=========================================================
// CRotButton::Spawn - func_rot_button
//
// As CBaseButton::Spawn, but uses AxisDir for the rotation axis and
// AngularMove, computing the pressed angle from pev->angles.
//=========================================================
void CRotButton::Spawn()
{
	int sound = (int)PevFloat(pev, PEV_SOUNDS);
	const char* pszSound = ButtonSound(sound);
	EnginePrecacheSound(pszSound);
	PevInt(pev, PEV_NOISE) = EngineAllocString(pszSound);

	AxisDir(pev);

	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & SF_ROTBUTTON_BACKWARDS) != 0)
	{
		Vector& movedir = PevVector(pev, PEV_MOVEDIR);
		movedir.x = -movedir.x;
		movedir.y = -movedir.y;
		movedir.z = -movedir.z;
	}

	PevFloat(pev, PEV_MOVETYPE) = BUTTON_MOVETYPE_PUSH;
	PevFloat(pev, PEV_SOLID) = BUTTON_SOLID_BSP;

	edict_t* edict = EdictFromEntvars(pev);
	EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	if ((PevInt(pev, PEV_SPEED) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_SPEED) = BUTTON_DEFAULT_SPEED;

	if (((*(int*)&m_flWait) & 0x7FFFFFFF) == 0)
		m_flWait = 1.0f;

	m_toggle_state = BS_HOME;

	// position1 =angles; position2 = angles + movedir * distance.
	Vector& angles = PevVector(pev, PEV_ANGLES);
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);

	VecCopy(VecPtr(m_vecPosition1), VecPtr(angles));

	m_vecPosition2.x = movedir.x * m_flMoveDistance + angles.x;
	m_vecPosition2.y = movedir.y * m_flMoveDistance + angles.y;
	m_vecPosition2.z = movedir.z * m_flMoveDistance + angles.z;

	m_fRotating = 1;
	m_fStayPushed = (m_flWait == -1.0f) ? 1 : 0;

	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & SF_BUTTON_TOUCH_ACTIVATE) != 0)
	{
		SetTouch(&CBaseButton::ButtonTouch);
	}
	else
	{
		SetDoNothingTouch();	// SUB_DoNothing
		SetUse(&CBaseButton::ButtonUse);
	}
}

//=========================================================
// CMomentaryRotButton (momentary_rot_button)
//
// CBaseEntity (28 bytes) plus its own field set (size 68):
//   [28]  m_lastUsed       (drive direction latch)
//   [32]  m_direction      (+1 / -1)
//   [36]  m_flDist         (rotation span in degrees, KeyValue "distance")
//   [40]  m_returnSpeed    (KeyValue "returnspeed")
//   [44]  m_start          (Vector)
//   [56]  m_end            (Vector)
//=========================================================
class CMomentaryRotButton : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pOther);

	void UpdateThink(CBaseEntity* pOther);
	void Off(CBaseEntity* pOther);

	void PlayPressSound();
	void UpdateTarget(float fraction);

	int m_lastUsed;			// [28]
	int m_direction;		// [32]
	float m_flDist;			// [36]
	float m_returnSpeed;	// [40]
	Vector m_start;			// [44]
	Vector m_end;			// [56]
};

HL_COMPILE_TIME_ASSERT(sizeof(CMomentaryRotButton) <= 68, CMomentaryRotButton_size);

//=========================================================
// CMomentaryRotButton::PlayPressSound
//
// Plays the press sound on the leading edge of a drive.
//=========================================================
void CMomentaryRotButton::PlayPressSound()
{
	if (m_lastUsed == 0)
	{
		edict_t* edict = EdictFromEntvars(pev);
		EngineEmitSound(edict, 2, EngineStringFromIndex(PevInt(pev, PEV_NOISE)), BUTTON_SND_VOLUME, BUTTON_SND_ATTN);
	}
}

//=========================================================
// CMomentaryRotButton::KeyValue
//
// Consumes the door/button keys but keeps only "distance" and
// "returnspeed".
//=========================================================
void CMomentaryRotButton::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "lip") == 0)
	{
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "wait") == 0)
	{
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "delay") == 0)
	{
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "distance") == 0)
	{
		m_flDist = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "master") == 0)
	{
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "returnspeed") == 0)
	{
		m_returnSpeed = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CMomentaryRotButton::Spawn
//
// Make a MOVETYPE_PUSH / SOLID_BSP rotating brush whose angle is driven
// directly by Use().  start/end are the two extremes of the span; the
// brush is placed at start.
//=========================================================
void CMomentaryRotButton::Spawn()
{
	AxisDir(pev);

	if ((PevInt(pev, PEV_SPEED) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_SPEED) = MOMENTARY_DEFAULT_SPEED;

	Vector& angles = PevVector(pev, PEV_ANGLES);
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);

	if (m_flDist < 0.0f)
	{
		m_start.x = movedir.x * m_flDist + angles.x;
		m_start.y = movedir.y * m_flDist + angles.y;
		m_start.z = movedir.z * m_flDist + angles.z;

		VecCopy(VecPtr(m_end), VecPtr(angles));
		m_flDist = -m_flDist;
		m_direction = 1;
	}
	else
	{
		VecCopy(VecPtr(m_start), VecPtr(angles));

		m_end.x = movedir.x * m_flDist + angles.x;
		m_end.y = movedir.y * m_flDist + angles.y;
		m_end.z = movedir.z * m_flDist + angles.z;
		m_direction = -1;
	}

	PevFloat(pev, PEV_SOLID) = BUTTON_SOLID_BSP;
	PevFloat(pev, PEV_MOVETYPE) = BUTTON_MOVETYPE_PUSH;

	edict_t* edict = EdictFromEntvars(pev);
	EngineSetOrigin(edict, VecPtr(PevVector(pev, PEV_ORIGIN)));
	EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	int sound = (int)PevFloat(pev, PEV_SOUNDS);
	const char* pszSound = ButtonSound(sound);
	EnginePrecacheSound(pszSound);
	PevInt(pev, PEV_NOISE) = EngineAllocString(pszSound);

	m_lastUsed = 0;
}

//=========================================================
// CMomentaryRotButton::UpdateTarget
//
// Drive every entity named by pev->target, threading the dial fraction
// through their Use slot so they follow this dial.
//=========================================================
void CMomentaryRotButton::UpdateTarget(float fraction)
{
	if (PevInt(pev, PEV_TARGET) == 0)
		return;

	void* globals = m_pGlobals;
	int oldSelf = *GlobalsInt(globals, BUTTON_GLOBALS_SELF_ENTINDEX);
	int oldActivator = *GlobalsInt(globals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX);

	const char* pszTarget = EngineStringFromIndex(PevInt(pev, PEV_TARGET));
	edict_t* pTarget = NULL;

	while ((pTarget = EngineFindEntityByString(pTarget, "targetname", pszTarget)) != NULL)
	{
		if (EngineIndexOfEdict(pTarget) == 0)
			break;

		*GlobalsInt(globals, BUTTON_GLOBALS_SELF_ENTINDEX) = EngineIndexOfEdict(pTarget);
		*GlobalsInt(globals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX) = oldSelf;

		CBaseEntity* pEntity = FindOrMakeEntity(pTarget);
		if (pEntity)
			pEntity->Use((CBaseEntity*)&fraction);

		*GlobalsInt(globals, BUTTON_GLOBALS_SELF_ENTINDEX) = oldSelf;
		*GlobalsInt(globals, BUTTON_GLOBALS_ACTIVATOR_ENTINDEX) = oldActivator;
	}
}

//=========================================================
// CMomentaryRotButton::Use
//
// Each press nudges the dial one step toward its end, spinning the
// brush.  When the dial reaches an extreme it snaps and stops; while
// in motion it spins toward the goal and re-thinks.
//=========================================================
void CMomentaryRotButton::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	if (m_lastUsed == 0)
		m_direction = -m_direction;

	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);

	Vector& angles = PevVector(pev, PEV_ANGLES);
	float delta = AxisDelta((int)PevFloat(pev, PEV_SPAWNFLAGS), VecPtr(angles), VecPtr(m_start));
	float fraction = delta / m_flDist;

	if (m_direction > 0 && fraction >= 1.0f)
	{
		// At the far extreme: snap to end, stop.
		VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), 0.0f, 0.0f, 0.0f);
		VecCopy(VecPtr(angles), VecPtr(m_end));
		m_lastUsed = 1;
		return;
	}

	if (m_direction < 0 && fraction <= 0.0f)
	{
		// At the near extreme: snap to start, stop.
		VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), 0.0f, 0.0f, 0.0f);
		VecCopy(VecPtr(angles), VecPtr(m_start));
		m_lastUsed = 1;
		return;
	}

	PlayPressSound();
	m_lastUsed = 1;
	UpdateTarget(fraction);

	// Spin toward the goal at pev->speed.
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	float speed = (float)m_direction * PevFloat(pev, PEV_SPEED);
	VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), movedir.x * speed, movedir.y * speed, movedir.z * speed);

	SetThink(&CMomentaryRotButton::UpdateThink);
}

//=========================================================
// CMomentaryRotButton::UpdateThink
//
// Stop spinning once the drive settles.  If the brush still has the
// auto-return flag and was driven forward, hand off to Off to spin
// back; otherwise just stop thinking.
//=========================================================
void CMomentaryRotButton::UpdateThink(CBaseEntity* pOther)
{
	VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), 0.0f, 0.0f, 0.0f);
	m_lastUsed = 0;

	// Auto-return: spawnflag 0x10 plus a positive return speed sends the
	// dial back toward start through Off.
	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & 0x10) != 0 && m_returnSpeed > 0.0f)
	{
		SetThink(&CMomentaryRotButton::Off);
		PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);
		m_direction = -1;
	}
	else
	{
		SetThink((ThinkFunc)NULL);
	}
}

//=========================================================
// CMomentaryRotButton::Off
//
// Settle the dial back toward start: when it reaches start, snap and
// stop; otherwise keep updating targets and spinning back.
//=========================================================
void CMomentaryRotButton::Off(CBaseEntity* pOther)
{
	Vector& angles = PevVector(pev, PEV_ANGLES);
	float delta = AxisDelta((int)PevFloat(pev, PEV_SPAWNFLAGS), VecPtr(angles), VecPtr(m_start));

	if (delta <= 0.0f)
	{
		VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), 0.0f, 0.0f, 0.0f);
		VecCopy(VecPtr(angles), VecPtr(m_start));
		PevFloat(pev, PEV_NEXTTHINK) = -1.0f;
		SetThink((ThinkFunc)NULL);
	}
	else
	{
		float fraction = delta / m_flDist;
		UpdateTarget(fraction);

		Vector& movedir = PevVector(pev, PEV_MOVEDIR);
		float speed = m_returnSpeed;
		VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), -(movedir.x * speed), -(movedir.y * speed), -(movedir.z * speed));

		PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);
	}
}

//=========================================================
// func_button
//=========================================================
DLLEXPORT void func_button(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 348);
		if (!privateData)
			return;

		memset(privateData, 0, 348);

		CBaseButton* self = new (privateData) CBaseButton();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_rot_button
//=========================================================
DLLEXPORT void func_rot_button(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 348);
		if (!privateData)
			return;

		memset(privateData, 0, 348);

		CRotButton* self = new (privateData) CRotButton();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// momentary_rot_button
//=========================================================
DLLEXPORT void momentary_rot_button(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 68);
		if (!privateData)
			return;

		memset(privateData, 0, 68);

		CMomentaryRotButton* self = new (privateData) CMomentaryRotButton();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// multisource
//=========================================================
DLLEXPORT void multisource(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 36);
		if (!privateData)
			return;

		memset(privateData, 0, 36);

		CMultiSource* self = new (privateData) CMultiSource();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
