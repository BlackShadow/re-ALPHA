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
#include "protocol.h"
#include "server.h"

#define SV_AREAENTITIES_COUNT 28000

static int		sv_areaentities[SV_AREAENTITIES_COUNT + 1];
static int		sv_entity_list_head;
static int		sv_entity_list_tail;

char			localmodels[MAX_MODELS][5];
byte			fatpvs[MAX_MAP_LEAFS/8];
int				fatbytes;

extern unsigned short	pr_crc;
extern void				DispatchEntityCallback(int callbackIndex);

cvar_t sv_maxvelocity = {"sv_maxvelocity", "2000"};
cvar_t sv_gravity = {"sv_gravity", "800", false, true};
cvar_t sv_friction = {"sv_friction", "4", false, true};
cvar_t sv_edgefriction = {"edgefriction", "2"};
cvar_t sv_stopspeed = {"sv_stopspeed", "100"};
cvar_t sv_maxspeed = {"sv_maxspeed", "320", false, true};
cvar_t sv_accelerate = {"sv_accelerate", "10"};
cvar_t sv_idealpitchscale = {"sv_idealpitchscale", "0.8"};
cvar_t sv_aim = {"sv_aim", "1"};
cvar_t sv_nostep = {"sv_nostep", "0"};

void SV_Init(void)
{
	int i;

	Cvar_RegisterVariable(&sv_maxvelocity);
	Cvar_RegisterVariable(&sv_gravity);
	Cvar_RegisterVariable(&sv_friction);
	Cvar_RegisterVariable(&sv_edgefriction);
	Cvar_RegisterVariable(&sv_stopspeed);
	Cvar_RegisterVariable(&sv_maxspeed);
	Cvar_RegisterVariable(&sv_accelerate);
	Cvar_RegisterVariable(&sv_idealpitchscale);
	Cvar_RegisterVariable(&sv_aim);
	Cvar_RegisterVariable(&sv_nostep);

	for (i = 0; i < MAX_MODELS; i++)
		sprintf(localmodels[i], "*%i", i);
}

