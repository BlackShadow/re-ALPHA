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
#ifndef UTIL_H
#define UTIL_H

#include "enginecallback.h"

// The engine's globals, refreshed on every entry into the DLL
extern globalvars_t *gpGlobals;

const Vector g_vecZero = Vector(0.0f, 0.0f, 0.0f);

//
// Conversion among the three types of "entity", including identity-conversions.
//
inline edict_t *ENT(const entvars_t *pev)		{ return pev->pContainingEntity; }
inline edict_t *ENT(edict_t *pent)				{ return pent; }
inline edict_t *ENT(EOFFSET eoffset)			{ return (*g_engfuncs.pfnPEntityOfEntOffset)(eoffset); }
inline EOFFSET OFFSET(const edict_t *pent)		{ return (*g_engfuncs.pfnEntOffsetOfPEntity)(pent); }
inline EOFFSET OFFSET(const entvars_t *pev)		{ return OFFSET(ENT(pev)); }
inline entvars_t *VARS(entvars_t *pev)			{ return pev; }
inline entvars_t *VARS(edict_t *pent)			{ return (*g_engfuncs.pfnGetVarsOfEnt)(pent); }
inline entvars_t *VARS(EOFFSET eoffset)			{ return VARS(ENT(eoffset)); }
inline int ENTINDEX(EOFFSET eoffset)			{ return (*g_engfuncs.pfnIndexOfEntOffset)(eoffset); }

inline const char *STRING(string_t iString)		{ return (*g_engfuncs.pfnSzFromIndex)(iString); }

// Testing for the world entity (or no entity at all)
inline BOOL FNullEnt(EOFFSET eoffset)			{ return eoffset == 0; }
inline BOOL FNullEnt(const edict_t *pent)		{ return pent == NULL || FNullEnt(OFFSET(pent)); }
inline BOOL FNullEnt(const entvars_t *pev)		{ return pev == NULL || FNullEnt(OFFSET(pev)); }

// Testing strings for nullity
inline BOOL FStringNull(string_t iString)		{ return iString == 0; }

#define ARRAYSIZE(p)	(sizeof(p) / sizeof((p)[0]))

// player hulls and eye positions
#define VEC_HULL_MIN		Vector(-16.0f, -16.0f, -36.0f)
#define VEC_HULL_MAX		Vector(16.0f, 16.0f, 36.0f)
#define VEC_DUCK_HULL_MIN	Vector(-16.0f, -16.0f, -18.0f)
#define VEC_DUCK_HULL_MAX	Vector(16.0f, 16.0f, 18.0f)
#define VEC_VIEW			Vector(0.0f, 0.0f, 28.0f)
#define VEC_DUCK_VIEW		Vector(0.0f, 0.0f, 12.0f)

// Misc useful
inline BOOL FStrEq(const char *sz1, const char *sz2)
{
	return strcmp(sz1, sz2) == 0;
}

inline BOOL FClassnameIs(const entvars_t *pev, const char *szClassname)
{
	return FStrEq(STRING(pev->classname), szClassname);
}

//
// Random numbers, from the C runtime rand()
//
inline float RANDOM_FLOAT(float flLow, float flHigh)
{
	return (float)(rand() & RAND_MAX) * (1.0f / RAND_MAX) * (flHigh - flLow) + flLow;
}

inline int RANDOM_LONG(int lLow, int lHigh)
{
	return lLow + rand() % (lHigh - lLow + 1);
}

//
// Engine wrappers
//
typedef enum
{
	dont_ignore_monsters = 0,
	ignore_monsters = 1,
} IGNORE_MONSTERS;

// MOVE_TO_ORIGIN movement types
#define MOVE_STRAIGHT	0		// step straight towards the goal
#define MOVE_CHASE		1		// steer around obstacles on the way to the goal

inline void UTIL_TraceLine(const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr)
{
	TRACE_LINE(vecStart, vecEnd, igmon, pentIgnore, ptr);
}

inline void UTIL_MakeVectors(const Vector &vecAngles)
{
	MAKE_VECTORS(vecAngles);
}

inline void UTIL_SetOrigin(entvars_t *pev, const Vector &vecOrigin)
{
	SET_ORIGIN(ENT(pev), vecOrigin);
}

inline void UTIL_SetSize(entvars_t *pev, const Vector &vecMin, const Vector &vecMax)
{
	SET_SIZE(ENT(pev), vecMin, vecMax);
}

inline float UTIL_VecToYaw(const Vector &vec)
{
	return VEC_TO_YAW(vec);
}

inline Vector UTIL_VecToAngles(const Vector &vec)
{
	float rgflVecOut[3];
	VEC_TO_ANGLES(vec, rgflVecOut);
	return Vector(rgflVecOut);
}

#endif // UTIL_H
