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
// Triggers - brush trigger entities (trigger_* family),
// multi_manager, func_friction and func_ladder.
//
// Every class here derives from the shared brush-entity
// foundation in cbase_toggle.h:
//   CBaseTrigger : CBaseToggle : CBaseDelay : CBaseEntity
//
// InitTrigger, SetMovedir, SUB_UseTargets, LinearMove and the
// KeyValue chain are reused from the foundation - they are not
// reimplemented here.
//=========================================================

// ChangeLevelTouch copies the destination map / landmark names with strcpy, exactly
// as the original the binary does (into fixed-size buffers). Silence the CRT C4996
// deprecation nag rather than diverge from the byte-faithful copy.
#define _CRT_SECURE_NO_WARNINGS
#include <new>
#include <string.h>
#include <stdlib.h>
#include "cbase_toggle.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// pev byte offsets used by the trigger family that are not
// already named in utils.h.
//   116 - globals 'other' ent index (toucher), threaded by the
//         engine before a Touch dispatch (== GLOBALS_OTHER_ENTINDEX)
//   496 - pev->speed for trigger_push (shares the door dmgtime slot)
//=========================================================
enum
{
	PEV_PUSH_SPEED		= 496,	// trigger_push glide speed
	PEV_NOISE			= 480,	// pev->noise (trigger arrival sound; distinct from
								// pev->noiseMoving at 484 used by the door family)
};

enum
{
	GLOBALS_OTHER_ENT	= 116,	// index of the entity that touched us
	GLOBALS_FORWARD		= 240,	// gpGlobals->v_forward (from MakeVectors)
};

//=========================================================
// Spawnflag bits.
//=========================================================
#define SF_TRIGGER_ALLOWMONSTERS	1	// monsters allowed to fire this trigger
#define SF_TRIGGER_PUSH_ONCE		1	// trigger_push removes itself after one push
#define SF_TRIGGER_PUSH_START_OFF	2	// trigger_push starts disabled

//=========================================================
// SOLID_* / MOVETYPE_* values stored in pev as floats.
//=========================================================
#define SOLID_NOT			0.0f
#define SOLID_TRIGGER		1.0f
#define SOLID_BSP			4.0f
#define MOVETYPE_PUSH		7.0f

//=========================================================
// Vtable Use slot offset (binary CBaseEntity layout): the
// 8th dword (Spawn,KeyValue,?,Save,Restore,Think,Touch,Use).
//=========================================================
#define VTABLE_USE_OFFSET	28

//=========================================================
// Strings referenced by this module.
//=========================================================
static const char kNullSound[]		= "common/null.wav";
static const char kPlayer[]			= "player";
static const char kProbeDroid[]		= "monster_probedroid";
static const char kTriggerSecret[]	= "trigger_secret";
static const char kInfoLandmark[]	= "info_landmark";
static const char kClassChangelevel[]	= "trigger_changelevel";

//=========================================================
// Default tunables (exact bit patterns from the decompiles).
//   0.2  = 0x3E4CCCCD  - trigger_multiple default re-trigger wait
//  -1.0  = 0xBF800000  - one-shot wait (fires once)
//=========================================================
#define MULTI_DEFAULT_WAIT		0.2f
#define ONESHOT_WAIT			(-1.0f)

//=========================================================
// Global activator slot used by target firing.  The binary stashes the firing
// player's ent index here between multi-trigger firing, CounterUse calls, and
// rotating-door side tests.
//=========================================================
int g_CounterActivatorIndex = 0;

// Destination-map name buffer handed to the engine ChangeLevel (the binary
// writes it during ChangeLevelTouch and passes it as arg1 to ChangeLevel).
static char g_szChangeMapName[64];

// Resolve the toucher (gpGlobals->other) into its CBaseEntity*,
// or NULL.  Mirrors the PEntityOfEntIndex / GetVarsOfEnt pair the
// touch handlers run before checking the toucher's classname.
//=========================================================
static entvars_t* OtherVars(void* globals)
{
	if (!globals)
		return NULL;

	int otherIndex = *GlobalsInt(globals, GLOBALS_OTHER_ENT);
	edict_t* pEdict = EnginePEntityOfEntIndex(otherIndex);
	return pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
}

//=========================================================
// True if the named entity's pev->classname matches sz.
//=========================================================
static int ClassNameIs(entvars_t* pev, const char* sz)
{
	if (!pev)
		return 0;

	const char* name = EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME));
	return name && strcmp(name, sz) == 0;
}

