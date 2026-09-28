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
// eiface.c -- loading the game DLLs and the engine functions handed to them

#include <windows.h>
#include <io.h>

#include "quakedef.h"
#include "eiface.h"

#define GAME_DLL_DIR	"valve\\dlls"

// handed to every game DLL by GiveFnptrsToDll, in this order
static void (*enginefuncs[])() =
{
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
	(void (*)())ED_FindModel
};

static char g_alertBuffer[1024];

static void *g_entityfuncs[NUM_ENTITYFUNCS];

void *g_pfnDispatchSpawn;
void *g_pfnDispatchThink;
void *g_pfnDispatchUse;
void *g_pfnDispatchTouch;
void *g_pfnDispatchSave;
void *g_pfnDispatchRestore;
void *g_pfnDispatchKeyValue;
void *g_pfnDispatchBlocked;

// game DLL exports that replace the progs functions of the same name,
// in ENTITYFUNC_* order
static const char *g_ExtDLLProcNames[NUM_ENTITYFUNCS] =
{
	"ClientDisconnect",
	"PlayerPreThink",
	"PlayerPostThink",
	"StartFrame",
	"SetNewParms",
	"SetChangeParms",
	"ClientKill",
	"ClientConnect",
	"PutClientInServer"
};

/*
==============
GetDispatch

Returns the first export of that name found in the loaded game DLLs.
==============
*/
void *GetDispatch(const char *pszProcName)
{
	int		i;
	FARPROC	pfn;

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

/*
==============
GetEntityInit

Each entity class is exported from the game DLL under its classname.
==============
*/
void *GetEntityInit(const char *pszClassName)
{
	return GetDispatch(pszClassName);
}

/*
==============
LoadThisDll

Loads a game DLL, hands it the engine functions and picks up the entity
functions no earlier DLL provided.
==============
*/
void *LoadThisDll(const char *szDLLPath)
{
	HMODULE	hDLL;
	FARPROC	pfnGiveFnptrsToDll;
	FARPROC	pfn;
	int		i;

	hDLL = LoadLibraryA(szDLLPath);
	if (!hDLL)
	{
		Con_Printf("LoadLibrary failed on %s\n", szDLLPath);
		return NULL;
	}

	pfnGiveFnptrsToDll = GetProcAddress(hDLL, "GiveFnptrsToDll");
	if (!pfnGiveFnptrsToDll)
	{
		Con_Printf("Couldn't get GiveFnptrsToDll in %s\n", szDLLPath);
		FreeLibrary(hDLL);
		return NULL;
	}

	((void (__stdcall *)(void **))pfnGiveFnptrsToDll)((void **)enginefuncs);

	if (g_iextdllcount == MAX_GAME_DLLS)
	{
		Con_Printf("Too many DLLs, ignoring remainder\n");
		FreeLibrary(hDLL);
		return NULL;
	}

	g_rgextdll[g_iextdllcount] = hDLL;
	g_iextdllcount++;

	pfn = NULL;
	for (i = 0; i < NUM_ENTITYFUNCS; i++)
	{
		if (!g_entityfuncs[i])
		{
			pfn = GetProcAddress(hDLL, g_ExtDLLProcNames[i]);
			if (pfn)
				g_entityfuncs[i] = (void *)pfn;
		}
	}

	return (void *)pfn;
}

/*
==============
LoadEntityDLLs

Loads every DLL in the game DLL directory and looks up the Dispatch functions.
==============
*/
int LoadEntityDLLs(const char *szBasePath)
{
	struct _finddata_t	findData;
	intptr_t			hFind;
	char				szSearchPath[MAX_PATH];
	char				szDLLPath[MAX_PATH];

	memset(g_entityfuncs, 0, sizeof(g_entityfuncs));
	g_iextdllcount = 0;
	memset(g_rgextdll, 0, sizeof(g_rgextdll));

	sprintf(szSearchPath, "%s\\%s\\*.dll", szBasePath, GAME_DLL_DIR);

	hFind = _findfirst(szSearchPath, &findData);
	if (hFind != -1)
	{
		do
		{
			sprintf(szDLLPath, "%s\\%s\\%s", szBasePath, GAME_DLL_DIR, findData.name);
			LoadThisDll(szDLLPath);
		} while (!_findnext(hFind, &findData));
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

	if (!g_pfnDispatchSpawn
		|| !g_pfnDispatchThink
		|| !g_pfnDispatchUse
		|| !g_pfnDispatchTouch
		|| !g_pfnDispatchSave
		|| !g_pfnDispatchRestore
		|| !g_pfnDispatchKeyValue
		|| !g_pfnDispatchBlocked)
	{
		Sys_Error("Can't get all dispatchfunctions!");
	}

	return 0;
}

int UnloadEntityDLLs(void)
{
	int		result;
	int		i;

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
	va_list	argptr;

	va_start(argptr, Format);
	vsprintf(Buffer, Format, argptr);
	va_end(argptr);

	return fprintf(Stream, Buffer);
}

/*
==============
AlertMessage

Developer messages from the game DLL: at_console goes to the console,
the others to a message box unless developer is above 2.
==============
*/
void AlertMessage(ALERT_TYPE atype, const char *fmt, ...)
{
	va_list	argptr;
	double	dev;
	UINT	uType;
	HWND	hWnd;

	va_start(argptr, fmt);

	dev = developer.value;
	uType = 0;

	if (developer.value)
	{
		if (atype != at_notice)
		{
			switch (atype)
			{
			case at_console:
				g_alertBuffer[0] = 0;
				break;
			case at_warning:
				strcpy(g_alertBuffer, "WARNING:  ");
				uType = MB_ICONWARNING;
				break;
			case at_error:
				strcpy(g_alertBuffer, "ERROR:  ");
				uType = MB_ICONERROR;
				break;
			default:
				break;
			}
		}
		else
		{
			strcpy(g_alertBuffer, "NOTE:  ");
			uType = MB_ICONINFORMATION;
		}

		vsprintf(g_alertBuffer + strlen(g_alertBuffer), fmt, argptr);

		// vid_mode is read but not used
		if (atype == at_console || (ED_GetCvarValue("vid_mode"), dev > 2))
		{
			Con_Printf(g_alertBuffer);
		}
		else
		{
			hWnd = GetActiveWindow();
			if (hWnd)
				MessageBoxA(hWnd, g_alertBuffer, "Alert", uType);
		}
	}

	va_end(argptr);
}

/*
==============
DispatchEntityCallback

Runs a game DLL entity function if one was loaded, the progs function otherwise.
==============
*/
void DispatchEntityCallback(int callbackIndex)
{
	int		(__stdcall *pfn)(globalvars_t *);
	edict_t	*self;
	int		self_ofs;
	int		max_self_ofs;

	if (callbackIndex < 0 || callbackIndex >= NUM_ENTITYFUNCS)
		return;

	pfn = (int (__stdcall *)(globalvars_t *))g_entityfuncs[callbackIndex];
	if (pfn)
	{
		if (!pr_global_struct)
			return;

		// make sure self is a valid edict and linked back to its entvars
		if (sv.edicts && sv.num_edicts > 0 && pr_edict_size > 0)
		{
			self_ofs = pr_global_struct->self;
			max_self_ofs = sv.num_edicts * pr_edict_size;
			if (self_ofs < 0 || self_ofs >= max_self_ofs)
			{
				self_ofs = 0;
				pr_global_struct->self = 0;
			}

			self = (edict_t *)((byte *)sv.edicts + self_ofs);
			self->v.pContainingEntity = self;
			self->v.pSystemGlobals = pr_global_struct;
		}

		pfn(pr_global_struct);
		return;
	}

	switch (callbackIndex)
	{
	case ENTITYFUNC_CLIENTDISCONNECT:
		PR_ExecuteProgram(pr_global_struct->ClientDisconnect);
		break;
	case ENTITYFUNC_PLAYERPRETHINK:
		PR_ExecuteProgram(pr_global_struct->PlayerPreThink);
		break;
	case ENTITYFUNC_PLAYERPOSTTHINK:
		PR_ExecuteProgram(pr_global_struct->PlayerPostThink);
		break;
	case ENTITYFUNC_STARTFRAME:
		PR_ExecuteProgram(pr_global_struct->StartFrame);
		break;
	case ENTITYFUNC_SETNEWPARMS:
		PR_ExecuteProgram(pr_global_struct->SetNewParms);
		break;
	case ENTITYFUNC_SETCHANGEPARMS:
		PR_ExecuteProgram(pr_global_struct->SetChangeParms);
		break;
	case ENTITYFUNC_CLIENTKILL:
		PR_ExecuteProgram(pr_global_struct->ClientKill);
		break;
	case ENTITYFUNC_CLIENTCONNECT:
		PR_ExecuteProgram(pr_global_struct->ClientConnect);
		break;
	case ENTITYFUNC_PUTCLIENTINSERVER:
		PR_ExecuteProgram(pr_global_struct->PutClientInServer);
		break;
	}
}
