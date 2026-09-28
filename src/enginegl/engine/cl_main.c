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

// cl_main.c  -- client main loop

#include "quakedef.h"
#include "studio.h"

#define STUDIO_HEADER_SIZE	188		// header written for a model with a bad version
#define CLIENT_MESSAGE_SIZE	1024	// reliable message buffer to the server

// these two are not intended to be set directly
cvar_t	cl_name = { "_cl_name", "player", true };
cvar_t	cl_color = { "_cl_color", "0", true };

cvar_t	cl_shownet = { "cl_shownet", "0" };	// can be 0, 1, or 2
cvar_t	cl_nolerp = { "cl_nolerp", "0" };
cvar_t	cl_lerplocal = { "cl_lerplocal", "0" };
cvar_t	cl_lerpstep = { "cl_lerpstep", "1" };
cvar_t	cl_pitchdriftspeed = { "cl_pitchdriftspeed", "0" };
cvar_t	cl_pitchdrift = { "cl_pitchdrift", "0" };

// V_UpdateIdleAnim state
int		v_idle_active = 0;
float	v_idle_time = 0;
double	v_idle_start = 0;
float	v_idle_counter = 0;
float	v_idle_max_time = 0;
float	v_idle_timeout = 0;

float	v_velocity_threshold = 0;
float	v_target_pitch = 0;

// V_CalcViewPunch screen flash
float	shake_next_time = 0;
int		shake_intensity = 0;
int		shake_color_r = 0;
int		shake_color_g = 0;
int		shake_color_b = 0;

void CL_PrintEntities_f(void);
void SCR_Screenshot_f(void);
void CL_StopMovie_f(void);

void CL_KeyDown_Null(void)
{
}

void CL_KeyUp_Null(void)
{
}

void CL_Stub(void)
{
}

/*
=================
CL_LoadSpriteModel

Loads a studio model and marks it as a sprite
=================
*/
void CL_LoadSpriteModel(model_t *mod, void *buffer)
{
	int					i;
	int					start, total;
	int					numtextures;
	studiohdr_t			*pinmodel;
	studiohdr_t			*phdr;
	mstudiotexture_t	*ptexture;
	byte				*pal;
	char				name[256];

	start = Hunk_LowMark();

	pinmodel = (studiohdr_t *)buffer;
	if (Mod_GetType(pinmodel->version) != STUDIO_VERSION)
	{
		memset(pinmodel, 0, STUDIO_HEADER_SIZE);
		strcpy(pinmodel->name, "bogus");
		pinmodel->length = STUDIO_HEADER_SIZE;
	}

	mod->type = mod_sprite;
	mod->flags = 0;

	phdr = Hunk_AllocName(pinmodel->length, "sprite");
	memcpy(phdr, buffer, pinmodel->length);

	numtextures = pinmodel->numtextures;
	ptexture = (mstudiotexture_t *)((byte *)phdr + pinmodel->textureindex);
	for (i = 0; i < numtextures; i++, ptexture++)
	{
		pal = (byte *)phdr + ptexture->index + ptexture->width * ptexture->height;

		strcpy(name, mod->name);
		strcat(name, ptexture->name);

		ptexture->index = GL_LoadTexture(name, ptexture->width, ptexture->height, (byte *)phdr + ptexture->index, 0, 0, pal);
	}

//
// move the complete, relocatable model to the cache
//
	total = Hunk_LowMark() - start;
	Cache_Alloc(&mod->cache, total, "sprite");
	if (!mod->cache.data)
		return;

	memcpy(mod->cache.data, phdr, total);
	Hunk_FreeToLowMark(start);
}