void SV_StartSound(edict_t *entity, int channel, const char *sample, int volume, float attenuation)
{
	int			sound_num;
	int			field_mask;
	int			i;
	int			ent;
	vec3_t		origin;

	if (volume < 0 || volume > 255)
		Sys_Error("SV_StartSound: volume = %i", volume);

	if (attenuation < 0.0f || attenuation > 4.0f)
		Sys_Error("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel >= 8)
		Sys_Error("SV_StartSound: channel = %i", channel);

	if (sv.datagram.cursize > MAX_DATAGRAM - 16)
		return;

	for (sound_num = 1; sound_num < MAX_SOUNDS && sv.sound_precache[sound_num]; sound_num++)
	{
		if (!strcmp(sample, sv.sound_precache[sound_num]))
			break;
	}

	if (sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num])
	{
		Con_Printf("SV_StartSound: %s not precacheed\n", sample);
		return;
	}

	ent = NUM_FOR_EDICT(entity);
	channel = (ent << 3) | channel;

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;

	MSG_WriteByte(&sv.datagram, svc_sound);
	MSG_WriteByte(&sv.datagram, field_mask);

	if (field_mask & SND_VOLUME)
		MSG_WriteByte(&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte(&sv.datagram, (int)(attenuation * 64.0f));

	MSG_WriteShort(&sv.datagram, channel);
	MSG_WriteByte(&sv.datagram, sound_num);

	for (i = 0; i < 3; i++)
		origin[i] = entity->v.origin[i] + 0.5f * (entity->v.mins[i] + entity->v.maxs[i]);

	MSG_WriteCoord(&sv.datagram, origin[0]);
	MSG_WriteCoord(&sv.datagram, origin[1]);
	MSG_WriteCoord(&sv.datagram, origin[2]);
}

void SV_SendServerinfo(server_client_t *client)
{
	char	**s;
	char	message[2048];

	MSG_WriteByte(&client->message, svc_print);
	sprintf(message, "VERSION %4.2f SERVER (%i CRC)\n", VERSION, pr_crc);
	MSG_WriteString(&client->message, message);

	MSG_WriteByte(&client->message, svc_serverinfo);
	MSG_WriteLong(&client->message, PROTOCOL_VERSION);
	MSG_WriteByte(&client->message, svs.maxclients);

	if (deathmatch.value != 0.0f || coop.value == 0.0f)
		MSG_WriteByte(&client->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte(&client->message, GAME_COOP);

	sprintf(message, "%s", pr_strings + sv.edicts->v.message);
	MSG_WriteString(&client->message, message);

	for (s = sv.model_precache + 1; *s; s++)
		MSG_WriteString(&client->message, *s);
	MSG_WriteByte(&client->message, 0);

	for (s = sv.sound_precache + 1; *s; s++)
		MSG_WriteString(&client->message, *s);
	MSG_WriteByte(&client->message, 0);

	MSG_WriteByte(&client->message, svc_cdtrack);
	MSG_WriteByte(&client->message, (int)sv.edicts->v.sounds);
	MSG_WriteByte(&client->message, (int)sv.edicts->v.sounds);

	MSG_WriteByte(&client->message, svc_setview);
	MSG_WriteShort(&client->message, NUM_FOR_EDICT(client->edict));

	MSG_WriteByte(&client->message, svc_signonnum);
	MSG_WriteByte(&client->message, 1);

	client->sendsignon = true;
	client->spawned = false;
}

void SV_ConnectClient(int clientnum)
{
	server_client_t	*client;
	edict_t		*ent;
	int			edictnum;
	qsocket_t	*saved_netconnection;
	float		spawn_parms[NUM_SPAWN_PARMS];
	int			i;

	client = svs.clients + clientnum;

	Con_DPrintf("Client %s connected\n", NET_QSocketGetString(client->netconnection));

	saved_netconnection = client->netconnection;

	edictnum = clientnum + 1;
	ent = EDICT_NUM(edictnum);

	memcpy(spawn_parms, client->spawn_parms, sizeof(spawn_parms));

	memset(client, 0, sizeof(*client));
	client->netconnection = saved_netconnection;
	client->edict = ent;
	strcpy(client->name, "unconnected");
	client->active = true;
	client->spawned = false;
	client->edict = ent;
	client->message.data = client->msgbuf;
	client->message.allowoverflow = true;
	client->privileged = false;
	client->message.maxsize = MAX_MSGLEN;

	if (sv.loadgame)
	{
		memcpy(client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	}
	else
	{
		qboolean has_parms = false;
		for (i = 0; i < NUM_SPAWN_PARMS; i++)
		{
			if (spawn_parms[i])
			{
				has_parms = true;
				break;
			}
		}

		if (has_parms)
		{
			memcpy(client->spawn_parms, spawn_parms, sizeof(spawn_parms));
		}
		else
		{
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(ent);
			DispatchEntityCallback(4);
			memcpy(client->spawn_parms, &pr_global_struct->parm1, sizeof(spawn_parms));
		}
	}

	SV_SendServerinfo(client);

	for (i = 0; i < sv_decalnamecount; i++)
	{
		MSG_WriteChar(&client->message, svc_decalname);
		MSG_WriteChar(&client->message, i);
		MSG_WriteString(&client->message, sv_decalnames[i]);
	}
}

void SV_CheckForNewClients(void)
{
	struct qsocket_s	*ret;
	int					i;

	while (1)
	{
		ret = NET_CheckNewConnections();
		if (!ret)
			break;

		for (i = 0; i < svs.maxclients; i++)
		{
			if (!svs.clients[i].active)
				break;
		}

		if (i == svs.maxclients)
			Sys_Error("Host_CheckForNewClients: no free clients");

		svs.clients[i].netconnection = ret;
		SV_ConnectClient(i);
		net_activeconnections++;
	}
}

void SV_ClearDatagram(void)
{
	SZ_Clear(&sv.datagram);
}

void SV_AddToFatPVS(vec3_t org, mnode_t *node)
{
	float		d;
	mplane_t	*plane;
	byte		*pvs;
	int			i;

	while (node->contents >= 0)
	{
		plane = node->plane;
		d = DotProduct(org, plane->normal) - plane->dist;

		if (d > 8.0f)
			node = node->children[0];
		else if (d < -8.0f)
			node = node->children[1];
		else
		{
			SV_AddToFatPVS(org, node->children[0]);
			node = node->children[1];
		}
	}

	if (node->contents != CONTENTS_SOLID)
	{
		pvs = Mod_LeafPVS((mleaf_t *)node, sv.worldmodel);
		for (i = 0; i < fatbytes; i++)
			fatpvs[i] |= pvs[i];
	}
}

byte *SV_FatPVS(vec3_t org)
{
	fatbytes = (sv.worldmodel->numleafs + 31) >> 3;
	Q_memset(fatpvs, 0, fatbytes);
	SV_AddToFatPVS(org, sv.worldmodel->nodes);
	return fatpvs;
}

void SV_WriteEntitiesToClient(edict_t *clent, sizebuf_t *msg)
{
	byte *pvs;
	vec3_t org;
	byte *ent_bytes;
	int ent_index;

	org[0] = clent->v.view_ofs[0] + clent->v.origin[0];
	org[1] = clent->v.view_ofs[1] + clent->v.origin[1];
	org[2] = clent->v.view_ofs[2] + clent->v.origin[2];
	pvs = SV_FatPVS(org);

	ent_bytes = (byte *)sv_edicts + pr_edict_size;

	for (ent_index = 1; ent_index < *sv_num_edicts; ++ent_index, ent_bytes += pr_edict_size)
	{
		float *origin;
		float *angles;
		int bits;
		int axis;

		if ((edict_t *)ent_bytes != clent && *(float *)(ent_bytes + 260) == (float)EF_NODRAW)
			continue;

		if ((edict_t *)ent_bytes != clent)
		{
			if (*(float *)(ent_bytes + 120) == 0.0f || !*(byte *)(pr_strings + *(int *)(ent_bytes + 248)))
				continue;

			{
				int leaf_i;
				for (leaf_i = 0; leaf_i < *(int *)(ent_bytes + 12); ++leaf_i)
				{
					int leafnum = *(short *)(ent_bytes + 16 + 2 * leaf_i);
					int byte_index = leafnum >> 3;
					int bit_index = leafnum & 7;
					if (pvs[byte_index] & (1 << bit_index))
						break;
				}
				if (leaf_i == *(int *)(ent_bytes + 12))
					continue;
			}
		}

		if (msg->maxsize - msg->cursize < 16)
		{
			Con_Printf("packet overflow\n");
			return;
		}

		bits = 0;
		origin = (float *)(ent_bytes + 160);
		angles = (float *)(ent_bytes + 196);

		for (axis = 0; axis < 3; ++axis)
		{
			float miss = origin[axis] - *(float *)(ent_bytes + 48 + 4 * axis);
			if (miss < -0.1f || miss > 0.1f)
				bits |= 2 << axis;
		}

		if (*(float *)(ent_bytes + 60) != angles[0])
			bits |= 0x100;
		if (*(float *)(ent_bytes + 64) != angles[1])
			bits |= 0x10;
		if (*(float *)(ent_bytes + 68) != angles[2])
			bits |= 0x200;

		if (*(float *)(ent_bytes + 152) == 4.0f)
			bits |= 0x20;

		if ((float)*(int *)(ent_bytes + 84) != *(float *)(ent_bytes + 504))
			bits |= 0x800;
		if ((float)*(int *)(ent_bytes + 88) != *(float *)(ent_bytes + 252))
			bits |= 0x1000;
		if ((float)*(int *)(ent_bytes + 80) != *(float *)(ent_bytes + 284))
			bits |= 0x40;
		if ((float)*(int *)(ent_bytes + 92) != *(float *)(ent_bytes + 260))
			bits |= 0x2000;
		if ((float)*(int *)(ent_bytes + 72) != *(float *)(ent_bytes + 120))
			bits |= 0x400;

		{
			const float animtime = *(float *)(ent_bytes + 280);

			if (animtime != 0.0f)
			{
				bits |= 0x20000;
			}
			else
			{

				const int modelindex = (int)*(float *)(ent_bytes + 120);
				if (modelindex > 0 && modelindex < MAX_MODELS)
				{
					model_t *model = sv.models[modelindex];
					if (model && model->type == mod_studio)
						bits |= 0x20000;
				}
			}
		}
		if (*(float *)(ent_bytes + 288) != 1.0f)
			bits |= 0x400000;
		if (*(float *)(ent_bytes + 256) != 0.0f)
			bits |= 0x200000;
		if (*(int *)(ent_bytes + 292))
			bits |= 0x40000;
		if (*(int *)(ent_bytes + 296))
			bits |= 0x100000;

		{
			float baseline_rendermode = (float)*(int *)(ent_bytes + 96);
			float baseline_renderamt = (float)*(int *)(ent_bytes + 100);
			float baseline_renderfx = (float)*(int *)(ent_bytes + 108);
			float baseline_rendercolor_x = (float)*(unsigned char *)(ent_bytes + 104);
			float baseline_rendercolor_y = (float)*(unsigned char *)(ent_bytes + 105);
			float baseline_rendercolor_z = (float)*(unsigned char *)(ent_bytes + 106);

			if (baseline_rendermode != *(float *)(ent_bytes + 360)
				|| baseline_renderamt != *(float *)(ent_bytes + 364)
				|| baseline_renderfx != *(float *)(ent_bytes + 380)
				|| baseline_rendercolor_x != *(float *)(ent_bytes + 368)
				|| baseline_rendercolor_y != *(float *)(ent_bytes + 372)
				|| baseline_rendercolor_z != *(float *)(ent_bytes + 376))
			{
				bits |= 0x80000;
			}
		}

		if (*(float *)(ent_bytes + 280) != 0.0f
			&& *(float *)(ent_bytes + 184) == 0.0f
			&& *(float *)(ent_bytes + 188) == 0.0f
			&& *(float *)(ent_bytes + 192) == 0.0f)
		{
			bits |= 0x800000;
		}

		if (ent_index >= 256)
			bits |= 0x4000;
		if (bits >= 256)
			bits |= 1;
		if (bits >= 0x10000)
			bits |= 0x8000;

		MSG_WriteByte(msg, (bits & 0xFF) | 0x80);
		if (bits & 1)
			MSG_WriteByte(msg, (bits >> 8) & 0xFF);
		if (bits & 0x8000)
			MSG_WriteByte(msg, (bits >> 16) & 0xFF);

		if (bits & 0x4000)
			MSG_WriteShort(msg, ent_index);
		else
			MSG_WriteByte(msg, ent_index);

		if (bits & 0x400)
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 120));
		if (bits & 0x40)
			MSG_WriteShort(msg, (int)(*(float *)(ent_bytes + 284) * 256.0f));
		if (bits & 0x800)
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 504));
		if (bits & 0x1000)
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 252));
		if (bits & 0x2000)
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 260));

		if (bits & 2)
			MSG_WriteCoord(msg, origin[0]);
		if (bits & 0x100)
			MSG_WriteAngle(msg, angles[0]);
		if (bits & 4)
			MSG_WriteCoord(msg, origin[1]);
		if (bits & 0x10)
			MSG_WriteAngle(msg, angles[1]);
		if (bits & 8)
			MSG_WriteCoord(msg, origin[2]);
		if (bits & 0x200)
			MSG_WriteAngle(msg, angles[2]);

		if (bits & 0x20000)
		{
			MSG_WriteByte(msg, *(int *)(ent_bytes + 276));
			{
				const float animtime = *(float *)(ent_bytes + 280);
				if (animtime != 0.0f)
					MSG_WriteByte(msg, (int)(animtime * 100.0f));
				else
					MSG_WriteByte(msg, (int)(sv.time * 100.0f));
			}
		}
		if (bits & 0x400000)
			MSG_WriteChar(msg, (int)(*(float *)(ent_bytes + 288) * 64.0f));
		if (bits & 0x40000)
			MSG_WriteLong(msg, *(int *)(ent_bytes + 292));
		if (bits & 0x100000)
			MSG_WriteShort(msg, *(short *)(ent_bytes + 296));
		if (bits & 0x200000)
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 256));

		if (bits & 0x80000)
		{
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 360));
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 364));
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 368));
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 372));
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 376));
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 380));
		}
	}
}

