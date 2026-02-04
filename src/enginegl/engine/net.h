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

 // net.h -- network interface
 // NOTE: This is a skeleton header derived from reverse engineering.
 // Network structures and functions will be filled during translation.

#ifndef NET_H
#define NET_H

 // =============================================================================
 // Network Constants
 // =============================================================================
#define NET_MAXMESSAGE      8192
#define NET_NAMELEN         64

 // Connectionless control messages (Quake-style)
#define CCREQ_CONNECT		0x01
#define CCREQ_SERVER_INFO	0x02
#define CCREQ_PLAYER_INFO	0x03
#define CCREQ_RULE_INFO		0x04

#define CCREP_ACCEPT		0x81
#define CCREP_REJECT		0x82
#define CCREP_SERVER_INFO	0x83
#define CCREP_PLAYER_INFO	0x84
#define CCREP_RULE_INFO		0x85

 // Connectionless query protocol version (CCREP_SERVER_INFO replies).
 // compares MSG_ReadByte against 3.
#define NET_PROTOCOL_VERSION 3

 // =============================================================================
 // Network Structures
 // =============================================================================

typedef struct
{
    byte    data[16]; // sockaddr_in is 16 bytes
} netadr_t;

typedef struct qsocket_s
{
    struct qsocket_s    *next; // 0
    double              connecttime; // 8
    double              lastMessageTime; // 16
    double              lastSendTime; // 24
    int                 disconnected; // 32
    int                 canSend; // 36
    int                 sendNext; // 40
    int                 driver; // 44 (sock[11])
    int                 landriver; // 48 (sock[12])
    int                 socket; // 52 (sock[13])
    void                *driverdata; // 56 (sock[14])
    unsigned int        ackSequence; // 60 (sock[15])
    unsigned int        sendSequence; // 64 (sock[16])
    unsigned int        unreliableSendSequence; // 68 (sock[17])
    int                 sendMessageLength; // 72 (sock[18])
    byte                sendMessage[NET_MAXMESSAGE]; // 76
    
 // 76 + 8192 = 8268
 // recvSequence is sock[2067] -> 2067 * 4 = 8268
    unsigned int        receiveSequence; // 8268
    unsigned int        unreliableReceiveSequence; // 8272
    int                 receiveMessageLength; // 8276
    byte                receiveMessage[NET_MAXMESSAGE]; // 8280
    
 // 8280 + 8192 = 16472
    netadr_t            addr; // 16472
    char                address[NET_NAMELEN]; // 16488
} qsocket_t;

 // =============================================================================
 // Network constants
 // =============================================================================

 // NOTE: Network constants to be filled from reverse engineering

 // #define NET_MAXMESSAGE 8192/ / max message size (defined above)
 // #define NET_NAMELEN 64/ / max address string length (defined above)
#define NET_HEADERSIZE      8 // protocol header bytes

 // =============================================================================
 // Host cache (server browser)
 // =============================================================================

#define HOSTCACHESIZE 8

typedef struct hostcache_s
{
	char name[16];
	char map[16];
	char cname[32]; // connect string
	int users;
	int maxusers;
	int driver;
	int ldriver;
	byte addr[16]; // sockaddr
} hostcache_t;

extern int hostCacheCount;
extern hostcache_t hostcache[HOSTCACHESIZE];

 // Number of active server connections (server-side).
extern int net_activeconnections;

 // Current driver index in net_main.c loops.
extern int net_driverlevel;

 // =============================================================================
 // Scheduled polling (net_main.c)
 // =============================================================================

typedef struct pollprocedure_s
{
	struct pollprocedure_s *next;
	double nextTime;
	void (*procedure)(void *);
	void *arg;
} pollprocedure_t;

 // Local addresses (set by network drivers)
extern char my_ipx_address[64];
extern char my_tcpip_address[64];

 // Driver availability flags
