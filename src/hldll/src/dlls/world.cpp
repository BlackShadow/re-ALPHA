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
// World - worldspawn entity (precaches base assets,
// sets up cvars, light styles, decals and the body queue)
//=========================================================

#include <new>
#include <string.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// Shared world globals (cross-entity state in the original
// hl.dll).  Kept file-local until shared storage exists.
//=========================================================

extern int g_LastSpawnEntIndex;		// last spawn point (ent index)
extern int g_fSquirmPlayed;			// tentacle squirm ambient gate
extern int g_fFliesPlayed;			// tentacle flies ambient gate
int g_pBodyQueueHead;				// body queue head/cursor (ent index)

static short g_sModelIndexShell;	// "models/shell.mdl" index
short g_sModelIndexShrapnel;		// "models/shrapnel.mdl" index

//=========================================================
// Number of bodies kept in the gib/body queue.
//=========================================================

#define BODYQUE_SIZE	4

//=========================================================
// Base sounds precached by every level.
//=========================================================

static const char* pBaseSounds[] =
{
	"common/null.wav",
	"common/thump.wav",
	"common/h2ohit1.wav",
	"common/outwater.wav",
	"common/talk.wav",
	"player/gasp2.wav",
	"player/sprayer.wav",
	"player/pl_jumpland2.wav",
	"player/pl_fallpain1.wav",
	"player/pl_fallpain2.wav",
	"player/pl_fallpain3.wav",
	"player/gasp1.wav",
	"player/pl_jump1.wav",
	"player/pl_jump2.wav",
	"player/pl_step1.wav",
	"player/pl_step2.wav",
	"player/pl_step3.wav",
	"player/pl_step4.wav",
	"hgrunt/gr_step1.wav",
	"hgrunt/gr_step2.wav",
	"hgrunt/gr_step3.wav",
	"hgrunt/gr_step4.wav",
	"player/inh2o.wav",
	"common/bodysplat.wav",
	"player/tornoff2.wav",
	"player/pl_pain2.wav",
	"player/pl_pain4.wav",
	"player/pl_pain5.wav",
	"player/pl_pain6.wav",
	"player/pl_pain7.wav",
	"common/water1.wav",
	"common/water2.wav",
};

//=========================================================
// Base models precached by every level.
//=========================================================

static const char* pBaseModels[] =
{
	"models/doctor.mdl",
	"models/gib_b_bone.mdl",
	"models/gib_b_gib.mdl",
	"models/gib_legbone.mdl",
	"models/gib_skull.mdl",
	"models/gib_lung.mdl",
};

//=========================================================
// Light styles.  Each entry is { style index, pattern }.
//=========================================================

typedef struct LightStyleEntry
{
	int			style;
	const char*	pattern;
} LightStyleEntry;