void SV_CleanupEnts(void)
{
	int		e;
	edict_t	*ent;

	for (e = 1; e < sv.num_edicts; e++)
	{
		ent = EDICT_NUM(e);
		ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
	}
}

void SV_WriteClientdataToMessage(edict_t *ent, sizebuf_t *msg)
{
	byte *ent_bytes = (byte *)ent;
	float *damage_take = (float *)(ent_bytes + 564);

	if (*(float *)(ent_bytes + 564) != 0.0f || *(float *)(ent_bytes + 568) != 0.0f)
	{
		byte *inflictor_bytes = (byte *)sv_edicts + *(int *)(ent_bytes + 572);
		float *inflictor_maxs = (float *)(inflictor_bytes + 312);
		int axis;

		MSG_WriteByte(msg, 19);
		MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 568));
		MSG_WriteByte(msg, (int)*damage_take);

		for (axis = 0; axis < 3; ++axis)
		{
			float from = (inflictor_maxs[axis - 3] + inflictor_maxs[axis]) * 0.5f + inflictor_maxs[axis - 38];
			MSG_WriteCoord(msg, from);
		}

		*damage_take = 0.0f;
		*(int *)(ent_bytes + 568) = 0;
	}

	SV_SetIdealPitch();

	if (*(float *)(ent_bytes + 468) != 0.0f)
	{
		int axis;

		MSG_WriteByte(msg, 10);
		for (axis = 0; axis < 3; ++axis)
			MSG_WriteAngle(msg, *(float *)(ent_bytes + 196 + 4 * axis));
		*(int *)(ent_bytes + 468) = 0;
	}

	{
		int bits = (*(float *)(ent_bytes + 456) != 22.0f);
		int axis;
		int items_combined = *(int *)(ent_bytes + 428) | (*(int *)(ent_bytes + 432) << 23);
		float *punch = (float *)(ent_bytes + 232);
		float *velocity = (float *)(ent_bytes + 184);

		if (*(float *)(ent_bytes + 484) != 0.0f)
			bits |= 2;

		bits |= 0x0200;

		if (*(int *)(ent_bytes + 396))
			bits = (bits & ~0xFF00) | 0x0300;
		if (((int)*(float *)(ent_bytes + 500) & 0x200) != 0)
			bits |= 0x0400;
		if (*(float *)(ent_bytes + 528) >= 2.0f)
			bits |= 0x0800;
		if (*(float *)(ent_bytes + 528) >= 3.0f)
			bits |= 0x8000;

		for (axis = 0; axis < 3; ++axis)
		{
			if (punch[axis] != 0.0f)
				bits |= 4 << axis;
			if (velocity[axis] != 0.0f)
				bits |= 32 << axis;
		}

		if (*(float *)(ent_bytes + 404) != 0.0f)
			bits |= 0x1000;
		if (*(float *)(ent_bytes + 524) != 0.0f)
			bits |= 0x2000;

		bits |= 0x4000;

		MSG_WriteByte(msg, 15);
		MSG_WriteShort(msg, bits);

		if (bits & 1)
			MSG_WriteChar(msg, (int)*(float *)(ent_bytes + 456));
		if (bits & 2)
			MSG_WriteChar(msg, (int)*(float *)(ent_bytes + 484));

		for (axis = 0; axis < 3; ++axis)
		{
			if (bits & (4 << axis))
				MSG_WriteChar(msg, (int)punch[axis]);
			if (bits & (32 << axis))
				MSG_WriteChar(msg, (int)(velocity[axis] / 16.0f));
		}

		MSG_WriteLong(msg, items_combined);
		MSG_WriteLong(msg, *(int *)(ent_bytes + 432));

		if (bits & 0x0100)
			MSG_WriteLong(msg, *(int *)(ent_bytes + 396));
		if (bits & 0x1000)
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 404));
		if (bits & 0x2000)
			MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 524));
		if (bits & 0x4000)
			MSG_WriteByte(msg, SV_ModelIndex(pr_strings + *(int *)(ent_bytes + 400)));

		MSG_WriteShort(msg, (int)*(float *)(ent_bytes + 384));
		MSG_WriteByte(msg, (int)*(float *)(ent_bytes + 408));
		MSG_WriteByte(msg, *(int *)(ent_bytes + 412));
		MSG_WriteByte(msg, *(int *)(ent_bytes + 416));
		MSG_WriteByte(msg, *(int *)(ent_bytes + 420));
		MSG_WriteByte(msg, *(int *)(ent_bytes + 424));
		MSG_WriteLong(msg, *(int *)(ent_bytes + 392));
	}
}

