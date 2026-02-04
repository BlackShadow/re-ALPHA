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

#include "quakedef.h"
#include "nullsubs.h"

#define cls_signon		cls.signon
#define cls_state		cls.state
#define cls_demonum		cls.demonum
#define cls_demos		cls.demos
#define cls_netchan		cls.netchan
#define cls_demoplayback cls.demoplayback
#define cls_demorecording cls.demorecording
#define cl_nointerp		cl.nointerp
#define svs_clients		svs.clients
#define svs_maxclients	svs.maxclients
#define svs_maxclientslimit svs.maxclientslimit

#include "server.h"

#define SAVEGAME_VERSION    16

jmp_buf host_abortserver;

cvar_t host_framerate = {"host_framerate", "0", false, false};
cvar_t host_speeds = {"host_speeds", "0", false, false};
cvar_t sys_ticrate = {"sys_ticrate", "0.05", false, false};
cvar_t serverprofile = {"serverprofile", "0", false, false};
cvar_t fraglimit = {"fraglimit", "0", false, true};
cvar_t timelimit = {"timelimit", "0", false, true};
cvar_t teamplay = {"teamplay", "0", false, true};
cvar_t samelevel = {"samelevel", "0", false, false};
cvar_t noexit = {"noexit", "0", false, true};
cvar_t skill = {"skill", "1", false, false};
cvar_t deathmatch = {"deathmatch", "0", false, false};
cvar_t coop = {"coop", "0", false, false};
cvar_t pausable = {"pausable", "1", false, false};
cvar_t developer = {"developer", "0", false, false};

extern int current_skill;
extern int scr_disabled_for_loading;
extern byte *host_colormap;

#define SAVEGAME_MAGIC      0x56414853

extern int cl_stats_monsters;
extern int cl_stats_totalmonsters;

extern void Mod_Print(void);
extern void SV_SaveSpawnparms(void);
extern void SV_ClearServerState(void);
extern void V_CalcRefdef(void);
extern int R_InitBrushLighting(void);
extern int scr_copyeverything;

char *Host_GetConsoleCommands(void);

void Host_FindMaxClients(void)
{
	int i;

	svs.maxclients = 1;

	i = COM_CheckParm("-dedicated");
	if (i)
	{
		cls.state = ca_dedicated;
		if (i == (com_argc - 1))
			svs.maxclients = 8;
		else
			svs.maxclients = Q_atoi(com_argv[i + 1]);
	}
	else
	{
		cls.state = ca_disconnected;
	}

	i = COM_CheckParm("-listen");
	if (i)
	{
		if (!cls.state)
			Sys_Error("Only one of -dedicated or -listen can be specified");

		if (i == (com_argc - 1))
			svs.maxclients = 8;
		else
			svs.maxclients = Q_atoi(com_argv[i + 1]);
	}

	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > 16)
		svs.maxclients = 16;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclients < 4)
		svs.maxclientslimit = 4;

	svs.clients = Hunk_AllocName(svs.maxclientslimit * sizeof(*svs.clients), "clients");

	if (svs.maxclients <= 1)
		Cvar_SetValue("deathmatch", 0.0f);
	else
		Cvar_SetValue("deathmatch", 1.0f);
}

void Host_InitLocal(void)
{
	Host_InitCommands();

	Cvar_RegisterVariable(&host_framerate);
	Cvar_RegisterVariable(&host_speeds);
	Cvar_RegisterVariable(&sys_ticrate);
	Cvar_RegisterVariable(&serverprofile);

	Cvar_RegisterVariable(&fraglimit);
	Cvar_RegisterVariable(&timelimit);
	Cvar_RegisterVariable(&teamplay);
	Cvar_RegisterVariable(&samelevel);
	Cvar_RegisterVariable(&noexit);
	Cvar_RegisterVariable(&skill);
	Cvar_RegisterVariable(&deathmatch);
	Cvar_RegisterVariable(&coop);
	Cvar_RegisterVariable(&pausable);
	Cvar_RegisterVariable(&developer);

	Host_FindMaxClients();
}

void Host_WriteConfiguration(void)
{
	FILE *f;

	if (cls_state == ca_dedicated || !host_initialized)
		return;

	f = fopen(va("%s/config.cfg", com_gamedir), "w");
	if (!f)
	{
		Con_Printf("Couldn't write config.cfg.\n");
		return;
	}

	Key_WriteBindings(f);
	Cvar_WriteVariables(f);

	fclose(f);
}

