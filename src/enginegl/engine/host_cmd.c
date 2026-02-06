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

#include "quakedef.h"
#include "host.h"
#include "eiface.h"

#define MAX_DEMOS       8

qboolean noclip_anglehack;

extern cvar_t hostname;
extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t teamplay;
extern int g_iextdllcount;
extern cvar_t cl_name;
extern cvar_t cl_color;

extern int current_skill;

extern int net_activeconnections;

extern int type_size[];
extern char *PR_UglyValueString(int type, void *val);
extern edict_t *ED_SetEdictPointers(edict_t *e);
extern void *ED_CallDispatch(void *ent, int callback_type, void *param);
extern void *ED_GetPrivateData(edict_t *ent);
extern edict_t *FindEntityByString(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue);

#define sv_active           (sv.active)
#define sv_mapname          (sv.name)

#define svs_maxclients      (svs.maxclients)
#define svs_maxclientslimit (svs.maxclientslimit)
#define svs_clients         (svs.clients)
#define svs_serverflags      (svs.serverflags)

#define demonum             (cls.demonum)
#define demos               (cls.demos)
#define cls_demoplayback    (cls.demoplayback)
#define cls_state           ((int)cls.state)

typedef struct host_landmark_transition_s
{
	qboolean pending;
	char name[64];
	vec3_t offset;
	vec3_t angles;
} host_landmark_transition_t;

static host_landmark_transition_t host_landmark_transition;

static void Host_ClearLandmarkTransition(void)
{
	host_landmark_transition.pending = false;
	host_landmark_transition.name[0] = 0;
	VectorClear(host_landmark_transition.offset);
	VectorClear(host_landmark_transition.angles);
}

static void Host_CaptureLandmarkTransition(const char *landmark)
{
	edict_t *old_landmark;
	edict_t *player_ent;

	if (!landmark || !landmark[0])
		return;

	if (g_iextdllcount <= 0 || svs_maxclients != 1 || !svs_clients[0].active)
		return;

	player_ent = svs_clients[0].edict;
	if (!player_ent || player_ent->free)
		return;

	VectorClear(host_landmark_transition.offset);
	VectorCopy(player_ent->v.v_angle, host_landmark_transition.angles);
	Q_strcpy(host_landmark_transition.name, landmark);
	host_landmark_transition.pending = true;

	old_landmark = FindEntityByString(NULL, "targetname", landmark);
	if (old_landmark && !old_landmark->free)
		VectorSubtract(player_ent->v.origin, old_landmark->v.origin, host_landmark_transition.offset);
}

static void Host_ApplyLandmarkTransition(edict_t *ent)
{
	edict_t *new_landmark;
	vec3_t translated_origin;

	if (!host_landmark_transition.pending)
		return;

	if (svs_maxclients != 1 || !ent || ent->free)
	{
		Host_ClearLandmarkTransition();
		return;
	}

	new_landmark = FindEntityByString(NULL, "targetname", host_landmark_transition.name);
	if (new_landmark && !new_landmark->free)
	{
		VectorAdd(new_landmark->v.origin, host_landmark_transition.offset, translated_origin);
		VectorCopy(translated_origin, ent->v.origin);
		VectorCopy(translated_origin, ent->v.oldorigin);
		VectorCopy(host_landmark_transition.angles, ent->v.angles);
		VectorCopy(host_landmark_transition.angles, ent->v.v_angle);
		ent->v.fixangle = 1.0f;
		SV_LinkEdict(ent, false);
	}

	Host_ClearLandmarkTransition();
}

void Host_Status_f(void)
{
	void (*print_func)(char *, ...);
	char *hostname_str;
	int i;
	int hours, minutes, seconds;
	server_client_t *client;

	if (cmd_source == src_command)
	{
		if (!sv_active)
		{
			Cmd_ForwardToServer();
			return;
		}
		print_func = Con_Printf;
	}
	else
	{
		print_func = SV_ClientPrintf;
	}

	hostname_str = Cvar_VariableString("hostname");
	print_func("host:    %s\n", hostname_str);
	print_func("version: %4.2f\n", 1.07);

	if (ipxAvailable)
		print_func("ipx:     %s\n", my_ipx_address);
	if (tcpipAvailable)
		print_func("tcp/ip:  %s\n", my_tcpip_address);

	print_func("map:     %s\n", sv_mapname);
	print_func("players: %i active (%i max)\n\n", net_activeconnections, svs_maxclients);

	for (i = 0, client = svs_clients; i < svs_maxclients; i++, client++)
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

		print_func("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n",
			i + 1,
			client->name,
			(int)client->edict->v.frags,
			hours, minutes, seconds);
		print_func("   %s\n", client->netconnection->address);
	}
}