//=========================================================
// CBaseTrigger::KeyValue
//
// The trigger family adds "wait","damage","count" on top of the
// CBaseDelay "delay" key.  "damage" is stored as pev->dmg.
// "delay" is truncated to an int before being stored as a float
// (matching the binary exactly).
//=========================================================
static void TriggerKeyValue(CBaseTrigger* self, entvars_t* pev, float& waitField, int& countField, KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "wait") == 0)
	{
		waitField = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}

	if (strcmp(pkvd->szKeyName, "damage") == 0)
	{
		PevFloat(pev, PEV_DMG) = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "count") == 0)
	{
		countField = (int)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "delay") == 0)
	{
		// delay is intentionally truncated to a whole second here.
		float& delayField = *(float*)((unsigned char*)self + 28);
		delayField = (float)(int)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CBaseTrigger (the bare "trigger" export) - the binary wraps
// CBaseToggle with the trigger KeyValue handler and no Spawn.
//=========================================================
class CTrigger : public CBaseTrigger
{
public:
	void KeyValue(KeyValueData* pkvd);
};

void CTrigger::KeyValue(KeyValueData* pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// CTriggerMultiple - trigger_multiple / trigger_once
//
// Spawn precaches the null sound, defaults wait to
// 0.2s, runs InitTrigger and (unless the allow-monsters flag is
// set) parks MultiTouch. trigger_once is the same
// entity with wait forced to -1 so it fires a single time.
//=========================================================
class CTriggerMultiple : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void MultiTouch(CBaseEntity* pOther);
	void ActivateMultiTrigger();
	void MultiWaitOver(CBaseEntity* pOther);
	void MultiClear(CBaseEntity* pOther);
};

void CTriggerMultiple::KeyValue(KeyValueData* pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// CTriggerMultiple::ActivateMultiTrigger
//
// Fire the trigger's targets, optionally announcing a found
// secret, then either disable until the wait expires (MultiWaitOver)
// or remove ourselves (one-shot path).
//=========================================================
void CTriggerMultiple::ActivateMultiTrigger()
{
	void* globals = m_pGlobals;

	// only fire once we have passed the re-trigger time stored in
	// pev->nextthink
	if (GlobalsTime(globals) < PevFloat(pev, PEV_NEXTTHINK))
		return;

	// secret-found bookkeeping
	if (ClassNameIs(pev, kTriggerSecret))
	{
		int enemyIndex = PevInt(pev, PEV_ENEMY);
		edict_t* pEdict = EnginePEntityOfEntIndex(enemyIndex);
		entvars_t* pevEnemy = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
		if (!ClassNameIs(pevEnemy, kPlayer))
			return;

		// gpGlobals->found_secrets++ (globals + 168)
		float& foundSecrets = *(float*)((unsigned char*)globals + 168);
		foundSecrets = foundSecrets + 1.0f;

		// MESSAGE_BEGIN-style note that a secret was found
		EngineWriteByte(2, 28);
	}

	// play the noise/arrival sound if one was bound
	int noise = PevInt(pev, PEV_NOISE);
	if (noise)
	{
		edict_t* edict = EdictFromEntvars(pev);
		const char* sample = EngineStringFromIndex(noise);
		EngineEmitSound(edict, 2, sample, 1.0f, 0.8f);
	}

	// remember the activator for SUB_UseTargets to thread through
	g_CounterActivatorIndex = PevInt(pev, PEV_ENEMY);

	SUB_UseTargets(NULL, 0, 0.0f);

	if (m_flWait <= 0.0f)
	{
		// one-shot: hand off to RemoveEntity on the next think
		SetTouch((EntityFunc)NULL);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;
		SetRemoveThink();
	}
	else
	{
		// re-arm after m_flWait seconds
		SetThink(&CTriggerMultiple::MultiWaitOver);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + m_flWait;
	}
}

//=========================================================
// CTriggerMultiple::MultiWaitOver
//
// Clear the think callback so the trigger can fire again.
//=========================================================
void CTriggerMultiple::MultiWaitOver(CBaseEntity* pOther)
{
	SetThink((ThinkFunc)NULL);
}

//=========================================================
// CTriggerMultiple::MultiClear
//
// One-shot tear-down: remove the trigger entity.
//=========================================================
void CTriggerMultiple::MultiClear(CBaseEntity* pOther)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// CTriggerMultiple::MultiTouch
//
// Only players (and probe droids) trip the trigger.  If the
// trigger has a movedir the toucher must be facing roughly the
// same way (DotProduct(movedir, toucher v_forward) >= 0) before
// it fires.
//=========================================================
void CTriggerMultiple::MultiTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	entvars_t* pevOther = OtherVars(m_pGlobals);
	if (!ClassNameIs(pevOther, kPlayer))
		return;

	if (!ClassNameIs(pevOther, kPlayer) && !ClassNameIs(pevOther, kProbeDroid))
		return;

	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	int hasDir = 0;
	if (movedir.x != 0.0f || movedir.y != 0.0f || movedir.z != 0.0f)
		hasDir = 1;

	int fire = 0;
	if (!hasDir)
	{
		fire = 1;
	}
	else
	{
		// MakeVectors(toucher->angles) -> gpGlobals->v_forward
		Vector& otherAngles = PevVector(pevOther, PEV_ANGLES);
		EngineMakeVectors(VecPtr(otherAngles));

		const float* forward = GlobalsForward(m_pGlobals);
		float dot = movedir.x * forward[0] + movedir.y * forward[1] + movedir.z * forward[2];
		if (dot >= 0.0f)
			fire = 1;
	}

	if (fire)
	{
		// record the toucher as the trigger's enemy/activator
		PevInt(pev, PEV_ENEMY) = *GlobalsInt(m_pGlobals, GLOBALS_OTHER_ENT);
		ActivateMultiTrigger();
	}
}

//=========================================================
// CTriggerMultiple::Spawn
//=========================================================
void CTriggerMultiple::Spawn()
{
	EnginePrecacheSound(kNullSound);
	PevInt(pev, PEV_NOISE) = EngineAllocString(kNullSound);

	if (((*(int*)&m_flWait) & 0x7FFFFFFF) == 0)
		m_flWait = MULTI_DEFAULT_WAIT;

	InitTrigger();

	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & SF_TRIGGER_ALLOWMONSTERS) == 0)
		SetTouch(&CTriggerMultiple::MultiTouch);
}

//=========================================================
// trigger_once shares CTriggerMultiple but forces wait to -1.
//=========================================================
class CTriggerOnce : public CTriggerMultiple
{
public:
	void Spawn();
};

void CTriggerOnce::Spawn()
{
	m_flWait = ONESHOT_WAIT;
	CTriggerMultiple::Spawn();
}

//=========================================================
// CTriggerHurt - trigger_hurt (Spawn @)
//
// A trigger that damages whatever touches it.  When it has a
// targetname it can be toggled on/off through ToggleUse; otherwise
// its Use slot is empty (the binary installs an empty stub).
//=========================================================
class CTriggerHurt : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void HurtTouch(CBaseEntity* pOther);
	void ToggleUse(CBaseEntity* pOther);
};

