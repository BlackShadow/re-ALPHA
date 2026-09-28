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

// cl_parse.c  -- parse a message received from the server

#include "quakedef.h"
#include "nullsubs.h"

int		parsecountmod;
int		parsecounttime;
int		oldparsecountmod;
int		bitcounts[16];	// how often each fast update bit is used

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
	int		field_mask = MSG_ReadByte();
	int		volume = DEFAULT_SOUND_PACKET_VOLUME;
	float	attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
	int		channel, ent;
	int		sound_num;
	vec3_t	pos;

	if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte();

	if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte() / 64.0f;

	channel = MSG_ReadShort();
	ent = channel >> 3;
	channel &= 7;
	if (ent > MAX_EDICTS)
		Host_Error("CL_ParseStartSoundPacket: ent = %i", ent);

	sound_num = MSG_ReadByte();

	pos[0] = MSG_ReadCoord();
	pos[1] = MSG_ReadCoord();
	pos[2] = MSG_ReadCoord();

	S_StartSound(ent, channel, cl_sound_precache[sound_num], pos, volume / 255.0f, attenuation);
}

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
entity_t *CL_EntityNum(int num)
{
	if (num >= cl_num_entities)
	{
		if (num >= MAX_EDICTS)
			Host_Error("CL_EntityNum: %i is an invalid number", num);

		for (; cl_num_entities <= num; cl_num_entities++)
			cl_entities[cl_num_entities].colormap = vid.colormap;
	}

	return &cl_entities[num];
}

/*
=====================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
=====================
*/
void CL_KeepaliveMessage(void)
{
	float			time;
	static float	lastmsg;
	int				ret;
	sizebuf_t		old;
	byte			olddata[MAX_MSGLEN];

	if (sv.active)
		return;		// no need if server is local
	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
	old = net_message;
	memcpy(olddata, net_message.data, net_message.cursize);

	while (1)
	{
		ret = CL_GetMessage();
		if (ret == 0)
			break;		// nothing waiting
		if (ret == 1)
			Host_Error("CL_KeepaliveMessage: CL_GetMessage failed\n");
		if (ret != 2)
			Host_Error("CL_KeepaliveMessage: CL_GetMessage returned %d\n", ret);

		MSG_BeginReading();
		if (MSG_ReadByte() != svc_nop)
			Host_Error("CL_KeepaliveMessage: datagram wasn't a nop\n");
	}

	net_message = old;
	memcpy(net_message.data, olddata, old.cursize);

// check time
	time = Sys_FloatTime();
	if (time - lastmsg >= 5.0f)
	{
		lastmsg = time;

	// write out a nop
		Con_Printf("Client->Server keepalive\n");
		MSG_WriteByte(&cls.message, clc_nop);
		NET_SendMessage(cls.netcon, &cls.message);
		SZ_Clear(&cls.message);
	}
}

