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

// host_cmd.c -- console commands handled by the host

#include <windows.h>

#include "quakedef.h"
#include "eiface.h"

#define ENGINE_VERSION	1.07	// reported by "status" and "version"

#define MAX_DEMONAME	16		// demo loop names are cut to 15 characters
#define NUM_PING_TIMES	16

qboolean noclip_anglehack;

// A single player changelevel with a landmark keeps the player's offset from the
// landmark entity and the view angles, and applies them once the new level spawns.
typedef struct host_landmark_transition_s
{
	qboolean	pending;
	char		name[64];
	vec3_t		offset;
	vec3_t		angles;
} host_landmark_transition_t;

static host_landmark_transition_t host_landmark_transition;

/*
==================
Host_ClearLandmarkTransition
==================
*/
static void Host_ClearLandmarkTransition(void)
{
	host_landmark_transition.pending = false;
	host_landmark_transition.name[0] = 0;
	VectorClear(host_landmark_transition.offset);
	VectorClear(host_landmark_transition.angles);
}

/*
==================
Host_CaptureLandmarkTransition

Called before the old level goes away
==================
*/
static void Host_CaptureLandmarkTransition(const char *landmark)
{
	edict_t *old_landmark;
	edict_t *player;

	if (!landmark || !landmark[0])
		return;

	if (g_iextdllcount <= 0 || svs.maxclients != 1 || !svs.clients[0].active)
		return;

	player = svs.clients[0].edict;
	if (!player || player->free)
		return;

	VectorClear(host_landmark_transition.offset);
	VectorCopy(player->v.v_angle, host_landmark_transition.angles);
	Q_strcpy(host_landmark_transition.name, landmark);
	host_landmark_transition.pending = true;

	old_landmark = FindEntityByString(NULL, "targetname", landmark);
	if (old_landmark && !old_landmark->free)
		VectorSubtract(player->v.origin, old_landmark->v.origin, host_landmark_transition.offset);
}

/*
==================
Host_ApplyLandmarkTransition

Moves the spawned player next to the landmark of the new level
==================
*/
static void Host_ApplyLandmarkTransition(edict_t *ent)
{
	edict_t *new_landmark;
	vec3_t origin;

	if (!host_landmark_transition.pending)
		return;

	if (svs.maxclients != 1 || !ent || ent->free)
	{
		Host_ClearLandmarkTransition();
		return;
	}

	new_landmark = FindEntityByString(NULL, "targetname", host_landmark_transition.name);
	if (new_landmark && !new_landmark->free)
	{
		VectorAdd(new_landmark->v.origin, host_landmark_transition.offset, origin);
		VectorCopy(origin, ent->v.origin);
		VectorCopy(origin, ent->v.oldorigin);
		VectorCopy(host_landmark_transition.angles, ent->v.angles);
		VectorCopy(host_landmark_transition.angles, ent->v.v_angle);
		ent->v.fixangle = 1.0f;
		SV_LinkEdict(ent, false);
	}

	Host_ClearLandmarkTransition();
}

/*
==================
Host_Status_f
==================
*/
static void Host_Status_f(void)
{
	void (*print)(char *fmt, ...);
	char *name;
	int i;
	int hours, minutes, seconds;
	server_client_t *client;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer();
			return;
		}
		print = Con_Printf;
	}
	else
	{
		print = SV_ClientPrintf;
	}

	name = Cvar_VariableString("hostname");
	print("host:    %s\n", name);
	print("version: %4.2f\n", ENGINE_VERSION);
	if (ipxAvailable)
		print("ipx:     %s\n", my_ipx_address);
	if (tcpipAvailable)
		print("tcp/ip:  %s\n", my_tcpip_address);
	print("map:     %s\n", sv.name);
	print("players: %i active (%i max)\n\n", net_activeconnections, svs.maxclients);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;

		seconds = (int)(realtime - client->netconnection->connecttime);
		minutes = seconds / 60;
		if (minutes)
		{
			seconds -= minutes * 60;
			hours = minutes / 60;
			if (hours)
				minutes -= hours * 60;
		}
		else
		{
			hours = 0;
		}

		print("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", i + 1, client->name, (int)client->edict->v.frags, hours, minutes, seconds);
		print("   %s\n", client->netconnection->address);
	}
}

/*
==================
Host_God_f

Sets client to godmode
==================
*/
static void Host_God_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (pr_global_struct->deathmatch == 0.0f || host_client->privileged)
	{
		sv_player->v.flags = (int)sv_player->v.flags ^ FL_GODMODE;
		if ((int)sv_player->v.flags & FL_GODMODE)
			SV_ClientPrintf("godmode ON\n");
		else
			SV_ClientPrintf("godmode OFF\n");
	}
}

/*
==================
Host_Notarget_f
==================
*/
static void Host_Notarget_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (pr_global_struct->deathmatch == 0.0f || host_client->privileged)
	{
		sv_player->v.flags = (int)sv_player->v.flags ^ FL_NOTARGET;
		if ((int)sv_player->v.flags & FL_NOTARGET)
			SV_ClientPrintf("notarget ON\n");
		else
			SV_ClientPrintf("notarget OFF\n");
	}
}

