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

cvar_t cl_name = { "_cl_name", "player", true, false };
cvar_t cl_color = { "_cl_color", "0", true, false };
cvar_t cl_shownet = { "cl_shownet", "0" };
cvar_t cl_nolerp = { "cl_nolerp", "0" };
cvar_t cl_lerplocal = { "cl_lerplocal", "0" };
cvar_t cl_lerpstep = { "cl_lerpstep", "1" };
cvar_t cl_pitchdriftspeed = { "cl_pitchdriftspeed", "0" };
cvar_t cl_pitchdrift = { "cl_pitchdrift", "0" };

int v_idle_active = 0;
float v_idle_time = 0.0f;
double v_idle_start = 0.0;
float v_idle_counter = 0.0f;
float v_idle_max_time = 0.0f;
float v_idle_timeout = 0.0f;

float v_velocity_threshold = 0.0f;
float v_target_pitch = 0.0f;

float shake_next_time = 0.0f;
int shake_intensity = 0;
int shake_color_r = 0;
int shake_color_g = 0;
int shake_color_b = 0;

void CL_PrintEntities_f(void);
int CL_Disconnect(void);
int CL_Disconnect_f(void);

void CL_Record_f(void);
void CL_Stop_f(void);
void CL_PlayDemo_f(void);
void CL_TimeDemo_f(void);

void SCR_Screenshot_f(void);
void CL_StartMovie_f(void);
void CL_StopMovie_f(void);

int R_PrecacheWeaponSounds(void);

//=========================================================
// CL_KeyDown_Null - empty stub
//=========================================================
void CL_KeyDown_Null(void)
{
}

//=========================================================
// CL_KeyUp_Null - empty stub
//=========================================================
void CL_KeyUp_Null(void)
{
}

//=========================================================
// CL_Stub - empty client stub
//=========================================================
void CL_Stub(void)
{
}

int CL_LoadSpriteModel(model_t *mod, void *spriteData)
{
	int *framePtr;
	unsigned int totalSize;
	int result;
	void *allocatedData;
	int startMark;
	dsprite_t *pSpriteData;
	int frameIndex;
	int frameDataOffset;
	int frameCount;
	char pathBuffer[256];

	startMark = Hunk_LowMark();

	if (Mod_GetType(*(int *)((byte *)spriteData + 4)) != 6)
	{
		memset(spriteData, 0, 188);
		strcpy((char *)spriteData + 8, "bogus");
		*((int *)spriteData + 18) = 188;
	}

	mod->type = mod_sprite;
	mod->flags = 0;

	pSpriteData = (dsprite_t *)Hunk_AllocName(*((int *)spriteData + 18), "sprite");
	memcpy(pSpriteData, spriteData, *((int *)spriteData + 18));

	frameIndex = 0;
	frameCount = *((int *)spriteData + 25);

	if (frameCount > 0)
	{
		framePtr = (int *)((byte *)pSpriteData + *((int *)spriteData + 26) + 76);

		do
		{
			frameDataOffset = (int)pSpriteData + *framePtr + *(framePtr - 2) * *(framePtr - 1);

			strcpy(pathBuffer, mod->name);
			strcat(pathBuffer, (char *)(framePtr - 19));

			*framePtr = GL_LoadTexture(
				pathBuffer,
				*(framePtr - 2),
				*(framePtr - 1),
				(byte *)((int)pSpriteData + *framePtr),
				0,
				0,
				(byte *)frameDataOffset
			);

			framePtr += 20;
			frameIndex++;
		}
		while (frameCount > frameIndex);
	}

	totalSize = Hunk_LowMark() - startMark;
	result = (int)Cache_Alloc(&mod->cache, totalSize, "sprite");
	allocatedData = mod->cache.data;

	if (allocatedData)
	{
		memcpy(allocatedData, pSpriteData, totalSize);
		return Hunk_FreeToLowMark(startMark);
	}

	return result;
}

