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
// h_export.cpp - the engine callbacks: client functions
// and entity dispatch
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "client.h"

// Holds engine functionality callbacks
enginefuncs_t g_engfuncs;
globalvars_t *gpGlobals;

extern EOFFSET g_pBodyQueueHead;
extern int g_PlayerModelIndex;

extern "C" void WINAPI GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine)
{
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
}

//=========================================================
// set_suicide_frame - a player still in one piece drops
// dead
//=========================================================
static void set_suicide_frame(entvars_t *pev)
{
	if (!FStrEq(STRING(pev->model), "models/doctor.mdl"))
		return;		// already gibbed

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->deadflag = DEAD_DEAD;
	pev->nextthink = -1;
}

//=========================================================
// CopyToBodyQue - leaves a copy of the dead player's body
//=========================================================
static void CopyToBodyQue(entvars_t *pev)
{
	entvars_t *pevHead = VARS(g_pBodyQueueHead);

	pevHead->angles = pev->angles;
	pevHead->model = pev->model;
	pevHead->modelindex = pev->modelindex;
	pevHead->frame = pev->frame;
	pevHead->colormap = pev->colormap;
	pevHead->movetype = pev->movetype;
	pevHead->velocity = pev->velocity;
	pevHead->flags = 0;
	pevHead->deadflag = pev->deadflag;
	pevHead->sequence = pev->sequence;

	UTIL_SetOrigin(pevHead, pev->origin);
	UTIL_SetSize(pevHead, pev->mins, pev->maxs);

	g_pBodyQueueHead = pevHead->owner;
}

//=========================================================
// respawn - the dead player comes back in coop and
// deathmatch, single player restarts the level
//=========================================================
void respawn(entvars_t *pev)
{
	if (gpGlobals->coop == 0 && gpGlobals->deathmatch == 0)
	{
		SERVER_COMMAND("restart\n");
		return;
	}

	// make a copy of the dead body for appearances sake
	CopyToBodyQue(pev);

	if (gpGlobals->coop != 0)
		GET_SPAWN_PARMS(ENT(pev));	// the spawn parms as they were at level start
	else
		SetNewParms(gpGlobals);

	gpGlobals->self = OFFSET(pev);
	PutClientInServer(gpGlobals);
}

extern "C"
{

//=========================================================
// SetChangeParms - saves the player's state for the next
// level
//=========================================================
void WINAPI SetChangeParms(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;

	entvars_t *pev = VARS(gpGlobals->self);

	if (pev->health <= 0)
	{
		SetNewParms(pGlobals);
		return;
	}

	// cap super health
	if (pev->health > 100)
		pev->health = 100;
	if (pev->health < 50)
		pev->health = 50;

	SPAWNPARMS *pParms = (SPAWNPARMS *)gpGlobals->pSpawnParms;
	if (!pParms)
		return;

	pParms->items = (float)pev->items;
	pParms->health = pev->health;
	pParms->armorvalue = pev->armorvalue;
	pParms->ammo[0] = (float)pev->ammo_1;
	pParms->ammo[1] = (float)pev->ammo_2;
	pParms->ammo[2] = (float)pev->ammo_3;
	pParms->ammo[3] = (float)pev->ammo_4;
	pParms->weapon = (float)pev->weapon;
	pParms->armortype = pev->armortype * 100;
}

//=========================================================
// SetNewParms - the player's state when entering the game
//=========================================================
void WINAPI SetNewParms(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;

	SPAWNPARMS *pParms = (SPAWNPARMS *)malloc(sizeof(SPAWNPARMS));
	if (!pParms)
		return;

	memset(pParms, 0, sizeof(SPAWNPARMS));
	pParms->health = 100;

	entvars_t *pev = VARS(gpGlobals->self);

	pParms->items = (float)pev->items;
	pParms->armorvalue = pev->armorvalue;
	pParms->weapon = (float)pev->weapon;
	pParms->iFlags = (int)pev->flags & FL_DUCKING;
	pParms->fLandmark = FALSE;

	gpGlobals->pSpawnParms = pParms;
}

void WINAPI ClientKill(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;

	entvars_t *pev = VARS(gpGlobals->self);

	set_suicide_frame(pev);
	pev->modelindex = (float)g_PlayerModelIndex;
	pev->frags -= 2;	// extra penalty
	respawn(pev);
}

void WINAPI PutClientInServer(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;

	CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)VARS(gpGlobals->self));
	pPlayer->Spawn();
}

