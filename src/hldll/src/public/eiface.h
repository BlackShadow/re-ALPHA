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
#ifndef EIFACE_H
#define EIFACE_H

#include "progdefs.h"

typedef enum
{
	at_notice,		// "NOTE:" message box in developer mode
	at_console,		// plain console text
	at_warning,		// "WARNING:" message box in developer mode
	at_error,		// "ERROR:" message box in developer mode
} ALERT_TYPE;

typedef struct
{
	int			fAllSolid;		// if true, plane is not valid
	int			fStartSolid;	// if true, the initial point was in a solid area
	int			fInOpen;
	int			fInWater;
	float		flFraction;		// time completed, 1.0 = didn't hit anything
	Vector		vecEndPos;		// final position
	float		flPlaneDist;
	Vector		vecPlaneNormal;	// surface normal at impact
	EOFFSET		pHit;			// entity the surface is on
} TraceResult;

typedef struct
{
	const char	*szClassName;
	const char	*szKeyName;
	const char	*szValue;
	int			fHandled;
} KeyValueData;

typedef struct saverestore_s SAVERESTOREDATA;

// Engine functions handed to the DLL by GiveFnptrsToDll, in engine table order.
typedef struct enginefuncs_s
{
	int			(*pfnPrecacheModel)			(const char *s);
	void		(*pfnPrecacheSound)			(const char *s);
	void		(*pfnSetModel)				(edict_t *e, const char *m);
	int			(*pfnModelIndex)			(const char *m);
	void		(*pfnSetSize)				(edict_t *e, const float *rgflMin, const float *rgflMax);
	void		(*pfnChangeLevel)			(const char *s1, const char *s2);
	void		(*pfnGetSpawnParms)			(edict_t *ent);
	void		(*pfnSaveSpawnParms)		(edict_t *ent);
	float		(*pfnVecToYaw)				(const float *rgflVector);
	void		(*pfnVecToAngles)			(const float *rgflVectorIn, float *rgflVectorOut);
	int			(*pfnMoveToGoal)			(edict_t *ent, float dist);
	void		(*pfnMoveToOrigin)			(edict_t *ent, const float *pflGoal, float dist, int iMoveType);
	void		(*pfnChangeYaw)				(edict_t *ent);
	void		(*pfnChangePitch)			(edict_t *ent);
	edict_t*	(*pfnFindEntityByString)	(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue);
	int			(*pfnGetEntityIllum)		(edict_t *pEnt);
	edict_t*	(*pfnFindEntityInSphere)	(const float *org, float rad);
	edict_t*	(*pfnFindClientInPVS)		(void);
	void		(*pfnMakeVectors)			(const float *rgflVector);
	edict_t*	(*pfnCreateEntity)			(void);
	void		(*pfnRemoveEntity)			(edict_t *e);
	void		(*pfnMakeStatic)			(edict_t *ent);
	int			(*pfnEntIsOnFloor)			(edict_t *e);
	float		(*pfnDropToFloor)			(edict_t *e);
	float		(*pfnWalkMove)				(edict_t *ent, float yaw, float dist);
	void		(*pfnSetOrigin)				(edict_t *e, const float *rgflOrigin);
	void		(*pfnEmitSound)				(edict_t *entity, int channel, const char *sample, float volume, float attenuation);
	void		(*pfnEmitAmbientSound)		(const float *pos, const char *samp, float vol, float attenuation);
	void		(*pfnTraceLine)				(const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
	void		(*pfnTraceToss)				(edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr);
	void		(*pfnGetAimVector)			(edict_t *ent, float speed, float *rgflReturn);
	void		(*pfnServerCommand)			(const char *str);
	void		(*pfnClientCommand)			(edict_t *pEdict, const char *szFmt, ...);
	void		(*pfnParticleEffect)		(const float *org, const float *dir, float color, float count);
	void		(*pfnLightStyle)			(int style, const char *val);
	void		(*pfnDecalSetName)			(int decal, const char *name);
	int			(*pfnPointContents)			(const float *rgflVector);
	void		(*pfnWriteByte)				(int msg_dest, int iValue);
	void		(*pfnWriteChar)				(int msg_dest, int iValue);
	void		(*pfnWriteShort)			(int msg_dest, int iValue);
	void		(*pfnWriteLong)				(int msg_dest, int iValue);
	void		(*pfnWriteAngle)			(int msg_dest, float flValue);
	void		(*pfnWriteCoord)			(int msg_dest, float flValue);
	void		(*pfnWriteString)			(int msg_dest, const char *sz);
	void		(*pfnWriteEntity)			(int msg_dest, int iValue);
	float		(*pfnCVarGetFloat)			(const char *szVarName);
	const char*	(*pfnCVarGetString)			(const char *szVarName);
	void		(*pfnCVarSetFloat)			(const char *szVarName, float flValue);
	void		(*pfnCVarSetString)			(const char *szVarName, const char *szValue);
	void		(*pfnAlertMessage)			(ALERT_TYPE atype, const char *szFmt, ...);
	void		(*pfnEngineFprintf)			(void *pfile, const char *szFmt, ...);
	void*		(*pfnPvAllocEntPrivateData)	(edict_t *pEdict, int cb);
	void*		(*pfnPvEntPrivateData)		(edict_t *pEdict);
	void		(*pfnFreeEntPrivateData)	(edict_t *pEdict);
	void*		(*pfnGetDispatch)			(void *pEdict, int callbackType);
	void		(*pfnAssertMethodNotChanged)(void);
	const char*	(*pfnSzFromIndex)			(int iString);
	int			(*pfnAllocString)			(const char *szValue);
	entvars_t*	(*pfnGetVarsOfEnt)			(edict_t *pEdict);
	edict_t*	(*pfnPEntityOfEntOffset)	(EOFFSET iEntOffset);
	EOFFSET		(*pfnEntOffsetOfPEntity)	(const edict_t *pEdict);
	int			(*pfnIndexOfEntOffset)		(EOFFSET iEntOffset);
	edict_t*	(*pfnFindEntityByVars)		(entvars_t *pvars);
	void*		(*pfnGetModelPtr)			(edict_t *pEdict);
} enginefuncs_t;

#endif // EIFACE_H