void CTriggerHurt::KeyValue(KeyValueData* pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// CTriggerHurt::HurtTouch
//
// Damage the toucher (if it can take damage) at most twice a
// second.  The re-arm time lives in the object's offset-28 slot,
// the same slot the "delay" key writes.
//=========================================================
void CTriggerHurt::HurtTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	entvars_t* pevOther = OtherVars(m_pGlobals);
	if (!pevOther)
		return;

	// only damage things that take damage
	if (((*(int*)&PevFloat(pevOther, PEV_TAKEDAMAGE)) & 0x7FFFFFFF) == 0)
		return;

	// throttle: re-arm time stored at object offset 28
	float& flNextDmgTime = *(float*)((unsigned char*)this + 28);
	if (flNextDmgTime > GlobalsTime(m_pGlobals))
		return;

	edict_t* pOtherEdict = EdictFromEntvars(pevOther);
	void* priv = pOtherEdict ? EngineGetPrivateData(pOtherEdict) : NULL;
	if (priv)
	{
		CBaseEntity* pEntity = (CBaseEntity*)priv;
		pEntity->TakeDamage(pev, pev, PevFloat(pev, PEV_DMG));
	}

	// advance the re-arm time by half a second (the binary adds to the
	// existing value, it does not reset it from the current game time).
	flNextDmgTime = flNextDmgTime + 0.5f;
}

//=========================================================
// CTriggerHurt::ToggleUse
//
// Flip the brush between SOLID_TRIGGER and SOLID_NOT so a named
// trigger_hurt can be switched on and off.
//=========================================================
void CTriggerHurt::ToggleUse(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	float& solid = PevFloat(pev, PEV_SOLID);
	if (((*(int*)&solid) & 0x7FFFFFFF) == 0)
		solid = SOLID_TRIGGER;
	else
		solid = SOLID_NOT;
}

//=========================================================
// CTriggerHurt::Spawn
//
// A named trigger_hurt gets ToggleUse so it can be switched on
// and off; an un-named one gets an empty Use, reached here by
// installing a NULL use callback.
//=========================================================
void CTriggerHurt::Spawn()
{
	InitTrigger();

	SetTouch(&CTriggerHurt::HurtTouch);

	if (PevInt(pev, PEV_TARGETNAME) != 0)
		SetUse(&CTriggerHurt::ToggleUse);
	else
		SetDoNothingUse();
}

//=========================================================
// CTriggerMonsterJump - trigger_monsterjump
//
// A trigger zone that launches monsters into a fixed jump.  The
// touch handler does the launch; the use handler toggles the
// brush solid (so a named jump pad can be switched on/off).
//=========================================================
class CTriggerMonsterJump : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void JumpTouch(CBaseEntity* pOther);
	void ToggleUse(CBaseEntity* pOther);
};

void CTriggerMonsterJump::KeyValue(KeyValueData* pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// CTriggerMonsterJump::JumpTouch
//
// If the toucher is on the ground, launch it: bump its jump
// count, clear the onground flag, and set velocity to
// movedir*speed with the configured upward height added in.
//=========================================================
void CTriggerMonsterJump::JumpTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	entvars_t* pevOther = OtherVars(m_pGlobals);
	if (!pevOther)
		return;

	// FL_ONGROUND (bit 0x20) on pev->flags (offset 380 = float index 95)
	float& flags = PevFloat(pevOther, PEV_FLAGS);
	if (((int)flags & 0x20) == 0)
		return;

	// bump the toucher's "jumped" counter (other + 48 = float index 12)
	float& jumpCount = *(float*)((unsigned char*)pevOther + 48);
	jumpCount = jumpCount + 1.0f;

	// clear FL_ONGROUND (bit 0x200) if it was set
	if (((int)flags & 0x200) != 0)
		flags = flags - 512.0f;

	float speed = PevFloat(pev, PEV_PUSH_SPEED);	// monsterjump launch speed
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	Vector& otherVel = PevVector(pevOther, PEV_VELOCITY);
	otherVel.x = movedir.x * speed;
	otherVel.y = movedir.y * speed;
	otherVel.z = movedir.z * speed + m_flHeight;

	PevFloat(pev, PEV_SOLID) = SOLID_NOT;
}

//=========================================================
// CTriggerMonsterJump::ToggleUse
//
// Flip the brush between SOLID_TRIGGER and SOLID_NOT.
//=========================================================
void CTriggerMonsterJump::ToggleUse(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	float& solid = PevFloat(pev, PEV_SOLID);
	if (((*(int*)&solid) & 0x7FFFFFFF) == 0)
		solid = SOLID_TRIGGER;
	else
		solid = SOLID_NOT;
}

//=========================================================
// CTriggerMonsterJump::Spawn
//
// Derive movedir, set the default launch speed (200) and height
// (150), and disable the trigger if it starts named/off.
//=========================================================
void CTriggerMonsterJump::Spawn()
{
	SetMovedir(pev);
	InitTrigger();

	SetUse(&CTriggerMonsterJump::ToggleUse);
	SetTouch(&CTriggerMonsterJump::JumpTouch);

	PevFloat(pev, PEV_PUSH_SPEED) = 200.0f;	// launch speed
	m_flHeight = 150.0f;

	if (PevInt(pev, PEV_TARGETNAME) != 0)
		PevFloat(pev, PEV_SOLID) = SOLID_NOT;
}

//=========================================================
// CTriggerCDAudio - trigger_cdaudio
//
// A touch zone that drives the CD player; the track is taken from
// pev->health.
//=========================================================
class CTriggerCDAudio : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void CDAudioTouch(CBaseEntity* pOther);
	void CDAudioNullTouch(CBaseEntity* pOther);
	void CDAudioRemove(CBaseEntity* pOther);
};

void CTriggerCDAudio::KeyValue(KeyValueData* pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// CDAudio track command table (track -1 stops the CD).
//=========================================================
static const char* const kCDPlayCommands[] =
{
	"cd play 1\n",		// track 1
	"cd play 2\n",
	"cd play 3\n",
	"cd play 4\n",
	"cd play 5\n",
	"cd play 6\n",
	"cd play 7\n",
	"cd play 8\n",
	"cd play 9\n",
	"cd play 10\n",
	"cd play 11\n",
	"cd play 12\n",		// track 12
};

//=========================================================
// CTriggerCDAudio::CDAudioNullTouch
//
// Empty touch; installed after the command is issued so the
// entity stops responding to further touches.
//=========================================================
void CTriggerCDAudio::CDAudioNullTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);
}

