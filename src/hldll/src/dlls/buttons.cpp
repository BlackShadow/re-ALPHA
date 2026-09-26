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
// Buttons - func_button, func_rot_button,
// momentary_rot_button and multisource
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

// func_button spawnflags
#define SF_BUTTON_TOGGLE			32		// button stays pushed until used again
#define SF_BUTTON_SPARK_IF_OFF		64		// button sparks in OFF state
#define SF_BUTTON_TOUCH_ONLY		256		// button is pressed by touching it, not by using it

// func_rot_button / momentary_rot_button spawnflags
#define SF_ROTBUTTON_BACKWARDS		2
#define SF_ROTBUTTON_AUTO_RETURN	16		// momentary_rot_button spins back to the start when released
#define SF_ROTBUTTON_ROTATE_Z		64
#define SF_ROTBUTTON_ROTATE_X		128

#define BUTTON_DEFAULT_SPEED		40.0f
#define BUTTON_DEFAULT_WAIT			1.0f
#define BUTTON_DEFAULT_LIP			4.0f
#define BUTTON_SHOT_HEALTH			9999.0f	// a shootable button never runs out of health
#define MOMENTARY_DEFAULT_SPEED		100.0f

#define MS_MAX_TARGETS				10		// most entities a multisource waits for

#define BUTTON_SPARK_SOUNDS			6

static const char *pSparkSounds[BUTTON_SPARK_SOUNDS] =
{
	"buttons/spark1.wav",
	"buttons/spark2.wav",
	"buttons/spark3.wav",
	"buttons/spark4.wav",
	"buttons/spark5.wav",
	"buttons/spark6.wav",
};

//=========================================================
// ButtonSound - the press sound picked by the "sounds" key,
// 0 is silent
//=========================================================
static const char *ButtonSound(int sound)
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
// AxisDir - rotation axis from the spawnflags
//=========================================================
static void AxisDir(entvars_t *pev)
{
	Vector &vecMoveDir = pev->movedir;
	int iFlags = (int)pev->spawnflags;

	if (iFlags & SF_ROTBUTTON_ROTATE_Z)
	{
		// around z-axis
		vecMoveDir.x = 0.0f;
		vecMoveDir.y = 0.0f;
		vecMoveDir.z = 1.0f;
	}
	else if (iFlags & SF_ROTBUTTON_ROTATE_X)
	{
		// around x-axis
		vecMoveDir.x = 1.0f;
		vecMoveDir.y = 0.0f;
		vecMoveDir.z = 0.0f;
	}
	else
	{
		// around y-axis
		vecMoveDir.x = 0.0f;
		vecMoveDir.y = 1.0f;
		vecMoveDir.z = 0.0f;
	}
}

//=========================================================
// AxisDelta - angle difference around the rotation axis
//=========================================================
static float AxisDelta(int iFlags, const Vector &vecAngle1, const Vector &vecAngle2)
{
	if (iFlags & SF_ROTBUTTON_ROTATE_Z)
		return vecAngle1.z - vecAngle2.z;

	if (iFlags & SF_ROTBUTTON_ROTATE_X)
		return vecAngle1.x - vecAngle2.x;

	return vecAngle1.y - vecAngle2.y;
}

//=========================================================
// multisource - fires its target once every entity that
// targets it has fired
//=========================================================
class CMultiSource : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pOther);

	void Register();
	void RegisterThink(CBaseEntity *pOther);
	int IsTriggered();

	int		m_iCount;		// entities that have not fired yet
	int		m_iTotal;		// entities that target the multisource
};

