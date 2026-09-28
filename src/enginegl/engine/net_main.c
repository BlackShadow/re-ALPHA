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

// net_main.c -- qsockets, the network drivers and the server list

#include "quakedef.h"

// windows.h maps these to GetMessageA/SendMessageA
#undef GetMessage
#undef SendMessage

#define DEFAULT_NET_HOSTPORT	26000
#define NET_MAXCLIENTS			32		// NET_SendToAll state arrays

// server list search timing, in seconds
#define SLIST_SENDTIME			0.75	// the request is broadcast again this often...
#define SLIST_SENDDURATION		0.5		// ...for this long
#define SLIST_POLLTIME			0.1		// replies are read this often...
#define SLIST_DURATION			1.5		// ...for this long

qsocket_t		*net_activeSockets = NULL;
qsocket_t		*net_freeSockets = NULL;
int				net_numsockets = 0;

static qboolean	listening = false;

qboolean		slistInProgress = false;
qboolean		slistSilent = false;
qboolean		slistLocal = false;
static double	slistStartTime = 0.0;
static int		slistLastShown = 0;

static void Slist_SendPoll(void *arg);
static void Slist_Poll(void *arg);
static pollprocedure_t	slistSendProcedure = { NULL, 0.0, Slist_SendPoll, NULL };
static pollprocedure_t	slistPollProcedure = { NULL, 0.0, Slist_Poll, NULL };

int				net_driverlevel = 0;
int				net_numdrivers = MAX_NET_DRIVERS;

double			net_time = 0.0;

int				vcrFile = -1;
qboolean		recording = false;

static pollprocedure_t	*pollProcedureList = NULL;
qboolean		pollProceduresInstalled = false;

int				messagesSent = 0;
int				messagesReceived = 0;
int				unreliableMessagesSent = 0;
int				unreliableMessagesReceived = 0;

int				net_hostport = DEFAULT_NET_HOSTPORT;

void (*NET_SetComPortConfig)(int portNumber, int port, int irq, int baud, qboolean useModem);
void (*NET_SetModemConfig)(int portNumber, const char *dialType, const char *clear, const char *init, const char *hangup);

net_driver_t net_drivers[MAX_NET_DRIVERS] =
{
	{
		"Loopback",
		false,
		Loop_Init,
		Loop_Listen,
		Loop_SearchForHosts,
		Loop_Connect,
		Loop_CheckNewConnections,
		Loop_GetMessage,
		Loop_SendMessage,
		Loop_SendUnreliableMessage,
		Loop_CanSendMessage,
		Loop_CanSendUnreliableMessage,
		Loop_Close,
		Loop_Shutdown,
		-1
	},
	{
		"Datagram",
		false,
		Datagram_Init,
		Datagram_Listen,
		Datagram_SearchForHosts,
		Datagram_Connect,
		Datagram_CheckNewConnections,
		Datagram_GetMessage,
		Datagram_SendMessage,
		Datagram_SendUnreliableMessage,
		Datagram_CanSendMessage,
		Datagram_CanSendUnreliableMessage,
		Datagram_Close,
		Datagram_Shutdown,
		-1
	}
};

cvar_t	net_messagetimeout = { "net_messagetimeout", "300" };
cvar_t	hostname = { "hostname", "UNNAMED" };

cvar_t	config_com_port = { "_config_com_port", "0x3f8", true, false };
cvar_t	config_com_irq = { "_config_com_irq", "4", true, false };
cvar_t	config_com_baud = { "_config_com_baud", "57600", true, false };
cvar_t	config_com_modem = { "_config_com_modem", "1", true, false };
cvar_t	config_modem_dialtype = { "_config_modem_dialtype", "T", true, false };
cvar_t	config_modem_clear = { "_config_modem_clear", "ATZ", true, false };
cvar_t	config_modem_init = { "_config_modem_init", "", true, false };
cvar_t	config_modem_hangup = { "_config_modem_hangup", "AT H", true, false };

static const int vcr_zero = 0;

/*
================
VCR_Write

Appends to the -record file.
================
*/
static void VCR_Write(const void *data, size_t len)
{
	Sys_FileWrite(vcrFile, (void *)data, len);
}

/*
================
SetNetTime
================
*/
double SetNetTime(void)
{
	net_time = Sys_FloatTime();
	return net_time;
}

