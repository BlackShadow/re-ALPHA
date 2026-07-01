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
// Tentacle - Tall sound-tracking swiper monster
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// monster-specific constants
//=========================================================

#define TENTACLE_THINK_INTERVAL		0.1f

#define TENTACLE_HEALTH				30.0f
#define TENTACLE_YAWSPEED			8.0f

#define TENTACLE_MAX_SEQUENCE		10
#define TENTACLE_NUM_SEQUENCES		11

#define TENTACLE_AMBIENT_VOL		1.0f
#define TENTACLE_AMBIENT_ATTN		0.8f
#define TENTACLE_AMBIENT_CHANNEL	4

// pev field offset 440 (targetname) is PEV_TARGETNAME in utils.h.

static const char kTentacleModel[] = "models/tentacle.mdl";
static const char kTentbeakModel[] = "models/tentbeak.mdl";

//=========================================================
// Sound Table
//=========================================================

static const char* pFliesSounds[] =
{
	"ambience/flies.wav",
};

static const char* pSquirmSounds[] =
{
	"ambience/squirm2.wav",
};

//=========================================================
// Shared "first activated tentacle plays flies, second
// plays squirm" state. These are module globals in the
// original.
//=========================================================
int g_fFliesPlayed = 0;
int g_fSquirmPlayed = 0;

//=========================================================
// CTentacle
//=========================================================

class CTentacle : public CBaseMonster
{
public:
	void Spawn();
	int Classify();

	void SetActivity(int activity);

protected:
	void BeginActive(CBaseEntity* pOther);
	void ActiveThink(CBaseEntity* pOther);

	void EmitAmbientSound();
};

HL_COMPILE_TIME_ASSERT(sizeof(CTentacle) <= 336, CTentacle_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CTentacle::Spawn()
{
	EnginePrecacheSound(pFliesSounds[0]);
	EnginePrecacheSound(pSquirmSounds[0]);
	EnginePrecacheModel(kTentacleModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kTentacleModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 18.0f, 18.0f, 72.0f);
		VecSet(mins, -18.0f, -18.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 0.0f;
	PevFloat(pev, PEV_MOVETYPE) = 0.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = TENTACLE_HEALTH;
	PevFloat(pev, PEV_YAWSPEED) = TENTACLE_YAWSPEED;
	PevInt(pev, PEV_SEQUENCE) = 0;

	if (PevInt(pev, PEV_TARGETNAME))
	{
		// Has a target/trigger - stay dormant until used.
		SetUse(&CTentacle::BeginActive);
		m_pfnThink = NULL;
	}
	else
	{
		// No trigger - start swiping immediately.
		SetThink(&CTentacle::ActiveThink);
	}

	SetNextThink(1.0f);
}

//=========================================================
// Classify
//=========================================================
int CTentacle::Classify()
{
	return 0;
}

//=========================================================
// EmitAmbientSound (helper)
//=========================================================
void CTentacle::EmitAmbientSound()
{
	edict_t* edict = EdictFromEntvars(pev);
	if (!edict)
		return;

	if (g_fFliesPlayed)
	{
		if (!g_fSquirmPlayed)
		{
			EngineEmitSound(edict, TENTACLE_AMBIENT_CHANNEL, pSquirmSounds[0], TENTACLE_AMBIENT_VOL, TENTACLE_AMBIENT_ATTN);
			g_fSquirmPlayed = 1;
		}
	}
	else
	{
		EngineEmitSound(edict, TENTACLE_AMBIENT_CHANNEL, pFliesSounds[0], TENTACLE_AMBIENT_VOL, TENTACLE_AMBIENT_ATTN);
		g_fFliesPlayed = 1;
	}
}

//=========================================================
// BeginActive
//=========================================================
void CTentacle::BeginActive(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	SetActivity(1);
	SetThink(&CTentacle::ActiveThink);

	EmitAmbientSound();

	PevFloat(pev, PEV_FRAME) = RandomFloat(0.0f, 1.0f) * 30.0f;
	SetNextThink(TENTACLE_THINK_INTERVAL);
}

//=========================================================
// ActiveThink
//=========================================================
void CTentacle::ActiveThink(CBaseEntity* pOther)
{
	SetNextThink(TENTACLE_THINK_INTERVAL);
	AdvanceAnimation(TENTACLE_THINK_INTERVAL);

	if (!m_fSequenceFinished)
		return;

	PevInt(pev, PEV_SEQUENCE) = PevInt(pev, PEV_SEQUENCE) + 1;
	if (PevInt(pev, PEV_SEQUENCE) == TENTACLE_NUM_SEQUENCES)
		PevInt(pev, PEV_SEQUENCE) = 0;

	SetActivity(1);
}

//=========================================================
// SetActivity
//
// The tentacle does not pick a sequence from the activity;
// it simply rebuilds the sequence info for whatever sequence
// is currently playing and validates it.
//=========================================================
void CTentacle::SetActivity(int activity)
{
	HL_UNUSED(activity);

	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(TENTACLE_THINK_INTERVAL);

	if ((unsigned int)PevInt(pev, PEV_SEQUENCE) > TENTACLE_MAX_SEQUENCE)
	{
		EngineAlertMessage(3, "Bogus Tentacle anim: %d", PevInt(pev, PEV_SEQUENCE));
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
	}
}

//=========================================================
// CTentacleBeak
//=========================================================

class CTentacleBeak : public CTentacle
{
public:
	void Spawn();
	void Think(CBaseEntity* pOther);

	void SetActivity(int activity);
};

HL_COMPILE_TIME_ASSERT(sizeof(CTentacleBeak) <= 336, CTentacleBeak_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CTentacleBeak::Spawn()
{
	EnginePrecacheModel(kTentbeakModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kTentbeakModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 18.0f, 18.0f, 72.0f);
		VecSet(mins, -18.0f, -18.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 0.0f;
	PevFloat(pev, PEV_MOVETYPE) = 0.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = TENTACLE_HEALTH;
	PevFloat(pev, PEV_YAWSPEED) = TENTACLE_YAWSPEED;
	PevInt(pev, PEV_SEQUENCE) = 0;

	SetActivity(1);

	SetNextThink(1.0f);
}

//=========================================================
// Think
//=========================================================
void CTentacleBeak::Think(CBaseEntity* pOther)
{
	SetNextThink(TENTACLE_THINK_INTERVAL);
	AdvanceAnimation(TENTACLE_THINK_INTERVAL);
}

//=========================================================
// SetActivity
//
// Like the tentacle, but it forces the sequence back to 0
// after rebuilding the sequence info.
//=========================================================
void CTentacleBeak::SetActivity(int activity)
{
	HL_UNUSED(activity);

	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(TENTACLE_THINK_INTERVAL);
	PevInt(pev, PEV_SEQUENCE) = 0;

	if (PevInt(pev, PEV_SEQUENCE))
	{
		EngineAlertMessage(3, "Bogus Tentacle anim: %d", PevInt(pev, PEV_SEQUENCE));
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
	}
}

//=========================================================
// monster_tentacle
//=========================================================
DLLEXPORT void monster_tentacle(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 336);
		if (!privateData)
			return;

		memset(privateData, 0, 336);

		CTentacle* monster = new (privateData) CTentacle();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// monster_tentacle_beak
//=========================================================
DLLEXPORT void monster_tentacle_beak(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 336);
		if (!privateData)
			return;

		memset(privateData, 0, 336);

		CTentacleBeak* monster = new (privateData) CTentacleBeak();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
