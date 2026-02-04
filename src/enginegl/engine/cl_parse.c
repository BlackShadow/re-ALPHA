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
#include "nullsubs.h"

extern float cl_velocity_old[3];
extern int cl_weaponframe;
extern int cl_armorvalue;
extern int cl_weaponmodel;
extern int cl_health;
extern int cl_activeweapon;
extern int cl_weaponbits[32];
extern int r_dowarp;
extern int cl_spectator;
extern int cl_targetentity;
extern int cl_maxspectators;
extern int cl_autoaim;
extern int msg_readcount;
extern int msg_badread;
extern void CDAudio_Pause(void);
extern void CDAudio_Resume(void);
extern void CDAudio_Play(byte track, qboolean looping);

int parsecountmod;
int parsecounttime;
int oldparsecountmod;
int bitcounts[16];

void CL_ParseStartSoundPacket(void)
{
	const int flags = MSG_ReadByte();
	int volume = 255;
	float attenuation = 1.0f;
	int channel;
	int ent;
	int sound_num;
	vec3_t pos;

	if (flags & 1)
		volume = MSG_ReadByte();

	if (flags & 2)
		attenuation = (float)MSG_ReadByte() / 64.0f;

	channel = MSG_ReadShort();
	ent = channel >> 3;
	channel &= 7;
	if (ent > 600)
		Host_Error("CL_ParseStartSoundPacket: ent = %i", ent);

	sound_num = MSG_ReadByte();

	pos[0] = MSG_ReadCoord();
	pos[1] = MSG_ReadCoord();
	pos[2] = MSG_ReadCoord();

	S_StartSound(ent, channel, cl_sound_precache[sound_num], pos, (float)volume / 255.0f, attenuation);
}

entity_t *CL_EntityNum(int num)
{
	if (cl_num_entities <= num)
	{
		if (num >= MAX_EDICTS)
			Host_Error("CL_EntityNum: %i is an invalid number", num);

		for (; cl_num_entities <= num; ++cl_num_entities)
			cl_entities[cl_num_entities].colormap = (byte *)vid.colormap;
	}

	return &cl_entities[num];
}

void CL_KeepaliveMessage(void)
{
	int ret;
	sizebuf_t old_msg;
	byte old_data[MAX_MSGLEN];
	float time;
	static float lastmsg;

	if (sv.active || cls.demoplayback)
		return;

	old_msg = net_message;
	memcpy(old_data, net_message.data, net_message.cursize);

	while (1)
	{
		ret = CL_GetMessage();
		if (ret == 0)
			break;
		if (ret == 1)
			Host_Error("CL_KeepaliveMessage: CL_GetMessage failed\n");
		if (ret != 2)
			Host_Error("CL_KeepaliveMessage: CL_GetMessage returned %d\n", ret);

		MSG_BeginReading();
		if (MSG_ReadByte() != svc_nop)
			Host_Error("CL_KeepaliveMessage: datagram wasn't a nop\n");
	}

	net_message = old_msg;
	memcpy(net_message.data, old_data, old_msg.cursize);

	time = Sys_FloatTime();
	if (time - lastmsg >= 5.0f)
	{
		lastmsg = time;
		Con_Printf("Client->Server keepalive\n");
		MSG_WriteByte(&cls.message, clc_nop);
		NET_SendMessage(cls.netcon, &cls.message);
		SZ_Clear(&cls.message);
	}
}

