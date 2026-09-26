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
// Doors - func_door, func_water, func_door_rotating and
// momentary_door
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "doors.h"

#define DOOR_DEFAULT_SPEED			100.0f
#define DOOR_DEFAULT_DMG			2.0f
#define ROTDOOR_WAIT				2.0f	// func_door_rotating always waits this long
#define DOOR_PLAYER_LOOKAHEAD		10.0f	// seconds of player movement that pick the swing direction

#define noiseMoving		noise1
#define noiseArrived	noise2

// activator of the last trigger that fired (triggers.cpp)
extern int g_CounterActivatorIndex;

// "movesnd" and "stopsnd" 1 to DOOR_SOUND_COUNT, anything else is silent
#define DOOR_SOUND_COUNT	8

static const char szNullSound[] = "common/null.wav";

static const char *const pDoorMoveSounds[DOOR_SOUND_COUNT] =
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

static const char *const pDoorStopSounds[DOOR_SOUND_COUNT] =
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

class CBaseDoor : public CBaseToggle
{
public:
	void Spawn();
	void Precache();
	void Use(CBaseEntity *pOther);
	void Blocked(CBaseEntity *pOther);

	void DoorTouch(CBaseEntity *pOther);
	void DoorActivate();
	void DoorGoUp(CBaseEntity *pOther);
	void DoorGoDown(CBaseEntity *pOther);
	void DoorHitTop(CBaseEntity *pOther);
	void DoorHitBottom(CBaseEntity *pOther);
	void WaterThink(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(func_door, CBaseDoor);
LINK_ENTITY_TO_CLASS(func_water, CBaseDoor);

//=========================================================
// Precache - binds the moving and stop sounds picked by
// the "movesnd" and "stopsnd" keys
//=========================================================
void CBaseDoor::Precache()
{
	int iMoveSnd = m_bMoveSnd;
	const char *pszMoveSound;
	if (iMoveSnd >= 1 && iMoveSnd <= DOOR_SOUND_COUNT)
	{
		pszMoveSound = pDoorMoveSounds[iMoveSnd - 1];
		PRECACHE_SOUND(pszMoveSound);
	}
	else
	{
		pszMoveSound = szNullSound;
	}
	pev->noiseMoving = ALLOC_STRING(pszMoveSound);

	int iStopSnd = m_bStopSnd;
	const char *pszStopSound;
	if (iStopSnd >= 1 && iStopSnd <= DOOR_SOUND_COUNT)
	{
		pszStopSound = pDoorStopSounds[iStopSnd - 1];
		PRECACHE_SOUND(pszStopSound);
	}
	else
	{
		pszStopSound = szNullSound;
	}
	pev->noiseArrived = ALLOC_STRING(pszStopSound);
}

//=========================================================
// WaterThink - func_water sends its render color to the
// clients as the water color
//=========================================================
void CBaseDoor::WaterThink(CBaseEntity *pOther)
{
	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_WATERCOLOR);
	WRITE_BYTE(MSG_BROADCAST, (int)pev->rendercolor.x);
	WRITE_BYTE(MSG_BROADCAST, (int)pev->rendercolor.y);
	WRITE_BYTE(MSG_BROADCAST, (int)pev->rendercolor.z);
	WRITE_BYTE(MSG_BROADCAST, 1);	// unused

	SetThink(NULL);
}

//=========================================================
// Spawn
//=========================================================
void CBaseDoor::Spawn()
{
	Precache();
	SetMovedir(pev);

	if (pev->skin == 0)
	{
		// normal door
		pev->solid = SOLID_NOT;
		if (((int)pev->spawnflags & SF_DOOR_PASSABLE) == 0)
			pev->solid = SOLID_BSP;

		pev->movetype = MOVETYPE_PUSH;
	}
	else
	{
		// water, the skin holds the contents
		pev->solid = SOLID_NOT;
		SetThink(&CBaseDoor::WaterThink);
		pev->nextthink = gpGlobals->time + 1.0f;
	}

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model));

	if (pev->speed == 0)
		pev->speed = DOOR_DEFAULT_SPEED;
	if (pev->dmg == 0)
		pev->dmg = DOOR_DEFAULT_DMG;

	m_vecPosition1 = pev->origin;

	// the door moves the size of the brush along movedir, minus the lip
	float flTravel = fabsf(pev->movedir.y * pev->size.y + pev->movedir.z * pev->size.z + pev->size.x * pev->movedir.x) - m_flLip;
	m_vecPosition2 = pev->movedir * flTravel + m_vecPosition1;

	if ((int)pev->spawnflags & SF_DOOR_START_OPEN)
	{
		// swap pos1 and pos2, put door at pos2
		UTIL_SetOrigin(pev, m_vecPosition2);
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = pev->origin;
	}

	m_toggle_state = TS_AT_BOTTOM;

	if ((int)pev->spawnflags & SF_DOOR_USE_ONLY)
		SetTouch(&CBaseEntity::SUB_DoNothing);
	else
		SetTouch(&CBaseDoor::DoorTouch);
}

