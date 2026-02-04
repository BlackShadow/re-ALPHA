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
#include <winsock.h>

#ifndef _SOCKADDR_IPX_DEFINED
#define _SOCKADDR_IPX_DEFINED 1
struct sockaddr_ipx
{
	short sa_family;
	char  sa_netnum[4];
	char  sa_nodenum[6];
	u_short sa_socket;
	char  sa_zero[2];
};
#endif

#define AF_IPX  6

extern int (__stdcall *WSAStartupFunc)(WORD, LPWSADATA);
extern int (__stdcall *WSACleanupFunc)(void);
extern int (__stdcall *ioctlsocketFunc)(SOCKET, long, u_long *);
extern int (__stdcall *recvfromFunc)(SOCKET, char *, int, int, struct sockaddr *, int *);
extern int (__stdcall *sendtoFunc)(SOCKET, const char *, int, int, const struct sockaddr *, int);
extern SOCKET (__stdcall *socketFunc)(int, int, int);
extern int (__stdcall *setsockoptFunc)(SOCKET, int, int, const char *, int);
extern int (__stdcall *closesocketFunc)(SOCKET);
extern int (__stdcall *WSAGetLastErrorFunc)(void);
extern int (__stdcall *getsocknameFunc)(SOCKET, struct sockaddr *, int *);
extern int (__stdcall *gethostnameFunc)(char *, int);
extern qboolean ipxAvailable;

int         winsock_initialized;
int         ipx_controlSocket;
int         ipx_acceptSocket = -1;
qboolean    ipx_configured;

SOCKET      ipx_sockets[18];
int         ipx_sequence[18];

int         ipx_packetBuffer[1];
byte        ipx_packetData[8192];

short       ipx_broadcastAddr_family;
byte        ipx_broadcastAddr_net[4];
byte        ipx_broadcastAddr_node[6];
short       ipx_broadcastAddr_port;

char        ipx_addressBuffer[32];

char        my_ipx_address[64];

void WIPX_Listen(qboolean state);
int WIPX_OpenSocket(int port);
void WIPX_CloseSocket(int socket);
int WIPX_GetSocketAddr(int socket, struct sockaddr *addr);
int WIPX_Write(int socket, void *buf, int len, struct sockaddr *addr);
char *WIPX_AddrToString(struct sockaddr *addr);

int WIPX_Init(void)
{
    char        hostname_buf[256];
    char        *s;
    int         i;
    struct sockaddr_ipx addr;
    WSADATA     winsockdata;

    if (COM_CheckParm("-noipx"))
        return -1;

    if (!ipxAvailable)
        return -1;

    if (winsock_initialized || WSAStartupFunc(0x0101, &winsockdata) == 0)
    {
        winsock_initialized++;
        memset(ipx_sockets, 0, sizeof(ipx_sockets));

        if (gethostnameFunc(hostname_buf, 256) == 0)
        {
            if (Q_strcmp(hostname.string, "UNNAMED") == 0)
            {

                s = hostname_buf;
                while (*s)
                {
                    if ((*s < '0' || *s > '9') && *s != '.')
                        break;
                    s++;
                }

                if (*s)
                {

                    for (i = 0; i < 15; i++)
                    {
                        if (hostname_buf[i] == '.')
                            break;
                    }
                    hostname_buf[i] = 0;
                }

                Cvar_Set("hostname", hostname_buf);
            }
        }

        ipx_controlSocket = WIPX_OpenSocket(0);
        if (ipx_controlSocket == -1)
        {
            Con_DPrintf("WIPX_Init: Unable to open control socket\n");
            if (--winsock_initialized == 0)
                WSACleanupFunc();
            return -1;
        }

        ipx_broadcastAddr_family = AF_IPX;
        memset(ipx_broadcastAddr_net, 0, 4);
        memset(ipx_broadcastAddr_node, 0xFF, 6);
        ipx_broadcastAddr_port = htons(hostshort);

        WIPX_GetSocketAddr(ipx_controlSocket, (struct sockaddr *)&addr);
        Q_strcpy(my_ipx_address, WIPX_AddrToString((struct sockaddr *)&addr));

        s = strchr(my_ipx_address, ':');
        if (s)
            *s = 0;

        Con_Printf("Winsock IPX initialized\n");
        ipx_configured = true;

        return ipx_controlSocket;
    }
    else
    {
        Con_Printf("Winsock initialization failed\n");
        return -1;
    }
}

