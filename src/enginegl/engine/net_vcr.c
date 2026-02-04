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

extern int		vcrFile;
double			vcr_time;
int				vcr_op;
int				vcr_extra;
extern double	net_time;
extern sizebuf_t net_message;

#define VCR_OP_CONNECT			1
#define VCR_OP_GETMESSAGE		2
#define VCR_OP_SENDMESSAGE		3
#define VCR_OP_CANSENDMESSAGE	4

int VCR_Init(void)
{
	memset(key_repeats, 0, sizeof(key_repeats));
	memset(keydown, 0, sizeof(keydown));
	return 0;
}

void VCR_Listen(qboolean state);
void VCR_Shutdown(void);
int VCR_GetMessage(qsocket_t *sock);
int VCR_SendMessage(qsocket_t *sock, sizebuf_t *data);
qboolean VCR_CanSendMessage(qsocket_t *sock);
void VCR_Close(qsocket_t *sock);
void VCR_SearchForHosts(qboolean xmit);
qsocket_t *VCR_Connect(char *host);
qsocket_t *VCR_CheckNewConnections(void);

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

int VCR_ReadNext(void)
{
	int result;
	byte buffer[16];

	result = Sys_FileRead(vcrFile, buffer, 16);
	if (!result)
	{
		vcr_op = 255;
		Sys_Error("End of playback");
	}

	vcr_time = *(double *)buffer;
	vcr_op = *(int *)(buffer + 8);
	vcr_extra = *(int *)(buffer + 12);

	if (vcr_op < 1 || vcr_op > 4)
		Sys_Error("VCR_ReadNext: bad op");

	return result;
}

void VCR_Listen(qboolean state)
{
}

void VCR_Shutdown(void)
{
}

int VCR_GetMessage(qsocket_t *sock)
{
	int		ret;

	if (net_time != vcr_time || vcr_op != VCR_OP_GETMESSAGE || sock->socket != vcr_extra)
		Sys_Error("VCR missmatch");

	Sys_FileRead(vcrFile, &ret, 4);
	if (ret == 1)
	{
		Sys_FileRead(vcrFile, &net_message.cursize, 4);
		Sys_FileRead(vcrFile, net_message.data, net_message.cursize);
		VCR_ReadNext();
		return 1;
	}
	else
	{
		VCR_ReadNext();
		return ret;
	}
}

int VCR_SendMessage(qsocket_t *sock, sizebuf_t *data)
{
	int		ret;

	if (net_time != vcr_time || vcr_op != VCR_OP_SENDMESSAGE || sock->socket != vcr_extra)
		Sys_Error("VCR missmatch");

	Sys_FileRead(vcrFile, &ret, 4);
	VCR_ReadNext();
	return ret;
}

qboolean VCR_CanSendMessage(qsocket_t *sock)
{
	int		ret;

	if (net_time != vcr_time || vcr_op != VCR_OP_CANSENDMESSAGE || sock->socket != vcr_extra)
		Sys_Error("VCR missmatch");

	Sys_FileRead(vcrFile, &ret, 4);
	VCR_ReadNext();
	return ret;
}

void VCR_Close(qsocket_t *sock)
{

}

void VCR_SearchForHosts(qboolean xmit)
{

}

qsocket_t *VCR_Connect(char *host)
{
	return NULL;
}

qsocket_t *VCR_CheckNewConnections(void)
{
	qsocket_t	*sock;

	if (net_time != vcr_time || vcr_op != VCR_OP_CONNECT)
		Sys_Error("VCR missmatch");

	if (vcr_extra)
	{
		sock = NET_NewQSocket();
		sock->socket = vcr_extra;
		Sys_FileRead(vcrFile, sock->address, 64);
		VCR_ReadNext();
		return sock;
	}
	else
	{
		VCR_ReadNext();
		return NULL;
	}
}
