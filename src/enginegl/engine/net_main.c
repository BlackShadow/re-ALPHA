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

#ifdef GetMessage
#undef GetMessage
#endif
#ifdef SendMessage
#undef SendMessage
#endif

static void NET_Slist_f(void);
static void NET_Listen_f(void);
static void MaxPlayers_f(void);
static void NET_Port_f(void);
static void PrintSlistHeader(void);
static void PrintSlist(void);
static void PrintSlistTrailer(void);
static void Slist_SendPoll(void *arg);
static void Slist_Poll(void *arg);

extern int vcrFile;
static const int vcr_zero = 0;

static void VCR_Write(const void *data, size_t len)
{
	Sys_FileWrite(vcrFile, (void *)data, len);
}

double					net_time = 0.0;
qsocket_t				*net_freeSockets = NULL;
qsocket_t				*net_activeSockets = NULL;
int						net_numsockets = 0;
int						net_listening = 0;
int						net_driverlevel = 0;
int						net_numdrivers = 2;
int						vcrFile = -1;
int						recording = 0;
int						slistInProgress = 0;
int						slistSilent = 0;
double					slistStartTime = 0.0;
int						slistLastShown = 0;
int						slistComplete = 0;
pollprocedure_t			*scheduledPoll = NULL;
int						pollProceduresInstalled = 0;

int						messagesSent = 0;
int						messagesReceived = 0;
int						unreliableMessagesSent = 0;
int						unreliableMessagesReceived = 0;

int						net_hostport = 26000;

static pollprocedure_t	slistSendProcedure = { NULL, 0.0, Slist_SendPoll, NULL };
static pollprocedure_t	slistPollProcedure = { NULL, 0.0, Slist_Poll, NULL };

void (*NET_SetComPortConfig)(int mode, int port, int irq, int baud, qboolean useModem);
void (*NET_SetModemConfig)(int mode, const char *dialtype, const char *clear, const char *init, const char *hangup);

#define sv_maxclients       (svs.maxclients)
#define maxclients_default  (svs.maxclientslimit)
#define svs_clients         (svs.clients)
#define cls_state			((int)cls.state)
extern unsigned short	hostshort;
extern sizebuf_t		net_message;

net_driver_t net_drivers[2] =
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

extern net_driver_t		net_vcr;

extern int				hostCacheCount;

cvar_t					net_messagetimeout = { "net_messagetimeout", "300" };

cvar_t hostname = { "hostname", "UNNAMED" };

cvar_t config_com_port = { "_config_com_port", "0x3f8", true, false };
cvar_t config_com_irq = { "_config_com_irq", "4", true, false };
cvar_t config_com_baud = { "_config_com_baud", "57600", true, false };
cvar_t config_com_modem = { "_config_com_modem", "1", true, false };
cvar_t config_modem_dialtype = { "_config_modem_dialtype", "T", true, false };
cvar_t config_modem_clear = { "_config_modem_clear", "ATZ", true, false };
cvar_t config_modem_init = { "_config_modem_init", "", true, false };
cvar_t config_modem_hangup = { "_config_modem_hangup", "AT H", true, false };

double SetNetTime(void)
{
	net_time = Sys_FloatTime();
	return net_time;
}