void WIPX_Shutdown(void)
{
    WIPX_Listen(false);
    WIPX_CloseSocket(ipx_controlSocket);

    if (--winsock_initialized == 0)
        WSACleanupFunc();
}

void WIPX_Listen(qboolean state)
{
    if (state)
    {
        if (ipx_acceptSocket == -1)
        {
            ipx_acceptSocket = WIPX_OpenSocket(hostshort);
            if (ipx_acceptSocket == -1)
                Sys_Error("WIPX_Listen: Unable to open accept socket");
        }
    }
    else
    {
        if (ipx_acceptSocket != -1)
        {
            WIPX_CloseSocket(ipx_acceptSocket);
            ipx_acceptSocket = -1;
        }
    }
}

int WIPX_OpenSocket(int port)
{
    int         i;
    SOCKET      newsocket;
    int         one = 1;
    struct sockaddr_ipx addr;

    for (i = 0; i < 18; i++)
    {
        if (ipx_sockets[i] == 0)
            break;
    }

    if (i == 18)
        return -1;

    newsocket = socketFunc(AF_IPX, SOCK_DGRAM, 1000);
    if (newsocket == INVALID_SOCKET)
        return -1;

    if (ioctlsocketFunc(newsocket, FIONBIO, (u_long *)&one) == -1 ||
        setsockoptFunc(newsocket, 0xFFFF, 32, (char *)&one, sizeof(one)) < 0)
    {
        closesocketFunc(newsocket);
        return -1;
    }

    addr.sa_family = AF_IPX;
    memset(addr.sa_netnum, 0, 10);
    addr.sa_socket = htons((u_short)port);

    if (bind(newsocket, (struct sockaddr *)&addr, 14))
        Sys_Error("Winsock IPX bind failed");

    ipx_sockets[i] = newsocket;
    ipx_sequence[i] = 0;

    return i;
}

void WIPX_CloseSocket(int socket)
{
    closesocketFunc(ipx_sockets[socket]);
    ipx_sockets[socket] = 0;
}

int WIPX_Connect(void)
{
    return 0;
}

int WIPX_CheckNewConnections(void)
{
    u_long  available;

    if (ipx_acceptSocket == -1)
        return -1;

    if (ioctlsocketFunc(ipx_sockets[ipx_acceptSocket], FIONREAD, &available) == -1)
        Sys_Error("WIPX: ioctlsocket (FIONREAD) failed");

    if (available)
        return ipx_acceptSocket;

    return -1;
}

int WIPX_Read(int socket, void *buf, int len, struct sockaddr *addr)
{
    int         ret;
    int         addrlen = 16;

    ret = recvfromFunc(ipx_sockets[socket], (char *)ipx_packetBuffer, len + 4, 0, addr, &addrlen);

    if (ret == -1)
    {
        int err = WSAGetLastErrorFunc();
        if (err == WSAEWOULDBLOCK || err == WSAECONNREFUSED)
            return 0;
    }

    if (ret < 4)
        return 0;

    ret -= 4;
    Q_memcpy(buf, ipx_packetData, ret);

    return ret;
}

int WIPX_Broadcast(int socket, void *buf, int len)
{
    return WIPX_Write(socket, buf, len, (struct sockaddr *)&ipx_broadcastAddr_family);
}

