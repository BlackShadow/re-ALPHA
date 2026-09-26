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
// Tentacle - plays all its animations in a loop
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define TENTACLE_HEALTH				30.0f
#define TENTACLE_YAWSPEED			8.0f
#define TENTACLE_THINK_INTERVAL		0.1f
#define TENTACLE_NUM_SEQUENCES		11		// tentacle.mdl, played one after the other
#define TENTACLE_START_FRAMES		30.0f	// BeginActive starts at a random frame below this

// The first tentacle to be triggered plays the flies, the second one
// the squirm. The world resets both on spawn.
int g_fFliesPlayed = FALSE;
int g_fSquirmPlayed = FALSE;

class CTentacle : public CBaseMonster
{
public:
	void Spawn();
	int Classify();
	void SetActivity(int activity);

	void BeginActive(CBaseEntity *pOther);
	void ActiveThink(CBaseEntity *pOther);

	void EmitAmbientSound();
};

//=========================================================
// Spawn
//=========================================================
void CTentacle::Spawn()
{
	PRECACHE_SOUND("ambience/flies.wav");
	PRECACHE_SOUND("ambience/squirm2.wav");
	PRECACHE_MODEL("models/tentacle.mdl");

	SET_MODEL(ENT(pev), "models/tentacle.mdl");
	UTIL_SetSize(pev, Vector(-18, -18, 0), Vector(18, 18, 72));

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->health = TENTACLE_HEALTH;
	pev->yaw_speed = TENTACLE_YAWSPEED;
	pev->sequence = 0;

	if (!FStringNull(pev->targetname))
	{
		// wait until triggered
		SetUse(&CTentacle::BeginActive);
		SetThink(NULL);
	}
	else
	{
		SetThink(&CTentacle::ActiveThink);
	}

	pev->nextthink = gpGlobals->time + 1.0f;
}

//=========================================================
// Classify
//=========================================================
int CTentacle::Classify()
{
	return CLASS_NONE;
}

//=========================================================
// EmitAmbientSound - starts one of the two ambient sounds
// that are still free
//=========================================================
void CTentacle::EmitAmbientSound()
{
	if (g_fFliesPlayed)
	{
		if (!g_fSquirmPlayed)
		{
			EMIT_SOUND(ENT(pev), CHAN_BODY, "ambience/squirm2.wav", VOL_NORM, ATTN_NORM);
			g_fSquirmPlayed = TRUE;
		}
	}
	else
	{
		EMIT_SOUND(ENT(pev), CHAN_BODY, "ambience/flies.wav", VOL_NORM, ATTN_NORM);
		g_fFliesPlayed = TRUE;
	}
}

//=========================================================
// BeginActive - the trigger starts the animation
//=========================================================
void CTentacle::BeginActive(CBaseEntity *pOther)
{
	SetActivity(MONSTERSTATE_IDLE);
	SetThink(&CTentacle::ActiveThink);

	EmitAmbientSound();

	pev->frame = RANDOM_FLOAT(0.0f, 1.0f) * TENTACLE_START_FRAMES;
	pev->nextthink = gpGlobals->time + TENTACLE_THINK_INTERVAL;
}

//=========================================================
// ActiveThink - plays the next sequence when one ends
//=========================================================
void CTentacle::ActiveThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + TENTACLE_THINK_INTERVAL;
	AdvanceAnimation(TENTACLE_THINK_INTERVAL);

	if (!m_fSequenceFinished)
		return;

	pev->sequence++;
	if (pev->sequence == TENTACLE_NUM_SEQUENCES)
		pev->sequence = 0;

	SetActivity(MONSTERSTATE_IDLE);
}

//=========================================================
// SetActivity - restarts the current sequence, whatever
// the state
//=========================================================
void CTentacle::SetActivity(int activity)
{
	pev->frame = 0;
	ResetSequenceInfo(TENTACLE_THINK_INTERVAL);

	if (pev->sequence < 0 || pev->sequence >= TENTACLE_NUM_SEQUENCES)
	{
		ALERT(at_error, "Bogus Tentacle anim: %d", pev->sequence);
		m_flFrameRate = 0;
		m_flGroundSpeed = 0;
	}
}

class CTentacleBeak : public CTentacle
{
public:
	void Spawn();
	void Think(CBaseEntity *pOther);
	void SetActivity(int activity);
};

//=========================================================
// Spawn
//=========================================================
void CTentacleBeak::Spawn()
{
	PRECACHE_MODEL("models/tentbeak.mdl");

	SET_MODEL(ENT(pev), "models/tentbeak.mdl");
	UTIL_SetSize(pev, Vector(-18, -18, 0), Vector(18, 18, 72));

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->health = TENTACLE_HEALTH;
	pev->yaw_speed = TENTACLE_YAWSPEED;
	pev->sequence = 0;

	SetActivity(MONSTERSTATE_IDLE);

	pev->nextthink = gpGlobals->time + 1.0f;
}

//=========================================================
// Think - loops its only sequence
//=========================================================
void CTentacleBeak::Think(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + TENTACLE_THINK_INTERVAL;
	AdvanceAnimation(TENTACLE_THINK_INTERVAL);
}

//=========================================================
// SetActivity - restarts its only sequence
//=========================================================
void CTentacleBeak::SetActivity(int activity)
{
	pev->frame = 0;
	ResetSequenceInfo(TENTACLE_THINK_INTERVAL);
	pev->sequence = 0;

	// tentbeak.mdl has a single sequence
	if (pev->sequence != 0)
	{
		ALERT(at_error, "Bogus Tentacle anim: %d", pev->sequence);
		m_flFrameRate = 0;
		m_flGroundSpeed = 0;
	}
}

LINK_ENTITY_TO_CLASS(monster_tentacle, CTentacle);
LINK_ENTITY_TO_CLASS(monster_tentacle_beak, CTentacleBeak);
