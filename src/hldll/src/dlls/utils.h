#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "hl_types.h"
#include "../public/edict.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(p) (sizeof(p) / sizeof((p)[0]))
#endif

//=========================================================
// entvars_t accessors (alpha offsets)
//=========================================================

enum
{
	PEV_LTIME				= 28,
	PEV_MOVETYPE			= 32,
	PEV_GRAVITY				= 144,	// pev->gravity (movement gravity scale)
	PEV_SOLID				= 36,
	PEV_ORIGIN				= 40,
	PEV_VELOCITY			= 64,
	PEV_ANGLES				= 76,
	PEV_CLASSNAME			= 124,
	PEV_STUCK				= 140,
	PEV_MINS				= 180,
	PEV_MAXS				= 192,
	PEV_SIZE				= 204,
	PEV_SEQUENCE			= 156,
	PEV_ANIMTIME			= 160,
	PEV_FRAME				= 164,
	PEV_FRAMERATE			= 168,
	PEV_NEXTTHINK			= 232,
	PEV_RENDERMODE			= 240,
	PEV_HEALTH				= 264,
	PEV_ITEMS_LOW			= 276,
	PEV_WEAPON				= 272,
	PEV_ITEMS_HIGH			= 308,
	PEV_TAKEDAMAGE			= 316,
	PEV_CHAIN				= 320,
	PEV_ENEMY				= 376,
	PEV_FLAGS				= 380,
	PEV_IDEAL_YAW			= 416,
	PEV_YAWSPEED			= 420,
	PEV_SPAWNFLAGS			= 432,
	PEV_TARGET				= 436,
	PEV_TARGETNAME			= 440,
	PEV_GOALENTINDEX		= 428,
	PEV_OWNER_ENTINDEX		= 456,
	PEV_DMG					= 500,
	PEV_DMG_TOGGLE			= 500,	// alias: pev->dmg as brush blocking damage
	PEV_PAIN_FINISHED		= 512,
	// brush / mover fields (func_door / triggers / breakables)
	PEV_MODELINDEX			= 0,	// pev->modelindex
	PEV_MODEL				= 128,	// pev->model (brush model string index)
	PEV_SKIN				= 132,	// pev->skin (KeyValue "skin")
	PEV_MOVEDIR				= 460,	// pev->movedir (12 bytes)
	PEV_NOISE_MOVING		= 484,	// pev->noiseMoving (looping move sound)
	PEV_NOISE_ARRIVED		= 488,	// pev->noiseArrived (arrival/stop sound)
	PEV_DMGTIME				= 496,	// pev->dmgtime (door dmg re-arm time)
	PEV_EDICT_PTR			= 520,
	PEV_GLOBALS_PTR			= 524,
	PEV_VIEWOFS				= 328,
	PEV_AVELOCITY			= 88,
	PEV_V_ANGLE				= 112,
};

inline float& PevFloat(entvars_t* pev, size_t byteOffset)
{
	return *(float *)((uint8_t *)pev + byteOffset);
}

inline int& PevInt(entvars_t* pev, size_t byteOffset)
{
	return *(int *)((uint8_t *)pev + byteOffset);
}

inline Vector& PevVector(entvars_t* pev, size_t byteOffset)
{
	return *(Vector *)((uint8_t *)pev + byteOffset);
}

inline edict_t* EdictFromEntvars(entvars_t* pev)
{
	if (!pev)
		return NULL;

	return *(edict_t **)((uint8_t *)pev + PEV_EDICT_PTR);
}

inline void* GlobalsFromEntvars(entvars_t* pev)
{
	if (!pev)
		return NULL;

	return *(void **)((uint8_t *)pev + PEV_GLOBALS_PTR);
}

inline float GlobalsTime(void* globals)
{
	if (!globals)
		return 0.0f;

	return *(float *)((uint8_t *)globals + 124);
}

// globalvars_t byte offsets used by gameplay code.
enum
{
	GLOBALS_TIME			= 124,
	GLOBALS_SELF_ENTINDEX	= 112,	// current entity index for Touch/Use dispatch
	GLOBALS_OTHER_ENTINDEX	= 116,	// 'other'/activator ent index stashed before Touch/Use dispatch
};

inline int* GlobalsInt(void* globals, size_t byteOffset)
{
	return (int *)((uint8_t *)globals + byteOffset);
}

inline const float* GlobalsForward(void* globals)
{
	if (!globals)
		return NULL;

	return (const float *)((uint8_t *)globals + 240);
}

inline const float* GlobalsRight(void* globals)
{
	if (!globals)
		return NULL;

	return (const float *)((uint8_t *)globals + 264);
}

inline const float* GlobalsUp(void* globals)
{
	if (!globals)
		return NULL;

	return (const float *)((uint8_t *)globals + 252);
}

inline void VecSet(float* out, float x, float y, float z)
{
	out[0] = x;
	out[1] = y;
	out[2] = z;
}

inline float* VecPtr(Vector& v)
{
	return &v.x;
}

inline const float* VecPtr(const Vector& v)
{
	return &v.x;
}

inline void VecCopy(float* out, const float* in)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

inline void Vec3Add(float* out, const float* a, const float* b)
{
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

inline void Vec3Sub(float* out, const float* a, const float* b)
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

inline void Vec3Scale(float* out, const float* in, float scale)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

inline float VecLength(const float* v)
{
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

inline float VecDot(const float* a, const float* b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline BOOL VecNormalize(float* v)
{
	float length = VecLength(v);
	if (length <= 0.0f)
		return 0;

	float inv = 1.0f / length;
	v[0] *= inv;
	v[1] *= inv;
	v[2] *= inv;
	return 1;
}

inline float RandomFloat(float a1, float a2)
{
	return ((float)(rand() & 0x7FFF)) * 0.000030518509f * (a2 - a1) + a1;
}

inline int RandomLong(int a1, int a2)
{
	return a1 + rand() % (a2 - a1 + 1);
}

// Original DLL global the binary..A8: last combat/damage direction.
extern float g_vecAttackDir[3];

// Original DLL global the binary: debug line drawing toggled by impulse 200.
extern int g_fDrawLines;

void DrawDebugLine(const Vector &start, const Vector &end);