//=========================================================
// CTriggerCDAudio::CDAudioRemove
//
// Removes the entity on the scheduled think.
//=========================================================
void CTriggerCDAudio::CDAudioRemove(CBaseEntity* pOther)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// CTriggerCDAudio::CDAudioTouch
//
// Only players trip it.  Issue the CD command matching pev->health,
// then go quiet and schedule a one-shot remove.
//=========================================================
void CTriggerCDAudio::CDAudioTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	entvars_t* pevOther = OtherVars(m_pGlobals);
	if (!ClassNameIs(pevOther, kPlayer))
		return;

	edict_t* pPlayer = EdictFromEntvars(pevOther);
	int track = (int)PevFloat(pev, PEV_HEALTH);

	if (track == -1)
	{
		EngineClientCommand(pPlayer, "cd stop\n");
	}
	else if (track >= 1 && track <= 12)
	{
		EngineClientCommand(pPlayer, kCDPlayCommands[track - 1]);
	}
	else
	{
		EngineAlertMessage(1, "Unknown Track!\n");
	}

	// go dormant, then remove on the next think
	SetDoNothingTouch();
	SetRemoveThink();
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(m_pGlobals) + 0.1f;
}

//=========================================================
// CTriggerCDAudio::Spawn
//=========================================================
void CTriggerCDAudio::Spawn()
{
	InitTrigger();
	SetTouch(&CTriggerCDAudio::CDAudioTouch);
}

//=========================================================
// CTriggerCounter - trigger_counter
//
// Counts down player activations; fires its target once the count
// reaches zero, announcing the remaining tally each step.
//=========================================================
class CTriggerCounter : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void CounterUse(CBaseEntity* pOther);
	void ActivateMultiTrigger();
	void CounterWaitOver(CBaseEntity* pOther);
	void CounterRemoveThink(CBaseEntity* pOther);
};

void CTriggerCounter::KeyValue(KeyValueData* pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// CTriggerCounter::ActivateMultiTrigger (shared)
//
// The counter reaches its target through the same firing helper
// the multiples use.  trigger_counter has no wait, so this takes
// the one-shot path.
//=========================================================
void CTriggerCounter::ActivateMultiTrigger()
{
	void* globals = m_pGlobals;

	if (GlobalsTime(globals) < PevFloat(pev, PEV_NEXTTHINK))
		return;

	if (ClassNameIs(pev, kTriggerSecret))
	{
		int enemyIndex = PevInt(pev, PEV_ENEMY);
		edict_t* pEdict = EnginePEntityOfEntIndex(enemyIndex);
		entvars_t* pevEnemy = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
		if (!ClassNameIs(pevEnemy, kPlayer))
			return;

		float& foundSecrets = *(float*)((unsigned char*)globals + 168);
		foundSecrets = foundSecrets + 1.0f;
		EngineWriteByte(2, 28);
	}

	int noise = PevInt(pev, PEV_NOISE);
	if (noise)
	{
		edict_t* edict = EdictFromEntvars(pev);
		const char* sample = EngineStringFromIndex(noise);
		EngineEmitSound(edict, 2, sample, 1.0f, 0.8f);
	}

	g_CounterActivatorIndex = PevInt(pev, PEV_ENEMY);
	SUB_UseTargets(NULL, 0, 0.0f);

	if (m_flWait <= 0.0f)
	{
		SetTouch((EntityFunc)NULL);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;
		SetThink(&CTriggerCounter::CounterRemoveThink);
	}
	else
	{
		SetThink(&CTriggerCounter::CounterWaitOver);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + m_flWait;
	}
}

//=========================================================
// CTriggerCounter::CounterUse
//
// Decrement the remaining count and report progress to the
// player.  When the count hits zero, route the firing player's
// index back into pev->enemy and fire the target.
//=========================================================
void CTriggerCounter::CounterUse(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	int remaining = m_cTriggersLeft - 1;
	m_cTriggersLeft = remaining;
	if (remaining < 0)
		return;

	// only announce progress to a player activator on an un-flagged trigger
	edict_t* pEdict = EnginePEntityOfEntIndex(g_CounterActivatorIndex);
	entvars_t* pevActivator = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;

	int announce = 1;
	if (!ClassNameIs(pevActivator, kPlayer) || ((int)PevFloat(pev, PEV_SPAWNFLAGS) & 1) != 0)
		announce = 0;

	if (m_cTriggersLeft != 0)
	{
		if (announce)
		{
			switch (m_cTriggersLeft)
			{
			case 1:
				EngineAlertMessage(1, "Only 1 more to go...");
				break;
			case 2:
				EngineAlertMessage(1, "Only 2 more to go...");
				break;
			case 3:
				EngineAlertMessage(1, "Only 3 more to go...");
				break;
			default:
				EngineAlertMessage(1, "There are more to go...");
				break;
			}
		}
	}
	else
	{
		if (announce)
			EngineAlertMessage(1, "Sequence completed!");

		PevInt(pev, PEV_ENEMY) = g_CounterActivatorIndex;
		ActivateMultiTrigger();
	}
}

//=========================================================
// CTriggerCounter::CounterWaitOver (shared)
//
// Positive-wait counters use the same wait-over callback as
// trigger_multiple: clear the think slot when the wait expires.
//=========================================================
void CTriggerCounter::CounterWaitOver(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);
	SetThink((ThinkFunc)NULL);
}

//=========================================================
// CTriggerCounter::CounterRemoveThink - one-shot tear-down used
// by the shared firing helper.
//=========================================================
void CTriggerCounter::CounterRemoveThink(CBaseEntity* pOther)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// CTriggerCounter::Spawn
//
// Defaults wait to -1 (one-shot) and the count to 2, then parks
// CounterUse.
//=========================================================
void CTriggerCounter::Spawn()
{
	m_flWait = ONESHOT_WAIT;

	if (m_cTriggersLeft == 0)
		m_cTriggersLeft = 2;

	SetUse(&CTriggerCounter::CounterUse);
}

