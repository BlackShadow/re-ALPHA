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

// net_datagram.c -- datagram driver: reliable and unreliable messages over UDP or IPX

#include "quakedef.h"

#define RESEND_TIME			1.0		// resend an unacked reliable fragment after this
#define CONNECT_TIMEOUT		2.5		// wait this long for each connect reply
#define CONNECT_RETRIES		3
#define DUPLICATE_WINDOW	2.0		// a repeated connect request this soon gets the same reply

#define TEST_MAXPLAYERS		16		// player info requests sent by "test"
#define TEST_POLLCOUNT		20		// reply polls before "test" gives up
#define TEST_POLLTIME		0.1
#define TEST2_POLLTIME		0.05

static net_landriver_t net_landrivers[MAX_NET_LANDRIVERS] =
{
	{
		"TCPIP",
		false,
		-1,
		WINS_Init,
		WINS_Shutdown,
		WINS_Listen,
		WINS_OpenSocket,
		WINS_Connect,
		WINS_CloseSocket,
		WINS_CheckNewConnections,
		WINS_Read,
		WINS_Write,
		WINS_Broadcast,
		WINS_AddrToString,
		WINS_GetSocketAddr,
		WINS_GetNameFromAddr,
		WINS_GetAddrFromName,
		WINS_AddrCompare,
		WINS_GetSocketPort,
		WINS_SetSocketPort
	},
	{
		"IPX",
		false,
		-1,
		WIPX_Init,
		WIPX_Shutdown,
		WIPX_Listen,
		WIPX_OpenSocket,
		WIPX_Connect,
		WIPX_CloseSocket,
		WIPX_CheckNewConnections,
		WIPX_Read,
		WIPX_Write,
		WIPX_Broadcast,
		WIPX_AddrToString,
		WIPX_GetSocketAddr,
		WIPX_GetNameFromAddr,
		WIPX_GetAddrFromName,
		WIPX_AddrCompare,
		WIPX_GetSocketPort,
		WIPX_SetSocketPort
	}
};

static const int	net_numlandrivers = MAX_NET_LANDRIVERS;
static int			net_landriverlevel;

// statistic counters
int		packetsSent = 0;
int		packetsReSent = 0;
int		packetsReceived = 0;
int		receivedDuplicateCount = 0;
int		shortPacketCount = 0;
int		droppedDatagrams;

// printed by net_stats, but nothing counts them
int		net_unreliable_sent = 0;
int		net_unreliable_recv = 0;
int		net_reliable_sent = 0;
int		net_reliable_recv = 0;

static int	myDriverLevel;

static struct
{
	unsigned int	length;
	unsigned int	sequence;
	byte			data[MAX_DATAGRAM];
} packetBuffer;

struct in_addr	banAddr = { 0 };
struct in_addr	banMask = { { { 255, 255, 255, 255 } } };

// menu state for a failed connect; these are private to this file,
// so the menu never sees them
static qboolean	connect_return_onerror;
static int		connect_key_dest;
static int		connect_menu_state;
static int		connect_return_state;
static char		connect_return_reason[32];

/*
================
NET_Ban_f
================
*/
void NET_Ban_f(void)
{
	char	addrStr[32];
	char	maskStr[32];
	void	(*print)(char *fmt, ...);

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			NET_Poll();
			return;
		}
		print = (void (*)(char *, ...))Con_Printf;
	}
	else
	{
		// FIXME: host_client was probably meant, not g_physents
		if (pr_global_struct->deathmatch != 0.0f && !((server_client_t *)g_physents)->privileged)
			return;
		print = SV_ClientPrintf;
	}

	switch (Cmd_Argc())
	{
	case 1:
		if (banAddr.s_addr)
		{
			Q_strcpy(addrStr, inet_ntoa(banAddr));
			Q_strcpy(maskStr, inet_ntoa(banMask));
			print("Banning %s [%s]\n", addrStr, maskStr);
		}
		else
			print("Banning not active\n");
		break;

	case 2:
		if (Q_strcasecmp(Cmd_Argv(1), "off"))
			banAddr.s_addr = inet_addr(Cmd_Argv(1));
		else
			banAddr.s_addr = 0x00000000;
		banMask.s_addr = 0xffffffff;
		break;

	case 3:
		banAddr.s_addr = inet_addr(Cmd_Argv(1));
		banMask.s_addr = inet_addr(Cmd_Argv(2));
		break;

	default:
		print("BAN ip_address [mask]\n");
		break;
	}
}

