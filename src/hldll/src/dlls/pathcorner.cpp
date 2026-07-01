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
// PathCorner - Path corner entities (path nodes for trains)
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

// pev->target (offset 436) is PEV_TARGET in utils.h.

//=========================================================
// Engine globalvars_t offsets used by Touch (read straight
// from the globals block the engine fills before the call).
//   116 - index of the entity that triggered the touch
//   112 - index of the path_corner being touched (self)
//=========================================================
enum
{
	GLOBALS_TRACE_ENT		= 116,
	GLOBALS_TOUCH_SELF		= 112,
};

//=========================================================
// CPathCorner
//=========================================================

class CPathCorner : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Touch(CBaseEntity* pOther);

private:
	// In the binary m_flWait lives at object offset 0x150 (336), the last
	// dword of the 340-byte private block (KeyValue writes *(this + 0x150),
	// Touch reads (*(this + 0x150) & 0x7FFFFFFF)). CBaseEntity occupies the
	// first 28 bytes, so pad up to offset 336 before the field.
	char m_reserved[336 - sizeof(CBaseEntity)];
	float m_flWait;	// offset 336 (0x150)
};

HL_COMPILE_TIME_ASSERT(sizeof(CPathCorner) <= 340, CPathCorner_private_data_size);


//=========================================================
// Spawn
//=========================================================
void CPathCorner::Spawn()
{
	PevFloat(pev, PEV_SOLID) = 1.0f;

	float mins[3];
	float maxs[3];
	VecSet(maxs, 8.0f, 8.0f, 8.0f);
	VecSet(mins, -8.0f, -8.0f, -8.0f);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetSize(edict, mins, maxs);
}

//=========================================================
// KeyValue
//=========================================================
void CPathCorner::KeyValue(KeyValueData* pkvd)
{
	if (!pkvd)
		return;

	if (strcmp(pkvd->szKeyName, "wait") == 0)
	{
		m_flWait = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// Touch
//=========================================================
void CPathCorner::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = GlobalsFromEntvars(pev);
	if (!globals)
		return;

	edict_t* pOtherEdict = EnginePEntityOfEntIndex(*GlobalsInt(globals, GLOBALS_TRACE_ENT));
	entvars_t* pevOther = pOtherEdict ? EngineGetVarsOfEnt(pOtherEdict) : NULL;
	if (!pevOther)
		return;

	if (*GlobalsInt(globals, GLOBALS_TOUCH_SELF) != PevInt(pevOther, PEV_GOALENTINDEX))
		return;

	if (PevInt(pevOther, PEV_ENEMY) != 0)
		return;

	// Original alpha behavior: warn about non-zero waits, then keep advancing.
	if ((*(int*)&m_flWait & 0x7FFFFFFF) != 0)
		EngineAlertMessage(2, "Non-zero path-cornder waits NYI");

	if (PevInt(pev, PEV_TARGET) == 0)
		EngineAlertMessage(2, "PathCornerTouch: no next stop specified");

	const char* pszNextName = EngineStringFromIndex(PevInt(pev, PEV_TARGET));
	edict_t* pNextEdict = EngineFindEntityByString(NULL, "targetname", pszNextName);
	int nextIndex = EngineIndexOfEdict(pNextEdict);

	PevInt(pevOther, PEV_GOALENTINDEX) = nextIndex;

	if (nextIndex)
	{
		edict_t* pNext = EnginePEntityOfEntIndex(nextIndex);
		entvars_t* pevNext = pNext ? EngineGetVarsOfEnt(pNext) : NULL;
		if (pevNext)
		{
			float vecDir[3];
			Vec3Sub(vecDir, VecPtr(PevVector(pevNext, PEV_ORIGIN)), VecPtr(PevVector(pevOther, PEV_ORIGIN)));
			PevFloat(pevOther, PEV_IDEAL_YAW) = EngineVecToYaw(vecDir);
		}
	}
	else
	{
		const char* pszTarget = EngineStringFromIndex(PevInt(pev, PEV_TARGET));
		const char* pszClassname = EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME));
		EngineAlertMessage(3, "PathCornerTouch--%s couldn't find next stop in path: %s", pszClassname, pszTarget);
	}
}

//=========================================================
// path_corner
//=========================================================
DLLEXPORT void path_corner(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 340);
		if (!privateData)
			return;

		memset(privateData, 0, 340);

		CPathCorner* self = new (privateData) CPathCorner();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
