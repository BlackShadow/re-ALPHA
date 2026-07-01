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
// Exports - DLL entrypoints and dispatch
//=========================================================

#include <new>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "cbase.h"
#include "player.h"
#include "client.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

enum
{
	kPlayerPrivateDataSize = 452	// PutClientInServer export
};

enum
{
	DISPATCH_EVENT_TOUCH,
	DISPATCH_EVENT_USE,
	DISPATCH_EVENT_BLOCKED
};

static const char kTransitionIdKey[] = "transitionid";
static const char kTeamplayCvar[] = "teamplay";
static const char kSkillCvar[] = "skill";
static const char kRestartCommand[] = "restart\n";
static const char kPlayerTornoff2[] = "player/tornoff2.wav";
static const char kModelsDoctor[] = "models/doctor.mdl";

static int g_skillValue = 0;
static int g_frameCount = 0;
static int g_disableDisconnectSound = 0;
extern int g_pBodyQueueHead;

// The binary - the doctor player model index, stamped by
// CBasePlayer::Spawn (player.cpp). ClientKill restores it onto the
// killed player's pev->modelindex.
extern int g_PlayerModelIndex;

enum
{
	kEntIndex_Model = 32,
	kEntIndex_Sequence = 9,
	kEntIndex_Frame = 8,
	kEntIndex_Extra1 = 58,
	kEntIndex_RenderAmount = 81,
	kEntIndex_264 = 66,
	kEntIndex_272 = 68,
	kEntIndex_292 = 73,
	kEntIndex_296 = 74,
	kEntIndex_300 = 75,
	kEntIndex_304 = 76,
	kEntIndex_308 = 77,
	kEntIndex_380 = 95,
	kEntIndex_400 = 100,
	kEntIndex_404 = 101,
	kEntIndex_67 = 67,
	kEntIndex_Edict = 130,
	kEntIndex_Client = 131,
};

static inline int& EntInt(entvars_t* pev, size_t index)
{
	return ((int*)pev)[index];
}

static inline float& EntFloat(entvars_t* pev, size_t index)
{
	return ((float*)pev)[index];
}

static inline edict_t* EntEdict(entvars_t* pev)
{
	return (edict_t*)EntInt(pev, kEntIndex_Edict);
}

static inline client_t* EntClient(entvars_t* pev)
{
	return (client_t*)EntInt(pev, kEntIndex_Client);
}

static inline int& ClientInt(client_t* client, size_t index)
{
	return ((int*)client)[index];
}

static inline void* ClientParms(client_t* client)
{
	return *(void**)((uint8_t*)client + 176);
}

static inline void SetClientParms(client_t* client, void* parms)
{
	*(void**)((uint8_t*)client + 176) = parms;
}

