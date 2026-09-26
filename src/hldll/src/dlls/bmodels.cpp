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
// BModels - brush model entities: func_wall,
// func_illusionary, func_rotating and func_pendulum
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

// func_rotating spawnflags
#define SF_BRUSH_ROTATE_START_ON	1		// starts spinning when the level starts
#define SF_BRUSH_ROTATE_BACKWARDS	2
#define SF_BRUSH_ROTATE_Z_AXIS		4
#define SF_BRUSH_ROTATE_X_AXIS		8
#define SF_BRUSH_ACCDCC				16		// speeds up and slows down instead of starting and stopping at once
#define SF_BRUSH_HURT				32		// hurts whatever touches it
#define SF_BRUSH_ROTATE_NOT_SOLID	64

// func_pendulum spawnflags
#define SF_PENDULUM_START_ON		1		// starts swinging when the level starts
#define SF_PENDULUM_SWING			2		// a player that touches it grabs on
#define SF_PENDULUM_AUTO_RETURN		16		// using a swinging pendulum sends it back to its start
#define SF_PENDULUM_Z_AXIS			64
#define SF_PENDULUM_X_AXIS			128

#define FAN_SOUND_SETS				5		// pev->sounds 1..5 picks one of the fan sound sets
#define FAN_DEFAULT_DMG				2.0f
#define FAN_DMG_SCALE				0.1f	// touch damage per unit of angular speed
#define FAN_START_DELAY				0.1f
#define FAN_SPIN_INTERVAL			0.1		// think interval while speeding up or slowing down
#define FAN_NO_THINK				99999.0f	// puts the next think out of reach

#define PENDULUM_DEFAULT_SPEED		100.0f
#define PENDULUM_DAMP_SCALE			0.001f	// "damp" is given in thousandths
#define PENDULUM_MIN_SPEED			30.0f	// a damped swing slower than this stops
#define PENDULUM_DMG_SCALE			0.01f	// touch damage per unit of speed and pev->dmg
#define PENDULUM_START_DELAY		0.1f
#define PENDULUM_SWING_INTERVAL		0.1

// fan sounds: spin up, spin down and running, one per pev->sounds value
static const char *const pFanStartSounds[FAN_SOUND_SETS] =
{
	"fans/fan1on.wav",
	"fans/fan2on.wav",
	"fans/fan3on.wav",
	"fans/fan4on.wav",
	"fans/fan5on.wav",
};

static const char *const pFanStopSounds[FAN_SOUND_SETS] =
{
	"fans/fan1off.wav",
	"fans/fan2off.wav",
	"fans/fan3off.wav",
	"fans/fan4off.wav",
	"fans/fan5off.wav",
};

static const char *const pFanRunSounds[FAN_SOUND_SETS] =
{
	"fans/fan1.wav",
	"fans/fan2.wav",
	"fans/fan3.wav",
	"fans/fan4.wav",
	"fans/fan5.wav",
};

//=========================================================
// AxisDir - sets pev->movedir to the rotation axis picked
// by the spawnflags: z, x or the default y
//=========================================================
static void AxisDir(entvars_t *pev, int iZAxisFlag, int iXAxisFlag)
{
	int iSpawnFlags = (int)pev->spawnflags;

	if (iSpawnFlags & iZAxisFlag)
		pev->movedir = Vector(0.0f, 0.0f, 1.0f);
	else if (iSpawnFlags & iXAxisFlag)
		pev->movedir = Vector(1.0f, 0.0f, 0.0f);
	else
		pev->movedir = Vector(0.0f, 1.0f, 0.0f);
}

//=========================================================
// AxisDelta - the component of (a - b) along a pendulum's
// rotation axis
//=========================================================
static float AxisDelta(int iSpawnFlags, const Vector &a, const Vector &b)
{
	if (iSpawnFlags & SF_PENDULUM_Z_AXIS)
		return a.z - b.z;
	if (iSpawnFlags & SF_PENDULUM_X_AXIS)
		return a.x - b.x;
	return a.y - b.y;
}