//=========================================================
// CTriggerChangeLevel - trigger_changelevel (Spawn @)
//
// Transitions the player to another map.  The "map" key names the
// destination; the touch handler builds the level list around the
// matching info_landmark and asks the engine to change level.
//=========================================================
class CTriggerChangeLevel : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void ChangeLevelTouch(CBaseEntity* pOther);

	// The "map" key is stored as a 64-byte string at object offset
	// 128 (where a brush's model string would otherwise sit; a
	// changelevel carries no model).  Accessed through MapName().
	char* MapName() { return (char*)((unsigned char*)this + 128); }
};

//=========================================================
// CTriggerChangeLevel::KeyValue
//
// Stores the "map" key into the map-name buffer at offset 128.
//=========================================================
void CTriggerChangeLevel::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "map") == 0)
	{
		size_t len = strlen(pkvd->szValue) + 1;
		memcpy((unsigned char*)this + 128, pkvd->szValue, len);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CTriggerChangeLevel::ChangeLevelTouch
//
// Only the player triggers the transition.  Build a one-entry
// level list keyed on the matching info_landmark, then hand off to
// the engine's ChangeLevel.  The level-list bookkeeping mirrors the binary.
//=========================================================
// LEVELLIST entry stashed at gpGlobals+176 (the engine's level-transition slot).
// The binary mallocs 120 bytes, stores the pointer at globals+176, clears the
// found flag, and -- if an info_landmark is within 255u -- records the landmark
// name and the player's landmark-relative state. Field labels are cosmetic; the
// byte offsets are taken directly from the decompile and are authoritative.
struct LEVELLIST
{
	char  pad0[48];          // 0..47
	int   foundLandmark;     // 48      ([v10+12] = 0, then 1 when found)
	char  landmarkName[20];  // 52..71  (strcpy of landmark pev+436)
	float deltaOrigin[3];    // 72/76/80   (player origin - landmark origin)
	int   velocity[3];       // 84/88/92   (player pev+64)
	int   angles[3];         // 96/100/104 (player pev+76; [104] zeroed)
	int   view[3];           // 108/112/116(player pev+352; [116] zeroed)
};

void CTriggerChangeLevel::ChangeLevelTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	// The binary: only the player triggers the transition.
	entvars_t* pevPlayer = OtherVars(m_pGlobals);
	if (!ClassNameIs(pevPlayer, kPlayer))
		return;

	// The binary: disable so we only fire once.
	SetTouch((EntityFunc)NULL);
	PevFloat(pev, PEV_SOLID) = SOLID_NOT;

	// The binary: malloc(120) level list -> gpGlobals+176; clear found flag.
	LEVELLIST* pLevelList = (LEVELLIST*)malloc(sizeof(LEVELLIST));
	*(void**)((unsigned char*)m_pGlobals + 176) = pLevelList;
	if (pLevelList)
		pLevelList->foundLandmark = 0;

	// The binary: copy the destination map name into the engine dest-map global.
	strcpy(g_szChangeMapName, MapName());

	// The binary: fire pev->target / killtarget before the transition.
	SUB_UseTargets(NULL, 0, 0.0f);

	// The binary: search for the info_landmark within 255u of the trigger's
	// bbox center (: pev+4 absmin + 0.5*pev->size) and, if found, record its
	// name plus the player's landmark-relative origin/velocity/angles into the level list.
	if (pLevelList)
	{
		const float* absmin = (const float*)((const unsigned char*)pev + 4);
		const float* size = VecPtr(PevVector(pev, PEV_SIZE));
		float center[3];
		center[0] = absmin[0] + size[0] * 0.5f;
		center[1] = absmin[1] + size[1] * 0.5f;
		center[2] = absmin[2] + size[2] * 0.5f;

		edict_t* pEnt = EngineFindEntityInSphere(center, 255.0f);
		while (pEnt)
		{
			if (EngineIndexOfEdict(pEnt) == 0)	// : IndexOfEdict==0 -> stop
				break;

			entvars_t* pevEnt = EngineGetVarsOfEnt(pEnt);
			if (!pevEnt)
				break;

			if (ClassNameIs(pevEnt, kInfoLandmark))
			{
				pLevelList->foundLandmark = 1;

				const char* name = EngineStringFromIndex(PevInt(pevEnt, PEV_TARGET));	// landmark name = pev+436
				strcpy(pLevelList->landmarkName, name ? name : "");

				const float* pOrigin = VecPtr(PevVector(pevPlayer, PEV_ORIGIN));
				const float* lOrigin = VecPtr(PevVector(pevEnt, PEV_ORIGIN));
				pLevelList->deltaOrigin[0] = pOrigin[0] - lOrigin[0];
				pLevelList->deltaOrigin[1] = pOrigin[1] - lOrigin[1];
				pLevelList->deltaOrigin[2] = pOrigin[2] - lOrigin[2];

				pLevelList->velocity[0] = PevInt(pevPlayer, 64);
				pLevelList->velocity[1] = PevInt(pevPlayer, 68);
				pLevelList->velocity[2] = PevInt(pevPlayer, 72);

				pLevelList->angles[0] = PevInt(pevPlayer, 76);
				pLevelList->angles[1] = PevInt(pevPlayer, 80);
				pLevelList->angles[2] = 0;	// : [104] zeroed

				pLevelList->view[0] = PevInt(pevPlayer, 352);
				pLevelList->view[1] = PevInt(pevPlayer, 356);
				pLevelList->view[2] = 0;	// : [116] zeroed
			}

			// advance to the next sphere result via pev->chain (pev+320).
			pEnt = EnginePEntityOfEntIndex(PevInt(pevEnt, PEV_CHAIN));
		}
	}

	// The binary: hand off to the engine ChangeLevel (table byte offset 0x14).
	// Landmark arg is "" (the binary passes, a single NUL).
	EngineChangeLevel(g_szChangeMapName, "");
}