static inline void CopyVec3Int(int* dst, const int* src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

static inline void SetVec3Int(int* dst, int x, int y, int z)
{
	dst[0] = x;
	dst[1] = y;
	dst[2] = z;
}

// Construct (once) and return the real CBasePlayer for an
// entvars. The player object is a 452-byte private-data block
// that installs the C++ player vtable in place, exactly like
// The binary PutClientInServer (export).
static CBasePlayer *GetPlayer(entvars_t *pev)
{
  if (!pev)
    return NULL;

  edict_t *edict = EntEdict(pev);
  if (!edict)
    return NULL;

  void *privateData = EngineGetPrivateData(edict);
	if (!privateData)
	{
		privateData = EngineAllocPrivateData(edict, kPlayerPrivateDataSize);
		if (!privateData)
			return NULL;

		memset(privateData, 0, kPlayerPrivateDataSize);
		CBasePlayer* player = new (privateData) CBasePlayer();
		void* vtable = *(void **)player;

		memset((unsigned char *)privateData + 12, 0, kPlayerPrivateDataSize - 12);
		*(void **)player = vtable;
		player->pev = pev;
		player->m_pGlobals = GlobalsFromEntvars(pev);
	}

  return (CBasePlayer *)privateData;
}

static entvars_t *GetEntvarsFromClient(client_t *client)
{
  if (!client)
    return NULL;
  edict_t *edict = EnginePEntityOfEntIndex(client->entIndex);
  return edict ? EngineGetVarsOfEnt(edict) : NULL;
}

static int FixupDoctorModel(entvars_t *pev)
{
  if (!pev)
    return 0;

  const char *modelName = EngineStringFromIndex(EntInt(pev, kEntIndex_Model));
  if (!modelName)
    return 0;

  int result = strcmp(modelName, kModelsDoctor);
  if (result != 0)
    return result;

  EntInt(pev, kEntIndex_Sequence) = 0;			// pev+36 = 0
  EntFloat(pev, kEntIndex_Frame) = 6.0f;		// pev+32 = 1086324736
  EntFloat(pev, kEntIndex_RenderAmount) = 2.0f;	// pev+324 = 0x40000000
  EntFloat(pev, kEntIndex_Extra1) = -1.0f;		// pev+232 = -1082130432
  return 0;
}

static int CopyRespawnState(entvars_t* pev)
{
	if (!pev)
		return 0;

	edict_t* spawnEdict = EnginePEntityOfEntIndex(g_pBodyQueueHead);
	entvars_t* spawnVars = spawnEdict ? EngineGetVarsOfEnt(spawnEdict) : NULL;
	if (!spawnVars)
		return 0;

	int* src = (int*)pev;
	int* dst = (int*)spawnVars;
	dst[19] = src[19];
	dst[20] = src[20];
	dst[21] = src[21];
	dst[32] = src[32];
	dst[0] = src[0];
	dst[41] = src[41];
	dst[96] = src[96];
	dst[8] = src[8];
	dst[16] = src[16];
	dst[17] = src[17];
	dst[18] = src[18];
	dst[95] = 0;
	dst[81] = src[81];
	dst[39] = src[39];

	int vecA[3];
	int vecB[3];
	CopyVec3Int(vecA, src + 10);
	float origin[3];
	memcpy(origin, vecA, sizeof(origin));
	EngineSetOrigin(spawnEdict, origin);
	CopyVec3Int(vecA, src + 48);
	CopyVec3Int(vecB, src + 45);
	float mins[3];
	float maxs[3];
	memcpy(mins, vecB, sizeof(mins));
	memcpy(maxs, vecA, sizeof(maxs));
	EngineSetSize(spawnEdict, mins, maxs);
	g_pBodyQueueHead = dst[114];
	return 0;
}

// Shared with player.cpp (CBasePlayer::DeadThink):
// the dead-think respawn path is the same engine respawn handshake
// ClientKill uses.
int ClientRespawn(entvars_t *pev)
{
  if (!pev)
    return 0;

  client_t *client = EntClient(pev);
  if (!client)
    return 0;

  if ((ClientInt(client, 37) & 0x7FFFFFFF) == 0 && (ClientInt(client, 36) & 0x7FFFFFFF) == 0)
    return EngineServerCommand(kRestartCommand);

  CopyRespawnState(pev);
  if ((ClientInt(client, 37) & 0x7FFFFFFF) != 0)
    EngineGetSpawnParms(EntEdict(pev));
  else
    SetNewParms(client);

  ClientInt(client, 28) = EngineIndexOfEdict(EntEdict(pev));
  return PutClientInServer(client);
}

// The alpha engine identifies an entity to the DLL by its entvars pointer, NOT
// its edict. Every Dispatch* entry point (and every classname factory) receives
// entvars_t* pev; the edict is recovered via pev->pContainingEntity (pev+0x208)
// and the C++ object via the engine GET_PRIVATE. This mirrors the binary exactly
// (e.g. DispatchSpawn: GET_PRIVATE(*(pev+0x208)) -> vtable[0]).
static CBaseEntity *EntityFromPev(entvars_t *pev)
{
	if (!pev)
		return NULL;

	edict_t *edict = EdictFromEntvars(pev);
	if (!edict)
		return NULL;

	return GetEntity(edict);
}

static int EntIndexFromPev(entvars_t *pev)
{
	if (!pev)
		return 0;

	edict_t *edict = EdictFromEntvars(pev);
	if (!edict)
		return 0;

	return EngineIndexOfEdict(edict);
}

static int EntIndexFromDispatchArg(entvars_t *pev)
{
	int index = EntIndexFromPev(pev);
	if (index)
		return index;

	edict_t *edict = (edict_t *)pev;
	entvars_t *vars = edict ? EngineGetVarsOfEnt(edict) : NULL;
	if (vars && EdictFromEntvars(vars) == edict)
		return EngineIndexOfEdict(edict);

	return 0;
}

// The original alpha engine seeded globalvars +0x70/+0x74 before it called the
// DLL.  IDA shows the DLL-side DispatchTouch/Use/Blocked exports are otherwise
// thin vtable forwards, while many translated callbacks read those alpha slots
// directly.  Repair that engine-side state from validated export arguments, but
// preserve an already-seeded slot when the argument cannot be proven.  The
// second dispatch argument itself is forwarded raw to the vtable call; some
// original handlers, such as momentary_door::Use, interpret it as non-entity
// data.
static void DispatchEntityEvent(entvars_t *pev, entvars_t *pevOther, int eventType)
{
	CBaseEntity *pEntity = EntityFromPev(pev);
	if (!pEntity)
		return;

	void *globals = GlobalsFromEntvars(pev);
	if (!globals)
		globals = pEntity->m_pGlobals;

	int otherIndex = EntIndexFromDispatchArg(pevOther);
	CBaseEntity *pOther = (CBaseEntity *)pevOther;

	if (!globals)
	{
		if (eventType == DISPATCH_EVENT_TOUCH)
			pEntity->Touch(pOther);
		else if (eventType == DISPATCH_EVENT_USE)
			pEntity->Use(pOther);
		else
			pEntity->Blocked(pOther);
		return;
	}

	int oldSelf = *GlobalsInt(globals, GLOBALS_SELF_ENTINDEX);
	int oldOther = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);
	int selfIndex = EntIndexFromPev(pev);

	if (!otherIndex)
	{
		otherIndex = oldOther;
	}

	if (selfIndex)
		*GlobalsInt(globals, GLOBALS_SELF_ENTINDEX) = selfIndex;
	if (otherIndex)
		*GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX) = otherIndex;

	if (eventType == DISPATCH_EVENT_TOUCH)
		pEntity->Touch(pOther);
	else if (eventType == DISPATCH_EVENT_USE)
		pEntity->Use(pOther);
	else
		pEntity->Blocked(pOther);

	*GlobalsInt(globals, GLOBALS_SELF_ENTINDEX) = oldSelf;
	*GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX) = oldOther;
}