/*
=================
CL_Init
=================
*/
void CL_Init(void)
{
	SZ_Alloc(&cls.message, CLIENT_MESSAGE_SIZE);

	CL_InitInput();
	R_PrecacheWeaponSounds();

//
// register our commands
//
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

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect(void)
{
// stop sounds (especially looping!)
	S_StopAllSounds(true);

// if running a local server, shut it down
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
	cls.demoplayback = false;
	cls.timedemo = false;
}

void CL_Disconnect_f(void)
{
	CL_Disconnect();
	if (sv.active)
		Host_ShutdownServer(false);
}

/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection(char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect();

	cls.netcon = NET_Connect(host);
	if (!cls.netcon)
		Host_Error("CL_Connect: connect failed\n");
	Con_DPrintf("CL_EstablishConnection: connected to %s\n", host);

	cls.demonum = -1;			// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply(void)
{
	char	str[8192];

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
		MSG_WriteString(&cls.message, va("color %i %i\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 15));

		MSG_WriteByte(&cls.message, clc_stringcmd);
		sprintf(str, "spawn %s", cls_spawnparms);
		MSG_WriteString(&cls.message, str);
		break;

	case 3:
		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, "begin");
		Cache_Report();		// print remaining memory
		break;

	case 4:
		SCR_EndLoadingPlaque();		// allow normal screen updates

		if (cls.demoplayback)
		{
			key_dest = key_game;
			scr_con_current = 0;
			Con_ClearNotify();
		}
		break;
	}
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo(void)
{
	char	str[1024];

	if (cls.demonum == -1)
		return;		// don't play demos

	SCR_BeginLoadingPlaque();

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
		cls.demonum = 0;	// back to the start of the loop

	if (cls.demos[cls.demonum][0])
	{
		sprintf(str, "playdemo %s\n", cls.demos[cls.demonum]);
		Cbuf_InsertText(str);
		cls.demonum++;
	}
	else
	{
		Con_Printf("No demos listed with startdemos\n");
		cls.demonum = -1;
	}
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f(void)
{
	entity_t	*ent;
	int			i;

	for (i = 0, ent = cl_entities; i < cl.num_entities; i++, ent++)
	{
		Con_Printf("%3i:", i);
		if (ent->model)
		{
			Con_Printf("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
				ent->model->name, (int)ent->frame,
				ent->origin[0], ent->origin[1], ent->origin[2],
				ent->angles[0], ent->angles[1], ent->angles[2]);
		}
		else
		{
			Con_Printf("EMPTY\n");
		}
	}
}

/*
==============
SCR_Screenshot_f

Writes the next numbered snapshot, named after the current map
==============
*/
void SCR_Screenshot_f(void)
{
	char	name[64];
	char	base[64];

	if (cl.num_entities && cl.worldmodel)
		COM_FileBase(cl.worldmodel->name, base);
	else
		strcpy(base, "Snapshot");

	sprintf(name, "%s%04d.bmp", base, cl.snapshot_number++);
	VID_TakeSnapshot(name);
}

/*
==============
CL_StopMovie_f
==============
*/
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

/*
===============
CL_AllocDlight
===============
*/
dlight_t *CL_AllocDlight(int key)
{
	int			i;
	dlight_t	*dl;

// first look for an exact key match
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

// then look for anything else
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

/*
===============
CL_DecayLights
===============
*/
void CL_DecayLights(void)
{
	int			i;
	dlight_t	*dl;
	float		time;

	time = cl_time - cl_oldtime;

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
}

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
double CL_LerpPoint(void)
{
	float		f, frac;
	double		time;
	qboolean	locallerp;

	f = cl_mtime[0] - cl_mtime[1];

	if (f == 0 || cl_nolerp.value || cls.timedemo || (sv.active && !cl_lerplocal.value))
	{
		cl_time = cl_mtime[0];
		return 1;
	}

	if (f > 0.1f)
	{	// dropped packet, or start of demo
		f = 0.1f;
		cl_mtime[1] = cl_mtime[0] - 0.1;
	}

// a listen server with cl_lerplocal runs one message behind
	time = cl_time;
	locallerp = (sv.active && cl_lerplocal.value);
	if (locallerp)
		time = cl_time - f;

	frac = (time - cl_mtime[1]) / f;

	if (frac < 0)
	{
		if (frac < -0.01f && !locallerp)
			cl_time = cl_mtime[1];
		return 0;
	}

	if (frac > 1)
	{
		if (frac > 1.01f)
		{
			if (locallerp)
			{
				cl_time = cl_mtime[0] + f;
				if (cl_oldtime > cl_time)
					cl_oldtime = cl_time;
			}
			else
			{
				cl_time = cl_mtime[0];
			}
		}
		return 1;
	}

	return frac;
}

/*
===============
CL_RelinkEntities
===============
*/
void CL_RelinkEntities(void)
{
	entity_t	*ent;
	int			i, j;
	float		frac, f, d;
	vec3_t		delta;
	float		angledelta[3];
	vec3_t		oldorg;
	dlight_t	*dl;
	int			modelflags;

// determine partial update time
	frac = CL_LerpPoint();

	cl.num_visedicts = 0;
	cl_numvisedicts = 0;

//
// interpolate player info
//
	for (i = 0; i < 3; i++)
		cl.punchangle[i] = (cl.mviewangles[0][i] - cl.punchangle[i]) * frac + cl.punchangle[i];

	if (cls.demoplayback)
	{
	// interpolate the angles
		for (j = 0; j < 3; j++)
		{
			angledelta[j] = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (angledelta[j] > 180.0)
				angledelta[j] -= 360.0;
			else if (angledelta[j] < -180.0)
				angledelta[j] += 360.0;

			cl_viewangles[j] = angledelta[j] * frac + cl.mviewangles[1][j];
		}
	}

// start on the entity after the world
	for (i = 1; i < cl_num_entities; i++)
	{
		ent = &cl_entities[i];

		if (ent->msgtime != cl_mtime[0])
		{	// empty slot
			if (ent->efrag)
				R_RemoveEfrags(ent);	// just became empty
			ent->model = NULL;
			continue;
		}

		VectorCopy(ent->origin, oldorg);

		if (ent->forcelink)
		{	// the entity was not updated in the last message
			// so move to the final spot
			VectorCopy(ent->msg_origins[0], ent->origin);
			VectorCopy(ent->msg_angles[0], ent->angles);
		}
		else
		{	// if the delta is large, assume a teleport and don't lerp
			f = frac;
			for (j = 0; j < 3; j++)
			{
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
				if (delta[j] > 100.0f || delta[j] < -100.0f)
					f = 1;		// assume a teleportation, not a motion
			}

		// interpolate the origin and angles
			for (j = 0; j < 3; j++)
			{
				ent->origin[j] = ent->msg_origins[1][j] + delta[j] * f;

				d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
				if (d > 180.0f)
					d -= 360.0f;
				else if (d < -180.0f)
					d += 360.0f;
				ent->angles[j] = ent->msg_angles[1][j] + d * f;
			}
		}

		if (!ent->model)
		{
			if (ent->efrag)
				R_RemoveEfrags(ent);
			ent->forcelink = false;
			continue;
		}

	// rotate bonus items locally
		modelflags = ent->model->flags;
		if (modelflags & EF_ROTATE)
			ent->angles[1] = anglemod(cl_time * 100.0);

		if (ent->effects & EF_BRIGHTFIELD)
			R_EntityParticles(ent);

		if (ent->effects & EF_NOINTERP)
			R_DarkFieldParticles(ent->origin);

		if (ent->effects & EF_BRIGHTLIGHT)
		{
			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->origin[2] += 16.0;
			j = rand();
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 250;
			dl->radius = (j & 31) + 400;
			dl->die = cl_time + 0.001;
		}

		if (ent->effects & EF_DIMLIGHT)
		{
			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			j = rand();
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 100;
			dl->radius = (j & 31) + 200;
			dl->die = cl_time + 0.001;
		}

		if (ent->effects & EF_INVLIGHT)
		{
			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			j = rand();
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 100;
			dl->radius = (j & 31) + 200;
			dl->ramp = 1;
			dl->die = cl_time + 0.001;
		}

		if (ent->effects & EF_LIGHT)
		{
			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 100;
			dl->radius = 200;
			dl->die = cl_time + 0.001;
		}

		if (modelflags & EF_GIB)
			R_RocketTrail(oldorg, ent->origin, 2);
		else if (modelflags & EF_ZOMGIB)
			R_RocketTrail(oldorg, ent->origin, 4);
		else if (modelflags & EF_TRACER)
			R_RocketTrail(oldorg, ent->origin, 3);
		else if (modelflags & EF_TRACER2)
			R_RocketTrail(oldorg, ent->origin, 5);
		else if (modelflags & EF_ROCKET)
		{
			R_RocketTrail(oldorg, ent->origin, 0);
			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->radius = 200;
			dl->color.rgb[0] = dl->color.rgb[1] = dl->color.rgb[2] = 200;
			dl->die = cl_time + 0.01;
			dl->minlight = 200;
		}
		else if (modelflags & EF_GRENADE)
			R_RocketTrail(oldorg, ent->origin, 1);
		else if (modelflags & EF_TRACER3)
			R_RocketTrail(oldorg, ent->origin, 6);

		ent->forcelink = false;

		if (cam_thirdperson || cl_viewentity != i || chase_active.value)
		{
			if (!(ent->effects & EF_NODRAW) && cl_numvisedicts < MAX_VISEDICTS)
			{
				cl_visedicts[cl_numvisedicts++] = ent;
				cl.num_visedicts = cl_numvisedicts;
			}
		}
	}

	if (cl_entities[cl_viewentity].effects & EF_MUZZLEFLASH)
		viewent_entity.effects |= EF_MUZZLEFLASH;
}

/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
void CL_ReadFromServer(void)
{
	int		ret;

	cl_oldtime = cl_time;

// a single player game pauses while the console or the menu is up
	if (!sv.active || svs.maxclients > 1 || (key_dest != key_console && key_dest != key_menu))
		cl_time += host_frametime;

	do
	{
		ret = CL_GetMessage();
		if (ret == -1)
			Host_Error("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		cl.last_received_message = (float)realtime;
		CL_ParseServerMessage();
	} while (cls.state == ca_connected);

	if (cl_shownet.value)
		Con_Printf("\n");

//
// bring the links up to date
//
	CL_RelinkEntities();
	CL_UpdateTEnts();
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd(void)
{
	usercmd_t	cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS)
	{
	// get basic movement from keyboard
		CL_BaseMove(&cmd);

	// allow mice or other external controllers to add to the move
		IN_Move(&cmd);

	// send the unreliable message
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

/*
===============
V_StartIdleAnim
===============
*/
void V_StartIdleAnim(void)
{
	v_idle_active = 1;
	v_idle_time = 0;
	v_idle_start = cl_time;
}

/*
===============
V_StopIdleAnim
===============
*/
void V_StopIdleAnim(void)
{
	if (v_idle_start != cl_time && (v_idle_active || v_idle_time == 0))
	{
		v_idle_time = v_idle_max_time;
		v_idle_active = 0;
		v_idle_counter = 0;
	}
}

/*
===============
V_UpdateIdleAnim

Moves the view pitch toward v_target_pitch while standing still
===============
*/
void V_UpdateIdleAnim(void)
{
	float	delta;
	float	move;

	if (cl_intermission || !cl_onground || cl_dead)
	{
		v_idle_counter = 0;
		v_idle_time = 0;
	}
	else if (v_idle_active)
	{
		if (fabs(cl_forward_velocity) >= v_velocity_threshold)
			v_idle_counter += host_frametime;
		else
			v_idle_counter = 0;

		if (v_idle_counter > (double)v_idle_timeout)
			V_StopIdleAnim();
	}
	else
	{
		delta = v_target_pitch - cl_viewangles_pitch;
		if (delta == 0)
		{
			v_idle_time = 0;
		}
		else
		{
			move = host_frametime * v_idle_time;
			v_idle_time = host_frametime * v_idle_max_time + v_idle_time;

			if (delta > 0)
			{
				if (move > (double)delta)
				{
					v_idle_time = 0;
					move = v_target_pitch - cl_viewangles_pitch;
				}
				cl_viewangles_pitch += move;
			}
			else if (delta < 0)
			{
				if (-delta < (double)move)
				{
					v_idle_time = 0;
					move = -delta;
				}
				cl_viewangles_pitch -= move;
			}
		}
	}
}

/*
=================
V_BuildGammaTables

Builds the texture gamma table and the lightmap gamma table
=================
*/
void V_BuildGammaTables(float gamma)
{
	int		i;
	int		inf;
	float	g;
	float	brightness;
	double	f;

	if (gamma > 1)
	{
		if (gamma > 3)
			gamma = 3;
		g = cv_texgamma.value / gamma;

		if (cv_brightness.value > 0)
		{
			if (cv_brightness.value <= 1)
				brightness = cv_brightness.value * cv_brightness.value * -0.075f + 0.125f;
			else
				brightness = 0.05f;
		}
		else
		{
			brightness = 0.125f;
		}
	}
	else
	{
		brightness = 0.125f;
		g = gamma;
	}

	for (i = 0; i < 256; i++)
	{
		inf = pow(i / 255.0, g) * 255.0;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		gammatable[i] = inf;
	}

// the light below the brightness point covers the bottom 1/8 of the range
	for (i = 0; i < 1024; i++)
	{
		f = pow(i / 1023.0, cv_lightgamma.value);

		if (brightness <= f)
			f = (f - brightness) / (1.0 - brightness) * 0.875 + 0.125;
		else
			f = f / brightness * 0.125;

		inf = pow(f, 1.0 / gamma) * 1023.0;
		if (inf < 0)
			inf = 0;
		if (inf > 1023)
			inf = 1023;
		lightgammatable[i] = inf;
	}
}

/*
=================
V_CheckGamma
=================
*/
int V_CheckGamma(void)
{
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

/*
=================
V_DecayPunchAngle
=================
*/
void V_DecayPunchAngle(void)
{
	int		i;
	double	d;

	for (i = 0; i < 4; i++)
	{
		d = v_punchangle_decay[i] - host_frametime;
		if (d <= 0)
			d = 0;
		v_punchangle_decay[i] = d;
	}
}

/*
===============
V_CalcViewPunch

Reads an svc_damage message
===============
*/
void V_CalcViewPunch(void)
{
	int			armor, blood;
	int			shade;
	vec3_t		from;
	vec3_t		forward, right, up;
	entity_t	*ent;
	float		side, upward;
	float		count;
	float		len;
	double		punch;

	ent = &cl_entities[cl_viewentity];

	armor = MSG_ReadByte();
	blood = MSG_ReadByte();
	from[0] = MSG_ReadCoord();
	from[1] = MSG_ReadCoord();
	from[2] = MSG_ReadCoord();

	count = blood * 0.5 + armor * 0.5;
	if (count < 10)
		count = 10;

	shake_next_time = (int)(cl_time + 0.2);
	shake_intensity = count * 4 + shake_intensity;
	if (shake_intensity < 0)
		shake_intensity = 0;
	if (shake_intensity > 150)
		shake_intensity = 150;

	if (blood >= armor)
	{
		if (armor)
		{
			shake_color_r = 220;
			shade = 50;
		}
		else
		{
			shake_color_r = 255;
			shade = 0;
		}
	}
	else
	{
		shake_color_r = 200;
		shade = 100;
	}
	shake_color_g = shade;
	shake_color_b = shade;

//
// calculate view angle kicks
//
	VectorSubtract(from, ent->origin, from);
	len = Length(from);
	VectorNormalize(from);

	AngleVectors(ent->angles, forward, right, up);

	side = DotProduct(right, from);
	v_lateral_punch = v_lateral_scale * count * side;

	upward = DotProduct(up, from);
	v_vertical_punch = count * upward * v_vertical_scale;

	v_dmg_time = cv_v_kicktime.value;

	if (len > 50)
	{
		if (upward <= 0)
		{
			upward = fabs(upward);
			if (upward > 0.3f)
			{
				punch = v_punchangle_decay[3];
				if (upward >= (double)v_punchangle_decay[3])
					punch = upward;
				v_punchangle_decay[3] = punch;
			}
		}
		else if (upward > 0.3f)
		{
			punch = v_punchangle_decay[2];
			if (upward >= (double)v_punchangle_decay[2])
				punch = upward;
			v_punchangle_decay[2] = punch;
		}

		if (side <= 0)
		{
			upward = fabs(side);
			if (upward > 0.3f)
			{
				punch = v_punchangle_decay[0];
				if (upward >= (double)v_punchangle_decay[0])
					punch = upward;
				v_punchangle_decay[0] = punch;
			}
		}
		else if (side > 0.3f)
		{
			punch = v_punchangle_decay[1];
			if (side >= (double)v_punchangle_decay[1])
				punch = side;
			v_punchangle_decay[1] = punch;
		}
	}
	else
	{
		v_punchangle_decay[0] = 1;
		v_punchangle_decay[1] = 1;
		v_punchangle_decay[3] = 1;
		v_punchangle_decay[2] = 1;
	}
}

/*
=====================
CL_ClearState
=====================
*/
void CL_ClearState(void)
{
	int		i;

	if (!sv.active)
		Host_ClearMemory();

// wipe the entire cl structure
	memset(&cl, 0, sizeof(cl));
	cl_time = 0;
	cl_oldtime = 0;
	cl_mtime[0] = 0;
	cl_mtime[1] = 0;
	memset(cl_model_precache, 0, sizeof(cl_model_precache));
	memset(cl_sound_precache, 0, sizeof(cl_sound_precache));
	cl_worldmodel = NULL;
	cl_scores = NULL;

	SZ_Clear(&cls.message);

// clear other arrays
	memset(cl_efrags, 0, sizeof(cl_efrags));
	memset(cl_entities, 0, sizeof(cl_entities));
	memset(cl_dlights, 0, sizeof(cl_dlights));
	memset(cl_lightstyle, 0, sizeof(cl_lightstyle));

	CL_InitTEnts();

//
// allocate the efrags and chain together into a free list
//
	cl_free_efrags = cl_efrags;
	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl_efrags[i].entnext = &cl_efrags[i + 1];
	cl_efrags[i].entnext = NULL;

	cl_num_statics = 0;
	cl_num_entities = 0;
}
