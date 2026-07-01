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
// EngineFuncs - Engine function table wrappers
//=========================================================

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "enginefuncs.h"

enum
{
	ENGINE_TABLE_BYTES = 0x100
};

static void *g_EngineFuncsTable[ENGINE_TABLE_BYTES / sizeof(void *)];

enum
{
	kEng_PrecacheModel		= 0x00 / 4,
	kEng_PrecacheSound		= 0x04 / 4,
	kEng_SetModel			= 0x08 / 4,
	kEng_SetSize			= 0x10 / 4,
	kEng_ChangeLevel		= 0x14 / 4,
	kEng_GetSpawnParms		= 0x18 / 4,
	kEng_SaveSpawnParms		= 0x1C / 4,
	kEng_VecToYaw			= 0x20 / 4,
	kEng_VecToAngles		= 0x24 / 4,
	kEng_MoveToOrigin		= 0x2C / 4,
	kEng_ChangeYaw			= 0x30 / 4,
	kEng_ChangePitch		= 0x34 / 4,
	kEng_FindEntityByString	= 0x38 / 4,
	kEng_FindEntityInSphere	= 0x40 / 4,
	kEng_FindClientInPVS	= 0x44 / 4,
	kEng_MakeVectors		= 0x48 / 4,
	kEng_CreateEntity		= 0x4C / 4,
	kEng_RemoveEntity		= 0x50 / 4,
	kEng_MakeStatic			= 0x54 / 4,
	kEng_DropToFloor		= 0x5C / 4,
	kEng_WalkMove			= 0x60 / 4,
	kEng_SetOrigin			= 0x64 / 4,
	kEng_EmitSound			= 0x68 / 4,
	kEng_EmitAmbientSound	= 0x6C / 4,
	kEng_TraceLine			= 0x70 / 4,
	kEng_GetAimVector		= 0x78 / 4,
	kEng_ServerCommand		= 0x7C / 4,
	kEng_ClientCommand		= 0x80 / 4,
	kEng_ParticleEffect		= 0x84 / 4,
	kEng_LightStyle			= 0x88 / 4,
	kEng_DecalIndex			= 0x8C / 4,
	kEng_PointContents		= 0x90 / 4,
	kEng_WriteByte			= 0x94 / 4,
	kEng_WriteShort			= 0x9C / 4,
	kEng_WriteCoord			= 0xA8 / 4,
	kEng_CvarGetFloat		= 0xB4 / 4,
	kEng_CvarSetString		= 0xC0 / 4,
	kEng_AlertMessage		= 0xC4 / 4,
	kEng_AllocPrivateData	= 0xCC / 4,
	kEng_GetPrivateData		= 0xD0 / 4,
	kEng_StringFromIndex	= 0xE0 / 4,
	kEng_AllocString		= 0xE4 / 4,
	kEng_GetVarsOfEnt		= 0xE8 / 4,
	kEng_PEntityOfEntIndex	= 0xEC / 4,
	kEng_IndexOfEdict		= 0xF0 / 4,
	kEng_ModelIndex			= 0xF4 / 4,
	kEng_GetModelPtr		= 0xFC / 4,
};

void EngineCopyTable(enginefuncs_t *pEngineFuncs)
{
	if (pEngineFuncs)
		memcpy(g_EngineFuncsTable, pEngineFuncs, sizeof(g_EngineFuncsTable));
}

int EnginePrecacheModel(const char *name)
{
	int (*fn)(const char *) = (int (*)(const char *))g_EngineFuncsTable[kEng_PrecacheModel];
	return fn ? fn(name) : 0;
}

int EnginePrecacheSound(const char *name)
{
	int (*fn)(const char *) = (int (*)(const char *))g_EngineFuncsTable[kEng_PrecacheSound];
	return fn ? fn(name) : 0;
}

void EngineSetModel(edict_t *edict, const char *model)
{
	void (*fn)(edict_t *, const char *) = (void (*)(edict_t *, const char *))g_EngineFuncsTable[kEng_SetModel];
	if (fn)
		fn(edict, model);
}