/*
===================
NET_NewQSocket

Called by drivers when a new communications endpoint is required
The sequence and buffer fields will be filled in properly
===================
*/
qsocket_t *NET_NewQSocket(void)
{
	qsocket_t	*sock;

	if (net_freeSockets == NULL)
		return NULL;

	if (net_activeconnections >= svs.maxclients)
		return NULL;

	// get one from free list
	sock = net_freeSockets;
	net_freeSockets = sock->next;

	// add it to active list
	sock->next = net_activeSockets;
	net_activeSockets = sock;

	sock->disconnected = false;
	sock->connecttime = net_time;
	sock->lastMessageTime = net_time;
	Q_strcpy(sock->address, "UNSET ADDRESS");
	sock->driver = net_driverlevel;
	sock->socket = 0;
	sock->driverdata = NULL;
	sock->canSend = true;
	sock->sendNext = false;
	sock->lastSendTime = net_time;
	sock->ackSequence = 0;
	sock->sendSequence = 0;
	sock->unreliableSendSequence = 0;
	sock->sendMessageLength = 0;
	sock->receiveSequence = 0;
	sock->unreliableReceiveSequence = 0;
	sock->receiveMessageLength = 0;

	return sock;
}

/*
===================
NET_FreeQSocket
===================
*/
void NET_FreeQSocket(qsocket_t *sock)
{
	qsocket_t	*s;

	// remove it from active list
	if (sock == net_activeSockets)
	{
		net_activeSockets = net_activeSockets->next;
	}
	else
	{
		for (s = net_activeSockets; s; s = s->next)
		{
			if (s->next == sock)
			{
				s->next = sock->next;
				break;
			}
		}

		if (!s)
			Sys_Error("NET_FreeQSocket: not active\n");
	}

	// add it to free list
	sock->disconnected = true;
	sock->next = net_freeSockets;
	net_freeSockets = sock;
}

/*
===================
PrintSlistHeader
===================
*/
static void PrintSlistHeader(void)
{
	Con_Printf("Server          Map             Users\n");
	Con_Printf("------------    ---------------  -----\n");
	slistLastShown = 0;
}

/*
===================
PrintSlist
===================
*/
static void PrintSlist(void)
{
	int		n;

	for (n = slistLastShown; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			Con_Printf("%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			Con_Printf("%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
	}

	slistLastShown = n;
}

/*
===================
PrintSlistTrailer
===================
*/
static void PrintSlistTrailer(void)
{
	if (hostCacheCount)
		Con_Printf("---- End of list\n");
	else
		Con_Printf("No Half-Life servers found.\n");
}

/*
===================
NET_Slist_f
===================
*/
static void NET_Slist_f(void)
{
	if (slistInProgress)
		return;

	if (!slistSilent)
	{
		Con_Printf("Looking for Half-Life servers...\n");
		PrintSlistHeader();
	}

	slistInProgress = true;
	slistStartTime = Sys_FloatTime();

	SchedulePollProcedure(&slistSendProcedure, 0.0);
	SchedulePollProcedure(&slistPollProcedure, SLIST_POLLTIME);

	hostCacheCount = 0;
}

/*
===================
Slist_Send
===================
*/
void Slist_Send(void)
{
	Slist_SendPoll(NULL);
}

/*
===================
Slist_SendPoll

Broadcasts a server info request; repeats for the first half second.
===================
*/
static void Slist_SendPoll(void *arg)
{
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized)
			net_drivers[net_driverlevel].SearchForHosts(true);
	}

	if ((Sys_FloatTime() - slistStartTime) < SLIST_SENDDURATION)
		SchedulePollProcedure(&slistSendProcedure, SLIST_SENDTIME);
}

/*
===================
Slist_Poll

Collects server info replies for a second and a half.
===================
*/
static void Slist_Poll(void *arg)
{
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized)
			net_drivers[net_driverlevel].SearchForHosts(false);
	}

	if (!slistSilent)
		PrintSlist();

	if ((Sys_FloatTime() - slistStartTime) < SLIST_DURATION)
	{
		SchedulePollProcedure(&slistPollProcedure, SLIST_POLLTIME);
		return;
	}

	if (!slistSilent)
		PrintSlistTrailer();

	slistLocal = true;
	slistInProgress = false;
	slistSilent = false;
}

/*
===================
NET_Connect
===================
*/
qsocket_t *NET_Connect(char *host)
{
	qsocket_t	*ret;
	int			n;
	int			numdrivers = net_numdrivers;

	SetNetTime();

	if (host && *host && hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (Q_strcmp(host, hostcache[n].name) == 0)
			{
				host = hostcache[n].cname;
				break;
			}
		}
		if (n < hostCacheCount)
			goto JustDoIt;
	}

	slistSilent = (host && *host);
	NET_Slist_f();

	while (slistInProgress)
		NET_Poll();

	if (host == NULL)
	{
		if (hostCacheCount != 1)
			return NULL;
		host = hostcache[0].cname;
		Con_Printf("Connecting to...\n%s @ %s\n\n", hostcache[0].name, hostcache[0].cname);
	}

	if (hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (Q_strcmp(host, hostcache[n].name) == 0)
			{
				host = hostcache[n].cname;
				break;
			}
		}
	}

