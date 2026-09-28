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

// net_vcr.c -- plays back a network session recorded with -record

#include "quakedef.h"

#define VCR_OP_CONNECT			1
#define VCR_OP_GETMESSAGE		2
#define VCR_OP_SENDMESSAGE		3
#define VCR_OP_CANSENDMESSAGE	4
#define VCR_MAX_MESSAGE			4

// the next record in the playback file
static struct
{
	double	time;
	int		op;
	int		session;
} next;

int VCR_Init(void);
void VCR_Listen(qboolean state);
void VCR_SearchForHosts(qboolean xmit);
qsocket_t *VCR_Connect(char *host);
qsocket_t *VCR_CheckNewConnections(void);
int VCR_GetMessage(qsocket_t *sock);
int VCR_SendMessage(qsocket_t *sock, sizebuf_t *data);
qboolean VCR_CanSendMessage(qsocket_t *sock);
void VCR_Close(qsocket_t *sock);
void VCR_Shutdown(void);

net_driver_t net_vcr =
{
	"VCR",
	false,
	VCR_Init,
	VCR_Listen,
	VCR_SearchForHosts,
	VCR_Connect,
	VCR_CheckNewConnections,
	VCR_GetMessage,
	VCR_SendMessage,
	VCR_SendMessage,
	VCR_CanSendMessage,
	VCR_CanSendMessage,
	VCR_Close,
	VCR_Shutdown,
	-1
};

/*
================
VCR_Init
================
*/
int VCR_Init(void)
{
	memset(key_repeats, 0, sizeof(key_repeats));
	memset(keydown, 0, sizeof(keydown));
	return 0;
}

/*
================
VCR_ReadNext
================
*/
void VCR_ReadNext(void)
{
	if (Sys_FileRead(vcrFile, &next, sizeof(next)) == 0)
	{
		next.op = 255;
		Sys_Error("End of playback");
	}

	if (next.op < 1 || next.op > VCR_MAX_MESSAGE)
		Sys_Error("VCR_ReadNext: bad op");
}

/*
================
VCR_Listen
================
*/
void VCR_Listen(qboolean state)
{
}

/*
================
VCR_Shutdown
================
*/
void VCR_Shutdown(void)
{
}

/*
================
VCR_GetMessage
================
*/
int VCR_GetMessage(qsocket_t *sock)
{
	int		ret;

	if (net_time != next.time || next.op != VCR_OP_GETMESSAGE || sock->socket != next.session)
		Sys_Error("VCR missmatch");

	Sys_FileRead(vcrFile, &ret, sizeof(int));
	if (ret != 1)
	{
		VCR_ReadNext();
		return ret;
	}

	Sys_FileRead(vcrFile, &net_message.cursize, sizeof(int));
	Sys_FileRead(vcrFile, net_message.data, net_message.cursize);

	VCR_ReadNext();

	return 1;
}

/*
================
VCR_SendMessage
================
*/
int VCR_SendMessage(qsocket_t *sock, sizebuf_t *data)
{
	int		ret;

	if (net_time != next.time || next.op != VCR_OP_SENDMESSAGE || sock->socket != next.session)
		Sys_Error("VCR missmatch");

	Sys_FileRead(vcrFile, &ret, sizeof(int));

	VCR_ReadNext();

	return ret;
}

/*
================
VCR_CanSendMessage
================
*/
qboolean VCR_CanSendMessage(qsocket_t *sock)
{
	qboolean	ret;

	if (net_time != next.time || next.op != VCR_OP_CANSENDMESSAGE || sock->socket != next.session)
		Sys_Error("VCR missmatch");

	Sys_FileRead(vcrFile, &ret, sizeof(int));

	VCR_ReadNext();

	return ret;
}

/*
================
VCR_Close
================
*/
void VCR_Close(qsocket_t *sock)
{
}

/*
================
VCR_SearchForHosts
================
*/
void VCR_SearchForHosts(qboolean xmit)
{
}

/*
================
VCR_Connect
================
*/
qsocket_t *VCR_Connect(char *host)
{
	return NULL;
}

/*
================
VCR_CheckNewConnections
================
*/
qsocket_t *VCR_CheckNewConnections(void)
{
	qsocket_t	*sock;

	if (net_time != next.time || next.op != VCR_OP_CONNECT)
		Sys_Error("VCR missmatch");

	if (!next.session)
	{
		VCR_ReadNext();
		return NULL;
	}

	sock = NET_NewQSocket();
	sock->socket = next.session;
	Sys_FileRead(vcrFile, sock->address, NET_NAMELEN);

	VCR_ReadNext();

	return sock;
}