//=========================================================
// VecBModelOrigin - the center of a brush model's bounds
//=========================================================
static Vector VecBModelOrigin(entvars_t *pevBModel)
{
	return pevBModel->size * 0.5f + pevBModel->absmin;
}

//=========================================================
// func_wall - a solid brush that toggles its texture frame
// when used
//=========================================================
class CFuncWall : public CBaseEntity
{
public:
	void Spawn();
	void Use(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(func_wall, CFuncWall);

void CFuncWall::Spawn()
{
	pev->angles = g_vecZero;
	pev->movetype = MOVETYPE_PUSH;	// so it doesn't get pushed by anything
	pev->solid = SOLID_BSP;
	SET_MODEL(ENT(pev), STRING(pev->model));
}

void CFuncWall::Use(CBaseEntity *pOther)
{
	pev->frame = 1.0f - pev->frame;
}

//=========================================================
// func_illusionary - a brush that is drawn but not solid
//=========================================================
class CFuncIllusionary : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
};

LINK_ENTITY_TO_CLASS(func_illusionary, CFuncIllusionary);

void CFuncIllusionary::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "skin"))	// skin is used for content type
	{
		pev->skin = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

void CFuncIllusionary::Spawn()
{
	pev->angles = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SET_MODEL(ENT(pev), STRING(pev->model));
	MAKE_STATIC(ENT(pev));
}

//=========================================================
// func_rotating - a brush that spins around one axis
//=========================================================
class CFuncRotating : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pOther);
	void Touch(CBaseEntity *pOther);
	void Blocked(CBaseEntity *pOther);

	void SpinUp(CBaseEntity *pOther);
	void SpinDown(CBaseEntity *pOther);
	void SpinUpFromUse(CBaseEntity *pOther);
	void HurtTouch(CBaseEntity *pOther);

	unsigned char	m_bFriction;	// number of thinks it takes to speed up or slow down
};

LINK_ENTITY_TO_CLASS(func_rotating, CFuncRotating);

void CFuncRotating::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "friction"))
	{
		m_bFriction = (unsigned char)(int)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

void CFuncRotating::Spawn()
{
	// pick the fan sounds
	int iSounds = (int)pev->sounds;
	if (iSounds >= 1 && iSounds <= FAN_SOUND_SETS)
	{
		int i = iSounds - 1;
		PRECACHE_SOUND(pFanStartSounds[i]);
		PRECACHE_SOUND(pFanStopSounds[i]);
		PRECACHE_SOUND(pFanRunSounds[i]);
		pev->noise1 = ALLOC_STRING(pFanStartSounds[i]);
		pev->noise2 = ALLOC_STRING(pFanStopSounds[i]);
		pev->noise3 = ALLOC_STRING(pFanRunSounds[i]);
	}
	else
	{
		pev->noise1 = ALLOC_STRING("common/null.wav");
		pev->noise2 = ALLOC_STRING("common/null.wav");
		pev->noise3 = ALLOC_STRING("common/null.wav");
	}

	if (m_bFriction == 0)
		m_bFriction = 1;

	AxisDir(pev, SF_BRUSH_ROTATE_Z_AXIS, SF_BRUSH_ROTATE_X_AXIS);

	int iSpawnFlags = (int)pev->spawnflags;
	if (iSpawnFlags & SF_BRUSH_ROTATE_BACKWARDS)
		pev->movedir = -pev->movedir;

	if (iSpawnFlags & SF_BRUSH_ROTATE_NOT_SOLID)
	{
		pev->solid = SOLID_NOT;
		pev->skin = CONTENTS_EMPTY;
	}
	else
	{
		pev->solid = SOLID_BSP;
	}

	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model));

	if (pev->speed <= 0.0f)
		pev->speed = 0.0f;

	if (pev->dmg == 0.0f)
		pev->dmg = FAN_DEFAULT_DMG;

	if (iSpawnFlags & SF_BRUSH_ROTATE_START_ON)
	{
		SetThink(&CFuncRotating::SpinUpFromUse);
		pev->nextthink = gpGlobals->time + FAN_START_DELAY;
	}

	if (iSpawnFlags & SF_BRUSH_HURT)
		SetTouch(&CFuncRotating::HurtTouch);
}