float EngineVecToYaw(const float *vec)
{
	float (*fn)(const float *) = (float (*)(const float *))g_EngineFuncsTable[kEng_VecToYaw];
	return fn ? fn(vec) : 0.0f;
}

void EngineChangeLevel(const char *s1, const char *s2)
{
	void (*fn)(const char *, const char *) = (void (*)(const char *, const char *))g_EngineFuncsTable[kEng_ChangeLevel];
	if (fn)
		fn(s1, s2);
}

edict_t *EngineFindEntityByString(edict_t *pStart, const char *pszField, const char *pszValue)
{
	edict_t *(*fn)(edict_t *, const char *, const char *) =
		(edict_t *(*)(edict_t *, const char *, const char *))g_EngineFuncsTable[kEng_FindEntityByString];
	return fn ? fn(pStart, pszField, pszValue) : NULL;
}

edict_t *EngineFindEntityInSphere(const float *origin, float radius)
{
	edict_t *(*fn)(const float *, float) = (edict_t *(*)(const float *, float))g_EngineFuncsTable[kEng_FindEntityInSphere];
	return fn ? fn(origin, radius) : NULL;
}

edict_t *EngineFindClientInPVS()
{
	edict_t *(*fn)() = (edict_t *(*)())g_EngineFuncsTable[kEng_FindClientInPVS];
	return fn ? fn() : NULL;
}

edict_t *EnginePEntityOfEntIndex(int index)
{
	edict_t *(*fn)(int) = (edict_t *(*)(int))g_EngineFuncsTable[kEng_PEntityOfEntIndex];
	return fn ? fn(index) : NULL;
}

entvars_t *EngineGetVarsOfEnt(edict_t *edict)
{
	entvars_t *(*fn)(edict_t *) = (entvars_t *(*)(edict_t *))g_EngineFuncsTable[kEng_GetVarsOfEnt];
	return fn ? fn(edict) : NULL;
}

void *EngineGetPrivateData(edict_t *edict)
{
	void *(*fn)(edict_t *) = (void *(*)(edict_t *))g_EngineFuncsTable[kEng_GetPrivateData];
	return fn ? fn(edict) : NULL;
}

void *EngineAllocPrivateData(edict_t *edict, int size)
{
	void *(*fn)(edict_t *, int) = (void *(*)(edict_t *, int))g_EngineFuncsTable[kEng_AllocPrivateData];
	return fn ? fn(edict, size) : NULL;
}

edict_t *EngineCreateEntity()
{
	edict_t *(*fn)() = (edict_t *(*)())g_EngineFuncsTable[kEng_CreateEntity];
	return fn ? fn() : NULL;
}

int EngineRemoveEntity(edict_t *edict)
{
	int (*fn)(edict_t *) = (int (*)(edict_t *))g_EngineFuncsTable[kEng_RemoveEntity];
	return fn ? fn(edict) : 0;
}

const char *EngineStringFromIndex(int index)
{
	const char *(*fn)(int) = (const char *(*)(int))g_EngineFuncsTable[kEng_StringFromIndex];
	return fn ? fn(index) : NULL;
}

int EngineAllocString(const char *str)
{
	int (*fn)(const char *) = (int (*)(const char *))g_EngineFuncsTable[kEng_AllocString];
	return fn ? fn(str) : 0;
}

void EngineEmitSound(edict_t *entity, int channel, const char *sample, float volume, float attenuation)
{
	void (*fn)(edict_t *, int, const char *, float, float) =
		(void (*)(edict_t *, int, const char *, float, float))g_EngineFuncsTable[kEng_EmitSound];
	if (fn)
		fn(entity, channel, sample, volume, attenuation);
}

void EngineGetSpawnParms(edict_t *edict)
{
	void (*fn)(edict_t *) = (void (*)(edict_t *))g_EngineFuncsTable[kEng_GetSpawnParms];
	if (fn)
		fn(edict);
}

