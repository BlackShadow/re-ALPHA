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
// sv_main.c -- server main program

#include "quakedef.h"
#include "eiface.h"

// free list of 320 byte blocks set up by SV_InitEntityList
#define AREALIST_BLOCKS		350
#define AREALIST_BLOCKSIZE	80		// in ints
#define AREALIST_NEXT		2		// int holding the address of the next block
#define AREALIST_SIZE		(AREALIST_BLOCKS * AREALIST_BLOCKSIZE)

#define MAX_UPDATE_SIZE		16		// room needed for one more entity update or sound
#define ORIGIN_EPSILON		0.1f	// smaller origin changes are not sent
#define NOP_INTERVAL		5		// seconds between keepalives while signing on
#define RECONNECT_WAIT		5		// seconds NET_SendToAll may block
#define MAX_PRINT_STRING	1024	// SV_ClientPrintf and friends
#define SERVERINFO_BUFSIZE	2048

static int		sv_arealist[AREALIST_SIZE + 1];
static int		sv_arealist_head;
static int		sv_arealist_tail;

char			localmodels[MAX_MODELS][5];		// inline model names for precache

byte			fatpvs[MAX_MAP_LEAFS / 8];
int				fatbytes;

cvar_t	sv_maxvelocity = {"sv_maxvelocity", "2000"};
cvar_t	sv_gravity = {"sv_gravity", "800", false, true};
cvar_t	sv_friction = {"sv_friction", "4", false, true};
cvar_t	sv_edgefriction = {"edgefriction", "2"};
cvar_t	sv_stopspeed = {"sv_stopspeed", "100"};
cvar_t	sv_maxspeed = {"sv_maxspeed", "320", false, true};
cvar_t	sv_accelerate = {"sv_accelerate", "10"};
cvar_t	sv_idealpitchscale = {"sv_idealpitchscale", "0.8"};
cvar_t	sv_aim = {"sv_aim", "1"};
cvar_t	sv_nostep = {"sv_nostep", "0"};

//============================================================================

