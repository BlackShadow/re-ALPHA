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
// BModels - Brush model entities
//
//   func_wall (Spawn, Use)
//   func_illusionary (Spawn, KeyValue)
//   func_rotating (Spawn, KeyValue, Touch, Use, Blocked,
//                  spin-up / spin-down think helpers)
//   func_pendulum (Spawn, KeyValue, Use, Touch,
//                  swing think helpers, rope touch)
//
// func_wall / func_illusionary derive straight from CBaseEntity.
// func_rotating / func_pendulum are compact movers that also derive
// from CBaseEntity (their alpha private-data sizes - 32 and 76 - are
// far below the 132-byte CBaseToggle layout) and carry their own
// fields; they reuse the SetMovedir / move framework conventions of
// the CBaseToggle foundation only where it overlaps.
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// pev field byte offsets used by the brush models that are
// not yet named PEV_* constants in utils.h.
//
//   4   - pev->absmin (3 floats; brush world-space bounds min)
//   204 - pev->size   (3 floats; absmax - absmin)
//   496 - pev->speed  (rotation / swing speed; shares the byte
//         offset that utils.h also calls PEV_DMGTIME)
//=========================================================
enum
{
	PEV_ABSMIN	= 4,
	PEV_BRUSH_SIZE	= 204,
	PEV_SPEED	= 496,
};

//=========================================================
// movetype / solid values written by the brush spawns.
//=========================================================
enum
{
	MOVETYPE_NONE	= 0,
	MOVETYPE_PUSH	= 7,
	SOLID_NOT	= 0,
	SOLID_BSP	= 4,
};

//=========================================================
// func_rotating / func_pendulum spawnflags.
//=========================================================
enum
{
	SF_BRUSH_ROTATE_START_ON	= 1,	// begins spinning at level start
	SF_BRUSH_ROTATE_BACKWARDS	= 2,	// reverse the rotation direction
	SF_BRUSH_ROTATE_Z_AXIS		= 4,	// spin about the Z axis
	SF_BRUSH_ROTATE_X_AXIS		= 8,	// spin about the X axis
	SF_BRUSH_ACCDCC			= 0x10,	// accelerate / decelerate smoothly
	SF_BRUSH_HURT			= 0x20,	// hurt entities that touch the brush
	SF_BRUSH_ROTATE_NOT_SOLID	= 0x40,	// non-solid (decorative) spin

	SF_PENDULUM_START_ON		= 1,	// swinging at level start
	SF_PENDULUM_SWING		= 2,	// touchable "rope" mode
	SF_PENDULUM_AUTO_RETURN		= 0x10,	// Use re-launches the swing

	// func_pendulum is a door-family rotator: it selects its swing axis
	// with the door rotation flags (Z=0x40, X=0x80), NOT the func_rotating
	// brush flags (Z=4, X=8).  The alpha proves this - func_rotating tests
	// bits 4/8 inline in its Spawn, while func_pendulum
	// routes through the shared axis helpers, both of which test
	// bits 0x40/0x80.
	SF_PENDULUM_Z_AXIS		= 0x40,	// swing about the Z axis
	SF_PENDULUM_X_AXIS		= 0x80,	// swing about the X axis
};

//=========================================================
// CHAN_STATIC and the looping-sound mix used by the fan
// emit calls (volume 1.0, attenuation 0.8).
//=========================================================
enum
{
	BRUSH_SND_CHANNEL	= 1,	// CHAN_STATIC
};
#define BRUSH_SND_VOLUME	1.0f
#define BRUSH_SND_ATTEN		0.8f

static entvars_t* CurrentOtherEntvars(entvars_t* pevSelf, edict_t** ppOtherEdict)
{
	if (ppOtherEdict)
		*ppOtherEdict = NULL;

	void* globals = GlobalsFromEntvars(pevSelf);
	if (!globals)
		return NULL;

	int otherIndex = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);
	edict_t* pOtherEdict = otherIndex ? EnginePEntityOfEntIndex(otherIndex) : NULL;
	if (ppOtherEdict)
		*ppOtherEdict = pOtherEdict;

	return pOtherEdict ? EngineGetVarsOfEnt(pOtherEdict) : NULL;
}