//=========================================================
// SpinUpFromUse - first think of a func_rotating that
// starts on
//=========================================================
void CFuncRotating::SpinUpFromUse(CBaseEntity *pOther)
{
	Use(this);
}

void CFuncRotating::HurtTouch(CBaseEntity *pOther)
{
	Touch(pOther);
}

//=========================================================
// Use - toggles the spinning, at once or through SpinUp
// and SpinDown when it accelerates
//=========================================================
void CFuncRotating::Use(CBaseEntity *pOther)
{
	int iSpawnFlags = (int)pev->spawnflags;
	BOOL fSpinning = (pev->avelocity != g_vecZero);

	if (iSpawnFlags & SF_BRUSH_ACCDCC)
	{
		if (fSpinning)
		{
			SetThink(&CFuncRotating::SpinDown);
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING(pev->noise2), VOL_NORM, ATTN_NORM);
		}
		else
		{
			SetThink(&CFuncRotating::SpinUp);
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING(pev->noise1), VOL_NORM, ATTN_NORM);
		}

		pev->nextthink = pev->ltime + FAN_SPIN_INTERVAL;
		return;
	}

	if (fSpinning)
	{
		pev->avelocity = g_vecZero;
		return;
	}

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING(pev->noise3), VOL_NORM, ATTN_NORM);

	pev->avelocity = pev->movedir * pev->speed;
	pev->nextthink = gpGlobals->time + FAN_NO_THINK;
}

//=========================================================
// SpinUp - speeds up by one step until full speed
//=========================================================
void CFuncRotating::SpinUp(CBaseEntity *pOther)
{
	pev->nextthink = pev->ltime + FAN_SPIN_INTERVAL;

	Vector &vecAVelocity = pev->avelocity;
	Vector &vecMoveDir = pev->movedir;
	float flStep = pev->speed / (float)m_bFriction;

	vecAVelocity += vecMoveDir * flStep;

	// reached full speed?
	float flSpeed = pev->speed;
	if (vecMoveDir.x * flSpeed <= vecAVelocity.x
		&& vecMoveDir.y * flSpeed <= vecAVelocity.y
		&& vecMoveDir.z * flSpeed <= vecAVelocity.z)
	{
		vecAVelocity = vecMoveDir * flSpeed;
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, STRING(pev->noise3), VOL_NORM, ATTN_NORM);

		pev->nextthink = pev->ltime + FAN_NO_THINK;
	}
}

//=========================================================
// SpinDown - slows down by one step until stopped
//=========================================================
void CFuncRotating::SpinDown(CBaseEntity *pOther)
{
	pev->nextthink = pev->ltime + FAN_SPIN_INTERVAL;

	Vector &vecAVelocity = pev->avelocity;
	Vector &vecMoveDir = pev->movedir;
	float flStep = pev->speed / (float)m_bFriction;

	vecAVelocity -= vecMoveDir * flStep;

	// stopped?
	if (vecAVelocity.x <= 0.0f && vecAVelocity.y <= 0.0f && vecAVelocity.z <= 0.0f)
	{
		vecAVelocity = g_vecZero;
		pev->nextthink = pev->ltime + FAN_NO_THINK;
	}
}

