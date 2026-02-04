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
 // net_loop.h -- Loopback network driver declarations

#ifndef NET_LOOP_H
#define NET_LOOP_H

 // Loopback driver functions
int  Loop_Init(void);
void Loop_Listen(qboolean state);
void Loop_SearchForHosts(qboolean xmit);
qsocket_t *Loop_Connect(char *host);
qsocket_t *Loop_CheckNewConnections(void);
int  Loop_GetMessage(qsocket_t *sock);
int  Loop_SendMessage(qsocket_t *sock, sizebuf_t *data);
int  Loop_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data);
qboolean Loop_CanSendMessage(qsocket_t *sock);
qboolean Loop_CanSendUnreliableMessage(qsocket_t *sock);
void Loop_Close(qsocket_t *sock);
void Loop_Shutdown(void);

 // Loopback state
extern qboolean localconnectpending;
extern qsocket_t *loop_client;
extern qsocket_t *loop_server;

#endif // NET_LOOP_H