//=========================================================
// Fan sound sets selected by pev->message / "spawnobject"
// (stored as pev->skin during precache).  Index 1..5 map to
// the fans/fanN family; anything else falls back to silence.
//=========================================================
static const char* const kFanSoundOn[] =
{
	"fans/fan1on.wav",
	"fans/fan2on.wav",
	"fans/fan3on.wav",
	"fans/fan4on.wav",
	"fans/fan5on.wav",
};

static const char* const kFanSoundOff[] =
{
	"fans/fan1off.wav",
	"fans/fan2off.wav",
	"fans/fan3off.wav",
	"fans/fan4off.wav",
	"fans/fan5off.wav",
};

static const char* const kFanSoundLoop[] =
{
	"fans/fan1.wav",
	"fans/fan2.wav",
	"fans/fan3.wav",
	"fans/fan4.wav",
	"fans/fan5.wav",
};

static const char kNullSound[] = "common/null.wav";

//=========================================================
// BrushCenter
//
// World-space centre of a brush: absmin + size * 0.5, which is
// the midpoint of (absmin, absmax).
//=========================================================
static void BrushCenter(float* out, entvars_t* pevBrush)
{
	const float* absmin = VecPtr(PevVector(pevBrush, PEV_ABSMIN));
	const float* size = VecPtr(PevVector(pevBrush, PEV_BRUSH_SIZE));

	out[0] = size[0] * 0.5f + absmin[0];
	out[1] = size[1] * 0.5f + absmin[1];
	out[2] = size[2] * 0.5f + absmin[2];
}

//=========================================================
// AxisDir
//
// Derive pev->movedir from the rotation-axis spawnflags
// (Z / X / default Y).  The caller passes the Z and X bit masks
// because func_rotating and func_pendulum use different flag bits
// (4/8 vs 0x40/0x80) for the same axis selection.
//=========================================================
static void AxisDir(entvars_t* pev, int zAxisBit, int xAxisBit)
{
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	int spawnflags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	if ((spawnflags & zAxisBit) != 0)
		VecSet(VecPtr(movedir), 0.0f, 0.0f, 1.0f);
	else if ((spawnflags & xAxisBit) != 0)
		VecSet(VecPtr(movedir), 1.0f, 0.0f, 0.0f);
	else
		VecSet(VecPtr(movedir), 0.0f, 1.0f, 0.0f);	// default: Y axis
}

//=========================================================
// AxisDelta
//
// Component of (a - b) along the active rotation axis selected
// by the spawnflags.  Used to measure remaining swing distance.
// Only func_pendulum calls this, so it tests the door rotation
// flags (Z=0x40, X=0x80) as the binary helper does.
//=========================================================
static float AxisDelta(int spawnflags, const float* a, const float* b)
{
	if ((spawnflags & SF_PENDULUM_Z_AXIS) != 0)
		return a[2] - b[2];
	if ((spawnflags & SF_PENDULUM_X_AXIS) != 0)
		return a[0] - b[0];
	return a[1] - b[1];
}

//=========================================================
// CFuncWall
//
// A solid brush whose only behaviour is to flip its texture
// frame each time it is used.
//=========================================================
class CFuncWall : public CBaseEntity
{
public:
	void Spawn();
	void Use(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CFuncWall) <= 28, CFuncWall_size);

//=========================================================
// CFuncWall::Spawn
//=========================================================
void CFuncWall::Spawn()
{
	VecSet(VecPtr(PevVector(pev, PEV_ANGLES)), 0.0f, 0.0f, 0.0f);
	PevFloat(pev, PEV_MOVETYPE) = (float)MOVETYPE_PUSH;	// not pushed by anything
	PevFloat(pev, PEV_SOLID) = (float)SOLID_BSP;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));
}

//=========================================================
// CFuncWall::Use
//
// Toggle pev->frame between 0 and 1 to swap the brush texture.
//=========================================================
void CFuncWall::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	PevFloat(pev, PEV_FRAME) = 1.0f - PevFloat(pev, PEV_FRAME);
}