qboolean SV_SendClientDatagram(server_client_t *client)
{
	byte		buf[MAX_DATAGRAM];
	sizebuf_t	msg;

	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	MSG_WriteByte(&msg, svc_time);
	MSG_WriteFloat(&msg, sv.time);

	SV_WriteClientdataToMessage(client->edict, &msg);
	SV_WriteEntitiesToClient(client->edict, &msg);

	if (msg.cursize + sv.datagram.cursize < msg.maxsize)
		SZ_Write(&msg, sv.datagram.data, sv.datagram.cursize);

	if (NET_SendUnreliableMessage(client->netconnection, &msg) == -1)
	{
		SV_DropClient(true);
		return false;
	}

	return true;
}

void SV_UpdateToReliableMessages(void)
{
	int		i, j;
	server_client_t *client;

	for (i = 0; i < svs.maxclients; i++)
	{
		host_client = svs.clients + i;

		if ((int)host_client->edict->v.frags != host_client->old_frags)
		{
			for (j = 0; j < svs.maxclients; j++)
			{
				client = svs.clients + j;
				if (client->active)
				{
					MSG_WriteByte(&client->message, svc_updatefrags);
					MSG_WriteByte(&client->message, i);
					MSG_WriteShort(&client->message, (int)host_client->edict->v.frags);
				}
			}
			host_client->old_frags = (int)host_client->edict->v.frags;
		}
	}

	for (i = 0; i < svs.maxclients; i++)
	{
		client = svs.clients + i;
		if (client->active)
			SZ_Write(&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
	}

	SZ_Clear(&sv.reliable_datagram);
}

void SV_SendNop(server_client_t *client)
{
	sizebuf_t	msg;
	byte		buf[4];

	msg.data = buf;
	msg.maxsize = 4;
	msg.cursize = 0;

	MSG_WriteChar(&msg, svc_nop);

	if (NET_SendUnreliableMessage(client->netconnection, &msg) == -1)
		SV_DropClient(true);

	client->last_message = realtime;
}

void SV_SendClientMessages(void)
{
	int		i;
	server_client_t *client;

	SV_UpdateToReliableMessages();

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;

		if (client->spawned)
		{
			if (!SV_SendClientDatagram(client))
				continue;
		}
		else
		{

			if (!client->sendsignon)
			{

				if (realtime - client->last_message > 5.0)
					SV_SendNop(client);
				continue;
			}
		}

		host_client = client;

		if (client->dropasap)
		{
			SV_DropClient(true);
			client->dropasap = false;
		}
		else if (client->message.overflowed || client->message.cursize)
		{
			if (NET_CanSendMessage(client->netconnection))
			{
				if (client->message.overflowed)
				{
					SV_DropClient(false);
				}
				else
				{
					if (NET_SendMessage(client->netconnection, &client->message) == -1)
						SV_DropClient(true);
					SZ_Clear(&client->message);
					client->sendsignon = false;
					client->last_message = realtime;
				}
			}
		}
	}

	SV_CleanupEnts();
}