int WIPX_Write(int socket, void *buf, int len, struct sockaddr *addr)
{
    int         ret;

    ipx_packetBuffer[0] = ipx_sequence[socket]++;

    Q_memcpy(ipx_packetData, buf, len);

    ret = sendtoFunc(ipx_sockets[socket], (char *)ipx_packetBuffer, len + 4, 0, addr, 16);

    if (ret == -1)
    {
        if (WSAGetLastErrorFunc() == WSAEWOULDBLOCK)
            return 0;
    }

    return ret;
}

char *WIPX_AddrToString(struct sockaddr *addr)
{
    byte *a = (byte *)addr;

    sprintf(ipx_addressBuffer, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%u",
        a[2], a[3], a[4], a[5],
        a[6], a[7], a[8], a[9], a[10], a[11],
        ntohs(*(u_short *)&a[12]));

    return ipx_addressBuffer;
}

int WIPX_StringToAddr(char *string, struct sockaddr *addr)
{
    int     val;
    char    buf[3];

    buf[2] = 0;
    memset(addr, 0, 16);
    ((struct sockaddr_ipx *)addr)->sa_family = AF_IPX;

    buf[0] = string[0]; buf[1] = string[1];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[2] = val;

    buf[0] = string[2]; buf[1] = string[3];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[3] = val;

    buf[0] = string[4]; buf[1] = string[5];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[4] = val;

    buf[0] = string[6]; buf[1] = string[7];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[5] = val;

    buf[0] = string[9]; buf[1] = string[10];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[6] = val;

    buf[0] = string[11]; buf[1] = string[12];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[7] = val;

    buf[0] = string[13]; buf[1] = string[14];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[8] = val;

    buf[0] = string[15]; buf[1] = string[16];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[9] = val;

    buf[0] = string[17]; buf[1] = string[18];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[10] = val;

    buf[0] = string[19]; buf[1] = string[20];
    if (sscanf(buf, "%x", &val) != 1) return -1;
    ((byte *)addr)[11] = val;

    sscanf(&string[22], "%u", &val);
    *(u_short *)&((byte *)addr)[12] = htons((u_short)val);

    return 0;
}

int WIPX_GetSocketAddr(int socket, struct sockaddr *addr)
{
    int     addrlen = 16;

    memset(addr, 0, 16);

    if (getsocknameFunc(ipx_sockets[socket], addr, &addrlen))
        WSAGetLastErrorFunc();

    return 0;
}

int WIPX_GetNameFromAddr(struct sockaddr *addr, char *name)
{
    Q_strcpy(name, WIPX_AddrToString(addr));
    return 0;
}

int WIPX_GetAddrFromName(char *name, struct sockaddr *addr)
{
    int     len;
    char    buffer[32];

    len = strlen(name);

    if (len == 12)
    {
        sprintf(buffer, "00000000:%s:%u", name, hostshort);
        return WIPX_StringToAddr(buffer, addr);
    }
    else if (len == 21)
    {
        sprintf(buffer, "%s:%u", name, hostshort);
        return WIPX_StringToAddr(buffer, addr);
    }
    else if (len > 21 && len <= 27)
    {
        return WIPX_StringToAddr(name, addr);
    }

    return -1;
}

int WIPX_AddrCompare(struct sockaddr *addr1, struct sockaddr *addr2)
{
    byte *a1 = (byte *)addr1;
    byte *a2 = (byte *)addr2;

    if (*(short *)a2 != *(short *)a1)
        return -1;

    if (a1[2] && a2[2])
    {
        if (memcmp(&a1[2], &a2[2], 4))
            return -1;
    }

    if (memcmp(&a1[6], &a2[6], 6))
        return -1;

    return (*(u_short *)&a2[12] != *(u_short *)&a1[12]) ? 1 : 0;
}

int WIPX_GetSocketPort(struct sockaddr *addr)
{
    return ntohs(*(u_short *)&((byte *)addr)[12]);
}

int WIPX_SetSocketPort(struct sockaddr *addr, int port)
{
    *(u_short *)&((byte *)addr)[12] = htons((u_short)port);
    return 0;
}
