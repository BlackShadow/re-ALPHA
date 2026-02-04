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
#include "net_loop.h"

qboolean    localconnectpending = false;
qsocket_t   *loop_client = NULL;
qsocket_t   *loop_server = NULL;

int Loop_Init(void)
{
    return 1;
}

void Loop_Close(qsocket_t *sock)
{
    qsocket_t *other;

    other = (qsocket_t *)sock->driverdata;
    if (other)
        other->driverdata = NULL;

    sock->canSend = true;
    sock->receiveMessageLength = 0;
    sock->sendMessageLength = 0;

    if (sock == loop_client)
        loop_client = NULL;
    else
        loop_server = NULL;
}

void Loop_SearchForHosts(qboolean xmit)
{
    if (!sv.active)
        return;

    hostCacheCount = 1;

    if (Q_strcmp(hostname.string, "UNNAMED"))
        Q_strcpy(hostcache[0].name, hostname.string);
    else
        Q_strcpy(hostcache[0].name, "local");

    Q_strcpy(hostcache[0].map, sv.name);
    hostcache[0].users = net_activeconnections;
    hostcache[0].maxusers = svs.maxclients;
    hostcache[0].driver = net_driverlevel;
    Q_strcpy(hostcache[0].cname, "local");
}

qsocket_t *Loop_Connect(char *host)
{
    if (Q_strcmp(host, "local") != 0)
        return NULL;

    localconnectpending = true;

    if (!loop_client)
    {
        loop_client = NET_NewQSocket();
        if (!loop_client)
        {
            Con_Printf("Loop_Connect: no qsocket available\n");
            return NULL;
        }
        Q_strcpy(loop_client->address, "localhost");
    }

    loop_client->receiveMessageLength = 0;
    loop_client->sendMessageLength = 0;
    loop_client->canSend = true;

    if (!loop_server)
    {
        loop_server = NET_NewQSocket();
        if (!loop_server)
        {
            Con_Printf("Loop_Connect: no qsocket available\n");
            return NULL;
        }
        Q_strcpy(loop_server->address, "LOCAL");
    }

    loop_client->driverdata = (void *)loop_server;
    loop_server->receiveMessageLength = 0;
    loop_server->sendMessageLength = 0;
    loop_server->canSend = true;
    loop_server->driverdata = (void *)loop_client;

    return loop_client;
}

qsocket_t *Loop_CheckNewConnections(void)
{
    if (!localconnectpending)
        return NULL;

    localconnectpending = false;

    loop_server->sendMessageLength = 0;
    loop_server->receiveMessageLength = 0;
    loop_server->canSend = true;

    loop_client->sendMessageLength = 0;
    loop_client->receiveMessageLength = 0;
    loop_client->canSend = true;

    return loop_server;
}

static int IntAlign(int value)
{
    return (value + (sizeof(int) - 1)) & (~(sizeof(int) - 1));
}

int Loop_GetMessage(qsocket_t *sock)
{
    int     ret;
    int     length;

    if (sock->receiveMessageLength == 0)
        return 0;

    ret = sock->receiveMessage[0];
    length = sock->receiveMessage[1] + (sock->receiveMessage[2] << 8);

    SZ_Clear(&net_message);
    SZ_Write(&net_message, &sock->receiveMessage[4], length);

    length = IntAlign(length + 4);
    sock->receiveMessageLength -= length;

    if (sock->receiveMessageLength)
        Q_memcpy(sock->receiveMessage, &sock->receiveMessage[length], sock->receiveMessageLength);

    if (sock->driverdata && ret == 1)
        ((qsocket_t *)sock->driverdata)->canSend = true;

    return ret;
}

int Loop_SendMessage(qsocket_t *sock, sizebuf_t *data)
{
    byte    *buffer;
    int     *bufferLength;

    if (!sock->driverdata)
        return -1;

    bufferLength = &((qsocket_t *)sock->driverdata)->receiveMessageLength;

    if ((*bufferLength + data->cursize + 4) > NET_MAXMESSAGE)
        Sys_Error("Loop_SendMessage: overflow\n");

    buffer = ((qsocket_t *)sock->driverdata)->receiveMessage + *bufferLength;

    *buffer++ = 1;

    *buffer++ = data->cursize & 0xff;
    *buffer++ = data->cursize >> 8;

    buffer++;

    Q_memcpy(buffer, data->data, data->cursize);
    *bufferLength = IntAlign(*bufferLength + data->cursize + 4);

    sock->canSend = false;
    return 1;
}

int Loop_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data)
{
    byte    *buffer;
    int     *bufferLength;

    if (!sock->driverdata)
        return -1;

    bufferLength = &((qsocket_t *)sock->driverdata)->receiveMessageLength;

    if ((*bufferLength + data->cursize + sizeof(byte) + sizeof(short)) > NET_MAXMESSAGE)
        return 0;

    buffer = ((qsocket_t *)sock->driverdata)->receiveMessage + *bufferLength;

    *buffer++ = 2;

    *buffer++ = data->cursize & 0xff;
    *buffer++ = data->cursize >> 8;

    buffer++;

    Q_memcpy(buffer, data->data, data->cursize);
    *bufferLength = IntAlign(*bufferLength + data->cursize + 4);

    return 1;
}

qboolean Loop_CanSendMessage(qsocket_t *sock)
{
    if (!sock->driverdata)
        return false;
    return sock->canSend;
}

qboolean Loop_CanSendUnreliableMessage(qsocket_t *sock)
{
    return true;
}

void Loop_Shutdown(void)
{
}

void Loop_Listen(qboolean state)
{
}
