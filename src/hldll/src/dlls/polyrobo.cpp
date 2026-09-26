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
// PolyRobo - big robot that turns its head towards the
// enemy
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "studio.h"

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define POLYROBO_HEALTH				99999.0f	// practically invulnerable
#define POLYROBO_THINK_INTERVAL		0.1f

// head sweep
#define POLYROBO_SWEEP_SPEED		10.0f		// degrees per second
#define POLYROBO_SWEEP_RANGE		45			// degrees

// polyrobo.mdl sequences
enum
{
	POLYROBO_SEQ_IDLE = 0,
	POLYROBO_SEQ_PAIN,
	POLYROBO_SEQ_SPAWN = 3,
};

// polyrobo.mdl bone controllers
#define POLYROBO_CONTROLLER_HEAD	0

//=========================================================
// SetBoneController - sets a bone controller of the model
// to flValue, in degrees
//=========================================================
static void SetBoneController(entvars_t *pev, int iController, float flValue)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)GET_MODEL_PTR(ENT(pev));
	if (!pstudiohdr)
		return;

	if (iController >= pstudiohdr->numbonecontrollers)
		return;

	mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *)((unsigned char *)pstudiohdr + pstudiohdr->bonecontrollerindex) + iController;

	float flStart = pbonecontroller->start;
	float flEnd = pbonecontroller->end;

	// wrap the angle when the controller does not cover a full turn
	if ((pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
		&& flStart + 359.0f >= flEnd
		&& (flStart + flEnd) * 0.5f + 180.0f < flValue)
	{
		flValue = flValue - 360.0f;
	}

	int iSetting = (int)((flValue - flStart) / (flEnd - flStart) * 255.0f);
	if (iSetting < 0)
		iSetting = 0;
	if (iSetting > 255)
		iSetting = 255;

	pev->controller[iController] = iSetting;
}

class CPolyRobo : public CBaseMonster
{
public:
	void Spawn();
	void Pain(float flDamage);
	void Think(CBaseEntity *pOther);
};

//=========================================================
// Spawn
//=========================================================
void CPolyRobo::Spawn()
{
	PRECACHE_MODEL("models/polyrobo.mdl");

	SET_MODEL(ENT(pev), "models/polyrobo.mdl");
	UTIL_SetSize(pev, Vector(-48, -48, 0), Vector(48, 48, 212));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->effects = 0;
	pev->health = POLYROBO_HEALTH;
	pev->yaw_speed = 10.0f;
	pev->sequence = POLYROBO_SEQ_SPAWN;
	pev->takedamage = DAMAGE_AIM;

	pev->flags = (float)((int)pev->flags | FL_MONSTER);

	m_flFrameRate = 10000.0f;	// hurry through the spawn sequence

	pev->nextthink = pev->nextthink + RANDOM_FLOAT(0.0f, 0.5f);
}

//=========================================================
// Pain - flinches when idle
//=========================================================
void CPolyRobo::Pain(float flDamage)
{
	if (pev->sequence == POLYROBO_SEQ_IDLE)
	{
		pev->sequence = POLYROBO_SEQ_PAIN;
		pev->frame = 0;
		ResetSequenceInfo(POLYROBO_THINK_INTERVAL);
	}
}

//=========================================================
// Think - plays the one shot sequences, then idles, and
// turns the head towards the enemy
//=========================================================
void CPolyRobo::Think(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + POLYROBO_THINK_INTERVAL;

	if (m_fSequenceFinished)
	{
		// the one shot sequences go back to idle
		if (pev->sequence >= POLYROBO_SEQ_PAIN && pev->sequence <= POLYROBO_SEQ_SPAWN)
		{
			pev->sequence = POLYROBO_SEQ_IDLE;
			pev->frame = 0;
		}

		ResetSequenceInfo(POLYROBO_THINK_INTERVAL);
	}

	GetAnimationEventFlags(POLYROBO_THINK_INTERVAL);
	AdvanceAnimation(POLYROBO_THINK_INTERVAL);

	// a slow head sweep, overridden by the aim below
	int iSweep = (int)(gpGlobals->time * POLYROBO_SWEEP_SPEED) % POLYROBO_SWEEP_RANGE;
	SetBoneController(pev, POLYROBO_CONTROLLER_HEAD, (float)iSweep);

	// look at the enemy
	float flYaw = pev->angles.y;
	if (!FNullEnt(pev->enemy))
		flYaw = UTIL_VecToYaw(VARS(pev->enemy)->origin - pev->origin);

	SetBoneController(pev, POLYROBO_CONTROLLER_HEAD, flYaw - pev->angles.y);
}

LINK_ENTITY_TO_CLASS(monster_polyrobo, CPolyRobo);