/*
==================
CL_ParseServerInfo
==================
*/
void CL_ParseServerInfo(void)
{
	char	*str;
	int		i;
	int		maxclients;
	int		nummodels, numsounds;
	char	model_precache[MAX_MODELS][MAX_QPATH];
	char	sound_precache[MAX_SOUNDS][MAX_QPATH];
	model_t	*mod;

	Con_DPrintf("Serverinfo packet received.\n");

//
// wipe the client_t struct
//
	SCR_BeginLoadingPlaque();
	CL_ClearState();

// parse protocol version number
	i = MSG_ReadLong();
	if (i != PROTOCOL_VERSION)
	{
		Con_Printf("Server returned version %i, not %i", i, PROTOCOL_VERSION);
		return;
	}

// parse maxclients
	maxclients = MSG_ReadByte();
	cl_maxclients = maxclients;
	if (maxclients < 1 || maxclients > MAX_CLIENTS)
	{
		Con_Printf("Bad maxclients (%u) from server\n", cl_maxclients);
		return;
	}
	cl_scores = Hunk_AllocName(maxclients * sizeof(*cl_scores), "scores");

// parse gametype
	cl_gametype = MSG_ReadByte();

// parse signon message
	str = MSG_ReadString();
	strncpy(cl_levelname, str, sizeof(cl_levelname) - 1);

// separate the printfs so the server message can have a color
	Con_Printf("\n");
	Con_Printf("%s\n", str);

//
// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it
//

// precache models
	memset(cl_model_precache, 0, sizeof(cl_model_precache));
	for (nummodels = 1; ; nummodels++)
	{
		str = MSG_ReadString();
		if (!str[0])
			break;
		if (nummodels >= MAX_MODELS)
		{
			Con_Printf("Server sent too many model precaches");
			return;
		}
		strcpy(model_precache[nummodels], str);
		Mod_TouchModel(str);
	}

// precache sounds
	memset(cl_sound_precache, 0, sizeof(cl_sound_precache));
	for (numsounds = 1; ; numsounds++)
	{
		str = MSG_ReadString();
		if (!str[0])
			break;
		if (numsounds >= MAX_SOUNDS)
		{
			Con_Printf("Server sent too many sound precaches");
			return;
		}
		strcpy(sound_precache[numsounds], str);
		S_InsertText(str);
	}

//
// now we try to load everything else until a cache allocation fails
//
	for (i = 1; i < nummodels; i++)
	{
		mod = Mod_ForName(model_precache[i], false);
		if (!mod)
		{
			Con_Printf("Model %s not found\n", model_precache[i]);
			return;
		}
		cl_model_precache[i] = mod;
		CL_KeepaliveMessage();
	}

	for (i = 1; i < numsounds; i++)
	{
		cl_sound_precache[i] = S_PrecacheSound(sound_precache[i]);
		CL_KeepaliveMessage();
	}

// local state
	cl_worldmodel = cl_model_precache[1];
	cl.worldmodel = cl_worldmodel;
	cl_entities[0].model = cl_model_precache[1];

	R_NewMap();

	Hunk_Check();		// make sure nothing is hurt

	noclip_anglehack = false;		// noclip is turned off at start
}

/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked. Other attributes can change without relinking.
==================
*/
void CL_ParseUpdate(int bits)
{
	int			i;
	int			num;
	int			modnum;
	int			colormap;
	entity_t	*ent;
	model_t		*model;
	qboolean	forcelink;

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply();
	}

	if (bits & U_MOREBITS)
		bits |= MSG_ReadByte() << 8;

	if (bits & U_MOREBITS2)
		bits |= MSG_ReadByte() << 16;

	if (bits & U_LONGENTITY)
		num = MSG_ReadShort();
	else
		num = MSG_ReadByte();

	ent = CL_EntityNum(num);

	for (i = 0; i < 16; i++)
	{
		if (bits & (1 << i))
			bitcounts[i]++;
	}

	forcelink = (ent->msgtime != cl_mtime[1]);	// no previous frame to lerp from
	ent->msgtime = cl_mtime[0];

	if (bits & U_MODEL)
	{
		modnum = MSG_ReadByte();
		if (modnum >= MAX_MODELS)
			Host_Error("CL_ParseModel: bad modelindex");
	}
	else
	{
		modnum = ent->baseline.modelindex;
	}

	model = cl_model_precache[modnum];
	if (ent->model != model)
	{
		ent->model = model;
	// automatic animation (torches, etc) can be either all together
	// or randomized
		if (model)
		{
			if (model->synctype == ST_RAND)
				ent->syncbase = (float)(rand() & RAND_MAX) / RAND_MAX;
			else
				ent->syncbase = 0.0f;
		}
		else
		{
			forcelink = true;	// hack to make null model players work
		}

		if (num > 0 && cl_maxclients >= num)
			S_AmbientOff_Null();
	}

	if (bits & U_FRAME)
		ent->frame = MSG_ReadUShort() / 256.0f;
	else
		ent->frame = ent->baseline.frame;

	if (bits & U_COLORMAP)
		colormap = MSG_ReadByte();
	else
		colormap = ent->baseline.colormap;

	if (colormap)
	{
		if (cl_maxclients < colormap)
			Sys_Error("i > cl.maxclients");

		ent->colormap = cl_scores[colormap - 1].translations;
	}
	else
	{
		ent->colormap = vid.colormap;
	}

	if (bits & U_SKIN)
		ent->skinnum = MSG_ReadByte();
	else
		ent->skinnum = ent->baseline.skinnum;

	if (bits & U_EFFECTS)
		ent->effects = MSG_ReadByte();
	else
		ent->effects = ent->baseline.effects;