void Host_God_f(void)
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

void Host_Notarget_f(void)
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

void Host_Noclip_f(void)
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

void Host_Fly_f(void)
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

void Host_Startdemos_f(void)
{
	int i, c;

	if (cls_demoplayback)
		return;

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		c = MAX_DEMOS;
		Con_Printf("Max %i demos in demoloop\n", MAX_DEMOS);
	}
	Con_Printf("%i demo(s) in loop\n", c);

	for (i = 1; i < c + 1; i++)
		Q_strncpy(demos[i - 1], Cmd_Argv(i), 15);

	if (!sv_active && demonum != -1 && !cls_demoplayback)
	{
		demonum = 0;
		CL_NextDemo();
	}
	else
	{
		demonum = -1;
	}
}

void Host_Demos_f(void)
{
	if (cls_demoplayback)
		return;

	if (demonum == -1)
		demonum = 1;
	CL_Disconnect();
	CL_NextDemo();
}

void Host_Stopdemo_f(void)
{
	if (cls_demoplayback)
	{
		CL_StopPlayback();
		CL_Disconnect();
	}
}

void Host_Maps_f(void)
{
	HANDLE hFind;
	WIN32_FIND_DATA findData;
	char filename[64];

	if (cmd_source != src_command)
		return;

	strcpy(filename, com_gamedir);
	strcat(filename, "\\maps\\");

	if (Cmd_Argc())
		strcat(filename, Cmd_Argv(1));

	if (filename[strlen(filename) - 1] != '\\')
		strcat(filename, "\\");

	strcat(filename, "*.bsp");

	hFind = FindFirstFile(filename, &findData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		Con_Printf("No files at %s\n", filename);
	}
	else
	{
		do
		{
			Con_Printf("     %s\n", findData.cFileName);
		}
		while (FindNextFile(hFind, &findData));
	}
}

void Host_Map_f(void)
{
	int i;
	char mapname[64];

	if (cmd_source != src_command)
		return;

	demonum = -1;
	CL_Disconnect();
	Host_ShutdownServer(0);

	key_dest = key_game;
	SCR_BeginLoadingPlaque();

	svs_serverflags = 0;

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

	strcpy(mapname, Cmd_Argv(1));
	SV_SpawnServer(mapname, NULL);

	if (sv_active && cls.state)
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

void Host_Restart_f(void)
{
	char mapname[64];
	char startspot[64];
	char *landmark;

	if (!cls_demoplayback && sv_active && cmd_source == src_command)
	{
		strcpy(mapname, sv.name);
		strcpy(startspot, sv.startspot);
		landmark = startspot[0] ? startspot : NULL;
		SV_SpawnServer(mapname, landmark);
	}
}

void Host_Reconnect_f(void)
{
	SCR_BeginLoadingPlaque();
	cls.signon = 0;
}

void Host_Connect_f(void)
{
	char host[64];

	demonum = -1;

	if (cls_demoplayback)
	{
		CL_StopPlayback();
		CL_Disconnect();
	}

	strcpy(host, Cmd_Argv(1));
	CL_EstablishConnection(host);
	Host_Reconnect_f();
}

void Host_Changelevel_f(void)
{
	char level[64];
	char spot[64];
	char *landmark;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("changelevel <levelname> : continue game on a new level\n");
		return;
	}

	if (!sv_active || cls_demoplayback)
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

void Host_Changelevel2_f(void)
{
	char level[64];
	char spot[64];
	char *landmark;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("changelevel2 <levelname> : continue game on a new level in the unit\n");
		return;
	}

	if (!sv_active || cls_demoplayback)
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

void Host_Ping_f(void)
{
	int i;
	server_client_t *client;
	float *pings;
	int j;
	float total;
	float avg;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
	}
	else
	{
		i = 0;
		SV_ClientPrintf("Client ping times:\n");
		client = svs_clients;

		if (svs_maxclients > 0)
		{
			do
			{
				if (client->active)
				{
					pings = &client->ping_times[0];
					j = 16;
					total = 0.0;

					for (total = 0.0; j > 0; j--)
					{
						total = *pings++ + total;
					}

					avg = total / 16.0;
					SV_ClientPrintf("%4i %s\n", (int)(avg * 1000.0), client->name);
				}

				i++;
				client += 1;
			}
			while (i < svs_maxclients);
		}
	}
}