//=========================================================
// CTriggerChangeLevel::Spawn
//
// Warns if no map was set, then runs InitTrigger and parks the
// transition touch handler.
//=========================================================
void CTriggerChangeLevel::Spawn()
{
	if (MapName()[0] == '\0')
		EngineAlertMessage(1, "a trigger_changelevel doesn't have a map");

	InitTrigger();
	SetTouch(&CTriggerChangeLevel::ChangeLevelTouch);
}

//=========================================================
// CTriggerPush - trigger_push (Spawn @)
//
// A zone that continuously pushes entities along movedir while
// they remain inside it.
//=========================================================
class CTriggerPush : public CBaseTrigger
{
public:
	void Spawn();

	// trigger_push overrides the Touch (vtable slot 6) and Use (slot 7)
	// virtuals directly - the binary installs PushTouch/PushUse into the
	// vtable, not through SetTouch/SetUse.  Its KeyValue slot is empty
	// so it deliberately ignores every brush key.
	void KeyValue(KeyValueData* pkvd);
	void Touch(CBaseEntity* pOther);
	void Use(CBaseEntity* pOther);
};

//=========================================================
// CTriggerPush::KeyValue - empty; trigger_push ignores
// all keys.
//=========================================================
void CTriggerPush::KeyValue(KeyValueData* pkvd)
{
	HL_UNUSED(pkvd);
}

//=========================================================
// CTriggerPush::Touch (vtable slot 6)
//
// Push entities that have health (movers/monsters) by setting their
// velocity to movedir*speed*10; if the once flag is set, remove the
// trigger after this push.
//=========================================================
void CTriggerPush::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	entvars_t* pevOther = OtherVars(m_pGlobals);
	if (!pevOther)
		return;

	// the binary tests the raw bit pattern of pev->health as a signed
	// int ( > 0 ), not the truncated float value.
	if (*(int*)&PevFloat(pevOther, PEV_HEALTH) > 0)
	{
		Vector& movedir = PevVector(pev, PEV_MOVEDIR);
		float speed = PevFloat(pev, PEV_PUSH_SPEED);
		Vector& otherVel = PevVector(pevOther, PEV_VELOCITY);
		otherVel.x = movedir.x * speed * 10.0f;
		otherVel.y = movedir.y * speed * 10.0f;
		otherVel.z = movedir.z * speed * 10.0f;
	}

	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & SF_TRIGGER_PUSH_ONCE) != 0)
	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineRemoveEntity(edict);
	}
}

//=========================================================
// CTriggerPush::Use (vtable slot 7)
//
// Toggle the brush solid state (SOLID_TRIGGER <-> SOLID_NOT).
//=========================================================
void CTriggerPush::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	float& solid = PevFloat(pev, PEV_SOLID);
	solid = (float)((*(int*)&solid) != 0x3F800000);
}

//=========================================================
// CTriggerPush::Spawn
//
// Default the push direction to yaw 360 (treated as "up" via
// SetMovedir), run InitTrigger, default the speed to 1000, and
// start disabled if the start-off flag (bit 1) is set.
//=========================================================
void CTriggerPush::Spawn()
{
	Vector& angles = PevVector(pev, PEV_ANGLES);
	if (angles.x == 0.0f && angles.y == 0.0f && angles.z == 0.0f)
		angles.y = 360.0f;

	InitTrigger();

	if (((*(int*)&PevFloat(pev, PEV_PUSH_SPEED)) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_PUSH_SPEED) = 1000.0f;

	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & SF_TRIGGER_PUSH_START_OFF) != 0)
		PevFloat(pev, PEV_SOLID) = SOLID_NOT;

	// Touch/Use are vtable overrides (the binary's Spawn installs no
	// SetTouch/SetUse here); KeyValue is empty.
}

//=========================================================
// CLadder - func_ladder
//
// A simple touch zone that flags the player as "on a ladder".
//=========================================================
class CLadder : public CBaseTrigger
{
public:
	void Spawn();

	// func_ladder overrides the Touch virtual (vtable slot 6) directly;
	// its KeyValue slot is empty.
	void KeyValue(KeyValueData* pkvd);
	void Touch(CBaseEntity* pOther);
};

//=========================================================
// CLadder::KeyValue - empty; func_ladder ignores all keys.
//=========================================================
void CLadder::KeyValue(KeyValueData* pkvd)
{
	HL_UNUSED(pkvd);
}

//=========================================================
// CLadder::Touch (vtable slot 6)
//
// While a player is inside, set the player's on-ladder flag
// (bit 0 of the field at offset 372 in the player's private data).
//=========================================================
void CLadder::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	entvars_t* pevOther = OtherVars(m_pGlobals);
	if (!ClassNameIs(pevOther, kPlayer))
		return;

	edict_t* pPlayer = EdictFromEntvars(pevOther);
	void* priv = pPlayer ? EngineGetPrivateData(pPlayer) : NULL;
	if (!priv)
		return;

	// set the on-ladder flag (player private data + 372, bit 0)
	int* flags = (int*)((unsigned char*)priv + 372);
	*flags |= 1;
}

//=========================================================
// CLadder::Spawn
//
// Bind the brush model but keep it solid (SOLID_BSP) so it blocks /
// is climbable, make it MOVETYPE_PUSH and invisible.
//=========================================================
void CLadder::Spawn()
{
	PevFloat(pev, PEV_SOLID) = SOLID_TRIGGER;
	PevFloat(pev, PEV_SOLID) = SOLID_BSP;

	edict_t* edict = EdictFromEntvars(pev);
	EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_PUSH;
	PevFloat(pev, PEV_RENDERMODE) = 1.0f;	// kRenderTransColor
	PevInt(pev, PEV_RENDERMODE + 4) = 0;	// renderamt = 0 (invisible)

	// Touch is a vtable override; the binary's Spawn installs no SetTouch.
}

//=========================================================
// CFriction - func_friction (Spawn @)
//
// A touch zone that scales the toucher's friction.  Unlike the
// trigger_* family it only needs Touch + KeyValue, so it derives
// straight from CBaseEntity (private data is just 32 bytes).
//=========================================================
class CFriction : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void FrictionTouch(CBaseEntity* pOther);

	// [28] friction modifier (0..1), parsed from the "modifier" key.
	float m_frictionFraction;	// [28]
};