int SV_ModelIndex(char *name)
{
	int	i;

	if (!name || !name[0])
		return 0;

	for (i = 0; i < MAX_MODELS && sv.model_precache[i]; i++)
	{
		if (!strcmp(sv.model_precache[i], name))
			return i;
	}

	if (i == MAX_MODELS || !sv.model_precache[i])
		Sys_Error("SV_ModelIndex: model %s not precached", name);

	return i;
}

void SV_CreateBaseline(void)
{
	int ent_index;

	for (ent_index = 0; ent_index < *sv_num_edicts; ++ent_index)
	{
		byte *ent_bytes = (byte *)sv_edicts + ent_index * pr_edict_size;
		int model;
		float *baseline_pos_angles;
		int axis;

		if (*(int *)ent_bytes)
			continue;

		if (ent_index > svs.maxclients && *(float *)(ent_bytes + 120) == 0.0f)
			continue;

		*(int *)(ent_bytes + 48) = *(int *)(ent_bytes + 160);
		*(int *)(ent_bytes + 52) = *(int *)(ent_bytes + 164);
		*(int *)(ent_bytes + 56) = *(int *)(ent_bytes + 168);
		*(int *)(ent_bytes + 60) = *(int *)(ent_bytes + 196);
		*(int *)(ent_bytes + 64) = *(int *)(ent_bytes + 200);
		*(int *)(ent_bytes + 68) = *(int *)(ent_bytes + 204);

		*(int *)(ent_bytes + 80) = (int)*(float *)(ent_bytes + 284);
		*(int *)(ent_bytes + 88) = (int)*(float *)(ent_bytes + 252);

		if (ent_index > 0 && ent_index <= svs.maxclients)
		{
			*(int *)(ent_bytes + 84) = ent_index;
			model = SV_ModelIndex("models/doctor.mdl");
		}
		else
		{
			*(int *)(ent_bytes + 84) = 0;
			model = SV_ModelIndex(pr_strings + *(int *)(ent_bytes + 248));
		}

		*(int *)(ent_bytes + 72) = model;

		*(int *)(ent_bytes + 96) = (int)*(float *)(ent_bytes + 360);
		*(int *)(ent_bytes + 100) = (int)*(float *)(ent_bytes + 364);
		*(byte *)(ent_bytes + 104) = (byte)(int)*(float *)(ent_bytes + 368);
		*(byte *)(ent_bytes + 105) = (byte)(int)*(float *)(ent_bytes + 372);
		*(byte *)(ent_bytes + 106) = (byte)(int)*(float *)(ent_bytes + 376);
		*(int *)(ent_bytes + 108) = (int)*(float *)(ent_bytes + 380);

		MSG_WriteByte(&sv.signon, svc_spawnbaseline);
		MSG_WriteShort(&sv.signon, ent_index);
		MSG_WriteByte(&sv.signon, *(int *)(ent_bytes + 72));
		MSG_WriteByte(&sv.signon, *(int *)(ent_bytes + 76));
		MSG_WriteByte(&sv.signon, *(int *)(ent_bytes + 80));
		MSG_WriteByte(&sv.signon, *(int *)(ent_bytes + 84));
		MSG_WriteByte(&sv.signon, *(int *)(ent_bytes + 88));

		baseline_pos_angles = (float *)(ent_bytes + 48);
		for (axis = 0; axis < 3; ++axis)
		{
			float coord = *baseline_pos_angles++;
			MSG_WriteCoord(&sv.signon, coord);
			MSG_WriteAngle(&sv.signon, baseline_pos_angles[2]);
		}

		MSG_WriteByte(&sv.signon, (int)*(float *)(ent_bytes + 360));
		if (*(float *)(ent_bytes + 360) != 0.0f)
		{
			MSG_WriteByte(&sv.signon, (int)*(float *)(ent_bytes + 364));
			MSG_WriteByte(&sv.signon, (int)*(float *)(ent_bytes + 368));
			MSG_WriteByte(&sv.signon, (int)*(float *)(ent_bytes + 372));
			MSG_WriteByte(&sv.signon, (int)*(float *)(ent_bytes + 376));
			MSG_WriteByte(&sv.signon, (int)*(float *)(ent_bytes + 380));
		}
	}
}