//=========================================================
// KeyValue - the editor keys are accepted but ignored
//=========================================================
void CMultiSource::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "style")
		|| FStrEq(pkvd->szKeyName, "height")
		|| FStrEq(pkvd->szKeyName, "killtarget")
		|| FStrEq(pkvd->szKeyName, "value1")
		|| FStrEq(pkvd->szKeyName, "value2")
		|| FStrEq(pkvd->szKeyName, "value3"))
	{
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// RegisterThink - counts the entities that target the
// multisource, once they have all spawned
//=========================================================
void CMultiSource::RegisterThink(CBaseEntity *pOther)
{
	m_iTotal = 0;
	m_iCount = 0;
	SetThink(&CBaseEntity::SUB_DoNothing);

	const char *pszTargetname = STRING(pev->targetname);
	edict_t *pentTarget = FIND_ENTITY_BY_STRING(NULL, "target", pszTargetname);

	while (!FNullEnt(pentTarget) && m_iTotal < MS_MAX_TARGETS)
	{
		m_iTotal++;

		// the search links every match through pev->chain
		entvars_t *pevTarget = VARS(pentTarget);
		EOFFSET eoffsetNext = pevTarget ? pevTarget->chain : 0;
		pentTarget = eoffsetNext ? ENT(eoffsetNext) : NULL;
	}

	m_iCount = m_iTotal;
}

//=========================================================
// Spawn
//=========================================================
void CMultiSource::Spawn()
{
	pev->nextthink = pev->ltime + 1.0f;
	SetThink(&CMultiSource::RegisterThink);
	SetUse(&CMultiSource::Use);
}

//=========================================================
// Use - one more entity has fired, fire the target when
// it was the last one
//=========================================================
void CMultiSource::Use(CBaseEntity *pOther)
{
	if (m_iCount <= 0)
		return;

	m_iCount--;
	if (m_iCount != 0)
		return;

	if (FStringNull(pev->target))
		return;

	EOFFSET eoffsetSelf = gpGlobals->self;
	EOFFSET eoffsetOther = gpGlobals->other;

	const char *pszTarget = STRING(pev->target);
	edict_t *pentTarget = NULL;

	while ((pentTarget = FIND_ENTITY_BY_STRING(pentTarget, "targetname", pszTarget)) != NULL)
	{
		// the search ends at the world
		if (FNullEnt(pentTarget))
			break;

		gpGlobals->self = OFFSET(pentTarget);
		gpGlobals->other = eoffsetSelf;

		CBaseEntity *pTarget = GetClassPtr((CBaseEntity *)VARS(pentTarget));
		pTarget->Use(NULL);

		gpGlobals->self = eoffsetSelf;
		gpGlobals->other = eoffsetOther;
	}
}

//=========================================================
// Register - a button that targets the multisource is back
// home, it has to fire again
//=========================================================
void CMultiSource::Register()
{
	if (m_iTotal > m_iCount)
		m_iCount++;
}

//=========================================================
// IsTriggered - every entity that targets the multisource
// has fired
//=========================================================
int CMultiSource::IsTriggered()
{
	return (m_iCount == 0) ? TRUE : FALSE;
}

//=========================================================
// func_button
//=========================================================
class CBaseButton : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage);

	void ButtonUse(CBaseEntity *pOther);
	void ButtonTouch(CBaseEntity *pOther);
	void ButtonSpark(CBaseEntity *pOther);

	void ButtonActivate();
	void TriggerAndWait(CBaseEntity *pOther);
	void ButtonReturn(CBaseEntity *pOther);
	void ButtonBackHome(CBaseEntity *pOther);

	void PlayPressSound();

	BOOL		m_fStayPushed;	// button stays pushed in until touched again?
	BOOL		m_fRotating;	// a rotating button? default is a sliding button
	string_t	m_sMaster;		// multisource that has to be triggered before the button works
};

//=========================================================
// func_rot_button
//=========================================================
class CRotButton : public CBaseButton
{
public:
	void Spawn();
};

void CBaseButton::PlayPressSound()
{
	edict_t *pentButton = (pev ? ENT(pev) : NULL);
	const char *pszSample = STRING(pev->noise);
	EMIT_SOUND(pentButton, CHAN_VOICE, pszSample, VOL_NORM, ATTN_NORM);
}