//=========================================================
// Touch - hurts whatever can take damage, by how fast we
// spin, and throws it away from the center
//=========================================================
void CFuncRotating::Touch(CBaseEntity *pOther)
{
	EOFFSET eoffsetOther = gpGlobals->other;
	if (FNullEnt(eoffsetOther))
		return;

	edict_t *pentOther = ENT(eoffsetOther);
	entvars_t *pevOther = VARS(pentOther);

	if (pevOther->takedamage == DAMAGE_NO)
		return;

	pev->dmg = pev->avelocity.Length() * FAN_DMG_SCALE;

	CBaseEntity *pHit = CBaseEntity::Instance(pentOther);
	if (pHit)
		pHit->TakeDamage(pev, pev, pev->dmg);

	pevOther->velocity = (pevOther->origin - VecBModelOrigin(pev)).Normalize() * pev->dmg;
}

//=========================================================
// Blocked - hurts the blocker
//=========================================================
void CFuncRotating::Blocked(CBaseEntity *pOther)
{
	EOFFSET eoffsetOther = gpGlobals->other;
	if (FNullEnt(eoffsetOther))
		return;

	CBaseEntity *pHit = CBaseEntity::Instance(eoffsetOther);
	if (pHit)
		pHit->TakeDamage(pev, pev, pev->dmg);
}

//=========================================================
// func_pendulum - a brush that swings back and forth
// around one axis
//=========================================================
class CFuncPendulum : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pOther);
	void Touch(CBaseEntity *pOther);

	void Swing(CBaseEntity *pOther);
	void Stop(CBaseEntity *pOther);
	void StartFromUse(CBaseEntity *pOther);
	void RopeTouch(CBaseEntity *pOther);

	float	m_accel;		// acceleration towards the center
	float	m_distance;		// size of the swing, in degrees
	float	m_time;			// pev->ltime of the last swing think
	float	m_damp;			// how fast the swing dies out
	float	m_maxSpeed;		// pev->speed at spawn
	float	m_dampSpeed;	// the damped speed limit
	Vector	m_center;		// angles at the middle of the swing
	Vector	m_start;		// angles at spawn
};

LINK_ENTITY_TO_CLASS(func_pendulum, CFuncPendulum);

void CFuncPendulum::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_distance = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damp"))
	{
		m_damp = (float)atof(pkvd->szValue) * PENDULUM_DAMP_SCALE;
		pkvd->fHandled = TRUE;
	}
}

void CFuncPendulum::Spawn()
{
	AxisDir(pev, SF_PENDULUM_Z_AXIS, SF_PENDULUM_X_AXIS);

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model));

	if (m_distance == 0.0f)
		return;

	if (pev->speed == 0.0f)
		pev->speed = PENDULUM_DEFAULT_SPEED;

	float flSpeed = pev->speed;
	m_accel = (flSpeed * flSpeed) / (m_distance * 2.0f);
	m_maxSpeed = flSpeed;

	// swing around the middle of the arc
	float flHalfArc = m_distance * 0.5f;
	m_start = pev->angles;
	m_center = pev->movedir * flHalfArc + pev->angles;

	int iSpawnFlags = (int)pev->spawnflags;
	if (iSpawnFlags & SF_PENDULUM_START_ON)
	{
		SetThink(&CFuncPendulum::StartFromUse);
		pev->nextthink = gpGlobals->time + PENDULUM_START_DELAY;
	}

	pev->speed = 0.0f;

	if (iSpawnFlags & SF_PENDULUM_SWING)
		SetTouch(&CFuncPendulum::RopeTouch);
}

//=========================================================
// StartFromUse - first think of a func_pendulum that
// starts on
//=========================================================
void CFuncPendulum::StartFromUse(CBaseEntity *pOther)
{
	Use(this);
}

