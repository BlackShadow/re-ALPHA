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

// net_wins.c -- Winsock UDP/IP driver

#include "quakedef.h"
#include "winquake.h"

#define MAXHOSTNAMELEN		256
#define WINSOCK_VERSION_1_1	0x0101

static int		winsock_lib_initialized;
static int		winsock_initialized = 0;

static int		net_acceptsocket = INVALID_SOCKET;	// socket for fielding new connections
static int		net_controlsocket = INVALID_SOCKET;
static int		net_broadcastsocket = 0;
static u_long	myAddr = 0;

static double	blocktime = 0.0;

static struct sockaddr_in	broadcastaddr;

char			my_tcpip_address[NET_NAMELEN];

int (PASCAL FAR *pWSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
int (PASCAL FAR *pWSACleanup)(void);
int (PASCAL FAR *pWSAGetLastError)(void);
SOCKET (PASCAL FAR *psocket)(int af, int type, int protocol);
int (PASCAL FAR *pioctlsocket)(SOCKET s, long cmd, u_long FAR *argp);
int (PASCAL FAR *psetsockopt)(SOCKET s, int level, int optname, const char FAR *optval, int optlen);
int (PASCAL FAR *precvfrom)(SOCKET s, char FAR *buf, int len, int flags, struct sockaddr FAR *from, int FAR *fromlen);
int (PASCAL FAR *psendto)(SOCKET s, const char FAR *buf, int len, int flags, const struct sockaddr FAR *to, int tolen);
int (PASCAL FAR *pclosesocket)(SOCKET s);
int (PASCAL FAR *pgethostname)(char FAR *name, int namelen);
struct hostent FAR *(PASCAL FAR *pgethostbyname)(const char FAR *name);
struct hostent FAR *(PASCAL FAR *pgethostbyaddr)(const char FAR *addr, int len, int type);
int (PASCAL FAR *pgetsockname)(SOCKET s, struct sockaddr FAR *name, int FAR *namelen);

//=============================================================================

/*
================
BlockingHook

Keeps the message pump running during blocking winsock calls and gives up
after two seconds.
================
*/
BOOL PASCAL FAR BlockingHook(void)
{
	MSG		msg;
	BOOL	ret;

	if ((Sys_FloatTime() - blocktime) <= 2.0)
	{
		// get the next message, if any
		ret = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);

		// if we got one, process it
		if (ret)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// TRUE if we got a message
		return ret;
	}

	WSACancelBlockingCall();
	return FALSE;
}