/*
================
Datagram_SendMessage
================
*/
int Datagram_SendMessage(qsocket_t *sock, sizebuf_t *data)
{
	unsigned int	dataLen;
	unsigned int	eom;
	unsigned int	sequence;

	Q_memcpy(sock->sendMessage, data->data, data->cursize);
	dataLen = data->cursize;
	sock->sendMessageLength = dataLen;

	eom = NETFLAG_EOM;
	if (dataLen > MAX_DATAGRAM)
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}

	sequence = sock->sendSequence++;
	packetBuffer.length = BigLong((NET_HEADERSIZE + dataLen) | eom | NETFLAG_DATA);
	packetBuffer.sequence = BigLong(sequence);
	Q_memcpy(packetBuffer.data, sock->sendMessage, dataLen);

	sock->canSend = false;

	if (net_landrivers[sock->landriver].Write(sock->socket, &packetBuffer, NET_HEADERSIZE + dataLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsSent++;
	return 1;
}

/*
================
SendMessageNext

Sends the next fragment of a reliable message after the last one was acked.
================
*/
static void SendMessageNext(qsocket_t *sock)
{
	unsigned int	dataLen;
	unsigned int	eom;

	eom = NETFLAG_EOM;
	dataLen = sock->sendMessageLength;
	if (dataLen > MAX_DATAGRAM)
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}

	packetBuffer.length = BigLong((NET_HEADERSIZE + dataLen) | eom | NETFLAG_DATA);
	packetBuffer.sequence = BigLong(sock->sendSequence++);
	Q_memcpy(packetBuffer.data, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (net_landrivers[sock->landriver].Write(sock->socket, &packetBuffer, NET_HEADERSIZE + dataLen, &sock->addr) == -1)
		return;

	sock->lastSendTime = net_time;
	packetsSent++;
}

/*
================
ReSendMessage

Sends the unacked fragment again.
================
*/
static int ReSendMessage(qsocket_t *sock)
{
	unsigned int	dataLen;
	unsigned int	eom;

	eom = NETFLAG_EOM;
	dataLen = sock->sendMessageLength;
	if (dataLen > MAX_DATAGRAM)
	{
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}

	packetBuffer.length = BigLong((NET_HEADERSIZE + dataLen) | eom | NETFLAG_DATA);
	packetBuffer.sequence = BigLong(sock->sendSequence - 1);
	Q_memcpy(packetBuffer.data, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (net_landrivers[sock->landriver].Write(sock->socket, &packetBuffer, NET_HEADERSIZE + dataLen, &sock->addr) == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsReSent++;
	return 1;
}

/*
================
Datagram_CanSendMessage
================
*/
qboolean Datagram_CanSendMessage(qsocket_t *sock)
{
	if (sock->sendNext)
		SendMessageNext(sock);

	return sock->canSend;
}

/*
================
Datagram_CanSendUnreliableMessage
================
*/
qboolean Datagram_CanSendUnreliableMessage(qsocket_t *sock)
{
	return true;
}

/*
================
Datagram_SendUnreliableMessage
================
*/
int Datagram_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data)
{
	int				packetLen;
	unsigned int	sequence;

	packetLen = NET_HEADERSIZE + data->cursize;

	packetBuffer.length = BigLong(packetLen | NETFLAG_UNRELIABLE);
	sequence = sock->unreliableSendSequence++;
	packetBuffer.sequence = BigLong(sequence);
	Q_memcpy(packetBuffer.data, data->data, data->cursize);

	if (net_landrivers[sock->landriver].Write(sock->socket, &packetBuffer, packetLen, &sock->addr) == -1)
		return -1;

	packetsSent++;
	return 1;
}

/*
================
Datagram_GetMessage
================
*/
int Datagram_GetMessage(qsocket_t *sock)
{
	unsigned int	length;
	unsigned int	flags;
	int				ret = 0;
	struct qsockaddr	readaddr;
	unsigned int	sequence;
	unsigned int	count;
	int				control;

	if (!sock->canSend && (net_time - sock->lastSendTime) > RESEND_TIME)
		ReSendMessage(sock);

	while (1)
	{
		length = net_landrivers[sock->landriver].Read(sock->socket, &packetBuffer, NET_DATAGRAMSIZE, &readaddr);

		if (length == 0)
			break;

		if (length == -1)
		{
			Con_Printf("Read Error\n");
			return -1;
		}

		if (net_landrivers[sock->landriver].AddrCompare(&readaddr, &sock->addr) != 0)
			continue;

		if (length < NET_HEADERSIZE)
		{
			shortPacketCount++;
			continue;
		}

		control = BigLong(packetBuffer.length);
		length = control & NETFLAG_LENGTH_MASK;
		flags = control & ~NETFLAG_LENGTH_MASK;

		if (flags & NETFLAG_CTL)
			continue;

		sequence = BigLong(packetBuffer.sequence);
		packetsReceived++;

		if (flags & NETFLAG_UNRELIABLE)
		{
			if (sequence >= sock->unreliableReceiveSequence)
			{
				if (sequence != sock->unreliableReceiveSequence)
				{
					count = sequence - sock->unreliableReceiveSequence;
					droppedDatagrams += count;
					Con_DPrintf("Dropped %u datagram(s)\n", count);
				}
				sock->unreliableReceiveSequence = sequence + 1;

				SZ_Clear(&net_message);
				SZ_Write(&net_message, packetBuffer.data, length - NET_HEADERSIZE);
				ret = 2;
			}
			else
			{
				Con_DPrintf("Got a stale datagram\n");
				ret = 0;
			}
			break;
		}

		if (flags & NETFLAG_ACK)
		{
			// only the last packet sent can be acked
			if (sock->sendSequence - sequence != 1)
			{
				Con_DPrintf("stale ack received\n");
				continue;
			}
			if (sequence != sock->ackSequence)
			{
				Con_DPrintf("duplicate ack received\n");
				continue;
			}

			sock->ackSequence++;
			if (sock->ackSequence != sock->sendSequence)
				Con_DPrintf("ack sequencing error\n");

			sock->sendMessageLength -= MAX_DATAGRAM;
			if (sock->sendMessageLength <= 0)
			{
				sock->canSend = true;
				sock->sendMessageLength = 0;
			}
			else
			{
				Q_memcpy(sock->sendMessage, sock->sendMessage + MAX_DATAGRAM, sock->sendMessageLength);
				sock->sendNext = true;
			}
			continue;
		}

		if (flags & NETFLAG_DATA)
		{
			packetBuffer.length = BigLong(NET_HEADERSIZE | NETFLAG_ACK);
			packetBuffer.sequence = BigLong(sequence);
			net_landrivers[sock->landriver].Write(sock->socket, &packetBuffer, NET_HEADERSIZE, &readaddr);

			if (sequence != sock->receiveSequence)
			{
				receivedDuplicateCount++;
				continue;
			}
			sock->receiveSequence++;

			length -= NET_HEADERSIZE;

			if (flags & NETFLAG_EOM)
			{
				SZ_Clear(&net_message);
				SZ_Write(&net_message, sock->receiveMessage, sock->receiveMessageLength);
				SZ_Write(&net_message, packetBuffer.data, length);
				sock->receiveMessageLength = 0;

				ret = 1;
				break;
			}

			Q_memcpy(sock->receiveMessage + sock->receiveMessageLength, packetBuffer.data, length);
			sock->receiveMessageLength += length;
		}
	}

	if (sock->sendNext)
		SendMessageNext(sock);

	return ret;
}

/*
================
PrintStats
================
*/
static void PrintStats(qsocket_t *s)
{
	Con_Printf("canSend = %4u   \n", s->canSend);
	Con_Printf("sendSeq = %4u   ", s->sendSequence);
	Con_Printf("recvSeq = %4u   \n", s->receiveSequence);
	Con_Printf("\n");
}

/*
================
NET_Stats_f
================
*/
void NET_Stats_f(void)
{
	qsocket_t	*s;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("unreliable messages sent   = %i\n", net_unreliable_sent);
		Con_Printf("unreliable messages recv   = %i\n", net_unreliable_recv);
		Con_Printf("reliable messages sent     = %i\n", net_reliable_sent);
		Con_Printf("reliable messages received = %i\n", net_reliable_recv);
		Con_Printf("packetsSent                = %i\n", packetsSent);
		Con_Printf("packetsReSent              = %i\n", packetsReSent);
		Con_Printf("packetsReceived            = %i\n", packetsReceived);
		Con_Printf("receivedDuplicateCount     = %i\n", receivedDuplicateCount);
		Con_Printf("shortPacketCount           = %i\n", shortPacketCount);
		Con_Printf("droppedDatagrams           = %i\n", droppedDatagrams);
	}
	else if (Q_strcmp(Cmd_Argv(1), "*") == 0)
	{
		for (s = net_activeSockets; s; s = s->next)
			PrintStats(s);
		for (s = net_freeSockets; s; s = s->next)
			PrintStats(s);
	}
	else
	{
		for (s = net_activeSockets; s; s = s->next)
		{
			if (Q_strcasecmp(Cmd_Argv(1), s->address) == 0)
				break;
		}
		if (s == NULL)
		{
			for (s = net_freeSockets; s; s = s->next)
			{
				if (Q_strcasecmp(Cmd_Argv(1), s->address) == 0)
					break;
			}
		}
		if (s == NULL)
			return;
		PrintStats(s);
	}
}

static qboolean	testInProgress = false;
static int		testPollCount;
static int		testDriver;
static int		testSocket;

static void Test_Poll(void *arg);
static pollprocedure_t	testPollProcedure = { NULL, 0.0, Test_Poll, NULL };

/*
================
Test_Poll

Prints the player info replies to "test".
================
*/
static void Test_Poll(void *arg)
{
	struct qsockaddr	clientaddr;
	int				control;
	unsigned int	len;
	char			name[32];
	char			address[64];
	int				colors;
	int				frags;
	int				connectTime;

	net_landriverlevel = testDriver;

	while (1)
	{
		len = net_landrivers[net_landriverlevel].Read(testSocket, net_message.data, net_message.maxsize, &clientaddr);
		if (len < sizeof(int))
			break;

		net_message.cursize = len;

		MSG_BeginReading();
		control = BigLong(*((int *)net_message.data));
		MSG_ReadLong();
		if (control == -1)
			break;
		if ((control & ~NETFLAG_LENGTH_MASK) != NETFLAG_CTL)
			break;
		if ((control & NETFLAG_LENGTH_MASK) != len)
			break;

		if (MSG_ReadByte() != CCREP_PLAYER_INFO)
			Sys_Error("Unexpected repsonse to Player Info request\n");

		MSG_ReadByte();	// player number
		Q_strcpy(name, MSG_ReadString());
		frags = MSG_ReadLong();
		colors = MSG_ReadLong();
		connectTime = MSG_ReadLong();
		Q_strcpy(address, MSG_ReadString());

		Con_Printf("%s\n  frags:%3i  colors:%u %u  time:%u\n  %s\n", name, frags, colors >> 4, colors & 0x0f, connectTime / 60, address);
	}

	testPollCount--;
	if (testPollCount > 0)
	{
		SchedulePollProcedure(&testPollProcedure, TEST_POLLTIME);
	}
	else
	{
		net_landrivers[net_landriverlevel].CloseSocket(testSocket);
		testInProgress = false;
	}
}

/*
================
Test_f

Asks a server for the info of each of its players.
================
*/
void Test_f(void)
{
	char	*host;
	int		n;
	int		max = TEST_MAXPLAYERS;
	struct qsockaddr	sendaddr;

	if (testInProgress)
		return;

	host = Cmd_Argv(1);

	if (host && hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (Q_strcasecmp(host, hostcache[n].name) == 0 && hostcache[n].driver == myDriverLevel)
			{
				max = hostcache[n].maxusers;
				net_landriverlevel = hostcache[n].ldriver;
				Q_memcpy(&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				goto JustDoIt;
			}
		}
	}

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (net_landrivers[net_landriverlevel].GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
		return;

JustDoIt:
	testSocket = net_landrivers[net_landriverlevel].OpenSocket(0);
	if (testSocket == -1)
		return;

	testDriver = net_landriverlevel;
	testInProgress = true;
	testPollCount = TEST_POLLCOUNT;

	for (n = 0; n < max; n++)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_PLAYER_INFO);
		MSG_WriteByte(&net_message, n);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Write(testSocket, net_message.data, net_message.cursize, &sendaddr);
	}
	SZ_Clear(&net_message);
	SchedulePollProcedure(&testPollProcedure, TEST_POLLTIME);
}