void CL_ParseServerInfo(void)
{
	int protocol;
	int maxclients;
	char *mapname;
	int model_count;
	int sound_count;
	int i;
	char model_precache[256][64];
	char sound_precache[256][64];
	model_t *mod;

	Con_DPrintf("Serverinfo packet received.\n");

	SCR_BeginLoadingPlaque();
	CL_ClearState();

	protocol = MSG_ReadLong();
	if (protocol != 15)
	{
		Con_Printf("Server returned version %i, not %i", protocol, 15);
		return;
	}

	maxclients = MSG_ReadByte();
	cl_maxclients = maxclients;

	if (maxclients < 1 || maxclients > 16)
	{
		Con_Printf("Bad maxclients (%u) from server\n", cl_maxclients);
		return;
	}

	cl_scores = Hunk_AllocName(16428 * maxclients, "scores");

	cl_gametype = MSG_ReadByte();

	mapname = MSG_ReadString();
	strncpy(cl_levelname, mapname, 0x27);

	Con_Printf("\n");
	Con_Printf("%s\n", mapname);

	memset(cl_model_precache, 0, sizeof(cl_model_precache));
	for (model_count = 1; ; model_count++)
	{
		mapname = MSG_ReadString();
		if (!*mapname)
			break;
		if (model_count >= 256)
		{
			Con_Printf("Server sent too many model precaches");
			return;
		}
		strcpy(model_precache[model_count], mapname);
		Mod_TouchModel(mapname);
	}

	memset(cl_sound_precache, 0, sizeof(cl_sound_precache));
	for (sound_count = 1; ; sound_count++)
	{
		mapname = MSG_ReadString();
		if (!*mapname)
			break;
		if (sound_count >= 256)
		{
			Con_Printf("Server sent too many sound precaches");
			return;
		}
		strcpy(sound_precache[sound_count], mapname);
		S_InsertText(mapname);
	}

	if (model_count > 1)
	{
		for (i = 1; i < model_count; i++)
		{
			mod = Mod_ForName(model_precache[i], 0);
			if (!mod)
			{
				Con_Printf("Model %s not found\n", model_precache[i]);
				return;
			}
			cl_model_precache[i] = mod;
			CL_KeepaliveMessage();
		}
	}

	if (sound_count > 1)
	{
		for (i = 1; i < sound_count; i++)
		{
			cl_sound_precache[i] = S_PrecacheSound(sound_precache[i]);
			CL_KeepaliveMessage();
		}
	}

	cl_worldmodel = cl_model_precache[1];
	cl.worldmodel = cl_worldmodel;
	cl_entities[0].model = cl_model_precache[1];
	R_NewMap();
	Hunk_Check();

	noclip_anglehack = false;
}

