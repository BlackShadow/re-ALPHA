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
// Triggers - brush trigger volumes (trigger_*), the
// multi_manager, func_friction and func_ladder
//=========================================================

#define _CRT_SECURE_NO_WARNINGS		// strcpy

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

// spawnflags
#define SF_TRIGGER_NOTOUCH			1		// trigger_multiple: not fired by touch
#define SF_COUNTER_NOMESSAGE		1		// trigger_counter: no progress messages
#define SF_TRIGGER_PUSH_ONCE		1		// trigger_push: removed after the first push
#define SF_TRIGGER_PUSH_START_OFF	2		// trigger_push: starts turned off

#define MULTI_DEFAULT_WAIT			0.2f	// trigger_multiple: time before it can fire again
#define HURT_DAMAGE_INTERVAL		0.5f	// trigger_hurt: time between two damages
#define MONSTERJUMP_SPEED			200.0f	// trigger_monsterjump: toss speed along movedir
#define MONSTERJUMP_HEIGHT			150.0f	// trigger_monsterjump: upward toss speed
#define COUNTER_DEFAULT_COUNT		2		// trigger_counter: uses needed to fire
#define PUSH_DEFAULT_SPEED			1000.0f
#define PUSH_SPEED_SCALE			10.0f	// trigger_push velocity is speed * 10
#define LANDMARK_SEARCH_RADIUS		255.0f	// info_landmark search around a trigger_changelevel

#define CDAUDIO_TRACK_STOP			-1		// trigger_cdaudio track that stops the CD
#define CDAUDIO_MAX_TRACK			12

#define cchMapNameMost				64
#define MAX_MULTI_TARGETS			16		// maximum number of targets a single multi_manager entity may be assigned.

// activator of the last trigger that fired, read by trigger_counter and the rotating doors
int g_CounterActivatorIndex = 0;

