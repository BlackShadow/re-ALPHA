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
#include "winsock.h"

typedef struct net_landriver_s
{
	const char *name;
	qboolean initialized;
	int controlSock;

	int (*Init)(void);
	void (*Shutdown)(void);
	void (*Listen)(qboolean state);
	int (*OpenSocket)(int port);
	int (*Connect)(void);
	void (*CloseSocket)(int socket);
	int (*CheckNewConnections)(void);

	int (*Read)(int socket, void *buf, int len, struct sockaddr *addr);
	int (*Write)(int socket, void *buf, int len, struct sockaddr *addr);
	int (*Broadcast)(int socket, void *buf, int len);

	char *(*AddrToString)(struct sockaddr *addr);
	int (*GetSocketAddr)(int socket, struct sockaddr *addr);
	int (*GetNameFromAddr)(struct sockaddr *addr, char *name);
	int (*GetAddrFromName)(const char *name, struct sockaddr *addr);
	int (*AddrCompare)(struct sockaddr *addr1, struct sockaddr *addr2);
	int (*GetSocketPort)(struct sockaddr *addr);
	int (*SetSocketPort)(struct sockaddr *addr, int port);
} net_landriver_t;

int WINS_Init(void);
int WINS_Shutdown(void);
void WINS_Listen(qboolean state);
SOCKET WINS_OpenSocket(u_short port);
int WINS_Connect(void);
int WINS_CloseSocket(int socket);
int WINS_CheckNewConnections(void);
int WINS_Read(int socket, void *buf, int len, struct sockaddr_in *addr);
int WINS_Write(int socket, void *buf, int len, struct sockaddr_in *addr);
int WINS_Broadcast(int socket, void *buf, int len);
char *WINS_AddrToString(struct sockaddr_in *addr);
int WINS_GetSocketAddr(int socket, struct sockaddr_in *addr);
int WINS_GetNameFromAddr(struct sockaddr_in *addr, char *name);
int WINS_GetAddrFromName(char *name, struct sockaddr_in *addr);
int WINS_AddrCompare(struct sockaddr_in *addr1, struct sockaddr_in *addr2);
int WINS_GetSocketPort(struct sockaddr_in *addr);
int WINS_SetSocketPort(struct sockaddr_in *addr, u_short port);

static int WINS_OpenSocket_Wrapper(int port) { return (int)WINS_OpenSocket((u_short)port); }
static void WINS_CloseSocket_Wrapper(int socket) { (void)WINS_CloseSocket(socket); }
static int WINS_Read_Wrapper(int socket, void *buf, int len, struct sockaddr *addr) { return WINS_Read(socket, buf, len, (struct sockaddr_in *)addr); }
static int WINS_Write_Wrapper(int socket, void *buf, int len, struct sockaddr *addr) { return WINS_Write(socket, buf, len, (struct sockaddr_in *)addr); }
static int WINS_Broadcast_Wrapper(int socket, void *buf, int len) { return WINS_Broadcast(socket, buf, len); }
static char *WINS_AddrToString_Wrapper(struct sockaddr *addr) { return WINS_AddrToString((struct sockaddr_in *)addr); }
static int WINS_GetSocketAddr_Wrapper(int socket, struct sockaddr *addr) { return WINS_GetSocketAddr(socket, (struct sockaddr_in *)addr); }
static int WINS_GetNameFromAddr_Wrapper(struct sockaddr *addr, char *name) { return WINS_GetNameFromAddr((struct sockaddr_in *)addr, name); }
static int WINS_GetAddrFromName_Wrapper(const char *name, struct sockaddr *addr) { return WINS_GetAddrFromName((char *)name, (struct sockaddr_in *)addr); }
static int WINS_AddrCompare_Wrapper(struct sockaddr *addr1, struct sockaddr *addr2) { return WINS_AddrCompare((struct sockaddr_in *)addr1, (struct sockaddr_in *)addr2); }
static int WINS_GetSocketPort_Wrapper(struct sockaddr *addr) { return WINS_GetSocketPort((struct sockaddr_in *)addr); }
static int WINS_SetSocketPort_Wrapper(struct sockaddr *addr, int port) { return WINS_SetSocketPort((struct sockaddr_in *)addr, (u_short)port); }

int WIPX_Init(void);
void WIPX_Shutdown(void);
void WIPX_Listen(qboolean state);
int WIPX_OpenSocket(int port);
int WIPX_Connect(void);
void WIPX_CloseSocket(int socket);
int WIPX_CheckNewConnections(void);
int WIPX_Read(int socket, void *buf, int len, struct sockaddr *addr);
int WIPX_Write(int socket, void *buf, int len, struct sockaddr *addr);
int WIPX_Broadcast(int socket, void *buf, int len);
char *WIPX_AddrToString(struct sockaddr *addr);
int WIPX_GetSocketAddr(int socket, struct sockaddr *addr);
int WIPX_GetNameFromAddr(struct sockaddr *addr, char *name);
int WIPX_GetAddrFromName(char *name, struct sockaddr *addr);
int WIPX_AddrCompare(struct sockaddr *addr1, struct sockaddr *addr2);
int WIPX_GetSocketPort(struct sockaddr *addr);
int WIPX_SetSocketPort(struct sockaddr *addr, int port);

static int WIPX_GetAddrFromName_Wrapper(const char *name, struct sockaddr *addr) { return WIPX_GetAddrFromName((char *)name, addr); }
static void WIPX_CloseSocket_Wrapper(int socket) { WIPX_CloseSocket(socket); }