qsocket_t *NET_NewQSocket(void)
{
	qsocket_t	*sock;

	if (net_freeSockets == NULL)
		return NULL;

	if (net_activeconnections >= sv_maxclients)
		return NULL;

	sock = net_freeSockets;
	net_freeSockets = sock->next;

	sock->next = net_activeSockets;
	net_activeSockets = sock;

	sock->disconnected = 0;
	sock->connecttime = net_time;
	sock->lastMessageTime = net_time;
	Q_strcpy(sock->address, "UNSET ADDRESS");
	sock->driver = net_driverlevel;
	sock->socket = 0;
	sock->driverdata = NULL;
	sock->canSend = 1;
	sock->sendNext = 0;
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

void NET_FreeQSocket(qsocket_t *sock)
{
	qsocket_t	*s;

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

	sock->disconnected = 1;
	sock->next = net_freeSockets;
	net_freeSockets = sock;
}

void NET_Slist_f(void)
{
	if (slistInProgress)
		return;

	if (!slistSilent)
	{
		Con_Printf("Looking for Half-Life servers...\n");
		PrintSlistHeader();
	}

	slistInProgress = 1;
	slistStartTime = Sys_FloatTime();
	SchedulePollProcedure(&slistSendProcedure, 0.0);
	SchedulePollProcedure(&slistPollProcedure, 0.1);
	hostCacheCount = 0;
}

static void PrintSlistHeader(void)
{
	Con_Printf("Server          Map             Users\n");
	Con_Printf("------------    ---------------  -----\n");
	slistLastShown = 0;
}

void Slist_Send(void)
{
	Slist_SendPoll(NULL);
}

static void Slist_SendPoll(void *arg)
{
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistComplete && !net_driverlevel)
			continue;

		if (net_drivers[net_driverlevel].initialized)
			net_drivers[net_driverlevel].SearchForHosts(1);
	}

	if ((Sys_FloatTime() - slistStartTime) < 0.5)
		SchedulePollProcedure(&slistSendProcedure, 0.75);
}

static void Slist_Poll(void *arg)
{
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistComplete && !net_driverlevel)
			continue;

		if (net_drivers[net_driverlevel].initialized)
			net_drivers[net_driverlevel].SearchForHosts(0);
	}

	if (!slistSilent)
		PrintSlist();

	if ((Sys_FloatTime() - slistStartTime) < 1.5)
	{
		SchedulePollProcedure(&slistPollProcedure, 0.1);
		return;
	}

	if (!slistSilent)
		PrintSlistTrailer();

	slistComplete = 1;
	slistInProgress = 0;
	slistSilent = 0;
}

static void PrintSlist(void)
{
	int			n;

	for (n = slistLastShown; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
		{
			Con_Printf("%-15.15s %-15.15s %2u/%2u\n",
				hostcache[n].name,
				hostcache[n].map,
				hostcache[n].users,
				hostcache[n].maxusers);
		}
		else
		{
			Con_Printf("%-15.15s %-15.15s\n",
				hostcache[n].name,
				hostcache[n].map);
		}
	}

	slistLastShown = n;
}

static void PrintSlistTrailer(void)
{
	if (hostCacheCount)
		Con_Printf("---- End of list\n");
	else
		Con_Printf("No Half-Life servers found.\n");
}

qsocket_t *NET_Connect(char *host)
{
	qsocket_t		*ret;
	int				n;
	int				numdrivers = net_numdrivers;

	SetNetTime();

	if (host && *host)
	{

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

			if (n < hostCacheCount)
				goto JumpStart;
		}
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

JumpStart:
	net_driverlevel = 0;

	for (net_driverlevel = 0; net_driverlevel < numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized)
		{
			ret = net_drivers[net_driverlevel].Connect(host);
			if (ret)
				return ret;
		}
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

qsocket_t *NET_CheckNewConnections(void)
{
	qsocket_t		*ret;

	SetNetTime();

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized)
		{

			if (net_driverlevel && !net_listening)
				continue;

			ret = net_drivers[net_driverlevel].CheckNewConnections();
			if (ret)
			{
				if (recording)
				{

					double vcrtime = Sys_FloatTime();
					VCR_Write(&vcrtime, sizeof(vcrtime));
					VCR_Write(&net_driverlevel, sizeof(int));
					VCR_Write(&ret, sizeof(qsocket_t *));
					VCR_Write(&ret->address, 64);
				}
				return ret;
			}
		}
	}

	if (recording)
	{

		double vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&net_driverlevel, sizeof(int));
		VCR_Write(&vcr_zero, sizeof(vcr_zero));
	}

	return NULL;
}

void NET_Close(qsocket_t *sock)
{
	if (!sock)
		return;

	if (sock->disconnected)
		return;

	SetNetTime();

	net_drivers[sock->driver].Close(sock);

	NET_FreeQSocket(sock);
}