static qboolean	test2InProgress = false;
static int		test2Driver;
static int		test2Socket;

static void Test2_Poll(void *arg);
static pollprocedure_t	test2PollProcedure = { NULL, 0.0, Test2_Poll, NULL };

/*
================
Test2_Poll

Prints one server rule and asks for the next.
================
*/
static void Test2_Poll(void *arg)
{
	struct qsockaddr	clientaddr;
	int				control;
	unsigned int	len;
	char			name[256];
	char			value[256];

	net_landriverlevel = test2Driver;
	name[0] = 0;

	len = net_landrivers[net_landriverlevel].Read(test2Socket, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
		goto Reschedule;

	net_message.cursize = len;

	MSG_BeginReading();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		goto Error;
	if ((control & ~NETFLAG_LENGTH_MASK) != NETFLAG_CTL)
		goto Error;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		goto Error;

	if (MSG_ReadByte() != CCREP_RULE_INFO)
		goto Error;

	Q_strcpy(name, MSG_ReadString());
	if (name[0] == 0)
		goto Done;
	Q_strcpy(value, MSG_ReadString());

	Con_Printf("%-16.16s  %-16.16s\n", name, value);

	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREQ_RULE_INFO);
	MSG_WriteString(&net_message, name);
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	net_landrivers[net_landriverlevel].Write(test2Socket, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear(&net_message);

Reschedule:
	SchedulePollProcedure(&test2PollProcedure, TEST2_POLLTIME);
	return;

Error:
	Con_Printf("Unexpected response to Rule Info request\n");
Done:
	net_landrivers[net_landriverlevel].CloseSocket(test2Socket);
	test2InProgress = false;
}

/*
================
Test2_f

Lists the server rules of a server.
================
*/
void Test2_f(void)
{
	char	*host;
	int		n;
	struct qsockaddr	sendaddr;

	if (test2InProgress)
		return;

	host = Cmd_Argv(1);

	if (host && hostCacheCount)
	{
		for (n = 0; n < hostCacheCount; n++)
		{
			if (Q_strcasecmp(host, hostcache[n].name) == 0 && hostcache[n].driver == myDriverLevel)
			{
				net_landriverlevel = hostcache[n].ldriver;
				Q_memcpy(&sendaddr, &hostcache[n].addr, sizeof(struct qsockaddr));
				goto JustDoIt;
			}
		}
	}

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (!net_landrivers[net_landriverlevel].initialized)
			continue;

		// see if we can resolve the host name
		if (net_landrivers[net_landriverlevel].GetAddrFromName(host, &sendaddr) != -1)
			break;
	}
	if (net_landriverlevel == net_numlandrivers)
		return;

JustDoIt:
	test2Socket = net_landrivers[net_landriverlevel].OpenSocket(0);
	if (test2Socket == -1)
		return;

	test2Driver = net_landriverlevel;
	test2InProgress = true;

	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREQ_RULE_INFO);
	MSG_WriteString(&net_message, "");
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	net_landrivers[net_landriverlevel].Write(test2Socket, net_message.data, net_message.cursize, &sendaddr);
	SZ_Clear(&net_message);
	SchedulePollProcedure(&test2PollProcedure, TEST2_POLLTIME);
}