//=========================================================
// CFuncIllusionary
//
// A non-solid decorative brush.  It is rendered but never
// blocks movement, and the engine bakes it into a static.
//=========================================================
class CFuncIllusionary : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
};

HL_COMPILE_TIME_ASSERT(sizeof(CFuncIllusionary) <= 132, CFuncIllusionary_size);

//=========================================================
// CFuncIllusionary::KeyValue
//
// Accepts "skin" (the contents value, stored in pev->skin).
//=========================================================
void CFuncIllusionary::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "skin") == 0)
	{
		PevFloat(pev, PEV_SKIN) = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CFuncIllusionary::Spawn
//=========================================================
void CFuncIllusionary::Spawn()
{
	VecSet(VecPtr(PevVector(pev, PEV_ANGLES)), 0.0f, 0.0f, 0.0f);
	PevFloat(pev, PEV_MOVETYPE) = (float)MOVETYPE_NONE;
	PevFloat(pev, PEV_SOLID) = (float)SOLID_NOT;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
	{
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));
		EngineMakeStatic(edict);
	}
}

//=========================================================
// CFuncRotating
//
// A brush that spins continuously about one axis.  It can hurt
// touchers, accelerate / decelerate when toggled, and play a
// fan sound set.
//
//   [28] m_pitch - acceleration divisor / step count (byte)
//=========================================================
class CFuncRotating : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pOther);
	void Touch(CBaseEntity* pOther);
	void Blocked(CBaseEntity* pOther);

	void SpinUp(CBaseEntity* pOther);
	void SpinDown(CBaseEntity* pOther);
	void SpinUpFromUse(CBaseEntity* pOther);		// start-on think -> own Use
	void HurtTouch(CBaseEntity* pOther);	// touch -> own Touch

private:
	unsigned char m_pitch;	// [28]
};

HL_COMPILE_TIME_ASSERT(sizeof(CFuncRotating) <= 32, CFuncRotating_size);

//=========================================================
// CFuncRotating::KeyValue
//
// "friction" (0..100) drives the acceleration step count.
//=========================================================
void CFuncRotating::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "friction") == 0)
	{
		m_pitch = (unsigned char)(int)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CFuncRotating::Spawn
//=========================================================
void CFuncRotating::Spawn()
{
	//
	// Pick the fan sound set from the numeric sound id the editor
	// stored at pev offset 476 (the "sounds" selector, 1..5).  The
	// switch precaches the matching trio and records the resulting
	// string indices back into the noise fields.
	//
	int soundId = (int)PevFloat(pev, 476);	// pev sound-set selector
	if (soundId >= 1 && soundId <= 5)
	{
		int idx = soundId - 1;
		EnginePrecacheSound(kFanSoundOn[idx]);
		EnginePrecacheSound(kFanSoundOff[idx]);
		EnginePrecacheSound(kFanSoundLoop[idx]);
		PevInt(pev, PEV_NOISE_MOVING) = EngineAllocString(kFanSoundOn[idx]);
		PevInt(pev, PEV_NOISE_ARRIVED) = EngineAllocString(kFanSoundOff[idx]);
		PevInt(pev, 492) = EngineAllocString(kFanSoundLoop[idx]);	// running loop sound
	}
	else
	{
		PevInt(pev, PEV_NOISE_MOVING) = EngineAllocString(kNullSound);
		PevInt(pev, PEV_NOISE_ARRIVED) = EngineAllocString(kNullSound);
		PevInt(pev, 492) = EngineAllocString(kNullSound);	// running loop sound
	}

	if (m_pitch == 0)
		m_pitch = 1;

	// Set the spin axis, then optionally reverse it.  func_rotating
	// selects its axis with the brush flags (Z=4, X=8).
	AxisDir(pev, SF_BRUSH_ROTATE_Z_AXIS, SF_BRUSH_ROTATE_X_AXIS);

	int spawnflags = (int)PevFloat(pev, PEV_SPAWNFLAGS);
	if ((spawnflags & SF_BRUSH_ROTATE_BACKWARDS) != 0)
	{
		Vector& movedir = PevVector(pev, PEV_MOVEDIR);
		VecSet(VecPtr(movedir), -movedir.x, -movedir.y, -movedir.z);
	}

	if ((spawnflags & SF_BRUSH_ROTATE_NOT_SOLID) != 0)
	{
		PevFloat(pev, PEV_SOLID) = (float)SOLID_NOT;
		PevFloat(pev, PEV_SKIN) = -1.0f;
	}
	else
	{
		PevFloat(pev, PEV_SOLID) = (float)SOLID_BSP;
	}

	PevFloat(pev, PEV_MOVETYPE) = (float)MOVETYPE_PUSH;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
	{
		EngineSetOrigin(edict, VecPtr(PevVector(pev, PEV_ORIGIN)));
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));
	}

	if (PevFloat(pev, PEV_SPEED) <= 0.0f)
		PevFloat(pev, PEV_SPEED) = 0.0f;

	if ((PevInt(pev, PEV_DMG) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_DMG) = 2.0f;

	if ((spawnflags & SF_BRUSH_ROTATE_START_ON) != 0)
	{
		// Fire our own Use one tick from now to start spinning.
		SetThink(&CFuncRotating::SpinUpFromUse);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.1f;
	}

	if ((spawnflags & SF_BRUSH_HURT) != 0)
		SetTouch(&CFuncRotating::HurtTouch);
}