int NET_GetMessage(qsocket_t *sock)
{
	int		ret;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_GetMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();

	ret = net_drivers[sock->driver].GetMessage(sock);

	if (ret == 0)
	{
		if (sock->driver && (net_time - sock->lastMessageTime) > net_messagetimeout.value)
		{
			NET_Close(sock);
			return -1;
		}

		if (recording)
		{

			double vcrtime = Sys_FloatTime();
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

		double vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&sock, sizeof(qsocket_t *));
		VCR_Write(&ret, sizeof(int));
		VCR_Write(&net_message.cursize, sizeof(int));
		VCR_Write(net_message.data, (size_t)net_message.cursize);
	}

	return ret;
}

int NET_SendMessage(qsocket_t *sock, sizebuf_t *data)
{
	int		ret;

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

		double vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&sock, sizeof(qsocket_t *));
		VCR_Write(&ret, sizeof(int));
		VCR_Write(&vcr_zero, sizeof(vcr_zero));
	}

	return ret;
}

int NET_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data)
{
	int		ret;

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

		double vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&sock, sizeof(qsocket_t *));
		VCR_Write(&ret, sizeof(int));
		VCR_Write(&vcr_zero, sizeof(vcr_zero));
	}

	return ret;
}

int NET_CanSendMessage(qsocket_t *sock)
{
	int		ret;

	if (!sock)
		return 0;

	if (sock->disconnected)
		return 0;

	SetNetTime();

	ret = net_drivers[sock->driver].CanSendMessage(sock);

	if (recording)
	{

		double vcrtime = Sys_FloatTime();
		VCR_Write(&vcrtime, sizeof(vcrtime));
		VCR_Write(&sock, sizeof(qsocket_t *));
		VCR_Write(&ret, sizeof(int));
		VCR_Write(&vcr_zero, sizeof(vcr_zero));
	}

	return ret;
}

int NET_SendToAll(sizebuf_t *data, int blocktime)
{
	int			i;
	int			count = 0;
	int			canSend[32];
	int			sent[32];
	double		start;
	server_client_t	*client;

	client = svs_clients;

	for (i = 0; i < sv_maxclients; i++, client++)
	{
		if (client->netconnection)
		{
			if (!client->active)
			{

				NET_SendMessage(client->netconnection, data);
				sent[i] = 1;
				canSend[i] = 1;
			}
			else if (!client->netconnection->driver)
			{

				NET_SendMessage(client->netconnection, data);
				sent[i] = 1;
				canSend[i] = 1;
			}
			else
			{

				count++;
				canSend[i] = 0;
				sent[i] = 0;
			}
		}
		else
		{
			sent[i] = 1;
			canSend[i] = 1;
		}
	}

	start = Sys_FloatTime();

	while (count)
	{
		count = 0;
		client = svs_clients;

		for (i = 0; i < sv_maxclients; i++, client++)
		{
			if (!sent[i])
			{
				if (!canSend[i])
				{
					if (NET_CanSendMessage(client->netconnection))
					{
						canSend[i] = 1;
						continue;
					}

					NET_GetMessage(client->netconnection);
					continue;
				}

				if (NET_CanSendMessage(client->netconnection))
				{
					sent[i] = 1;
					NET_SendMessage(client->netconnection, data);
				}
				else
				{
					NET_GetMessage(client->netconnection);
				}

				count++;
			}
		}

		if ((Sys_FloatTime() - start) >= (double)blocktime)
			break;
	}

	return count;
}