/*
================
Datagram_Init
================
*/
int Datagram_Init(void)
{
	int		i;
	int		csock;

	myDriverLevel = net_driverlevel;
	Cmd_AddCommand("net_stats", NET_Stats_f);

	if (COM_CheckParm("-nolan"))
		return -1;

	for (i = 0; i < net_numlandrivers; i++)
	{
		csock = net_landrivers[i].Init();
		if (csock == -1)
			continue;
		net_landrivers[i].initialized = true;
		net_landrivers[i].controlSock = csock;
	}

	Cmd_AddCommand("ban", NET_Ban_f);
	Cmd_AddCommand("test", Test_f);
	Cmd_AddCommand("test2", Test2_f);

	return 0;
}

/*
================
Datagram_Shutdown
================
*/
void Datagram_Shutdown(void)
{
	int		i;

	// shutdown the lan drivers
	for (i = 0; i < net_numlandrivers; i++)
	{
		if (net_landrivers[i].initialized)
		{
			net_landrivers[i].Shutdown();
			net_landrivers[i].initialized = false;
			net_landrivers[i].controlSock = -1;
		}
	}
}

/*
================
Datagram_Close
================
*/
void Datagram_Close(qsocket_t *sock)
{
	net_landrivers[sock->landriver].CloseSocket(sock->socket);
}