//=========================================================
// CFuncRotating::SpinUpFromUse (thunk target)
//
// The "start on" think simply dispatches the entity's own Use.
//=========================================================
void CFuncRotating::SpinUpFromUse(CBaseEntity* pOther)
{
	Use(this);
}

//=========================================================
// CFuncRotating::HurtTouch (thunk target)
//
// The "hurt" touch routes straight to the Touch override.
//=========================================================
void CFuncRotating::HurtTouch(CBaseEntity* pOther)
{
	Touch(pOther);
}

//=========================================================
// CFuncRotating::Use
//
// Toggle the spin.  In accelerate mode the speed ramps via the
// SpinUp / SpinDown thinks; otherwise it snaps on / off.
//=========================================================
void CFuncRotating::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	Vector& avelocity = PevVector(pev, PEV_AVELOCITY);
	int spawnflags = (int)PevFloat(pev, PEV_SPAWNFLAGS);
	int spinning = (avelocity.x != 0.0f || avelocity.y != 0.0f || avelocity.z != 0.0f);

	if ((spawnflags & SF_BRUSH_ACCDCC) != 0)
	{
		edict_t* edict = EdictFromEntvars(pev);

		if (spinning)
		{
			SetThink(&CFuncRotating::SpinDown);
			if (edict)
				EngineEmitSound(edict, BRUSH_SND_CHANNEL,
					EngineStringFromIndex(PevInt(pev, PEV_NOISE_ARRIVED)),
					BRUSH_SND_VOLUME, BRUSH_SND_ATTEN);
		}
		else
		{
			SetThink(&CFuncRotating::SpinUp);
			if (edict)
				EngineEmitSound(edict, BRUSH_SND_CHANNEL,
					EngineStringFromIndex(PevInt(pev, PEV_NOISE_MOVING)),
					BRUSH_SND_VOLUME, BRUSH_SND_ATTEN);
		}

		PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);
		return;
	}

	if (spinning)
	{
		// stop instantly
		VecSet(VecPtr(avelocity), 0.0f, 0.0f, 0.0f);
		return;
	}

	// start instantly at full speed and play the looping fan sound
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, BRUSH_SND_CHANNEL,
			EngineStringFromIndex(PevInt(pev, 492)),	// running loop sound
			BRUSH_SND_VOLUME, BRUSH_SND_ATTEN);

	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	float speed = PevFloat(pev, PEV_SPEED);
	VecSet(VecPtr(avelocity), movedir.x * speed, movedir.y * speed, movedir.z * speed);

	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 99999.0f;
}