void NET_Init(void)
{
	int				i;
	int				controlSocket;
	qsocket_t		*s;

	if (COM_CheckParm("-playback"))
	{
		net_numdrivers = 1;
		net_drivers[0] = net_vcr;
	}

	if (COM_CheckParm("-record"))
		recording = 1;

	i = COM_CheckParm("-port");
	if (!i)
		i = COM_CheckParm("-udpport");
	if (!i)
		i = COM_CheckParm("-ipxport");

	if (i)
	{
		if (i >= com_argc - 1)
			Sys_Error("NET_Init: you must specify a port number after -port\n");

		net_hostport = Q_atoi(com_argv[i + 1]);
	}

	hostshort = (unsigned short)net_hostport;

	if (COM_CheckParm("-listen") || !cls_state)
		net_listening = 1;

	net_numsockets = maxclients_default;
	if (cls_state)
		net_numsockets++;

	SetNetTime();

	for (i = 0; i < net_numsockets; i++)
	{
		s = (qsocket_t *)Hunk_AllocName(sizeof(qsocket_t), "qsocket");
		s->next = net_freeSockets;
		net_freeSockets = s;
		s->disconnected = 1;
	}

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

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		controlSocket = net_drivers[net_driverlevel].Init();
		if (controlSocket != -1)
		{
			net_drivers[net_driverlevel].initialized = 1;
			net_drivers[net_driverlevel].controlSock = controlSocket;

			if (net_listening)
				net_drivers[net_driverlevel].Listen(1);
		}
	}

	if (my_ipx_address[0])
		Con_DPrintf("IPX address %s\n", my_ipx_address);
	if (my_tcpip_address[0])
		Con_DPrintf("TCP/IP address %s\n", my_tcpip_address);
}

void NET_Listen_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"listen\" is \"%u\"\n", net_listening ? 1 : 0);
		return;
	}

	net_listening = Q_atoi(Cmd_Argv(1));

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized)
			net_drivers[net_driverlevel].Listen(net_listening);
	}
}

void MaxPlayers_f(void)
{
	int		n;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"maxplayers\" is \"%u\"\n", sv_maxclients);
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

	if (n > maxclients_default)
	{
		n = maxclients_default;
		Con_Printf("\"maxplayers\" set to \"%u\"\n", maxclients_default);
	}

	if (n == 1 && net_listening)
		Cmd_ExecuteString("listen 0", src_command);

	if (n > 1 && !net_listening)
		Cmd_ExecuteString("listen 1", src_command);

	sv_maxclients = n;

	if (n == 1)
		Cvar_Set("deathmatch", "0");
	else
		Cvar_Set("deathmatch", "1");
}

void NET_Port_f(void)
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
	hostshort = (unsigned short)net_hostport;

	if (net_listening)
	{

		Cmd_ExecuteString("listen 0", src_command);
		Cmd_ExecuteString("listen 1", src_command);
	}
}

void NET_Shutdown(void)
{
	qsocket_t	*sock;

	SetNetTime();

	for (sock = net_activeSockets; sock; sock = sock->next)
	{
		NET_Close(sock);
	}

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized)
		{
			net_drivers[net_driverlevel].Shutdown();
			net_drivers[net_driverlevel].initialized = 0;
		}
	}

	if (vcrFile != -1)
	{
		Con_Printf("Closing vcrfile.\n");
		Sys_FileClose(vcrFile);
	}
}

void NET_Poll(void)
{
	pollprocedure_t		*pp;
	void				*arg;

	if (!pollProceduresInstalled)
	{
		if (serialAvailable)
		{
			NET_SetComPortConfig(0,
				(int)config_com_port.value,
				(int)config_com_irq.value,
				(int)config_com_baud.value,
				(config_com_modem.value == 1.0f));
			NET_SetModemConfig(0,
				config_modem_dialtype.string,
				config_modem_clear.string,
				config_modem_init.string,
				config_modem_hangup.string);
		}
		pollProceduresInstalled = 1;
	}

	SetNetTime();

	for (pp = scheduledPoll; pp; pp = pp->next)
	{
		if (pp->nextTime > net_time)
			break;

		arg = pp->arg;
		scheduledPoll = pp->next;
		pp->procedure(arg);
	}
}

void SchedulePollProcedure(pollprocedure_t *proc, double timeOffset)
{
	pollprocedure_t	*pp;
	pollprocedure_t	*prev;
	double			systime;

	systime = Sys_FloatTime();
	proc->nextTime = systime + timeOffset;

	prev = NULL;
	for (pp = scheduledPoll; pp; pp = pp->next)
	{
		if (pp->nextTime >= proc->nextTime)
			break;
		prev = pp;
	}

	if (prev)
	{
		proc->next = pp;
		prev->next = proc;
	}
	else
	{
		proc->next = scheduledPoll;
		scheduledPoll = proc;
	}
}

const char *NET_QSocketGetString(qsocket_t *sock)
{
	if (!sock)
		return "";
	return sock->address;
}
