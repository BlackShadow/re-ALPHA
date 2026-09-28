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

// net.h -- the engine's interface to the networking layer

#ifndef NET_H
#define NET_H

struct qsockaddr
{
	short			sa_family;
	unsigned char	sa_data[14];
};

#define NET_NAMELEN			64

#define NET_MAXMESSAGE		8192
#define NET_HEADERSIZE		(2 * sizeof(unsigned int))
#define NET_DATAGRAMSIZE	(MAX_DATAGRAM + NET_HEADERSIZE)

// NetHeader flags
#define NETFLAG_LENGTH_MASK	0x0000ffff
#define NETFLAG_DATA		0x00010000
#define NETFLAG_ACK			0x00020000
#define NETFLAG_NAK			0x00040000
#define NETFLAG_EOM			0x00080000
#define NETFLAG_UNRELIABLE	0x00100000
#define NETFLAG_CTL			0x80000000

#define NET_PROTOCOL_VERSION	3

// connectionless requests and replies, sent with NETFLAG_CTL
#define CCREQ_CONNECT		0x01
#define CCREQ_SERVER_INFO	0x02
#define CCREQ_PLAYER_INFO	0x03
#define CCREQ_RULE_INFO		0x04

#define CCREP_ACCEPT		0x81
#define CCREP_REJECT		0x82
#define CCREP_SERVER_INFO	0x83
#define CCREP_PLAYER_INFO	0x84
#define CCREP_RULE_INFO		0x85

typedef struct qsocket_s
{
	struct qsocket_s	*next;
	double				connecttime;
	double				lastMessageTime;
	double				lastSendTime;

	qboolean			disconnected;
	qboolean			canSend;
	qboolean			sendNext;

	int					driver;
	int					landriver;
	int					socket;
	void				*driverdata;

	unsigned int		ackSequence;
	unsigned int		sendSequence;
	unsigned int		unreliableSendSequence;
	int					sendMessageLength;
	byte				sendMessage[NET_MAXMESSAGE];

	unsigned int		receiveSequence;
	unsigned int		unreliableReceiveSequence;
	int					receiveMessageLength;
	byte				receiveMessage[NET_MAXMESSAGE];

	struct qsockaddr	addr;
	char				address[NET_NAMELEN];
} qsocket_t;

extern qsocket_t	*net_activeSockets;
extern qsocket_t	*net_freeSockets;
extern int			net_numsockets;

typedef struct net_driver_s
{
	char		*name;
	qboolean	initialized;
	int			(*Init)(void);
	void		(*Listen)(qboolean state);
	void		(*SearchForHosts)(qboolean xmit);
	qsocket_t	*(*Connect)(char *host);
	qsocket_t	*(*CheckNewConnections)(void);
	int			(*GetMessage)(qsocket_t *sock);
	int			(*SendMessage)(qsocket_t *sock, sizebuf_t *data);
	int			(*SendUnreliableMessage)(qsocket_t *sock, sizebuf_t *data);
	qboolean	(*CanSendMessage)(qsocket_t *sock);
	qboolean	(*CanSendUnreliableMessage)(qsocket_t *sock);
	void		(*Close)(qsocket_t *sock);
	void		(*Shutdown)(void);
	int			controlSock;
} net_driver_t;

#define MAX_NET_DRIVERS		2

typedef struct
{
	char		*name;
	qboolean	initialized;
	int			controlSock;
	int			(*Init)(void);
	void		(*Shutdown)(void);
	void		(*Listen)(qboolean state);
	int			(*OpenSocket)(int port);
	int			(*Connect)(void);
	void		(*CloseSocket)(int socket);
	int			(*CheckNewConnections)(void);
	int			(*Read)(int socket, void *buf, int len, struct qsockaddr *addr);
	int			(*Write)(int socket, void *buf, int len, struct qsockaddr *addr);
	int			(*Broadcast)(int socket, void *buf, int len);
	char		*(*AddrToString)(struct qsockaddr *addr);
	int			(*GetSocketAddr)(int socket, struct qsockaddr *addr);
	int			(*GetNameFromAddr)(struct qsockaddr *addr, char *name);
	int			(*GetAddrFromName)(char *name, struct qsockaddr *addr);
	int			(*AddrCompare)(struct qsockaddr *addr1, struct qsockaddr *addr2);
	int			(*GetSocketPort)(struct qsockaddr *addr);
	int			(*SetSocketPort)(struct qsockaddr *addr, int port);
} net_landriver_t;

#define MAX_NET_LANDRIVERS	2

extern int			net_numdrivers;
extern net_driver_t	net_drivers[MAX_NET_DRIVERS];
extern net_driver_t	net_vcr;

extern int			net_hostport;

extern int			net_driverlevel;
extern cvar_t		net_messagetimeout;

extern int			messagesSent;
extern int			messagesReceived;
extern int			unreliableMessagesSent;
extern int			unreliableMessagesReceived;

extern int			vcrFile;

// number of active server connections
extern int			net_activeconnections;

extern double		net_time;

qsocket_t *NET_NewQSocket(void);
void NET_FreeQSocket(qsocket_t *sock);
double SetNetTime(void);

#define HOSTCACHESIZE	8