void Host_Kill_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (sv_player->v.health <= 0.0)
	{
		SV_ClientPrintf("Can't suicide -- already dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	DispatchEntityCallback(6);
}

void Host_Pause_f(void)
{
	const char *player_name;

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

	player_name = PR_GetString(sv_player->v.netname);
	if (sv.paused)
	{
		SV_BroadcastPrintf("%s paused the game\n", player_name);
	}
	else
	{
		SV_BroadcastPrintf("%s unpaused the game\n", player_name);
	}

	MSG_WriteByte(&sv.reliable_datagram, svc_setpause);
	MSG_WriteByte(&sv.reliable_datagram, sv.paused);
}

void Host_PreSpawn_f(void)
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
	host_client->sendsignon = 1;
}

void Host_Spawn_f(void)
{
	int i;
	edict_t *ent;
	server_client_t *client;

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

	if (sv.loadgame)
	{
		ent = host_client->edict;

		// Savegames restore entvars from text, but game-DLL player private data may still be null.
		// Re-run player callbacks once to allocate DLL-side state, then restore loaded entvars.
		if (g_iextdllcount > 0 && !ED_GetPrivateData(ent))
		{
			entvars_t saved_entvars;

			memcpy(&saved_entvars, &ent->v, sizeof(saved_entvars));
			memcpy(&pr_global_struct->parm1, host_client->spawn_parms, sizeof(host_client->spawn_parms));

			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(ent);

			DispatchEntityCallback(7);
			DispatchEntityCallback(8);

			memcpy(&ent->v, &saved_entvars, sizeof(saved_entvars));
			ent->v.pContainingEntity = ent;
			ent->v.pSystemGlobals = pr_global_struct;
			SV_LinkEdict(ent, false);
		}

		// Save files can contain stale transition tokens from prior runs.
		// Rebuild spawn parms now so future levelchanges use a valid parm block.
		if (g_iextdllcount > 0)
		{
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(ent);
			DispatchEntityCallback(4);
			memcpy(host_client->spawn_parms, &pr_global_struct->parm1, sizeof(host_client->spawn_parms));
		}

		sv.paused = false;
		sv.loadgame = false;
	}
	else
	{
		ent = host_client->edict;

		ED_FreePrivateData(ent);
		memset((byte *)ent + 120, 0, 4 * ((dprograms_t *)progs)->entityfields);

		ent->v.pContainingEntity = ent;
		ent->v.pSystemGlobals = pr_global_struct;

		ent->v.colormap = (float)NUM_FOR_EDICT(ent);
		ent->v.team = (float)((host_client->colors & 0xF) + 1);
		ent->v.netname = (int)((char *)host_client->name - pr_strings);

		memcpy(&pr_global_struct->parm1, host_client->spawn_parms, sizeof(host_client->spawn_parms));

		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(ent);

		DispatchEntityCallback(7);

		if (Sys_FloatTime() - host_client->netconnection->connecttime <= sv.time)
			Sys_Printf("%s entered the game\n", host_client->name);

		DispatchEntityCallback(8);

	}

	Host_ApplyLandmarkTransition(ent);

	SZ_Clear(&host_client->message);
	MSG_WriteByte(&host_client->message, svc_time);
	MSG_WriteFloat(&host_client->message, sv.time);

	for (i = 0, client = svs_clients; i < svs_maxclients; i++, client++)
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

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		MSG_WriteByte(&host_client->message, svc_lightstyle);
		MSG_WriteByte(&host_client->message, i);
		MSG_WriteString(&host_client->message, sv.lightstyles[i]);
	}

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong(&host_client->message, (int)pr_global_struct->total_secrets);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong(&host_client->message, (int)pr_global_struct->total_monsters);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_SECRETS);
	MSG_WriteLong(&host_client->message, (int)pr_global_struct->found_secrets);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_MONSTERS);
	MSG_WriteLong(&host_client->message, (int)pr_global_struct->killed_monsters);

	ent = EDICT_NUM((int)(host_client - svs_clients) + 1);

	MSG_WriteByte(&host_client->message, svc_setangle);
	MSG_WriteAngle(&host_client->message, ent->v.angles[0]);
	MSG_WriteAngle(&host_client->message, ent->v.angles[1]);
	MSG_WriteAngle(&host_client->message, 0.0f);

	SV_WriteClientdataToMessage(sv_player, &host_client->message);

	MSG_WriteByte(&host_client->message, svc_signonnum);
	MSG_WriteByte(&host_client->message, 3);
	host_client->sendsignon = 1;
}