//=========================================================
// CFriction::KeyValue
//
// "modifier" is a percentage stored as a 0..1 fraction.
//=========================================================
void CFriction::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "modifier") == 0)
	{
		m_frictionFraction = (float)(atof(pkvd->szValue) * 0.01);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CFriction::FrictionTouch
//
// Copy our friction fraction into the toucher's friction field.
//=========================================================
void CFriction::FrictionTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	entvars_t* pevOther = OtherVars(m_pGlobals);
	if (!pevOther)
		return;

	// pev->friction (offset 148 = float index 37) on the toucher
	PevFloat(pevOther, 148) = m_frictionFraction;
}

//=========================================================
// CFriction::Spawn
//
// Make the brush a SOLID_TRIGGER, bind its model, drop movetype,
// then park the friction touch handler.
//=========================================================
void CFriction::Spawn()
{
	PevFloat(pev, PEV_SOLID) = SOLID_TRIGGER;

	edict_t* edict = EdictFromEntvars(pev);
	EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	PevFloat(pev, PEV_MOVETYPE) = SOLID_NOT;	// MOVETYPE_NONE (0)
	SetTouch(&CFriction::FrictionTouch);
}

//=========================================================
// CMultiManager - multi_manager (Spawn @)
//
// Fires a fixed set of named targets, each after its own delay.
// Private-data layout (size 392):
//   [128] m_cTargets               number of targets
//   [132] m_index                  number fired in the current cycle
//   [136] m_iTargetName[16]        target name string indices
//   [200] m_startTime[16]          per-target "fired" markers
//   [264] m_flTargetDelay[16]      per-target delays (KeyValue order)
//   [328] m_flTargetFire[16]       absolute fire times (computed on Use)
//=========================================================
#define MAX_MULTI_TARGETS	16

//=========================================================
// The multi_manager target arrays overlap the CBaseToggle sound
// bytes at offset 128 (a manager never uses those), so the fields
// are reached by byte offset rather than as members.  Offsets are
// taken verbatim from the decompiles:
//   [128] count   [132] index   [136] names[16]
//   [200] fired[16]   [264] delay[16]   [328] fireTime[16]
//=========================================================
enum
{
	MM_COUNT_OFFSET		= 128,
	MM_INDEX_OFFSET		= 132,
	MM_NAMES_OFFSET		= 136,
	MM_FIRED_OFFSET		= 200,
	MM_DELAY_OFFSET		= 264,
	MM_FIRETIME_OFFSET	= 328,
};

class CMultiManager : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);

	void ManagerUse(CBaseEntity* pOther);
	void ManagerThink(CBaseEntity* pOther);

	int& Count()				{ return *(int*)((unsigned char*)this + MM_COUNT_OFFSET); }
	int& Index()				{ return *(int*)((unsigned char*)this + MM_INDEX_OFFSET); }
	int& TargetName(int i)		{ return *(int*)((unsigned char*)this + MM_NAMES_OFFSET + 4 * i); }
	int& Fired(int i)			{ return *(int*)((unsigned char*)this + MM_FIRED_OFFSET + 4 * i); }
	float& Delay(int i)			{ return *(float*)((unsigned char*)this + MM_DELAY_OFFSET + 4 * i); }
	float& FireTime(int i)		{ return *(float*)((unsigned char*)this + MM_FIRETIME_OFFSET + 4 * i); }
};

//=========================================================
// CMultiManager::KeyValue
//
// "wait" sets m_flWait; any other key is taken as "<target> <delay>"
// and appended to the target list (up to 16).
//=========================================================
void CMultiManager::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "wait") == 0)
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else
	{
		int i = Count();
		if (i < MAX_MULTI_TARGETS)
		{
			TargetName(i) = EngineAllocString(pkvd->szKeyName);
			Delay(i) = (float)atof(pkvd->szValue);
			Fired(i) = 0;
			Count() = i + 1;
			pkvd->fHandled = 1;
		}
	}
}

//=========================================================
// CMultiManager::ManagerThink
//
// Walk the target list, firing any whose scheduled time has
// arrived and that has not yet fired this cycle.  Once every
// target has fired, reset the cycle and return to waiting for Use.
//=========================================================
void CMultiManager::ManagerThink(CBaseEntity* pOther)
{
	float time = GlobalsTime(m_pGlobals);

	PevFloat(pev, PEV_NEXTTHINK) = time + 0.1f;

	int i;
	for (i = 0; i < Count(); ++i)
	{
		if (time >= FireTime(i) && Fired(i) == 0)
		{
			const char* pszName = EngineStringFromIndex(TargetName(i));
			edict_t* pTarget = EngineFindEntityByString(NULL, "targetname", pszName);

			if (pTarget && EngineIndexOfEdict(pTarget) != 0)
			{
				for (; pTarget != NULL; pTarget = EngineFindEntityByString(pTarget, "targetname", pszName))
				{
					if (EngineIndexOfEdict(pTarget) == 0)
						break;

					entvars_t* pevTarget = EngineGetVarsOfEnt(pTarget);
					if (!pevTarget)
					{
						edict_t* created = EngineCreateEntity();
						pevTarget = created ? EngineGetVarsOfEnt(created) : NULL;
						if (!pevTarget)
							break;
						pTarget = EdictFromEntvars(pevTarget);
					}

					void* priv = EngineGetPrivateData(pTarget);
					CBaseEntity* pEntity;
					if (priv)
					{
						pEntity = (CBaseEntity*)priv;
					}
					else
					{
						priv = EngineAllocPrivateData(pTarget, sizeof(CBaseEntity));
						if (!priv)
							break;
						pEntity = new (priv) CBaseEntity();
						pEntity->pev = pevTarget;
						pEntity->m_pGlobals = GlobalsFromEntvars(pevTarget);
					}

					pEntity->Use(NULL);
				}

				Fired(i) = 1;
				++Index();
			}
			else
			{
				EngineAlertMessage(1, "Manager cannot find target:%s\n", pszName);
			}
		}
	}

	// every target fired -> reset the cycle and go dormant
	if (Index() == Count())
	{
		Index() = 0;
		for (i = 0; i < Count(); ++i)
			Fired(i) = 0;

		SetDoNothingThink();			// SUB_DoNothing
		SetUse(&CMultiManager::ManagerUse);
	}
}