/*
================
WINS_Init
================
*/
int WINS_Init(void)
{
	int			i;
	char		buff[MAXHOSTNAMELEN];
	char		*p;
	char		c;
	int			r;
	WORD		wVersionRequested;
	HINSTANCE	hInst;
	WSADATA		winsockdata;
	struct hostent		*local;
	struct qsockaddr	addr;

	// load winsock at run time so the game still starts without it
	hInst = LoadLibrary("wsock32.dll");
	if (hInst == NULL)
	{
		Con_Printf("Failed to load wsock32.dll\n");
		winsock_lib_initialized = false;
		return -1;
	}

	winsock_lib_initialized = true;

	pWSAStartup = (void *)GetProcAddress(hInst, "WSAStartup");
	pWSACleanup = (void *)GetProcAddress(hInst, "WSACleanup");
	pWSAGetLastError = (void *)GetProcAddress(hInst, "WSAGetLastError");
	psocket = (void *)GetProcAddress(hInst, "socket");
	pioctlsocket = (void *)GetProcAddress(hInst, "ioctlsocket");
	psetsockopt = (void *)GetProcAddress(hInst, "setsockopt");
	precvfrom = (void *)GetProcAddress(hInst, "recvfrom");
	psendto = (void *)GetProcAddress(hInst, "sendto");
	pclosesocket = (void *)GetProcAddress(hInst, "closesocket");
	pgethostname = (void *)GetProcAddress(hInst, "gethostname");
	pgethostbyname = (void *)GetProcAddress(hInst, "gethostbyname");
	pgethostbyaddr = (void *)GetProcAddress(hInst, "gethostbyaddr");
	pgetsockname = (void *)GetProcAddress(hInst, "getsockname");

	if (!pWSAStartup || !pWSACleanup || !pWSAGetLastError ||
		!psocket || !pioctlsocket || !psetsockopt ||
		!precvfrom || !psendto || !pclosesocket ||
		!pgethostname || !pgethostbyname || !pgethostbyaddr ||
		!pgetsockname)
	{
		Con_Printf("Couldn't get winsock function pointers\n");
		return -1;
	}

	if (COM_CheckParm("-noudp"))
		return -1;

	if (winsock_initialized == 0)
	{
		wVersionRequested = WINSOCK_VERSION_1_1;
		r = pWSAStartup(wVersionRequested, &winsockdata);
		if (r)
		{
			Con_Printf("Winsock initialization failed\n");
			return -1;
		}
	}
	winsock_initialized++;

	// determine my name & address
	if (pgethostname(buff, MAXHOSTNAMELEN) == 0)
	{
		blocktime = Sys_FloatTime();
		WSASetBlockingHook((FARPROC)BlockingHook);
		local = pgethostbyname(buff);
		WSAUnhookBlockingHook();
		if (local == NULL)
		{
			Con_DPrintf("Winsock TCP/IP initialization failed\n");
			if (--winsock_initialized == 0)
				pWSACleanup();
			return -1;
		}

		myAddr = *(u_long *)local->h_addr_list[0];

		// if the hostname cvar isn't set, set it to the machine name
		if (Q_strcmp(hostname.string, "unnamed") == 0)
		{
			// see if it's a text IP address (well, close enough)
			p = buff;
			if (*p)
			{
				do
				{
					c = *p;
					if ((c < '0' || c > '9') && c != '.')
						break;
					p++;
				} while (*p);
			}

			// if it is a real name, strip off the domain; we only want the host
			if (*p)
			{
				for (i = 0; i < 15; i++)
				{
					if (buff[i] == '.')
						break;
				}
				buff[i] = 0;
			}
			Cvar_Set("hostname", buff);
		}
	}

	if ((net_controlsocket = WINS_OpenSocket(0)) == INVALID_SOCKET)
	{
		Con_SafePrintf("WINS_Init: Unable to open control socket\n");
		if (--winsock_initialized == 0)
			pWSACleanup();
		return -1;
	}

	broadcastaddr.sin_family = AF_INET;
	broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
	broadcastaddr.sin_port = htons(hostshort);

	WINS_GetSocketAddr(net_controlsocket, &addr);
	Q_strcpy(my_tcpip_address, WINS_AddrToString(&addr));
	p = Q_strstr(my_tcpip_address, ":");
	if (p)
		*p = 0;

	Con_DPrintf("Winsock TCP/IP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

/*
================
WINS_Shutdown
================
*/
void WINS_Shutdown(void)
{
	WINS_Listen(false);
	WINS_CloseSocket(net_controlsocket);
	if (--winsock_initialized == 0)
		pWSACleanup();
}

/*
================
WINS_Listen
================
*/
void WINS_Listen(qboolean state)
{
	// enable listening
	if (state)
	{
		if (net_acceptsocket != INVALID_SOCKET)
			return;
		if ((net_acceptsocket = WINS_OpenSocket(hostshort)) == INVALID_SOCKET)
			Sys_Error("WINS_Listen: Unable to open accept socket\n");
		return;
	}

	// disable listening
	if (net_acceptsocket == INVALID_SOCKET)
		return;
	WINS_CloseSocket(net_acceptsocket);
	net_acceptsocket = INVALID_SOCKET;
}

/*
================
WINS_OpenSocket
================
*/
int WINS_OpenSocket(int port)
{
	int					newsocket;
	struct sockaddr_in	address;
	u_long				_true = 1;

	if ((newsocket = psocket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (pioctlsocket(newsocket, FIONBIO, &_true) == -1)
	{
		pclosesocket(newsocket);
		return INVALID_SOCKET;
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((u_short)port);
	if (bind(newsocket, (struct sockaddr *)&address, sizeof(address)) != 0)
		Sys_Error("bind");

	return newsocket;
}

/*
================
WINS_CloseSocket
================
*/
void WINS_CloseSocket(int socket)
{
	if (socket == net_broadcastsocket)
		net_broadcastsocket = 0;
	pclosesocket(socket);
}

/*
================
WINS_Connect
================
*/
int WINS_Connect(void)
{
	return 0;
}

/*
================
WINS_CheckNewConnections
================
*/
int WINS_CheckNewConnections(void)
{
	char	buf[4096];

	if (net_acceptsocket == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (precvfrom(net_acceptsocket, buf, sizeof(buf), MSG_PEEK, NULL, NULL) > 0)
		return net_acceptsocket;

	return INVALID_SOCKET;
}

/*
================
WINS_Read
================
*/
int WINS_Read(int socket, void *buf, int len, struct qsockaddr *addr)
{
	int		addrlen = sizeof(struct qsockaddr);
	int		ret;
	int		err;

	ret = precvfrom(socket, buf, len, 0, (struct sockaddr *)addr, &addrlen);
	if (ret == -1)
	{
		err = pWSAGetLastError();
		if (err == WSAEWOULDBLOCK || err == WSAECONNREFUSED)
			return 0;
	}

	return ret;
}

/*
================
WINS_MakeSocketBroadcastCapable
================
*/
int WINS_MakeSocketBroadcastCapable(int socket)
{
	int		i = 1;

	// make this socket broadcast capable
	if (psetsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) < 0)
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

/*
================
WINS_Broadcast
================
*/
int WINS_Broadcast(int socket, void *buf, int len)
{
	if (socket == net_broadcastsocket)
		return WINS_Write(socket, buf, len, (struct qsockaddr *)&broadcastaddr);

	if (net_broadcastsocket)
		Sys_Error("Attempted to use multiple broadcasts sockets\n");

	if (WINS_MakeSocketBroadcastCapable(socket) != -1)
		return WINS_Write(socket, buf, len, (struct qsockaddr *)&broadcastaddr);

	Con_SafePrintf("Unable to make socket broadcast capable\n");
	return -1;
}

/*
================
WINS_Write
================
*/
int WINS_Write(int socket, void *buf, int len, struct qsockaddr *addr)
{
	int		ret;

	ret = psendto(socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct qsockaddr));
	if (ret == -1)
	{
		if (pWSAGetLastError() == WSAEWOULDBLOCK)
			return 0;
	}

	return ret;
}

/*
================
WINS_AddrToString
================
*/
char *WINS_AddrToString(struct qsockaddr *addr)
{
	static char	buffer[NET_NAMELEN];
	u_long		haddr;
	u_short		hport;

	haddr = ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr);
	hport = ntohs(((struct sockaddr_in *)addr)->sin_port);
	sprintf(buffer, "%d.%d.%d.%d:%d", (haddr >> 24) & 0xff, (haddr >> 16) & 0xff, (haddr >> 8) & 0xff, haddr & 0xff, hport);
	return buffer;
}

/*
================
WINS_StringToAddr
================
*/
int WINS_StringToAddr(char *string, struct qsockaddr *addr)
{
	int		ha1, ha2, ha3, ha4, hp;
	u_long	ipaddr;

	hp = 0;
	sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha3, &ha2, &ha4, &hp);
	ipaddr = ha4 | (ha3 << 16) | ((ha2 | (ha1 << 16)) << 8);

	addr->sa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
	((struct sockaddr_in *)addr)->sin_port = htons((u_short)hp);
	return 0;
}

/*
================
WINS_GetSocketAddr
================
*/
int WINS_GetSocketAddr(int socket, struct qsockaddr *addr)
{
	int		addrlen = sizeof(struct qsockaddr);
	u_long	a;

	Q_memset(addr, 0, sizeof(struct qsockaddr));
	pgetsockname(socket, (struct sockaddr *)addr, &addrlen);
	a = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
	if (a == 0 || a == inet_addr("127.0.0.1"))
		((struct sockaddr_in *)addr)->sin_addr.s_addr = myAddr;

	return 0;
}

/*
================
WINS_GetNameFromAddr
================
*/
int WINS_GetNameFromAddr(struct qsockaddr *addr, char *name)
{
	struct hostent	*hostentry;

	hostentry = pgethostbyaddr((char *)&((struct sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET);
	if (hostentry)
		Q_strncpy(name, hostentry->h_name, NET_NAMELEN - 1);
	else
		Q_strcpy(name, WINS_AddrToString(addr));

	return 0;
}

/*
================
PartialIPAddress

Fills in the front of a partial dotted address from our own address,
e.g. ".2" or "3.2", with an optional ":port".
================
*/
static int PartialIPAddress(char *in, struct qsockaddr *hostaddr)
{
	char	buff[256];
	char	*b;
	u_long	addr;
	u_long	mask;
	int		num;
	int		run;
	char	c;
	u_short	port;
	u_long	myaddrpart;

	buff[0] = '.';
	strcpy(buff + 1, in);
	b = buff;
	if (buff[1] == '.')
		b++;

	addr = 0;
	mask = -1;
	if (*b == '.')
	{
		do
		{
			b++;
			num = 0;
			run = 0;
			while (1)
			{
				c = *b;
				if (c < '0' || c > '9')
					break;
				b++;
				run++;
				num = c + 10 * num - '0';
				if (run > 3)
					return -1;
			}
			if (c != '.' && c != ':' && c != 0)
				return -1;
			if (num > 255)
				return -1;
			mask <<= 8;
			addr = num + (addr << 8);
		} while (c == '.');
	}

	if (*b == ':')
		port = Q_atoi(b + 1);
	else
		port = hostshort;

	hostaddr->sa_family = AF_INET;
	((struct sockaddr_in *)hostaddr)->sin_port = htons(port);
	myaddrpart = myAddr & htonl(mask);
	((struct sockaddr_in *)hostaddr)->sin_addr.s_addr = htonl(addr) | myaddrpart;

	return 0;
}

/*
================
WINS_GetAddrFromName
================
*/
int WINS_GetAddrFromName(char *name, struct qsockaddr *addr)
{
	struct hostent	*hostentry;

	if (name[0] >= '0' && name[0] <= '9')
		return PartialIPAddress(name, addr);

	hostentry = pgethostbyname(name);
	if (!hostentry)
		return -1;

	addr->sa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_port = htons(hostshort);
	((struct sockaddr_in *)addr)->sin_addr.s_addr = *(u_long *)hostentry->h_addr_list[0];

	return 0;
}

/*
================
WINS_AddrCompare

Returns 0 for the same address and port, 1 for the same address on
another port, -1 otherwise.
================
*/
int WINS_AddrCompare(struct qsockaddr *addr1, struct qsockaddr *addr2)
{
	if (addr1->sa_family != addr2->sa_family)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_addr.s_addr != ((struct sockaddr_in *)addr2)->sin_addr.s_addr)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_port != ((struct sockaddr_in *)addr2)->sin_port)
		return 1;

	return 0;
}

/*
================
WINS_GetSocketPort
================
*/
int WINS_GetSocketPort(struct qsockaddr *addr)
{
	return ntohs(((struct sockaddr_in *)addr)->sin_port);
}

/*
================
WINS_SetSocketPort
================
*/
int WINS_SetSocketPort(struct qsockaddr *addr, int port)
{
	((struct sockaddr_in *)addr)->sin_port = htons((u_short)port);
	return 0;
}
