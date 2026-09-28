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

// host.c -- coordinates spawning and killing of local servers

#include "quakedef.h"
#include "nullsubs.h"

#define MAX_HOST_CLIENTS		16	// -dedicated / -listen player limit
#define DEFAULT_HOST_CLIENTS	8	// -dedicated / -listen without a count
#define MIN_CLIENT_SLOTS		4	// client_t structs always allocated

jmp_buf host_abortserver;

cvar_t host_framerate = {"host_framerate", "0", false, false};	// set for slow motion
cvar_t host_speeds = {"host_speeds", "0", false, false};		// set for running times
cvar_t sys_ticrate = {"sys_ticrate", "0.05", false, false};
cvar_t serverprofile = {"serverprofile", "0", false, false};
cvar_t fraglimit = {"fraglimit", "0", false, true};
cvar_t timelimit = {"timelimit", "0", false, true};
cvar_t teamplay = {"teamplay", "0", false, true};
cvar_t samelevel = {"samelevel", "0", false, false};
cvar_t noexit = {"noexit", "0", false, true};
cvar_t skill = {"skill", "1", false, false};				// 0 - 3
cvar_t deathmatch = {"deathmatch", "0", false, false};		// 0, 1, or 2
cvar_t coop = {"coop", "0", false, false};					// 0 or 1
cvar_t pausable = {"pausable", "1", false, false};
cvar_t developer = {"developer", "0", false, false};

/*
================
Host_FindMaxClients
================
*/
static void Host_FindMaxClients(void)
{
	int i;

	svs.maxclients = 1;

	i = COM_CheckParm("-dedicated");
	if (i)
	{
		cls.state = ca_dedicated;
		if (i == (com_argc - 1))
			svs.maxclients = DEFAULT_HOST_CLIENTS;
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
		if (cls.state == ca_dedicated)
			Sys_Error("Only one of -dedicated or -listen can be specified");

		if (i == (com_argc - 1))
			svs.maxclients = DEFAULT_HOST_CLIENTS;
		else
			svs.maxclients = Q_atoi(com_argv[i + 1]);
	}

	if (svs.maxclients < 1)
		svs.maxclients = DEFAULT_HOST_CLIENTS;
	else if (svs.maxclients > MAX_HOST_CLIENTS)
		svs.maxclients = MAX_HOST_CLIENTS;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclients < MIN_CLIENT_SLOTS)
		svs.maxclientslimit = MIN_CLIENT_SLOTS;

	svs.clients = Hunk_AllocName(svs.maxclientslimit * sizeof(*svs.clients), "clients");

	if (svs.maxclients <= 1)
		Cvar_SetValue("deathmatch", 0.0f);
	else
		Cvar_SetValue("deathmatch", 1.0f);
}

/*
================
Host_InitLocal
================
*/
static void Host_InitLocal(void)
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

/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
static void Host_WriteConfiguration(void)
{
	FILE *f;

	if (cls.state == ca_dedicated || !host_initialized)
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

/*
===================
Host_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
static void Host_GetConsoleCommands(void)
{
	char *cmd;

	while (1)
	{
		cmd = Sys_ConsoleInput();
		if (!cmd)
			break;
		Cbuf_AddText(cmd);
	}
}

/*
==================
Host_ServerFrame
==================
*/
static void Host_ServerFrame(void)
{
	pr_global_struct->frametime = (float)host_frametime;

	// clear the general datagram
	SV_ClearDatagram();

	// check for new clients
	SV_CheckForNewClients();

	// read client messages
	SV_RunClients();

	// move things around and think; always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
		SV_Physics();

	// send all messages to the clients
	SV_SendClientMessages();
}

/*
==================
Host_ClientFrame
==================
*/
void Host_ClientFrame(void)
{
}

/*
==================
Host_ClientCommands_Null
==================
*/
void Host_ClientCommands_Null(void)
{
}

/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
static qboolean Host_FilterTime(float time)
{
	double newtime;

	newtime = realtime + time;

	if (cls.demoplayback)
	{
		realtime = newtime;
	}
	else
	{
		realtime = newtime;
		if (newtime - oldrealtime < 1.0 / 72)
			return false;	// framerate is too high
	}

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	if (host_framerate.value <= 0.0)
	{
		// don't allow really long or short frames
		if (host_frametime > 0.1)
			host_frametime = 0.1;
		if (host_frametime < 0.001)
			host_frametime = 0.001;
	}
	else
	{
		host_frametime = host_framerate.value;
	}

	return true;
}

/*
==================
Host_Frame

Runs all active servers
==================
*/
void Host_Frame(float time)
{
	static double time1 = 0;
	static double time2 = 0;
	static double time3 = 0;
	int pass1, pass2, pass3;

	if (setjmp(host_abortserver))
		return;		// something bad happened, or the server disconnected

	// keep the random time dependent
	rand();

	// decide the simulation time
	if (!Host_FilterTime(time))
		return;		// don't run too fast, or packets will flood out

	// get new key events
	Sys_SendKeyEvents();

	V_CalcRefdef();

	// process console commands
	Cbuf_Execute();

	NET_Poll();

	// if running the server locally, make intentions now
	Host_GetConsoleCommands();

	if (sv.active)
	{
		CL_SendCmd();
		Host_ServerFrame();

		// if running the server remotely, send intentions now after
		// the incoming messages have been read
		if (!sv.active)
			CL_SendCmd();
	}
	else
	{
		CL_SendCmd();
	}

	// fetch results from server
	if (cls.state == ca_connected)
		CL_ReadFromServer();

	CAM_Think();

	// update video
	if (host_speeds.value)
		time1 = Sys_FloatTime();

	SCR_UpdateScreen();

	if (cl.movie_recording)
		VID_WriteBuffer(NULL);

	if (host_speeds.value)
		time2 = Sys_FloatTime();

	// update audio
	if (cls.signon == SIGNONS)
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

/*
====================
Host_Init
====================
*/
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
			parms->memsize / (float)(1024 * 1024));

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

	if (cls.state != ca_dedicated)
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
		R_Init();
		S_Init();
		CDAudio_Init();
		HUD_Init();
		CL_Init();
		IN_RegisterCvars();
	}

	Cbuf_InsertText("exec valve.rc\n");

	Hunk_AllocName(0, "-MARK-");
	host_hunklevel = Hunk_LowMark();

	host_initialized = true;

	Sys_Printf("========Half-Life Initialized=========\n");
}

