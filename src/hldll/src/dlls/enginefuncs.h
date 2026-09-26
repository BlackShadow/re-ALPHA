#pragma once

// Transitional wrappers for code not yet converted to the enginecallback.h
// macros. Removed once every file calls the engine through the macros.

#include "extdll.h"
#include "util.h"

inline int EnginePrecacheModel(const char* name) { return PRECACHE_MODEL(name); }
inline void EnginePrecacheSound(const char* name) { PRECACHE_SOUND(name); }
inline void EngineSetModel(edict_t* edict, const char* model) { SET_MODEL(edict, model); }
inline float EngineVecToYaw(const float* vec) { return VEC_TO_YAW(vec); }
inline void EngineChangeLevel(const char* s1, const char* s2) { CHANGE_LEVEL(s1, s2); }
inline edict_t* EngineFindEntityByString(edict_t* pStart, const char* pszField, const char* pszValue) { return FIND_ENTITY_BY_STRING(pStart, pszField, pszValue); }
inline edict_t* EngineFindEntityInSphere(const float* origin, float radius) { return FIND_ENTITY_IN_SPHERE(origin, radius); }
inline edict_t* EngineFindClientInPVS() { return FIND_CLIENT_IN_PVS(); }
inline edict_t* EnginePEntityOfEntIndex(int index) { return ENT(index); }
inline int EngineIndexOfEdict(edict_t* edict) { return OFFSET(edict); }
inline entvars_t* EngineGetVarsOfEnt(edict_t* edict) { return VARS(edict); }
inline void* EngineGetPrivateData(edict_t* edict) { return GET_PRIVATE(edict); }
inline void* EngineAllocPrivateData(edict_t* edict, int size) { return ALLOC_PRIVATE(edict, size); }
inline edict_t* EngineCreateEntity() { return CREATE_ENTITY(); }
inline void EngineRemoveEntity(edict_t* edict) { REMOVE_ENTITY(edict); }
inline const char* EngineStringFromIndex(int index) { return STRING(index); }
inline int EngineAllocString(const char* str) { return ALLOC_STRING(str); }
inline void EngineEmitSound(edict_t* entity, int channel, const char* sample, float volume, float attenuation) { EMIT_SOUND(entity, channel, sample, volume, attenuation); }
inline void EngineGetSpawnParms(edict_t* edict) { GET_SPAWN_PARMS(edict); }
inline void EngineSaveSpawnParms(edict_t* edict) { SAVE_SPAWN_PARMS(edict); }
inline float EngineDropToFloor(edict_t* edict) { return DROP_TO_FLOOR(edict); }
inline float EngineWalkMove(edict_t* edict, float yaw, float dist) { return WALK_MOVE(edict, yaw, dist); }
inline void EngineMoveToOrigin(edict_t* edict, const float* goal, float dist, int moveType) { MOVE_TO_ORIGIN(edict, goal, dist, moveType); }
inline void EngineChangeYaw(edict_t* edict) { CHANGE_YAW(edict); }
inline void EngineMakeVectors(const float* angles) { MAKE_VECTORS(angles); }
inline void EngineGetAimVector(edict_t* edict, float speed, float* vecReturn) { GET_AIM_VECTOR(edict, speed, vecReturn); }
inline void EngineVecToAngles(const float* vecIn, float* vecOut) { VEC_TO_ANGLES(vecIn, vecOut); }
inline void EngineServerCommand(const char* command) { SERVER_COMMAND(command); }
inline void EngineClientCommand(edict_t* edict, const char* command) { CLIENT_COMMAND(edict, command); }
inline float EngineCvarGetFloat(const char* name) { return CVAR_GET_FLOAT(name); }
inline void* EngineGetModelPtr(edict_t* edict) { return GET_MODEL_PTR(edict); }
inline void EngineTraceLine(const float* start, const float* end, int fNoMonsters, edict_t* skip, TraceResult* tr) { TRACE_LINE(start, end, fNoMonsters == 0, skip, tr); }
inline int TraceHitIndex(const TraceResult* tr) { return tr ? tr->pHit : 0; }
inline edict_t* TraceHitEdict(const TraceResult* tr) { int index = TraceHitIndex(tr); return index ? ENT(index) : NULL; }
inline void EngineSetOrigin(edict_t* edict, const float* origin) { SET_ORIGIN(edict, origin); }
inline void EngineSetSize(edict_t* edict, const float* mins, const float* maxs) { SET_SIZE(edict, mins, maxs); }
inline void EngineWriteByte(int msgDest, int value) { WRITE_BYTE(msgDest, value); }
inline void EngineWriteShort(int msgDest, int value) { WRITE_SHORT(msgDest, value); }
inline void EngineWriteCoord(int msgDest, float value) { WRITE_COORD(msgDest, value); }
inline void EngineParticleEffect(const float* origin, const float* direction, float color, float count) { PARTICLE_EFFECT(origin, direction, color, count); }
inline void EngineLightStyle(int style, const char* pattern) { LIGHT_STYLE(style, pattern); }
inline void EngineDecalIndex(int index, const char* name) { DECAL_SET_NAME(index, name); }
inline int EnginePointContents(const float* point) { return POINT_CONTENTS(point); }
inline void EngineCvarSetString(const char* name, const char* value) { CVAR_SET_STRING(name, value); }
inline void EngineEmitAmbientSound(const float* origin, const char* sample, float volume, float attenuation) { EMIT_AMBIENT_SOUND(origin, sample, volume, attenuation); }
inline void EngineChangePitch(edict_t* edict) { CHANGE_PITCH(edict); }
inline void EngineMakeStatic(edict_t* edict) { MAKE_STATIC(edict); }
inline int EngineModelIndex(int entIndex) { return ENTINDEX(entIndex); }

#define EngineAlertMessage(level, ...) ALERT((ALERT_TYPE)(level), __VA_ARGS__)