void Host_Begin_f(void)
{
	if (cmd_source == src_command)
	{
		SV_ClientPrintf("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = 1;
}

void Host_Kick_f(void)
{
	char *reason;
	int userid;
	int i;
	server_client_t *cl;
	server_client_t *saved_host_client;
	char *sv_player_name_ptr;
	int byuserid;

	reason = NULL;
	byuserid = 0;

	if (cmd_source == src_client)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else if (sv.time != 0.0f && !host_client->active)
	{
		return;
	}

	saved_host_client = host_client;

	if (Cmd_Argc() <= 2 || !Q_strcmp(Cmd_Argv(1), "#"))
	{

		for (i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++)
		{
			if (cl->active)
			{
				host_client = cl;
				if (!Q_strcasecmp(cl->name, Cmd_Argv(1)))
					break;
			}
		}
	}
	else
	{

		userid = (int)(Q_atof(Cmd_Argv(2)) - 1.0);
		if (userid < 0)
			return;
		if (userid >= svs.maxclients)
			return;

		cl = &svs.clients[userid];
		if (!cl->active)
			return;

		byuserid = 1;
	}

	host_client = cl;

	if (i >= svs.maxclients)
	{
		host_client = saved_host_client;
		return;
	}

	if (cmd_source == src_client)
	{
		if (cls_state)
			sv_player_name_ptr = hostname.string;
		else
			sv_player_name_ptr = "console";
	}
	else
	{
		sv_player_name_ptr = saved_host_client->name;
	}

	host_client = cl;

	if (cl != saved_host_client)
	{

		if (Cmd_Argc() > 2)
		{
			reason = (char *)COM_Parse(Cmd_Args());
			if (byuserid)
			{

				for (reason++; *reason == ' '; reason++)
					;
				reason += Q_strlen(Cmd_Argv(2));
			}

			while (*reason)
			{
				if (*reason != ' ')
					break;
				reason++;
			}
		}

		if (reason)
			SV_BroadcastPrintf("Kicked by %s: %s\n", sv_player_name_ptr, reason);
		else
			SV_BroadcastPrintf("Kicked by %s\n", sv_player_name_ptr);

		SV_DropClient(0);
		host_client = saved_host_client;
	}
}

void Host_Quit_f(void)
{
	if (Cmd_Argc() == 1)
	{
		scr_disabled_for_loading = 0;
		scr_drawloading = 0;
		scr_con_current = 0;

		Sys_Quit();
	}
}

void Host_Name_f(void)
{
	const char *newname;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}

	if (Cmd_Argc() == 2)
		newname = Cmd_Argv(1);
	else
		newname = Cmd_Args();

	((char *)newname)[15] = 0;

	if (cmd_source == src_command)
	{
		if (Q_strcmp(cl_name.string, newname))
		{
			Cvar_Set("_cl_name", newname);
			if (cls.state == ca_connected)
				Cmd_ForwardToServer();
		}
	}
	else
	{

		if (host_client->name[0])
		{
			if (strcmp(host_client->name, "unconnected"))
			{
				if (Q_strcmp(host_client->name, newname))
				{
					Con_Printf("%s renamed to %s\n", host_client->name, newname);
				}
			}
		}

		Q_strcpy(host_client->name, newname);
		host_client->edict->v.netname = host_client->name - pr_strings;

		MSG_WriteByte(&sv.reliable_datagram, svc_updatename);
		MSG_WriteByte(&sv.reliable_datagram, (int)(host_client - svs_clients));
		MSG_WriteString(&sv.reliable_datagram, host_client->name);
	}
}

void Host_Version_f(void)
{
	Con_Printf("Version %4.2f\n", 1.07);
	Con_Printf("Exe: 09:18:27 Sep 4 1998\n");
}

void Host_Say(int teamonly)
{
	server_client_t *save_client;
	int i;
	int console_mode;
	char msgbuf[64];
	char *args;
	char *msg;
	int max_msg_len;
	const char *name;

	console_mode = 0;
	if (cmd_source == src_command)
	{
		if (cls_state)
		{
			Cmd_ForwardToServer();
			return;
		}

		console_mode = 1;
		teamonly = 0;
	}

	if (Cmd_Argc() < 2)
		return;

	save_client = host_client;

	args = Cmd_Args();
	msg = args;
	if (*args == '"')
	{
		msg = args + 1;
		args[strlen(msg)] = 0;
	}

	name = console_mode ? "Console" : save_client->name;
	sprintf(msgbuf, "%c%s: ", 1, name);

	max_msg_len = 62 - (int)strlen(msgbuf);
	if ((int)strlen(msg) > max_msg_len)
		msg[max_msg_len] = 0;

	strcat(msgbuf, msg);
	strcat(msgbuf, "\n");

	for (i = 0, host_client = svs_clients; i < svs_maxclients; i++, host_client++)
	{
		if (!host_client->active || !host_client->spawned)
			continue;

		if (teamplay.value != 0.0f && teamonly &&
		    save_client->edict->v.team != host_client->edict->v.team)
			continue;

		SV_ClientPrintf("%s", msgbuf);
	}

	host_client = save_client;
	Sys_Printf("%s", msgbuf);
}

void Host_Say_f(void)
{
	Host_Say(0);
}

void Host_Say_Team_f(void)
{
	Host_Say(1);
}

void Host_Tell_f(void)
{
	server_client_t *save_client;
	int i;
	char *text;
	const char *recipient_name;
	char msgbuf[64];
	int max_msg_len;

	if (cls_state == 1)
	{
		Cmd_ForwardToServer();
		return;
	}

	if (Cmd_Argc() < 3)
		return;

	save_client = host_client;

	Q_strcpy(msgbuf, save_client->name);
	Q_strcat(msgbuf, ": ");

	text = Cmd_Args();
	if (*text == '"')
	{
		text++;
		text[strlen(text) - 1] = 0;
	}

	max_msg_len = 62 - strlen(msgbuf);
	if ((int)strlen(text) > max_msg_len)
		text[max_msg_len] = 0;

	strcat(msgbuf, text);
	strcat(msgbuf, "\n");

	recipient_name = Cmd_Argv(1);
	for (i = 0, host_client = svs_clients; i < svs_maxclients; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		if (!host_client->spawned)
			continue;

		if (!Q_strcasecmp(host_client->name, recipient_name))
		{
			SV_ClientPrintf("%s", msgbuf);
			break;
		}
	}

	host_client = save_client;
}

void Host_Color_f(void)
{
	int top, bottom;
	int playercolor;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"color\" is \"%i %i\"\n",
		           ((int)cl_color.value) >> 4, ((int)cl_color.value) & 0x0F);
		Con_Printf("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
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

	if (cls_state == 1)
	{
		Cvar_SetValue("_cl_color", (float)playercolor);
		if (sv.active == 2)
			Cmd_ForwardToServer();
	}
	else
	{
		host_client->colors = playercolor;
		host_client->edict->v.team = (float)((bottom & 15) + 1);

		MSG_WriteByte(&sv.reliable_datagram, svc_updatecolors);
		MSG_WriteByte(&sv.reliable_datagram, host_client - svs_clients);
		MSG_WriteByte(&sv.reliable_datagram, host_client->colors);
	}
}

void Host_Loadgame_f(void)
{
	FILE *f;
	float spawn_parms[NUM_SPAWN_PARMS];
	float *parm_ptr;
	char str[32768];
	char comment[64];
	char buffer[128];
	int version = 0;
	float skill_float;
	float time;
	int i;
	edict_t *ent;
	int entnum;
	char *start;
	unsigned int j;
	int ch;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("load <savename> : load a game\n");
		return;
	}

demonum = -1;

	sprintf(buffer, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension(buffer, ".sav");

	Con_Printf("Loading game from %s...\n", buffer);

	CL_Disconnect();

	f = fopen(buffer, "r");
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

	fscanf(f, "%s\n", str);

	parm_ptr = spawn_parms;
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
	{
		fscanf(f, "%f\n", parm_ptr);
		parm_ptr++;
	}

	fscanf(f, "%f\n", &skill_float);
	current_skill = (int)(skill_float + 0.1);
	Cvar_SetValue("skill", (float)current_skill);
	Cvar_SetValue("deathmatch", 0.0);
	Cvar_SetValue("coop", 0.0);
	Cvar_SetValue("teamplay", 0.0);

	fscanf(f, "%s\n", comment);
	fscanf(f, "%f\n", &time);

	CL_Disconnect();
	SV_SpawnServer(comment, 0);

	if (!sv_active)
	{
		Con_Printf("Couldn't load map\n");
		return;
	}

	sv_paused = 1;
	sv_loadgame = 1;

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		fscanf(f, "%s\n", str);
		sv_lightstyles[i] = Hunk_Alloc(strlen(str) + 1);
		Q_strcpy(sv_lightstyles[i], str);
	}

	entnum = -1;
	while (!feof(f))
	{

		for (j = 0; j < 0x7FFF; j++)
		{
			ch = fgetc(f);
			if (ch == -1 || !ch)
				break;
			str[j] = ch;
			if (ch == '}')
			{
				j++;
				break;
			}
		}

		if (j == 0x7FFF)
			Sys_Error("Loadgame buffer overflow");

		str[j] = 0;
		start = COM_Parse(str);

		if (!com_token[0])
			break;

		if (strcmp(com_token, "{"))
			Sys_Error("First token isn't a brace");

		if (entnum == -1)
		{

			ED_ParseGlobals(start);
			svs_serverflags = (int)pr_global_struct->serverflags;
		}
		else
		{

			ent = EDICT_NUM(entnum);
			// Game-DLL builds expect pvPrivateData to remain valid across loadgame so entity
			// methods (DispatchThink/Use/Touch) can continue to operate after restoring entvars.
			// Freeing it here without a matching DispatchRestore path breaks movers/triggers.
			if (g_iextdllcount <= 0)
				ED_FreePrivateData(ent);
			memset((byte *)ent + 120, 0, ((dprograms_t *)progs)->entityfields * 4);
			ent->free = false;
			ED_SetEdictPointers(ent);
			ED_ParseEdict(start, ent);

			ED_SetEdictPointers(ent);

			if (!ent->free)
			{
				SV_LinkEdict(ent, false);
			}
			else
				ED_Free(ent);
		}

	entnum++;
	}

	sv_time = time;
	*sv_num_edicts = entnum;

	fclose(f);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs_clients[0].spawn_parms[i] = spawn_parms[i];

	if (cls_state != 0)
	{
		CL_EstablishConnection("local");
		Host_Reconnect_f();
	}
}