void CL_Init(void)
{
	SZ_Alloc(&cls.message, 1024);
	CL_InitInput();

	R_PrecacheWeaponSounds();

	Cvar_RegisterVariable(&cl_name);
	Cvar_RegisterVariable(&cl_color);
	Cvar_RegisterVariable(&cl_shownet);
	Cvar_RegisterVariable(&cl_nolerp);
	Cvar_RegisterVariable(&cl_lerplocal);
	Cvar_RegisterVariable(&cl_lerpstep);
	Cvar_RegisterVariable(&cl_pitchdriftspeed);

	Cvar_RegisterVariable(&cl_upspeed);
	Cvar_RegisterVariable(&cl_forwardspeed);
	Cvar_RegisterVariable(&cl_backspeed);
	Cvar_RegisterVariable(&cl_sidespeed);
	Cvar_RegisterVariable(&cl_movespeedkey);
	Cvar_RegisterVariable(&cl_yawspeed);
	Cvar_RegisterVariable(&cl_pitchspeed);
	Cvar_RegisterVariable(&cl_anglespeedkey);

	Cmd_AddCommand("entities", CL_PrintEntities_f);
	Cmd_AddCommand("disconnect", CL_Disconnect_f);
	Cmd_AddCommand("record", CL_Record_f);
	Cmd_AddCommand("stop", CL_Stop_f);
	Cmd_AddCommand("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand("timedemo", CL_TimeDemo_f);
	Cmd_AddCommand("snapshot", SCR_Screenshot_f);
	Cmd_AddCommand("startmovie", CL_StartMovie_f);
	Cmd_AddCommand("endmovie", CL_StopMovie_f);
}

int CL_Disconnect(void)
{
	S_StopAllSounds(true);

	if (cls.demoplayback)
	{
		CL_StopPlayback();
	}
	else if (cls.state == ca_connected)
	{
		if (cls.demorecording)
			CL_Stop_f();

		Con_DPrintf("Sending clc_disconnect\n");
		SZ_Clear(&cls.message);
		MSG_WriteByte(&cls.message, clc_disconnect);
		NET_SendMessage(cls.netcon, &cls.message);
		SZ_Clear(&cls.message);

		NET_Close(cls.netcon);
		cls.state = ca_disconnected;

		if (sv.active)
			Host_ShutdownServer(false);
	}

	cls.signon = 0;
	cls.demoplayback = 0;
	cls.timedemo = 0;

	return 0;
}

int CL_Disconnect_f(void)
{
	const int result = CL_Disconnect();

	if (sv.active)
		Host_ShutdownServer(false);

	return result;
}

void CL_EstablishConnection(char *host)
{
	if (cls.state && !cls.demoplayback)
	{
		CL_Disconnect();
		cls.netcon = NET_Connect(host);
		if (!cls.netcon)
			Host_Error("CL_Connect: connect failed\n");
		Con_DPrintf("CL_EstablishConnection: connected to %s\n", host);
		cls.demonum = -1;
		cls.state = ca_connected;
		cls.signon = 0;
	}
}

void CL_SignonReply(void)
{
	char spawnBuffer[8192];

	Con_DPrintf("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, "prespawn");
		break;

	case 2:
		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, va("name \"%s\"\n", cl_name.string));

		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, va("color %i %i\n",
			(int)cl_color.value >> 4,
			(unsigned char)(int)cl_color.value & 15));

		MSG_WriteByte(&cls.message, clc_stringcmd);
		sprintf(spawnBuffer, "spawn %s", cls_spawnparms);
		MSG_WriteString(&cls.message, spawnBuffer);
		break;

	case 3:
		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, "begin");
		Cache_Report();
		break;

	case 4:
		SCR_EndLoadingPlaque();

		if (cls.demoplayback)
		{
			key_dest = key_game;
			scr_con_current = 0.0f;
			Con_ClearNotify();
		}
		break;
	}
}

void CL_NextDemo(void)
{
	char demoCommandBuffer[1024];

	if (cls.demonum != -1)
	{
		SCR_BeginLoadingPlaque();

		if (cls.demos[cls.demonum][0] && cls.demonum != 8 ||
		    (cls.demonum = 0, cls.demos[0][0]))
		{
			sprintf(demoCommandBuffer, "playdemo %s\n", cls.demos[cls.demonum]);
			Cbuf_InsertText(demoCommandBuffer);
			cls.demonum++;
		}
		else
		{
			Con_Printf("No demos listed with startdemos\n");
			cls.demonum = -1;
		}
	}
}