//=========================================================
// TriggerKeyValue - the keys shared by the trigger_*
// entities. "delay" is whole seconds only.
//=========================================================
static void TriggerKeyValue(CBaseTrigger *pTrigger, entvars_t *pev, float &flWait, int &cTriggersLeft, KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "wait"))
	{
		flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "count"))
	{
		cTriggersLeft = (int)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "delay"))
	{
		pTrigger->m_flDelay = (float)(int)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// trigger - reads the trigger keys but has no behaviour
// of its own
//=========================================================
class CTrigger : public CBaseTrigger
{
public:
	void KeyValue(KeyValueData *pkvd);
};

LINK_ENTITY_TO_CLASS(trigger, CTrigger);

void CTrigger::KeyValue(KeyValueData *pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// trigger_multiple - fires its targets when the player
// touches it, then waits m_flWait seconds before it can
// fire again
//=========================================================
class CTriggerMultiple : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	void MultiTouch(CBaseEntity *pOther);
	void ActivateMultiTrigger();
	void MultiWaitOver(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(trigger_multiple, CTriggerMultiple);

void CTriggerMultiple::KeyValue(KeyValueData *pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// ActivateMultiTrigger - counts a found secret, fires the
// targets, then waits m_flWait seconds or, without a wait,
// removes the trigger
//=========================================================
void CTriggerMultiple::ActivateMultiTrigger()
{
	if (gpGlobals->time < pev->nextthink)
		return;		// still waiting for reactivation

	if (FClassnameIs(pev, "trigger_secret"))
	{
		if (!FClassnameIs(VARS(pev->enemy), "player"))
			return;

		gpGlobals->found_secrets++;
		WRITE_BYTE(MSG_ALL, SVC_FOUNDSECRET);
	}

	if (pev->noise)
	{
		edict_t *pent = ENT(pev);
		EMIT_SOUND(pent, CHAN_VOICE, STRING(pev->noise), VOL_NORM, ATTN_NORM);
	}

	g_CounterActivatorIndex = pev->enemy;

	SUB_UseTargets();

	if (m_flWait <= 0.0f)
	{
		// this is a touch function: remove the trigger on the next think
		SetTouch(NULL);
		pev->nextthink = gpGlobals->time + 0.1f;
		SetThink(&CBaseEntity::SUB_Remove);
	}
	else
	{
		SetThink(&CTriggerMultiple::MultiWaitOver);
		pev->nextthink = gpGlobals->time + m_flWait;
	}
}

void CTriggerMultiple::MultiWaitOver(CBaseEntity *pOther)
{
	SetThink(NULL);
}

//=========================================================
// MultiTouch - only the player fires it. With a movedir,
// the player must also be facing that way.
//=========================================================
void CTriggerMultiple::MultiTouch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);

	if (!FClassnameIs(pevOther, "player"))
		return;

	// monster_probedroid is already turned away above
	if (!FClassnameIs(pevOther, "player") && !FClassnameIs(pevOther, "monster_probedroid"))
		return;

	const Vector &vecMoveDir = pev->movedir;
	BOOL fFire = FALSE;

	if (vecMoveDir == g_vecZero)
	{
		fFire = TRUE;
	}
	else
	{
		UTIL_MakeVectors(pevOther->angles);
		if (DotProduct(vecMoveDir, gpGlobals->v_forward) >= 0.0f)
			fFire = TRUE;
	}

	if (fFire)
	{
		pev->enemy = gpGlobals->other;
		ActivateMultiTrigger();
	}
}

void CTriggerMultiple::Spawn()
{
	PRECACHE_SOUND("common/null.wav");
	pev->noise = ALLOC_STRING("common/null.wav");

	if (m_flWait == 0.0f)
		m_flWait = MULTI_DEFAULT_WAIT;

	InitTrigger();

	if (!((int)pev->spawnflags & SF_TRIGGER_NOTOUCH))
		SetTouch(&CTriggerMultiple::MultiTouch);
}

//=========================================================
// trigger_once - a trigger_multiple that removes itself
// after firing
//=========================================================
class CTriggerOnce : public CTriggerMultiple
{
public:
	void Spawn();
};

LINK_ENTITY_TO_CLASS(trigger_once, CTriggerOnce);

void CTriggerOnce::Spawn()
{
	m_flWait = -1.0f;
	CTriggerMultiple::Spawn();
}

//=========================================================
// trigger_hurt - damages whatever touches it. A named
// trigger_hurt is switched on and off by its Use.
//=========================================================
class CTriggerHurt : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	void HurtTouch(CBaseEntity *pOther);
	void ToggleUse(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(trigger_hurt, CTriggerHurt);

void CTriggerHurt::KeyValue(KeyValueData *pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// HurtTouch - m_flDelay holds the time of the next damage
//=========================================================
void CTriggerHurt::HurtTouch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);

	if (pevOther->takedamage == DAMAGE_NO)
		return;

	if (m_flDelay > gpGlobals->time)
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance(pevOther);
	if (pEntity)
		pEntity->TakeDamage(pev, pev, pev->dmg);

	m_flDelay += HURT_DAMAGE_INTERVAL;
}

void CTriggerHurt::ToggleUse(CBaseEntity *pOther)
{
	if (pev->solid == SOLID_NOT)
		pev->solid = SOLID_TRIGGER;
	else
		pev->solid = SOLID_NOT;
}

void CTriggerHurt::Spawn()
{
	InitTrigger();
	SetTouch(&CTriggerHurt::HurtTouch);

	if (!FStringNull(pev->targetname))
		SetUse(&CTriggerHurt::ToggleUse);
	else
		SetUse(&CBaseEntity::SUB_DoNothing);
}

//=========================================================
// trigger_monsterjump - tosses the monsters that touch it
// along movedir
//=========================================================
class CTriggerMonsterJump : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	void JumpTouch(CBaseEntity *pOther);
	void ToggleUse(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(trigger_monsterjump, CTriggerMonsterJump);

void CTriggerMonsterJump::KeyValue(KeyValueData *pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

void CTriggerMonsterJump::JumpTouch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);

	if (!((int)pevOther->flags & FL_MONSTER))
		return;		// touched by a non-monster

	// lift it off the ground
	pevOther->origin.z += 1.0f;

	if ((int)pevOther->flags & FL_ONGROUND)
		pevOther->flags -= FL_ONGROUND;

	// toss the monster
	pevOther->velocity = pev->movedir * pev->speed;
	pevOther->velocity.z += m_flHeight;

	pev->solid = SOLID_NOT;
}

void CTriggerMonsterJump::ToggleUse(CBaseEntity *pOther)
{
	if (pev->solid == SOLID_NOT)
		pev->solid = SOLID_TRIGGER;
	else
		pev->solid = SOLID_NOT;
}

void CTriggerMonsterJump::Spawn()
{
	SetMovedir(pev);
	InitTrigger();

	SetUse(&CTriggerMonsterJump::ToggleUse);
	SetTouch(&CTriggerMonsterJump::JumpTouch);

	pev->speed = MONSTERJUMP_SPEED;
	m_flHeight = MONSTERJUMP_HEIGHT;

	// a named trigger starts off
	if (!FStringNull(pev->targetname))
		pev->solid = SOLID_NOT;
}

//=========================================================
// trigger_cdaudio - plays the CD track in pev->health when
// the player touches it
//=========================================================
class CTriggerCDAudio : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	void CDAudioTouch(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(trigger_cdaudio, CTriggerCDAudio);

void CTriggerCDAudio::KeyValue(KeyValueData *pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

static const char *const g_szCDPlayCommands[CDAUDIO_MAX_TRACK] =
{
	"cd play 1\n",
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
	"cd play 12\n",
};

void CTriggerCDAudio::CDAudioTouch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);
	if (!FClassnameIs(pevOther, "player"))
		return;

	edict_t *pentPlayer = ENT(pevOther);
	int iTrack = (int)pev->health;

	if (iTrack == CDAUDIO_TRACK_STOP)
		CLIENT_COMMAND(pentPlayer, "cd stop\n");
	else if (iTrack >= 1 && iTrack <= CDAUDIO_MAX_TRACK)
		CLIENT_COMMAND(pentPlayer, g_szCDPlayCommands[iTrack - 1]);
	else
		ALERT(at_console, "Unknown Track!\n");

	// plays only once
	SetTouch(&CBaseEntity::SUB_DoNothing);
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CTriggerCDAudio::Spawn()
{
	InitTrigger();
	SetTouch(&CTriggerCDAudio::CDAudioTouch);
}

//=========================================================
// trigger_counter - fires its targets once it has been
// used m_cTriggersLeft times
//=========================================================
class CTriggerCounter : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	void CounterUse(CBaseEntity *pOther);
	void ActivateMultiTrigger();
	void CounterWaitOver(CBaseEntity *pOther);
	void CounterRemoveThink(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(trigger_counter, CTriggerCounter);

void CTriggerCounter::KeyValue(KeyValueData *pkvd)
{
	TriggerKeyValue(this, pev, m_flWait, m_cTriggersLeft, pkvd);
}

//=========================================================
// ActivateMultiTrigger - the trigger_multiple firing, with
// the counter's own think functions
//=========================================================
void CTriggerCounter::ActivateMultiTrigger()
{
	if (gpGlobals->time < pev->nextthink)
		return;		// still waiting for reactivation

	if (FClassnameIs(pev, "trigger_secret"))
	{
		if (!FClassnameIs(VARS(pev->enemy), "player"))
			return;

		gpGlobals->found_secrets++;
		WRITE_BYTE(MSG_ALL, SVC_FOUNDSECRET);
	}

	if (pev->noise)
	{
		edict_t *pent = ENT(pev);
		EMIT_SOUND(pent, CHAN_VOICE, STRING(pev->noise), VOL_NORM, ATTN_NORM);
	}

	g_CounterActivatorIndex = pev->enemy;
	SUB_UseTargets();

	if (m_flWait <= 0.0f)
	{
		SetTouch(NULL);
		pev->nextthink = gpGlobals->time + 0.1f;
		SetThink(&CTriggerCounter::CounterRemoveThink);
	}
	else
	{
		SetThink(&CTriggerCounter::CounterWaitOver);
		pev->nextthink = gpGlobals->time + m_flWait;
	}
}

//=========================================================
// CounterUse - tells the player how many are left, and
// fires the targets on the last use
//=========================================================
void CTriggerCounter::CounterUse(CBaseEntity *pOther)
{
	m_cTriggersLeft--;
	if (m_cTriggersLeft < 0)
		return;

	BOOL fTellActivator = TRUE;
	if (!FClassnameIs(VARS(g_CounterActivatorIndex), "player") || ((int)pev->spawnflags & SF_COUNTER_NOMESSAGE))
		fTellActivator = FALSE;

	if (m_cTriggersLeft != 0)
	{
		if (fTellActivator)
		{
			switch (m_cTriggersLeft)
			{
			case 1:		ALERT(at_console, "Only 1 more to go...");		break;
			case 2:		ALERT(at_console, "Only 2 more to go...");		break;
			case 3:		ALERT(at_console, "Only 3 more to go...");		break;
			default:	ALERT(at_console, "There are more to go...");	break;
			}
		}
	}
	else
	{
		if (fTellActivator)
			ALERT(at_console, "Sequence completed!");

		pev->enemy = g_CounterActivatorIndex;
		ActivateMultiTrigger();
	}
}

void CTriggerCounter::CounterWaitOver(CBaseEntity *pOther)
{
	SetThink(NULL);
}

void CTriggerCounter::CounterRemoveThink(CBaseEntity *pOther)
{
	REMOVE_ENTITY(ENT(pev));
}

void CTriggerCounter::Spawn()
{
	// the counter disappears after it has fired
	m_flWait = -1.0f;

	if (m_cTriggersLeft == 0)
		m_cTriggersLeft = COUNTER_DEFAULT_COUNT;

	SetUse(&CTriggerCounter::CounterUse);
}

//=========================================================
// trigger_changelevel - moves the player to the level
// named by the "map" key
//=========================================================
class CTriggerChangeLevel : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	void ChangeLevelTouch(CBaseEntity *pOther);

	char		m_szMapName[cchMapNameMost];	// next map
};

LINK_ENTITY_TO_CLASS(trigger_changelevel, CTriggerChangeLevel);

// the map name handed to the engine
static char st_szNextMap[cchMapNameMost];

void CTriggerChangeLevel::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "map"))
	{
		memcpy(m_szMapName, pkvd->szValue, strlen(pkvd->szValue) + 1);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// ChangeLevelTouch - changes the level. The player keeps
// his position relative to an info_landmark near the
// trigger, saved here in the spawn parms.
//=========================================================
void CTriggerChangeLevel::ChangeLevelTouch(CBaseEntity *pOther)
{
	entvars_t *pevPlayer = VARS(gpGlobals->other);
	if (!FClassnameIs(pevPlayer, "player"))
		return;

	// fire only once
	SetTouch(NULL);
	pev->solid = SOLID_NOT;

	SPAWNPARMS *pSpawnParms = (SPAWNPARMS *)malloc(sizeof(SPAWNPARMS));
	gpGlobals->pSpawnParms = pSpawnParms;
	if (pSpawnParms)
		pSpawnParms->fLandmark = FALSE;

	strcpy(st_szNextMap, m_szMapName);

	SUB_UseTargets();

	if (pSpawnParms)
	{
		Vector vecCenter = pev->absmin + pev->size * 0.5f;

		// the search ends at the world
		edict_t *pent = FIND_ENTITY_IN_SPHERE(vecCenter, LANDMARK_SEARCH_RADIUS);
		while (!FNullEnt(pent))
		{
			entvars_t *pevEnt = VARS(pent);

			// the landmark's target names the landmark in the next level
			if (FClassnameIs(pevEnt, "info_landmark"))
			{
				pSpawnParms->fLandmark = TRUE;
				strcpy(pSpawnParms->szLandmarkName, STRING(pevEnt->target));
				pSpawnParms->vecLandmarkOffset = pevPlayer->origin - pevEnt->origin;
				pSpawnParms->velocity = pevPlayer->velocity;
				pSpawnParms->angles = Vector(pevPlayer->angles.x, pevPlayer->angles.y, 0.0f);
				pSpawnParms->v_angle = Vector(pevPlayer->v_angle.x, pevPlayer->v_angle.y, 0.0f);
			}

			pent = ENT(pevEnt->chain);
		}
	}

	CHANGE_LEVEL(st_szNextMap, "");
}

void CTriggerChangeLevel::Spawn()
{
	if (!m_szMapName[0])
		ALERT(at_console, "a trigger_changelevel doesn't have a map");

	InitTrigger();
	SetTouch(&CTriggerChangeLevel::ChangeLevelTouch);
}

//=========================================================
// trigger_push - pushes whatever touches it along movedir
//=========================================================
class CTriggerPush : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Touch(CBaseEntity *pOther);
	void Use(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(trigger_push, CTriggerPush);

// trigger_push takes no keys
void CTriggerPush::KeyValue(KeyValueData *pkvd)
{
}

void CTriggerPush::Touch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);

	// only living things are pushed
	if (pevOther->health > 0.0f)
		pevOther->velocity = pev->movedir * pev->speed * PUSH_SPEED_SCALE;

	if ((int)pev->spawnflags & SF_TRIGGER_PUSH_ONCE)
		REMOVE_ENTITY(ENT(pev));
}

void CTriggerPush::Use(CBaseEntity *pOther)
{
	if (pev->solid == SOLID_TRIGGER)
		pev->solid = SOLID_NOT;
	else
		pev->solid = SOLID_TRIGGER;
}

void CTriggerPush::Spawn()
{
	// InitTrigger sets movedir only for non-zero angles
	if (pev->angles == g_vecZero)
		pev->angles.y = 360.0f;

	InitTrigger();

	if (pev->speed == 0.0f)
		pev->speed = PUSH_DEFAULT_SPEED;

	if ((int)pev->spawnflags & SF_TRIGGER_PUSH_START_OFF)
		pev->solid = SOLID_NOT;
}

//=========================================================
// func_ladder - an invisible brush that puts the touching
// player on the ladder
//=========================================================
class CLadder : public CBaseTrigger
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Touch(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(func_ladder, CLadder);

// func_ladder takes no keys
void CLadder::KeyValue(KeyValueData *pkvd)
{
}

void CLadder::Touch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);
	if (!FClassnameIs(pevOther, "player"))
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pevOther);
	if (pPlayer)
		pPlayer->m_afPhysicsFlags |= PFLAG_ONLADDER;
}

