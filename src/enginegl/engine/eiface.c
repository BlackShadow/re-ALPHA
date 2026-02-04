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
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <io.h>

#include "quakedef.h"
#include "eiface.h"

#define MAX_GAME_DLLS 50

extern void *g_rgextdll[MAX_GAME_DLLS];
extern int g_iextdllcount;
extern char Buffer[1024];


extern int			PF_precache_model_internal(const char *model);
extern void			PF_precache_sound_internal(const char *sound);
extern void			PF_setmodel_I(edict_t *e, const char *m);
extern int			PF_modelindex(const char *m);
extern void			PF_setsize_I(edict_t *e, float *min, float *max);
extern int			SV_EntityChangeLevelCallback(const char *level, const char *startspot);
extern int			PF_setspawnparms(edict_t *ent);
extern int			ED_EntVarsToCl(edict_t *ent);
extern float		VectorToYaw(float *value1);
extern void			VectorAngles(float *forward, float *angles);
extern int			SV_MoveToGoal_step(edict_t *ent, float dist);
extern float		SV_MoveToGoal_internal(edict_t *self, float *goalPos, float dist, int chase);
extern void			SV_ChangeYaw(edict_t *ent);
extern void			SV_ChangePitch(edict_t *ent);
extern edict_t		*FindEntityByString(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue);
extern int			GetEntityIllum(edict_t *pEnt);
extern edict_t		*FindEntityInSphere(vec3_t org, float rad);