static net_landriver_t net_landrivers[] =
{
	{
		"TCPIP",
		false,
		-1,
		WINS_Init,
		(void (*)(void))WINS_Shutdown,
		WINS_Listen,
		WINS_OpenSocket_Wrapper,
		WINS_Connect,
		WINS_CloseSocket_Wrapper,
		WINS_CheckNewConnections,
		WINS_Read_Wrapper,
		WINS_Write_Wrapper,
		WINS_Broadcast_Wrapper,
		WINS_AddrToString_Wrapper,
		WINS_GetSocketAddr_Wrapper,
		WINS_GetNameFromAddr_Wrapper,
		WINS_GetAddrFromName_Wrapper,
		WINS_AddrCompare_Wrapper,
		WINS_GetSocketPort_Wrapper,
		WINS_SetSocketPort_Wrapper
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
		WIPX_CloseSocket_Wrapper,
		WIPX_CheckNewConnections,
		WIPX_Read,
		WIPX_Write,
		WIPX_Broadcast,
		WIPX_AddrToString,
		WIPX_GetSocketAddr,
		WIPX_GetNameFromAddr,
		WIPX_GetAddrFromName_Wrapper,
		WIPX_AddrCompare,
		WIPX_GetSocketPort,
		WIPX_SetSocketPort
	}
};

static const int net_numlandrivers = (int)(sizeof(net_landrivers) / sizeof(net_landrivers[0]));
static int net_landriverlevel;

static void _Datagram_SearchForHosts(qboolean xmit);
static qsocket_t *_Datagram_Connect(char *host);
static qsocket_t *_Datagram_CheckNewConnections(void);

void NET_Stats_f(void);
void NET_Ban_f(void);
void Test_f(void);
void Test2_f(void);

extern double net_time;
extern int net_driverlevel;
extern int net_activeconnections;
extern qsocket_t *net_activeSockets;
extern qsocket_t *net_freeSockets;
extern int net_numsockets;
extern int net_listening;

typedef struct datagram_packet_s
{
	int header;
	int sequence;
	byte data[1024];
} datagram_packet_t;

static datagram_packet_t dgram_packet;
extern int (*BigLong)(int l);
extern sizebuf_t net_message;

extern int hostCacheCount;

struct in_addr banAddr = {0};
struct in_addr banMask = {{ { 255, 255, 255, 255 } }};

int packetsSent;
int packetsReSent;
int packetsReceived;
int receivedDuplicateCount;
int shortPacketCount;
int droppedDatagrams;

extern int cmd_source;

static int test_driver;
static int test_count;
static int test_driver_save;
static int test2_driver;
static int test2_socket;
static qboolean test_in_progress;
static qboolean test2_in_progress;

static void Test_Poll(void *arg);
static void Test2_Poll(void *arg);
static pollprocedure_t testPollProcedure = { NULL, 0.0, Test_Poll, NULL };
static pollprocedure_t test2PollProcedure = { NULL, 0.0, Test2_Poll, NULL };

static qboolean connect_in_progress;
static int m_return_state;
static int m_state;
static int m_return_state_save;
static char m_return_reason[32];

static int myDriverLevel;

extern globalvars_t *pr_global_struct;
extern void *g_physents;
extern cvar_t *cvar_vars;

int net_unreliable_sent = 0;
int net_unreliable_recv = 0;
int net_reliable_sent = 0;
int net_reliable_recv = 0;

char net_version_string[32] = "1.0";
int packetBuffer_maxsize = (int)sizeof(dgram_packet);

static void SendMessageNext(qsocket_t *sock);
static void PrintStats(qsocket_t *sock);

void NET_Ban_f(void)
{
	void (*printfunc)(char *, ...);
	int argc;
	char addrstr[32];
	char maskstr[32];
	const char *cmd;
	const char *ipstr;
	const char *maskstr2;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			NET_Poll();
			return;
		}
		printfunc = (void (*)(char *, ...))Con_Printf;
	}
	else
	{
		if (pr_global_struct->deathmatch != 0.0f && !*(u32 *)((byte *)g_physents + 12))
			return;
		printfunc = (void (*)(char *, ...))SV_ClientPrintf;
	}

	argc = Cmd_Argc();
	switch (argc)
	{
	case 1:
		if (banAddr.S_un.S_addr)
		{
			Q_strcpy(addrstr, inet_ntoa(banAddr));
			Q_strcpy(maskstr, inet_ntoa(banMask));
			printfunc("Banning %s [%s]\n", addrstr, maskstr);
		}
		else
		{
			printfunc("Banning not active\n");
		}
		break;

	case 2:
		cmd = (const char *)Cmd_Argv(1u);
		if (Q_strcasecmp((char *)cmd, "off"))
		{
			ipstr = (const char *)Cmd_Argv(1u);
			banAddr.S_un.S_addr = inet_addr(ipstr);
		}
		else
		{
			banAddr.S_un.S_addr = 0;
		}
		banMask.S_un.S_addr = 0xFFFFFFFF;
		break;

	case 3:
		ipstr = (const char *)Cmd_Argv(1u);
		banAddr.S_un.S_addr = inet_addr(ipstr);
		maskstr2 = (const char *)Cmd_Argv(2u);
		banMask.S_un.S_addr = inet_addr(maskstr2);
		break;

	default:
		printfunc("BAN ip_address [mask]\n");
		break;
	}
}