void SV_SendReconnect(void)
{
	byte		buf[128];
	sizebuf_t	msg;

	msg.data = buf;
	msg.maxsize = 128;
	msg.cursize = 0;

	MSG_WriteChar(&msg, svc_stufftext);
	MSG_WriteString(&msg, "reconnect\n");

	NET_SendToAll(&msg, 5);

	if (cls.state != ca_dedicated)
		Cmd_ExecuteString("reconnect\n", src_command);
}

void SV_SaveSpawnparms(void)
{
	int		i;
	server_client_t *client;
	int token_live;
	qboolean keep_live_token;

	svs.serverflags = (int)pr_global_struct->serverflags;

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;

		// HL alpha SetChangeParms expects incoming parm1..parm16 to contain the
		// current transition block (parm1 is used as a pointer-sized token).
		pr_global_struct->self = EDICT_TO_PROG(client->edict);
		pr_global_struct->time = sv.time;
		token_live = *(int *)&pr_global_struct->parm1;
		keep_live_token = (g_iextdllcount > 0 && svs.changelevel_issued && token_live != 0);

		if (!keep_live_token)
		{
			memcpy(&pr_global_struct->parm1, client->spawn_parms, sizeof(client->spawn_parms));
			if (g_iextdllcount > 0 && (*(int *)&pr_global_struct->parm1) == 0)
				DispatchEntityCallback(4);
		}

		DispatchEntityCallback(5);
		memcpy(client->spawn_parms, &pr_global_struct->parm1, sizeof(client->spawn_parms));
	}
}