void CLadder::Spawn()
{
	pev->solid = SOLID_TRIGGER;
	pev->solid = SOLID_BSP;

	edict_t *pent = ENT(pev);
	SET_MODEL(pent, STRING(pev->model));

	pev->movetype = MOVETYPE_PUSH;
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0.0f;
}

//=========================================================
// func_friction - sets the friction of whatever touches it
//=========================================================
class CFriction : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	void FrictionTouch(CBaseEntity *pOther);

	float		m_frictionFraction;		// the friction of the touching entity, 0..1
};

LINK_ENTITY_TO_CLASS(func_friction, CFriction);

void CFriction::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "modifier"))
	{
		// a percentage
		m_frictionFraction = (float)(atof(pkvd->szValue) * 0.01);
		pkvd->fHandled = TRUE;
	}
}

void CFriction::FrictionTouch(CBaseEntity *pOther)
{
	entvars_t *pevOther = VARS(gpGlobals->other);
	pevOther->friction = m_frictionFraction;
}

void CFriction::Spawn()
{
	pev->solid = SOLID_TRIGGER;

	edict_t *pent = ENT(pev);
	SET_MODEL(pent, STRING(pev->model));

	pev->movetype = MOVETYPE_NONE;
	SetTouch(&CFriction::FrictionTouch);
}