extern qboolean serialAvailable;
extern qboolean ipxAvailable;
extern qboolean tcpipAvailable;
extern unsigned short hostshort;

 // =============================================================================
 // Network driver interface
 // =============================================================================

 // Windows headers define GetMessage/ SendMessage macros (A/ W variants) which
 // conflict with our net_driver_t fields. Ensure the field names aren't macro-expanded.
#ifdef GetMessage
#undef GetMessage
#endif
#ifdef SendMessage
#undef SendMessage
#endif

 // NOTE: Network driver structure to be filled
 // Provides abstraction for different network protocols (Winsock, IPX, etc.)

typedef struct net_driver_s
{
    char    *name;
    qboolean initialized;
    int (*Init)(void);
    void (*Listen)(qboolean state);
    void (*SearchForHosts)(qboolean xmit);
    qsocket_t *(*Connect)(char *host);
    qsocket_t *(*CheckNewConnections)(void);
    int (*GetMessage)(qsocket_t *sock);
    int (*SendMessage)(qsocket_t *sock, sizebuf_t *data);
    int (*SendUnreliableMessage)(qsocket_t *sock, sizebuf_t *data);
    qboolean (*CanSendMessage)(qsocket_t *sock);
    qboolean (*CanSendUnreliableMessage)(qsocket_t *sock);
    void (*Close)(qsocket_t *sock);
    void (*Shutdown)(void);
    int controlSock;
} net_driver_t;

 // =============================================================================
 // Network functions
 // =============================================================================

 // NOTE: Network system functions to be filled from net_main.c

void NET_Init(void);
void NET_Shutdown(void);
qsocket_t *NET_NewQSocket(void);
void NET_FreeQSocket(qsocket_t *sock);
double SetNetTime(void);
qsocket_t *NET_CheckNewConnections(void);
int NET_GetMessage(qsocket_t *sock);
int NET_SendMessage(qsocket_t *sock, sizebuf_t *data);
int NET_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data);
qboolean NET_CanSendMessage(qsocket_t *sock);
qsocket_t *NET_Connect(char *host);
void NET_Close(qsocket_t *sock);
int NET_SendToAll(sizebuf_t *data, int blocktime);
void NET_Poll(void);
void SchedulePollProcedure(pollprocedure_t *proc, double timeOffset);
const char *NET_QSocketGetString(qsocket_t *sock);
void Slist_Send(void);

 // =============================================================================
 // Loopback (local) networking
 // =============================================================================

 // NOTE: Loopback functions to be filled from net_loop.c

int Loop_Init(void);
void Loop_Listen(qboolean state);
void Loop_SearchForHosts(qboolean xmit);
qsocket_t *Loop_Connect(char *host);
qsocket_t *Loop_CheckNewConnections(void);
int Loop_GetMessage(qsocket_t *sock);
int Loop_SendMessage(qsocket_t *sock, sizebuf_t *data);
int Loop_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data);
qboolean Loop_CanSendMessage(qsocket_t *sock);
qboolean Loop_CanSendUnreliableMessage(qsocket_t *sock);
void Loop_Close(qsocket_t *sock);
void Loop_Shutdown(void);

 // =============================================================================
 // Datagram (UDP/ IPX) networking
 // =============================================================================

int Datagram_Init(void);
void Datagram_Listen(qboolean state);
void Datagram_SearchForHosts(qboolean xmit);
qsocket_t *Datagram_Connect(char *host);
qsocket_t *Datagram_CheckNewConnections(void);
int Datagram_GetMessage(qsocket_t *sock);
int Datagram_SendMessage(qsocket_t *sock, sizebuf_t *data);
int Datagram_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data);
qboolean Datagram_CanSendMessage(qsocket_t *sock);
qboolean Datagram_CanSendUnreliableMessage(qsocket_t *sock);
void Datagram_Close(qsocket_t *sock);
void Datagram_Shutdown(void);

 // =============================================================================

#endif // NET_H