/*
================
Datagram_Listen
================
*/
void Datagram_Listen(qboolean state)
{
	int		i;

	for (i = 0; i < net_numlandrivers; i++)
	{
		if (net_landrivers[i].initialized)
			net_landrivers[i].Listen(state);
	}
}

/*
================
_Datagram_CheckNewConnections

Answers connectionless requests on the accept socket; returns a new qsocket
when a client connects.
================
*/
static qsocket_t *_Datagram_CheckNewConnections(void)
{
	struct qsockaddr	clientaddr;
	struct qsockaddr	newaddr;
	int				newsock;
	int				acceptsock;
	qsocket_t		*sock;
	qsocket_t		*s;
	unsigned int	len;
	int				command;
	int				control;
	int				ret;

	acceptsock = net_landrivers[net_landriverlevel].CheckNewConnections();
	if (acceptsock == -1)
		return NULL;

	SZ_Clear(&net_message);

	len = net_landrivers[net_landriverlevel].Read(acceptsock, net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(int))
		return NULL;
	net_message.cursize = len;

	MSG_BeginReading();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		return NULL;
	if ((control & ~NETFLAG_LENGTH_MASK) != NETFLAG_CTL)
		return NULL;
	if ((control & NETFLAG_LENGTH_MASK) != len)
		return NULL;

	command = MSG_ReadByte();
	if (command == CCREQ_SERVER_INFO)
	{
		if (Q_strcmp(MSG_ReadString(), "QUAKE") != 0)
			return NULL;

		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_SERVER_INFO);
		net_landrivers[net_landriverlevel].GetSocketAddr(acceptsock, &newaddr);
		MSG_WriteString(&net_message, net_landrivers[net_landriverlevel].AddrToString(&newaddr));
		MSG_WriteString(&net_message, hostname.string);
		MSG_WriteString(&net_message, sv.name);
		MSG_WriteByte(&net_message, net_activeconnections);
		MSG_WriteByte(&net_message, svs.maxclients);
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Write(acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	if (command == CCREQ_PLAYER_INFO)
	{
		int				playerNumber;
		int				activeNumber;
		int				clientNumber;
		server_client_t	*client;

		playerNumber = MSG_ReadByte();
		activeNumber = -1;
		client = NULL;
		for (clientNumber = 0; clientNumber < svs.maxclients; clientNumber++)
		{
			if (svs.clients[clientNumber].active)
			{
				activeNumber++;
				if (activeNumber == playerNumber)
				{
					client = &svs.clients[clientNumber];
					break;
				}
			}
		}
		if (!client)
			return NULL;

		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_PLAYER_INFO);
		MSG_WriteByte(&net_message, playerNumber);
		MSG_WriteString(&net_message, client->name);
		MSG_WriteLong(&net_message, client->colors);
		MSG_WriteLong(&net_message, (int)client->edict->v.frags);
		MSG_WriteLong(&net_message, client->netconnection ? (int)(net_time - client->netconnection->connecttime) : 0);
		MSG_WriteString(&net_message, client->netconnection ? client->netconnection->address : "");
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Write(acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	if (command == CCREQ_RULE_INFO)
	{
		char	*prevCvarName;
		cvar_t	*var;

		// find the search start location
		prevCvarName = MSG_ReadString();
		if (*prevCvarName)
		{
			var = Cvar_FindVar(prevCvarName);
			if (!var)
				return NULL;
			var = var->next;
		}
		else
			var = cvar_vars;

		// search for the next server cvar
		while (var)
		{
			if (var->server)
				break;
			var = var->next;
		}

		// send the response
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_RULE_INFO);
		if (var)
		{
			MSG_WriteString(&net_message, var->name);
			MSG_WriteString(&net_message, var->string);
		}
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Write(acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	if (command != CCREQ_CONNECT)
		return NULL;

	if (Q_strcmp(MSG_ReadString(), "QUAKE") != 0)
		return NULL;

	if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_REJECT);
		MSG_WriteString(&net_message, "Incompatible version.\n");
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Write(acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	// check for a ban
	if (clientaddr.sa_family == AF_INET
		&& (((struct sockaddr_in *)&clientaddr)->sin_addr.s_addr & banMask.s_addr) == banAddr.s_addr)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_REJECT);
		MSG_WriteString(&net_message, "You have been banned.\n");
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Write(acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	// see if this guy is already connected
	for (s = net_activeSockets; s; s = s->next)
	{
		if (s->driver != net_driverlevel)
			continue;
		ret = net_landrivers[net_landriverlevel].AddrCompare(&clientaddr, &s->addr);
		if (ret >= 0)
		{
			// it's somebody coming back in from a crash/disconnect
			// so close the old qsocket and let their retry get them back in
			if (ret != 0 || net_time - s->connecttime >= DUPLICATE_WINDOW)
			{
				NET_Close(s);
				return NULL;
			}

			// a duplicate connection request, so send a duplicate reply
			SZ_Clear(&net_message);
			// save space for the header, filled in later
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CCREP_ACCEPT);
			net_landrivers[net_landriverlevel].GetSocketAddr(s->socket, &newaddr);
			MSG_WriteLong(&net_message, net_landrivers[net_landriverlevel].GetSocketPort(&newaddr));
			*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
			net_landrivers[net_landriverlevel].Write(acceptsock, net_message.data, net_message.cursize, &clientaddr);
			SZ_Clear(&net_message);
			return NULL;
		}
	}

	// allocate a QSocket
	sock = NET_NewQSocket();
	if (sock == NULL)
	{
		// no room; try to let him know
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREP_REJECT);
		MSG_WriteString(&net_message, "Server is full.\n");
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Write(acceptsock, net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		return NULL;
	}

	// allocate a network socket
	newsock = net_landrivers[net_landriverlevel].OpenSocket(0);
	if (newsock == -1)
	{
		NET_FreeQSocket(sock);
		return NULL;
	}

	// connect to the client
	if (net_landrivers[net_landriverlevel].Connect() == -1)
	{
		net_landrivers[net_landriverlevel].CloseSocket(newsock);
		NET_FreeQSocket(sock);
		return NULL;
	}

	// everything is allocated, just fill in the details
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;
	sock->addr = clientaddr;
	Q_strcpy(sock->address, net_landrivers[net_landriverlevel].AddrToString(&clientaddr));

	// send him back the info about the server connection he has been allocated
	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CCREP_ACCEPT);
	net_landrivers[net_landriverlevel].GetSocketAddr(newsock, &newaddr);
	MSG_WriteLong(&net_message, net_landrivers[net_landriverlevel].GetSocketPort(&newaddr));
	*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
	net_landrivers[net_landriverlevel].Write(acceptsock, net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear(&net_message);

	return sock;
}

/*
================
Datagram_CheckNewConnections
================
*/
qsocket_t *Datagram_CheckNewConnections(void)
{
	qsocket_t	*ret = NULL;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			if ((ret = _Datagram_CheckNewConnections()) != NULL)
				break;
		}
	}
	return ret;
}