extern "C" {

DLLEXPORT void __stdcall GiveFnptrsToDll(enginefuncs_t *pEngineFuncs)
{
  EngineCopyTable(pEngineFuncs);
}

DLLEXPORT int __stdcall SetChangeParms(client_t *pClient)
{
  if (!pClient)
    return 0;

  entvars_t* pev = GetEntvarsFromClient(pClient);
  if (!pev)
    return 0;

  int value = EntInt(pev, kEntIndex_264);
  if (value <= 0)
    return SetNewParms(pClient);

  if (value > 1120403456)
    EntInt(pev, kEntIndex_264) = 1120403456;

  if (value < 1112014848)
    EntInt(pev, kEntIndex_264) = 1112014848;

  float *parms = (float *)ClientParms(pClient);
  if (!parms)
    return 0;

  int *parmsInt = (int *)parms;
  parms[0] = (float)EntInt(pev, kEntIndex_308);
  parmsInt[3] = EntInt(pev, kEntIndex_264);
  parmsInt[4] = EntInt(pev, kEntIndex_404);
  parms[5] = (float)EntInt(pev, kEntIndex_292);
  parms[6] = (float)EntInt(pev, kEntIndex_296);
  parms[7] = (float)EntInt(pev, kEntIndex_300);
  parms[8] = (float)EntInt(pev, kEntIndex_304);
  parms[9] = (float)EntInt(pev, kEntIndex_272);
  parms[10] = EntFloat(pev, kEntIndex_400) * 100.0f;
  return (int)pev;
}

DLLEXPORT int __stdcall SetNewParms(client_t *pClient)
{
  if (!pClient)
    return 0;

  float *parms = (float *)malloc(0x78);
  if (!parms)
    return 0;

  memset(parms, 0, 0x78);
  int *parmsInt = (int *)parms;
  parms[0] = 2.0f;
  parms[3] = 100.0f;

  int result = 0;
  entvars_t* pev = GetEntvarsFromClient(pClient);
  if (pev)
  {
    parms[0] = (float)EntInt(pev, kEntIndex_308);
    parms[4] = EntFloat(pev, kEntIndex_404);
    parms[9] = (float)EntInt(pev, kEntIndex_272);
    result = (int)EntFloat(pev, kEntIndex_380) & 0x4000;	// ftol(pev->flags) & FL_DUCKING
    parmsInt[11] = result;
    parms[12] = 0.0f;
  }

  SetClientParms(pClient, parms);
  return result;
}

DLLEXPORT int __stdcall ClientKill(client_t *pClient)
{
  entvars_t *pev = GetEntvarsFromClient(pClient);
  if (!pev)
    return 0;

  FixupDoctorModel(pev);
  EntFloat(pev, 0) = (float)g_PlayerModelIndex;	// pev->modelindex = doctor model
  EntFloat(pev, kEntIndex_67) = EntFloat(pev, kEntIndex_67) - 2.0f;
  return ClientRespawn(pev);
}

DLLEXPORT int __stdcall PutClientInServer(client_t *pClient)
{
  if (!pClient)
    return 0;

  edict_t *edict = EnginePEntityOfEntIndex(pClient->entIndex);
  entvars_t *pev = edict ? EngineGetVarsOfEnt(edict) : NULL;
  if (!pev)
  {
    edict_t *created = EngineCreateEntity();
    pev = created ? EngineGetVarsOfEnt(created) : NULL;
  }
  if (!pev)
    return 0;

  CBasePlayer *player = GetPlayer(pev);
  if (!player)
    return 0;

  player->Spawn();
  return (int)player;
}

DLLEXPORT int __stdcall PlayerPreThink(client_t *pClient)
{
  entvars_t *pev = GetEntvarsFromClient(pClient);
  if (!pev)
    return 0;

  CBasePlayer *player = (CBasePlayer *)EngineGetPrivateData(EntEdict(pev));
  if (!player)
    return 0;

  player->PreThink();
  return (int)player;
}

DLLEXPORT int __stdcall PlayerPostThink(client_t *pClient)
{
  entvars_t *pev = GetEntvarsFromClient(pClient);
  if (!pev)
    return 0;

  CBasePlayer *player = (CBasePlayer *)EngineGetPrivateData(EntEdict(pev));
  if (!player)
    return 0;

  player->PostThink();
  return (int)player;
}

DLLEXPORT void __stdcall ClientConnect(client_t *pClient)
{
  HL_UNUSED(pClient);
}

DLLEXPORT int __stdcall ClientDisconnect(client_t *pClient)
{
  entvars_t *pev = GetEntvarsFromClient(pClient);
  if (!pev)
    return 0;

  if (!g_disableDisconnectSound)
  {
    EngineEmitSound(EntEdict(pev), 4, kPlayerTornoff2, 1.0f, 0.0f);
    return FixupDoctorModel(pev);
  }

  return (int)pev;
}

DLLEXPORT int __stdcall StartFrame(globalvars_t *pGlobals)
{
  if (pGlobals)
  {
    float *globalsFloat = (float *)pGlobals;
    globalsFloat[38] = EngineCvarGetFloat(kTeamplayCvar);
  }

  g_skillValue = (int)EngineCvarGetFloat(kSkillCvar);
  ++g_frameCount;
  return g_skillValue;
}

DLLEXPORT int DispatchSpawn(entvars_t *pev)
{
  CBaseEntity *pEntity = EntityFromPev(pev);
  if (!pEntity)
    return 0;

  typedef int (__thiscall *SpawnSlotFn)(CBaseEntity *);
  void *vtable = *(void **)pEntity;
  SpawnSlotFn spawn = (SpawnSlotFn)((void **)vtable)[0];
  spawn(pEntity);
  return 0;
}

DLLEXPORT const char* DispatchKeyValue(entvars_t *pev, KeyValueData *pkvd)
{
  CBaseEntity *pEntity = EntityFromPev(pev);
  if (pEntity)
    pEntity->KeyValue(pkvd);

  if (!pkvd || pkvd->fHandled)
    return NULL;
  if (!pkvd->szKeyName)
    return NULL;
  if (strcmp(pkvd->szKeyName, kTransitionIdKey) == 0)
    return NULL;

  return pkvd->szKeyName;
}

DLLEXPORT int DispatchRestore(entvars_t *pev, SAVERESTOREDATA *pSaveData)
{
	CBaseEntity *pEntity = EntityFromPev(pev);
	if (pEntity)
		return pEntity->Restore(pSaveData);
	return 0;
}

DLLEXPORT int DispatchSave(entvars_t *pev, SAVERESTOREDATA *pSaveData)
{
  CBaseEntity *pEntity = EntityFromPev(pev);
  if (pEntity)
    return pEntity->Save(pSaveData);
  return 0;
}

DLLEXPORT void DispatchThink(entvars_t *pev, entvars_t *pevOther)
{
  CBaseEntity *pEntity = EntityFromPev(pev);
  if (pEntity)
    pEntity->Think((CBaseEntity *)pevOther);
}

// Touch/Use/Blocked receive the "other" dispatch payload as a second pointer.
// The binary forwards it raw to the vtable slot; some handlers interpret it as
// non-entity data.
DLLEXPORT void DispatchTouch(entvars_t *pev, entvars_t *pevOther)
{
  DispatchEntityEvent(pev, pevOther, DISPATCH_EVENT_TOUCH);
}

DLLEXPORT void DispatchUse(entvars_t *pev, entvars_t *pevOther)
{
  DispatchEntityEvent(pev, pevOther, DISPATCH_EVENT_USE);
}

DLLEXPORT void DispatchBlocked(entvars_t *pev, entvars_t *pevOther)
{
  DispatchEntityEvent(pev, pevOther, DISPATCH_EVENT_BLOCKED);
}

DLLEXPORT BOOL __stdcall DllEntryPoint(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
  HL_UNUSED(hInstance);
  HL_UNUSED(dwReason);
  HL_UNUSED(lpReserved);
  return 1;
}

}