int Datagram_SendMessage(qsocket_t *sock, sizebuf_t *data)
{
	unsigned int dataLen;
	unsigned int flags;
	int ret;
	unsigned int seq;

	Q_memcpy(sock->sendMessage, data->data, data->cursize);
	dataLen = data->cursize;
	sock->sendMessageLength = dataLen;

	flags = 0x80000;
	if (dataLen > 1024)
	{
		dataLen = 1024;
		flags = 0;
	}

	seq = sock->sendSequence;
	sock->sendSequence = seq + 1;
	dgram_packet.header = BigLong((dataLen + 8) | flags | 0x10000);
	dgram_packet.sequence = BigLong(seq);

	Q_memcpy(dgram_packet.data, sock->sendMessage, dataLen);

	sock->canSend = 0;

	ret = net_landrivers[sock->landriver].Write(sock->socket, &dgram_packet, dataLen + 8, (struct sockaddr *)&sock->addr);
	if (ret == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsSent++;

	return 1;
}

static void SendMessageNext(qsocket_t *sock)
{
	unsigned int dataLen;
	unsigned int flags;
	unsigned int seq;
	int ret;

	flags = 0x80000;
	dataLen = sock->sendMessageLength;
	if (dataLen > 1024)
	{
		dataLen = 1024;
		flags = 0;
	}

	dgram_packet.header = BigLong((dataLen + 8) | flags | 0x10000);
	seq = sock->sendSequence;
	sock->sendSequence = seq + 1;
	dgram_packet.sequence = BigLong(seq);

	Q_memcpy(dgram_packet.data, sock->sendMessage, dataLen);

	sock->sendNext = 0;

	ret = net_landrivers[sock->landriver].Write(sock->socket, &dgram_packet, dataLen + 8, (struct sockaddr *)&sock->addr);
	if (ret == -1)
		return;

	sock->lastSendTime = net_time;
	packetsSent++;
}

static int ReSendMessage(qsocket_t *sock)
{
	unsigned int dataLen;
	unsigned int flags;
	int ret;

	flags = 0x80000;
	dataLen = sock->sendMessageLength;
	if (dataLen > 1024)
	{
		dataLen = 1024;
		flags = 0;
	}

	dgram_packet.header = BigLong((dataLen + 8) | flags | 0x10000);
	dgram_packet.sequence = BigLong(sock->sendSequence - 1);

	Q_memcpy(dgram_packet.data, sock->sendMessage, dataLen);

	sock->sendNext = 0;

	ret = net_landrivers[sock->landriver].Write(sock->socket, &dgram_packet, dataLen + 8, (struct sockaddr *)&sock->addr);
	if (ret == -1)
		return -1;

	sock->lastSendTime = net_time;
	packetsReSent++;

	return 1;
}

qboolean Datagram_CanSendMessage(qsocket_t *sock)
{
	if (sock->sendNext)
		SendMessageNext(sock);

	return sock->canSend;
}

qboolean Datagram_CanSendUnreliableMessage(qsocket_t *sock)
{
	return 1;
}

int Datagram_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data)
{
	int packetLen;
	int ret;
	unsigned int seq;

	packetLen = data->cursize + 8;

	dgram_packet.header = BigLong(packetLen | 0x100000);
	seq = sock->unreliableSendSequence;
	sock->unreliableSendSequence = seq + 1;
	dgram_packet.sequence = BigLong(seq);

	Q_memcpy(dgram_packet.data, data->data, data->cursize);

	ret = net_landrivers[sock->landriver].Write(sock->socket, &dgram_packet, packetLen, (struct sockaddr *)&sock->addr);
	if (ret == -1)
		return -1;

	packetsSent++;
	return 1;
}

int Datagram_GetMessage(qsocket_t *sock)
{
	unsigned int length;
	int flags;
	int count;
	unsigned int sequence;
	int ackSequence;
	int fragmentLen;
	qboolean lastFragment;
	unsigned int recvSeq;
	unsigned int expectedAck;
	int newRecvLen;
	unsigned int unreliableSeq;
	unsigned int droppedCount;
	char readbuf[16];
	int retval;

	retval = 0;

	if (!sock->canSend && net_time - sock->lastSendTime > 1.0)
		ReSendMessage(sock);

	while (1)
	{
		length = net_landrivers[sock->landriver].Read(sock->socket, &dgram_packet, packetBuffer_maxsize, (struct sockaddr *)readbuf);
		if (!length)
			break;

		if (length == -1)
		{
			Con_Printf("Read Error\n");
			return -1;
		}

		if (net_landrivers[sock->landriver].AddrCompare((struct sockaddr *)readbuf, (struct sockaddr *)&sock->addr))
			continue;

		if (length < 8)
		{
			shortPacketCount++;
			continue;
		}

		const int control = BigLong(dgram_packet.header);
		count = (u16)control;
		flags = control & 0xFFFF0000;

		if (control >= 0)
		{
			sequence = BigLong(dgram_packet.sequence);
			packetsReceived++;

			if (flags & 0x100000)
			{
				unreliableSeq = sock->unreliableReceiveSequence;
				if (sequence >= unreliableSeq)
				{
					if (sequence != unreliableSeq)
					{
						droppedCount = sequence - unreliableSeq;
						droppedDatagrams += droppedCount;
						Con_DPrintf("Dropped %u datagram(s)\n", droppedCount);
					}

					sock->unreliableReceiveSequence = sequence + 1;
					SZ_Clear(&net_message);
					SZ_Write(&net_message, dgram_packet.data, count - 8);
					retval = 2;
				}
				else
				{
					Con_DPrintf("Got a stale datagram\n");
					retval = 0;
				}
				break;
			}

			if (flags & 0x20000)
			{
				expectedAck = sock->sendSequence;
				ackSequence = sock->ackSequence;

				if (expectedAck - sequence == 1)
				{
					if (sequence == ackSequence)
					{
						sock->ackSequence = ackSequence + 1;

						if (ackSequence + 1 != expectedAck)
							Con_DPrintf("ack sequencing error\n");

						newRecvLen = sock->sendMessageLength - 1024;
						sock->sendMessageLength = newRecvLen;

						if (newRecvLen <= 0)
						{
							sock->canSend = 1;
							sock->sendMessageLength = 0;
						}
						else
						{
							Q_memcpy(sock->sendMessage, sock->sendMessage + 1024, newRecvLen);
							sock->sendNext = 1;
						}
					}
					else
					{
						Con_DPrintf("duplicate ack received\n");
					}
				}
				else
				{
					Con_DPrintf("stale ack received\n");
				}
			}

			else if (flags & 0x10000)
			{

				dgram_packet.header = BigLong(0x20008);
				dgram_packet.sequence = BigLong(sequence);
				net_landrivers[sock->landriver].Write(sock->socket, &dgram_packet, 8, (struct sockaddr *)readbuf);

				recvSeq = sock->receiveSequence;
				if (sequence == recvSeq)
				{
					fragmentLen = count - 8;
					lastFragment = (flags & 0x80000) != 0;
					sock->receiveSequence = recvSeq + 1;

					if (lastFragment)
					{
						SZ_Clear(&net_message);
						SZ_Write(&net_message, sock->receiveMessage, sock->receiveMessageLength);
						SZ_Write(&net_message, dgram_packet.data, fragmentLen);
						sock->receiveMessageLength = 0;
						retval = 1;
						break;
					}

					Q_memcpy(sock->receiveMessage + sock->receiveMessageLength, dgram_packet.data, fragmentLen);
					sock->receiveMessageLength += fragmentLen;
				}
				else
				{
					receivedDuplicateCount++;
				}
			}
		}
	}

	if (sock->sendNext)
		SendMessageNext(sock);

	return retval;
}