void CL_PrintEntities_f(void)
{
	int entityNum;
	cl_entity_t *ent;
	model_t *model;

	entityNum = 0;
	ent = cl_entities;

	for (entityNum = 0; entityNum < cl.num_entities; entityNum++, ent++)
	{
		Con_Printf("%3i:", entityNum);
		model = ent->model;

		if (model)
		{
			Con_Printf("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
				model->name,
				(int)ent->frame,
				ent->origin[0],
				ent->origin[1],
				ent->origin[2],
				ent->angles[0],
				ent->angles[1],
				ent->angles[2]);
		}
		else
		{
			Con_Printf("EMPTY\n");
		}
	}
}

void SCR_Screenshot_f(void)
{
	int screenshotNum;
	char filenameBuffer[64];
	char baseNameBuffer[64];

	if (cl.num_entities && cl.worldmodel)
		COM_FileBase(cl.worldmodel->name, baseNameBuffer);
	else
		strcpy(baseNameBuffer, "Snapshot");

	screenshotNum = cl.snapshot_number++;
	sprintf(filenameBuffer, "%s%04d.bmp", baseNameBuffer, screenshotNum);

	VID_TakeSnapshot(filenameBuffer);
}

void CL_StopMovie_f(void)
{
	if (!cl.movie_recording)
	{
		Con_Printf("No movie started.\n");
		return;
	}

	cl.movie_recording = false;
	Con_Printf("Stopped recording movie...\n");
}