void CL_ParseUpdate(int bits)
{
	int fullBits = bits;

	if (cls.signon == 3)
	{
		cls.signon = 4;
		CL_SignonReply();
	}

	if ((fullBits & 1) != 0)
		fullBits = (MSG_ReadByte() << 8) | bits;

	if ((fullBits & 0x8000) != 0)
		fullBits |= MSG_ReadByte() << 16;

	int entnum;
	if ((fullBits & 0x4000) != 0)
		entnum = MSG_ReadShort();
	else
		entnum = MSG_ReadByte();

	entity_t *ent = CL_EntityNum(entnum);

	for (int i = 0; i < 16; ++i)
	{
		if (((1 << i) & fullBits) != 0)
			++bitcounts[i];
	}

	qboolean forcelink = (ent->msgtime != cl_mtime[1]);
	ent->msgtime = cl_mtime[0];

	int modelindex;
	if ((fullBits & 0x400) != 0)
	{
		modelindex = MSG_ReadByte();
		if (modelindex >= MAX_MODELS)
			Host_Error("CL_ParseModel: bad modelindex");
	}
	else
	{
		modelindex = ent->baseline.modelindex;
	}

	model_t *model = cl_model_precache[modelindex];
	if (ent->model != model)
	{
		ent->model = model;
		if (model)
		{
			if (model->synctype == 1)
				ent->syncbase = (float)(rand() & 0x7FFF) / 32767.0f;
			else
				ent->syncbase = 0.0f;
		}
		else
		{

			forcelink = true;
		}

		if (entnum > 0 && cl_maxclients >= entnum)
			S_AmbientOff_Null();
	}

	if ((fullBits & 0x40) != 0)
		ent->frame = (float)MSG_ReadUShort() / 256.0f;
	else
		ent->frame = (float)ent->baseline.frame;

	int colormap;
	if ((fullBits & 0x800) != 0)
		colormap = MSG_ReadByte();
	else
		colormap = ent->baseline.colormap;

	if (colormap)
	{
		if (cl_maxclients < colormap)
			Sys_Error("i > cl.maxclients");

		ent->colormap = (byte *)cl_scores + 16428 * colormap - 0x4000;
	}
	else
	{
		ent->colormap = (byte *)vid.colormap;
	}

	if ((fullBits & 0x1000) != 0)
		ent->skinnum = MSG_ReadByte();
	else
		ent->skinnum = ent->baseline.skinnum;

	if ((fullBits & 0x2000) != 0)
		ent->effects = MSG_ReadByte();
	else
		ent->effects = ent->baseline.effects;

	VectorCopy(ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy(ent->msg_angles[0], ent->msg_angles[1]);

	if ((fullBits & 2) != 0)
		ent->msg_origins[0][0] = MSG_ReadCoord();
	else
		ent->msg_origins[0][0] = ent->baseline.origin[0];

	if ((fullBits & 0x100) != 0)
		ent->msg_angles[0][0] = MSG_ReadAngle();
	else
		ent->msg_angles[0][0] = ent->baseline.angles[0];

	if ((fullBits & 4) != 0)
		ent->msg_origins[0][1] = MSG_ReadCoord();
	else
		ent->msg_origins[0][1] = ent->baseline.origin[1];

	if ((fullBits & 0x10) != 0)
		ent->msg_angles[0][1] = MSG_ReadAngle();
	else
		ent->msg_angles[0][1] = ent->baseline.angles[1];

	if ((fullBits & 8) != 0)
		ent->msg_origins[0][2] = MSG_ReadCoord();
	else
		ent->msg_origins[0][2] = ent->baseline.origin[2];

	if ((fullBits & 0x200) != 0)
		ent->msg_angles[0][2] = MSG_ReadAngle();
	else
		ent->msg_angles[0][2] = ent->baseline.angles[2];

	if ((fullBits & 0x20) != 0 && cl_lerpstep.value == 0.0f)
		ent->forcelink = true;

	if ((fullBits & 0x20000) != 0)
	{
		const int newSequence = MSG_ReadByte();
		const float baseTime = (float)(int)(__int64)(cl_time * 100.0 / 256.0) * 2.56f;

		float newAnimTime = (float)MSG_ReadByte() / 100.0f + baseTime;
		while (cl_time < newAnimTime)
			newAnimTime -= 2.56f;

		if (ent->anim_time != newAnimTime || newSequence != ent->sequence)
		{
			if (newSequence != ent->sequence)
			{
				ent->blend_oldseq = ent->sequence;
				ent->blend_time = newAnimTime;
				ent->sequence = newSequence;
			}

			memcpy(ent->latched_controller, ent->controller, sizeof(ent->controller));
			memcpy(ent->latched_blending, ent->blending, sizeof(ent->blending));

			ent->latched_anim_time = ent->anim_time;
			VectorCopy(ent->origin, ent->lerp_origin);
			VectorCopy(ent->angles, ent->lerp_angles);

			ent->anim_time = newAnimTime;
		}
	}
	else
	{
		ent->sequence = ent->baseline.sequence;
		ent->anim_time = (float)cl_time;
	}

	if ((fullBits & 0x400000) != 0)
		ent->framerate = (float)MSG_ReadChar() / 64.0f;
	else
		ent->framerate = 1.0f;

	if ((fullBits & 0x40000) != 0)
	{
		ent->controller[0] = (byte)MSG_ReadByte();
		ent->controller[1] = (byte)MSG_ReadByte();
		ent->controller[2] = (byte)MSG_ReadByte();
		ent->controller[3] = (byte)MSG_ReadByte();
	}
	else
	{
		ent->controller[0] = 0;
		ent->controller[1] = 0;
		ent->controller[2] = 0;
		ent->controller[3] = 0;
	}

	if ((fullBits & 0x100000) != 0)
	{
		ent->blending[0] = (byte)MSG_ReadByte();
		ent->blending[1] = (byte)MSG_ReadByte();
	}
	else
	{
		ent->blending[0] = 0;
		ent->blending[1] = 0;
	}

	if ((fullBits & 0x200000) != 0)
		ent->body = MSG_ReadByte();
	else
		ent->body = 0;

	if ((fullBits & 0x80000) != 0)
	{
		ent->rendermode = MSG_ReadByte();
		ent->renderamt = MSG_ReadByte();
		ent->rendercolor[0] = (byte)MSG_ReadByte();
		ent->rendercolor[1] = (byte)MSG_ReadByte();
		ent->rendercolor[2] = (byte)MSG_ReadByte();
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

	ent->has_lerpdata = 4;
	if ((fullBits & 0x800000) == 0)
		ent->has_lerpdata = 0;

	if (forcelink)
	{
		VectorCopy(ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy(ent->msg_origins[0], ent->origin);
		VectorCopy(ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy(ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
	}
}

void CL_ParseBaseline(entity_t *ent)
{
	int i;

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
		ent->baseline.rendercolor[0] = (byte)MSG_ReadByte();
		ent->baseline.rendercolor[1] = (byte)MSG_ReadByte();
		ent->baseline.rendercolor[2] = (byte)MSG_ReadByte();
		ent->baseline.renderfx = MSG_ReadByte();
	}
}

void CL_ParseClientdata(int bits)
{
	int i;

	if ((bits & 1) != 0)
		cl_viewheight = (float)MSG_ReadChar();
	else
		cl_viewheight = 22.0f;

	if ((bits & 2) != 0)
		cl_idealpitch = (float)MSG_ReadChar();
	else
		cl_idealpitch = 0.0f;

	VectorCopy(cl_punchangle, cl_punchangle_old);

	for (i = 0; i < 3; ++i)
	{
		if ((bits & (4 << i)) != 0)
			cl_punchangle[i] = (float)MSG_ReadChar();
		else
			cl_punchangle[i] = 0.0f;

		if ((bits & (32 << i)) != 0)
			cl_velocity[i] = (float)(16 * MSG_ReadChar());
		else
			cl_velocity[i] = 0.0f;
	}

	int newItems = MSG_ReadLong();
	if (cl_items != newItems)
	{
		VID_HandlePause();

		for (i = 0; i < 32; ++i)
		{
			if ((newItems & (1 << i)) != 0 && ((1 << i) & cl_items) == 0)
				cl_weaponbits[i] = (int)cl_time;
		}

		cl_items = newItems;
	}

	int newActiveWeapon = MSG_ReadLong();
	if (cl_activeweapon != newActiveWeapon)
	{
		VID_HandlePause();
		cl_activeweapon = newActiveWeapon;
	}

	if ((bits & 0x100) != 0)
		newActiveWeapon = MSG_ReadLong();
	if (cl_activeweapon != newActiveWeapon)
	{
		VID_HandlePause();
		cl_activeweapon = newActiveWeapon;
	}

	cl_onground = (bits & 0x400) >> 10;
	cl_inwater = (bits & 0x800) >> 11;

	if (cl_inwater != 0)
		r_dowarp = (bits & 0x8000) != 0 ? 3 : 2;
	else
		r_dowarp = 0;

	int newWeaponFrame = 0;
	if ((bits & 0x1000) != 0)
		newWeaponFrame = MSG_ReadByte();
	if (cl_weaponframe != newWeaponFrame)
	{
		VID_HandlePause();
		cl_weaponframe = newWeaponFrame;
	}

	int newArmorValue = 0;
	if ((bits & 0x2000) != 0)
		newArmorValue = MSG_ReadByte();
	if (cl_armorvalue != newArmorValue)
	{
		VID_HandlePause();
		cl_armorvalue = newArmorValue;
	}

	int newWeaponModel = 0;
	if ((bits & 0x4000) != 0)
		newWeaponModel = MSG_ReadByte();
	if (cl_weaponmodel != newWeaponModel)
	{
		VID_HandlePause();
		cl_weaponmodel = newWeaponModel;
	}

	int newHealth = MSG_ReadShort();
	if (cl_health != newHealth)
	{
		VID_HandlePause();
		cl_health = newHealth;
	}

	int newLightLevel = MSG_ReadByte();
	if (cl_lightlevel != newLightLevel)
	{
		VID_HandlePause();
		cl_lightlevel = newLightLevel;
	}

	for (i = 0; i < 4; ++i)
	{
		int stat = MSG_ReadByte();
		if (cl_stats[i] != stat)
		{
			VID_HandlePause();
			cl_stats[i] = stat;
		}
	}

	int stat4 = MSG_ReadLong();
	if (cl_stats[4] != stat4)
	{
		VID_HandlePause();
		cl_stats[4] = stat4;
	}
}

void CL_NewTranslation(int playernum)
{
	int *dest;
	int topcolor, bottomcolor;
	int i, j;
	byte *colormap;

	if (playernum > cl_maxclients)
		Sys_Error("CL_NewTranslation: player > cl.maxclients");

	dest = (int *)((byte *)cl_scores + 16428 * playernum + 44);

	colormap = vid.colormap;
	memcpy(dest, colormap, 0x4000);

	topcolor = *(int *)((byte *)cl_scores + 16428 * playernum + 40) & 0xF0;
	bottomcolor = 16 * (*(int *)((byte *)cl_scores + 16428 * playernum + 40) & 0x0F);

	for (i = 0; i < 64; i++)
	{

		if (topcolor >= 128)
		{
			for (j = 0; j < 16; j++)
			{
				((byte *)dest)[16 + j] = colormap[topcolor + 15 - j];
			}
		}
		else
		{
			dest[4] = *(int *)(colormap + topcolor);
			dest[5] = *(int *)(colormap + topcolor + 4);
			dest[6] = *(int *)(colormap + topcolor + 8);
			dest[7] = *(int *)(colormap + topcolor + 12);
		}

		if (bottomcolor >= 128)
		{
			for (j = 0; j < 16; j++)
			{
				((byte *)dest)[96 + j] = colormap[bottomcolor + 15 - j];
			}
		}
		else
		{
			dest[24] = *(int *)(colormap + bottomcolor);
			dest[25] = *(int *)(colormap + bottomcolor + 4);
			dest[26] = *(int *)(colormap + bottomcolor + 8);
			dest[27] = *(int *)(colormap + bottomcolor + 12);
		}

		colormap += 256;
		dest += 64;
	}
}

void CL_ParseStatic(void)
{
	entity_t *ent;

	if (cl_num_statics >= 128)
		Host_Error("Too many static entities\n");

	ent = &cl_static_entities[cl_num_statics];
	cl_num_statics++;

	CL_ParseBaseline(ent);

	ent->model = cl_model_precache[ent->baseline.modelindex];
	ent->frame = (float)ent->baseline.frame;
	ent->colormap = host_colormap;
	ent->effects = ent->baseline.effects;
	ent->skinnum = ent->baseline.skinnum;

	VectorCopy(ent->baseline.origin, ent->origin);
	VectorCopy(ent->baseline.angles, ent->angles);

	R_AddEfrags(ent);
}

void CL_ParseStaticSound(void)
{
	vec3_t pos;
	int sound_num;
	float vol, atten;

	pos[0] = MSG_ReadCoord();
	pos[1] = MSG_ReadCoord();
	pos[2] = MSG_ReadCoord();

	sound_num = MSG_ReadByte();
	vol = (float)MSG_ReadByte();
	atten = (float)MSG_ReadByte();

	S_StaticSound(cl_sound_precache[sound_num], pos, vol, atten);
}

void CL_ParseServerMessage(void)
{
	int cmd;
	char *s;
	int i;
	int version;
	int lightnum;
	char *lightstyle;
	int playernum;
	int bits;
	int entnum;
	entity_t *ent;
	int signon_stage;
	char *centerprint;
	char *stufftext;
	char *stuffcmd;
	char *name;
	int room_type;

	if (cl_shownet.value == 1.0f)
		Con_Printf("%i ", msg_readcount);
	else if (cl_shownet.value == 2.0f)
		Con_Printf("\n");

	cl_onground = 0;
	MSG_BeginReading();

	while (1)
	{
		if (msg_badread)
			Host_Error("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte();

		if (cmd == -1)
			break;

		if ((cmd & 0x80) != 0)
		{
			if (cl_shownet.value == 2.0f)
				Con_Printf("%3i:fast update\n", msg_readcount - 1);

			CL_ParseUpdate(cmd & 0x7F);
		}
		else
		{
			if (cl_shownet.value == 2.0f)
				Con_Printf("%3i:svc %i\n", msg_readcount - 1, cmd);

			switch (cmd)
			{
				case svc_nop:
					continue;

				case svc_disconnect:
					Host_EndGame("Server disconnected\n");

				case svc_updatestat:
					i = MSG_ReadByte();
					if (i >= 32)
						Sys_Error("svc_updatestat: %i is invalid", i);
					cl_stats[i] = MSG_ReadLong();
					break;

				case svc_version:
					version = MSG_ReadLong();
					if (version != 15)
						Host_Error("CL_ParseServerMessage: Server is protocol %i instead of %i", version, 15);
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

					if (cls.demoplayback && cl_time < 0.0)
					{
						cl_time = cl_mtime[0];
						cl_oldtime = cl_time;
						cl_mtime[1] = cl_mtime[0];
					}
					break;

				case svc_print:
					Con_Printf("Half-Life, by valve L.L.C\n");
					s = MSG_ReadString();
					Con_Printf("%s", s);
					break;

				case svc_stufftext:
					stufftext = MSG_ReadString();
					Cbuf_AddText(stufftext);
					break;

				case svc_setangle:
					for (i = 0; i < 3; i++)
						cl_viewangles[i] = MSG_ReadAngle();
					break;

				case svc_serverinfo:
					CL_ParseServerInfo();
					break;

				case svc_lightstyle:
					lightnum = MSG_ReadByte();
					if (lightnum >= 64)
						Sys_Error("svc_lightstyle > MAX_LIGHTSTYLES");
					lightstyle = MSG_ReadString();
					strcpy(cl_lightstyle_value[lightnum], lightstyle);
					Q_strncpy(cl_lightstyle[lightnum].map, cl_lightstyle_value[lightnum], MAX_STYLESTRING - 1);
					cl_lightstyle[lightnum].map[MAX_STYLESTRING - 1] = 0;
					cl_lightstyle[lightnum].length = Q_strlen(cl_lightstyle[lightnum].map);
					break;

				case svc_updatename:
					playernum = MSG_ReadByte();
					if (playernum >= cl_maxclients)
						Host_Error("svc_updatename > MAX_CLIENTS");
					strcpy((char *)cl_scores + 16428 * playernum, MSG_ReadString());
					break;

				case svc_updatefrags:
					playernum = MSG_ReadByte();
					if (playernum >= cl_maxclients)
						Host_Error("svc_updatefrags > MAX_CLIENTS");
					*(int *)((byte *)cl_scores + 16428 * playernum + 36) = MSG_ReadShort();
					break;

				case svc_clientdata:
					bits = MSG_ReadShort();
					CL_ParseClientdata(bits);
					break;

				case svc_stopsound:
					i = MSG_ReadShort();
					S_StopSound(i >> 3, i & 7);
					break;

				case svc_updatecolors:
					playernum = MSG_ReadByte();
					if (playernum >= cl_maxclients)
						Host_Error("svc_updatecolors > MAX_CLIENTS");
					*(int *)((byte *)cl_scores + 16428 * playernum + 40) = MSG_ReadByte();
					CL_NewTranslation(playernum);
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
					entnum = MSG_ReadShort();
					ent = CL_EntityNum(entnum);
					CL_ParseBaseline(ent);
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
					signon_stage = MSG_ReadByte();
					if (signon_stage <= cls.signon)
						Host_Error("Received signon %i when at %i", signon_stage, cls.signon);
					cls.signon = signon_stage;
					CL_SignonReply();
					break;

				case svc_centerprint:
					centerprint = MSG_ReadString();
					SCR_CenterPrint(centerprint);
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
					stuffcmd = MSG_ReadString();
					SCR_CenterPrint(stuffcmd);
					break;

				case svc_cdtrack:
					cl_cdtrack = MSG_ReadByte();
					cl_looptrack = MSG_ReadByte();
					if ((cls.demoplayback || cls.timedemo) && cls.forcetrack != -1)
						CDAudio_Play((byte)cls.forcetrack, true);
					else
						CDAudio_Play((byte)cl_cdtrack, true);
					break;

				case svc_sellscreen:
					Cmd_ExecuteString("help", src_command);
					break;

				case svc_cutscene:
					cl_intermission = 3;
					cl_completed_time = (int)cl_time;
					name = MSG_ReadString();
					SCR_CenterPrint(name);
					break;

				case svc_weaponanim:
					cl_viewent_animtime = (float)cl_time;
					cl_viewent_sequence = MSG_ReadByte();
					break;

				case svc_decalname:
					playernum = MSG_ReadByte();
					name = MSG_ReadString();
					Draw_NameToDecal(playernum, name);
					break;

				case svc_roomtype:
					room_type = MSG_ReadShort();
					Cvar_SetValue("room_type", (float)room_type);
					break;

				default:
					Host_Error("CL_ParseServerMessage: Illegible server message");
			}
		}
	}

	if (cl_shownet.value == 2.0f)
		Con_Printf("%3i:END OF MESSAGE\n", msg_readcount - 1);
}

void CL_NullStub(void)
{

}