static void Host_Savegame_WriteGlobalsText(FILE *f)
{
	int i;
	int numglobaldefs;
	ddef_t *def;
	int type;
	float *val;
	const char *name;
	char *value;

	fprintf(f, "{\n");

	numglobaldefs = ((dprograms_t *)progs)->numglobaldefs;
	def = (ddef_t *)pr_globaldefs;
	for (i = 0; i < numglobaldefs; ++i, ++def)
	{
		type = def->type;
		if (!(type & 0x8000))
			continue;

		type &= 0x7FFF;
		if (type != 1 && type != 2 && type != 4)
			continue;

		name = pr_strings + def->s_name;
		val = (float *)((byte *)pr_globals + 4 * def->ofs);

		value = PR_UglyValueString(type, val);
		fprintf(f, "\"%s\" \"%s\"\n", name, value);
	}

	fprintf(f, "}\n");
}

static qboolean Host_Savegame_FormatDLLFunctionRef(char *dest, int dest_size, int func_value)
{
	MEMORY_BASIC_INFORMATION mbi;
	void *ptr;
	void *base;
	int i;

	if (!dest || dest_size <= 0)
		return false;

	if (!func_value)
	{
		sprintf(dest, "0");
		return true;
	}

	ptr = (void *)(ULONG_PTR)(unsigned int)func_value;
	if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
		return false;

	base = mbi.AllocationBase;
	for (i = 0; i < g_iextdllcount; ++i)
	{
		if (g_rgextdll[i] == base)
		{
			unsigned int offset = (unsigned int)((byte *)ptr - (byte *)base);
			sprintf(dest, "dll:%i:%u", i, offset);
			return true;
		}
	}

	return false;
}