/*
================
_Datagram_SearchForHosts
================
*/
static void _Datagram_SearchForHosts(qboolean xmit)
{
	int		ret;
	int		n;
	int		i;
	struct qsockaddr	readaddr;
	struct qsockaddr	myaddr;
	int		control;
	char	*connectName;
	char	*string;

	net_landrivers[net_landriverlevel].GetSocketAddr(net_landrivers[net_landriverlevel].controlSock, &myaddr);
	if (xmit)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Broadcast(net_landrivers[net_landriverlevel].controlSock, net_message.data, net_message.cursize);
		SZ_Clear(&net_message);
	}

	while ((ret = net_landrivers[net_landriverlevel].Read(net_landrivers[net_landriverlevel].controlSock, net_message.data, net_message.maxsize, &readaddr)) > 0)
	{
		if (ret < sizeof(int))
			continue;
		net_message.cursize = ret;

		// is the cache full?
		if (hostCacheCount == HOSTCACHESIZE)
			continue;

		// don't answer our own query
		if (net_landrivers[net_landriverlevel].AddrCompare(&readaddr, &myaddr) >= 0)
			continue;

		MSG_BeginReading();
		control = BigLong(*((int *)net_message.data));
		MSG_ReadLong();
		if (control == -1)
			continue;
		if ((control & ~NETFLAG_LENGTH_MASK) != NETFLAG_CTL)
			continue;
		if ((control & NETFLAG_LENGTH_MASK) != ret)
			continue;

		if (MSG_ReadByte() != CCREP_SERVER_INFO)
			continue;

		connectName = MSG_ReadString();
		net_landrivers[net_landriverlevel].GetAddrFromName(connectName, &readaddr);
		// search the cache for this server
		for (n = 0; n < hostCacheCount; n++)
		{
			if (net_landrivers[net_landriverlevel].AddrCompare(&readaddr, &hostcache[n].addr) == 0)
				break;
		}

		// is it already there?
		if (n < hostCacheCount)
			continue;

		// add it
		n = hostCacheCount++;
		if (n >= HOSTCACHESIZE)
		{
			hostCacheCount = HOSTCACHESIZE;
			continue;
		}

		string = MSG_ReadString();
		Q_strcpy(hostcache[n].name, string);
		string = MSG_ReadString();
		Q_strcpy(hostcache[n].map, string);
		hostcache[n].users = MSG_ReadByte();
		hostcache[n].maxusers = MSG_ReadByte();
		if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		{
			char	oldName[sizeof(hostcache[n].name)];

			Q_strcpy(oldName, hostcache[n].name);
			Q_strcpy(hostcache[n].name, "*");
			Q_strcat(hostcache[n].name, oldName);
		}
		Q_memcpy(&hostcache[n].addr, &readaddr, sizeof(struct qsockaddr));
		hostcache[n].driver = net_driverlevel;
		hostcache[n].ldriver = net_landriverlevel;
		Q_strcpy(hostcache[n].cname, net_landrivers[net_landriverlevel].AddrToString(&readaddr));

		// check for a name conflict
		for (i = 0; i < hostCacheCount; i++)
		{
			if (i == n)
				continue;
			if (Q_strcasecmp(hostcache[n].name, hostcache[i].name) == 0)
			{
				i = Q_strlen(hostcache[n].name);
				if (i > 0)
				{
					if (i >= sizeof(hostcache[n].name) - 1 || hostcache[n].name[i - 1] <= '8')
						hostcache[n].name[i - 1]++;
					else
					{
						hostcache[n].name[i] = '0';
						hostcache[n].name[i + 1] = 0;
					}
				}
				i = -1;
			}
		}
	}
}