void Host_ServerFrame(void)
{

	pr_global_struct->frametime = (float)host_frametime;

	SV_ClearDatagram();
	SV_CheckForNewClients();
	SV_RunClients();

	if (!sv.paused && (svs.maxclients > 1 || !key_dest))
		SV_Physics();

	SV_SendClientMessages();
}

void Host_ClientFrame(void)
{

}

//=========================================================
// Host_ClientCommands_Null - empty stub
//=========================================================
void Host_ClientCommands_Null(void)
{
}

qboolean Host_FilterTime(float time)
{
	double newtime;

	newtime = realtime + time;

	if (cls_demoplayback)
	{
		realtime = newtime;
	}
	else
	{
		realtime = newtime;
		if (newtime - oldrealtime < 0.01388888888888889)
			return 0;
	}

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	if (host_framerate.value <= 0.0)
	{
		if (host_frametime > 0.1)
			host_frametime = 0.1;
		if (host_frametime < 0.001)
			host_frametime = 0.001;
	}
	else
	{
		host_frametime = host_framerate.value;
	}

	return 1;
}

void Host_Frame(float time)
{
	static double time1 = 0;
	static double time2 = 0;
	static double time3 = 0;
	int pass1, pass2, pass3;

	if (setjmp(host_abortserver))
		return;

	rand();

	if (!Host_FilterTime(time))
		return;

	Sys_SendKeyEvents();

	V_CalcRefdef();
	Cbuf_Execute();

	NET_Poll();

	Host_GetConsoleCommands();

	if (sv.active)
	{
		CL_SendCmd();
		Host_ServerFrame();
		if (!sv.active)
			CL_SendCmd();
	}
	else
	{
		CL_SendCmd();
	}

	if (cls.state == ca_connected)
		CL_ReadFromServer();

	CAM_Think();

	if (host_speeds.value)
		time1 = Sys_FloatTime();

	SCR_UpdateScreen();

	if (cl.movie_recording)
		VID_WriteBuffer(NULL);

	if (host_speeds.value)
		time2 = Sys_FloatTime();

	if (cls_signon == SIGNONS)
	{
		S_Update(r_origin, vpn, vright, vup);
		CL_DecayLights();
	}
	else
	{
		S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);
	}

	CDAudio_Update();

	if (host_speeds.value)
	{
		pass1 = (time1 - time3) * 1000;
		time3 = Sys_FloatTime();
		pass2 = (time2 - time1) * 1000;
		pass3 = (time3 - time2) * 1000;
		Con_Printf("%3i tot %3i server %3i gfx %3i snd\n",
			pass1 + pass2 + pass3, pass1, pass2, pass3);
	}

	host_framecount++;
}

void Host_Init(quakeparms_t *parms)
{
	if (standard_quake)
		minimum_memory = MINIMUM_MEMORY;
	else
		minimum_memory = MINIMUM_MEMORY_LEVELPAK;

	if (COM_CheckParm("-minmemory"))
		parms->memsize = minimum_memory;

	host_parms = *parms;

	if (parms->memsize < minimum_memory)
		Sys_Error("Only %4.1f megs of memory available, can't execute game",
			parms->memsize / (float)0x100000);

	Memory_Init(parms->membase, parms->memsize);
	Cbuf_Init();
	Cmd_Init();
	V_Init();
	COM_Init();
	Host_InitLocal();
	W_LoadWadFile("gfx.wad");
	Key_Init();
	Con_Init();
	M_Init();
	ED_Init();
	Mod_Init();
	NET_Init();
	SV_Init();

	Con_DPrintf("Exe: " __TIME__ " " __DATE__ "\n");
	Con_DPrintf("%4.1f megabyte heap\n", parms->memsize / (1024 * 1024.0));

	R_InitNoTexture();

	if (cls_state != 0)
	{
		host_basepal = COM_LoadHunkFile("gfx/palette.lmp");
		if (!host_basepal)
			Sys_Error("Couldn't load gfx/palette.lmp");
		host_colormap = COM_LoadHunkFile("gfx/colormap.lmp");
		if (!host_colormap)
			Sys_Error("Couldn't load gfx/colormap.lmp");

		VID_Init();
		Draw_Init();
		SCR_Init();
		R_InitBrushLighting();
		S_Init();
		CDAudio_Init();
		HUD_Init();
		CL_Init();
		IN_RegisterCvars();
	}

	Cbuf_InsertText("exec valve.rc\n");

	Hunk_AllocName(0, "-MARK-");
	host_hunklevel = Hunk_LowMark();

	host_initialized = 1;

	Sys_Printf("========Half-Life Initialized=========\n");
}