/*
===============
SV_Init
===============
*/
void SV_Init(void)
{
	int		i;

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

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off. (max 4 attenuation)
==================
*/
void SV_StartSound(edict_t *entity, int channel, const char *sample, int volume, float attenuation)
{
	int		sound_num;
	int		field_mask;
	int		i;
	int		ent;
	vec3_t	origin;

	if (volume < 0 || volume > 255)
		Sys_Error("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error("SV_StartSound: channel = %i", channel);

	if (sv.datagram.cursize > MAX_DATAGRAM - MAX_UPDATE_SIZE)
		return;

	// find precache number for sound
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

	// directed messages go only to the entity the are targeted on
	MSG_WriteByte(&sv.datagram, svc_sound);
	MSG_WriteByte(&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte(&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte(&sv.datagram, (int)(attenuation * 64));
	MSG_WriteShort(&sv.datagram, channel);
	MSG_WriteByte(&sv.datagram, sound_num);

	for (i = 0; i < 3; i++)
		origin[i] = entity->v.origin[i] + 0.5f * (entity->v.mins[i] + entity->v.maxs[i]);

	MSG_WriteCoord(&sv.datagram, origin[0]);
	MSG_WriteCoord(&sv.datagram, origin[1]);
	MSG_WriteCoord(&sv.datagram, origin[2]);
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
static void SV_SendServerinfo(server_client_t *client)
{
	char	**s;
	char	message[SERVERINFO_BUFSIZE];

	MSG_WriteByte(&client->message, svc_print);
	sprintf(message, "VERSION %4.2f SERVER (%i CRC)\n", VERSION, pr_crc);
	MSG_WriteString(&client->message, message);

	MSG_WriteByte(&client->message, svc_serverinfo);
	MSG_WriteLong(&client->message, PROTOCOL_VERSION);
	MSG_WriteByte(&client->message, svs.maxclients);

	if (deathmatch.value != 0 || coop.value == 0)
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

	// send music
	MSG_WriteByte(&client->message, svc_cdtrack);
	MSG_WriteByte(&client->message, sv.edicts->v.sounds);
	MSG_WriteByte(&client->message, sv.edicts->v.sounds);

	// set view
	MSG_WriteByte(&client->message, svc_setview);
	MSG_WriteShort(&client->message, NUM_FOR_EDICT(client->edict));

	MSG_WriteByte(&client->message, svc_signonnum);
	MSG_WriteByte(&client->message, 1);

	client->sendsignon = true;
	client->spawned = false;	// need prespawn, spawn, etc
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
static void SV_ConnectClient(int clientnum)
{
	server_client_t		*client;
	edict_t				*ent;
	int					edictnum;
	struct qsocket_s	*netconnection;
	float				spawn_parms[NUM_SPAWN_PARMS];
	qboolean			has_parms;
	int					i;

	client = svs.clients + clientnum;

	Con_DPrintf("Client %s connected\n", NET_QSocketGetString(client->netconnection));

	// set up the client_t
	netconnection = client->netconnection;

	edictnum = clientnum + 1;
	ent = EDICT_NUM(edictnum);

	memcpy(spawn_parms, client->spawn_parms, sizeof(spawn_parms));

	memset(client, 0, sizeof(*client));
	client->netconnection = netconnection;
	client->edict = ent;

	strcpy(client->name, "unconnected");
	client->active = true;
	client->spawned = false;
	client->edict = ent;
	client->message.data = client->msgbuf;
	client->message.allowoverflow = true;	// we can catch it
	client->privileged = false;
	client->message.maxsize = sizeof(client->msgbuf);

	if (sv.loadgame)
	{
		memcpy(client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	}
	else
	{
		// keep the parms a changelevel left, else ask the game for new ones
		has_parms = false;
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
			DispatchEntityCallback(ENTITYFUNC_SETNEWPARMS);
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

/*
===================
SV_CheckForNewClients
===================
*/
void SV_CheckForNewClients(void)
{
	struct qsocket_s	*ret;
	int					i;

	// check for new connections
	while (1)
	{
		ret = NET_CheckNewConnections();
		if (!ret)
			break;

		// init a new client structure
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

/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
==================
SV_ClearDatagram
==================
*/
void SV_ClearDatagram(void)
{
	SZ_Clear(&sv.datagram);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

/*
=============
SV_AddToFatPVS
=============
*/
static void SV_AddToFatPVS(vec3_t org, mnode_t *node)
{
	int			i;
	byte		*pvs;
	mplane_t	*plane;
	float		d;

	while (1)
	{
		// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				pvs = Mod_LeafPVS((mleaf_t *)node, sv.worldmodel);
				for (i = 0; i < fatbytes; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct(org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else
		{
			// go down both
			SV_AddToFatPVS(org, node->children[0]);
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
static byte *SV_FatPVS(vec3_t org)
{
	fatbytes = (sv.worldmodel->numleafs + 31) >> 3;
	Q_memset(fatpvs, 0, fatbytes);
	SV_AddToFatPVS(org, sv.worldmodel->nodes);
	return fatpvs;
}

//=============================================================================

/*
=============
SV_WriteEntitiesToClient
=============
*/
static void SV_WriteEntitiesToClient(edict_t *clent, sizebuf_t *msg)
{
	int		e, i;
	int		bits;
	byte	*pvs;
	vec3_t	org;
	float	*origin;
	float	miss;
	edict_t	*ent;
	int		modelindex;
	model_t	*model;

	// find the client's PVS
	VectorAdd(clent->v.view_ofs, clent->v.origin, org);
	pvs = SV_FatPVS(org);

	// send over all entities (except the world) that touch the pvs
	ent = NEXT_EDICT(sv_edicts);
	for (e = 1; e < *sv_num_edicts; e++, ent = NEXT_EDICT(ent))
	{
		// clent is ALWAYS sent
		if (ent != clent && ent->v.effects == EF_NODRAW)
			continue;

		if (ent != clent)
		{
			// ignore ents without visible models
			if (ent->v.modelindex == 0 || !pr_strings[ent->v.model])
				continue;

			for (i = 0; i < ent->num_leafs; i++)
			{
				if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i] & 7)))
					break;
			}

			if (i == ent->num_leafs)
				continue;	// not visible
		}

		if (msg->maxsize - msg->cursize < 16)
		{
			Con_Printf("packet overflow\n");
			return;
		}

		// send an update
		bits = 0;

		origin = ent->v.origin;
		for (i = 0; i < 3; i++)
		{
			miss = origin[i] - ent->baseline.origin[i];
			if (miss < -ORIGIN_EPSILON || miss > ORIGIN_EPSILON)
				bits |= U_ORIGIN1 << i;
		}

		if (ent->baseline.angles[0] != ent->v.angles[0])
			bits |= U_ANGLE1;
		if (ent->baseline.angles[1] != ent->v.angles[1])
			bits |= U_ANGLE2;
		if (ent->baseline.angles[2] != ent->v.angles[2])
			bits |= U_ANGLE3;

		if (ent->v.movetype == MOVETYPE_STEP)
			bits |= U_NOLERP;	// don't mess up the step animation

		if (ent->baseline.colormap != ent->v.colormap)
			bits |= U_COLORMAP;
		if (ent->baseline.skin != ent->v.skin)
			bits |= U_SKIN;
		if (ent->baseline.frame != ent->v.frame)
			bits |= U_FRAME;
		if (ent->baseline.effects != ent->v.effects)
			bits |= U_EFFECTS;
		if (ent->baseline.modelindex != ent->v.modelindex)
			bits |= U_MODEL;

		// studio models always send their sequence
		if (ent->v.animtime != 0)
		{
			bits |= U_SEQUENCE;
		}
		else
		{
			modelindex = (int)ent->v.modelindex;
			if (modelindex > 0 && modelindex < MAX_MODELS)
			{
				model = sv.models[modelindex];
				if (model && model->type == mod_studio)
					bits |= U_SEQUENCE;
			}
		}

		if (ent->v.framerate != 1)
			bits |= U_FRAMERATE;
		if (ent->v.body != 0)
			bits |= U_BODY;
		if (ent->v.controller)
			bits |= U_CONTROLLER;
		if (ent->v.blending)
			bits |= U_BLENDING;

		if (ent->baseline.rendermode != ent->v.rendermode
			|| ent->baseline.renderamt != ent->v.renderamt
			|| ent->baseline.renderfx != ent->v.renderfx
			|| ent->baseline.rendercolor[0] != ent->v.rendercolor[0]
			|| ent->baseline.rendercolor[1] != ent->v.rendercolor[1]
			|| ent->baseline.rendercolor[2] != ent->v.rendercolor[2])
		{
			bits |= U_RENDER;
		}

		if (ent->v.animtime != 0
			&& ent->v.velocity[0] == 0
			&& ent->v.velocity[1] == 0
			&& ent->v.velocity[2] == 0)
		{
			bits |= U_STEP;
		}

		if (e >= 256)
			bits |= U_LONGENTITY;

		if (bits >= 256)
			bits |= U_MOREBITS;
		if (bits >= 65536)
			bits |= U_MOREBITS2;

		// write the message
		MSG_WriteByte(msg, (bits & 255) | U_SIGNAL);

		if (bits & U_MOREBITS)
			MSG_WriteByte(msg, (bits >> 8) & 255);
		if (bits & U_MOREBITS2)
			MSG_WriteByte(msg, (bits >> 16) & 255);

		if (bits & U_LONGENTITY)
			MSG_WriteShort(msg, e);
		else
			MSG_WriteByte(msg, e);

		if (bits & U_MODEL)
			MSG_WriteByte(msg, (int)ent->v.modelindex);
		if (bits & U_FRAME)
			MSG_WriteShort(msg, (int)(ent->v.frame * 256));
		if (bits & U_COLORMAP)
			MSG_WriteByte(msg, (int)ent->v.colormap);
		if (bits & U_SKIN)
			MSG_WriteByte(msg, (int)ent->v.skin);
		if (bits & U_EFFECTS)
			MSG_WriteByte(msg, (int)ent->v.effects);
		if (bits & U_ORIGIN1)
			MSG_WriteCoord(msg, ent->v.origin[0]);
		if (bits & U_ANGLE1)
			MSG_WriteAngle(msg, ent->v.angles[0]);
		if (bits & U_ORIGIN2)
			MSG_WriteCoord(msg, ent->v.origin[1]);
		if (bits & U_ANGLE2)
			MSG_WriteAngle(msg, ent->v.angles[1]);
		if (bits & U_ORIGIN3)
			MSG_WriteCoord(msg, ent->v.origin[2]);
		if (bits & U_ANGLE3)
			MSG_WriteAngle(msg, ent->v.angles[2]);

		if (bits & U_SEQUENCE)
		{
			MSG_WriteByte(msg, ent->v.sequence);
			if (ent->v.animtime != 0)
				MSG_WriteByte(msg, (int)(ent->v.animtime * 100));
			else
				MSG_WriteByte(msg, (int)(sv.time * 100));
		}
		if (bits & U_FRAMERATE)
			MSG_WriteChar(msg, (int)(ent->v.framerate * 64));
		if (bits & U_CONTROLLER)
			MSG_WriteLong(msg, ent->v.controller);
		if (bits & U_BLENDING)
			MSG_WriteShort(msg, (short)ent->v.blending);
		if (bits & U_BODY)
			MSG_WriteByte(msg, (int)ent->v.body);

		if (bits & U_RENDER)
		{
			MSG_WriteByte(msg, (int)ent->v.rendermode);
			MSG_WriteByte(msg, (int)ent->v.renderamt);
			MSG_WriteByte(msg, (int)ent->v.rendercolor[0]);
			MSG_WriteByte(msg, (int)ent->v.rendercolor[1]);
			MSG_WriteByte(msg, (int)ent->v.rendercolor[2]);
			MSG_WriteByte(msg, (int)ent->v.renderfx);
		}
	}
}

/*
=============
SV_CleanupEnts
=============
*/
static void SV_CleanupEnts(void)
{
	int		e;
	edict_t	*ent;

	for (e = 1; e < sv.num_edicts; e++)
	{
		ent = EDICT_NUM(e);
		ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
	}
}

/*
==================
SV_WriteClientdataToMessage
==================
*/
void SV_WriteClientdataToMessage(edict_t *ent, sizebuf_t *msg)
{
	int		bits;
	int		i;
	edict_t	*other;
	int		items;

	// send a damage message
	if (ent->v.dmg_take != 0 || ent->v.dmg_save != 0)
	{
		other = (edict_t *)((byte *)sv_edicts + ent->v.dmg_inflictor);
		MSG_WriteByte(msg, svc_damage);
		MSG_WriteByte(msg, (int)ent->v.dmg_save);
		MSG_WriteByte(msg, (int)ent->v.dmg_take);
		for (i = 0; i < 3; i++)
			MSG_WriteCoord(msg, other->v.origin[i] + 0.5f * (other->v.mins[i] + other->v.maxs[i]));

		ent->v.dmg_take = 0;
		ent->v.dmg_save = 0;
	}

	// send the current viewpos offset from the view entity
	SV_SetIdealPitch();		// how much to look up / down ideally

	// a fixangle might get lost in a dropped packet.  Oh well.
	if (ent->v.fixangle != 0)
	{
		MSG_WriteByte(msg, svc_setangle);
		for (i = 0; i < 3; i++)
			MSG_WriteAngle(msg, ent->v.angles[i]);
		ent->v.fixangle = 0;
	}

	bits = 0;

	if (ent->v.view_ofs[2] != DEFAULT_VIEWHEIGHT)
		bits |= SU_VIEWHEIGHT;

	// items2 go in the high bits
	items = ent->v.items | (ent->v.items2 << 23);

	if (ent->v.idealpitch != 0)
		bits |= SU_IDEALPITCH;

	bits |= SU_ITEMS;

	if (ent->v.weapons)
		bits |= SU_WEAPONS;

	if ((int)ent->v.flags & FL_ONGROUND)
		bits |= SU_ONGROUND;

	if (ent->v.waterlevel >= 2)
		bits |= SU_INWATER;
	if (ent->v.waterlevel >= 3)
		bits |= SU_UNDERWATER;

	for (i = 0; i < 3; i++)
	{
		if (ent->v.punchangle[i] != 0)
			bits |= (SU_PUNCH1 << i);
		if (ent->v.velocity[i] != 0)
			bits |= (SU_VELOCITY1 << i);
	}

	if (ent->v.weaponframe != 0)
		bits |= SU_WEAPONFRAME;

	if (ent->v.armorvalue != 0)
		bits |= SU_ARMOR;

	bits |= SU_WEAPON;

	// send the data
	MSG_WriteByte(msg, svc_clientdata);
	MSG_WriteShort(msg, bits);

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteChar(msg, (int)ent->v.view_ofs[2]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteChar(msg, (int)ent->v.idealpitch);

	for (i = 0; i < 3; i++)
	{
		if (bits & (SU_PUNCH1 << i))
			MSG_WriteChar(msg, (int)ent->v.punchangle[i]);
		if (bits & (SU_VELOCITY1 << i))
			MSG_WriteChar(msg, (int)(ent->v.velocity[i] / 16));
	}

	// [always sent]	if (bits & SU_ITEMS)
	MSG_WriteLong(msg, items);
	MSG_WriteLong(msg, ent->v.items2);

	if (bits & SU_WEAPONS)
		MSG_WriteLong(msg, ent->v.weapons);
	if (bits & SU_WEAPONFRAME)
		MSG_WriteByte(msg, (int)ent->v.weaponframe);
	if (bits & SU_ARMOR)
		MSG_WriteByte(msg, (int)ent->v.armorvalue);
	if (bits & SU_WEAPON)
		MSG_WriteByte(msg, SV_ModelIndex(pr_strings + ent->v.weaponmodel));

	MSG_WriteShort(msg, (int)ent->v.health);
	MSG_WriteByte(msg, (int)ent->v.currentammo);
	MSG_WriteByte(msg, ent->v.ammo_1);
	MSG_WriteByte(msg, ent->v.ammo_2);
	MSG_WriteByte(msg, ent->v.ammo_3);
	MSG_WriteByte(msg, ent->v.ammo_4);
	MSG_WriteLong(msg, ent->v.weapon);
}

/*
=======================
SV_SendClientDatagram
=======================
*/
static qboolean SV_SendClientDatagram(server_client_t *client)
{
	byte		buf[MAX_DATAGRAM];
	sizebuf_t	msg;

	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	MSG_WriteByte(&msg, svc_time);
	MSG_WriteFloat(&msg, sv.time);

	// add the client specific data to the datagram
	SV_WriteClientdataToMessage(client->edict, &msg);

	SV_WriteEntitiesToClient(client->edict, &msg);

	// copy the server datagram if there is space
	if (msg.cursize + sv.datagram.cursize < msg.maxsize)
		SZ_Write(&msg, sv.datagram.data, sv.datagram.cursize);

	// send the datagram
	if (NET_SendUnreliableMessage(client->netconnection, &msg) == -1)
	{
		SV_DropClient(true);	// if the message couldn't send, kick off
		return false;
	}

	return true;
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
static void SV_UpdateToReliableMessages(void)
{
	int				i, j;
	server_client_t	*client;

	// check for changes to be sent over the reliable streams
	for (i = 0; i < svs.maxclients; i++)
	{
		host_client = svs.clients + i;

		if ((int)host_client->edict->v.frags != host_client->old_frags)
		{
			for (j = 0; j < svs.maxclients; j++)
			{
				client = svs.clients + j;
				if (!client->active)
					continue;

				MSG_WriteByte(&client->message, svc_updatefrags);
				MSG_WriteByte(&client->message, i);
				MSG_WriteShort(&client->message, (int)host_client->edict->v.frags);
			}

			host_client->old_frags = (int)host_client->edict->v.frags;
		}
	}

	for (i = 0; i < svs.maxclients; i++)
	{
		client = svs.clients + i;
		if (!client->active)
			continue;

		SZ_Write(&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
	}

	SZ_Clear(&sv.reliable_datagram);
}

/*
=======================
SV_SendNop

Send a nop message without trashing or sending the accumulated client
message buffer
=======================
*/
static void SV_SendNop(server_client_t *client)
{
	sizebuf_t	msg;
	byte		buf[4];

	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	MSG_WriteChar(&msg, svc_nop);

	if (NET_SendUnreliableMessage(client->netconnection, &msg) == -1)
		SV_DropClient(true);	// if the message couldn't send, kick off

	client->last_message = realtime;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages(void)
{
	int				i;
	server_client_t	*client;

	// update frags, names, etc
	SV_UpdateToReliableMessages();

	// build individual updates
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
			// the player isn't totally in the game yet
			// send small keepalive messages if too much time has passed
			// send a full message when the next signon stage has been requested
			// some other message data (name changes, etc) may accumulate
			// between signon stages
			if (!client->sendsignon)
			{
				if (realtime - client->last_message > NOP_INTERVAL)
					SV_SendNop(client);
				continue;	// don't send out non-signon messages
			}
		}

		host_client = client;

		// check for an overflowed message.  Should only happen
		// on a very fucked up connection that backs up a lot, then
		// changes level
		if (client->dropasap)
		{
			SV_DropClient(true);
			client->dropasap = false;
		}
		else if (client->message.overflowed || client->message.cursize)
		{
			if (!NET_CanSendMessage(client->netconnection))
				continue;

			if (client->message.overflowed)
			{
				SV_DropClient(false);
			}
			else
			{
				if (NET_SendMessage(client->netconnection, &client->message) == -1)
					SV_DropClient(true);	// if the message couldn't send, kick off

				SZ_Clear(&client->message);
				client->sendsignon = false;
				client->last_message = realtime;
			}
		}
	}

	// clear muzzle flashes
	SV_CleanupEnts();
}

/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex
================
*/
int SV_ModelIndex(char *name)
{
	int		i;

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

/*
================
SV_CreateBaseline
================
*/
static void SV_CreateBaseline(void)
{
	int		i;
	edict_t	*svent;
	int		entnum;

	for (entnum = 0; entnum < *sv_num_edicts; entnum++)
	{
		// get the current server version
		svent = (edict_t *)((byte *)sv_edicts + entnum * pr_edict_size);
		if (svent->free)
			continue;

		if (entnum > svs.maxclients && svent->v.modelindex == 0)
			continue;

		// create entity baseline
		VectorCopy(svent->v.origin, svent->baseline.origin);
		VectorCopy(svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = (int)svent->v.frame;
		svent->baseline.skin = (int)svent->v.skin;

		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("models/doctor.mdl");
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex = SV_ModelIndex(pr_strings + svent->v.model);
		}

		svent->baseline.rendermode = (int)svent->v.rendermode;
		svent->baseline.renderamt = (int)svent->v.renderamt;
		svent->baseline.rendercolor[0] = (int)svent->v.rendercolor[0];
		svent->baseline.rendercolor[1] = (int)svent->v.rendercolor[1];
		svent->baseline.rendercolor[2] = (int)svent->v.rendercolor[2];
		svent->baseline.renderfx = (int)svent->v.renderfx;

		// add to the message
		MSG_WriteByte(&sv.signon, svc_spawnbaseline);
		MSG_WriteShort(&sv.signon, entnum);

		MSG_WriteByte(&sv.signon, svent->baseline.modelindex);
		MSG_WriteByte(&sv.signon, svent->baseline.sequence);
		MSG_WriteByte(&sv.signon, svent->baseline.frame);
		MSG_WriteByte(&sv.signon, svent->baseline.colormap);
		MSG_WriteByte(&sv.signon, svent->baseline.skin);
		for (i = 0; i < 3; i++)
		{
			MSG_WriteCoord(&sv.signon, svent->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->baseline.angles[i]);
		}

		MSG_WriteByte(&sv.signon, (int)svent->v.rendermode);
		if (svent->v.rendermode != 0)
		{
			MSG_WriteByte(&sv.signon, (int)svent->v.renderamt);
			MSG_WriteByte(&sv.signon, (int)svent->v.rendercolor[0]);
			MSG_WriteByte(&sv.signon, (int)svent->v.rendercolor[1]);
			MSG_WriteByte(&sv.signon, (int)svent->v.rendercolor[2]);
			MSG_WriteByte(&sv.signon, (int)svent->v.renderfx);
		}
	}
}

/*
================
SV_SendReconnect

Tell all the clients that the server is changing levels
================
*/
static void SV_SendReconnect(void)
{
	byte		data[128];
	sizebuf_t	msg;

	msg.data = data;
	msg.maxsize = sizeof(data);
	msg.cursize = 0;

	MSG_WriteChar(&msg, svc_stufftext);
	MSG_WriteString(&msg, "reconnect\n");
	NET_SendToAll(&msg, RECONNECT_WAIT);

	if (cls.state != ca_dedicated)
		Cmd_ExecuteString("reconnect\n", src_command);
}

/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms(void)
{
	int				i;
	server_client_t	*client;
	int				token;
	qboolean		keeptoken;

	svs.serverflags = (int)pr_global_struct->serverflags;

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;

		// call the progs to get default spawn parms for the new client
		pr_global_struct->self = EDICT_TO_PROG(client->edict);
		pr_global_struct->time = sv.time;

		// during a game DLL changelevel, parm1 already holds the transition token
		token = *(int *)&pr_global_struct->parm1;
		keeptoken = g_iextdllcount > 0 && svs.changelevel_issued && token != 0;
		if (!keeptoken)
		{
			memcpy(&pr_global_struct->parm1, client->spawn_parms, sizeof(client->spawn_parms));
			if (g_iextdllcount > 0 && *(int *)&pr_global_struct->parm1 == 0)
				DispatchEntityCallback(ENTITYFUNC_SETNEWPARMS);
		}

		DispatchEntityCallback(ENTITYFUNC_SETCHANGEPARMS);
		memcpy(client->spawn_parms, &pr_global_struct->parm1, sizeof(client->spawn_parms));
	}
}

/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
void SV_SpawnServer(char *server, char *startspot)
{
	edict_t	*ent;
	int		i;

	// let's not have any servers with no name
	if (!hostname.string[0])
		Cvar_Set("hostname", "unnamed");

	scr_centertime_off = 0;

	Con_DPrintf("SpawnServer: %s\n", server);
	svs.changelevel_issued = false;	// now safe to issue another

	// tell all connected clients that we are going to a new level
	if (sv.active)
		SV_SendReconnect();

	// make cvars consistant
	if (coop.value != 0)
		Cvar_SetValue("deathmatch", 0);

	current_skill = (int)(skill.value + 0.5f);
	if (current_skill < 0)
		current_skill = 0;
	if (current_skill > 3)
		current_skill = 3;

	Cvar_SetValue("skill", (float)current_skill);

	// set up the new server
	Host_ClearMemory();

	memset(&sv, 0, sizeof(sv));

	strcpy(sv.name, server);
	if (startspot)
		strcpy(sv.startspot, startspot);

	// load progs to get entity field count
	PR_LoadProgs();

	// allocate server memory
	sv.max_edicts = MAX_EDICTS;
	sv.edicts = Hunk_AllocName(sv.max_edicts * pr_edict_size, "edicts");
	sv_max_edicts = sv.max_edicts;
	sv_edicts = sv.edicts;

	sv.datagram.cursize = 0;
	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.signon.cursize = 0;
	sv.signon.maxsize = sizeof(sv.signon_buf);
	sv.datagram.data = sv.datagram_buf;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;
	sv.signon.data = sv.signon_buf;

	// leave slots at start for clients only
	sv.num_edicts = svs.maxclients + 1;
	for (i = 0; i < svs.maxclients; i++)
		svs.clients[i].edict = EDICT_NUM(i + 1);

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

	// clear world interaction links
	SV_ClearWorld();

	sv.sound_precache[0] = pr_strings;

	sv.model_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;
	for (i = 1; i < sv.worldmodel->numsubmodels; i++)
	{
		sv.model_precache[1 + i] = localmodels[i];
		sv.models[i + 1] = Mod_ForName(localmodels[i], false);
	}

	// load the rest of the entities
	ent = EDICT_NUM(0);
	memset(&ent->v, 0, ((dprograms_t *)progs)->entityfields * 4);
	ent->free = false;
	ent->v.solid = SOLID_BSP;
	ent->v.model = sv.worldmodel->name - pr_strings;
	ent->v.modelindex = 1;		// world model
	ent->v.movetype = MOVETYPE_PUSH;

	if (coop.value == 0)
		pr_global_struct->deathmatch = deathmatch.value;
	else
		pr_global_struct->coop = coop.value;

	pr_global_struct->mapname = sv.name - pr_strings;
	pr_global_struct->startspot = sv.startspot - pr_strings;

	// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;

	ED_LoadFromFile(sv.worldmodel->entities);

	sv.active = true;

	// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

	// run two frames to allow everything to settle
	host_frametime = 0.1;
	SV_Physics();
	SV_Physics();

	// create a baseline for more efficient communications
	SV_CreateBaseline();

	// send serverinfo to all connected clients
	for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
	{
		if (host_client->active)
			SV_SendServerinfo(host_client);
	}

	Con_DPrintf("Server spawned.\n");
}

/*
================
SV_ClearServerState
================
*/
void SV_ClearServerState(server_t *state)
{
	if (state->areanode_data)
	{
		Z_Free(state->areanode_data);
		state->areanode_count = 0;
		state->areanode_data = NULL;
	}
}

/*
================
SV_InitEntityList

Links the blocks of sv_arealist into a free list.  Not called.
================
*/
void SV_InitEntityList(void)
{
	int		*next;

	memset(sv_arealist, 0, sizeof(sv_arealist));

	next = &sv_arealist[AREALIST_NEXT];
	do
	{
		*next = (int)(next - AREALIST_NEXT + AREALIST_BLOCKSIZE);
		next += AREALIST_BLOCKSIZE;
	} while (next < &sv_arealist[AREALIST_SIZE]);

	sv_arealist[AREALIST_SIZE] = 0;
	sv_arealist_head = 0;
	sv_arealist_tail = (int)sv_arealist;
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf(char *fmt, ...)
{
	va_list		argptr;
	char		string[MAX_PRINT_STRING];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&host_client->message, svc_print);
	MSG_WriteString(&host_client->message, string);
}

/*
=================
SV_ClientCommand

Send text to be executed in the client's console
=================
*/
void SV_ClientCommand(char *fmt, ...)
{
	va_list		argptr;
	char		string[MAX_PRINT_STRING];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&host_client->message, svc_stufftext);
	MSG_WriteString(&host_client->message, string);
}

/*
=============
SV_RunRemoveThink

Like SV_RunThink, but the entity is freed when its think time comes.
Not called.
=============
*/
qboolean SV_RunRemoveThink(edict_t *ent)
{
	float	thinktime;

	thinktime = ent->v.nextthink;
	if (thinktime <= 0.0 || host_frametime + sv.time < thinktime)
		return true;

	if (sv.time > thinktime)
		thinktime = sv.time;	// don't let things stay in the past

	ent->v.nextthink = 0;
	pr_global_struct->time = thinktime;
	pr_global_struct->self = ((byte *)ent - (byte *)sv.edicts) / pr_edict_size;
	pr_global_struct->other = 0;

	ED_Free(ent);

	return !ent->free;
}

/*
===================
SV_ReadClientMove
===================
*/
static void SV_ReadClientMove(usercmd_t *move)
{
	edict_t	*ent;
	float	timestamp, ping;
	vec3_t	angle;
	int		buttons, impulse, lightlevel;

	// read ping time
	timestamp = MSG_ReadFloat();
	ping = sv.time - timestamp;
	host_client->ping_times[host_client->num_pings & (NUM_PING_TIMES - 1)] = ping;
	host_client->num_pings++;

	// read current angles
	angle[0] = MSG_ReadAngle();
	angle[1] = MSG_ReadAngle();
	angle[2] = MSG_ReadAngle();

	// the roll slot gets the ping, the roll that was read is dropped
	ent = host_client->edict;
	ent->v.v_angle[0] = angle[0];
	ent->v.v_angle[1] = angle[1];
	ent->v.v_angle[2] = ping;

	// read movement
	move->forwardmove = MSG_ReadShort();
	move->sidemove = MSG_ReadShort();
	move->upmove = MSG_ReadShort();

	// read buttons
	buttons = MSG_ReadShort();
	impulse = MSG_ReadByte();
	lightlevel = MSG_ReadByte();

	ent->v.button = buttons;
	if (impulse)
		ent->v.impulse = impulse;
	ent->v.light_level = lightlevel;
}

/*
===================
SV_ReadClientMessage

Returns false if the client should be killed
===================
*/
static qboolean SV_ReadClientMessage(void)
{
	int		ret;
	int		cmd;
	int		cmdlevel;
	char	*s;

	while (1)
	{
		ret = NET_GetMessage(host_client->netconnection);
		if (ret == -1)
		{
			Con_Printf("SV: read client message error\n");
			return false;
		}
		if (!ret)
			return true;

		MSG_BeginReading();

		while (host_client->active)
		{
			if (msg_badread)
			{
				Con_Printf("SV: read client message - frame complete\n");
				return false;
			}

			cmd = MSG_ReadChar();
			if (cmd == -1)
				break;	// end of message

			switch (cmd)
			{
			case clc_nop:
				break;

			case clc_disconnect:
				return false;

			case clc_move:
				SV_ReadClientMove(&host_client->cmd);
				break;

			case clc_stringcmd:
				s = MSG_ReadString();
				if (host_client->privileged)
					cmdlevel = 2;
				else
					cmdlevel = 0;

				if (!Q_strncasecmp(s, "status", 6)
					|| !Q_strncasecmp(s, "sv", 3)
					|| !Q_strncasecmp(s, "notarget", 8)
					|| !Q_strncasecmp(s, "fly", 3)
					|| !Q_strncasecmp(s, "name", 4)
					|| !Q_strncasecmp(s, "noclip", 6)
					|| !Q_strncasecmp(s, "god", 3)
					|| !Q_strncasecmp(s, "say_team", 8)
					|| !Q_strncasecmp(s, "tell", 4)
					|| !Q_strncasecmp(s, "color", 5)
					|| !Q_strncasecmp(s, "kill", 4)
					|| !Q_strncasecmp(s, "pause", 5)
					|| !Q_strncasecmp(s, "spawn", 5)
					|| !Q_strncasecmp(s, "begin", 5)
					|| !Q_strncasecmp(s, "prespawn", 8)
					|| !Q_strncasecmp(s, "kick", 4)
					|| !Q_strncasecmp(s, "ping", 4)
					|| !Q_strncasecmp(s, "give", 4)
					|| !Q_strncasecmp(s, "ban", 3))
				{
					cmdlevel = 1;
				}

				if (cmdlevel == 2)
					Cmd_ExecuteString(s, src_client);
				else if (cmdlevel == 1)
					Cmd_ExecuteString(s, src_client);
				else
					Con_Printf("%s tried to %s\n", host_client->name, s);
				break;

			default:
				Con_Printf("SV: read client message - unknown command\n");
				return false;
			}
		}
	}
}

/*
==================
SV_RunClients
==================
*/
void SV_RunClients(void)
{
	int		i;

	for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		sv_player = host_client->edict;

		if (!SV_ReadClientMessage())
		{
			SV_DropClient(false);	// client misbehaved...
			continue;
		}

		if (!host_client->spawned)
		{
			// clear client movement until a new packet is received
			memset(&host_client->cmd, 0, sizeof(host_client->cmd));
			continue;
		}

		// always pause in single player if in console or menus
		if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
			SV_ClientThink();
	}
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf(char *fmt, ...)
{
	va_list			argptr;
	char			string[MAX_PRINT_STRING];
	int				i;
	server_client_t	*client;

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (client->active && client->spawned)
		{
			MSG_WriteByte(&client->message, svc_print);
			MSG_WriteString(&client->message, string);
		}
	}
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient(qboolean crash)
{
	int				saveSelf;
	int				i;
	int				clientnum;
	server_client_t	*client;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage(host_client->netconnection))
		{
			MSG_WriteByte(&host_client->message, svc_disconnect);
			NET_SendMessage(host_client->netconnection, &host_client->message);
		}

		if (host_client->edict && host_client->spawned)
		{
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			DispatchEntityCallback(ENTITYFUNC_CLIENTDISCONNECT);
			pr_global_struct->self = saveSelf;
		}

		Con_Printf("Client %s removed\n", host_client->name);
	}

	// break the net connection
	NET_Close(host_client->netconnection);
	net_activeconnections--;
	host_client->netconnection = NULL;

	// free the client (the body stays around)
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	host_client->active = false;

	// send notification to all clients
	clientnum = host_client - svs.clients;
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;

		MSG_WriteByte(&client->message, svc_updatename);
		MSG_WriteByte(&client->message, clientnum);
		MSG_WriteString(&client->message, "");
		MSG_WriteByte(&client->message, svc_updatefrags);
		MSG_WriteByte(&client->message, clientnum);
		MSG_WriteShort(&client->message, 0);
		MSG_WriteByte(&client->message, svc_updatecolors);
		MSG_WriteByte(&client->message, clientnum);
		MSG_WriteByte(&client->message, 0);
	}
}