void EngineSaveSpawnParms(edict_t *edict)
{
	void (*fn)(edict_t *) = (void (*)(edict_t *))g_EngineFuncsTable[kEng_SaveSpawnParms];
	if (fn)
		fn(edict);
}

float EngineDropToFloor(edict_t *edict)
{
	float (*fn)(edict_t *) = (float (*)(edict_t *))g_EngineFuncsTable[kEng_DropToFloor];
	return fn ? fn(edict) : 0.0f;
}

float EngineWalkMove(edict_t *edict, float yaw, float dist)
{
	float (*fn)(edict_t *, float, float) = (float (*)(edict_t *, float, float))g_EngineFuncsTable[kEng_WalkMove];
	return fn ? fn(edict, yaw, dist) : 0.0f;
}

void EngineMoveToOrigin(edict_t *edict, const float *goal, float dist, int moveType)
{
	void (*fn)(edict_t *, const float *, float, int) =
		(void (*)(edict_t *, const float *, float, int))g_EngineFuncsTable[kEng_MoveToOrigin];
	if (fn)
		fn(edict, goal, dist, moveType);
}

void EngineChangeYaw(edict_t *edict)
{
	void (*fn)(edict_t *) = (void (*)(edict_t *))g_EngineFuncsTable[kEng_ChangeYaw];
	if (fn)
		fn(edict);
}

void EngineMakeVectors(const float *angles)
{
	void (*fn)(const float *) = (void (*)(const float *))g_EngineFuncsTable[kEng_MakeVectors];
	if (fn)
		fn(angles);
}

void EngineGetAimVector(edict_t *edict, float speed, float *vecReturn)
{
	void (*fn)(edict_t *, float, float *) =
		(void (*)(edict_t *, float, float *))g_EngineFuncsTable[kEng_GetAimVector];
	if (fn)
		fn(edict, speed, vecReturn);
}

void EngineVecToAngles(const float *vecIn, float *vecOut)
{
	void (*fn)(const float *, float *) = (void (*)(const float *, float *))g_EngineFuncsTable[kEng_VecToAngles];
	if (fn)
		fn(vecIn, vecOut);
}

int EngineServerCommand(const char *command)
{
	int (*fn)(const char *) = (int (*)(const char *))g_EngineFuncsTable[kEng_ServerCommand];
	return fn ? fn(command) : 0;
}

void EngineClientCommand(edict_t *edict, const char *command)
{
	void (*fn)(edict_t *, const char *) = (void (*)(edict_t *, const char *))g_EngineFuncsTable[kEng_ClientCommand];
	if (fn)
		fn(edict, command);
}

float EngineCvarGetFloat(const char *name)
{
	float (*fn)(const char *) = (float (*)(const char *))g_EngineFuncsTable[kEng_CvarGetFloat];
	return fn ? fn(name) : 0.0f;
}

int EngineIndexOfEdict(edict_t *edict)
{
	int (*fn)(edict_t *) = (int (*)(edict_t *))g_EngineFuncsTable[kEng_IndexOfEdict];
	return fn ? fn(edict) : 0;
}

void *EngineGetModelPtr(edict_t *edict)
{
	void *(*fn)(edict_t *) = (void *(*)(edict_t *))g_EngineFuncsTable[kEng_GetModelPtr];
	return fn ? fn(edict) : NULL;
}

void EngineTraceLine(const float *start, const float *end, int fNoMonsters, edict_t *skip, TraceResult *tr)
{
	void (*fn)(const float *, const float *, int, edict_t *, TraceResult *) =
		(void (*)(const float *, const float *, int, edict_t *, TraceResult *))g_EngineFuncsTable[kEng_TraceLine];
	if (fn)
	{
		// The alpha hl.dll thunk passes (fNoMonsters == 0) into the engine.
		fn(start, end, fNoMonsters == 0, skip, tr);
	}
}

int TraceHitIndex(const TraceResult *tr)
{
	return tr ? tr->pHit : 0;
}