//=========================================================
// DoorTouch - a player touching the door opens it, unless
// the door has a targetname
//=========================================================
void CBaseDoor::DoorTouch(CBaseEntity *pOther)
{
	entvars_t *pevToucher = VARS(gpGlobals->other);

	// ignore touches by anything but players
	if (!FClassnameIs(pevToucher, "player"))
		return;

	// a door with a targetname is opened by the entity that targets it
	if (!FStringNull(pev->targetname))
		return;

	SetTouch(NULL);
	m_hActivator = gpGlobals->other;	// remember who activated the door
	DoorActivate();
}

//=========================================================
// Use - opens a closed door, or closes an open
// no-auto-return door
//=========================================================
void CBaseDoor::Use(CBaseEntity *pOther)
{
	if (m_toggle_state == TS_AT_BOTTOM || (((int)pev->spawnflags & SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP))
		DoorActivate();
}

//=========================================================
// DoorActivate
//=========================================================
void CBaseDoor::DoorActivate()
{
	if (((int)pev->spawnflags & SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP)
	{
		// door should close
		DoorGoDown(NULL);
		return;
	}

	// door should open, give health if a player opened it
	entvars_t *pevActivator = VARS(m_hActivator);
	if (FClassnameIs(pevActivator, "player"))
		pevActivator->health += m_bHealthValue;

	DoorGoUp(NULL);
}

//=========================================================
// DoorGoUp - starts the door going to its "up" position
// (m_vecPosition2)
//=========================================================
void CBaseDoor::DoorGoUp(CBaseEntity *pOther)
{
	entvars_t *pevActivator = VARS(gpGlobals->other);

	EMIT_SOUND(ENT(pev), CHAN_VOICE, STRING(pev->noiseMoving), VOL_NORM, ATTN_NORM);

	m_toggle_state = TS_GOING_UP;
	SetMoveDone(&CBaseDoor::DoorHitTop);

	const char *pszClassname = STRING(pev->classname);
	if (FStrEq(pszClassname, "func_door"))
	{
		LinearMove(m_vecPosition2, pev->speed);
	}
	else if (FStrEq(pszClassname, "func_door_rotating"))
	{
		float flSign = 1.0f;

		// not opened by a player, try the activator of the last trigger
		if (!FClassnameIs(pevActivator, "player"))
			pevActivator = VARS(g_CounterActivatorIndex);

		if (((int)pev->spawnflags & SF_DOOR_ONEWAY) == 0 && FClassnameIs(pevActivator, "player"))
		{
			// Y axis rotation, move away from the player
			if (pev->movedir.y != 0)
			{
				UTIL_MakeVectors(Vector(0.0f, pevActivator->angles.y, 0.0f));

				Vector vecToPlayer = pevActivator->origin - pev->origin;
				Vector vecNext = (pevActivator->velocity * DOOR_PLAYER_LOOKAHEAD + pevActivator->origin) - pev->origin;
				if (vecNext.y * vecToPlayer.x - vecNext.x * vecToPlayer.y < 0.0f)
					flSign = -1.0f;
			}
		}

		AngularMove(m_vecPosition2 * flSign, pev->speed);
	}

	SUB_UseTargets();
}

//=========================================================
// DoorGoDown - starts the door going to its "down" position
// (m_vecPosition1)
//=========================================================
void CBaseDoor::DoorGoDown(CBaseEntity *pOther)
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, STRING(pev->noiseMoving), VOL_NORM, ATTN_NORM);

	m_toggle_state = TS_GOING_DOWN;
	SetMoveDone(&CBaseDoor::DoorHitBottom);

	const char *pszClassname = STRING(pev->classname);
	if (FStrEq(pszClassname, "func_door"))
	{
		LinearMove(m_vecPosition1, pev->speed);
	}
	else if (FStrEq(pszClassname, "func_door_rotating"))
	{
		AngularMove(m_vecPosition1, pev->speed);
	}
}