dlight_t *CL_AllocDlight(int key)
{
	dlight_t *dl;
	int i;

	if (key)
	{
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++)
		{
			if (dl->key == key)
			{
				memset(dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl_time)
		{
			memset(dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset(dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

int CL_DecayLights(void)
{
	int i;
	dlight_t *dl;
	float time;

	time = (float)(cl_time - cl_oldtime);
	dl = cl_dlights;

	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl_time)
		{
			dl->radius = 0;
			continue;
		}

		if (dl->radius > 0)
		{
			dl->radius -= dl->decay * time;
			if (dl->radius < 0)
				dl->radius = 0;
		}
	}

	return 0;
}

double CL_LerpPoint(void)
{
	float timeDiff;
	float lerpFrac;
	double lerpTime;

	timeDiff = (float)(cl_mtime[0] - cl_mtime[1]);

	if (timeDiff == 0.0f || cl_nolerp.value != 0.0f || cls.timedemo || (sv.active && cl_lerplocal.value == 0.0f))
	{
		cl_time = cl_mtime[0];
		return 1.0;
	}

	if (timeDiff > 0.1f)
	{
		timeDiff = 0.1f;
		cl_mtime[1] = cl_mtime[0] - 0.1;
	}

	lerpTime = cl_time;
	const qboolean listenServerLerp = (sv.active && cl_lerplocal.value != 0.0f);
	if (listenServerLerp)
		lerpTime = cl_time - (double)timeDiff;

	lerpFrac = (float)((lerpTime - cl_mtime[1]) / (double)timeDiff);

	if (lerpFrac < 0.0f)
	{
		if (lerpFrac < -0.01f && !listenServerLerp)
			cl_time = cl_mtime[1];
		return 0.0;
	}

	if (lerpFrac > 1.0f)
	{
		if (lerpFrac > 1.01f)
		{
			if (listenServerLerp)
			{
				cl_time = cl_mtime[0] + (double)timeDiff;
				if (cl_oldtime > cl_time)
					cl_oldtime = cl_time;
			}
			else
			{
				cl_time = cl_mtime[0];
			}
		}
		return 1.0;
	}

	return lerpFrac;
}

int CL_RelinkEntities(void)
{
	entity_t *ent;
	float lerpFrac;
	vec3_t oldOrigin;
	float angleDelta[3];
	int i;

	extern int cl_numvisedicts;
	extern cl_entity_t *cl_visedicts[MAX_VISEDICTS];
	extern cvar_t chase_active;
	extern int cam_thirdperson;
	extern entity_t viewent_entity;

	lerpFrac = (float)CL_LerpPoint();
	cl.num_visedicts = 0;
	cl_numvisedicts = 0;

	for (i = 0; i < 3; i++)
	{
		cl.punchangle[i] = (cl.mviewangles[0][i] - cl.punchangle[i]) * lerpFrac + cl.punchangle[i];
	}

	if (cls.demoplayback)
	{
		for (i = 0; i < 3; i++)
		{
			angleDelta[i] = cl.mviewangles[0][i] - cl.mviewangles[1][i];

			if (angleDelta[i] > 180.0)
				angleDelta[i] -= 360.0;
			else if (angleDelta[i] < -180.0)
				angleDelta[i] += 360.0;

			cl_viewangles[i] = angleDelta[i] * lerpFrac + cl.mviewangles[1][i];
		}
	}

	for (i = 1; i < cl_num_entities; i++)
	{
		ent = &cl_entities[i];

		if (ent->msgtime != cl_mtime[0])
		{
			if (ent->efrag)
				R_RemoveEfrags(ent);
			ent->model = NULL;
			continue;
		}

		VectorCopy(ent->origin, oldOrigin);

		if (ent->forcelink)
		{
			VectorCopy(ent->msg_origins[0], ent->origin);
			VectorCopy(ent->msg_angles[0], ent->angles);
		}
		else
		{
			float entityLerpFrac;
			float deltaOrigin[3];
			int axis;

			entityLerpFrac = lerpFrac;
			for (axis = 0; axis < 3; ++axis)
			{
				deltaOrigin[axis] = ent->msg_origins[0][axis] - ent->msg_origins[1][axis];
				if (deltaOrigin[axis] > 100.0f || deltaOrigin[axis] < -100.0f)
					entityLerpFrac = 1.0f;
			}

			for (axis = 0; axis < 3; ++axis)
			{
				float deltaAngle;

				ent->origin[axis] = ent->msg_origins[1][axis] + deltaOrigin[axis] * entityLerpFrac;

				deltaAngle = ent->msg_angles[0][axis] - ent->msg_angles[1][axis];
				if (deltaAngle > 180.0f)
					deltaAngle = deltaAngle - 360.0f;
				else if (deltaAngle < -180.0f)
					deltaAngle = deltaAngle + 360.0f;

				ent->angles[axis] = ent->msg_angles[1][axis] + deltaAngle * entityLerpFrac;
			}
		}

		if (!ent->model)
		{
			if (ent->efrag)
				R_RemoveEfrags(ent);

			ent->forcelink = false;
			continue;
		}

		const int modelFlags = ent->model ? ent->model->flags : 0;
		if ((modelFlags & EF_ROTATE) != 0)
			ent->angles[1] = anglemod((float)(cl_time * 100.0));

		if ((ent->effects & EF_BRIGHTFIELD) != 0)
			R_EntityParticles(ent);

		if ((ent->effects & EF_NOINTERP) != 0)
			R_DarkFieldParticles(ent->origin);

		if ((ent->effects & EF_BRIGHTLIGHT) != 0)
		{
			dlight_t *dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->origin[2] += 16.0;
			const int randVal = rand();
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 250;
			dl->radius = (float)((randVal & 0x1F) + 400);
			dl->die = (float)(cl_time + 0.001);
		}

		if ((ent->effects & EF_DIMLIGHT) != 0)
		{
			dlight_t *dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			const int randVal = rand();
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 100;
			dl->radius = (float)((randVal & 0x1F) + 200);
			dl->die = (float)(cl_time + 0.001);
		}

		if ((ent->effects & EF_INVLIGHT) != 0)
		{
			dlight_t *dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			const int randVal = rand();
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 100;
			dl->radius = (float)((randVal & 0x1F) + 200);
			dl->ramp = 1;
			dl->die = (float)(cl_time + 0.001);
		}

		if ((ent->effects & EF_LIGHT) != 0)
		{
			dlight_t *dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 100;
			dl->radius = 200.0f;
			dl->die = (float)(cl_time + 0.001);
		}

		const int entityFlags = modelFlags;

		if ((entityFlags & EF_GIB) != 0)
			R_RocketTrail(oldOrigin, ent->origin, 2);
		else if ((entityFlags & EF_ZOMGIB) != 0)
			R_RocketTrail(oldOrigin, ent->origin, 4);
		else if ((entityFlags & EF_TRACER) != 0)
			R_RocketTrail(oldOrigin, ent->origin, 3);
		else if ((entityFlags & EF_TRACER2) != 0)
			R_RocketTrail(oldOrigin, ent->origin, 5);
		else if ((entityFlags & EF_ROCKET) != 0)
		{
			R_RocketTrail(oldOrigin, ent->origin, 0);
			dlight_t *dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->radius = 200.0f;
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 200;
			dl->die = (float)(cl_time + 0.01);
			dl->minlight = 200;
		}
		else if ((entityFlags & EF_GRENADE) != 0)
		{
			R_RocketTrail(oldOrigin, ent->origin, 1);
		}
		else if ((entityFlags & EF_TRACER3) != 0)
		{
			R_RocketTrail(oldOrigin, ent->origin, 6);
		}

		ent->forcelink = false;

		if ((cam_thirdperson || cl_viewentity != i || chase_active.value != 0.0f) &&
		    (ent->effects & EF_NODRAW) == 0 && cl_numvisedicts < MAX_VISEDICTS)
		{
			cl_visedicts[cl_numvisedicts++] = ent;
			cl.num_visedicts = cl_numvisedicts;
		}
	}

	if ((cl_entities[cl_viewentity].effects & EF_MUZZLEFLASH) != 0)
		viewent_entity.effects |= EF_MUZZLEFLASH;

	return 0x130 * cl_viewentity;
}

int CL_ReadFromServer(void)
{
	int ret;
	float timeStamp;

	cl_oldtime = cl_time;

	const qboolean listenServer = (sv.active && svs.maxclients <= 1);
	const qboolean uiCapturing = (key_dest == key_console || key_dest == key_menu);
	if (!(listenServer && uiCapturing))
		cl_time = cl_time + host_frametime;

	do
	{
		ret = CL_GetMessage();
		if (ret == -1)
			Host_Error("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		timeStamp = realtime;
		cl.last_received_message = timeStamp;
		CL_ParseServerMessage();
	}
	while (cls.state == ca_connected);

	if (cl_shownet.value)
		Con_Printf("\n");

	CL_RelinkEntities();
	CL_UpdateTEnts();

	return 0;
}

void CL_SendCmd(void)
{
	usercmd_t cmd;

	if (cls.state == ca_connected)
	{
		if (cls.signon == 4)
		{
			CL_BaseMove(&cmd);
			IN_Move(&cmd);
			CL_SendMove(&cmd);
		}

		if (!cls.demoplayback)
		{
			if (!NET_CanSendMessage(cls.netcon))
			{
				Con_DPrintf("CL_WriteToServer: can't send\n");
				return;
			}

			if (NET_SendMessage(cls.netcon, &cls.message) == -1)
				Host_Error("CL_WriteToServer: lost server connection");
		}

		SZ_Clear(&cls.message);
	}
}

int V_StartIdleAnim(void)
{
	v_idle_active = 1;
	v_idle_time = 0.0f;
	v_idle_start = cl_time;

	return 0;
}

void V_StopIdleAnim(void)
{
	if (v_idle_start != cl_time && (v_idle_active || v_idle_time == 0.0f))
	{
		v_idle_time = v_idle_max_time;
		v_idle_active = 0;
		v_idle_counter = 0.0f;
	}
}

void V_UpdateIdleAnim(void)
{
	extern int cl_intermission;
	extern int cl_onground;
	extern int cl_dead;
	extern float cl_forward_velocity;
	extern double host_frametime;
	extern float cl_viewangles_pitch;

	float newCounter;
	float delta;
	float speed;
	float targetDelta;

	if (cl_intermission || !cl_onground || cl_dead)
	{
		v_idle_counter = 0.0f;
		v_idle_time = 0.0f;
	}
	else if (v_idle_active)
	{
		if (fabs(cl_forward_velocity) >= v_velocity_threshold)
		{
			newCounter = v_idle_counter + host_frametime;
			v_idle_counter = newCounter;
		}
		else
		{
			v_idle_counter = 0.0f;
		}

		if (v_idle_counter > (double)v_idle_timeout)
			V_StopIdleAnim();
	}
	else
	{
		delta = v_target_pitch - cl_viewangles_pitch;

		if (delta == 0.0f)
		{
			v_idle_time = 0.0f;
		}
		else
		{
			speed = host_frametime * v_idle_time;
			newCounter = host_frametime * v_idle_max_time + v_idle_time;
			v_idle_time = newCounter;

			if (delta > 0.0f)
			{
				if (speed > (double)delta)
				{
					v_idle_time = 0.0f;
					speed = v_target_pitch - cl_viewangles_pitch;
				}
				cl_viewangles_pitch = cl_viewangles_pitch + speed;
			}
			else if (delta < 0.0f)
			{
				targetDelta = -delta;
				if (targetDelta < (double)speed)
				{
					v_idle_time = 0.0f;
					speed = targetDelta;
				}
				cl_viewangles_pitch = cl_viewangles_pitch - speed;
			}
		}
	}
}

int V_BuildGammaTables(float gamma)
{
	extern byte gammatable[256];
	extern int lightgammatable[1024];
	extern cvar_t cv_brightness;
	extern cvar_t cv_texgamma;
	extern cvar_t cv_lightgamma;

	int i;
	int value;
	int j;
	double pow_result;
	double final_value;
	float adjusted_gamma;
	float brightness_scale;

	if (gamma > 1.0f)
	{
		if (gamma > 3.0f)
			gamma = 3.0f;
		adjusted_gamma = cv_texgamma.value / gamma;

		if (cv_brightness.value > 0.0f)
		{
			if (cv_brightness.value <= 1.0f)
				brightness_scale = cv_brightness.value * cv_brightness.value * -0.075f + 0.125f;
			else
				brightness_scale = 0.050000001f;
		}
		else
		{
			brightness_scale = 0.125f;
		}
	}
	else
	{
		brightness_scale = 0.125f;
		adjusted_gamma = gamma;
	}

	for (i = 0; i < 256; i++)
	{
		value = (int)(pow((double)i / 255.0, adjusted_gamma) * 255.0);
		if (value < 0)
			value = 0;
		if (value > 255)
			value = 255;
		gammatable[i] = (byte)value;
	}

	for (j = 0; j < 1024; j++)
	{
		pow_result = pow((double)j / 1023.0, cv_lightgamma.value);

		if (brightness_scale <= pow_result)
			final_value = (pow_result - brightness_scale) / (1.0 - brightness_scale) * 0.875 + 0.125;
		else
			final_value = pow_result / brightness_scale * 0.125;

		value = (int)(pow(final_value, 1.0 / gamma) * 1023.0);
		if (value < 0)
			value = 0;
		if (value > 1023)
			value = 1023;
		lightgammatable[j] = value;
	}

	return value;
}

int V_CheckGamma(void)
{
	extern float old_gamma;
	extern float old_lightgamma;
	extern float old_brightness;
	extern cvar_t cv_v_gamma;
	extern cvar_t cv_lightgamma;
	extern cvar_t cv_brightness;
	extern int vid_gamma_changed;
	extern void S_AmbientOn(void);

	if (old_gamma != cv_v_gamma.value || old_lightgamma != cv_lightgamma.value || old_brightness != cv_brightness.value)
	{
		old_gamma = cv_v_gamma.value;
		old_lightgamma = cv_lightgamma.value;
		old_brightness = cv_brightness.value;

		V_BuildGammaTables(cv_v_gamma.value);
		S_AmbientOn();
		vid_gamma_changed = 1;
		return 1;
	}

	return 0;
}

int V_DecayPunchAngle(void)
{
	extern float v_punchangle_decay[4];
	extern double host_frametime;

	double decay0;
	double decay1;
	double decay2;
	double decay3;
	float result0;
	float result1;
	float result2;
	float result3;

	decay0 = v_punchangle_decay[0] - host_frametime;
	if (decay0 <= 0.0)
		decay0 = 0.0;
	result0 = (float)decay0;

	decay1 = v_punchangle_decay[1] - host_frametime;
	v_punchangle_decay[0] = result0;
	if (decay1 <= 0.0)
		decay1 = 0.0;
	result1 = (float)decay1;

	decay2 = v_punchangle_decay[2] - host_frametime;
	v_punchangle_decay[1] = result1;
	if (decay2 <= 0.0)
		decay2 = 0.0;
	result2 = (float)decay2;

	decay3 = v_punchangle_decay[3] - host_frametime;
	v_punchangle_decay[2] = result2;
	if (decay3 <= 0.0)
		decay3 = 0.0;
	result3 = (float)decay3;

	v_punchangle_decay[3] = result3;

	return (int)result3;
}

void V_CalcViewPunch(void)
{
	extern float v_lateral_punch;
	extern float v_lateral_scale;
	extern float v_vertical_punch;
	extern float v_vertical_scale;
	extern cvar_t cv_v_kicktime;
	extern float v_dmg_time;
	extern float v_punchangle_decay[4];

	int random1;
	int random2;
	int intensity;
	double absValue;
	double absLateral;
	vec3_t forward;
	vec3_t rightVec;
	vec3_t upVec;
	vec3_t delta;
	float distance;
	float lateral;
	float vertical;

	entity_t *ent = &cl_entities[cl_viewentity];

	random1 = MSG_ReadByte();
	random2 = MSG_ReadByte();

	delta[0] = MSG_ReadCoord();
	delta[1] = MSG_ReadCoord();
	delta[2] = MSG_ReadCoord();

	vertical = (float)((double)random2 * 0.5 + (double)random1 * 0.5);
	if (vertical < 10.0f)
		vertical = 10.0f;

	const float intensityScale = vertical;

	shake_next_time = (float)((int)(cl_time + 0.2));
	shake_intensity = (int)(intensityScale * 4.0f + (float)shake_intensity);

	if (shake_intensity < 0)
		shake_intensity = 0;
	if (shake_intensity > 150)
		shake_intensity = 150;

	if (random2 >= random1)
	{
		if (random1)
		{
			shake_color_r = 220;
			intensity = 50;
		}
		else
		{
			shake_color_r = 255;
			intensity = 0;
		}
	}
	else
	{
		shake_color_r = 200;
		intensity = 100;
	}

	shake_color_g = intensity;
	shake_color_b = intensity;

	VectorSubtract(delta, ent->origin, delta);

	distance = Length(delta);
	VectorNormalize(delta);

	AngleVectors(ent->angles, forward, rightVec, upVec);

	lateral = DotProduct(rightVec, delta);
	v_lateral_punch = v_lateral_scale * intensityScale * lateral;

	vertical = DotProduct(upVec, delta);
	v_vertical_punch = intensityScale * vertical * v_vertical_scale;
	v_dmg_time = cv_v_kicktime.value;

	if (distance > 50.0f)
	{
		if (vertical <= 0.0f)
		{
			vertical = fabs(vertical);
			if (vertical > 0.3f)
			{
				absValue = v_punchangle_decay[3];
				if (vertical >= (double)v_punchangle_decay[3])
					absValue = vertical;
				v_punchangle_decay[3] = (float)absValue;
			}
		}
		else if (vertical > 0.3f)
		{
			absValue = v_punchangle_decay[2];
			if (vertical >= (double)v_punchangle_decay[2])
				absValue = vertical;
			v_punchangle_decay[2] = (float)absValue;
		}

		if (lateral <= 0.0f)
		{
			vertical = fabs(lateral);
			if (vertical > 0.3f)
			{
				absLateral = v_punchangle_decay[0];
				if (vertical >= (double)v_punchangle_decay[0])
					absLateral = vertical;
				v_punchangle_decay[0] = (float)absLateral;
			}
		}
		else if (lateral > 0.3f)
		{
			absValue = v_punchangle_decay[1];
			if (lateral >= (double)v_punchangle_decay[1])
				absValue = lateral;
			v_punchangle_decay[1] = (float)absValue;
		}
	}
	else
	{
		v_punchangle_decay[0] = 1.0f;
		v_punchangle_decay[1] = 1.0f;
		v_punchangle_decay[3] = 1.0f;
		v_punchangle_decay[2] = 1.0f;
	}
}

int CL_ClearState(void)
{
	int i;

	if (!sv.active)
		Host_ClearMemory();

	memset(&cl, 0, sizeof(cl));
	cl_time = 0.0;
	cl_oldtime = 0.0;
	cl_mtime[0] = 0.0;
	cl_mtime[1] = 0.0;
	memset(cl_model_precache, 0, sizeof(cl_model_precache));
	memset(cl_sound_precache, 0, sizeof(cl_sound_precache));
	cl_worldmodel = NULL;
	cl_scores = NULL;
	SZ_Clear(&cls.message);

	memset(cl_efrags, 0, sizeof(cl_efrags));
	memset(cl_entities, 0, sizeof(cl_entities));
	memset(cl_dlights, 0, sizeof(cl_dlights));
	memset(cl_lightstyle, 0, sizeof(cl_lightstyle));

	CL_InitTEnts();

	cl_free_efrags = cl_efrags;
	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl_efrags[i].entnext = &cl_efrags[i + 1];
	cl_efrags[MAX_EFRAGS - 1].entnext = NULL;

	cl_num_statics = 0;
	cl_num_entities = 0;

	return 0;
}