//=========================================================
// CFuncRotating::SpinUp
//
// Accelerate toward the target speed one m_pitch-sized step at
// a time; once at speed, switch to the steady loop sound.
//=========================================================
void CFuncRotating::SpinUp(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);

	Vector& avelocity = PevVector(pev, PEV_AVELOCITY);
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	float step = PevFloat(pev, PEV_SPEED) / (float)m_pitch;

	avelocity.x += movedir.x * step;
	avelocity.y += movedir.y * step;
	avelocity.z += movedir.z * step;

	// reached (or passed) the requested speed on every axis?
	float speed = PevFloat(pev, PEV_SPEED);
	if (movedir.x * speed <= avelocity.x
		&& movedir.y * speed <= avelocity.y
		&& movedir.z * speed <= avelocity.z)
	{
		VecSet(VecPtr(avelocity), movedir.x * speed, movedir.y * speed, movedir.z * speed);

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, BRUSH_SND_CHANNEL,
				EngineStringFromIndex(PevInt(pev, 492)),	// running loop sound
				BRUSH_SND_VOLUME, BRUSH_SND_ATTEN);

		PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + 99999.0f;
	}
}

//=========================================================
// CFuncRotating::SpinDown
//
// Decelerate one step at a time; once stopped, snap angular
// velocity to zero and park the think.
//=========================================================
void CFuncRotating::SpinDown(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);

	Vector& avelocity = PevVector(pev, PEV_AVELOCITY);
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	float step = PevFloat(pev, PEV_SPEED) / (float)m_pitch;

	avelocity.x -= movedir.x * step;
	avelocity.y -= movedir.y * step;
	avelocity.z -= movedir.z * step;

	// the binary tests the raw sign bits of each component as ints
	if (*(int*)&avelocity.x <= 0 && *(int*)&avelocity.y <= 0 && *(int*)&avelocity.z <= 0)
	{
		VecSet(VecPtr(avelocity), 0.0f, 0.0f, 0.0f);
		PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME) + 99999.0f;
	}
}

//=========================================================
// CFuncRotating::Touch
//
// HurtTouch: damage anything that can take damage and shove it
// away from the brush centre.
//=========================================================
void CFuncRotating::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	edict_t* pOtherEdict;
	entvars_t* pevOther = CurrentOtherEntvars(pev, &pOtherEdict);
	if (!pevOther)
		return;

	if ((PevInt(pevOther, PEV_TAKEDAMAGE) & 0x7FFFFFFF) == 0)
		return;

	// damage scales with how fast we are spinning
	Vector& avelocity = PevVector(pev, PEV_AVELOCITY);
	PevFloat(pev, PEV_DMG) =
		sqrtf(avelocity.x * avelocity.x + avelocity.y * avelocity.y + avelocity.z * avelocity.z) * 0.1f;

	CBaseEntity* pHit = (CBaseEntity*)EngineGetPrivateData(pOtherEdict);
	if (pHit)
		pHit->TakeDamage(pev, pev, PevFloat(pev, PEV_DMG));

	// push the toucher straight out from our centre at dmg units/sec
	float center[3];
	BrushCenter(center, pev);

	Vector& otherOrigin = PevVector(pevOther, PEV_ORIGIN);
	float dir[3];
	dir[0] = otherOrigin.x - center[0];
	dir[1] = otherOrigin.y - center[1];
	dir[2] = otherOrigin.z - center[2];

	if (VecNormalize(dir) == 0)
		VecSet(dir, 0.0f, 0.0f, 0.0f);

	float dmg = PevFloat(pev, PEV_DMG);
	Vector& otherVel = PevVector(pevOther, PEV_VELOCITY);
	VecSet(VecPtr(otherVel), dir[0] * dmg, dir[1] * dmg, dir[2] * dmg);
}

//=========================================================
// CFuncRotating::Blocked
//
// Whatever we run into takes pev->dmg.
//=========================================================
void CFuncRotating::Blocked(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	edict_t* pOtherEdict;
	entvars_t* pevOther = CurrentOtherEntvars(pev, &pOtherEdict);
	if (!pevOther)
		return;

	CBaseEntity* pHit = (CBaseEntity*)EngineGetPrivateData(pOtherEdict);
	if (pHit)
		pHit->TakeDamage(pev, pev, PevFloat(pev, PEV_DMG));
}