static void PrintStats(qsocket_t *sock)
{
	Con_Printf("canSend = %4u   \n", sock->canSend);
	Con_Printf("sendSeq = %4u   ", sock->sendSequence);
	Con_Printf("recvSeq = %4u   \n", sock->receiveSequence);
	Con_Printf("\n");
}

void NET_Stats_f(void)
{
	qsocket_t *s;
	char *arg;
	qsocket_t *scan;
	char *hostname;

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
	else
	{
		arg = (char *)Cmd_Argv(1u);
		if (!Q_strcmp(arg, "*"))
		{

			for (s = net_activeSockets; s; s = s->next)
				PrintStats(s);

			for (s = net_freeSockets; s; s = s->next)
				PrintStats(s);
		}
		else
		{

			for (scan = net_activeSockets; scan; scan = scan->next)
			{
				hostname = (char *)Cmd_Argv(1u);
				if (!Q_strcasecmp(hostname, scan->address))
					break;
			}

			if (!scan)
			{
				for (scan = net_freeSockets; scan; scan = scan->next)
				{
					hostname = (char *)Cmd_Argv(1u);
					if (!Q_strcasecmp(hostname, scan->address))
						break;
				}
			}

			if (scan)
				PrintStats(scan);
		}
	}
}

static void Test_Poll(void *arg)
{
	unsigned int len;
	int control;
	char *name;
	int frags;
	int colors;
	int time;
	char *userinfo;
	char namebuf[32];
	char infobuf[64];
	char readbuf[16];

	(void)arg;
	net_landriverlevel = test_driver_save;

	while (1)
	{
		len = (unsigned int)net_landrivers[net_landriverlevel].Read(
			test_driver,
			net_message.data,
			net_message.maxsize,
			(struct sockaddr *)readbuf);
		if (len < 4)
			break;

		net_message.cursize = len;
		MSG_BeginReading();
		control = BigLong(*(u32 *)net_message.data);
		MSG_ReadLong();

		if (control == -1 || (control & 0xFFFF0000) != 0x80000000 || (u16)control != len)
			break;

		if (MSG_ReadByte() != 132)
			Sys_Error("Unexpected repsonse to Player Info request\n");

		MSG_ReadByte();
		name = MSG_ReadString();
		Q_strcpy(namebuf, name);
		frags = MSG_ReadLong();
		colors = MSG_ReadLong();
		time = MSG_ReadLong();
		userinfo = MSG_ReadString();
		Q_strcpy(infobuf, userinfo);
		Con_Printf("%s\n  frags:%3i  colors:%u %u  time:%u\n  %s\n", namebuf, frags, colors >> 4, colors & 0xF, time / 60, infobuf);
	}

	if (--test_count > 0)
	{
		SchedulePollProcedure(&testPollProcedure, 0.1);
	}
	else
	{
		net_landrivers[net_landriverlevel].CloseSocket(test_driver);
		test_in_progress = 0;
	}
}

static void Test2_Poll(void *arg)
{
	unsigned int len;
	int control;
	char *rule;
	char *value;
	char rulebuf[256];
	char valuebuf[256];
	char readbuf[16];

	(void)arg;
	net_landriverlevel = test2_driver;
	rulebuf[0] = 0;

	len = (unsigned int)net_landrivers[test2_driver].Read(
		test2_socket,
		net_message.data,
		net_message.maxsize,
		(struct sockaddr *)readbuf);
	if (len < 4)
		goto cleanup;

	net_message.cursize = len;
	MSG_BeginReading();
	control = BigLong(*(u32 *)net_message.data);
	MSG_ReadLong();

	if (control == -1 || (control & 0xFFFF0000) != 0x80000000 || (u16)control != len || MSG_ReadByte() != 133)
	{
		Con_Printf("Unexpected response to Rule Info request\n");
		goto close_socket;
	}

	rule = MSG_ReadString();
	Q_strcpy(rulebuf, rule);

	if (rulebuf[0])
	{
		value = MSG_ReadString();
		Q_strcpy(valuebuf, value);
		Con_Printf("%-16.16s  %-16.16s\n", rulebuf, valuebuf);

		SZ_Clear(&net_message);
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, 4);
		MSG_WriteString(&net_message, rulebuf);
		*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
		net_landrivers[net_landriverlevel].Write(test2_socket, net_message.data, net_message.cursize, (struct sockaddr *)readbuf);
		SZ_Clear(&net_message);

cleanup:
		SchedulePollProcedure(&test2PollProcedure, 0.05);
		return;
	}

