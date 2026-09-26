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
// john_train - a test mover: a brush that keeps turning
// and drives along its facing at pev->speed
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define JOHN_TRAIN_TURN			2.0f	// yaw change per think
#define JOHN_TRAIN_SPIN			200.0f	// yaw angular velocity
#define JOHN_TRAIN_INTERVAL		0.1		// think interval

class CJohnTrain : public CBaseEntity
{
public:
	void Spawn();
	void Think(CBaseEntity *pOther);
};

LINK_ENTITY_TO_CLASS(john_train, CJohnTrain);

//=========================================================
// Think - turn a little and drive along the new facing
//=========================================================
void CJohnTrain::Think(CBaseEntity *pOther)
{
	UTIL_MakeVectors(pev->angles);

	pev->angles.y -= JOHN_TRAIN_TURN;
	pev->avelocity.y = -JOHN_TRAIN_SPIN;

	pev->velocity = gpGlobals->v_forward * pev->speed;
	pev->nextthink = pev->ltime + JOHN_TRAIN_INTERVAL;
}

void CJohnTrain::Spawn()
{
	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;
	SET_MODEL(ENT(pev), STRING(pev->model));

	pev->nextthink = pev->ltime + JOHN_TRAIN_INTERVAL;
}