//=========================================================
// DoorHitTop - the door has reached the "up" position.
// Either go back down, or wait for another activation.
//=========================================================
void CBaseDoor::DoorHitTop(CBaseEntity *pOther)
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, STRING(pev->noiseArrived), VOL_NORM, ATTN_NORM);

	m_toggle_state = TS_AT_TOP;

	int iFlags = (int)pev->spawnflags;
	if (iFlags & SF_DOOR_NO_AUTO_RETURN)
	{
		// toggle doors wait for refire, re-instate the touch
		if ((iFlags & SF_DOOR_USE_ONLY) == 0)
			SetTouch(&CBaseDoor::DoorTouch);
	}
	else
	{
		// in m_flWait seconds DoorGoDown fires, unless wait is -1, then the door stays open
		pev->nextthink = pev->ltime + m_flWait;
		SetThink(&CBaseDoor::DoorGoDown);

		if (m_flWait == -1.0f)
			pev->nextthink = -1.0f;
	}
}

//=========================================================
// DoorHitBottom - the door has reached the "down" position
//=========================================================
void CBaseDoor::DoorHitBottom(CBaseEntity *pOther)
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, STRING(pev->noiseArrived), VOL_NORM, ATTN_NORM);

	m_toggle_state = TS_AT_BOTTOM;

	// re-instate the touch, the cycle is complete
	if ((int)pev->spawnflags & SF_DOOR_USE_ONLY)
		SetTouch(&CBaseEntity::SUB_DoNothing);
	else
		SetTouch(&CBaseDoor::DoorTouch);
}

//=========================================================
// Blocked - hurts the blocker and reverses the door, along
// with every func_door of the same name
//=========================================================
void CBaseDoor::Blocked(CBaseEntity *pOther)
{
	CBaseEntity *pBlocker = CBaseEntity::Instance(gpGlobals->other);
	if (pBlocker)
		pBlocker->TakeDamage(pev, pev, pev->dmg);

	// a door with a negative wait would never come back if blocked,
	// so let it just squash the object to death
	if (m_flWait >= 0)
	{
		if (m_toggle_state == TS_GOING_DOWN)
			DoorGoUp(NULL);
		else
			DoorGoDown(NULL);
	}

	const char *pszTargetname = STRING(pev->targetname);
	edict_t *pentTarget = NULL;
	while ((pentTarget = FIND_ENTITY_BY_STRING(pentTarget, "targetname", pszTargetname)) != NULL)
	{
		// the search ends at the world
		if (FNullEnt(pentTarget))
			break;

		entvars_t *pevTarget = VARS(pentTarget);
		if (!FClassnameIs(pevTarget, "func_door"))
			continue;

		CBaseDoor *pDoor = GetClassPtr((CBaseDoor *)pevTarget);

		if (pDoor->m_flWait >= 0)
		{
			pevTarget->origin = pev->origin;

			if (pDoor->m_toggle_state == TS_GOING_DOWN)
				pDoor->DoorGoUp(NULL);
			else
				pDoor->DoorGoDown(NULL);
		}
	}
}

