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
// Lights - Light style entities (light, light_spot)
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// pev field offset used by CLight::Spawn. The binary tests
// *(pev + 0x1b8) (byte offset 440) to decide whether the light
// is named; offset 440 is pev->targetname (the same field as
// PEV_TARGETNAME in utils.h). Kept as a local alias for clarity.
//=========================================================
enum
{
	PEV_LIGHT_TARGETNAME	= 440,
};

//=========================================================
// Lowest custom (DLL-controlled) light style index. Styles
// below this are reserved by the engine for hard-coded
// animated styles, so the DLL never drives them.
//=========================================================
#define CUSTOM_LIGHTSTYLE_BASE	32

//=========================================================
// Light style pattern strings (single-frame styles).
//   "m" - normal/full brightness (matches map-baked default)
//   "a" - darkest (light switched off)
//=========================================================
static const char kLightStyleNormal[] = "m";
static const char kLightStyleOff[] = "a";

//=========================================================
// CLight
//=========================================================

class CLight : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pOther);

private:
	// Light style index. In the alpha private data this sits at
	// byte offset 28; the C++ field name is what the methods use,
	// so it is declared as a plain member after CBaseEntity.
	int m_iStyle;
};

HL_COMPILE_TIME_ASSERT(sizeof(CLight) <= 32, CLight_private_data_size);

//=========================================================
// KeyValue
//=========================================================
void CLight::KeyValue(KeyValueData* pkvd)
{
	if (!pkvd)
		return;

	if (strcmp(pkvd->szKeyName, "style") == 0)
	{
		m_iStyle = atoi(pkvd->szValue);
		pkvd->fHandled = 1;
	}

	if (strcmp(pkvd->szKeyName, "pitch") == 0)
		pkvd->fHandled = 1;
}

//=========================================================
// Spawn
//
// An unnamed light has no runtime behavior (its value is baked
// into the level lightmap), so it removes itself. A named light
// persists and is given an initial style string so it can be
// toggled later through Use().
//=========================================================
void CLight::Spawn()
{
	if (PevInt(pev, PEV_LIGHT_TARGETNAME) == 0)
	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineRemoveEntity(edict);
		return;
	}

	if (m_iStyle >= CUSTOM_LIGHTSTYLE_BASE)
	{
		if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & 1) != 0)
			EngineLightStyle(m_iStyle, kLightStyleOff);
		else
			EngineLightStyle(m_iStyle, kLightStyleNormal);
	}
}

//=========================================================
// Use
//
// Toggles the custom light style between its normal ("m") and
// off ("a") patterns, tracking the current state in spawnflag
// bit 0.
//=========================================================
void CLight::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	if (m_iStyle < CUSTOM_LIGHTSTYLE_BASE)
		return;

	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & 1) != 0)
	{
		EngineLightStyle(m_iStyle, kLightStyleNormal);
		PevFloat(pev, PEV_SPAWNFLAGS) = (float)((int)PevFloat(pev, PEV_SPAWNFLAGS) & ~1);
	}
	else
	{
		EngineLightStyle(m_iStyle, kLightStyleOff);
		PevFloat(pev, PEV_SPAWNFLAGS) = (float)((int)PevFloat(pev, PEV_SPAWNFLAGS) | 1);
	}
}

//=========================================================
// light
//=========================================================
DLLEXPORT void light(entvars_t* pev)
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

		CLight* self = new (privateData) CLight();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// light_spot
//
// Shares the CLight class and vtable with light.
//=========================================================
DLLEXPORT void light_spot(entvars_t* pev)
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

		CLight* self = new (privateData) CLight();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