//=========================================================
// CFuncPendulum
//
// A brush that swings about one axis like a pendulum.  Field
// byte offsets in the 76-byte alpha private data:
//   [28] m_accel       [32] m_distance   [36] m_time
//   [40] m_damp        [44] m_maxSpeed   [48] m_dampSpeed
//   [52] m_center      [64] m_start
//=========================================================
class CFuncPendulum : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pOther);
	void Touch(CBaseEntity* pOther);

	void Swing(CBaseEntity* pOther);
	void Stop(CBaseEntity* pOther);
	void StartFromUse(CBaseEntity* pOther);		// start-on think -> own Use
	void RopeTouch(CBaseEntity* pOther);

private:
	float m_accel;		// [28]
	float m_distance;	// [32]
	float m_time;		// [36]
	float m_damp;		// [40]
	float m_maxSpeed;	// [44]
	float m_dampSpeed;	// [48]
	Vector m_center;	// [52]
	Vector m_start;		// [64]
};

HL_COMPILE_TIME_ASSERT(sizeof(CFuncPendulum) <= 76, CFuncPendulum_size);

//=========================================================
// CFuncPendulum::KeyValue
//
// "distance" (swing arc, degrees) and "damp" (damping, scaled
// by 0.001).
//=========================================================
void CFuncPendulum::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "distance") == 0)
	{
		m_distance = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "damp") == 0)
	{
		m_damp = (float)atof(pkvd->szValue) * 0.001f;
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CFuncPendulum::Spawn
//=========================================================
void CFuncPendulum::Spawn()
{
	// func_pendulum is a door-family rotator: its axis comes from the
	// door rotation flags (Z=0x40, X=0x80), matching the binary.
	AxisDir(pev, SF_PENDULUM_Z_AXIS, SF_PENDULUM_X_AXIS);

	PevFloat(pev, PEV_SOLID) = (float)SOLID_BSP;
	PevFloat(pev, PEV_MOVETYPE) = (float)MOVETYPE_PUSH;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
	{
		EngineSetOrigin(edict, VecPtr(PevVector(pev, PEV_ORIGIN)));
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));
	}

	if ((*(int*)&m_distance & 0x7FFFFFFF) == 0)
		return;

	if ((PevInt(pev, PEV_SPEED) & 0x7FFFFFFF) == 0)
		PevFloat(pev, PEV_SPEED) = 100.0f;

	float speed = PevFloat(pev, PEV_SPEED);
	m_accel = (speed * speed) / (m_distance * 2.0f);
	m_maxSpeed = speed;

	// remember the rest angles and the far end of the swing arc
	float halfArc = m_distance * 0.5f;
	VecCopy(VecPtr(m_start), VecPtr(PevVector(pev, PEV_ANGLES)));

	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	Vector& angles = PevVector(pev, PEV_ANGLES);
	VecSet(VecPtr(m_center),
		movedir.x * halfArc + angles.x,
		movedir.y * halfArc + angles.y,
		movedir.z * halfArc + angles.z);

	int spawnflags = (int)PevFloat(pev, PEV_SPAWNFLAGS);
	if ((spawnflags & SF_PENDULUM_START_ON) != 0)
	{
		// kick the swing one tick from now via our own Use
		SetThink(&CFuncPendulum::StartFromUse);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.1f;
	}

	PevFloat(pev, PEV_SPEED) = 0.0f;

	if ((spawnflags & SF_PENDULUM_SWING) != 0)
		SetTouch(&CFuncPendulum::RopeTouch);
}

//=========================================================
// CFuncPendulum::StartFromUse (thunk target)
//=========================================================
void CFuncPendulum::StartFromUse(CBaseEntity* pOther)
{
	Use(this);
}