//=========================================================
// Use - stops a swinging pendulum, or sends it back to its
// start, or starts a new swing
//=========================================================
void CFuncPendulum::Use(CBaseEntity *pOther)
{
	int iSpawnFlags = (int)pev->spawnflags;

	if (pev->speed != 0.0f)
	{
		if (iSpawnFlags & SF_PENDULUM_AUTO_RETURN)
		{
			float flDelta = AxisDelta(iSpawnFlags, pev->angles, m_start);

			pev->avelocity = pev->movedir * m_maxSpeed;
			pev->nextthink = flDelta / m_maxSpeed + pev->ltime;
			SetThink(&CFuncPendulum::Stop);
		}
		else
		{
			// dead stop
			pev->speed = 0.0f;
			SetThink(NULL);
			pev->avelocity = g_vecZero;
		}
		return;
	}

	pev->nextthink = pev->ltime + PENDULUM_SWING_INTERVAL;
	m_time = pev->ltime;
	m_dampSpeed = m_maxSpeed;
	SetThink(&CFuncPendulum::Swing);
}

//=========================================================
// Stop - back at the start angles
//=========================================================
void CFuncPendulum::Stop(CBaseEntity *pOther)
{
	pev->angles = m_start;

	pev->speed = 0.0f;
	SetThink(NULL);

	pev->avelocity = g_vecZero;
}

//=========================================================
// Swing - accelerates towards the center of the swing,
// damped when "damp" is set
//=========================================================
void CFuncPendulum::Swing(CBaseEntity *pOther)
{
	int iSpawnFlags = (int)pev->spawnflags;
	float flDelta = AxisDelta(iSpawnFlags, pev->angles, m_center);

	float flInterval = pev->ltime - m_time;
	m_time = pev->ltime;

	if (flDelta > 0.0f && m_accel > 0.0f)
		pev->speed -= m_accel * flInterval;
	else
		pev->speed += m_accel * flInterval;

	pev->avelocity = pev->movedir * pev->speed;

	pev->nextthink = pev->ltime + PENDULUM_SWING_INTERVAL;

	if (m_damp == 0.0f)
		return;

	m_dampSpeed = (1.0f - m_damp * flInterval) * m_dampSpeed;

	if (m_dampSpeed >= PENDULUM_MIN_SPEED)
	{
		if (m_dampSpeed < pev->speed)
			pev->speed = m_dampSpeed;
		else if (-m_dampSpeed > pev->speed)
			pev->speed = -m_dampSpeed;
	}
	else
	{
		// the swing died out, stop in the middle
		pev->angles = m_center;
		pev->speed = 0.0f;
		SetThink(NULL);
		pev->avelocity = g_vecZero;
	}
}

//=========================================================
// Touch - hurts whatever can take damage, by how fast we
// swing, and throws it away from the center
//=========================================================
void CFuncPendulum::Touch(CBaseEntity *pOther)
{
	EOFFSET eoffsetOther = gpGlobals->other;
	if (FNullEnt(eoffsetOther))
		return;

	edict_t *pentOther = ENT(eoffsetOther);
	entvars_t *pevOther = VARS(pentOther);

	if (pev->dmg <= 0.0f)
		return;

	if (pevOther->takedamage == DAMAGE_NO)
		return;

	float flDamage = pev->speed * pev->dmg * PENDULUM_DMG_SCALE;
	if (flDamage < 0.0f)
		flDamage = -flDamage;

	CBaseEntity *pHit = CBaseEntity::Instance(pentOther);
	if (pHit)
		pHit->TakeDamage(pev, pev, flDamage);

	pevOther->velocity = (pevOther->origin - VecBModelOrigin(pev)).Normalize() * flDamage;
}

//=========================================================
// RopeTouch - a player that touches the pendulum grabs on
//=========================================================
void CFuncPendulum::RopeTouch(CBaseEntity *pOther)
{
	EOFFSET eoffsetOther = gpGlobals->other;
	entvars_t *pevOther = VARS(eoffsetOther);

	if (!((int)pevOther->flags & FL_CLIENT))
	{
		ALERT(at_console, "Not a client\n");
		return;
	}

	// grab the player once
	if (pev->enemy != eoffsetOther)
	{
		pev->enemy = eoffsetOther;
		pevOther->velocity = g_vecZero;
		pevOther->movetype = MOVETYPE_NONE;
	}
}