//=========================================================
// KeyValue
//=========================================================
void CBaseButton::KeyValue(KeyValueData *pkvd)
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
	else if (FStrEq(pkvd->szKeyName, "delay"))
	{
		m_flDelay = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_sMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// ButtonSpark - makes sparks at the button center, again
// and again at random intervals
//=========================================================
void CBaseButton::ButtonSpark(CBaseEntity *pOther)
{
	SetThink(&CBaseButton::ButtonSpark);
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.0f, 1.5f) + 0.1f;

	Vector &vecMins = pev->mins;
	Vector &vecSize = pev->size;

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_SPARKS);
	WRITE_COORD(MSG_BROADCAST, vecSize.x * 0.5f + vecMins.x);
	WRITE_COORD(MSG_BROADCAST, vecSize.y * 0.5f + vecMins.y);
	WRITE_COORD(MSG_BROADCAST, vecSize.z * 0.5f + vecMins.z);

	float flVolume = RANDOM_FLOAT(0.25f, 0.75f);
	int iSound = (int)(RANDOM_FLOAT(0.0f, 1.0f) * BUTTON_SPARK_SOUNDS);
	if (iSound < 0 || iSound >= BUTTON_SPARK_SOUNDS)
		return;

	edict_t *pentButton = (pev ? ENT(pev) : NULL);
	EMIT_SOUND(pentButton, CHAN_VOICE, pSparkSounds[iSound], flVolume, ATTN_NORM);
}

//=========================================================
// ButtonActivate - starts the button moving "in/up"
//=========================================================
void CBaseButton::ButtonActivate()
{
	PlayPressSound();

	m_toggle_state = TS_GOING_UP;
	SetMoveDone(&CBaseButton::TriggerAndWait);

	float flSpeed = pev->speed;

	if (!m_fRotating)
		LinearMove(m_vecPosition2, flSpeed);
	else
		AngularMove(m_vecPosition2, flSpeed);
}

//=========================================================
// ButtonReturn - starts the button moving "out/down"
//=========================================================
void CBaseButton::ButtonReturn(CBaseEntity *pOther)
{
	m_toggle_state = TS_GOING_DOWN;
	SetMoveDone(&CBaseButton::ButtonBackHome);

	float flSpeed = pev->speed;

	if (!m_fRotating)
		LinearMove(m_vecPosition1, flSpeed);
	else
		AngularMove(m_vecPosition1, flSpeed);

	pev->frame = 0;		// use normal textures
}

//=========================================================
// TriggerAndWait - the button has reached the "in/up"
// position, fire the targets and wait
//=========================================================
void CBaseButton::TriggerAndWait(CBaseEntity *pOther)
{
	m_toggle_state = TS_AT_TOP;

	int iFlags = (int)pev->spawnflags;

	// if the button automatically comes back out, start it moving out,
	// else re-instate the touch method
	if (m_fStayPushed || (iFlags & SF_BUTTON_TOGGLE))
	{
		SetTouch(&CBaseEntity::SUB_DoNothing);
		if (iFlags & SF_BUTTON_TOUCH_ONLY)
			SetTouch(&CBaseButton::ButtonTouch);
	}
	else
	{
		pev->nextthink = pev->ltime + m_flWait;
		SetThink(&CBaseButton::ButtonReturn);
	}

	pev->frame = 1;		// use alternate textures

	SUB_UseTargets();
}

//=========================================================
// ButtonBackHome - the button is back "out/down", the
// multisources it targets have to wait for it again
//=========================================================
void CBaseButton::ButtonBackHome(CBaseEntity *pOther)
{
	m_toggle_state = TS_AT_BOTTOM;

	if (!FStringNull(pev->target))
	{
		const char *pszTarget = STRING(pev->target);
		edict_t *pentTarget = NULL;

		while ((pentTarget = FIND_ENTITY_BY_STRING(pentTarget, "targetname", pszTarget)) != NULL)
		{
			// the search ends at the world
			if (FNullEnt(pentTarget))
				break;

			entvars_t *pevTarget = VARS(pentTarget);
			if (!FClassnameIs(pevTarget, "multisource"))
				continue;

			CMultiSource *pSource = GetClassPtr((CMultiSource *)pevTarget);
			pSource->Register();
		}
	}

	int iFlags = (int)pev->spawnflags;

	// re-instate the touch method, the movement cycle is complete
	SetTouch(&CBaseEntity::SUB_DoNothing);
	if (iFlags & SF_BUTTON_TOUCH_ONLY)
		SetTouch(&CBaseButton::ButtonTouch);

	// reset think for a sparking button
	if (iFlags & SF_BUTTON_SPARK_IF_OFF)
	{
		SetThink(&CBaseButton::ButtonSpark);
		pev->nextthink = gpGlobals->time + 0.5f;
	}
}

