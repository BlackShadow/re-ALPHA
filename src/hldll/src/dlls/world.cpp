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
// World - worldspawn precaches the shared models and sounds
// and sets up the light styles, decals and body queue
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "decals.h"

extern int g_LastSpawnEntIndex;		// the spawn spot the last player used
extern int g_fSquirmPlayed;			// tentacle ambient sounds
extern int g_fFliesPlayed;

EOFFSET g_pBodyQueueHead;			// first entity of the body queue

static short g_sModelIndexShell;
short g_sModelIndexShrapnel;

#define BODYQUE_SIZE		4

static const char *gBaseSounds[] =
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

static const char *gBaseModels[] =
{
	"models/doctor.mdl",
	"models/gib_b_bone.mdl",
	"models/gib_b_gib.mdl",
	"models/gib_legbone.mdl",
	"models/gib_skull.mdl",
	"models/gib_lung.mdl",
};

// decal names, in the order of the DECAL_* indices
static const char *gDecals[] =
{
	"{shot1",		// DECAL_SHOT1
	"{shot2",		// DECAL_SHOT2
	"{shot3",		// DECAL_SHOT3
	"{shot4",		// DECAL_SHOT4
	"{shot5",		// DECAL_SHOT5
	"{hl",			// DECAL_HL
	"{lambda01",	// DECAL_LAMBDA1
	"{lambda02",	// DECAL_LAMBDA2
	"{lambda03",	// DECAL_LAMBDA3
	"{lambda04",	// DECAL_LAMBDA4
	"{lambda05",	// DECAL_LAMBDA5
	"{lambda06",	// DECAL_LAMBDA6
	"{scorch1",		// DECAL_SCORCH1
	"{scorch2",		// DECAL_SCORCH2
	"{blood1",		// DECAL_BLOOD1
	"{blood2",		// DECAL_BLOOD2
	"{blood3",		// DECAL_BLOOD3
	"{blood4",		// DECAL_BLOOD4
	"{blood5",		// DECAL_BLOOD5
	"{blood6",		// DECAL_BLOOD6
	"{yblood1",		// DECAL_YBLOOD1
	"{yblood2",		// DECAL_YBLOOD2
	"{yblood3",		// DECAL_YBLOOD3
	"{yblood4",		// DECAL_YBLOOD4
	"{yblood5",		// DECAL_YBLOOD5
	"{yblood6",		// DECAL_YBLOOD6
	"{break1",		// DECAL_BREAK1
	"{break2",		// DECAL_BREAK2
	"{break3",		// DECAL_BREAK3
};

static const char *gWeaponModels[] =
{
	"models/grenade.mdl",
	"sprites/shard.spr",
	"models/v_crowbar.mdl",
	"models/v_glock.mdl",
	"models/v_mp5.mdl",
};

static const char *gWeaponSounds[] =
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

class CWorld : public CBaseEntity
{
public:
	void Spawn();
};

//=========================================================
// W_Precache - the weapon models and sounds
//=========================================================
static void W_Precache()
{
	for (int i = 0; i < (int)ARRAYSIZE(gWeaponModels); i++)
		PRECACHE_MODEL(gWeaponModels[i]);

	g_sModelIndexShell = (short)PRECACHE_MODEL("models/shell.mdl");
	g_sModelIndexShrapnel = (short)PRECACHE_MODEL("models/shrapnel.mdl");

	for (int i = 0; i < (int)ARRAYSIZE(gWeaponSounds); i++)
		PRECACHE_SOUND(gWeaponSounds[i]);
}

//=========================================================
// InitBodyQue - creates the body queue, a ring of
// BODYQUE_SIZE "bodyque" entities linked by pev->owner
//=========================================================
static void InitBodyQue()
{
	string_t istrClassname = ALLOC_STRING("bodyque");

	EOFFSET eoffsetHead = 0;
	EOFFSET eoffsetPrev = 0;

	for (int i = 0; i < BODYQUE_SIZE; i++)
	{
		EOFFSET eoffset = OFFSET(CREATE_ENTITY());

		if (i == 0)
		{
			g_pBodyQueueHead = eoffset;
			eoffsetHead = eoffset;
		}
		else
		{
			VARS(eoffsetPrev)->owner = eoffset;
		}

		VARS(eoffset)->classname = istrClassname;

		eoffsetPrev = eoffset;
	}

	// the last body points back at the head
	VARS(eoffsetPrev)->owner = eoffsetHead;
}

void CWorld::Spawn()
{
	g_LastSpawnEntIndex = 0;

	InitBodyQue();

	CVAR_SET_STRING("sv_gravity", "800");
	CVAR_SET_STRING("room_type", "0");	// clear the DSP

	g_fSquirmPlayed = FALSE;
	g_fFliesPlayed = FALSE;

	W_Precache();

	for (int i = 0; i < (int)ARRAYSIZE(gBaseSounds); i++)
		PRECACHE_SOUND(gBaseSounds[i]);

	for (int i = 0; i < (int)ARRAYSIZE(gBaseModels); i++)
		PRECACHE_MODEL(gBaseModels[i]);

	// Light styles: 'a' is total darkness, 'z' is full bright.
	// Styles from 32 up are switched by named lights (lights.cpp).

	// 0 normal
	LIGHT_STYLE(0, "m");

	// 1 flicker (first variety)
	LIGHT_STYLE(1, "mmnmmommommnonmmonqnmmo");

	// 2 slow strong pulse
	LIGHT_STYLE(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	// 3 candle (first variety)
	LIGHT_STYLE(3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

	// 4 fast strobe
	LIGHT_STYLE(4, "mamamamamama");

	// 5 gentle pulse
	LIGHT_STYLE(5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj");

	// 6 flicker (second variety)
	LIGHT_STYLE(6, "nmonqnmomnmomomno");

	// 7 candle (second variety)
	LIGHT_STYLE(7, "mmmaaaabcdefgmmmmaaaammmaamm");

	// 8 candle (third variety)
	LIGHT_STYLE(8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

	// 9 slow strobe
	LIGHT_STYLE(9, "aaaaaaaazzzzzzzz");

	// 10 fluorescent flicker
	LIGHT_STYLE(10, "mmamammmmammamamaaamammma");

	// 11 slow pulse, not fading to black
	LIGHT_STYLE(11, "abcdefghijklmnopqrrqponmlkjihgfedcba");

	// 12 underwater light mutation
	LIGHT_STYLE(12, "mmnnmmnnnmmnn");

	// 63 testing
	LIGHT_STYLE(63, "a");

	for (int i = 0; i < (int)ARRAYSIZE(gDecals); i++)
		DECAL_SET_NAME(i, gDecals[i]);
}

LINK_ENTITY_TO_CLASS(worldspawn, CWorld);
