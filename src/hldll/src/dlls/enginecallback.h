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
#ifndef ENGINECALLBACK_H
#define ENGINECALLBACK_H

// Filled in by GiveFnptrsToDll
extern enginefuncs_t g_engfuncs;

// The actual engine callbacks
#define PRECACHE_MODEL			(*g_engfuncs.pfnPrecacheModel)
#define PRECACHE_SOUND			(*g_engfuncs.pfnPrecacheSound)
#define SET_MODEL				(*g_engfuncs.pfnSetModel)
#define MODEL_INDEX				(*g_engfuncs.pfnModelIndex)
#define SET_SIZE				(*g_engfuncs.pfnSetSize)
#define CHANGE_LEVEL			(*g_engfuncs.pfnChangeLevel)
#define GET_SPAWN_PARMS			(*g_engfuncs.pfnGetSpawnParms)
#define SAVE_SPAWN_PARMS		(*g_engfuncs.pfnSaveSpawnParms)
#define VEC_TO_YAW				(*g_engfuncs.pfnVecToYaw)
#define VEC_TO_ANGLES			(*g_engfuncs.pfnVecToAngles)
#define MOVE_TO_GOAL			(*g_engfuncs.pfnMoveToGoal)
#define MOVE_TO_ORIGIN			(*g_engfuncs.pfnMoveToOrigin)
#define CHANGE_YAW				(*g_engfuncs.pfnChangeYaw)
#define CHANGE_PITCH			(*g_engfuncs.pfnChangePitch)
#define FIND_ENTITY_BY_STRING	(*g_engfuncs.pfnFindEntityByString)
#define GETENTITYILLUM			(*g_engfuncs.pfnGetEntityIllum)
#define FIND_ENTITY_IN_SPHERE	(*g_engfuncs.pfnFindEntityInSphere)
#define FIND_CLIENT_IN_PVS		(*g_engfuncs.pfnFindClientInPVS)
#define MAKE_VECTORS			(*g_engfuncs.pfnMakeVectors)
#define CREATE_ENTITY			(*g_engfuncs.pfnCreateEntity)
#define REMOVE_ENTITY			(*g_engfuncs.pfnRemoveEntity)
#define MAKE_STATIC				(*g_engfuncs.pfnMakeStatic)
#define ENT_IS_ON_FLOOR			(*g_engfuncs.pfnEntIsOnFloor)
#define DROP_TO_FLOOR			(*g_engfuncs.pfnDropToFloor)
#define WALK_MOVE				(*g_engfuncs.pfnWalkMove)
#define SET_ORIGIN				(*g_engfuncs.pfnSetOrigin)
#define EMIT_SOUND				(*g_engfuncs.pfnEmitSound)
#define EMIT_AMBIENT_SOUND		(*g_engfuncs.pfnEmitAmbientSound)
#define TRACE_LINE				(*g_engfuncs.pfnTraceLine)
#define TRACE_TOSS				(*g_engfuncs.pfnTraceToss)
#define GET_AIM_VECTOR			(*g_engfuncs.pfnGetAimVector)
#define SERVER_COMMAND			(*g_engfuncs.pfnServerCommand)
#define CLIENT_COMMAND			(*g_engfuncs.pfnClientCommand)
#define PARTICLE_EFFECT			(*g_engfuncs.pfnParticleEffect)
#define LIGHT_STYLE				(*g_engfuncs.pfnLightStyle)
#define DECAL_SET_NAME			(*g_engfuncs.pfnDecalSetName)
#define POINT_CONTENTS			(*g_engfuncs.pfnPointContents)
#define WRITE_BYTE				(*g_engfuncs.pfnWriteByte)
#define WRITE_CHAR				(*g_engfuncs.pfnWriteChar)
#define WRITE_SHORT				(*g_engfuncs.pfnWriteShort)
#define WRITE_LONG				(*g_engfuncs.pfnWriteLong)
#define WRITE_ANGLE				(*g_engfuncs.pfnWriteAngle)
#define WRITE_COORD				(*g_engfuncs.pfnWriteCoord)
#define WRITE_STRING			(*g_engfuncs.pfnWriteString)
#define WRITE_ENTITY			(*g_engfuncs.pfnWriteEntity)
#define CVAR_GET_FLOAT			(*g_engfuncs.pfnCVarGetFloat)
#define CVAR_GET_STRING			(*g_engfuncs.pfnCVarGetString)
#define CVAR_SET_FLOAT			(*g_engfuncs.pfnCVarSetFloat)
#define CVAR_SET_STRING			(*g_engfuncs.pfnCVarSetString)
#define ALERT					(*g_engfuncs.pfnAlertMessage)
#define ALLOC_PRIVATE			(*g_engfuncs.pfnPvAllocEntPrivateData)
#define GET_PRIVATE				(*g_engfuncs.pfnPvEntPrivateData)
#define FREE_PRIVATE			(*g_engfuncs.pfnFreeEntPrivateData)
#define ALLOC_STRING			(*g_engfuncs.pfnAllocString)
#define FIND_ENTITY_BY_VARS		(*g_engfuncs.pfnFindEntityByVars)
#define GET_MODEL_PTR			(*g_engfuncs.pfnGetModelPtr)

#endif // ENGINECALLBACK_H