//=========================================================
// ButtonUse
//=========================================================
void CBaseButton::ButtonUse(CBaseEntity *pOther)
{
	ALERT(at_console, "%d\n", m_fStayPushed);

	// ignore uses while the button is moving
	if (m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN)
		return;

	int iFlags = (int)pev->spawnflags;

	// a button that stays pushed can't be used again, unless it is a toggle button
	if (m_toggle_state == TS_AT_TOP && m_fStayPushed && !(iFlags & SF_BUTTON_TOGGLE))
		return;

	m_hActivator = gpGlobals->other;

	if ((!m_fStayPushed && !(iFlags & SF_BUTTON_TOGGLE)) || m_toggle_state != TS_AT_TOP)
	{
		ButtonActivate();
	}
	else
	{
		PlayPressSound();
		SUB_UseTargets();
		ButtonReturn(NULL);
	}
}

//=========================================================
// ButtonTouch - touching a SF_BUTTON_TOUCH_ONLY button
// presses it
//=========================================================
void CBaseButton::ButtonTouch(CBaseEntity *pOther)
{
	if (FNullEnt(gpGlobals->other))
		return;

	// ignore touches by anything but players
	if (!FClassnameIs(VARS(gpGlobals->other), "player"))
		return;

	// ignore touches while the button is moving
	if (m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN)
		return;

	int iFlags = (int)pev->spawnflags;

	// or pushed in and waiting to come out on its own
	if (m_toggle_state == TS_AT_TOP && !m_fStayPushed && !(iFlags & SF_BUTTON_TOGGLE))
		return;

	// the master has to be triggered
	if (m_sMaster)
	{
		edict_t *pentMaster = FIND_ENTITY_BY_STRING(NULL, "targetname", STRING(m_sMaster));
		if (!FNullEnt(pentMaster))
		{
			entvars_t *pevMaster = VARS(pentMaster);
			if (FClassnameIs(pevMaster, "multisource"))
			{
				CMultiSource *pSource = GetClassPtr((CMultiSource *)pevMaster);
				if (!pSource->IsTriggered())
					return;
			}
		}
	}

	// temporarily disable the touch function, until movement is finished
	SetTouch(NULL);
	m_hActivator = gpGlobals->other;

	if ((m_fStayPushed || (iFlags & SF_BUTTON_TOGGLE)) && m_toggle_state == TS_AT_TOP)
	{
		PlayPressSound();
		SUB_UseTargets();
		ButtonReturn(NULL);
	}
	else
	{
		ButtonActivate();
	}
}

//=========================================================
// TakeDamage - shooting the button presses it
//=========================================================
int CBaseButton::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage)
{
	pev->health = BUTTON_SHOT_HEALTH;

	// ignore shots while the button is moving, or pushed in and waiting to come out
	if (m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN)
		return 0;

	if (m_toggle_state == TS_AT_TOP && !m_fStayPushed)
		return 0;

	SetTouch(NULL);
	m_hActivator = gpGlobals->other;

	if (!m_fStayPushed || m_toggle_state != TS_AT_TOP)
	{
		ButtonActivate();
	}
	else
	{
		PlayPressSound();
		SUB_UseTargets();
		ButtonReturn(NULL);
	}

	return 0;
}