void WINAPI PlayerPreThink(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;

	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(gpGlobals->self);
	if (pPlayer)
		pPlayer->PreThink();
}

void WINAPI PlayerPostThink(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;

	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(gpGlobals->self);
	if (pPlayer)
		pPlayer->PostThink();
}

void WINAPI ClientConnect(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;
}

void WINAPI ClientDisconnect(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;

	entvars_t *pev = VARS(gpGlobals->self);

	EMIT_SOUND(ENT(pev), CHAN_BODY, "player/tornoff2.wav", VOL_NORM, ATTN_NONE);
	set_suicide_frame(pev);
}

void WINAPI StartFrame(globalvars_t *pGlobals)
{
	gpGlobals = pGlobals;

	gpGlobals->teamplay = CVAR_GET_FLOAT("teamplay");
}

//=========================================================
// Entity dispatch. The engine passes NULL as the other
// entity, which is found in gpGlobals->other.
//=========================================================
int DispatchSpawn(entvars_t *pev)
{
	gpGlobals = pev->pSystemGlobals;

	CBaseEntity *pEntity = CBaseEntity::Instance(pev);
	if (pEntity)
		pEntity->Spawn();

	return 0;
}

// Returns the key when nothing handled it
const char *DispatchKeyValue(entvars_t *pev, KeyValueData *pkvd)
{
	gpGlobals = pev->pSystemGlobals;

	CBaseEntity *pEntity = CBaseEntity::Instance(pev);
	if (pEntity)
		pEntity->KeyValue(pkvd);

	if (pkvd->fHandled || FStrEq(pkvd->szKeyName, "transitionid"))
		return NULL;

	return pkvd->szKeyName;
}

int DispatchRestore(entvars_t *pev, SAVERESTOREDATA *pSaveData)
{
	gpGlobals = pev->pSystemGlobals;

	CBaseEntity *pEntity = CBaseEntity::Instance(pev);
	if (pEntity)
		return pEntity->Restore(pSaveData);

	return 0;
}

int DispatchSave(entvars_t *pev, SAVERESTOREDATA *pSaveData)
{
	gpGlobals = pev->pSystemGlobals;

	CBaseEntity *pEntity = CBaseEntity::Instance(pev);
	if (pEntity)
		return pEntity->Save(pSaveData);

	return 0;
}

void DispatchThink(entvars_t *pev, CBaseEntity *pOther)
{
	gpGlobals = pev->pSystemGlobals;

	CBaseEntity *pEntity = CBaseEntity::Instance(pev);
	if (pEntity)
		pEntity->Think(pOther);
}

void DispatchTouch(entvars_t *pev, CBaseEntity *pOther)
{
	gpGlobals = pev->pSystemGlobals;

	CBaseEntity *pEntity = CBaseEntity::Instance(pev);
	if (pEntity)
		pEntity->Touch(pOther);
}

void DispatchUse(entvars_t *pev, CBaseEntity *pOther)
{
	gpGlobals = pev->pSystemGlobals;

	CBaseEntity *pEntity = CBaseEntity::Instance(pev);
	if (pEntity)
		pEntity->Use(pOther);
}

void DispatchBlocked(entvars_t *pev, CBaseEntity *pOther)
{
	gpGlobals = pev->pSystemGlobals;

	CBaseEntity *pEntity = CBaseEntity::Instance(pev);
	if (pEntity)
		pEntity->Blocked(pOther);
}

}