extern edict_t *PF_checkclient_I(void);
extern void PF_makevectors_I(float *angles);
extern edict_t *PF_Spawn_I(void);
extern void PF_Remove_I(edict_t *ed);
extern void PF_makestatic_I(edict_t *ent);
extern int PF_checkbottom_I(edict_t *ent);
extern float PF_droptofloor_I(edict_t *ent);
extern float PF_walkmove_I(edict_t *ent, float yaw, float dist);
extern void PF_setorigin_I(edict_t *e, float *org);
extern void PF_sound_I(edict_t *entity, int channel, const char *sample, float volume, float attenuation);
extern void PF_ambientsound_I(float *pos, const char *samp, float vol, float attenuation);
extern void PF_traceline_DLL(float *v1, float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
extern void PF_TraceToss_DLL(edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr);
extern void PF_aim_I(edict_t *ent, float speed, float *dir);
extern void PF_localcmd_I(char *str);
extern void PF_stuffcmd_I(edict_t *pEdict, char *szFmt, ...);
extern void SV_StartParticle(float *org, float *dir, float color, float count);
extern void SV_BroadcastLightStyle(int style_num, const char *style_str);
extern char *SV_DecalSetName(int decal, const char *name);
extern int PF_pointcontents_I(vec3_t p);


extern void MSG_WriteByte_Dest(int dest, int value);
extern void MSG_WriteChar_Dest(int dest, int value);
extern void MSG_WriteShort_Dest(int dest, int value);
extern void MSG_WriteLong_Dest(int dest, int value);
extern void MSG_WriteAngle_Dest(int dest, float value);
extern void MSG_WriteCoord_Dest(int dest, float value);
extern void MSG_WriteString_Dest(int dest, const char *s);
extern void MSG_WriteEntity_Dest(int dest, int entnum);
extern float ED_GetCvarValue(const char *cvarname);
extern const char *ED_GetCvarString(const char *cvarname);
extern void ED_SetCvarValue(const char *cvarname, float value);
extern void ED_SetCvarString(const char *cvarname, const char *value);
extern int EngineFprintf(FILE *Stream, const char *Format, ...);
extern void *ED_AllocPrivateData(edict_t *ent, int size);
extern void *ED_GetPrivateData(edict_t *ent);
extern void ED_FreePrivateData(edict_t *ent);
extern void *ED_GetDispatch(void *ent, int callback_type);
extern void ED_AssertMethodNotChanged(void);
extern void *EDICT_TO_ENTVAR(edict_t *ent);
extern int EDICT_INDEX(int offset);
extern edict_t *ENTVAR_TO_EDICT(void *entvar);
extern int ED_FindModel(edict_t *ent);
extern void AlertMessage(int alert_type, const char *fmt, ...);

static void (*enginefuncs[64])() = {
	(void (*)())PF_precache_model_internal,
	(void (*)())PF_precache_sound_internal,
	(void (*)())PF_setmodel_I,
	(void (*)())PF_modelindex,
	(void (*)())PF_setsize_I,
	(void (*)())SV_EntityChangeLevelCallback,
	(void (*)())PF_setspawnparms,
	(void (*)())ED_EntVarsToCl,
	(void (*)())VectorToYaw,
	(void (*)())VectorAngles,
	(void (*)())SV_MoveToGoal_step,
	(void (*)())SV_MoveToGoal_internal,
	(void (*)())SV_ChangeYaw,
	(void (*)())SV_ChangePitch,
	(void (*)())FindEntityByString,
	(void (*)())GetEntityIllum,
	(void (*)())FindEntityInSphere,
	(void (*)())PF_checkclient_I,
	(void (*)())PF_makevectors_I,
	(void (*)())PF_Spawn_I,
	(void (*)())PF_Remove_I,
	(void (*)())PF_makestatic_I,
	(void (*)())PF_checkbottom_I,
	(void (*)())PF_droptofloor_I,
	(void (*)())PF_walkmove_I,
	(void (*)())PF_setorigin_I,
	(void (*)())PF_sound_I,
	(void (*)())PF_ambientsound_I,
	(void (*)())PF_traceline_DLL,
	(void (*)())PF_TraceToss_DLL,
	(void (*)())PF_aim_I,
	(void (*)())PF_localcmd_I,
	(void (*)())PF_stuffcmd_I,
	(void (*)())SV_StartParticle,
	(void (*)())SV_BroadcastLightStyle,
	(void (*)())SV_DecalSetName,
	(void (*)())PF_pointcontents_I,
	(void (*)())MSG_WriteByte_Dest,
	(void (*)())MSG_WriteChar_Dest,
	(void (*)())MSG_WriteShort_Dest,
	(void (*)())MSG_WriteLong_Dest,
	(void (*)())MSG_WriteAngle_Dest,
	(void (*)())MSG_WriteCoord_Dest,
	(void (*)())MSG_WriteString_Dest,
	(void (*)())MSG_WriteEntity_Dest,
	(void (*)())ED_GetCvarValue,
	(void (*)())ED_GetCvarString,
	(void (*)())ED_SetCvarValue,
	(void (*)())ED_SetCvarString,
	(void (*)())AlertMessage,
	(void (*)())EngineFprintf,
	(void (*)())ED_AllocPrivateData,
	(void (*)())ED_GetPrivateData,
	(void (*)())ED_FreePrivateData,
	(void (*)())ED_GetDispatch,
	(void (*)())ED_AssertMethodNotChanged,
	(void (*)())PROG_TO_STRING,
	(void (*)())ED_AllocString,
	(void (*)())EDICT_TO_ENTVAR,
	(void (*)())PROG_TO_EDICT,
	(void (*)())EDICT_TO_PROG,
	(void (*)())EDICT_INDEX,
	(void (*)())ENTVAR_TO_EDICT,
	(void (*)())ED_FindModel,
};

static char g_alertBuffer[1024];

static void *g_entityfuncs[9];

void *g_pfnDispatchSpawn;
void *g_pfnDispatchThink;
void *g_pfnDispatchUse;
void *g_pfnDispatchTouch;
void *g_pfnDispatchSave;
void *g_pfnDispatchRestore;
void *g_pfnDispatchKeyValue;
void *g_pfnDispatchBlocked;

static const char *g_ExtDLLProcNames[] = {
	"ClientDisconnect",
	"PlayerPreThink",
	"PlayerPostThink",
	"StartFrame",
	"SetNewParms",
	"SetChangeParms",
	"ClientKill",
	"ClientConnect",
	"PutClientInServer",
};

extern void Con_Printf(const char *fmt, ...);
extern void Sys_Error(const char *error, ...);

void *GetDispatch(const char *pszProcName)
{
	int i;
	FARPROC pfn;

	if (g_iextdllcount <= 0)
	{
		Con_Printf("Can't find proc: %s\n", pszProcName);
		return NULL;
	}

	for (i = 0; i < g_iextdllcount; i++)
	{
		pfn = GetProcAddress(g_rgextdll[i], pszProcName);
		if (pfn)
			return (void *)pfn;
	}

	Con_Printf("Can't find proc: %s\n", pszProcName);
	return NULL;
}

void *GetEntityInit(const char *pszClassName)
{
	return GetDispatch(pszClassName);
}

void *LoadThisDll(const char *szDLLPath)
{
	HMODULE hLib;
	FARPROC pfnGiveFnptrsToDll;
	FARPROC result;
	int i;

	hLib = LoadLibraryA(szDLLPath);
	if (!hLib)
	{
		Con_Printf("LoadLibrary failed on %s\n", szDLLPath);
		return NULL;
	}

	pfnGiveFnptrsToDll = GetProcAddress(hLib, "GiveFnptrsToDll");
	if (!pfnGiveFnptrsToDll)
	{
		Con_Printf("Couldn't get GiveFnptrsToDll in %s\n", szDLLPath);
		FreeLibrary(hLib);
		return NULL;
	}

	((void(__stdcall *)(void **))pfnGiveFnptrsToDll)((void **)enginefuncs);

	if (g_iextdllcount == MAX_GAME_DLLS)
	{
		Con_Printf("Too many DLLs, ignoring remainder\n");
		FreeLibrary(hLib);
		return NULL;
	}

	g_rgextdll[g_iextdllcount] = NULL;
	g_rgextdll[g_iextdllcount] = hLib;
	g_iextdllcount++;

	result = NULL;
	for (i = 0; i < 9; i++)
	{
		if (!g_entityfuncs[i])
		{
			result = GetProcAddress(hLib, g_ExtDLLProcNames[i]);
			if (result)
				g_entityfuncs[i] = (void *)result;
		}
	}

	return (void *)result;
}

int LoadEntityDLLs(const char *szBasePath)
{
	struct _finddata_t finddata;
	intptr_t hFind;
	char szSearchPath[260];
	char szDLLPath[260];

	memset(g_entityfuncs, 0, sizeof(g_entityfuncs));
	g_iextdllcount = 0;
	memset(g_rgextdll, 0, sizeof(g_rgextdll));

	sprintf(szSearchPath, "%s\\%s\\*.dll", szBasePath, "valve\\dlls");

	hFind = _findfirst(szSearchPath, &finddata);
	if (hFind != -1)
	{
		do
		{
			sprintf(szDLLPath, "%s\\%s\\%s", szBasePath, "valve\\dlls", finddata.name);
			LoadThisDll(szDLLPath);
		} while (!_findnext(hFind, &finddata));
	}
	_findclose(hFind);

	g_pfnDispatchSpawn = GetDispatch("DispatchSpawn");
	g_pfnDispatchThink = GetDispatch("DispatchThink");
	g_pfnDispatchUse = GetDispatch("DispatchUse");
	g_pfnDispatchTouch = GetDispatch("DispatchTouch");
	g_pfnDispatchSave = GetDispatch("DispatchSave");
	g_pfnDispatchRestore = GetDispatch("DispatchRestore");
	g_pfnDispatchKeyValue = GetDispatch("DispatchKeyValue");
	g_pfnDispatchBlocked = GetDispatch("DispatchBlocked");

	if (!g_pfnDispatchSpawn ||
		!g_pfnDispatchThink ||
		!g_pfnDispatchUse ||
		!g_pfnDispatchTouch ||
		!g_pfnDispatchSave ||
		!g_pfnDispatchRestore ||
		!g_pfnDispatchKeyValue ||
		!g_pfnDispatchBlocked)
	{
		Sys_Error("Can't get all dispatchfunctions!");
	}

	return 0;
}

int UnloadEntityDLLs(void)
{
	int result;
	int i;

	result = g_iextdllcount;
	for (i = 0; i < g_iextdllcount; i++)
	{
		result = FreeLibrary(g_rgextdll[i]);
		g_rgextdll[i] = NULL;
	}

	return result;
}

int EngineFprintf(FILE *Stream, const char *Format, ...)
{
	va_list va;

	va_start(va, Format);
	vsprintf(Buffer, Format, va);
	va_end(va);

	return fprintf(Stream, Buffer);
}

void AlertMessage(int atype, const char *fmt, ...)
{
	double devValue;
	UINT uType;
	HWND activeWindow;
	va_list va;

	va_start(va, fmt);

	devValue = developer.value;
	uType = 0;

	if (developer.value != 0.0f)
	{
		if (atype)
		{
			switch (atype)
			{
				case 1:
					g_alertBuffer[0] = 0;
					break;
				case 2:
					strcpy(g_alertBuffer, "WARNING:  ");
					uType = 48;
					break;
				case 3:
					strcpy(g_alertBuffer, "ERROR:  ");
					uType = 16;
					break;
			}
		}
		else
		{
			strcpy(g_alertBuffer, "NOTE:  ");
			uType = 64;
		}

		vsprintf(g_alertBuffer + strlen(g_alertBuffer), fmt, va);

		if (atype == 1 || (ED_GetCvarValue("vid_mode"), devValue > 2.0))
		{
			Con_Printf(g_alertBuffer);
		}
		else
		{
			activeWindow = GetActiveWindow();
			if (activeWindow)
				MessageBoxA(activeWindow, g_alertBuffer, "Alert", uType);
		}
	}

	va_end(va);
}

void DispatchEntityCallback(int callbackIndex)
{
	int (__stdcall *pfn)(globalvars_t *);
	edict_t *self;

	pfn = (int (__stdcall *)(globalvars_t *))g_entityfuncs[callbackIndex];
	if (pfn)
	{
		self = (edict_t *)((byte *)sv_edicts + pr_global_struct->self);
		self->v.pContainingEntity = self;
		self->v.pSystemGlobals = pr_global_struct;

		pfn(pr_global_struct);
		return;
	}

	switch (callbackIndex)
	{
		case 0:
			PR_ExecuteProgram(pr_global_struct->ClientDisconnect);
			break;
		case 1:
			PR_ExecuteProgram(pr_global_struct->PlayerPreThink);
			break;
		case 2:
			PR_ExecuteProgram(pr_global_struct->PlayerPostThink);
			break;
		case 3:
			PR_ExecuteProgram(pr_global_struct->StartFrame);
			break;
		case 4:
			PR_ExecuteProgram(pr_global_struct->SetNewParms);
			break;
		case 5:
			PR_ExecuteProgram(pr_global_struct->SetChangeParms);
			break;
		case 6:
			PR_ExecuteProgram(pr_global_struct->ClientKill);
			break;
		case 7:
			PR_ExecuteProgram(pr_global_struct->ClientConnect);
			break;
		case 8:
			PR_ExecuteProgram(pr_global_struct->PutClientInServer);
			break;
		default:
			break;
	}
}