//=========================================================
// func_door_rotating
//=========================================================
class CRotDoor : public CBaseDoor
{
public:
	void Spawn();
};

LINK_ENTITY_TO_CLASS(func_door_rotating, CRotDoor);

//=========================================================
// AxisDir - rotation axis from the spawnflags
//=========================================================
static void AxisDir(entvars_t *pev)
{
	if ((int)pev->spawnflags & SF_DOOR_ROTATE_Z)
		pev->movedir = Vector(0.0f, 0.0f, 1.0f);	// around z-axis
	else if ((int)pev->spawnflags & SF_DOOR_ROTATE_X)
		pev->movedir = Vector(1.0f, 0.0f, 0.0f);	// around x-axis
	else
		pev->movedir = Vector(0.0f, 1.0f, 0.0f);	// around y-axis
}

//=========================================================
// Spawn
//=========================================================
void CRotDoor::Spawn()
{
	Precache();

	// set the axis of rotation
	AxisDir(pev);

	// check for clockwise rotation
	if ((int)pev->spawnflags & SF_DOOR_ROTATE_BACKWARDS)
		pev->movedir = -pev->movedir;

	m_flWait = ROTDOOR_WAIT;

	m_vecPosition1 = pev->angles;
	m_vecPosition2 = pev->movedir * m_flMoveDistance + pev->angles;

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model));

	if (pev->speed == 0)
		pev->speed = DOOR_DEFAULT_SPEED;
	if (pev->dmg == 0)
		pev->dmg = DOOR_DEFAULT_DMG;

	if ((int)pev->spawnflags & SF_DOOR_START_OPEN)
	{
		// put the door at pos2, which becomes the closed position
		pev->angles = m_vecPosition2;
		m_vecPosition2 = m_vecPosition1;
		pev->movedir = -pev->movedir;
	}

	m_toggle_state = TS_AT_BOTTOM;

	if ((int)pev->spawnflags & SF_DOOR_USE_ONLY)
		SetTouch(&CBaseEntity::SUB_DoNothing);
	else
		SetTouch(&CBaseDoor::DoorTouch);
}

//=========================================================
// momentary_door - moves to the position set by a
// momentary_rot_button
//=========================================================
class CMomentaryDoor : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(momentary_door, CMomentaryDoor);

//=========================================================
// Spawn
//=========================================================
void CMomentaryDoor::Spawn()
{
	SetMovedir(pev);

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model));

	if (pev->speed == 0)
		pev->speed = DOOR_DEFAULT_SPEED;
	if (pev->dmg == 0)
		pev->dmg = DOOR_DEFAULT_DMG;

	m_vecPosition1 = pev->origin;

	float flTravel = fabsf(pev->movedir.y * pev->size.y + pev->movedir.z * pev->size.z + pev->size.x * pev->movedir.x) - m_flLip;
	m_vecPosition2 = pev->movedir * flTravel + m_vecPosition1;

	SetMoveDone(&CBaseEntity::SUB_DoNothing);

	if ((int)pev->spawnflags & SF_DOOR_START_OPEN)
	{
		// swap pos1 and pos2, put door at pos2
		UTIL_SetOrigin(pev, m_vecPosition2);
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = pev->origin;
	}
}

//=========================================================
// KeyValue - the door sound keys are accepted but ignored
//=========================================================
void CMomentaryDoor::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsnd"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// Use - momentary_rot_button passes a pointer to its
// position (0 to 1) instead of an entity
//=========================================================
void CMomentaryDoor::Use(CBaseEntity *pOther)
{
	if (!pOther)
		return;

	float flValue = *(float *)pOther;
	if (flValue > 1.0f)
		flValue = 1.0f;

	Vector vecMove = m_vecPosition1 + (m_vecPosition2 - m_vecPosition1) * flValue;
	LinearMove(vecMove, pev->speed);
}