typedef struct
{
	char				name[16];
	char				map[16];
	char				cname[32];
	int					users;
	int					maxusers;
	int					driver;
	int					ldriver;
	struct qsockaddr	addr;
} hostcache_t;

extern int			hostCacheCount;
extern hostcache_t	hostcache[HOSTCACHESIZE];

//============================================================================
//
// public network functions
//
//============================================================================

void NET_Init(void);
void NET_Shutdown(void);

// returns a new connection number if there is one pending, else NULL
qsocket_t *NET_CheckNewConnections(void);

// called by client to connect to a host. Returns NULL if not able to
qsocket_t *NET_Connect(char *host);

// Returns true or false if the given qsocket can currently accept a
// message to be transmitted.
qboolean NET_CanSendMessage(qsocket_t *sock);

// returns data in net_message sizebuf
// returns 0 if no data is waiting
// returns 1 if a message was received
// returns 2 if an unreliable message was received
// returns -1 if the connection died
int NET_GetMessage(qsocket_t *sock);

// returns 0 if the message connot be delivered reliably, but the connection
//		is still considered valid
// returns 1 if the message was sent properly
// returns -1 if the connection died
int NET_SendMessage(qsocket_t *sock, sizebuf_t *data);
int NET_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data);

// This is a reliable *blocking* send to all attached clients.
int NET_SendToAll(sizebuf_t *data, int blocktime);

// if a dead connection is returned by a get or send function, this function
// should be called when it is convenient
void NET_Close(qsocket_t *sock);

const char *NET_QSocketGetString(qsocket_t *sock);

// runs the scheduled poll procedures that are due
void NET_Poll(void);

typedef struct pollprocedure_s
{
	struct pollprocedure_s	*next;
	double					nextTime;
	void					(*procedure)(void *arg);
	void					*arg;
} pollprocedure_t;

void SchedulePollProcedure(pollprocedure_t *pp, double timeOffset);

extern qboolean	serialAvailable;
extern qboolean	ipxAvailable;
extern qboolean	tcpipAvailable;
extern char		my_ipx_address[NET_NAMELEN];
extern char		my_tcpip_address[NET_NAMELEN];
extern unsigned short	hostshort;

extern void (*NET_SetComPortConfig)(int portNumber, int port, int irq, int baud, qboolean useModem);
extern void (*NET_SetModemConfig)(int portNumber, const char *dialType, const char *clear, const char *init, const char *hangup);

extern cvar_t	config_com_port;
extern cvar_t	config_com_irq;
extern cvar_t	config_com_baud;
extern cvar_t	config_com_modem;
extern cvar_t	config_modem_dialtype;
extern cvar_t	config_modem_clear;
extern cvar_t	config_modem_init;
extern cvar_t	config_modem_hangup;

extern qboolean	slistInProgress;
extern qboolean	slistSilent;
extern qboolean	slistLocal;

void Slist_Send(void);

//============================================================================
//
// network drivers
//
//============================================================================

#include "net_loop.h"

// net_wins.c
int WINS_Init(void);
void WINS_Shutdown(void);
void WINS_Listen(qboolean state);
int WINS_OpenSocket(int port);
void WINS_CloseSocket(int socket);
int WINS_Connect(void);
int WINS_CheckNewConnections(void);
int WINS_Read(int socket, void *buf, int len, struct qsockaddr *addr);
int WINS_Write(int socket, void *buf, int len, struct qsockaddr *addr);
int WINS_Broadcast(int socket, void *buf, int len);
char *WINS_AddrToString(struct qsockaddr *addr);
int WINS_GetSocketAddr(int socket, struct qsockaddr *addr);
int WINS_GetNameFromAddr(struct qsockaddr *addr, char *name);
int WINS_GetAddrFromName(char *name, struct qsockaddr *addr);
int WINS_AddrCompare(struct qsockaddr *addr1, struct qsockaddr *addr2);
int WINS_GetSocketPort(struct qsockaddr *addr);
int WINS_SetSocketPort(struct qsockaddr *addr, int port);

// net_wipx.c
int WIPX_Init(void);
void WIPX_Shutdown(void);
void WIPX_Listen(qboolean state);
int WIPX_OpenSocket(int port);
void WIPX_CloseSocket(int socket);
int WIPX_Connect(void);
int WIPX_CheckNewConnections(void);
int WIPX_Read(int socket, void *buf, int len, struct qsockaddr *addr);
int WIPX_Write(int socket, void *buf, int len, struct qsockaddr *addr);
int WIPX_Broadcast(int socket, void *buf, int len);
char *WIPX_AddrToString(struct qsockaddr *addr);
int WIPX_GetSocketAddr(int socket, struct qsockaddr *addr);
int WIPX_GetNameFromAddr(struct qsockaddr *addr, char *name);
int WIPX_GetAddrFromName(char *name, struct qsockaddr *addr);
int WIPX_AddrCompare(struct qsockaddr *addr1, struct qsockaddr *addr2);
int WIPX_GetSocketPort(struct qsockaddr *addr);
int WIPX_SetSocketPort(struct qsockaddr *addr, int port);

// net_datagram.c
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

#endif // NET_H