//=========================================================
// multi_manager - fires each of its targets after its own
// delay. Every key other than "wait" names a target, the
// value is the delay.
//=========================================================
class CMultiManager : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	void ManagerUse(CBaseEntity *pOther);
	void ManagerThink(CBaseEntity *pOther);

	int			m_cTargets;								// the total number of targets in this manager's fire list.
	int			m_index;								// number of targets fired in this cycle
	string_t	m_iTargetName[MAX_MULTI_TARGETS];		// list of target names
	BOOL		m_fTargetFired[MAX_MULTI_TARGETS];		// target fired in this cycle
	float		m_flTargetDelay[MAX_MULTI_TARGETS];		// delay (in seconds) from time of manager fire to target fire
	float		m_flTargetFireTime[MAX_MULTI_TARGETS];	// time to fire each target
};

LINK_ENTITY_TO_CLASS(multi_manager, CMultiManager);

void CMultiManager::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		int iTarget = m_cTargets;
		if (iTarget < MAX_MULTI_TARGETS)
		{
			m_iTargetName[iTarget] = ALLOC_STRING(pkvd->szKeyName);
			m_flTargetDelay[iTarget] = (float)atof(pkvd->szValue);
			m_fTargetFired[iTarget] = FALSE;
			m_cTargets = iTarget + 1;
			pkvd->fHandled = TRUE;
		}
	}
}

