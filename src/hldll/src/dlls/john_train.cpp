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
// john_train - alpha cinematic walker (CJohnTrain).
//
// A bare CBaseEntity SOLID_BSP / MOVETYPE_PUSH brush that, every think,
// re-derives its forward vector from a slowly turning facing and steers
// its velocity along it (a "scientist mover" used by the alpha cinematics).
// Extracted from plats.cpp into its own translation unit; reversed from
// The binary (Spawn, Think).
//=========================================================

#include <new>
#include <string.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// brush mover constants (pev floats), matching plats.cpp.
//=========================================================
#define SOLID_BSP			4.0f	// pev->solid
#define MOVETYPE_PUSH		7.0f	// pev->movetype

//=========================================================
// pev field offset not named in utils.h.
//=========================================================
enum
{
	PEV_AVEL_Y			= 92,	// pev->avelocity.y (john_train fixed spin)
};

//=========================================================
// CJohnTrain - alpha cinematic walker (john_train, private data 28).
//=========================================================
class CJohnTrain : public CBaseEntity
{
public:
	void Spawn();
	void Think(CBaseEntity* pOther);		// vtable slot 5 = (WalkThink)
};

HL_COMPILE_TIME_ASSERT(sizeof(CJohnTrain) <= 28, CJohnTrain_size);

//=========================================================
// CJohnTrain::Think
//
// Re-derive the forward vector from the (slowly turning) angles, set
// the walker's velocity along it, and re-arm the think.
//=========================================================
void CJohnTrain::Think(CBaseEntity* pOther)
{
	void* globals = GlobalsFromEntvars(pev);

	// rebuild v_forward from our facing
	Vector vecAngles;
	VecCopy(VecPtr(vecAngles), VecPtr(PevVector(pev, PEV_ANGLES)));
	EngineMakeVectors(VecPtr(vecAngles));

	// turn a little and stamp the fixed spin field
	PevVector(pev, PEV_ANGLES).y -= 2.0f;
	PevFloat(pev, PEV_AVEL_Y) = -200.0f;

	// velocity = v_forward * speed (speed kept in pev->dmgtime)
	float flSpeed = PevFloat(pev, PEV_DMGTIME);
	const float* vForward = GlobalsForward(globals);

	Vector& vel = PevVector(pev, PEV_VELOCITY);
	vel.x = vForward[0] * flSpeed;
	vel.y = vForward[1] * flSpeed;
	vel.z = vForward[2] * flSpeed;

	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);	// binary: double 0.1 added, stored as float
}

//=========================================================
// CJohnTrain::Spawn (john_train Spawn)
//
// Become a MOVETYPE_PUSH / SOLID_BSP brush, bind the model, then
// start thinking almost immediately.
//=========================================================
void CJohnTrain::Spawn()
{
	PevFloat(pev, PEV_MOVETYPE) = MOVETYPE_PUSH;
	PevFloat(pev, PEV_SOLID) = SOLID_BSP;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);	// binary: double 0.1 added, stored as float
}

//=========================================================
// john_train
//=========================================================
DLLEXPORT void john_train(entvars_t* pev)
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

		CJohnTrain* self = new (privateData) CJohnTrain();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