close_socket:
	net_landrivers[net_landriverlevel].CloseSocket(test2_socket);
	test2_in_progress = 0;
}

int Datagram_Init(void)
{
	int i;
	int ret;

	myDriverLevel = net_driverlevel;

	Cmd_AddCommand("net_stats", NET_Stats_f);

	if (COM_CheckParm("-nolan"))
		return -1;

	for (i = 0; i < net_numlandrivers; i++)
	{
		ret = net_landrivers[i].Init();
		if (ret != -1)
		{
			net_landrivers[i].initialized = true;
			net_landrivers[i].controlSock = ret;
		}
	}

	Cmd_AddCommand("ban", NET_Ban_f);
	Cmd_AddCommand("test", Test_f);
	Cmd_AddCommand("test2", Test2_f);

	return 0;
}

void Test_f(void)
{
	int maxPlayers;
	int cacheIdx;
	char *host;
	int ret;
	int playerNum;
	byte addrbuf[16];

	maxPlayers = 16;
	cacheIdx = 0;

	if (test_in_progress)
		return;

	host = (char *)Cmd_Argv(1u);
	if (host && hostCacheCount)
	{
		for (cacheIdx = 0; cacheIdx < hostCacheCount; cacheIdx++)
		{
			if (!Q_strcasecmp(host, hostcache[cacheIdx].name) && hostcache[cacheIdx].driver == myDriverLevel)
			{
				maxPlayers = hostcache[cacheIdx].maxusers;
				net_landriverlevel = hostcache[cacheIdx].ldriver;
				Q_memcpy(addrbuf, hostcache[cacheIdx].addr, 16);
				goto send_requests;
			}
		}
	}

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			ret = net_landrivers[net_landriverlevel].GetAddrFromName(host, (struct sockaddr *)addrbuf);
			if (ret != -1)
				break;
		}
	}

	if (net_landriverlevel == net_numlandrivers)
		return;

send_requests:
	test_driver = net_landrivers[net_landriverlevel].OpenSocket(0);
	if (test_driver == -1)
		return;

	test_driver_save = net_landriverlevel;
	test_in_progress = true;
	test_count = 20;

	for (playerNum = 0; playerNum < maxPlayers; playerNum++)
	{
		SZ_Clear(&net_message);
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, 3);
		MSG_WriteByte(&net_message, playerNum);
		*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
		net_landrivers[net_landriverlevel].Write(test_driver, net_message.data, net_message.cursize, (struct sockaddr *)addrbuf);
	}

	SZ_Clear(&net_message);
	SchedulePollProcedure(&testPollProcedure, 0.1);
}

void Test2_f(void)
{
	int cacheIdx;
	char *host;
	int ret;
	byte addrbuf[16];

	cacheIdx = 0;

	if (test2_in_progress)
		return;

	host = (char *)Cmd_Argv(1u);
	if (host && hostCacheCount)
	{
		for (cacheIdx = 0; cacheIdx < hostCacheCount; cacheIdx++)
		{
			if (!Q_strcasecmp(host, hostcache[cacheIdx].name) && hostcache[cacheIdx].driver == myDriverLevel)
			{
				net_landriverlevel = hostcache[cacheIdx].ldriver;
				Q_memcpy(addrbuf, hostcache[cacheIdx].addr, 16);
				goto send_request;
			}
		}
	}

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			ret = net_landrivers[net_landriverlevel].GetAddrFromName(host, (struct sockaddr *)addrbuf);
			if (ret != -1)
				break;
		}
	}

	if (net_landriverlevel == net_numlandrivers)
		return;

send_request:
	test2_socket = net_landrivers[net_landriverlevel].OpenSocket(0);
	if (test2_socket == -1)
		return;

	test2_driver = net_landriverlevel;
	test2_in_progress = true;

	SZ_Clear(&net_message);
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, 4);
	MSG_WriteString(&net_message, "");
	*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
	net_landrivers[net_landriverlevel].Write(test2_socket, net_message.data, net_message.cursize, (struct sockaddr *)addrbuf);
	SZ_Clear(&net_message);
	SchedulePollProcedure(&test2PollProcedure, 0.05);
}

void Datagram_Shutdown(void)
{
	int i;

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

void Datagram_Close(qsocket_t *sock)
{
	net_landrivers[sock->landriver].CloseSocket(sock->socket);
}

void Datagram_Listen(qboolean state)
{
	int i;

	for (i = 0; i < net_numlandrivers; i++)
	{
		if (net_landrivers[i].initialized)
			net_landrivers[i].Listen(state);
	}
}

qsocket_t *Datagram_CheckNewConnections(void)
{
	qsocket_t *result;

	result = NULL;
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; ++net_landriverlevel)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			result = _Datagram_CheckNewConnections();
			if (result)
				break;
		}
	}

	return result;
}