/*
================
Datagram_SearchForHosts
================
*/
void Datagram_SearchForHosts(qboolean xmit)
{
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (hostCacheCount == HOSTCACHESIZE)
			break;
		if (net_landrivers[net_landriverlevel].initialized)
			_Datagram_SearchForHosts(xmit);
	}
}

/*
================
_Datagram_Connect
================
*/
static qsocket_t *_Datagram_Connect(char *host)
{
	struct qsockaddr	sendaddr;
	struct qsockaddr	readaddr;
	qsocket_t	*sock;
	int			newsock;
	int			ret;
	int			reps;
	double		start_time;
	int			control;
	int			port;
	char		*reason;

	// see if we can resolve the host name
	if (net_landrivers[net_landriverlevel].GetAddrFromName(host, &sendaddr) == -1)
		return NULL;

	newsock = net_landrivers[net_landriverlevel].OpenSocket(0);
	if (newsock == -1)
		return NULL;

	sock = NET_NewQSocket();
	if (sock == NULL)
	{
		net_landrivers[net_landriverlevel].CloseSocket(newsock);
		return NULL;
	}
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;

	// connect to the host
	if (net_landrivers[net_landriverlevel].Connect() == -1)
		goto ErrorReturn;

	// send the connection request
	Con_Printf("trying...\n");
	SCR_UpdateScreen();
	reps = 0;
	start_time = net_time;

	while (1)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_CONNECT);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		net_landrivers[net_landriverlevel].Write(newsock, net_message.data, net_message.cursize, &sendaddr);
		SZ_Clear(&net_message);

		do
		{
			ret = net_landrivers[net_landriverlevel].Read(newsock, net_message.data, net_message.maxsize, &readaddr);
			// if we got something, validate it
			if (ret > 0)
			{
				// is it from the right place?
				if (net_landrivers[sock->landriver].AddrCompare(&readaddr, &sendaddr) != 0)
					ret = 0;
				else if (ret < sizeof(int))
					ret = 0;
				else
				{
					net_message.cursize = ret;
					MSG_BeginReading();

					control = BigLong(*((int *)net_message.data));
					MSG_ReadLong();
					if (control == -1)
						ret = 0;
					else if ((control & ~NETFLAG_LENGTH_MASK) != NETFLAG_CTL)
						ret = 0;
					else if ((control & NETFLAG_LENGTH_MASK) != ret)
						ret = 0;
				}
			}
		} while (ret == 0 && (Sys_FloatTime() - start_time) < CONNECT_TIMEOUT);

		if (ret)
			break;

		Con_Printf("still trying...\n");
		SCR_UpdateScreen();
		start_time = Sys_FloatTime();

		if (++reps >= CONNECT_RETRIES)
		{
			reason = "No Response\n";
			Con_Printf("%s\n", reason);
			goto ErrorReason;
		}
	}

	if (ret == -1)
	{
		reason = "Network Error\n";
		Con_Printf("%s\n", reason);
		goto ErrorReason;
	}

	ret = MSG_ReadByte();
	if (ret == CCREP_REJECT)
	{
		reason = MSG_ReadString();
		Con_Printf(reason);
		Q_strcpy(connect_return_reason, reason);
		goto ErrorReturn;
	}

	if (ret != CCREP_ACCEPT)
	{
		reason = "Bad Response\n";
		Con_Printf("%s\n", reason);
		goto ErrorReason;
	}

	Q_memcpy(&sock->addr, &sendaddr, sizeof(struct qsockaddr));
	port = MSG_ReadLong();
	net_landrivers[net_landriverlevel].SetSocketPort(&sock->addr, port);
	net_landrivers[net_landriverlevel].GetNameFromAddr(&sendaddr, sock->address);

	Con_Printf("Connection accepted\n");
	sock->lastMessageTime = Sys_FloatTime();

	// switch the connection to the specified address
	if (net_landrivers[net_landriverlevel].Connect() == -1)
	{
		reason = "Connect to game failed\n";
		Con_Printf("%s\n", reason);
		goto ErrorReason;
	}

	connect_return_onerror = false;
	return sock;

ErrorReason:
	Q_strcpy(connect_return_reason, reason);
ErrorReturn:
	NET_FreeQSocket(sock);
	net_landrivers[net_landriverlevel].CloseSocket(newsock);
	if (connect_return_onerror)
	{
		connect_return_onerror = false;
		connect_key_dest = key_menu;
		connect_menu_state = connect_return_state;
	}
	return NULL;
}

/*
================
Datagram_Connect
================
*/
qsocket_t *Datagram_Connect(char *host)
{
	qsocket_t	*ret = NULL;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			if ((ret = _Datagram_Connect(host)) != NULL)
				break;
		}
	}
	return ret;
}