void Host_Shutdown(void)
{
	static qboolean isdown = 0;

	if (isdown)
	{
		printf("recursive shutdown\n");
		return;
	}
	isdown = 1;

	scr_disabled_for_loading = 1;

	Host_WriteConfiguration();

	CDAudio_Shutdown();
	NET_Shutdown();
	S_Shutdown();
	IN_Shutdown();

	if (cls_state != 0)
		VID_Shutdown();
}

void Host_Error(const char *error, ...)
{
	va_list argptr;
	char string[1024];
	static qboolean inerror = 0;

	if (inerror)
		Sys_Error("Host_Error: recursively entered");
	inerror = 1;

	SCR_EndLoadingPlaque();

	va_start(argptr, error);
	vsprintf(string, error, argptr);
	va_end(argptr);

	Con_Printf("Host_Error: %s\n", string);

	if (sv.active)
		Host_ShutdownServer(0);

	if (cls_state == 0)
		Sys_Error("Host_Error: %s\n", string);

	CL_Disconnect();
	cls_demonum = -1;

	inerror = 0;

	longjmp(host_abortserver, 1);
}

void Host_EndGame(const char *message, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, message);
	vsprintf(string, message, argptr);
	va_end(argptr);

	Con_DPrintf("Host_EndGame: %s\n", string);

	if (sv.active)
		Host_ShutdownServer(0);

	if (cls_state == 0)
		Sys_Error("Host_EndGame: %s\n", string);

	if (cls_demonum != -1)
		CL_NextDemo();
	else
		CL_Disconnect();

	longjmp(host_abortserver, 1);
}

void Host_ShutdownServer(qboolean crash)
{
	int i;
	int blocked;
	server_client_t *client;
	double start;
	sizebuf_t buf;
	byte message[4];

	if (!sv.active)
		return;

	sv.active = 0;

	if (cls_state == 2)
		CL_Disconnect_f();

	start = Sys_FloatTime();

	do
	{
		blocked = 0;
		for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
		{
			if (client->active && client->spawned)
			{
				if (NET_CanSendMessage(client->netconnection))
				{
					NET_SendMessage(client->netconnection, &client->message);
					SZ_Clear(&client->message);
				}
				else
				{
					NET_GetMessage(client->netconnection);
					blocked++;
				}
			}
		}
	}
	while (Sys_FloatTime() - start <= 3.0 && blocked);

	buf.data = message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte(&buf, 2);

	i = NET_SendToAll(&buf, 5);
	if (i)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", i);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (client->active)
		{
			host_client = client;
			SV_DropClient(crash);
		}
	}

	memset(&sv, 0, 0x3638);
	memset(svs.clients, 0, svs_maxclientslimit * sizeof(*svs.clients));
}

int Host_ClearMemory(void)
{
	Con_DPrintf("Clearing memory\n");
	S_AmbientOn_Null();
	Mod_ClearAll();
	if (host_hunklevel)
		Hunk_FreeToLowMark(host_hunklevel);

	cls_signon = 0;
	memset(&sv, 0, 0x3638);
	memset(&cl, 0, 0xBF8);

	memset(cl_model_precache, 0, sizeof(cl_model_precache));
	memset(cl_sound_precache, 0, sizeof(cl_sound_precache));
	cl_worldmodel = NULL;
	cl_scores = NULL;
	cl_time = 0.0;
	cl_oldtime = 0.0;
	cl_mtime[0] = 0.0;
	cl_mtime[1] = 0.0;

	return 0;
}

char *Host_GetConsoleCommands(void)
{
	char *cmd;

	while (1)
	{
		cmd = Sys_ConsoleInput();
		if (!cmd)
			break;
		Cbuf_AddText(cmd);
	}

	return cmd;
}