/*
==================
Host_Noclip_f
==================
*/
static void Host_Noclip_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (pr_global_struct->deathmatch == 0.0f || host_client->privileged)
	{
		if (sv_player->v.movetype != MOVETYPE_NOCLIP)
		{
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			noclip_anglehack = true;
			SV_ClientPrintf("noclip ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			noclip_anglehack = false;
			SV_ClientPrintf("noclip OFF\n");
		}
	}
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
static void Host_Fly_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (pr_global_struct->deathmatch == 0.0f || host_client->privileged)
	{
		if (sv_player->v.movetype != MOVETYPE_FLY)
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf("flymode OFF\n");
		}
	}
}

/*
==================
Host_Startdemos_f
==================
*/
static void Host_Startdemos_f(void)
{
	int i, c;

	if (cls.demoplayback)
		return;

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		c = MAX_DEMOS;
		Con_Printf("Max %i demos in demoloop\n", MAX_DEMOS);
	}
	Con_Printf("%i demo(s) in loop\n", c);

	for (i = 1; i < c + 1; i++)
		Q_strncpy(cls.demos[i - 1], Cmd_Argv(i), MAX_DEMONAME - 1);

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback)
	{
		cls.demonum = 0;
		CL_NextDemo();
	}
	else
	{
		cls.demonum = -1;
	}
}

/*
==================
Host_Demos_f

Return to looping demos
==================
*/
static void Host_Demos_f(void)
{
	if (cls.demoplayback)
		return;

	if (cls.demonum == -1)
		cls.demonum = 1;
	CL_Disconnect();
	CL_NextDemo();
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
static void Host_Stopdemo_f(void)
{
	if (!cls.demoplayback)
		return;

	CL_StopPlayback();
	CL_Disconnect();
}

/*
==================
Host_Maps_f

List the .bsp files in the maps directory
==================
*/
static void Host_Maps_f(void)
{
	HANDLE handle;
	WIN32_FIND_DATA finddata;
	char path[64];

	if (cmd_source != src_command)
		return;

	strcpy(path, com_gamedir);
	strcat(path, "\\maps\\");

	if (Cmd_Argc())
		strcat(path, Cmd_Argv(1));

	if (path[strlen(path) - 1] != '\\')
		strcat(path, "\\");

	strcat(path, "*.bsp");

	handle = FindFirstFile(path, &finddata);
	if (handle == INVALID_HANDLE_VALUE)
	{
		Con_Printf("No files at %s\n", path);
		return;
	}

	do
	{
		Con_Printf("     %s\n", finddata.cFileName);
	} while (FindNextFile(handle, &finddata));
}

/*
======================
Host_Map_f

handle a
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
static void Host_Map_f(void)
{
	int i;
	char name[64];

	if (cmd_source != src_command)
		return;

	cls.demonum = -1;		// stop demo loop in case this fails

	CL_Disconnect();
	Host_ShutdownServer(false);

	key_dest = key_game;	// remove console or menu
	SCR_BeginLoadingPlaque();

	svs.serverflags = 0;	// haven't completed an episode yet

	com_cmdline[0] = 0;
	if (Cmd_Argc() > 0)
	{
		for (i = 0; Cmd_Argc() > i; i++)
		{
			strcat(com_cmdline, Cmd_Argv(i));
			strcat(com_cmdline, " ");
		}
	}
	strcat(com_cmdline, "\n");

	strcpy(name, Cmd_Argv(1));
	SV_SpawnServer(name, NULL);

	if (sv.active && cls.state != ca_dedicated)
	{
		com_serveractive[0] = 0;
		for (i = 2; Cmd_Argc() > i; i++)
		{
			strcat(com_serveractive, Cmd_Argv(i));
			strcat(com_serveractive, " ");
		}

		Cbuf_InsertText("connect local\n");
	}
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
static void Host_Restart_f(void)
{
	char mapname[64];
	char startspot[64];
	char *landmark;

	if (cls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;

	strcpy(mapname, sv.name);	// must copy out, because it gets cleared in sv_spawnserver
	strcpy(startspot, sv.startspot);
	landmark = startspot[0] ? startspot : NULL;
	SV_SpawnServer(mapname, landmark);
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
static void Host_Reconnect_f(void)
{
	SCR_BeginLoadingPlaque();
	cls.signon = 0;		// need new connection messages
}

/*
=====================
Host_Connect_f

User command to connect to server
=====================
*/
static void Host_Connect_f(void)
{
	char name[64];

	cls.demonum = -1;		// stop demo loop in case this fails
	if (cls.demoplayback)
	{
		CL_StopPlayback();
		CL_Disconnect();
	}

	strcpy(name, Cmd_Argv(1));
	CL_EstablishConnection(name);
	Host_Reconnect_f();
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
static void Host_Changelevel_f(void)
{
	char level[64];
	char spot[64];
	char *landmark;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("changelevel <levelname> : continue game on a new level\n");
		return;
	}

	if (!sv.active || cls.demoplayback)
	{
		Con_Printf("Only the server may changelevel\n");
		return;
	}

	SCR_BeginLoadingPlaque();
	strcpy(level, Cmd_Argv(1));
	Host_ClearLandmarkTransition();

	landmark = NULL;
	if (Cmd_Argc() > 2)
	{
		strcpy(spot, Cmd_Argv(2));
		if (spot[0])
			landmark = spot;
	}
	Host_CaptureLandmarkTransition(landmark);

	SV_SaveSpawnparms();
	SV_SpawnServer(level, landmark);
}

/*
==================
Host_Changelevel2_f

Changing levels within a unit
==================
*/
static void Host_Changelevel2_f(void)
{
	char level[64];
	char spot[64];
	char *landmark;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("changelevel2 <levelname> : continue game on a new level in the unit\n");
		return;
	}

	if (!sv.active || cls.demoplayback)
	{
		Con_Printf("Only the server may changelevel\n");
		return;
	}

	SCR_BeginLoadingPlaque();
	strcpy(level, Cmd_Argv(1));
	Host_ClearLandmarkTransition();

	landmark = NULL;
	if (Cmd_Argc() > 2)
	{
		strcpy(spot, Cmd_Argv(2));
		if (spot[0])
			landmark = spot;
	}
	Host_CaptureLandmarkTransition(landmark);

	SV_SaveSpawnparms();
	SV_SpawnServer(level, landmark);
}