static void Host_Savegame_WriteEdictText(FILE *f, edict_t *ed)
{
	int i;
	int j;
	int *progs_ptr;
	unsigned short *def;
	const char *fieldname;
	void *val;
	int type;
	int type_size_local;
	int *field_data;
	char *valstring;
	int numfielddefs;

	fprintf(f, "{\n");

	if (ed->free)
	{
		fprintf(f, "}\n");
		return;
	}

	progs_ptr = (int *)progs;
	numfielddefs = progs_ptr[7];

	for (i = 1; i < numfielddefs; i++)
	{
		char valbuf[128];

		def = (unsigned short *)((char *)pr_fielddefs + i * 8);
		fieldname = pr_strings + *(int *)((char *)def + 4);

		{
			int len = strlen(fieldname);
			if (len >= 2 && fieldname[len - 2] == '_')
				continue;
		}

		type = def[0] & 0x7FFF;
		if (type < 1 || type > 6)
			continue;
		type_size_local = type_size[type];
		val = (char *)ed + 120 + 4 * def[1];

		field_data = (int *)val;
		for (j = 0; j < type_size_local; j++)
		{
			if (field_data[j])
				break;
		}

		if (j == type_size_local)
			continue;

		if (type == 6 && g_iextdllcount > 0)
		{
			// Store game-DLL function slots as a DLL-relative reference so saves survive restarts
			// (ASLR can relocate the module base, making raw pointers invalid).
			if (Host_Savegame_FormatDLLFunctionRef(valbuf, (int)sizeof(valbuf), *(int *)val))
				valstring = valbuf;
			else
			{
				sprintf(valbuf, "%i", *(int *)val);
				valstring = valbuf;
			}
		}
		else if (type == 4)
		{

			if (!strcmp(fieldname, "weapon") ||
			    !strcmp(fieldname, "weapons") ||
			    !strcmp(fieldname, "ammo_1") ||
			    !strcmp(fieldname, "ammo_2") ||
			    !strcmp(fieldname, "ammo_3") ||
			    !strcmp(fieldname, "ammo_4") ||
			    !strcmp(fieldname, "items") ||
			    !strcmp(fieldname, "items2") ||
			    !strcmp(fieldname, "sequence") ||
			    !strcmp(fieldname, "controller") ||
			    !strcmp(fieldname, "blending") ||
			    !strcmp(fieldname, "button"))
			{
				sprintf(valbuf, "%i", *(int *)val);
				valstring = valbuf;
			}
			else
			{
				valstring = PR_UglyValueString(type, val);
			}
		}
		else
		{
			valstring = PR_UglyValueString(type, val);
		}
		fprintf(f, "\"%s\" \"%s\"\n", fieldname, valstring);
	}

	fprintf(f, "}\n");
}