edict_t *TraceHitEdict(const TraceResult *tr)
{
	int index = TraceHitIndex(tr);
	return index ? EnginePEntityOfEntIndex(index) : NULL;
}

void EngineSetOrigin(edict_t *edict, const float *origin)
{
	void (*fn)(edict_t *, const float *) = (void (*)(edict_t *, const float *))g_EngineFuncsTable[kEng_SetOrigin];
	if (fn)
		fn(edict, origin);
}

void EngineSetSize(edict_t *edict, const float *mins, const float *maxs)
{
	void (*fn)(edict_t *, const float *, const float *) =
		(void (*)(edict_t *, const float *, const float *))g_EngineFuncsTable[kEng_SetSize];
	if (fn)
		fn(edict, mins, maxs);
}

void EngineWriteByte(int msgDest, int value)
{
	void (*fn)(int, int) = (void (*)(int, int))g_EngineFuncsTable[kEng_WriteByte];
	if (fn)
		fn(msgDest, value);
}

void EngineWriteShort(int msgDest, int value)
{
	void (*fn)(int, int) = (void (*)(int, int))g_EngineFuncsTable[kEng_WriteShort];
	if (fn)
		fn(msgDest, value);
}

void EngineWriteCoord(int msgDest, float value)
{
	void (*fn)(int, float) = (void (*)(int, float))g_EngineFuncsTable[kEng_WriteCoord];
	if (fn)
		fn(msgDest, value);
}

void EngineParticleEffect(const float *origin, const float *direction, float color, float count)
{
	void (*fn)(const float *, const float *, float, float) =
		(void (*)(const float *, const float *, float, float))g_EngineFuncsTable[kEng_ParticleEffect];
	if (fn)
		fn(origin, direction, color, count);
}

void EngineLightStyle(int style, const char *pattern)
{
	void (*fn)(int, const char *) = (void (*)(int, const char *))g_EngineFuncsTable[kEng_LightStyle];
	if (fn)
		fn(style, pattern);
}

void EngineDecalIndex(int index, const char *name)
{
	void (*fn)(int, const char *) = (void (*)(int, const char *))g_EngineFuncsTable[kEng_DecalIndex];
	if (fn)
		fn(index, name);
}

int EnginePointContents(const float *point)
{
	int (*fn)(const float *) = (int (*)(const float *))g_EngineFuncsTable[kEng_PointContents];
	return fn ? fn(point) : 0;
}

void EngineCvarSetString(const char *name, const char *value)
{
	void (*fn)(const char *, const char *) = (void (*)(const char *, const char *))g_EngineFuncsTable[kEng_CvarSetString];
	if (fn)
		fn(name, value);
}

void EngineEmitAmbientSound(const float *origin, const char *sample, float volume, float attenuation)
{
	void (*fn)(const float *, const char *, float, float) =
		(void (*)(const float *, const char *, float, float))g_EngineFuncsTable[kEng_EmitAmbientSound];
	if (fn)
		fn(origin, sample, volume, attenuation);
}

void EngineChangePitch(edict_t *edict)
{
	void (*fn)(edict_t *) = (void (*)(edict_t *))g_EngineFuncsTable[kEng_ChangePitch];
	if (fn)
		fn(edict);
}

void EngineMakeStatic(edict_t *edict)
{
	void (*fn)(edict_t *) = (void (*)(edict_t *))g_EngineFuncsTable[kEng_MakeStatic];
	if (fn)
		fn(edict);
}

int EngineModelIndex(int entIndex)
{
	int (*fn)(int) = (int (*)(int))g_EngineFuncsTable[kEng_ModelIndex];
	return fn ? fn(entIndex) : 0;
}

void EngineAlertMessage(int level, const char *fmt, ...)
{
	void (*fn)(int, const char *, ...) = (void (*)(int, const char *, ...))g_EngineFuncsTable[kEng_AlertMessage];
	if (!fn)
		return;

	char buffer[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	fn(level, "%s", buffer);
}