//=========================================================
// Spawn
//=========================================================
void CBaseButton::Spawn()
{
	const char *pszSound = ButtonSound((int)pev->sounds);
	PRECACHE_SOUND(pszSound);
	pev->noise = ALLOC_STRING(pszSound);

	// this button should spark in OFF state
	if ((int)pev->spawnflags & SF_BUTTON_SPARK_IF_OFF)
	{
		PRECACHE_SOUND(pSparkSounds[0]);
		PRECACHE_SOUND(pSparkSounds[1]);
		PRECACHE_SOUND(pSparkSounds[2]);
		PRECACHE_SOUND(pSparkSounds[3]);
		PRECACHE_SOUND(pSparkSounds[4]);
		PRECACHE_SOUND(pSparkSounds[5]);

		SetThink(&CBaseButton::ButtonSpark);
		pev->nextthink = gpGlobals->time + 0.5f;
	}

	SetMovedir(pev);

	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;

	edict_t *pentButton = (pev ? ENT(pev) : NULL);
	SET_MODEL(pentButton, STRING(pev->model));

	if (pev->speed == 0)
		pev->speed = BUTTON_DEFAULT_SPEED;

	if (pev->health > 0)
		pev->takedamage = DAMAGE_YES;

	if (m_flWait == 0)
		m_flWait = BUTTON_DEFAULT_WAIT;

	if (m_flLip == 0)
		m_flLip = BUTTON_DEFAULT_LIP;

	m_toggle_state = TS_AT_BOTTOM;

	// the button moves the size of the brush along movedir, minus the lip
	m_vecPosition1 = pev->origin;

	float flTravel = (float)fabs(DotProduct(pev->movedir, pev->size)) - m_flLip;
	m_vecPosition2 = pev->movedir * flTravel + m_vecPosition1;

	m_fRotating = FALSE;
	m_fStayPushed = (m_flWait == -1.0f) ? TRUE : FALSE;

	if ((int)pev->spawnflags & SF_BUTTON_TOUCH_ONLY)
	{
		// touchable button
		SetTouch(&CBaseButton::ButtonTouch);
	}
	else
	{
		SetTouch(&CBaseEntity::SUB_DoNothing);
		SetUse(&CBaseButton::ButtonUse);
	}
}

//=========================================================
// Spawn
//=========================================================
void CRotButton::Spawn()
{
	const char *pszSound = ButtonSound((int)pev->sounds);
	PRECACHE_SOUND(pszSound);
	pev->noise = ALLOC_STRING(pszSound);

	// set the axis of rotation
	AxisDir(pev);

	// check for clockwise rotation
	if ((int)pev->spawnflags & SF_ROTBUTTON_BACKWARDS)
		pev->movedir = -pev->movedir;

	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;

	SET_MODEL(ENT(pev), STRING(pev->model));

	if (pev->speed == 0)
		pev->speed = BUTTON_DEFAULT_SPEED;

	if (m_flWait == 0)
		m_flWait = BUTTON_DEFAULT_WAIT;

	m_toggle_state = TS_AT_BOTTOM;

	m_vecPosition1 = pev->angles;
	m_vecPosition2 = pev->movedir * m_flMoveDistance + pev->angles;

	m_fRotating = TRUE;
	m_fStayPushed = (m_flWait == -1.0f) ? TRUE : FALSE;

	if ((int)pev->spawnflags & SF_BUTTON_TOUCH_ONLY)
	{
		// touchable button
		SetTouch(&CBaseButton::ButtonTouch);
	}
	else
	{
		SetTouch(&CBaseEntity::SUB_DoNothing);
		SetUse(&CBaseButton::ButtonUse);
	}
}

//=========================================================
// momentary_rot_button - a dial that turns while it is
// used and drives the position of its targets
//=========================================================
class CMomentaryRotButton : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pOther);

	void Off(CBaseEntity *pOther);
	void Return(CBaseEntity *pOther);

	void PlayPressSound();
	void UpdateTarget(float flValue);

	int		m_lastUsed;
	int		m_direction;		// 1 turns towards m_end, -1 towards m_start
	float	m_flDist;			// degrees between m_start and m_end
	float	m_returnSpeed;
	Vector	m_start;
	Vector	m_end;
};

//=========================================================
// PlayPressSound - the press sound when the button starts
// to turn
//=========================================================
void CMomentaryRotButton::PlayPressSound()
{
	if (!m_lastUsed)
	{
		edict_t *pentButton = (pev ? ENT(pev) : NULL);
		EMIT_SOUND(pentButton, CHAN_VOICE, STRING(pev->noise), VOL_NORM, ATTN_NORM);
	}
}