JustDoIt:
	for (net_driverlevel = 0; net_driverlevel < numdrivers; net_driverlevel++)
	{
		if (!net_drivers[net_driverlevel].initialized)
			continue;
		ret = net_drivers[net_driverlevel].Connect(host);
		if (ret)
			return ret;
	}

	if (host)
	{
		Con_Printf("\n");
		PrintSlistHeader();
		PrintSlist();
		PrintSlistTrailer();
	}

	return NULL;
}

/*
===================
NET_CheckNewConnections
===================
*/
qsocket_t *NET_CheckNewConnections(void)
{
	qsocket_t	*ret;
	double		vcrtime;

	SetNetTime();

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!net_drivers[net_driverlevel].initialized)
			continue;
		if (net_driverlevel && !listening)
			continue;

		ret = net_drivers[net_driverlevel].CheckNewConnections();
		if (ret)
		{
			if (recording)
			{
				vcrtime = Sys_FloatTime();
				VCR_Write(&vcrtime, sizeof(vcrtime));
				VCR_Write(&net_driverlevel, sizeof(int));
				VCR_Write(&ret, sizeof(qsocket_t *));
				VCR_Write(&ret->address, NET_NAMELEN);
			}
			return ret;
		}
	}

	if (recording)
	{
		vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&net_driverlevel, sizeof(int));
		VCR_Write(&vcr_zero, sizeof(vcr_zero));
	}

	return NULL;
}

/*
===================
NET_Close
===================
*/
void NET_Close(qsocket_t *sock)
{
	if (!sock)
		return;

	if (sock->disconnected)
		return;

	SetNetTime();

	// call the driver_Close function
	net_drivers[sock->driver].Close(sock);

	NET_FreeQSocket(sock);
}

/*
=================
NET_GetMessage

If there is a complete message, return it in net_message

returns 0 if no data is waiting
returns 1 if a message was received
returns -1 if connection is invalid
=================
*/
int NET_GetMessage(qsocket_t *sock)
{
	int		ret;
	double	vcrtime;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_GetMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();

	ret = net_drivers[sock->driver].GetMessage(sock);

	// see if this connection has timed out
	if (ret == 0)
	{
		if (sock->driver && (net_time - sock->lastMessageTime) > net_messagetimeout.value)
		{
			NET_Close(sock);
			return -1;
		}

		if (recording)
		{
			vcrtime = Sys_FloatTime();
			VCR_Write(&vcrtime, sizeof(vcrtime));
			VCR_Write(&sock, sizeof(qsocket_t *));
			VCR_Write(&ret, sizeof(int));
			VCR_Write(&vcr_zero, sizeof(vcr_zero));
		}
		return 0;
	}

	if (sock->driver)
	{
		sock->lastMessageTime = net_time;
		if (ret == 1)
			messagesReceived++;
		else if (ret == 2)
			unreliableMessagesReceived++;
	}

	if (recording)
	{
		vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&sock, sizeof(qsocket_t *));
		VCR_Write(&ret, sizeof(int));
		VCR_Write(&net_message.cursize, sizeof(int));
		VCR_Write(net_message.data, net_message.cursize);
	}

	return ret;
}

/*
==================
NET_SendMessage

Try to send a complete length+message unit over the reliable stream.
returns 0 if the message cannot be delivered reliably, but the connection
		is still considered valid
returns 1 if the message was sent properly
returns -1 if the connection died
==================
*/
int NET_SendMessage(qsocket_t *sock, sizebuf_t *data)
{
	int		ret;
	double	vcrtime;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();

	ret = net_drivers[sock->driver].SendMessage(sock, data);
	if (ret == 1 && sock->driver)
		messagesSent++;

	if (recording)
	{
		vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&sock, sizeof(qsocket_t *));
		VCR_Write(&ret, sizeof(int));
		VCR_Write(&vcr_zero, sizeof(vcr_zero));
	}

	return ret;
}

/*
==================
NET_SendUnreliableMessage
==================
*/
int NET_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data)
{
	int		ret;
	double	vcrtime;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();

	ret = net_drivers[sock->driver].SendUnreliableMessage(sock, data);
	if (ret == 1 && sock->driver)
		unreliableMessagesSent++;

	if (recording)
	{
		vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&sock, sizeof(qsocket_t *));
		VCR_Write(&ret, sizeof(int));
		VCR_Write(&vcr_zero, sizeof(vcr_zero));
	}

	return ret;
}