void SV_SpawnServer(char *server, char *startspot)
{
	edict_t		*ent;
	int			i;

	if (!hostname.string[0])
		Cvar_Set("hostname", "unnamed");

	scr_centertime_off = 0.0f;

	Con_DPrintf("SpawnServer: %s\n", server);
	svs.changelevel_issued = false;

	if (sv.active)
		SV_SendReconnect();

	if (coop.value != 0.0f)
		Cvar_SetValue("deathmatch", 0.0f);

	current_skill = (int)(skill.value + 0.5f);
	if (current_skill < 0)
		current_skill = 0;
	if (current_skill > 3)
		current_skill = 3;

	Cvar_SetValue("skill", (float)current_skill);

	Host_ClearMemory();

	memset(&sv, 0, 0x3638);
	strcpy(sv.name, server);

	if (startspot)
	{
		strcpy(sv.startspot, startspot);
	}

	PR_LoadProgs();

	sv.max_edicts = MAX_EDICTS;
	sv.edicts = Hunk_AllocName(sv.max_edicts * pr_edict_size, "edicts");
	sv_max_edicts = sv.max_edicts;
	sv_edicts = sv.edicts;

	sv.datagram.cursize = 0;
	sv.datagram.maxsize = MAX_DATAGRAM;
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.maxsize = MAX_DATAGRAM;
	sv.signon.cursize = 0;
	sv.signon.maxsize = MAX_SIGNON;
	sv.datagram.data = sv.datagram_buf;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;
	sv.signon.data = sv.signon_buf;

	sv.num_edicts = svs.maxclients + 1;

	for (i = 0; i < svs.maxclients; i++)
	{
		svs.clients[i].edict = EDICT_NUM(i + 1);
	}

	sv.state = ss_loading;
	sv.paused = false;
	sv.time = 1.0;

	strcpy(sv.name, server);
	sprintf(sv.modelname, "maps/%s.bsp", server);

	sv.worldmodel = Mod_ForName(sv.modelname, false);
	if (!sv.worldmodel)
	{
		Con_Printf("Couldn't spawn server %s\n", sv.modelname);
		sv.active = false;
		return;
	}

	sv.models[1] = sv.worldmodel;
	SV_ClearWorld();

	sv.sound_precache[0] = pr_strings;
	sv.model_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;

	for (i = 1; i < sv.worldmodel->numsubmodels; i++)
	{
		sv.model_precache[i + 1] = localmodels[i];
		sv.models[i + 1] = Mod_ForName(localmodels[i], false);
	}

	ent = EDICT_NUM(0);
	{
		memset((byte *)ent + 120, 0, 4 * ((dprograms_t *)progs)->entityfields);

		ent->free = false;
		ent->v.solid = SOLID_BSP;
		ent->v.model = (string_t)((byte *)sv.worldmodel - (byte *)pr_strings);
		ent->v.modelindex = 1;
		ent->v.movetype = MOVETYPE_PUSH;

		if (coop.value == 0.0f)
			pr_global_struct->deathmatch = deathmatch.value;
		else
			pr_global_struct->coop = coop.value;
		pr_global_struct->mapname = sv.name - pr_strings;
		pr_global_struct->startspot = sv.startspot - pr_strings;
	}
	pr_global_struct->serverflags = (float)svs.serverflags;

	ED_LoadFromFile(sv.worldmodel->entities);

	sv.active = true;
	sv.state = ss_active;

	host_frametime = 0.1;
	SV_Physics();
	SV_Physics();

	SV_CreateBaseline();

	for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
	{
		if (host_client->active)
			SV_SendServerinfo(host_client);
	}

	Con_DPrintf("Server spawned.\n");
}

void SV_ClearServerState(server_t *sv_state)
{
	if (sv_state->areanode_data)
	{
		Z_Free((void *)(sv_state->areanode_data));
		sv_state->areanode_count = 0;
		sv_state->areanode_data = 0;
	}
}

int SV_InitEntityList(void)
{
	int *entity_ptr;

	memset(sv_areaentities, 0, sizeof(sv_areaentities));

	entity_ptr = &sv_areaentities[2];
	do
	{

		*entity_ptr = (int)(entity_ptr + 78);
		entity_ptr += 80;
	}
	while (entity_ptr < &sv_areaentities[SV_AREAENTITIES_COUNT]);

	sv_areaentities[SV_AREAENTITIES_COUNT] = 0;
	sv_entity_list_head = 0;
	sv_entity_list_tail = (int)sv_areaentities;

	return 0;
}

int SV_ClientPrintf(char *format, ...)
{
	char buffer[1024];
	va_list va;

	va_start(va, format);
	vsprintf(buffer, format, va);

	MSG_WriteByte(&host_client->message, svc_print);
	MSG_WriteString(&host_client->message, buffer);

	return 0;
}

int SV_ClientCommand(char *format, ...)
{
	char buffer[1024];
	va_list va;

	va_start(va, format);
	vsprintf(buffer, format, va);

	MSG_WriteByte(&host_client->message, svc_stufftext);
	MSG_WriteString(&host_client->message, buffer);

	return 0;
}

qboolean SV_CheckClientTimeout(int client_idx)
{
	float timeout;
	int entity_num;

	timeout = *(float *)(client_idx + 352);

	if (timeout <= 0.0 || host_frametime + sv.time < timeout)
		return true;

	if (sv.time > timeout)
		timeout = sv.time;

	*(float *)(client_idx + 352) = 0.0;

	pr_global_struct->time = timeout;

	entity_num = (client_idx - (int)sv.edicts) / pr_edict_size;
	pr_global_struct->self = entity_num;
	pr_global_struct->other = 0;

	ED_Free((edict_t *)client_idx);

	return (*(int *)client_idx == 0);
}

int SV_ReadClientMove(usercmd_t *cmd)
{
	float cmd_time;
	float ping;
	float angle0, angle1, angle2;
	int packed_short;
	int byte0, byte1;
	byte *edict_bytes;

	cmd_time = MSG_ReadFloat();
	ping = (float)(sv.time - cmd_time);

	host_client->ping_times[host_client->ping_time_index & 0xF] = ping;
	host_client->ping_time_index++;

	angle0 = MSG_ReadAngle();
	angle1 = MSG_ReadAngle();
	angle2 = MSG_ReadAngle();

	edict_bytes = (byte *)host_client->edict;
	*(float *)(edict_bytes + 472) = angle0;
	*(float *)(edict_bytes + 476) = angle1;
	*(float *)(edict_bytes + 480) = ping;

	cmd->forwardmove = (float)MSG_ReadShort();
	cmd->sidemove = (float)MSG_ReadShort();
	cmd->upmove = (float)MSG_ReadShort();

	packed_short = MSG_ReadShort();
	byte0 = MSG_ReadByte();
	byte1 = MSG_ReadByte();

	*(int *)(edict_bytes + 460) = packed_short;
	if ((float)byte0 != 0.0f)
		*(float *)(edict_bytes + 464) = (float)byte0;
	*(float *)(edict_bytes + 272) = (float)byte1;

	(void)angle2;
	return byte1;
}

