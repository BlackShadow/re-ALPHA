#pragma once

#include "hl_types.h"
#include "../public/edict.h"
#include "../public/eiface.h"

void EngineCopyTable(enginefuncs_t* pEngineFuncs);

int EnginePrecacheModel(const char* name);
int EnginePrecacheSound(const char* name);
void EngineSetModel(edict_t* edict, const char* model);
float EngineVecToYaw(const float* vec);
void EngineChangeLevel(const char* s1, const char* s2);
edict_t* EngineFindEntityByString(edict_t* pStart, const char* pszField, const char* pszValue);
edict_t* EngineFindEntityInSphere(const float* origin, float radius);
edict_t* EngineFindClientInPVS();

edict_t* EnginePEntityOfEntIndex(int index);
int EngineIndexOfEdict(edict_t* edict);
entvars_t* EngineGetVarsOfEnt(edict_t* edict);
void* EngineGetPrivateData(edict_t* edict);
void* EngineAllocPrivateData(edict_t* edict, int size);
edict_t* EngineCreateEntity();
int EngineRemoveEntity(edict_t* edict);
const char* EngineStringFromIndex(int index);
int EngineAllocString(const char* str);
void EngineEmitSound(edict_t* entity, int channel, const char* sample, float volume, float attenuation);
void EngineGetSpawnParms(edict_t* edict);
void EngineSaveSpawnParms(edict_t* edict);
float EngineDropToFloor(edict_t* edict);
float EngineWalkMove(edict_t* edict, float yaw, float dist);
void EngineMoveToOrigin(edict_t* edict, const float* goal, float dist, int moveType);
void EngineChangeYaw(edict_t* edict);
void EngineMakeVectors(const float* angles);
void EngineGetAimVector(edict_t* edict, float speed, float* vecReturn);
void EngineVecToAngles(const float* vecIn, float* vecOut);
int EngineServerCommand(const char* command);
void EngineClientCommand(edict_t* edict, const char* command);
float EngineCvarGetFloat(const char* name);
void* EngineGetModelPtr(edict_t* edict);
void EngineTraceLine(const float* start, const float* end, int fNoMonsters, edict_t* skip, TraceResult* tr);
int TraceHitIndex(const TraceResult* tr);
edict_t* TraceHitEdict(const TraceResult* tr);
void EngineSetOrigin(edict_t* edict, const float* origin);
void EngineSetSize(edict_t* edict, const float* mins, const float* maxs);

void EngineWriteByte(int msgDest, int value);
void EngineWriteShort(int msgDest, int value);
void EngineWriteCoord(int msgDest, float value);

void EngineParticleEffect(const float* origin, const float* direction, float color, float count);

void EngineLightStyle(int style, const char* pattern);
void EngineDecalIndex(int index, const char* name);
int EnginePointContents(const float* point);
void EngineCvarSetString(const char* name, const char* value);
void EngineEmitAmbientSound(const float* origin, const char* sample, float volume, float attenuation);
void EngineChangePitch(edict_t* edict);
void EngineMakeStatic(edict_t* edict);
int EngineModelIndex(int entIndex);
void EngineAlertMessage(int level, const char* fmt, ...);