/*
==================
NET_CanSendMessage

Returns true or false if the given qsocket can currently accept a
message to be transmitted.
==================
*/
qboolean NET_CanSendMessage(qsocket_t *sock)
{
	int		r;
	double	vcrtime;

	if (!sock)
		return false;

	if (sock->disconnected)
		return false;

	SetNetTime();

	r = net_drivers[sock->driver].CanSendMessage(sock);

	if (recording)
	{
		vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&sock, sizeof(qsocket_t *));
		VCR_Write(&r, sizeof(int));
		VCR_Write(&vcr_zero, sizeof(vcr_zero));
	}

	return r;
}

/*
==================
NET_SendToAll

Reliable blocking send to every connected client; returns how many
still had not received it when blocktime ran out.
==================
*/
int NET_SendToAll(sizebuf_t *data, int blocktime)
{
	double			start;
	int				i;
	int				count = 0;
	qboolean		state1[NET_MAXCLIENTS];	// can send
	qboolean		state2[NET_MAXCLIENTS];	// sent
	server_client_t	*client;

	client = svs.clients;
	for (i = 0; i < svs.maxclients; i++, client++)
	{
		if (!client->netconnection)
		{
			state2[i] = true;
			state1[i] = true;
			continue;
		}

		if (!client->active)
		{
			NET_SendMessage(client->netconnection, data);
			state2[i] = true;
			state1[i] = true;
		}
		else if (!client->netconnection->driver)
		{
			NET_SendMessage(client->netconnection, data);
			state2[i] = true;
			state1[i] = true;
		}
		else
		{
			count++;
			state1[i] = false;
			state2[i] = false;
		}
	}

	start = Sys_FloatTime();
	while (count)
	{
		count = 0;
		for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
		{
			if (state2[i])
				continue;

			if (!state1[i])
			{
				if (NET_CanSendMessage(client->netconnection))
				{
					state1[i] = true;
					continue;
				}
				NET_GetMessage(client->netconnection);
				continue;
			}

			if (NET_CanSendMessage(client->netconnection))
			{
				state2[i] = true;
				NET_SendMessage(client->netconnection, data);
			}
			else
			{
				NET_GetMessage(client->netconnection);
			}
			count++;
		}

		if ((Sys_FloatTime() - start) >= blocktime)
			break;
	}

	return count;
}

//=============================================================================

/*
====================
NET_Listen_f
====================
*/
static void NET_Listen_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"listen\" is \"%u\"\n", listening ? 1 : 0);
		return;
	}

	listening = Q_atoi(Cmd_Argv(1));

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!net_drivers[net_driverlevel].initialized)
			continue;
		net_drivers[net_driverlevel].Listen(listening);
	}
}

/*
====================
MaxPlayers_f
====================
*/
static void MaxPlayers_f(void)
{
	int		n;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"maxplayers\" is \"%u\"\n", svs.maxclients);
		return;
	}

	if (sv.active)
	{
		Con_Printf("maxplayers can not be changed while a server is running.\n");
		return;
	}

	n = Q_atoi(Cmd_Argv(1));
	if (n < 1)
		n = 1;
	if (n > svs.maxclientslimit)
	{
		n = svs.maxclientslimit;
		Con_Printf("\"maxplayers\" set to \"%u\"\n", n);
	}

	if (n == 1 && listening)
		Cmd_ExecuteString("listen 0", src_command);

	if (n > 1 && !listening)
		Cmd_ExecuteString("listen 1", src_command);

	svs.maxclients = n;
	if (n == 1)
		Cvar_Set("deathmatch", "0");
	else
		Cvar_Set("deathmatch", "1");
}

/*
====================
NET_Port_f
====================
*/
static void NET_Port_f(void)
{
	int		n;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"port\" is \"%u\"\n", hostshort);
		return;
	}

	n = Q_atoi(Cmd_Argv(1));
	if (n < 1 || n > 65534)
	{
		Con_Printf("Bad value, must be between 1 and 65534\n");
		return;
	}

	net_hostport = n;
	hostshort = net_hostport;

	if (listening)
	{
		// force a change to the new port
		Cmd_ExecuteString("listen 0", src_command);
		Cmd_ExecuteString("listen 1", src_command);
	}
}