/*
==================
Host_Ping_f
==================
*/
static void Host_Ping_f(void)
{
	int i, j;
	float total;
	server_client_t *client;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	SV_ClientPrintf("Client ping times:\n");
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;

		total = 0;
		for (j = 0; j < NUM_PING_TIMES; j++)
			total = client->ping_times[j] + total;
		total = total / (double)NUM_PING_TIMES;
		SV_ClientPrintf("%4i %s\n", (int)(total * 1000.0), client->name);
	}
}

/*
==================
Host_Kill_f
==================
*/
static void Host_Kill_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (sv_player->v.health <= 0)
	{
		SV_ClientPrintf("Can't suicide -- already dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	DispatchEntityCallback(ENTITYFUNC_CLIENTKILL);
}

/*
==================
Host_Pause_f
==================
*/
static void Host_Pause_f(void)
{
	char *name;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (!pausable.value)
	{
		SV_ClientPrintf("Pause not allowed.\n");
		return;
	}

	sv.paused ^= 1;

	name = PR_GetString(sv_player->v.netname);
	if (sv.paused)
		SV_BroadcastPrintf("%s paused the game\n", name);
	else
		SV_BroadcastPrintf("%s unpaused the game\n", name);

	// send notification to all clients
	MSG_WriteByte(&sv.reliable_datagram, svc_setpause);
	MSG_WriteByte(&sv.reliable_datagram, sv.paused);
}

//===========================================================================

/*
==================
Host_PreSpawn_f
==================
*/
static void Host_PreSpawn_f(void)
{
	if (cmd_source == src_command)
	{
		SV_ClientPrintf("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		SV_ClientPrintf("prespawn not valid -- already spawned\n");
		return;
	}

	SZ_Write(&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte(&host_client->message, svc_signonnum);
	MSG_WriteByte(&host_client->message, 2);
	host_client->sendsignon = true;
}

/*
==================
Host_Spawn_f
==================
*/
static void Host_Spawn_f(void)
{
	int i;
	server_client_t *client;
	edict_t *ent;

	if (cmd_source == src_command)
	{
		SV_ClientPrintf("Spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		SV_ClientPrintf("Spawn not valid -- already spawned\n");
		return;
	}

	// run the entrance script
	if (sv.loadgame)
	{
		// loaded games are fully inited already
		ent = host_client->edict;

		// the game DLL has no private data for a player loaded from a save:
		// run the connect callbacks to create it, then put the loaded entvars back
		if (g_iextdllcount > 0 && !ED_GetPrivateData(ent))
		{
			entvars_t saved;

			memcpy(&saved, &ent->v, sizeof(saved));
			memcpy(&pr_global_struct->parm1, host_client->spawn_parms, sizeof(host_client->spawn_parms));

			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(ent);
			DispatchEntityCallback(ENTITYFUNC_CLIENTCONNECT);
			DispatchEntityCallback(ENTITYFUNC_PUTCLIENTINSERVER);

			memcpy(&ent->v, &saved, sizeof(saved));
			ent->v.pContainingEntity = ent;
			ent->v.pSystemGlobals = pr_global_struct;
			SV_LinkEdict(ent, false);
		}

		// rebuild the spawn parms, the ones in the save can be stale
		if (g_iextdllcount > 0)
		{
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(ent);
			DispatchEntityCallback(ENTITYFUNC_SETNEWPARMS);
			memcpy(host_client->spawn_parms, &pr_global_struct->parm1, sizeof(host_client->spawn_parms));
		}

		sv.paused = false;
		sv.loadgame = false;
	}
	else
	{
		// set up the edict
		ent = host_client->edict;

		ED_FreePrivateData(ent);
		memset(&ent->v, 0, ((dprograms_t *)progs)->entityfields * 4);

		ent->v.pContainingEntity = ent;
		ent->v.pSystemGlobals = pr_global_struct;
		ent->v.colormap = NUM_FOR_EDICT(ent);
		ent->v.team = (host_client->colors & 15) + 1;
		ent->v.netname = host_client->name - pr_strings;

		// copy spawn parms out of the client_t
		memcpy(&pr_global_struct->parm1, host_client->spawn_parms, sizeof(host_client->spawn_parms));

		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		DispatchEntityCallback(ENTITYFUNC_CLIENTCONNECT);

		if (Sys_FloatTime() - host_client->netconnection->connecttime <= sv.time)
			Sys_Printf("%s entered the game\n", host_client->name);

		DispatchEntityCallback(ENTITYFUNC_PUTCLIENTINSERVER);
	}

	Host_ApplyLandmarkTransition(ent);

	// send all current names, colors, and frag counts
	SZ_Clear(&host_client->message);

	// send time of update
	MSG_WriteByte(&host_client->message, svc_time);
	MSG_WriteFloat(&host_client->message, sv.time);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		MSG_WriteByte(&host_client->message, svc_updatename);
		MSG_WriteByte(&host_client->message, i);
		MSG_WriteString(&host_client->message, client->name);
		MSG_WriteByte(&host_client->message, svc_updatefrags);
		MSG_WriteByte(&host_client->message, i);
		MSG_WriteShort(&host_client->message, client->old_frags);
		MSG_WriteByte(&host_client->message, svc_updatecolors);
		MSG_WriteByte(&host_client->message, i);
		MSG_WriteByte(&host_client->message, client->colors);
	}

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		MSG_WriteByte(&host_client->message, svc_lightstyle);
		MSG_WriteByte(&host_client->message, i);
		MSG_WriteString(&host_client->message, sv.lightstyles[i]);
	}

	//
	// send some stats
	//
	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong(&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong(&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_SECRETS);
	MSG_WriteLong(&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_MONSTERS);
	MSG_WriteLong(&host_client->message, pr_global_struct->killed_monsters);

	//
	// send a fixangle
	// Never send a roll angle, because savegames can catch the server
	// in a state where it is expecting the client to correct the angle
	// and it won't happen if the game was just loaded, so you wind up
	// with a permanent head tilt
	ent = EDICT_NUM((int)(host_client - svs.clients) + 1);
	MSG_WriteByte(&host_client->message, svc_setangle);
	MSG_WriteAngle(&host_client->message, ent->v.angles[PITCH]);
	MSG_WriteAngle(&host_client->message, ent->v.angles[YAW]);
	MSG_WriteAngle(&host_client->message, 0);

	SV_WriteClientdataToMessage(sv_player, &host_client->message);

	MSG_WriteByte(&host_client->message, svc_signonnum);
	MSG_WriteByte(&host_client->message, 3);
	host_client->sendsignon = true;
}

/*
==================
Host_Begin_f
==================
*/
static void Host_Begin_f(void)
{
	if (cmd_source == src_command)
	{
		SV_ClientPrintf("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
}

//===========================================================================

/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
static void Host_Kick_f(void)
{
	char *who;
	char *message;
	server_client_t *save;
	server_client_t *client;
	int i;
	int userid;
	qboolean byNumber;

	message = NULL;
	byNumber = false;

	if (cmd_source == src_client)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else if (sv.time != 0 && !host_client->active)
	{
		return;
	}

	save = host_client;

	if (Cmd_Argc() <= 2 || !Q_strcmp(Cmd_Argv(1), "#"))
	{
		for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
		{
			if (!client->active)
				continue;

			host_client = client;
			if (!Q_strcasecmp(client->name, Cmd_Argv(1)))
				break;
		}
	}
	else
	{
		userid = Q_atof(Cmd_Argv(2)) - 1;
		if (userid < 0 || userid >= svs.maxclients)
			return;

		client = &svs.clients[userid];
		if (!client->active)
			return;

		byNumber = true;
	}

	host_client = client;

	// FIXME: i is not set when kicking by user id
	if (i >= svs.maxclients)
	{
		host_client = save;
		return;
	}

	if (cmd_source == src_client)
	{
		if (cls.state != ca_dedicated)
			who = hostname.string;
		else
			who = "console";
	}
	else
	{
		who = save->name;
	}

	host_client = client;

	// can't kick yourself!
	if (client == save)
		return;

	if (Cmd_Argc() > 2)
	{
		message = COM_Parse(Cmd_Args());
		if (byNumber)
		{
			message++;							// skip the #
			while (*message == ' ')				// skip white space
				message++;
			message += Q_strlen(Cmd_Argv(2));	// skip the number
		}
		while (*message && *message == ' ')
			message++;
	}

	if (message)
		SV_BroadcastPrintf("Kicked by %s: %s\n", who, message);
	else
		SV_BroadcastPrintf("Kicked by %s\n", who);
	SV_DropClient(false);

	host_client = save;
}

/*
==================
Host_Quit_f
==================
*/
static void Host_Quit_f(void)
{
	if (Cmd_Argc() != 1)
		return;

	scr_disabled_for_loading = false;
	scr_drawloading = false;
	scr_con_current = 0;

	Sys_Quit();
}

/*
======================
Host_Name_f
======================
*/
static void Host_Name_f(void)
{
	char *newname;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}

	if (Cmd_Argc() == 2)
		newname = Cmd_Argv(1);
	else
		newname = Cmd_Args();
	newname[15] = 0;	// names are cut to 15 characters

	if (cmd_source == src_command)
	{
		if (Q_strcmp(cl_name.string, newname) == 0)
			return;
		Cvar_Set("_cl_name", newname);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer();
		return;
	}

	if (host_client->name[0] && strcmp(host_client->name, "unconnected"))
	{
		if (Q_strcmp(host_client->name, newname) != 0)
			Con_Printf("%s renamed to %s\n", host_client->name, newname);
	}
	Q_strcpy(host_client->name, newname);
	host_client->edict->v.netname = host_client->name - pr_strings;

	// send notification to all clients
	MSG_WriteByte(&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte(&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString(&sv.reliable_datagram, host_client->name);
}

/*
==================
Host_Version_f
==================
*/
static void Host_Version_f(void)
{
	Con_Printf("Version %4.2f\n", ENGINE_VERSION);
	Con_Printf("Exe: 09:18:27 Sep 4 1998\n");
}

/*
==================
Host_Say
==================
*/
static void Host_Say(qboolean teamonly)
{
	server_client_t *save;
	int i, j;
	char *p;
	char *msg;
	char *name;
	char text[64];
	qboolean fromServer = false;

	if (cmd_source == src_command)
	{
		if (cls.state != ca_dedicated)
		{
			Cmd_ForwardToServer();
			return;
		}
		fromServer = true;
		teamonly = false;
	}

	if (Cmd_Argc() < 2)
		return;

	save = host_client;

	p = Cmd_Args();
	// remove quotes if present
	msg = p;
	if (*p == '"')
	{
		msg = p + 1;
		p[strlen(msg)] = 0;
	}

	name = fromServer ? "Console" : save->name;

	// turn on color set 1
	sprintf(text, "%c%s: ", 1, name);

	// check length & truncate if necessary
	j = sizeof(text) - 2 - strlen(text);	// -2 for /n and null terminator
	if ((int)strlen(msg) > j)
		msg[j] = 0;

	strcat(text, msg);
	strcat(text, "\n");

	for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
	{
		if (!host_client->active || !host_client->spawned)
			continue;
		if (teamplay.value && teamonly && save->edict->v.team != host_client->edict->v.team)
			continue;
		SV_ClientPrintf("%s", text);
	}
	host_client = save;

	Sys_Printf("%s", text);
}

/*
==================
Host_Say_f
==================
*/
static void Host_Say_f(void)
{
	Host_Say(false);
}

/*
==================
Host_Say_Team_f
==================
*/
static void Host_Say_Team_f(void)
{
	Host_Say(true);
}

/*
==================
Host_Tell_f
==================
*/
static void Host_Tell_f(void)
{
	server_client_t *save;
	int i, j;
	char *p;
	char *name;
	char text[64];

	if (cls.state == ca_disconnected)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (Cmd_Argc() < 3)
		return;

	save = host_client;

	Q_strcpy(text, save->name);
	Q_strcat(text, ": ");

	p = Cmd_Args();

	// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

	// check length & truncate if necessary
	j = sizeof(text) - 2 - strlen(text);	// -2 for /n and null terminator
	if ((int)strlen(p) > j)
		p[j] = 0;

	strcat(text, p);
	strcat(text, "\n");

	name = Cmd_Argv(1);
	for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
	{
		if (!host_client->active || !host_client->spawned)
			continue;
		if (Q_strcasecmp(host_client->name, name))
			continue;
		SV_ClientPrintf("%s", text);
		break;
	}
	host_client = save;
}

/*
==================
Host_Color_f
==================
*/
static void Host_Color_f(void)
{
	int top, bottom;
	int playercolor;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"color\" is \"%i %i\"\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 15);
		Con_Printf("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2)
	{
		top = bottom = atoi(Cmd_Argv(1));
	}
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	playercolor = top * 16 + bottom;

	if (cls.state == ca_disconnected)
	{
		Cvar_SetValue("_cl_color", playercolor);
		if (sv.active == 2)
			Cmd_ForwardToServer();
		return;
	}

	host_client->colors = playercolor;
	host_client->edict->v.team = (bottom & 15) + 1;

	// send notification to all clients
	MSG_WriteByte(&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte(&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte(&sv.reliable_datagram, host_client->colors);
}

/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

/*
===============
Host_Loadgame_f
===============
*/
static void Host_Loadgame_f(void)
{
	char name[128];
	FILE *f;
	char mapname[64];
	float time, tfloat;
	char str[32768], *start;
	int i, r;
	edict_t *ent;
	int entnum;
	int version = 0;
	float spawn_parms[NUM_SPAWN_PARMS];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("load <savename> : load a game\n");
		return;
	}

	cls.demonum = -1;		// stop demo loop in case this fails

	sprintf(name, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension(name, ".sav");

	// we can't call SCR_BeginLoadingPlaque, because too much stack space has
	// been used.  The menu calls it before stuffing loadgame command
	Con_Printf("Loading game from %s...\n", name);

	CL_Disconnect();

	f = fopen(name, "r");
	if (!f)
	{
		Con_Printf("ERROR: couldn't open.\n");
		return;
	}

	if (fscanf(f, "%i\n", &version) != 1)
	{
		fclose(f);
		Con_Printf("ERROR: couldn't read savegame header.\n");
		return;
	}

	if (version != SAVEGAME_VERSION)
	{
		fclose(f);
		Con_Printf("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}

	fscanf(f, "%s\n", str);		// comment
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		fscanf(f, "%f\n", &spawn_parms[i]);

	// skill is saved as a float
	fscanf(f, "%f\n", &tfloat);
	current_skill = (int)(tfloat + 0.1);
	Cvar_SetValue("skill", current_skill);

	Cvar_SetValue("deathmatch", 0);
	Cvar_SetValue("coop", 0);
	Cvar_SetValue("teamplay", 0);

	fscanf(f, "%s\n", mapname);
	fscanf(f, "%f\n", &time);

	CL_Disconnect();

	SV_SpawnServer(mapname, NULL);
	if (!sv.active)
	{
		Con_Printf("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

	// load the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		fscanf(f, "%s\n", str);
		sv.lightstyles[i] = Hunk_Alloc(strlen(str) + 1);
		Q_strcpy(sv.lightstyles[i], str);
	}

	// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	while (!feof(f))
	{
		for (i = 0; i < sizeof(str) - 1; i++)
		{
			r = fgetc(f);
			if (r == EOF || !r)
				break;
			str[i] = r;
			if (r == '}')
			{
				i++;
				break;
			}
		}
		if (i == sizeof(str) - 1)
			Sys_Error("Loadgame buffer overflow");
		str[i] = 0;
		start = COM_Parse(str);
		if (!com_token[0])
			break;		// end of file
		if (strcmp(com_token, "{"))
			Sys_Error("First token isn't a brace");

		if (entnum == -1)
		{
			// parse the global vars
			ED_ParseGlobals(start);
			svs.serverflags = pr_global_struct->serverflags;
		}
		else
		{
			// parse an edict
			ent = EDICT_NUM(entnum);

			// with a game DLL the entity keeps its private data
			if (g_iextdllcount <= 0)
				ED_FreePrivateData(ent);
			memset(&ent->v, 0, ((dprograms_t *)progs)->entityfields * 4);
			ent->free = false;
			ED_SetEdictPointers(ent);
			ED_ParseEdict(start, ent);
			ED_SetEdictPointers(ent);

			// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict(ent, false);
			else
				ED_Free(ent);
		}

		entnum++;
	}

	sv.time = time;
	sv.num_edicts = entnum;

	fclose(f);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	if (cls.state != ca_dedicated)
	{
		CL_EstablishConnection("local");
		Host_Reconnect_f();
	}
}

/*
===============
Host_Savegame_WriteGlobals
===============
*/
static void Host_Savegame_WriteGlobals(FILE *f)
{
	ddef_t *def;
	int i;
	int numglobaldefs;
	int type;
	char *name;

	fprintf(f, "{\n");

	numglobaldefs = ((dprograms_t *)progs)->numglobaldefs;
	for (i = 0, def = pr_globaldefs; i < numglobaldefs; i++, def++)
	{
		type = def->type;
		if (!(type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = pr_strings + def->s_name;
		fprintf(f, "\"%s\" \"%s\"\n", name, PR_UglyValueString(type, &pr_globals[def->ofs]));
	}

	fprintf(f, "}\n");
}

/*
===============
Host_Savegame_FunctionRef

Writes a game DLL function pointer as "dll:<index>:<offset>", relative to the
DLL it lives in, so the save still loads when the DLL is at another address.
===============
*/
static qboolean Host_Savegame_FunctionRef(char *dest, int destsize, int func)
{
	MEMORY_BASIC_INFORMATION mbi;
	void *base;
	int i;

	if (!dest || destsize <= 0)
		return false;

	if (!func)
	{
		sprintf(dest, "0");
		return true;
	}

	if (!VirtualQuery((void *)func, &mbi, sizeof(mbi)))
		return false;

	base = mbi.AllocationBase;
	for (i = 0; i < g_iextdllcount; i++)
	{
		if (g_rgextdll[i] == base)
		{
			sprintf(dest, "dll:%i:%u", i, (unsigned int)((byte *)func - (byte *)base));
			return true;
		}
	}

	return false;
}

/*
===============
Host_Savegame_WriteEdict

For savegames
===============
*/
static void Host_Savegame_WriteEdict(FILE *f, edict_t *ed)
{
	ddef_t *d;
	int *v;
	int i, j;
	int numfielddefs;
	char *name;
	int type;
	char *value;

	fprintf(f, "{\n");

	if (ed->free)
	{
		fprintf(f, "}\n");
		return;
	}

	numfielddefs = ((dprograms_t *)progs)->numfielddefs;
	for (i = 1; i < numfielddefs; i++)
	{
		char buf[128];
		int len;

		d = &((ddef_t *)pr_fielddefs)[i];
		name = pr_strings + d->s_name;

		len = strlen(name);
		if (len >= 2 && name[len - 2] == '_')
			continue;	// skip _x, _y, _z vars

		type = d->type & ~DEF_SAVEGLOBAL;
		if (type < ev_string || type > ev_function)
			continue;

		v = (int *)&ed->v + d->ofs;

		// if the value is still all 0, skip the field
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		if (type == ev_function && g_iextdllcount > 0)
		{
			if (!Host_Savegame_FunctionRef(buf, sizeof(buf), *v))
				sprintf(buf, "%i", *v);
			value = buf;
		}
		else if (type == ev_entity)
		{
			// these int fields are typed ev_entity in progs.dat
			if (!strcmp(name, "weapon")
				|| !strcmp(name, "weapons")
				|| !strcmp(name, "ammo_1")
				|| !strcmp(name, "ammo_2")
				|| !strcmp(name, "ammo_3")
				|| !strcmp(name, "ammo_4")
				|| !strcmp(name, "items")
				|| !strcmp(name, "items2")
				|| !strcmp(name, "sequence")
				|| !strcmp(name, "controller")
				|| !strcmp(name, "blending")
				|| !strcmp(name, "button"))
			{
				sprintf(buf, "%i", *v);
				value = buf;
			}
			else
			{
				value = PR_UglyValueString(type, v);
			}
		}
		else
		{
			value = PR_UglyValueString(type, v);
		}

		fprintf(f, "\"%s\" \"%s\"\n", name, value);
	}

	fprintf(f, "}\n");
}

/*
===============
Host_CanSave
===============
*/
static qboolean Host_CanSave(void)
{
	if (cmd_source != src_command)
		return false;

	if (!sv.active)
	{
		Con_Printf("Not playing a local game.\n");
		return false;
	}

	if (host_in_intermission)
	{
		Con_Printf("Can't save in intermission.\n");
		return false;
	}

	if (svs.maxclients != 1)
	{
		Con_Printf("Can't save multiplayer games.\n");
		return false;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf("save <savename> : save a game\n");
		return false;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf("Relative pathnames are not allowed.\n");
		return false;
	}

	if (svs.clients->active && svs.clients->edict->v.health <= 0)
	{
		Con_Printf("Can't savegame with a dead player\n");
		return false;
	}

	return true;
}

/*
===============
Host_BuildSaveComment

Level name and kill count, as shown in the load / save menus
===============
*/
static void Host_BuildSaveComment(char *text)
{
	int i;
	char kills[20];

	memset(text, ' ', SAVEGAME_COMMENT_LENGTH - 1);
	Q_memcpy(text, cl.levelname, strlen(cl.levelname));
	sprintf(kills, "kills:%3i/%3i", cl_stats_monsters, cl_stats_totalmonsters);
	Q_memcpy(text + 22, kills, strlen(kills));	// kills column

	// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH - 1; i++)
	{
		if (text[i] == ' ')
			text[i] = '_';
	}
	text[SAVEGAME_COMMENT_LENGTH - 1] = 0;
}

/*
===============
Host_ConvertPathSlashes

Turns / into \ and returns a pointer to the end of the string
===============
*/
static char *Host_ConvertPathSlashes(char *path)
{
	for (; *path; path++)
	{
		if (*path == '/')
			*path = '\\';
	}

	return path;
}

/*
===============
Savegame_WriteInt
===============
*/
void Savegame_WriteInt(savebuf_t *buf, int value)
{
	int *p;

	p = (int *)buf->curpos;
	buf->cursize += sizeof(int);
	*p = value;
	buf->curpos = (byte *)(p + 1);
}

/*
===============
Savegame_WriteInt2
===============
*/
void Savegame_WriteInt2(savebuf_t *buf, int value)
{
	int *p;

	p = (int *)buf->curpos;
	buf->cursize += sizeof(int);
	*p = value;
	buf->curpos = (byte *)(p + 1);
}

/*
===============
Savegame_WriteString
===============
*/
void Savegame_WriteString(savebuf_t *buf, const char *str)
{
	unsigned int len;
	char *dest;

	len = strlen(str) + 1;
	dest = (char *)buf->curpos;
	Q_memcpy(dest, str, len - 1);
	dest[len - 1] = 0;

	buf->cursize += len;
	buf->curpos = (byte *)(dest + len);
}

/*
===============
Host_Savegame_f
===============
*/
static void Host_Savegame_f(void)
{
	char name[256];
	FILE *f;
	int i;
	char comment[SAVEGAME_COMMENT_LENGTH];

	if (!Host_CanSave())
		return;

	sprintf(name, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension(name, ".sav");
	Host_ConvertPathSlashes(name);

	Con_Printf("Saving game to %s...\n", name);

	// store the current spawn parms of the player
	SV_SaveSpawnparms();

	f = fopen(name, "w");
	if (!f)
	{
		Con_Printf("ERROR: couldn't open.\n");
		return;
	}

	fprintf(f, "%i\n", SAVEGAME_VERSION);
	Host_BuildSaveComment(comment);
	fprintf(f, "%s\n", comment);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		fprintf(f, "%f\n", svs.clients->spawn_parms[i]);
	fprintf(f, "%f\n", (float)current_skill);
	fprintf(f, "%s\n", sv.name);
	fprintf(f, "%f\n", sv.time);

	// write the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
		fprintf(f, "%s\n", sv.lightstyles[i] ? sv.lightstyles[i] : "m");

	Host_Savegame_WriteGlobals(f);
	for (i = 0; i < sv.num_edicts; i++)
		Host_Savegame_WriteEdict(f, EDICT_NUM(i));

	fclose(f);
	Con_Printf("done.\n");
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

/*
==================
Host_FindViewthing
==================
*/
static edict_t *Host_FindViewthing(void)
{
	int i;
	edict_t *e;

	for (i = 0; i < sv.num_edicts; i++)
	{
		e = EDICT_NUM(i);
		if (!strcmp(PR_GetString(e->v.classname), "viewthing"))
			return e;
	}

	Con_Printf("No viewthing on map\n");
	return NULL;
}

/*
==================
Host_Viewmodel_f
==================
*/
static void Host_Viewmodel_f(void)
{
	edict_t *e;
	model_t *m;

	e = Host_FindViewthing();
	if (!e)
		return;

	m = Mod_ForName(Cmd_Argv(1), false);
	if (!m)
	{
		Con_Printf("Can't load %s\n", Cmd_Argv(1));
		return;
	}

	e->v.frame = 0;
	cl_model_precache[(int)e->v.modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
static void Host_Viewframe_f(void)
{
	edict_t *e;
	int f;
	model_t *m;

	e = Host_FindViewthing();
	if (!e)
		return;
	m = cl_model_precache[(int)e->v.modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes - 1;

	e->v.frame = f;
	Con_Printf("frame %i\n", f);
}

/*
==================
PrintFrameName
==================
*/
static void PrintFrameName(model_t *m, int frame)
{
	aliashdr_t *hdr;
	maliasframedesc_t *pframedesc;

	hdr = (aliashdr_t *)Mod_Extradata(m);
	if (!hdr)
		return;
	pframedesc = &hdr->frames[frame];

	Con_Printf("frame %i: %s\n", frame, pframedesc->name);
}

/*
==================
Host_Viewnext_f
==================
*/
static void Host_Viewnext_f(void)
{
	edict_t *e;
	model_t *m;

	e = Host_FindViewthing();
	if (!e)
		return;
	m = cl_model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame + 1;
	if (e->v.frame >= m->numframes)
		e->v.frame = m->numframes - 1;

	PrintFrameName(m, e->v.frame);
}

/*
==================
Host_Viewprev_f
==================
*/
static void Host_Viewprev_f(void)
{
	edict_t *e;
	model_t *m;

	e = Host_FindViewthing();
	if (!e)
		return;

	m = cl_model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame - 1;
	if (e->v.frame < 0)
		e->v.frame = 0;

	PrintFrameName(m, e->v.frame);
}

/*
==================
Host_Interp_f

Toggles frame interpolation
==================
*/
static void Host_Interp_f(void)
{
	r_interpolate_frames = !r_interpolate_frames;

	if (r_interpolate_frames)
		Con_Printf("Frame interpolation ON\n");
	else
		Con_Printf("Frame interpolation OFF\n");
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands(void)
{
	Cmd_AddCommand("status", Host_Status_f);
	Cmd_AddCommand("quit", Host_Quit_f);
	Cmd_AddCommand("god", Host_God_f);
	Cmd_AddCommand("notarget", Host_Notarget_f);
	Cmd_AddCommand("fly", Host_Fly_f);
	Cmd_AddCommand("noclip", Host_Noclip_f);
	Cmd_AddCommand("map", Host_Map_f);
	Cmd_AddCommand("maps", Host_Maps_f);
	Cmd_AddCommand("restart", Host_Restart_f);
	Cmd_AddCommand("changelevel", Host_Changelevel_f);
	Cmd_AddCommand("changelevel2", Host_Changelevel2_f);
	Cmd_AddCommand("connect", Host_Connect_f);
	Cmd_AddCommand("reconnect", Host_Reconnect_f);
	Cmd_AddCommand("name", Host_Name_f);
	Cmd_AddCommand("version", Host_Version_f);
	Cmd_AddCommand("say", Host_Say_f);
	Cmd_AddCommand("say_team", Host_Say_Team_f);
	Cmd_AddCommand("tell", Host_Tell_f);
	Cmd_AddCommand("color", Host_Color_f);
	Cmd_AddCommand("kill", Host_Kill_f);
	Cmd_AddCommand("pause", Host_Pause_f);
	Cmd_AddCommand("spawn", Host_Spawn_f);
	Cmd_AddCommand("begin", Host_Begin_f);
	Cmd_AddCommand("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand("kick", Host_Kick_f);
	Cmd_AddCommand("ping", Host_Ping_f);
	Cmd_AddCommand("load", Host_Loadgame_f);
	Cmd_AddCommand("save", Host_Savegame_f);
	Cmd_AddCommand("startdemos", Host_Startdemos_f);
	Cmd_AddCommand("demos", Host_Demos_f);
	Cmd_AddCommand("stopdemo", Host_Stopdemo_f);
	Cmd_AddCommand("viewmodel", Host_Viewmodel_f);
	Cmd_AddCommand("viewframe", Host_Viewframe_f);
	Cmd_AddCommand("viewnext", Host_Viewnext_f);
	Cmd_AddCommand("viewprev", Host_Viewprev_f);
	Cmd_AddCommand("mcache", Mod_Print);
	Cmd_AddCommand("interp", Host_Interp_f);
}