qsocket_t *_Datagram_CheckNewConnections(void)
{
	int acceptSocket;
	unsigned int len;
	int control;
	int ret;
	char *version;
	char *socketAddr;
	int command;
	int clientNum;
	char *playerName;
	qsocket_t *sock;
	int newsock;
	char *addrString;
	int portNum;
	char readbuf[16];
	struct sockaddr_in clientAddr;

	acceptSocket = net_landrivers[net_landriverlevel].CheckNewConnections();
	if (acceptSocket == -1)
		return NULL;

	SZ_Clear(&net_message);

	len = (unsigned int)net_landrivers[net_landriverlevel].Read(
		acceptSocket,
		net_message.data,
		net_message.maxsize,
		(struct sockaddr *)&clientAddr);
	if (len < 4)
		return NULL;

	net_message.cursize = len;
	MSG_BeginReading();
	control = BigLong(*(u32 *)net_message.data);
	MSG_ReadLong();

	if (control == -1)
		return NULL;
	if ((control & 0xFFFF0000) != 0x80000000)
		return NULL;
	if ((u16)control != len)
		return NULL;

	command = MSG_ReadByte();
	switch (command)
	{
	case CCREQ_SERVER_INFO:
		version = MSG_ReadString();
		if (!Q_strcmp(version, "QUAKE"))
		{

			SZ_Clear(&net_message);
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CCREP_SERVER_INFO);

			net_landrivers[net_landriverlevel].GetSocketAddr(acceptSocket, (struct sockaddr *)readbuf);
			socketAddr = net_landrivers[net_landriverlevel].AddrToString((struct sockaddr *)readbuf);
			MSG_WriteString(&net_message, socketAddr);
			MSG_WriteString(&net_message, hostname.string);
			MSG_WriteString(&net_message, sv.name);
			MSG_WriteByte(&net_message, net_activeconnections);
			MSG_WriteByte(&net_message, svs.maxclients);
			MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);

			*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
			net_landrivers[net_landriverlevel].Write(acceptSocket, net_message.data, net_message.cursize, (struct sockaddr *)&clientAddr);
			SZ_Clear(&net_message);
		}
		return NULL;

	case CCREQ_PLAYER_INFO:
		clientNum = MSG_ReadByte();
		{
			int slot;
			int activeIndex;
			server_client_t *cl;

			cl = NULL;
			activeIndex = -1;

			for (slot = 0; slot < svs.maxclients; slot++)
			{
				if (svs.clients[slot].active)
				{
					if (++activeIndex == clientNum)
					{
						cl = &svs.clients[slot];
						break;
					}
				}
			}

			if (!cl)
				return NULL;

			SZ_Clear(&net_message);
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CCREP_PLAYER_INFO);
			MSG_WriteByte(&net_message, clientNum);
			MSG_WriteString(&net_message, cl->name);
			MSG_WriteLong(&net_message, cl->colors);
			MSG_WriteLong(&net_message, (int)cl->edict->v.frags);
			MSG_WriteLong(&net_message, cl->netconnection ? (int)(net_time - cl->netconnection->connecttime) : 0);
			MSG_WriteString(&net_message, cl->netconnection ? cl->netconnection->address : "");

			*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
			net_landrivers[net_landriverlevel].Write(acceptSocket, net_message.data, net_message.cursize, (struct sockaddr *)&clientAddr);
			SZ_Clear(&net_message);
		}
		return NULL;

	case CCREQ_RULE_INFO:
		playerName = MSG_ReadString();
		{
			cvar_t *rule;

			if (*playerName)
			{
				cvar_t *foundVar = Cvar_FindVar(playerName);
				if (!foundVar)
					return NULL;
				rule = foundVar->next;
			}
			else
			{
				rule = cvar_vars;
			}

			for (; rule; rule = rule->next)
			{
				if (rule->server)
					break;
			}

			SZ_Clear(&net_message);
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CCREP_RULE_INFO);

			if (rule)
			{
				MSG_WriteString(&net_message, rule->name);
				MSG_WriteString(&net_message, rule->string);
			}

			*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
			net_landrivers[net_landriverlevel].Write(acceptSocket, net_message.data, net_message.cursize, (struct sockaddr *)&clientAddr);
			SZ_Clear(&net_message);
			return NULL;
		}

	case CCREQ_CONNECT:
		version = MSG_ReadString();
		if (Q_strcmp(version, "QUAKE"))
		{
			return NULL;
		}
		else if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		{

			SZ_Clear(&net_message);
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CCREP_REJECT);
			MSG_WriteString(&net_message, "Incompatible version.\n");

			*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
			net_landrivers[net_landriverlevel].Write(acceptSocket, net_message.data, net_message.cursize, (struct sockaddr *)&clientAddr);
			SZ_Clear(&net_message);
			return NULL;
		}
		else
		{

			if (clientAddr.sin_family == AF_INET &&
				(clientAddr.sin_addr.S_un.S_addr & banMask.S_un.S_addr) == banAddr.S_un.S_addr)
			{
				SZ_Clear(&net_message);
				MSG_WriteLong(&net_message, 0);
				MSG_WriteByte(&net_message, CCREP_REJECT);
				MSG_WriteString(&net_message, "You have been banned.\n");

				*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
				net_landrivers[net_landriverlevel].Write(acceptSocket, net_message.data, net_message.cursize, (struct sockaddr *)&clientAddr);
				SZ_Clear(&net_message);
				return NULL;
			}
			else
			{

				for (sock = net_activeSockets; sock; sock = sock->next)
				{
					if (sock->driver == net_driverlevel)
					{
						ret = net_landrivers[net_landriverlevel].AddrCompare((struct sockaddr *)&clientAddr, (struct sockaddr *)&sock->addr);
						if (ret >= 0)
							break;
					}
				}

				if (!sock)
					goto try_allocate;

				if (ret || net_time - sock->connecttime >= 2.0)
				{

					NET_Close(sock);
					return NULL;
				}
				else
				{

					SZ_Clear(&net_message);
					MSG_WriteLong(&net_message, 0);
					MSG_WriteByte(&net_message, CCREP_ACCEPT);

					net_landrivers[net_landriverlevel].GetSocketAddr(sock->socket, (struct sockaddr *)readbuf);
					portNum = net_landrivers[net_landriverlevel].GetSocketPort((struct sockaddr *)readbuf);
					MSG_WriteLong(&net_message, portNum);

					*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
					net_landrivers[net_landriverlevel].Write(acceptSocket, net_message.data, net_message.cursize, (struct sockaddr *)&clientAddr);
					SZ_Clear(&net_message);
					return NULL;
				}

try_allocate:
				sock = NET_NewQSocket();
				if (sock)
				{

					newsock = net_landrivers[net_landriverlevel].OpenSocket(0);
					if (newsock == -1)
					{
						NET_FreeQSocket(sock);
						return NULL;
					}
					else if (net_landrivers[net_landriverlevel].Connect() == -1)
					{
						net_landrivers[net_landriverlevel].CloseSocket(newsock);
						NET_FreeQSocket(sock);
						return NULL;
					}
					else
					{

						sock->socket = newsock;
						sock->landriver = net_landriverlevel;
						memcpy(sock->addr.data, &clientAddr, sizeof(clientAddr));

						addrString = net_landrivers[net_landriverlevel].AddrToString((struct sockaddr *)&clientAddr);
						Q_strcpy(sock->address, addrString);

						SZ_Clear(&net_message);
						MSG_WriteLong(&net_message, 0);
						MSG_WriteByte(&net_message, CCREP_ACCEPT);

						net_landrivers[net_landriverlevel].GetSocketAddr(newsock, (struct sockaddr *)readbuf);
						portNum = net_landrivers[net_landriverlevel].GetSocketPort((struct sockaddr *)readbuf);
						MSG_WriteLong(&net_message, portNum);

						*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
						net_landrivers[net_landriverlevel].Write(acceptSocket, net_message.data, net_message.cursize, (struct sockaddr *)&clientAddr);
						SZ_Clear(&net_message);

						return sock;
					}
				}
				else
				{

					SZ_Clear(&net_message);
					MSG_WriteLong(&net_message, 0);
					MSG_WriteByte(&net_message, CCREP_REJECT);
					MSG_WriteString(&net_message, "Server is full.\n");

					*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
					net_landrivers[net_landriverlevel].Write(acceptSocket, net_message.data, net_message.cursize, (struct sockaddr *)&clientAddr);
					SZ_Clear(&net_message);
					return NULL;
				}
			}
		}

	default:
		return NULL;
	}
}

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