/*
====================
NET_Init
====================
*/
void NET_Init(void)
{
	int			i;
	int			controlSocket;
	qsocket_t	*s;

	if (COM_CheckParm("-playback"))
	{
		net_numdrivers = 1;
		net_drivers[0] = net_vcr;
	}

	if (COM_CheckParm("-record"))
		recording = true;

	i = COM_CheckParm("-port");
	if (!i)
		i = COM_CheckParm("-udpport");
	if (!i)
		i = COM_CheckParm("-ipxport");

	if (i)
	{
		if (i < com_argc - 1)
			net_hostport = Q_atoi(com_argv[i + 1]);
		else
			Sys_Error("NET_Init: you must specify a port number after -port\n");
	}
	hostshort = net_hostport;

	if (COM_CheckParm("-listen") || cls.state == ca_dedicated)
		listening = true;

	net_numsockets = svs.maxclientslimit;
	if (cls.state != ca_dedicated)
		net_numsockets++;

	SetNetTime();

	for (i = 0; i < net_numsockets; i++)
	{
		s = (qsocket_t *)Hunk_AllocName(sizeof(qsocket_t), "qsocket");
		s->next = net_freeSockets;
		net_freeSockets = s;
		s->disconnected = true;
	}

	// allocate space for network message buffer
	SZ_Alloc(&net_message, MAX_MSGLEN);

	Cvar_RegisterVariable(&net_messagetimeout);
	Cvar_RegisterVariable(&hostname);
	Cvar_RegisterVariable(&config_com_port);
	Cvar_RegisterVariable(&config_com_irq);
	Cvar_RegisterVariable(&config_com_baud);
	Cvar_RegisterVariable(&config_com_modem);
	Cvar_RegisterVariable(&config_modem_dialtype);
	Cvar_RegisterVariable(&config_modem_clear);
	Cvar_RegisterVariable(&config_modem_init);
	Cvar_RegisterVariable(&config_modem_hangup);

	Cmd_AddCommand("slist", NET_Slist_f);
	Cmd_AddCommand("listen", NET_Listen_f);
	Cmd_AddCommand("maxplayers", MaxPlayers_f);
	Cmd_AddCommand("port", NET_Port_f);

	// initialize all the drivers
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		controlSocket = net_drivers[net_driverlevel].Init();
		if (controlSocket == -1)
			continue;
		net_drivers[net_driverlevel].initialized = true;
		net_drivers[net_driverlevel].controlSock = controlSocket;
		if (listening)
			net_drivers[net_driverlevel].Listen(true);
	}

	if (*my_ipx_address)
		Con_DPrintf("IPX address %s\n", my_ipx_address);
	if (*my_tcpip_address)
		Con_DPrintf("TCP/IP address %s\n", my_tcpip_address);
}

/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown(void)
{
	qsocket_t	*sock;

	SetNetTime();

	for (sock = net_activeSockets; sock; sock = sock->next)
		NET_Close(sock);

	// shutdown the drivers
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized)
		{
			net_drivers[net_driverlevel].Shutdown();
			net_drivers[net_driverlevel].initialized = false;
		}
	}

	if (vcrFile != -1)
	{
		Con_Printf("Closing vcrfile.\n");
		Sys_FileClose(vcrFile);
	}
}

/*
====================
NET_Poll
====================
*/
void NET_Poll(void)
{
	pollprocedure_t	*pp;
	void			*arg;

	if (!pollProceduresInstalled)
	{
		if (serialAvailable)
		{
			NET_SetComPortConfig(0, (int)config_com_port.value, (int)config_com_irq.value, (int)config_com_baud.value, config_com_modem.value == 1.0f);
			NET_SetModemConfig(0, config_modem_dialtype.string, config_modem_clear.string, config_modem_init.string, config_modem_hangup.string);
		}
		pollProceduresInstalled = true;
	}

	SetNetTime();

	for (pp = pollProcedureList; pp; pp = pp->next)
	{
		if (pp->nextTime > net_time)
			break;
		arg = pp->arg;
		pollProcedureList = pp->next;
		pp->procedure(arg);
	}
}

/*
====================
SchedulePollProcedure
====================
*/
void SchedulePollProcedure(pollprocedure_t *proc, double timeOffset)
{
	pollprocedure_t	*pp, *prev;

	proc->nextTime = Sys_FloatTime() + timeOffset;
	for (pp = pollProcedureList, prev = NULL; pp; pp = pp->next)
	{
		if (pp->nextTime >= proc->nextTime)
			break;
		prev = pp;
	}

	if (prev == NULL)
	{
		proc->next = pollProcedureList;
		pollProcedureList = proc;
		return;
	}

	proc->next = pp;
	prev->next = proc;
}

/*
====================
NET_QSocketGetString
====================
*/
const char *NET_QSocketGetString(qsocket_t *sock)
{
	if (!sock)
		return "";
	return sock->address;
}