/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error. It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
	static qboolean isdown = false;

	if (isdown)
	{
		printf("recursive shutdown\n");
		return;
	}
	isdown = true;

	// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration();

	CDAudio_Shutdown();
	NET_Shutdown();
	S_Shutdown();
	IN_Shutdown();

	if (cls.state != ca_dedicated)
		VID_Shutdown();
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error(const char *error, ...)
{
	va_list argptr;
	char string[1024];
	static qboolean inerror = false;

	if (inerror)
		Sys_Error("Host_Error: recursively entered");
	inerror = true;

	SCR_EndLoadingPlaque();		// reenable screen updates

	va_start(argptr, error);
	vsprintf(string, error, argptr);
	va_end(argptr);

	Con_Printf("Host_Error: %s\n", string);

	if (sv.active)
		Host_ShutdownServer(false);

	if (cls.state == ca_dedicated)
		Sys_Error("Host_Error: %s\n", string);	// dedicated servers exit

	CL_Disconnect();
	cls.demonum = -1;

	inerror = false;

	longjmp(host_abortserver, 1);
}

/*
================
Host_EndGame
================
*/
void Host_EndGame(const char *message, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, message);
	vsprintf(string, message, argptr);
	va_end(argptr);

	Con_DPrintf("Host_EndGame: %s\n", string);

	if (sv.active)
		Host_ShutdownServer(false);

	if (cls.state == ca_dedicated)
		Sys_Error("Host_EndGame: %s\n", string);	// dedicated servers exit

	if (cls.demonum != -1)
		CL_NextDemo();
	else
		CL_Disconnect();

	longjmp(host_abortserver, 1);
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(qboolean crash)
{
	int i;
	int count;
	server_client_t *client;
	double start;
	sizebuf_t buf;
	byte message[4];

	if (!sv.active)
		return;

	sv.active = false;

	// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect_f();

	// flush any pending messages - like the score!!!
	start = Sys_FloatTime();

	do
	{
		count = 0;
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
					count++;
				}
			}
		}
	}
	while (Sys_FloatTime() - start <= 3.0 && count);

	// make sure all the clients know we're disconnecting
	buf.data = message;
	buf.maxsize = sizeof(message);
	buf.cursize = 0;
	MSG_WriteByte(&buf, svc_disconnect);

	count = NET_SendToAll(&buf, 5);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (client->active)
		{
			host_client = client;
			SV_DropClient(crash);
		}
	}

	// clear structures
	memset(&sv, 0, sizeof(sv));
	memset(svs.clients, 0, svs.maxclientslimit * sizeof(*svs.clients));
}

/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory(void)
{
	Con_DPrintf("Clearing memory\n");
	S_AmbientOn_Null();
	Mod_ClearAll();
	if (host_hunklevel)
		Hunk_FreeToLowMark(host_hunklevel);

	cls.signon = 0;
	memset(&sv, 0, sizeof(sv));
	memset(&cl, 0, sizeof(cl));

	memset(cl_model_precache, 0, sizeof(cl_model_precache));
	memset(cl_sound_precache, 0, sizeof(cl_sound_precache));
	cl_worldmodel = NULL;
	cl_scores = NULL;
	cl_time = 0.0;
	cl_oldtime = 0.0;
	cl_mtime[0] = 0.0;
	cl_mtime[1] = 0.0;
}