//=========================================================
// KeyValue - the func_button keys are accepted, only
// "distance" and "returnspeed" are used
//=========================================================
void CMomentaryRotButton::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "delay"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flDist = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "returnspeed"))
	{
		m_returnSpeed = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// Spawn
//=========================================================
void CMomentaryRotButton::Spawn()
{
	AxisDir(pev);

	if (pev->speed == 0)
		pev->speed = MOMENTARY_DEFAULT_SPEED;

	if (m_flDist < 0)
	{
		m_start = pev->movedir * m_flDist + pev->angles;
		m_end = pev->angles;

		m_flDist = -m_flDist;
		m_direction = 1;		// this will toggle to -1 on the first use
	}
	else
	{
		m_start = pev->angles;
		m_end = pev->movedir * m_flDist + pev->angles;

		m_direction = -1;		// this will toggle to +1 on the first use
	}

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model));

	const char *pszSound = ButtonSound((int)pev->sounds);
	PRECACHE_SOUND(pszSound);
	pev->noise = ALLOC_STRING(pszSound);

	m_lastUsed = 0;
}

//=========================================================
// UpdateTarget - uses the targets, passing a pointer to the
// position of the button (0 to 1) instead of an entity
//=========================================================
void CMomentaryRotButton::UpdateTarget(float flValue)
{
	if (FStringNull(pev->target))
		return;

	EOFFSET eoffsetSelf = gpGlobals->self;
	EOFFSET eoffsetOther = gpGlobals->other;

	const char *pszTarget = STRING(pev->target);
	edict_t *pentTarget = NULL;

	while ((pentTarget = FIND_ENTITY_BY_STRING(pentTarget, "targetname", pszTarget)) != NULL)
	{
		// the search ends at the world
		if (FNullEnt(pentTarget))
			break;

		gpGlobals->self = OFFSET(pentTarget);
		gpGlobals->other = eoffsetSelf;

		CBaseEntity *pTarget = GetClassPtr((CBaseEntity *)VARS(pentTarget));
		pTarget->Use((CBaseEntity *)&flValue);

		gpGlobals->self = eoffsetSelf;
		gpGlobals->other = eoffsetOther;
	}
}

//=========================================================
// Use - turns the button one step further, and back the
// other way once it was released
//=========================================================
void CMomentaryRotButton::Use(CBaseEntity *pOther)
{
	if (!m_lastUsed)
		m_direction = -m_direction;

	pev->nextthink = pev->ltime + 0.1;

	float flValue = AxisDelta((int)pev->spawnflags, pev->angles, m_start) / m_flDist;

	if (m_direction > 0 && flValue >= 1.0f)
	{
		pev->avelocity = g_vecZero;
		pev->angles = m_end;
		m_lastUsed = 1;
		return;
	}

	if (m_direction < 0 && flValue <= 0.0f)
	{
		pev->avelocity = g_vecZero;
		pev->angles = m_start;
		m_lastUsed = 1;
		return;
	}

	PlayPressSound();
	m_lastUsed = 1;
	UpdateTarget(flValue);

	pev->avelocity = pev->movedir * ((float)m_direction * pev->speed);

	SetThink(&CMomentaryRotButton::Off);
}

//=========================================================
// Off - the button was released, stop turning
//=========================================================
void CMomentaryRotButton::Off(CBaseEntity *pOther)
{
	pev->avelocity = g_vecZero;

	m_lastUsed = 0;

	if (((int)pev->spawnflags & SF_ROTBUTTON_AUTO_RETURN) && m_returnSpeed > 0)
	{
		SetThink(&CMomentaryRotButton::Return);
		pev->nextthink = pev->ltime + 0.1;
		m_direction = -1;
	}
	else
	{
		SetThink(NULL);
	}
}

//=========================================================
// Return - turns the button back to the start
//=========================================================
void CMomentaryRotButton::Return(CBaseEntity *pOther)
{
	float flDelta = AxisDelta((int)pev->spawnflags, pev->angles, m_start);

	if (flDelta <= 0)
	{
		pev->avelocity = g_vecZero;
		pev->angles = m_start;

		pev->nextthink = -1.0f;
		SetThink(NULL);
	}
	else
	{
		UpdateTarget(flDelta / m_flDist);

		pev->avelocity = -(pev->movedir * m_returnSpeed);
		pev->nextthink = pev->ltime + 0.1;
	}
}

LINK_ENTITY_TO_CLASS(func_button, CBaseButton);
LINK_ENTITY_TO_CLASS(func_rot_button, CRotButton);
LINK_ENTITY_TO_CLASS(momentary_rot_button, CMomentaryRotButton);
LINK_ENTITY_TO_CLASS(multisource, CMultiSource);
