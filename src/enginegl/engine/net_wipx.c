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

// net_wipx.c -- Winsock IPX driver

#include "quakedef.h"
#include "winquake.h"
#include <wsipx.h>

#define MAXHOSTNAMELEN		256
#define WINSOCK_VERSION_1_1	0x0101

#define IPXSOCKETS			18
#define IPXSEQUENCESIZE		4	// sequence number in front of every packet

static int		winsock_initialized;

static int		net_controlsocket;
static int		net_acceptsocket = -1;	// socket for fielding new connections
qboolean		ipx_configured;		// set once IPX is up; nothing reads it

static SOCKET	ipxsocket[IPXSOCKETS];
static int		sequence[IPXSOCKETS];

static struct
{
	int		sequence;
	byte	data[NET_MAXMESSAGE];
} packetBuffer;

static struct qsockaddr	broadcastaddr;

char			my_ipx_address[NET_NAMELEN];

//=============================================================================

/*
================
WIPX_Init
================
*/
int WIPX_Init(void)
{
	int			i;
	char		buff[MAXHOSTNAMELEN];
	char		*p;
	WSADATA		winsockdata;
	struct qsockaddr	addr;

	if (COM_CheckParm("-noipx"))
		return -1;

	// FIXME: nothing sets ipxAvailable, so IPX never starts
	if (!ipxAvailable)
		return -1;

	if (!winsock_initialized && pWSAStartup(WINSOCK_VERSION_1_1, &winsockdata))
	{
		Con_Printf("Winsock initialization failed\n");
		return -1;
	}
	winsock_initialized++;

	memset(ipxsocket, 0, sizeof(ipxsocket));

	// determine my name & address
	if (pgethostname(buff, MAXHOSTNAMELEN) == 0)
	{
		// if the hostname cvar isn't set, set it to the machine name
		if (Q_strcmp(hostname.string, "UNNAMED") == 0)
		{
			// see if it's a text IP address (well, close enough)
			for (p = buff; *p; p++)
			{
				if ((*p < '0' || *p > '9') && *p != '.')
					break;
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

	if ((net_controlsocket = WIPX_OpenSocket(0)) == -1)
	{
		Con_DPrintf("WIPX_Init: Unable to open control socket\n");
		if (--winsock_initialized == 0)
			pWSACleanup();
		return -1;
	}

	((struct sockaddr_ipx *)&broadcastaddr)->sa_family = AF_IPX;
	memset(((struct sockaddr_ipx *)&broadcastaddr)->sa_netnum, 0, 4);
	memset(((struct sockaddr_ipx *)&broadcastaddr)->sa_nodenum, 0xff, 6);
	((struct sockaddr_ipx *)&broadcastaddr)->sa_socket = htons(hostshort);

	WIPX_GetSocketAddr(net_controlsocket, &addr);
	Q_strcpy(my_ipx_address, WIPX_AddrToString(&addr));
	p = strchr(my_ipx_address, ':');
	if (p)
		*p = 0;

	Con_Printf("Winsock IPX initialized\n");
	ipx_configured = true;

	return net_controlsocket;
}

/*
================
WIPX_Shutdown
================
*/
void WIPX_Shutdown(void)
{
	WIPX_Listen(false);
	WIPX_CloseSocket(net_controlsocket);
	if (--winsock_initialized == 0)
		pWSACleanup();
}

/*
================
WIPX_Listen
================
*/
void WIPX_Listen(qboolean state)
{
	// enable listening
	if (state)
	{
		if (net_acceptsocket != -1)
			return;
		if ((net_acceptsocket = WIPX_OpenSocket(hostshort)) == -1)
			Sys_Error("WIPX_Listen: Unable to open accept socket");
		return;
	}

	// disable listening
	if (net_acceptsocket == -1)
		return;
	WIPX_CloseSocket(net_acceptsocket);
	net_acceptsocket = -1;
}

/*
================
WIPX_OpenSocket

Returns an index into ipxsocket, not a winsock handle.
================
*/
int WIPX_OpenSocket(int port)
{
	int					handle;
	SOCKET				newsocket;
	u_long				_true = 1;
	struct sockaddr_ipx	address;

	for (handle = 0; handle < IPXSOCKETS; handle++)
	{
		if (ipxsocket[handle] == 0)
			break;
	}
	if (handle == IPXSOCKETS)
		return -1;

	if ((newsocket = psocket(AF_IPX, SOCK_DGRAM, NSPROTO_IPX)) == INVALID_SOCKET)
		return -1;

	if (pioctlsocket(newsocket, FIONBIO, &_true) == -1
		|| psetsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&_true, sizeof(_true)) < 0)
	{
		pclosesocket(newsocket);
		return -1;
	}

	address.sa_family = AF_IPX;
	memset(address.sa_netnum, 0, 4);
	memset(address.sa_nodenum, 0, 6);
	address.sa_socket = htons((u_short)port);
	if (bind(newsocket, (struct sockaddr *)&address, sizeof(address)) != 0)
		Sys_Error("Winsock IPX bind failed");

	ipxsocket[handle] = newsocket;
	sequence[handle] = 0;
	return handle;
}

/*
================
WIPX_CloseSocket
================
*/
void WIPX_CloseSocket(int handle)
{
	pclosesocket(ipxsocket[handle]);
	ipxsocket[handle] = 0;
}

/*
================
WIPX_Connect
================
*/
int WIPX_Connect(void)
{
	return 0;
}

/*
================
WIPX_CheckNewConnections
================
*/
int WIPX_CheckNewConnections(void)
{
	u_long	available;

	if (net_acceptsocket == -1)
		return -1;

	if (pioctlsocket(ipxsocket[net_acceptsocket], FIONREAD, &available) == -1)
		Sys_Error("WIPX: ioctlsocket (FIONREAD) failed");

	if (available)
		return net_acceptsocket;

	return -1;
}

/*
================
WIPX_Read
================
*/
int WIPX_Read(int handle, void *buf, int len, struct qsockaddr *addr)
{
	int		addrlen = sizeof(struct qsockaddr);
	int		ret;
	int		err;

	ret = precvfrom(ipxsocket[handle], (char *)&packetBuffer, len + IPXSEQUENCESIZE, 0, (struct sockaddr *)addr, &addrlen);
	if (ret == -1)
	{
		err = pWSAGetLastError();
		if (err == WSAEWOULDBLOCK || err == WSAECONNREFUSED)
			return 0;
	}

	if (ret < IPXSEQUENCESIZE)
		return 0;

	// remove sequence number, it's only needed for DOS IPX
	ret -= IPXSEQUENCESIZE;
	Q_memcpy(buf, packetBuffer.data, ret);
	return ret;
}

/*
================
WIPX_Broadcast
================
*/
int WIPX_Broadcast(int handle, void *buf, int len)
{
	return WIPX_Write(handle, buf, len, &broadcastaddr);
}

/*
================
WIPX_Write
================
*/
int WIPX_Write(int handle, void *buf, int len, struct qsockaddr *addr)
{
	int		ret;

	// build packet with sequence number
	packetBuffer.sequence = sequence[handle]++;
	Q_memcpy(packetBuffer.data, buf, len);

	ret = psendto(ipxsocket[handle], (char *)&packetBuffer, len + IPXSEQUENCESIZE, 0, (struct sockaddr *)addr, sizeof(struct qsockaddr));
	if (ret == -1)
	{
		if (pWSAGetLastError() == WSAEWOULDBLOCK)
			return 0;
	}

	return ret;
}

/*
================
WIPX_AddrToString
================
*/
char *WIPX_AddrToString(struct qsockaddr *addr)
{
	static char	buf[32];

	sprintf(buf, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%u",
		((struct sockaddr_ipx *)addr)->sa_netnum[0] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_netnum[1] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_netnum[2] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_netnum[3] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_nodenum[0] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_nodenum[1] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_nodenum[2] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_nodenum[3] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_nodenum[4] & 0xff,
		((struct sockaddr_ipx *)addr)->sa_nodenum[5] & 0xff,
		ntohs(((struct sockaddr_ipx *)addr)->sa_socket));
	return buf;
}

/*
================
WIPX_StringToAddr
================
*/
int WIPX_StringToAddr(char *string, struct qsockaddr *addr)
{
	int		val;
	char	buf[3];

	buf[2] = 0;
	memset(addr, 0, sizeof(struct qsockaddr));
	addr->sa_family = AF_IPX;

#define DO(src, dest)						\
	buf[0] = string[src];					\
	buf[1] = string[src + 1];				\
	if (sscanf(buf, "%x", &val) != 1)		\
		return -1;							\
	((struct sockaddr_ipx *)addr)->dest = val

	DO(0, sa_netnum[0]);
	DO(2, sa_netnum[1]);
	DO(4, sa_netnum[2]);
	DO(6, sa_netnum[3]);
	DO(9, sa_nodenum[0]);
	DO(11, sa_nodenum[1]);
	DO(13, sa_nodenum[2]);
	DO(15, sa_nodenum[3]);
	DO(17, sa_nodenum[4]);
	DO(19, sa_nodenum[5]);
#undef DO

	sscanf(&string[22], "%u", &val);
	((struct sockaddr_ipx *)addr)->sa_socket = htons((u_short)val);

	return 0;
}

/*
================
WIPX_GetSocketAddr
================
*/
int WIPX_GetSocketAddr(int handle, struct qsockaddr *addr)
{
	int		addrlen = sizeof(struct qsockaddr);

	memset(addr, 0, sizeof(struct qsockaddr));
	if (pgetsockname(ipxsocket[handle], (struct sockaddr *)addr, &addrlen) != 0)
		pWSAGetLastError();

	return 0;
}

/*
================
WIPX_GetNameFromAddr
================
*/
int WIPX_GetNameFromAddr(struct qsockaddr *addr, char *name)
{
	Q_strcpy(name, WIPX_AddrToString(addr));
	return 0;
}

/*
================
WIPX_GetAddrFromName

Accepts "node", "net:node" or "net:node:port" in hex.
================
*/
int WIPX_GetAddrFromName(char *name, struct qsockaddr *addr)
{
	int		n;
	char	buf[32];

	n = strlen(name);

	if (n == 12)
	{
		sprintf(buf, "00000000:%s:%u", name, hostshort);
		return WIPX_StringToAddr(buf, addr);
	}
	if (n == 21)
	{
		sprintf(buf, "%s:%u", name, hostshort);
		return WIPX_StringToAddr(buf, addr);
	}
	if (n > 21 && n <= 27)
		return WIPX_StringToAddr(name, addr);

	return -1;
}

/*
================
WIPX_AddrCompare

Returns 0 for the same address and port, 1 for the same address on
another port, -1 otherwise.
================
*/
int WIPX_AddrCompare(struct qsockaddr *addr1, struct qsockaddr *addr2)
{
	if (addr2->sa_family != addr1->sa_family)
		return -1;

	if (*((struct sockaddr_ipx *)addr1)->sa_netnum && *((struct sockaddr_ipx *)addr2)->sa_netnum)
	{
		if (memcmp(((struct sockaddr_ipx *)addr1)->sa_netnum, ((struct sockaddr_ipx *)addr2)->sa_netnum, 4) != 0)
			return -1;
	}

	if (memcmp(((struct sockaddr_ipx *)addr1)->sa_nodenum, ((struct sockaddr_ipx *)addr2)->sa_nodenum, 6) != 0)
		return -1;

	if (((struct sockaddr_ipx *)addr2)->sa_socket != ((struct sockaddr_ipx *)addr1)->sa_socket)
		return 1;

	return 0;
}

/*
================
WIPX_GetSocketPort
================
*/
int WIPX_GetSocketPort(struct qsockaddr *addr)
{
	return ntohs(((struct sockaddr_ipx *)addr)->sa_socket);
}

/*
================
WIPX_SetSocketPort
================
*/
int WIPX_SetSocketPort(struct qsockaddr *addr, int port)
{
	((struct sockaddr_ipx *)addr)->sa_socket = htons((u_short)port);
	return 0;
}