qboolean Host_CanSave(void)
{
	const char *saveName;

	if (cmd_source != src_command)
		return false;

	if (!sv_active)
	{
		Con_Printf("Not playing a local game.\n");
		return false;
	}

	if (host_in_intermission)
	{
		Con_Printf("Can't save in intermission.\n");
		return false;
	}

	if (svs_maxclients != 1)
	{
		Con_Printf("Can't save multiplayer games.\n");
		return false;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf("save <savename> : save a game\n");
		return false;
	}

	saveName = Cmd_Argv(1);
	if (strstr(saveName, ".."))
	{
		Con_Printf("Relative pathnames are not allowed.\n");
		return false;
	}

	if (svs_clients[0].active && svs_clients[0].edict->v.health <= 0.0f)
	{
		Con_Printf("Can't savegame with a dead player\n");
		return false;
	}

	return true;
}

void Host_BuildSaveComment(byte *comment)
{
	int i;
	char buffer[20];

	memset(comment, ' ', 39);
	Q_memcpy(comment, cl.levelname, strlen(cl.levelname));

	sprintf(buffer, "kills:%3i/%3i", cl_stats_monsters, cl_stats_totalmonsters);
	Q_memcpy(comment + 22, buffer, strlen(buffer));

	for (i = 0; i < 39; i++)
	{
		if (comment[i] == ' ')
			comment[i] = '_';
	}

	comment[39] = 0;
}