//=========================================================
// CMultiManager::ManagerUse
//
// Begin a firing cycle: schedule each target's absolute fire time
// (now + its delay) and switch over to ManagerThink.
//=========================================================
void CMultiManager::ManagerUse(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	float time = GlobalsTime(m_pGlobals);

	int i;
	for (i = 0; i < Count(); ++i)
		FireTime(i) = time + Delay(i);

	SetDoNothingUse();			// SUB_DoNothing
	SetThink(&CMultiManager::ManagerThink);
	PevFloat(pev, PEV_NEXTTHINK) = time;
}

//=========================================================
// CMultiManager::Spawn
//
// Make the manager non-solid and park Use/Think.
//=========================================================
void CMultiManager::Spawn()
{
	PevFloat(pev, PEV_SOLID) = SOLID_NOT;
	SetUse(&CMultiManager::ManagerUse);
	// The binary installs ManagerThink here; nextthink
	// stays 0 so it only runs once ManagerUse schedules a fire cycle.
	SetThink(&CMultiManager::ManagerThink);
}

//=========================================================
// DLLEXPORT construction wrappers
//
// The trigger_* classes share the 132-byte CBaseToggle layout but
// only ever allocate 128 bytes of private data (they never touch
// the sound bytes at [128..130]).  The size asserts bound each
// class against its natural CBaseToggle layout (132); the wrappers
// allocate the smaller entmap size (128) to match the binary.
// trigger_changelevel allocates 180 (room for the map name buffer
// at offset 128), multi_manager 392, func_friction 32.
//=========================================================

HL_COMPILE_TIME_ASSERT(sizeof(CTrigger) <= 132, CTrigger_size);
HL_COMPILE_TIME_ASSERT(sizeof(CTriggerMultiple) <= 132, CTriggerMultiple_size);
HL_COMPILE_TIME_ASSERT(sizeof(CTriggerOnce) <= 132, CTriggerOnce_size);
HL_COMPILE_TIME_ASSERT(sizeof(CTriggerHurt) <= 132, CTriggerHurt_size);
HL_COMPILE_TIME_ASSERT(sizeof(CTriggerMonsterJump) <= 132, CTriggerMonsterJump_size);
HL_COMPILE_TIME_ASSERT(sizeof(CTriggerCDAudio) <= 132, CTriggerCDAudio_size);
HL_COMPILE_TIME_ASSERT(sizeof(CTriggerCounter) <= 132, CTriggerCounter_size);
HL_COMPILE_TIME_ASSERT(sizeof(CTriggerChangeLevel) <= 180, CTriggerChangeLevel_size);
HL_COMPILE_TIME_ASSERT(sizeof(CTriggerPush) <= 132, CTriggerPush_size);
HL_COMPILE_TIME_ASSERT(sizeof(CLadder) <= 132, CLadder_size);
HL_COMPILE_TIME_ASSERT(sizeof(CFriction) <= 32, CFriction_size);
HL_COMPILE_TIME_ASSERT(sizeof(CMultiManager) <= 392, CMultiManager_size);

//=========================================================
// Shared construction helper - placement-new a T into the
// edict's private data and wire pev / m_pGlobals.
//=========================================================
template <typename T>
static void ConstructEntity(entvars_t* pev, int size)
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
		// The recompiled C++ layout can be larger than the original export's
		// private-data size (e.g. the trigger bases pad to 132 where the original
		// allocated 128). Under-allocating makes the placement-new constructor
		// write past the heap block -> delayed heap corruption. Allocate enough
		// for the actual object, while preserving any intentionally larger size
		// the caller passes (e.g. trigger_changelevel's 180).
		int allocSize = (int)sizeof(T) > size ? (int)sizeof(T) : size;
		privateData = EngineAllocPrivateData(edict, allocSize);
		if (!privateData)
			return;

		memset(privateData, 0, allocSize);

		T* self = new (privateData) T();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// Exports
//=========================================================
DLLEXPORT void trigger(entvars_t* pev)
{
	ConstructEntity<CTrigger>(pev, 128);
}

DLLEXPORT void trigger_multiple(entvars_t* pev)
{
	ConstructEntity<CTriggerMultiple>(pev, 128);
}

DLLEXPORT void trigger_once(entvars_t* pev)
{
	ConstructEntity<CTriggerOnce>(pev, 128);
}

DLLEXPORT void trigger_hurt(entvars_t* pev)
{
	ConstructEntity<CTriggerHurt>(pev, 128);
}

DLLEXPORT void trigger_push(entvars_t* pev)
{
	ConstructEntity<CTriggerPush>(pev, 128);
}

DLLEXPORT void trigger_monsterjump(entvars_t* pev)
{
	ConstructEntity<CTriggerMonsterJump>(pev, 128);
}

DLLEXPORT void trigger_counter(entvars_t* pev)
{
	ConstructEntity<CTriggerCounter>(pev, 128);
}

DLLEXPORT void trigger_changelevel(entvars_t* pev)
{
	ConstructEntity<CTriggerChangeLevel>(pev, 180);
}

DLLEXPORT void trigger_cdaudio(entvars_t* pev)
{
	ConstructEntity<CTriggerCDAudio>(pev, 128);
}

DLLEXPORT void multi_manager(entvars_t* pev)
{
	ConstructEntity<CMultiManager>(pev, 392);
}

DLLEXPORT void func_friction(entvars_t* pev)
{
	ConstructEntity<CFriction>(pev, 32);
}

DLLEXPORT void func_ladder(entvars_t* pev)
{
	ConstructEntity<CLadder>(pev, 128);
}