static const LightStyleEntry pLightStyles[] =
{
	{  0, "m" },
	{  1, "mmnmmommommnonmmonqnmmo" },
	{  2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba" },
	{  3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg" },
	{  4, "mamamamamama" },
	{  5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj" },
	{  6, "nmonqnmomnmomomno" },
	{  7, "mmmaaaabcdefgmmmmaaaammmaamm" },
	{  8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa" },
	{  9, "aaaaaaaazzzzzzzz" },
	{ 10, "mmamammmmammamamaaamammma" },
	{ 11, "abcdefghijklmnopqrrqponmlkjihgfedcba" },
	{ 12, "mmnnmmnnnmmnn" },
	{ 63, "a" },
};

//=========================================================
// Decals.  Registered in fixed index order.
//=========================================================

static const char* pDecals[] =
{
	"{shot1",
	"{shot2",
	"{shot3",
	"{shot4",
	"{shot5",
	"{hl",
	"{lambda01",
	"{lambda02",
	"{lambda03",
	"{lambda04",
	"{lambda05",
	"{lambda06",
	"{scorch1",
	"{scorch2",
	"{blood1",
	"{blood2",
	"{blood3",
	"{blood4",
	"{blood5",
	"{blood6",
	"{yblood1",
	"{yblood2",
	"{yblood3",
	"{yblood4",
	"{yblood5",
	"{yblood6",
	"{break1",
	"{break2",
	"{break3",
};

//=========================================================
// Weapon assets precached by every level (W_Precache).
//=========================================================

static const char* pWeaponModels[] =
{
	"models/grenade.mdl",
	"sprites/shard.spr",
	"models/v_crowbar.mdl",
	"models/v_glock.mdl",
	"models/v_mp5.mdl",
};

static const char* pWeaponSounds[] =
{
	"weapons/debris1.wav",
	"weapons/debris2.wav",
	"weapons/debris3.wav",
	"player/pl_shell1.wav",
	"player/pl_shell2.wav",
	"player/pl_shell3.wav",
	"weapons/hks1.wav",
	"weapons/hks2.wav",
	"weapons/hks3.wav",
	"weapons/pl_gun1.wav",
	"weapons/pl_gun2.wav",
	"weapons/glauncher.wav",
	"weapons/glauncher2.wav",
	"weapons/g_bounce1.wav",
	"weapons/g_bounce2.wav",
	"weapons/g_bounce3.wav",
};

//=========================================================
// CWorld
//=========================================================

class CWorld : public CBaseEntity
{
public:
	void Spawn();
};

HL_COMPILE_TIME_ASSERT(sizeof(CWorld) <= 28, CWorld_private_data_size);

//=========================================================
// W_Precache - precache weapon assets and
// record the shell/shrapnel model indices.
//=========================================================
static void W_Precache()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pWeaponModels); ++i)
		EnginePrecacheModel(pWeaponModels[i]);

	g_sModelIndexShell = (short)EnginePrecacheModel("models/shell.mdl");
	g_sModelIndexShrapnel = (short)EnginePrecacheModel("models/shrapnel.mdl");

	for (i = 0; i < (int)ARRAYSIZE(pWeaponSounds); ++i)
		EnginePrecacheSound(pWeaponSounds[i]);
}

//=========================================================
// InitBodyQue - allocate the body queue.
// Each node carries the "bodyque" classname and chains to
// the next node through PEV_OWNER_ENTINDEX; the last node
// loops back to the head.
//=========================================================
static void InitBodyQue()
{
	int classnameIndex;
	int i;
	int headIndex;
	int prevIndex;

	classnameIndex = EngineAllocString("bodyque");

	headIndex = 0;
	prevIndex = 0;

	for (i = 0; i < BODYQUE_SIZE; ++i)
	{
		edict_t* created = EngineCreateEntity();
		int index = EngineIndexOfEdict(created);

		if (i == 0)
		{
			g_pBodyQueueHead = index;
			headIndex = index;
		}
		else
		{
			edict_t* pPrev = EnginePEntityOfEntIndex(prevIndex);
			entvars_t* pevPrev = pPrev ? EngineGetVarsOfEnt(pPrev) : NULL;
			if (pevPrev)
				PevInt(pevPrev, PEV_OWNER_ENTINDEX) = index;
		}

		edict_t* pEdict = EnginePEntityOfEntIndex(index);
		entvars_t* pevNode = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
		if (pevNode)
			PevInt(pevNode, PEV_CLASSNAME) = classnameIndex;

		prevIndex = index;
	}

	// Close the ring: the last node points back at the head.
	{
		edict_t* pLast = EnginePEntityOfEntIndex(prevIndex);
		entvars_t* pevLast = pLast ? EngineGetVarsOfEnt(pLast) : NULL;
		if (pevLast)
			PevInt(pevLast, PEV_OWNER_ENTINDEX) = headIndex;
	}
}

//=========================================================
// Spawn
//=========================================================
void CWorld::Spawn()
{
	int i;

	g_LastSpawnEntIndex = 0;

	InitBodyQue();

	EngineCvarSetString("sv_gravity", "800");
	EngineCvarSetString("room_type", "0");

	g_fSquirmPlayed = 0;
	g_fFliesPlayed = 0;

	W_Precache();

	for (i = 0; i < (int)ARRAYSIZE(pBaseSounds); ++i)
		EnginePrecacheSound(pBaseSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pBaseModels); ++i)
		EnginePrecacheModel(pBaseModels[i]);

	for (i = 0; i < (int)ARRAYSIZE(pLightStyles); ++i)
		EngineLightStyle(pLightStyles[i].style, pLightStyles[i].pattern);

	for (i = 0; i < (int)ARRAYSIZE(pDecals); ++i)
		EngineDecalIndex(i, pDecals[i]);
}

//=========================================================
// worldspawn (export)
//=========================================================
DLLEXPORT void worldspawn(entvars_t* pev)
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

		CWorld* world = new (privateData) CWorld();
		world->pev = entvars;
		world->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