char *Host_ConvertPathSlashes(char *path)
{
	char *result;

	for (result = path; *result; ++result)
	{
		if (*result == '/')
			*result = '\\';
	}

	return result;
}

void Savegame_WriteInt(savebuf_t *buf, int value)
{
	int *ptr;

	ptr = (int *)buf->curpos;
	buf->cursize += 4;
	*ptr = value;
	buf->curpos = (byte *)(ptr + 1);
}

void Savegame_WriteInt2(savebuf_t *buf, int value)
{
	int *ptr;

	ptr = (int *)buf->curpos;
	buf->cursize += 4;
	*ptr = value;
	buf->curpos = (byte *)(ptr + 1);
}

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

void Host_Savegame_f(void)
{
	const char *saveName;
	int i;
	FILE *f;
	char fileName[256];
	char comment[40];

	if (!Host_CanSave())
		return;

	saveName = Cmd_Argv(1);
	sprintf(fileName, "%s/%s", com_gamedir, saveName);
	COM_DefaultExtension(fileName, ".sav");
	Host_ConvertPathSlashes(fileName);

	Con_Printf("Saving game to %s...\n", fileName);

	// Keep save spawn parms synchronized with current player state.
	SV_SaveSpawnparms();

	f = fopen(fileName, "w");
	if (!f)
	{
		Con_Printf("ERROR: couldn't open.\n");
		return;
	}

	fprintf(f, "%i\n", SAVEGAME_VERSION);

	Host_BuildSaveComment((byte *)comment);
	fprintf(f, "%s\n", comment);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		fprintf(f, "%f\n", svs_clients[0].spawn_parms[i]);

	fprintf(f, "%f\n", (float)current_skill);
	fprintf(f, "%s\n", sv.name);
	fprintf(f, "%f\n", sv.time);

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
		fprintf(f, "%s\n", sv.lightstyles[i] ? sv.lightstyles[i] : "m");

	Host_Savegame_WriteGlobalsText(f);

	for (i = 0; i < sv.num_edicts; i++)
		Host_Savegame_WriteEdictText(f, EDICT_NUM(i));

	fclose(f);
	Con_Printf("done.\n");
}

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

void Host_Viewmodel_f(void)
{
	edict_t *e;
	model_t *m;

	e = Host_FindViewthing();
	if (!e)
		return;

	m = Mod_ForName(Cmd_Argv(1), false);
	if (m)
	{
		e->v.frame = 0;
		cl_model_precache[(int)e->v.modelindex] = m;
	}
	else
	{
		Con_Printf("Can't load %s\n", Cmd_Argv(1));
	}
}

void Host_Viewframe_f(void)
{
	edict_t *e;
	model_t *m;
	int frame;

	e = Host_FindViewthing();
	if (!e)
		return;

	m = cl_model_precache[(int)e->v.modelindex];
	frame = atoi(Cmd_Argv(1));

	if (frame >= m->numframes)
		frame = m->numframes - 1;

	e->v.frame = frame;
	Con_Printf("frame %i\n", frame);
}

void PrintFrameName(model_t *m, int frame)
{
	aliashdr_t *hdr;
	maliasframedesc_t *pframedesc;

	hdr = (aliashdr_t *)Mod_Extradata(m);
	if (!hdr)
		return;

	pframedesc = &hdr->frames[frame];
	Con_Printf("frame %i: %s\n", frame, pframedesc->name);
}

void Host_Viewnext_f(void)
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

	PrintFrameName(m, (int)e->v.frame);
}

void Host_Viewprev_f(void)
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

	PrintFrameName(m, (int)e->v.frame);
}

void Host_Interp_f(void)
{
	extern int r_interpolate_frames;

	r_interpolate_frames = !r_interpolate_frames;

	if (r_interpolate_frames)
		Con_Printf("Frame interpolation ON\n");
	else
		Con_Printf("Frame interpolation OFF\n");
}

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