// shift the known values for interpolation
	VectorCopy(ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy(ent->msg_angles[0], ent->msg_angles[1]);

	if (bits & U_ORIGIN1)
		ent->msg_origins[0][0] = MSG_ReadCoord();
	else
		ent->msg_origins[0][0] = ent->baseline.origin[0];

	if (bits & U_ANGLE1)
		ent->msg_angles[0][0] = MSG_ReadAngle();
	else
		ent->msg_angles[0][0] = ent->baseline.angles[0];

	if (bits & U_ORIGIN2)
		ent->msg_origins[0][1] = MSG_ReadCoord();
	else
		ent->msg_origins[0][1] = ent->baseline.origin[1];

	if (bits & U_ANGLE2)
		ent->msg_angles[0][1] = MSG_ReadAngle();
	else
		ent->msg_angles[0][1] = ent->baseline.angles[1];

	if (bits & U_ORIGIN3)
		ent->msg_origins[0][2] = MSG_ReadCoord();
	else
		ent->msg_origins[0][2] = ent->baseline.origin[2];

	if (bits & U_ANGLE3)
		ent->msg_angles[0][2] = MSG_ReadAngle();
	else
		ent->msg_angles[0][2] = ent->baseline.angles[2];

	if ((bits & U_NOLERP) && !cl_lerpstep.value)
		ent->forcelink = true;

	if (bits & U_SEQUENCE)
	{
		int		sequence;
		float	basetime;
		float	animtime;

		sequence = MSG_ReadByte();

	// the animation time comes in hundredths of a second, wrapped at 256
		basetime = (int)(cl_time * 100.0 / 256.0) * 2.56f;
		animtime = MSG_ReadByte() / 100.0f + basetime;
		while (cl_time < animtime)
			animtime -= 2.56f;

		if (ent->anim_time != animtime || sequence != ent->sequence)
		{
			if (sequence != ent->sequence)
			{
				ent->blend_oldseq = ent->sequence;
				ent->blend_time = animtime;
				ent->sequence = sequence;
			}

			memcpy(ent->latched_controller, ent->controller, sizeof(ent->controller));
			memcpy(ent->latched_blending, ent->blending, sizeof(ent->blending));

			ent->latched_anim_time = ent->anim_time;
			VectorCopy(ent->origin, ent->lerp_origin);
			VectorCopy(ent->angles, ent->lerp_angles);

			ent->anim_time = animtime;
		}
	}
	else
	{
		ent->sequence = ent->baseline.sequence;
		ent->anim_time = cl_time;
	}

	if (bits & U_FRAMERATE)
		ent->framerate = MSG_ReadChar() / 64.0f;
	else
		ent->framerate = 1.0f;

	if (bits & U_CONTROLLER)
	{
		ent->controller[0] = MSG_ReadByte();
		ent->controller[1] = MSG_ReadByte();
		ent->controller[2] = MSG_ReadByte();
		ent->controller[3] = MSG_ReadByte();
	}
	else
	{
		ent->controller[0] = 0;
		ent->controller[1] = 0;
		ent->controller[2] = 0;
		ent->controller[3] = 0;
	}

	if (bits & U_BLENDING)
	{
		ent->blending[0] = MSG_ReadByte();
		ent->blending[1] = MSG_ReadByte();
	}
	else
	{
		ent->blending[0] = 0;
		ent->blending[1] = 0;
	}

	if (bits & U_BODY)
		ent->body = MSG_ReadByte();
	else
		ent->body = 0;

	if (bits & U_RENDER)
	{
		ent->rendermode = MSG_ReadByte();
		ent->renderamt = MSG_ReadByte();
		ent->rendercolor[0] = MSG_ReadByte();
		ent->rendercolor[1] = MSG_ReadByte();
		ent->rendercolor[2] = MSG_ReadByte();
		ent->renderfx = MSG_ReadByte();
	}
	else
	{
		ent->rendermode = ent->baseline.rendermode;
		ent->renderamt = ent->baseline.renderamt;
		ent->rendercolor[0] = ent->baseline.rendercolor[0];
		ent->rendercolor[1] = ent->baseline.rendercolor[1];
		ent->rendercolor[2] = ent->baseline.rendercolor[2];
		ent->renderfx = ent->baseline.renderfx;
	}

	ent->has_lerpdata = MOVETYPE_STEP;
	if (!(bits & U_STEP))
		ent->has_lerpdata = MOVETYPE_NONE;

	if (forcelink)
	{	// didn't have an update last message
		VectorCopy(ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy(ent->msg_origins[0], ent->origin);
		VectorCopy(ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy(ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline(entity_t *ent)
{
	int		i;

	ent->baseline.modelindex = MSG_ReadByte();
	ent->baseline.sequence = MSG_ReadByte();
	ent->baseline.frame = MSG_ReadByte();
	ent->baseline.colormap = MSG_ReadByte();
	ent->baseline.skinnum = MSG_ReadByte();

	for (i = 0; i < 3; i++)
	{
		ent->baseline.origin[i] = MSG_ReadCoord();
		ent->baseline.angles[i] = MSG_ReadAngle();
	}

	ent->baseline.rendermode = MSG_ReadByte();
	if (ent->baseline.rendermode)
	{
		ent->baseline.renderamt = MSG_ReadByte();
		ent->baseline.rendercolor[0] = MSG_ReadByte();
		ent->baseline.rendercolor[1] = MSG_ReadByte();
		ent->baseline.rendercolor[2] = MSG_ReadByte();
		ent->baseline.renderfx = MSG_ReadByte();
	}
}

/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata(int bits)
{
	int		i, j;

	if (bits & SU_VIEWHEIGHT)
		cl_viewheight = MSG_ReadChar();
	else
		cl_viewheight = DEFAULT_VIEWHEIGHT;

	if (bits & SU_IDEALPITCH)
		cl_idealpitch = MSG_ReadChar();
	else
		cl_idealpitch = 0;

	VectorCopy(cl_punchangle, cl_punchangle_old);

	for (i = 0; i < 3; i++)
	{
		if (bits & (SU_PUNCH1 << i))
			cl_punchangle[i] = MSG_ReadChar();
		else
			cl_punchangle[i] = 0;

		if (bits & (SU_VELOCITY1 << i))
			cl_velocity[i] = MSG_ReadChar() * 16;
		else
			cl_velocity[i] = 0;
	}

// [always sent]	if (bits & SU_ITEMS)
	i = MSG_ReadLong();
	if (cl_items != i)
	{	// set flash times
		VID_HandlePause();
		for (j = 0; j < 32; j++)
		{
			if ((i & (1 << j)) && !(cl_items & (1 << j)))
				cl_weaponbits[j] = cl_time;
		}
		cl_items = i;
	}

	i = MSG_ReadLong();
	if (cl_activeweapon != i)
	{
		VID_HandlePause();
		cl_activeweapon = i;
	}

	if (bits & SU_WEAPONS)
		i = MSG_ReadLong();
	if (cl_activeweapon != i)
	{
		VID_HandlePause();
		cl_activeweapon = i;
	}

	cl_onground = (bits & SU_ONGROUND) != 0;
	cl_inwater = (bits & SU_INWATER) != 0;

	r_dowarp = WATERLEVEL_DRY;
	if (cl_inwater)
		r_dowarp = (bits & SU_UNDERWATER) ? WATERLEVEL_HEAD : WATERLEVEL_WAIST;

	i = 0;
	if (bits & SU_WEAPONFRAME)
		i = MSG_ReadByte();
	if (cl_weaponframe != i)
	{
		VID_HandlePause();
		cl_weaponframe = i;
	}

	i = 0;
	if (bits & SU_ARMOR)
		i = MSG_ReadByte();
	if (cl_armorvalue != i)
	{
		VID_HandlePause();
		cl_armorvalue = i;
	}

	i = 0;
	if (bits & SU_WEAPON)
		i = MSG_ReadByte();
	if (cl_weaponmodel != i)
	{
		VID_HandlePause();
		cl_weaponmodel = i;
	}

	i = MSG_ReadShort();
	if (cl_health != i)
	{
		VID_HandlePause();
		cl_health = i;
	}

	i = MSG_ReadByte();
	if (cl_lightlevel != i)
	{
		VID_HandlePause();
		cl_lightlevel = i;
	}

	for (i = 0; i < 4; i++)
	{
		j = MSG_ReadByte();
		if (cl_stats[i] != j)
		{
			VID_HandlePause();
			cl_stats[i] = j;
		}
	}

	i = MSG_ReadLong();
	if (cl_stats[4] != i)
	{
		VID_HandlePause();
		cl_stats[4] = i;
	}
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation(int slot)
{
	int		i, j;
	int		top, bottom;
	byte	*dest, *source;

	if (slot > cl_maxclients)
		Sys_Error("CL_NewTranslation: player > cl.maxclients");

	dest = cl_scores[slot].translations;
	source = vid.colormap;
	memcpy(dest, vid.colormap, sizeof(cl_scores[slot].translations));
	top = cl_scores[slot].colors & 0xf0;
	bottom = (cl_scores[slot].colors & 15) << 4;

	for (i = 0; i < VID_GRADES; i++, dest += 256, source += 256)
	{
		if (top < 128)	// the artists made some backwards ranges.  sigh.
			memcpy(dest + TOP_RANGE, source + top, 16);
		else
		{
			for (j = 0; j < 16; j++)
				dest[TOP_RANGE + j] = source[top + 15 - j];
		}

		if (bottom < 128)
			memcpy(dest + BOTTOM_RANGE, source + bottom, 16);
		else
		{
			for (j = 0; j < 16; j++)
				dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
		}
	}
}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic(void)
{
	entity_t	*ent;

	if (cl_num_statics >= MAX_STATIC_ENTITIES)
		Host_Error("Too many static entities\n");

	ent = &cl_static_entities[cl_num_statics];
	cl_num_statics++;
	CL_ParseBaseline(ent);

// copy it to the current state
	ent->model = cl_model_precache[ent->baseline.modelindex];
	ent->frame = ent->baseline.frame;
	ent->colormap = host_colormap;
	ent->effects = ent->baseline.effects;
	ent->skinnum = ent->baseline.skinnum;

	VectorCopy(ent->baseline.origin, ent->origin);
	VectorCopy(ent->baseline.angles, ent->angles);
	R_AddEfrags(ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound(void)
{
	vec3_t	org;
	int		sound_num;
	float	vol, atten;

	org[0] = MSG_ReadCoord();
	org[1] = MSG_ReadCoord();
	org[2] = MSG_ReadCoord();
	sound_num = MSG_ReadByte();
	vol = MSG_ReadByte();
	atten = MSG_ReadByte();

	S_StaticSound(cl_sound_precache[sound_num], org, vol, atten);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage(void)
{
	int		cmd;
	int		i;

//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_Printf("%i ", msg_readcount);
	else if (cl_shownet.value == 2)
		Con_Printf("\n");

	cl_onground = false;	// unless the server says otherwise

//
// parse the message
//
	MSG_BeginReading();

	while (1)
	{
		if (msg_badread)
			Host_Error("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte();

		if (cmd == -1)
			break;		// end of message

	// if the high bit of the command byte is set, it is a fast update
		if (cmd & U_SIGNAL)
		{
			if (cl_shownet.value == 2)
				Con_Printf("%3i:fast update\n", msg_readcount - 1);
			CL_ParseUpdate(cmd & 127);
			continue;
		}

		if (cl_shownet.value == 2)
			Con_Printf("%3i:svc %i\n", msg_readcount - 1, cmd);

	// other commands
		switch (cmd)
		{
		case svc_nop:
			break;

		case svc_disconnect:
			Host_EndGame("Server disconnected\n");

		case svc_updatestat:
			i = MSG_ReadByte();
			if (i >= MAX_CL_STATS)
				Sys_Error("svc_updatestat: %i is invalid", i);
			cl_stats[i] = MSG_ReadLong();
			break;

		case svc_version:
			i = MSG_ReadLong();
			if (i != PROTOCOL_VERSION)
				Host_Error("CL_ParseServerMessage: Server is protocol %i instead of %i", i, PROTOCOL_VERSION);
			return;

		case svc_setview:
			cl_viewentity = MSG_ReadShort();
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;

		case svc_time:
			cl_mtime[1] = cl_mtime[0];
			cl_mtime[0] = MSG_ReadTime();

			if (cls.demoplayback && cl_time < 0)
			{	// first message of a demo
				cl_time = cl_mtime[0];
				cl_oldtime = cl_time;
				cl_mtime[1] = cl_mtime[0];
			}
			break;

		case svc_print:
			Con_Printf("Half-Life, by valve L.L.C\n");
			Con_Printf("%s", MSG_ReadString());
			break;

		case svc_stufftext:
			Cbuf_AddText(MSG_ReadString());
			break;

		case svc_setangle:
			for (i = 0; i < 3; i++)
				cl_viewangles[i] = MSG_ReadAngle();
			break;

		case svc_serverinfo:
			CL_ParseServerInfo();
			break;

		case svc_lightstyle:
			i = MSG_ReadByte();
			if (i >= MAX_LIGHTSTYLES)
				Sys_Error("svc_lightstyle > MAX_LIGHTSTYLES");
			strcpy(cl_lightstyle_value[i], MSG_ReadString());
			Q_strncpy(cl_lightstyle[i].map, cl_lightstyle_value[i], MAX_STYLESTRING - 1);
			cl_lightstyle[i].map[MAX_STYLESTRING - 1] = 0;
			cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
			break;

		case svc_updatename:
			i = MSG_ReadByte();
			if (i >= cl_maxclients)
				Host_Error("svc_updatename > MAX_CLIENTS");
			strcpy(cl_scores[i].name, MSG_ReadString());
			break;

		case svc_updatefrags:
			i = MSG_ReadByte();
			if (i >= cl_maxclients)
				Host_Error("svc_updatefrags > MAX_CLIENTS");
			cl_scores[i].frags = MSG_ReadShort();
			break;

		case svc_clientdata:
			i = MSG_ReadShort();
			CL_ParseClientdata(i);
			break;

		case svc_stopsound:
			i = MSG_ReadShort();
			S_StopSound(i >> 3, i & 7);
			break;

		case svc_updatecolors:
			i = MSG_ReadByte();
			if (i >= cl_maxclients)
				Host_Error("svc_updatecolors > MAX_CLIENTS");
			cl_scores[i].colors = MSG_ReadByte();
			CL_NewTranslation(i);
			break;

		case svc_particle:
			R_ParseParticleEffect();
			break;

		case svc_damage:
			V_ParseDamage();
			break;

		case svc_spawnstatic:
			CL_ParseStatic();
			break;

		case svc_spawnbaseline:
			i = MSG_ReadShort();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline(CL_EntityNum(i));
			break;

		case svc_temp_entity:
			CL_ParseTEnt();
			break;

		case svc_setpause:
			cl_paused = MSG_ReadByte();
			if (cl_paused)
				CDAudio_Pause();
			else
				CDAudio_Resume();
			D_BeginDirectRect();
			break;

		case svc_signonnum:
			i = MSG_ReadByte();
			if (i <= cls.signon)
				Host_Error("Received signon %i when at %i", i, cls.signon);
			cls.signon = i;
			CL_SignonReply();
			break;

		case svc_centerprint:
			SCR_CenterPrint(MSG_ReadString());
			break;

		case svc_killedmonster:
			parsecountmod++;
			break;

		case svc_foundsecret:
			parsecounttime++;
			break;

		case svc_spawnstaticsound:
			CL_ParseStaticSound();
			break;

		case svc_intermission:
			cl_intermission = 1;
			cl_completed_time = (int)cl_time;
			break;

		case svc_finale:
			cl_intermission = 2;
			cl_completed_time = (int)cl_time;
			SCR_CenterPrint(MSG_ReadString());
			break;

		case svc_cdtrack:
			cl_cdtrack = MSG_ReadByte();
			cl_looptrack = MSG_ReadByte();
			if ((cls.demoplayback || cls.timedemo) && cls.forcetrack != -1)
				CDAudio_Play(cls.forcetrack, true);
			else
				CDAudio_Play(cl_cdtrack, true);
			break;

		case svc_sellscreen:
			Cmd_ExecuteString("help", src_command);
			break;

		case svc_cutscene:
			cl_intermission = 3;
			cl_completed_time = (int)cl_time;
			SCR_CenterPrint(MSG_ReadString());
			break;

		case svc_weaponanim:
			cl_viewent_animtime = cl_time;
			cl_viewent_sequence = MSG_ReadByte();
			break;

		case svc_decalname:
			i = MSG_ReadByte();
			Draw_NameToDecal(i, MSG_ReadString());
			break;

		case svc_roomtype:
			i = MSG_ReadShort();
			Cvar_SetValue("room_type", i);
			break;

		default:
			Host_Error("CL_ParseServerMessage: Illegible server message");
		}
	}

	if (cl_shownet.value == 2)
		Con_Printf("%3i:END OF MESSAGE\n", msg_readcount - 1);
}

void CL_NullStub(void)
{
}
