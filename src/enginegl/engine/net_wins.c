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

static int PartialIPAddress(const char *in, struct sockaddr_in *hostaddr);
char *WINS_AddrToString(struct sockaddr_in *addr);
int WINS_GetSocketAddr(int socket, struct sockaddr_in *addr);
void WINS_Listen(qboolean state);
SOCKET WINS_OpenSocket(u_short port);
int WINS_CloseSocket(int socket);
int WINS_Write(int socket, void *buf, int len, struct sockaddr_in *addr);

static int g_winsock_initialized = 0;
static int g_winsock_refcount = 0;
static SOCKET g_net_controlsocket = INVALID_SOCKET;
static SOCKET g_net_acceptsocket = INVALID_SOCKET;
static SOCKET g_net_broadcastsocket = 0;
static u32 g_myAddr = 0;
static double g_blocktime = 0.0;
static struct sockaddr_in g_broadcastaddr;
char my_tcpip_address[64];
static char g_colon_stripped_tcpip[64];

int (__stdcall *WSAStartupFunc)(WORD wVersionRequested, LPWSADATA lpWSAData);
int (__stdcall *WSACleanupFunc)(void);
int (__stdcall *WSAGetLastErrorFunc)(void);
SOCKET (__stdcall *socketFunc)(int af, int type, int protocol);
int (__stdcall *ioctlsocketFunc)(SOCKET s, long cmd, u_long FAR *argp);
int (__stdcall *setsockoptFunc)(SOCKET s, int level, int optname, const char FAR *optval, int optlen);
int (__stdcall *recvfromFunc)(SOCKET s, char FAR *buf, int len, int flags, struct sockaddr FAR *from, int FAR *fromlen);
int (__stdcall *sendtoFunc)(SOCKET s, const char FAR *buf, int len, int flags, const struct sockaddr FAR *to, int tolen);
int (__stdcall *closesocketFunc)(SOCKET s);
int (__stdcall *gethostnameFunc)(char FAR *name, int namelen);
static struct hostent *(__stdcall *p_gethostbyname)(const char FAR *name);
static struct hostent *(__stdcall *p_gethostbyaddr)(const char FAR *addr, int len, int type);
int (__stdcall *getsocknameFunc)(SOCKET s, struct sockaddr FAR *name, int FAR *namelen);

BOOL __stdcall BlockingHook(void)
{

	BOOL result;
	MSG msg;

	if ((Sys_FloatTime() - g_blocktime) <= 2.0)
	{
		result = PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
		if (result)
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		return result;
	}
	else
	{
		WSACancelBlockingCall();
		return FALSE;
	}
}