static void _Datagram_SearchForHosts(qboolean xmit)
{
	int ret;
	int len;
	int control;
	int cacheIdx;
	int i;
	char *connectName;
	char *hostname;
	char *mapName;
	byte readaddr[16];
	byte myaddr[16];

	net_landrivers[net_landriverlevel].GetSocketAddr(net_landrivers[net_landriverlevel].controlSock, (struct sockaddr *)myaddr);

	if (xmit)
	{

		SZ_Clear(&net_message);
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);

		*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
		net_landrivers[net_landriverlevel].Broadcast(net_landrivers[net_landriverlevel].controlSock, net_message.data, net_message.cursize);
		SZ_Clear(&net_message);
	}

	while (1)
	{
		ret = net_landrivers[net_landriverlevel].Read(
			net_landrivers[net_landriverlevel].controlSock,
			net_message.data,
			net_message.maxsize,
			(struct sockaddr *)readaddr);
		len = ret;
		if (ret <= 0)
			return;

		if ((unsigned int)ret < 4)
			continue;

		net_message.cursize = ret;

		if (hostCacheCount == HOSTCACHESIZE)
			continue;

		if (net_landrivers[net_landriverlevel].AddrCompare((struct sockaddr *)readaddr, (struct sockaddr *)myaddr) >= 0)
			continue;

		MSG_BeginReading();
		control = BigLong(*(u32 *)net_message.data);
		MSG_ReadLong();

		if (control == -1)
			continue;
		if ((control & 0xFFFF0000) != 0x80000000)
			continue;
		if ((u16)control != len)
			continue;
		if (MSG_ReadByte() != CCREP_SERVER_INFO)
			continue;

		connectName = MSG_ReadString();

		net_landrivers[net_landriverlevel].GetAddrFromName(connectName, (struct sockaddr *)readaddr);

		for (cacheIdx = 0; cacheIdx < hostCacheCount; cacheIdx++)
		{
			if (!net_landrivers[net_landriverlevel].AddrCompare((struct sockaddr *)readaddr, (struct sockaddr *)hostcache[cacheIdx].addr))
				break;
		}

		if (cacheIdx < hostCacheCount)
			continue;

		cacheIdx = hostCacheCount++;
		if (cacheIdx >= HOSTCACHESIZE)
		{
			hostCacheCount = HOSTCACHESIZE;
			continue;
		}

		hostname = MSG_ReadString();
		Q_strcpy(hostcache[cacheIdx].name, hostname);

		mapName = MSG_ReadString();
		Q_strcpy(hostcache[cacheIdx].map, mapName);

		hostcache[cacheIdx].users = MSG_ReadByte();
		hostcache[cacheIdx].maxusers = MSG_ReadByte();

		if (MSG_ReadByte() != NET_PROTOCOL_VERSION)
		{
			char oldName[16];
			Q_strcpy(oldName, hostcache[cacheIdx].name);
			Q_strcpy(hostcache[cacheIdx].name, "*");
			Q_strcat(hostcache[cacheIdx].name, oldName);
		}

		Q_memcpy(hostcache[cacheIdx].addr, readaddr, 16);
		hostcache[cacheIdx].driver = net_driverlevel;
		hostcache[cacheIdx].ldriver = net_landriverlevel;

		Q_strcpy(hostcache[cacheIdx].cname, net_landrivers[net_landriverlevel].AddrToString((struct sockaddr *)readaddr));

		for (i = 0; i < hostCacheCount; i++)
		{
			if (i != cacheIdx && !Q_strcasecmp(hostcache[cacheIdx].name, hostcache[i].name))
			{
				int nameLen = Q_strlen(hostcache[cacheIdx].name);
				if (nameLen > 0)
				{
					if (nameLen >= 15 || hostcache[cacheIdx].name[nameLen - 1] <= '8')
					{
						hostcache[cacheIdx].name[nameLen - 1]++;
					}
					else
					{
						hostcache[cacheIdx].name[nameLen] = '0';
						hostcache[cacheIdx].name[nameLen + 1] = 0;
					}
				}
				i = -1;
			}
		}
	}
}