//=========================================================
// ManagerThink - fires the targets whose time has come.
// Once all have fired, waits to be used again.
//=========================================================
void CMultiManager::ManagerThink(CBaseEntity *pOther)
{
	float flTime = gpGlobals->time;

	pev->nextthink = flTime + 0.1f;

	int i;
	for (i = 0; i < m_cTargets; i++)
	{
		if (flTime >= m_flTargetFireTime[i] && !m_fTargetFired[i])
		{
			const char *pszTarget = STRING(m_iTargetName[i]);
			edict_t *pentTarget = FIND_ENTITY_BY_STRING(NULL, "targetname", pszTarget);

			if (!FNullEnt(pentTarget))
			{
				// the search ends at the world
				while (!FNullEnt(pentTarget))
				{
					CBaseEntity *pTarget = GetClassPtr((CBaseEntity *)VARS(pentTarget));
					pTarget->Use(NULL);

					pentTarget = FIND_ENTITY_BY_STRING(pentTarget, "targetname", pszTarget);
				}

				m_fTargetFired[i] = TRUE;
				m_index++;
			}
			else
			{
				ALERT(at_console, "Manager cannot find target:%s\n", pszTarget);
			}
		}
	}

	// all fired, wait to be used again
	if (m_index == m_cTargets)
	{
		m_index = 0;
		for (i = 0; i < m_cTargets; i++)
			m_fTargetFired[i] = FALSE;

		SetThink(&CBaseEntity::SUB_DoNothing);
		SetUse(&CMultiManager::ManagerUse);
	}
}

//=========================================================
// ManagerUse - starts a firing cycle
//=========================================================
void CMultiManager::ManagerUse(CBaseEntity *pOther)
{
	float flTime = gpGlobals->time;

	for (int i = 0; i < m_cTargets; i++)
		m_flTargetFireTime[i] = flTime + m_flTargetDelay[i];

	SetUse(&CBaseEntity::SUB_DoNothing);
	SetThink(&CMultiManager::ManagerThink);
	pev->nextthink = flTime;
}

void CMultiManager::Spawn()
{
	pev->solid = SOLID_NOT;
	SetUse(&CMultiManager::ManagerUse);

	// thinks only once ManagerUse sets nextthink
	SetThink(&CMultiManager::ManagerThink);
}