int SV_ReadClientMessage(void)
{
	int msg_result;
	int cmd;
	char *cmd_string;
	int cmd_type;

	while (1)
	{

		msg_result = NET_GetMessage(host_client->netconnection);

		if (msg_result == -1)
		{
			Con_Printf("SV: read client message error\n");
			return 0;
		}

		if (!msg_result)
			return 1;

		MSG_BeginReading();

		while (host_client->active)
		{
			if (msg_badread)
			{
				Con_Printf("SV: read client message - frame complete\n");
				return 0;
			}

			cmd = MSG_ReadChar();

			if (cmd == -1)
				break;

			if (cmd == clc_nop)
				continue;

			switch (cmd)
			{
				case clc_disconnect:
					return 0;

				case clc_move:

					SV_ReadClientMove(&host_client->cmd);
					break;

				case clc_stringcmd:

					cmd_string = MSG_ReadString();

					cmd_type = (host_client->privileged == 0) ? 0 : 2;

					if (!Q_strncasecmp(cmd_string, "status", 6) ||
						!Q_strncasecmp(cmd_string, "sv", 3) ||
						!Q_strncasecmp(cmd_string, "notarget", 8) ||
						!Q_strncasecmp(cmd_string, "fly", 3) ||
						!Q_strncasecmp(cmd_string, "name", 4) ||
						!Q_strncasecmp(cmd_string, "noclip", 6) ||
						!Q_strncasecmp(cmd_string, "god", 3) ||
						!Q_strncasecmp(cmd_string, "say_team", 8) ||
						!Q_strncasecmp(cmd_string, "tell", 4) ||
						!Q_strncasecmp(cmd_string, "color", 5) ||
						!Q_strncasecmp(cmd_string, "kill", 4) ||
						!Q_strncasecmp(cmd_string, "pause", 5) ||
						!Q_strncasecmp(cmd_string, "spawn", 5) ||
						!Q_strncasecmp(cmd_string, "begin", 5) ||
						!Q_strncasecmp(cmd_string, "prespawn", 8) ||
						!Q_strncasecmp(cmd_string, "kick", 4) ||
						!Q_strncasecmp(cmd_string, "ping", 4) ||
						!Q_strncasecmp(cmd_string, "give", 4) ||
						!Q_strncasecmp(cmd_string, "ban", 3))
					{
						cmd_type = 1;
					}

					if (cmd_type == 2)
					{
						Cmd_ExecuteString(cmd_string, src_client);
					}
					else if (cmd_type == 1)
					{
						Cmd_ExecuteString(cmd_string, src_client);
					}
					else
					{
						Con_Printf("%s tried to %s\n", host_client->name, cmd_string);
					}
					break;

				default:
					Con_Printf("SV: read client message - unknown command\n");
					return 0;
			}
		}
	}

	return 0;
}

void SV_RunClients(void)
{
	int i;

	for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		sv_player = host_client->edict;

		if (SV_ReadClientMessage())
		{

			if (host_client->spawned)
			{

				if (!sv.paused && (svs.maxclients > 1 || !key_dest))
					SV_ClientThink();
			}
			else
			{

				memset(&host_client->cmd, 0, sizeof(usercmd_t));
			}
		}
		else
		{

			SV_DropClient(false);
		}
	}
}

int SV_BroadcastPrintf(char *format, ...)
{
	int i;
	int result;
	server_client_t *client;
	char buffer[1024];
	va_list va;

	va_start(va, format);
	result = vsprintf(buffer, format, va);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{

		if (client->active && client->spawned)
		{

			MSG_WriteByte(&client->message, svc_print);
			MSG_WriteString(&client->message, buffer);
		}
	}

	return result;
}

void SV_DropClient(qboolean crash)
{
	int client_index;
	int saved_self;
	int i;
	server_client_t *client;

	if (!crash)
	{

		if (NET_CanSendMessage(host_client->netconnection))
		{
			MSG_WriteByte(&host_client->message, svc_disconnect);
			NET_SendMessage(host_client->netconnection, &host_client->message);
		}

		if (host_client->edict && host_client->spawned)
		{

			saved_self = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);

			DispatchEntityCallback(0);

			pr_global_struct->self = saved_self;
		}

		Con_Printf("Client %s removed\n", host_client->name);
	}

	NET_Close(host_client->netconnection);

	net_activeconnections--;

	host_client->netconnection = NULL;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	host_client->active = false;

	client_index = host_client - svs.clients;

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (client->active)
		{

			MSG_WriteByte(&client->message, svc_updatename);
			MSG_WriteByte(&client->message, client_index);
			MSG_WriteString(&client->message, "");

			MSG_WriteByte(&client->message, svc_updatefrags);
			MSG_WriteByte(&client->message, client_index);
			MSG_WriteShort(&client->message, 0);

			MSG_WriteByte(&client->message, svc_updatecolors);
			MSG_WriteByte(&client->message, client_index);
			MSG_WriteByte(&client->message, 0);
		}
	}
}