//=========================================================
// CFuncPendulum::Use
//
// Toggle the swing.  Auto-return launches a fresh swing; the
// manual modes either snap to a stop or begin a damped swing.
//=========================================================
void CFuncPendulum::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	int spawnflags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	if ((PevInt(pev, PEV_SPEED) & 0x7FFFFFFF) != 0)
	{
		// currently swinging
		if ((spawnflags & SF_PENDULUM_AUTO_RETURN) != 0)
		{
			// launch toward the far end at max speed
			float dest = AxisDelta(spawnflags, VecPtr(PevVector(pev, PEV_ANGLES)), VecPtr(m_start));

			Vector& movedir = PevVector(pev, PEV_MOVEDIR);
			Vector& avelocity = PevVector(pev, PEV_AVELOCITY);
			VecSet(VecPtr(avelocity),
				movedir.x * m_maxSpeed,
				movedir.y * m_maxSpeed,
				movedir.z * m_maxSpeed);

			PevFloat(pev, PEV_NEXTTHINK) = dest / m_maxSpeed + PevFloat(pev, PEV_LTIME);
			SetThink(&CFuncPendulum::Stop);
		}
		else
		{
			// stop dead
			PevFloat(pev, PEV_SPEED) = 0.0f;
			m_pfnThink = NULL;
			VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), 0.0f, 0.0f, 0.0f);
		}
		return;
	}

	// start a fresh damped swing
	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);
	m_time = PevFloat(pev, PEV_LTIME);
	m_dampSpeed = m_maxSpeed;
	SetThink(&CFuncPendulum::Swing);
}

//=========================================================
// CFuncPendulum::Stop
//
// Snap to the swing's end angle, clear the swing state and the
// angular velocity.
//=========================================================
void CFuncPendulum::Stop(CBaseEntity* pOther)
{
	VecCopy(VecPtr(PevVector(pev, PEV_ANGLES)), VecPtr(m_start));

	PevFloat(pev, PEV_SPEED) = 0.0f;
	m_pfnThink = NULL;

	VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), 0.0f, 0.0f, 0.0f);
}

//=========================================================
// CFuncPendulum::Swing
//
// Integrate the pendulum each tick: gravity-like acceleration
// toward centre, optional damping, and the stop condition once
// the swing decays below the minimum speed.
//=========================================================
void CFuncPendulum::Swing(CBaseEntity* pOther)
{
	int spawnflags = (int)PevFloat(pev, PEV_SPAWNFLAGS);
	float delta = AxisDelta(spawnflags, VecPtr(PevVector(pev, PEV_ANGLES)), VecPtr(m_center));

	float dt = PevFloat(pev, PEV_LTIME) - m_time;
	m_time = PevFloat(pev, PEV_LTIME);

	// accelerate toward the centre (the sign of delta steers it)
	if (delta > 0.0f && m_accel > 0.0f)
		PevFloat(pev, PEV_SPEED) -= m_accel * dt;
	else
		PevFloat(pev, PEV_SPEED) += m_accel * dt;

	// drive the angular velocity along the active axis
	Vector& movedir = PevVector(pev, PEV_MOVEDIR);
	float speed = PevFloat(pev, PEV_SPEED);
	Vector& avelocity = PevVector(pev, PEV_AVELOCITY);
	VecSet(VecPtr(avelocity), movedir.x * speed, movedir.y * speed, movedir.z * speed);

	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);

	if ((*(int*)&m_damp & 0x7FFFFFFF) == 0)
		return;

	// apply damping to the running peak speed
	m_dampSpeed = (1.0f - m_damp * dt) * m_dampSpeed;

	if (m_dampSpeed >= 30.0f)
	{
		// clamp the live speed to the damped envelope
		if (m_dampSpeed < PevFloat(pev, PEV_SPEED))
			PevFloat(pev, PEV_SPEED) = m_dampSpeed;
		else if (-m_dampSpeed > PevFloat(pev, PEV_SPEED))
			PevFloat(pev, PEV_SPEED) = -m_dampSpeed;
	}
	else
	{
		// decayed away - settle at the rest angles and stop
		VecCopy(VecPtr(PevVector(pev, PEV_ANGLES)), VecPtr(m_center));
		PevFloat(pev, PEV_SPEED) = 0.0f;
		m_pfnThink = NULL;
		VecSet(VecPtr(PevVector(pev, PEV_AVELOCITY)), 0.0f, 0.0f, 0.0f);
	}
}