qsocket_t *Datagram_Connect(char *host)
{
	qsocket_t *result;

	result = NULL;
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; ++net_landriverlevel)
	{
		if (net_landrivers[net_landriverlevel].initialized)
		{
			result = _Datagram_Connect(host);
			if (result)
				break;
		}
	}

	return result;
}

static qsocket_t *_Datagram_Connect(char *host)
{
	qsocket_t *sock;
	int newsock;
	int tryCount;
	int len;
	qboolean gotPacket;
	int control;
	int command;
	char *rejectMsg;
	char *responseMsg;
	int port;
	byte readbuf[16];
	byte addrbuf[16];
	double startTime;

	if (net_landrivers[net_landriverlevel].GetAddrFromName(host, (struct sockaddr *)addrbuf) == -1)
		return NULL;

	newsock = net_landrivers[net_landriverlevel].OpenSocket(0);
	if (newsock == -1)
		return NULL;

	sock = NET_NewQSocket();
	if (!sock)
	{
		net_landrivers[net_landriverlevel].CloseSocket(newsock);
		return NULL;
	}

	sock->socket = newsock;
	sock->landriver = net_landriverlevel;

	if (net_landrivers[net_landriverlevel].Connect() == -1)
		goto cleanup_socket;

	Con_Printf("trying...\n");
	SCR_UpdateScreen();
	tryCount = 0;
	startTime = net_time;

	while (1)
	{

		SZ_Clear(&net_message);
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_CONNECT);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);

		*(u32 *)net_message.data = BigLong((u16)net_message.cursize | 0x80000000);
		net_landrivers[net_landriverlevel].Write(newsock, net_message.data, net_message.cursize, (struct sockaddr *)addrbuf);
		SZ_Clear(&net_message);

		do
		{
			len = net_landrivers[net_landriverlevel].Read(
				newsock,
				net_message.data,
				net_message.maxsize,
				(struct sockaddr *)readbuf);
			gotPacket = len == 0;

			if (len > 0)
			{
				if (net_landrivers[sock->landriver].AddrCompare((struct sockaddr *)readbuf, (struct sockaddr *)addrbuf) ||
					(unsigned int)len < 4 ||
					(net_message.cursize = len,
					 MSG_BeginReading(),
					 control = BigLong(*(u32 *)net_message.data),
					 MSG_ReadLong(),
					 control == -1) ||
					(control & 0xFFFF0000) != 0x80000000 ||
					(u16)control != len)
				{
					len = 0;
				}

				gotPacket = len == 0;
			}
		}
		while (gotPacket && Sys_FloatTime() - startTime < 2.5);

		if (len)
			break;

		Con_Printf("still trying...\n");
		SCR_UpdateScreen();
		startTime = Sys_FloatTime();

		if (++tryCount >= 3)
		{
			rejectMsg = "No Response\n";
			Con_Printf("%s\n", rejectMsg);
			goto cleanup_with_message;
		}
	}

	if (len == -1)
	{
		rejectMsg = "Network Error\n";
		Con_Printf("%s\n", rejectMsg);
	}
	else
	{
		command = MSG_ReadByte();
		if (command == CCREP_REJECT)
		{
			responseMsg = MSG_ReadString();
			Con_Printf(responseMsg);
			Q_strcpy(m_return_reason, responseMsg);
			goto cleanup_socket;
		}

		if (command == CCREP_ACCEPT)
		{

			Q_memcpy(sock->addr.data, addrbuf, 16);
			port = MSG_ReadLong();
			net_landrivers[net_landriverlevel].SetSocketPort((struct sockaddr *)&sock->addr, port);
			net_landrivers[net_landriverlevel].GetNameFromAddr((struct sockaddr *)addrbuf, sock->address);

			Con_Printf("Connection accepted\n");
			sock->lastMessageTime = Sys_FloatTime();

			if (net_landrivers[net_landriverlevel].Connect() != -1)
			{
				connect_in_progress = 0;
				return sock;
			}

			rejectMsg = "Connect to game failed\n";
			Con_Printf("%s\n", rejectMsg);
		}
		else
		{
			rejectMsg = "Bad Response\n";
			Con_Printf("%s\n", rejectMsg);
		}
	}

cleanup_with_message:
	Q_strcpy(m_return_reason, rejectMsg);

cleanup_socket:
	NET_FreeQSocket(sock);

	net_landrivers[net_landriverlevel].CloseSocket(newsock);

	if (connect_in_progress)
	{
		connect_in_progress = 0;
		m_return_state = 3;
		m_state = m_return_state_save;
	}

	return NULL;
}