int WINS_Init(void)
{
	HMODULE hWinsock;
	struct hostent *local;
	char *p;
	char c;
	int i;
	char *addr_string;
	char *colon;
	char buff[256];
	struct sockaddr_in address;
	WSADATA wsadata;

	hWinsock = LoadLibraryA("wsock32.dll");
	if (!hWinsock)
	{
		Con_Printf("Failed to load wsock32.dll\n");
		g_winsock_initialized = 0;
		return -1;
	}

	g_winsock_initialized = 1;

	WSAStartupFunc = (int (__stdcall *)(WORD, LPWSADATA))GetProcAddress(hWinsock, "WSAStartup");
	WSACleanupFunc = (int (__stdcall *)(void))GetProcAddress(hWinsock, "WSACleanup");
	WSAGetLastErrorFunc = (int (__stdcall *)(void))GetProcAddress(hWinsock, "WSAGetLastError");
	socketFunc = (SOCKET (__stdcall *)(int, int, int))GetProcAddress(hWinsock, "socket");
	ioctlsocketFunc = (int (__stdcall *)(SOCKET, long, u_long FAR *))GetProcAddress(hWinsock, "ioctlsocket");
	setsockoptFunc = (int (__stdcall *)(SOCKET, int, int, const char FAR *, int))GetProcAddress(hWinsock, "setsockopt");
	recvfromFunc = (int (__stdcall *)(SOCKET, char FAR *, int, int, struct sockaddr FAR *, int FAR *))GetProcAddress(hWinsock, "recvfrom");
	sendtoFunc = (int (__stdcall *)(SOCKET, const char FAR *, int, int, const struct sockaddr FAR *, int))GetProcAddress(hWinsock, "sendto");
	closesocketFunc = (int (__stdcall *)(SOCKET))GetProcAddress(hWinsock, "closesocket");
	gethostnameFunc = (int (__stdcall *)(char FAR *, int))GetProcAddress(hWinsock, "gethostname");
	p_gethostbyname = (struct hostent *(__stdcall *)(const char FAR *))GetProcAddress(hWinsock, "gethostbyname");
	p_gethostbyaddr = (struct hostent *(__stdcall *)(const char FAR *, int, int))GetProcAddress(hWinsock, "gethostbyaddr");
	getsocknameFunc = (int (__stdcall *)(SOCKET, struct sockaddr FAR *, int FAR *))GetProcAddress(hWinsock, "getsockname");

	if (!WSAStartupFunc || !WSACleanupFunc || !WSAGetLastErrorFunc || !socketFunc ||
	    !ioctlsocketFunc || !setsockoptFunc || !recvfromFunc || !sendtoFunc ||
	    !closesocketFunc || !gethostnameFunc || !p_gethostbyname ||
	    !p_gethostbyaddr || !getsocknameFunc)
	{
		Con_Printf("Couldn't get winsock function pointers\n");
		return -1;
	}

	if (COM_CheckParm("-noudp"))
		return -1;

	if (!g_winsock_refcount && WSAStartupFunc(0x0101, &wsadata))
	{
		Con_Printf("Winsock initialization failed\n");
		return -1;
	}

	++g_winsock_refcount;

	if (!gethostnameFunc(buff, 256))
	{
		g_blocktime = Sys_FloatTime();
		WSASetBlockingHook((FARPROC)BlockingHook);
		local = p_gethostbyname(buff);
		WSAUnhookBlockingHook();

		if (!local)
		{
			Con_DPrintf("Winsock TCP/IP initialization failed\n");
			if (--g_winsock_refcount)
				return -1;
			WSACleanupFunc();
			return -1;
		}

		g_myAddr = *(u32 *)local->h_addr_list[0];

		if (!Q_strcmp(hostname.string, "unnamed"))
		{

			p = buff;
			if (buff[0])
			{
				do
				{
					c = *p;
					if ((c < '0' || c > '9') && c != '.')
						break;
					p++;
				}
				while (*p);
			}

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

	g_net_controlsocket = WINS_OpenSocket(0);
	if (g_net_controlsocket == INVALID_SOCKET)
	{
		Con_SafePrintf("WINS_Init: Unable to open control socket\n");
		if (--g_winsock_refcount)
			return -1;
		WSACleanupFunc();
		return -1;
	}

	g_broadcastaddr.sin_family = AF_INET;
	g_broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
	g_broadcastaddr.sin_port = htons(hostshort);

	WINS_GetSocketAddr(g_net_controlsocket, &address);
	addr_string = WINS_AddrToString((struct sockaddr_in *)&address);
	Q_strcpy(my_tcpip_address, addr_string);

	colon = Q_strstr(my_tcpip_address, ":");
	if (colon)
		*colon = 0;

	Con_DPrintf("Winsock TCP/IP Initialized\n");
	tcpipAvailable = true;

	return g_net_controlsocket;
}

int WINS_Shutdown(void)
{
	int result;

	WINS_Listen(0);
	result = WINS_CloseSocket(g_net_controlsocket);

	if (!--g_winsock_refcount)
		return WSACleanupFunc();

	return result;
}

void WINS_Listen(qboolean state)
{
	if (state)
	{

		if (g_net_acceptsocket != INVALID_SOCKET)
			return;

		g_net_acceptsocket = WINS_OpenSocket(hostshort);
		if (g_net_acceptsocket == INVALID_SOCKET)
			Sys_Error("WINS_Listen: Unable to open accept socket\n");
	}
	else
	{

		if (g_net_acceptsocket == INVALID_SOCKET)
			return;

		WINS_CloseSocket(g_net_acceptsocket);
		g_net_acceptsocket = INVALID_SOCKET;
	}
}

SOCKET WINS_OpenSocket(u_short port)
{
	SOCKET newsocket;
	struct sockaddr_in address;
	u_long nonblocking = 1;

	newsocket = socketFunc(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (newsocket == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (ioctlsocketFunc(newsocket, FIONBIO, &nonblocking) == -1)
	{
		closesocketFunc(newsocket);
		return INVALID_SOCKET;
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(newsocket, (struct sockaddr *)&address, sizeof(address)))
		Sys_Error("bind");

	return newsocket;
}

int WINS_CloseSocket(int socket)
{

	if (socket == g_net_broadcastsocket)
		g_net_broadcastsocket = 0;

	return closesocketFunc(socket);
}

int WINS_Connect(void)
{
	return 0;
}

int WINS_CheckNewConnections(void)
{
	char buffer[4096];

	if (g_net_acceptsocket == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (recvfromFunc(g_net_acceptsocket, buffer, sizeof(buffer), MSG_PEEK, NULL, NULL) > 0)
		return g_net_acceptsocket;

	return INVALID_SOCKET;
}

int WINS_Read(int socket, void *buf, int len, struct sockaddr_in *addr)
{
	int addrlen = sizeof(struct sockaddr_in);
	int ret;
	int err;

	ret = recvfromFunc(socket, buf, len, 0, (struct sockaddr *)addr, &addrlen);

	if (ret == -1)
	{
		err = WSAGetLastErrorFunc();

		if (err == WSAEWOULDBLOCK || err == WSAECONNREFUSED)
			return 0;
	}

	return ret;
}

int WINS_MakeSocketBroadcastCapable(int socket)
{
	int broadcast = 1;

	if (setsockoptFunc(socket, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast, sizeof(broadcast)) < 0)
		return -1;

	g_net_broadcastsocket = socket;
	return 0;
}

int WINS_Broadcast(int socket, void *buf, int len)
{

	if (socket == g_net_broadcastsocket)
		return WINS_Write(socket, buf, len, (struct sockaddr_in *)&g_broadcastaddr);

	if (g_net_broadcastsocket)
		Sys_Error("Attempted to use multiple broadcasts sockets\n");

	if (WINS_MakeSocketBroadcastCapable(socket) != -1)
		return WINS_Write(socket, buf, len, (struct sockaddr_in *)&g_broadcastaddr);

	Con_SafePrintf("Unable to make socket broadcast capable\n");
	return -1;
}

int WINS_Write(int socket, void *buf, int len, struct sockaddr_in *addr)
{
	int ret;

	ret = sendtoFunc(socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in));

	if (ret == -1)
	{

		if (WSAGetLastErrorFunc() == WSAEWOULDBLOCK)
			return 0;
	}

	return ret;
}

char *WINS_AddrToString(struct sockaddr_in *addr)
{
	static char buffer[64];
	u_long ip;
	u_short port;

	ip = ntohl(addr->sin_addr.s_addr);
	port = ntohs(addr->sin_port);

	sprintf(buffer, "%d.%d.%d.%d:%d",
	        (ip >> 24) & 0xFF,
	        (ip >> 16) & 0xFF,
	        (ip >> 8) & 0xFF,
	        ip & 0xFF,
	        port);

	return buffer;
}

int WINS_StringToAddr(char *string, struct sockaddr_in *addr)
{
	int b1, b2, b3, b4;
	int port;
	u_long ip;

	port = 0;
	sscanf(string, "%d.%d.%d.%d:%d", &b1, &b3, &b2, &b4, &port);

	ip = (b4) | (b3 << 16) | ((b2 | (b1 << 16)) << 8);

	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = htonl(ip);
	addr->sin_port = htons((u_short)port);

	return 0;
}

int WINS_GetSocketAddr(int socket, struct sockaddr_in *addr)
{
	int addrlen = sizeof(struct sockaddr_in);
	u_long ip;

	Q_memset(addr, 0, sizeof(struct sockaddr_in));
	getsocknameFunc(socket, (struct sockaddr *)addr, &addrlen);

	ip = addr->sin_addr.s_addr;

	if (!ip || ip == inet_addr("127.0.0.1"))
		addr->sin_addr.s_addr = g_myAddr;

	return 0;
}

int WINS_GetNameFromAddr(struct sockaddr_in *addr, char *name)
{
	struct hostent *hostentry;
	char *addr_string;

	hostentry = p_gethostbyaddr((char *)&addr->sin_addr, 4, AF_INET);

	if (hostentry)
	{
		Q_strncpy(name, hostentry->h_name, 63);
	}
	else
	{
		addr_string = WINS_AddrToString(addr);
		Q_strcpy(name, addr_string);
	}

	return 0;
}

int WINS_GetAddrFromName(char *name, struct sockaddr_in *addr)
{
	struct hostent *hostentry;

	if (*name >= '0' && *name <= '9')
		return PartialIPAddress(name, addr);

	hostentry = p_gethostbyname(name);
	if (!hostentry)
		return -1;

	addr->sin_family = AF_INET;
	addr->sin_port = htons(hostshort);
	addr->sin_addr.s_addr = *(u_long *)hostentry->h_addr_list[0];

	return 0;
}

static int PartialIPAddress(const char *in, struct sockaddr_in *addr)
{
	char buff[256];
	char *p;
	u_long partial_ip = 0;
	u_long mask = 0xFFFFFFFF;
	int octet_count = 0;
	int octet_value;
	int octet_len;
	char c;
	char *colon;
	u_short port;
	u_long final_ip;

	buff[0] = '.';
	strcpy(&buff[1], in);

	p = buff;
	if (buff[1] == '.')
		p = &buff[1];

	if (*p == '.')
	{
		while (1)
		{
			p++;

			octet_value = 0;
			octet_len = 0;

			while (1)
			{
				c = *p;
				if (c < '0')
					break;
				if (c > '9')
				{
					c = *p;
					if (c >= '0' && c <= '9')
						goto PARSE_DIGIT;
					break;
				}
PARSE_DIGIT:
				p++;
				octet_len++;
				octet_value = c + 10 * octet_value - '0';

				if (octet_len > 3)
					return -1;
			}

			if (c != '.' && c != ':' && c != '\0')
				return -1;

			if (octet_value >= 256)
				return -1;

			mask <<= 8;
			partial_ip = octet_value + (partial_ip << 8);

			if (c != '.')
				break;
		}
	}

	colon = p;
	if (*colon == ':')
		port = Q_atoi(p + 1);
	else
		port = hostshort;

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	final_ip = g_myAddr & htonl(mask);
	addr->sin_addr.s_addr = htonl(partial_ip) | final_ip;

	return 0;
}

int WINS_AddrCompare(struct sockaddr_in *addr1, struct sockaddr_in *addr2)
{
	if (addr1->sin_family != addr2->sin_family)
		return -1;

	if (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr)
		return (addr1->sin_port != addr2->sin_port);

	return -1;
}

int WINS_GetSocketPort(struct sockaddr_in *addr)
{
	return ntohs(addr->sin_port);
}

int WINS_SetSocketPort(struct sockaddr_in *addr, u_short port)
{
	addr->sin_port = htons(port);
	return 0;
}