//=========================================================
// CFuncPendulum::Touch
//
// Hurt whatever swings into us and shove it away from centre.
//=========================================================
void CFuncPendulum::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	edict_t* pOtherEdict;
	entvars_t* pevOther = CurrentOtherEntvars(pev, &pOtherEdict);
	if (!pevOther)
		return;

	if (PevFloat(pev, PEV_DMG) <= 0.0f)
		return;
	if ((PevInt(pevOther, PEV_TAKEDAMAGE) & 0x7FFFFFFF) == 0)
		return;

	float damage = PevFloat(pev, PEV_SPEED) * PevFloat(pev, PEV_DMG) * 0.01f;
	if (damage < 0.0f)
		damage = -damage;

	CBaseEntity* pHit = (CBaseEntity*)EngineGetPrivateData(pOtherEdict);
	if (pHit)
		pHit->TakeDamage(pev, pev, damage);

	// fling the toucher out from our centre at the impact speed
	float center[3];
	BrushCenter(center, pev);

	Vector& otherOrigin = PevVector(pevOther, PEV_ORIGIN);
	float dir[3];
	dir[0] = otherOrigin.x - center[0];
	dir[1] = otherOrigin.y - center[1];
	dir[2] = otherOrigin.z - center[2];

	if (VecNormalize(dir) == 0)
		VecSet(dir, 0.0f, 0.0f, 0.0f);

	Vector& otherVel = PevVector(pevOther, PEV_VELOCITY);
	VecSet(VecPtr(otherVel), dir[0] * damage, dir[1] * damage, dir[2] * damage);
}

//=========================================================
// CFuncPendulum::RopeTouch
//
// "Rope" mode: only a player may grab the pendulum, becoming
// its enemy/rider and having its velocity zeroed.
//=========================================================
void CFuncPendulum::RopeTouch(CBaseEntity* pOther)
{
	void* globals = GlobalsFromEntvars(pev);
	if (!globals)
		return;

	int otherIndex = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);
	edict_t* pToucher = EnginePEntityOfEntIndex(otherIndex);
	entvars_t* pevToucher = pToucher ? EngineGetVarsOfEnt(pToucher) : NULL;
	if (!pevToucher)
		return;

	// 8 == FL_CLIENT.  pev->flags is stored as a float, so the binary
	// loads it via FLD/__ftol before masking - convert, don't read raw bits.
	if (((int)PevFloat(pevToucher, PEV_FLAGS) & 8) == 0)
	{
		EngineAlertMessage(1, "Not a client\n");
		return;
	}

	// latch on the first touch only
	if (PevInt(pev, PEV_ENEMY) != otherIndex)
	{
		PevInt(pev, PEV_ENEMY) = otherIndex;
		VecSet(VecPtr(PevVector(pevToucher, PEV_VELOCITY)), 0.0f, 0.0f, 0.0f);
		PevInt(pevToucher, PEV_MOVETYPE) = 0;	// MOVETYPE_NONE
	}

	HL_UNUSED(pOther);
}

//=========================================================
// func_wall
//=========================================================
DLLEXPORT void func_wall(entvars_t* pev)
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

		CFuncWall* self = new (privateData) CFuncWall();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_illusionary
//=========================================================
DLLEXPORT void func_illusionary(entvars_t* pev)
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

		CFuncIllusionary* self = new (privateData) CFuncIllusionary();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_rotating
//=========================================================
DLLEXPORT void func_rotating(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 32);
		if (!privateData)
			return;

		memset(privateData, 0, 32);

		CFuncRotating* self = new (privateData) CFuncRotating();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_pendulum
//=========================================================
DLLEXPORT void func_pendulum(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 76);
		if (!privateData)
			return;

		memset(privateData, 0, 76);

		CFuncPendulum* self = new (privateData) CFuncPendulum();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
