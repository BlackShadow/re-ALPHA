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
#include "progdefs.h"
#include "server.h"
#include "model.h"
#include "eiface.h"
#include <ctype.h>

extern int pr_argc;
extern dfunction_t *pr_xfunction;
extern char pr_string_temp[2048];
extern void *pr_functions;
extern void *progs;

static byte checkpvs[MAX_MAP_LEAFS / 8];

static edict_t *PF_find_Shared(int startEntNum, int fieldOffset, const char *szValue);

void PF_Fixme(void)
{
	PR_RunError("unimplemented bulitin");
}

char *PF_VarString(int first)
{
	static char pr_string_temp[2048];
	int i;
	char *s;
	unsigned int length;

	pr_string_temp[0] = 0;

	for (i = first; i < pr_argc; i++)
	{
		s = G_STRING(OFS_PARM0 + i * 3);
		length = strlen(s) + 1;
		memcpy(&pr_string_temp[strlen(pr_string_temp)], s, length);
	}

	return pr_string_temp;
}

void PF_error(void)
{
	char *s;
	edict_t *ed;

	s = PF_VarString(0);
	Con_Printf("======SERVER ERROR in %s:\n%s\n",
		pr_strings + pr_xfunction->s_name, s);

	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print(ed);

	Host_Error("Program error");
}

void PF_objerror(void)
{
	char *s;
	edict_t *ed;

	s = PF_VarString(0);
	Con_Printf("======OBJECT ERROR in %s:\n%s\n",
		pr_strings + pr_xfunction->s_name, s);

	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print(ed);
	ED_Free(ed);

	Host_Error("Program error");
}

void PF_makevectors_I(float *angles)
{
	AngleVectors(angles, gGlobalVariables.v_forward, gGlobalVariables.v_right, gGlobalVariables.v_up);
}

void PF_makevectors(void)
{
	PF_makevectors_I(G_VECTOR(OFS_PARM0));
}

void PF_setorigin_I(edict_t *e, float *org)
{
	VectorCopy(org, e->v.origin);
	SV_LinkEdict(e, 0);
}

void PF_setorigin(void)
{
	edict_t *e;
	float *org;

	e = G_EDICT(OFS_PARM0);
	org = G_VECTOR(OFS_PARM1);
	PF_setorigin_I(e, org);
}

void SetMinMaxSize(edict_t *e, float *min, float *max)
{
	int i;

	for (i = 0; i < 3; i++)
		if (min[i] > max[i])
			PR_RunError("backwards mins/maxs");

	VectorCopy(min, e->v.mins);
	VectorCopy(max, e->v.maxs);
	VectorSubtract(max, min, e->v.size);

	SV_LinkEdict(e, 0);
}

void PF_setsize_I(edict_t *e, float *min, float *max)
{
	SetMinMaxSize(e, min, max);
}

void PF_setsize(void)
{
	edict_t *e;
	float *min, *max;

	e = G_EDICT(OFS_PARM0);
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	PF_setsize_I(e, min, max);
}

void PF_setmodel_I(edict_t *e, const char *m)
{
	int i;
	model_t *mod;

	for (i = 0; i < MAX_MODELS && sv.model_precache[i]; i++)
	{
		if (!strcmp(sv.model_precache[i], m))
			break;
	}

	if (!sv.model_precache[i])
		PR_RunError("no precache: %s", m);

	e->v.model = m - pr_strings;
	e->v.modelindex = i;

	mod = sv.models[(int)e->v.modelindex];

	if (mod)
		SetMinMaxSize(e, mod->mins, mod->maxs);
	else
		SetMinMaxSize(e, vec3_origin, vec3_origin);
}

int PF_modelindex(const char *name)
{
	return SV_ModelIndex((char *)name);
}

void PF_setmodel(void)
{
	edict_t *e;
	char *m;

	e = G_EDICT(OFS_PARM0);
	m = G_STRING(OFS_PARM1);
	PF_setmodel_I(e, m);
}

void PF_bprint(void)
{
	char *s;

	s = PF_VarString(0);
	SV_BroadcastPrintf("%s", s);
}

void PF_sprint(void)
{
	char *s;
	server_client_t *client;
	int entnum;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];
	MSG_WriteChar(&client->message, svc_print);
	MSG_WriteString(&client->message, s);
}

void PF_centerprint_I(edict_t *ent, const char *s)
{
	server_client_t *client;
	int entnum;

	entnum = NUM_FOR_EDICT(ent);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];
	MSG_WriteByte(&client->message, svc_centerprint);
	MSG_WriteString(&client->message, s);
}

void PF_normalize(void)
{
	float *value1;
	vec3_t newvalue;
	float new;

	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2] * value1[2];
	new = sqrt(new);

	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		new = 1 / new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}

	VectorCopy(newvalue, G_VECTOR(OFS_RETURN));
}

void PF_vlen(void)
{
	float *value1;
	float new;

	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2] * value1[2];
	new = sqrt(new);

	G_FLOAT(OFS_RETURN) = new;
}

float VectorToYaw(float *value1)
{
	float yaw;

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
	}
	else
	{
		yaw = (int)(atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	return yaw;
}

void PF_vectoyaw(void)
{
	float *value1;
	float yaw;

	value1 = G_VECTOR(OFS_PARM0);

	yaw = VectorToYaw(value1);

	G_FLOAT(OFS_RETURN) = yaw;
}

void VectorAngles(float *forward, float *angles)
{
	float tmp, yaw, pitch;

	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (int)(atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
		pitch = (int)(atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

void PF_vectoangles(void)
{
	float *value1;

	value1 = G_VECTOR(OFS_PARM0);

	VectorAngles(value1, G_VECTOR(OFS_RETURN));
}

void PF_random(void)
{
	float num;

	num = (rand() & 0x7FFF) / ((float)0x7FFF);

	G_FLOAT(OFS_RETURN) = num;
}

static void SV_WriteParticleEffect(float *org, float *dir, char color, char count)
{
	int i;
	int byteVal;

	if (sv.datagram.cursize <= 1008)
	{
		MSG_WriteByte(&sv.datagram, svc_particle);
		MSG_WriteCoord(&sv.datagram, org[0]);
		MSG_WriteCoord(&sv.datagram, org[1]);
		MSG_WriteCoord(&sv.datagram, org[2]);

		for (i = 0; i < 3; i++)
		{
			byteVal = (int)(dir[i] * 16.0f);
			if (byteVal > 127)
				byteVal = 127;
			else if (byteVal < -128)
				byteVal = -128;
			MSG_WriteChar(&sv.datagram, byteVal);
		}

		MSG_WriteByte(&sv.datagram, count);
		MSG_WriteByte(&sv.datagram, color);
	}
}

void SV_StartParticle(float *org, float *dir, float color, float count)
{
	SV_WriteParticleEffect(org, dir, (char)(int)color, (char)(int)count);
}

void PF_particle(void)
{
	float *org, *dir;
	float color;
	float count;

	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);
	SV_StartParticle(org, dir, color, count);
}

void PF_ambientsound_I(float *pos, const char *samp, float vol, float attenuation)
{
	int i, soundnum;

	for (soundnum = 0; soundnum < MAX_SOUNDS && sv.sound_precache[soundnum]; soundnum++)
	{
		if (!strcmp(sv.sound_precache[soundnum], samp))
			break;
	}

	if (!sv.sound_precache[soundnum])
	{
		Con_Printf("no precache: %s\n", samp);
		return;
	}

	MSG_WriteByte(&sv.signon, svc_spawnstaticsound);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte(&sv.signon, soundnum);

	MSG_WriteByte(&sv.signon, vol * 255);
	MSG_WriteByte(&sv.signon, attenuation * 64);
}

void PF_ambientsound(void)
{
	char *samp;
	float *pos;
	float vol, attenuation;

	pos = G_VECTOR(OFS_PARM0);
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

	PF_ambientsound_I(pos, samp, vol, attenuation);
}

void PF_sound_I(edict_t *entity, int channel, const char *sample, float volume, float attenuation)
{
	if (volume < 0.0f || volume > 255.0f)
	{
		Sys_Error("SV_StartSound: volume = %i", (int)volume);
	}

	if (attenuation < 0.0f || attenuation > 4.0f)
	{
		Sys_Error("SV_StartSound: attenuation = %f", attenuation);
	}

	if ((unsigned int)channel >= 8)
	{
		Sys_Error("SV_StartSound: channel = %i", channel);
	}

	SV_StartSound(entity, channel, sample, (int)(volume * 255.0f), attenuation);
}

void PF_sound(void)
{
	edict_t *entity;
	int channel;
	char *sample;
	float volume;
	float attenuation;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3);
	attenuation = G_FLOAT(OFS_PARM4);

	PF_sound_I(entity, channel, sample, volume, attenuation);
}

void PF_break(void)
{
	Con_Printf("break statement\n");
	*(int *)-4 = 0;
}

void PF_traceline_Shared(float *v1, float *v2, int nomonsters, edict_t *ent)
{
	trace_t trace;

	trace = SV_Move(v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	gGlobalVariables.trace_allsolid = trace.allsolid;
	gGlobalVariables.trace_startsolid = trace.startsolid;
	gGlobalVariables.trace_fraction = trace.fraction;
	gGlobalVariables.trace_inwater = trace.inwater;
	gGlobalVariables.trace_inopen = trace.inopen;
	VectorCopy(trace.endpos, gGlobalVariables.trace_endpos);
	VectorCopy(trace.plane.normal, gGlobalVariables.trace_plane_normal);
	gGlobalVariables.trace_plane_dist = trace.plane.dist;
	if (trace.ent)
		gGlobalVariables.trace_ent = EDICT_TO_PROG(trace.ent);
	else
		gGlobalVariables.trace_ent = 0;
}

void PF_traceline_DLL(float *v1, float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
{
	if (!pentToSkip)
		pentToSkip = sv_edicts;

	PF_traceline_Shared(v1, v2, fNoMonsters, pentToSkip);

	ptr->fAllSolid = gGlobalVariables.trace_allsolid;
	ptr->fStartSolid = gGlobalVariables.trace_startsolid;
	ptr->fInOpen = gGlobalVariables.trace_inopen;
	ptr->fInWater = gGlobalVariables.trace_inwater;
	ptr->flFraction = gGlobalVariables.trace_fraction;
	VectorCopy(gGlobalVariables.trace_endpos, ptr->vecEndPos);
	ptr->flPlaneDist = gGlobalVariables.trace_plane_dist;
	VectorCopy(gGlobalVariables.trace_plane_normal, ptr->vecPlaneNormal);
	ptr->pHit = gGlobalVariables.trace_ent;
}

void PF_traceline(void)
{
	float *v1, *v2;
	int nomonsters;
	edict_t *ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	PF_traceline_Shared(v1, v2, nomonsters, ent);
}

void PF_TraceToss_Shared(edict_t *ent, edict_t *ignore)
{
	trace_t trace;

	SV_Trace_Toss(&trace, ent, ignore);

	gGlobalVariables.trace_allsolid = trace.allsolid;
	gGlobalVariables.trace_startsolid = trace.startsolid;
	gGlobalVariables.trace_fraction = trace.fraction;
	gGlobalVariables.trace_inwater = trace.inwater;
	gGlobalVariables.trace_inopen = trace.inopen;
	VectorCopy(trace.endpos, gGlobalVariables.trace_endpos);
	VectorCopy(trace.plane.normal, gGlobalVariables.trace_plane_normal);
	gGlobalVariables.trace_plane_dist = trace.plane.dist;
	if (trace.ent)
		gGlobalVariables.trace_ent = EDICT_TO_PROG(trace.ent);
	else
		gGlobalVariables.trace_ent = 0;
}

void PF_TraceToss_DLL(edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr)
{
	if (!pentToIgnore)
		pentToIgnore = sv_edicts;

	PF_TraceToss_Shared(pent, pentToIgnore);

	ptr->fAllSolid = gGlobalVariables.trace_allsolid;
	ptr->fStartSolid = gGlobalVariables.trace_startsolid;
	ptr->fInOpen = gGlobalVariables.trace_inopen;
	ptr->fInWater = gGlobalVariables.trace_inwater;
	ptr->flFraction = gGlobalVariables.trace_fraction;
	VectorCopy(gGlobalVariables.trace_endpos, ptr->vecEndPos);
	ptr->flPlaneDist = gGlobalVariables.trace_plane_dist;
	VectorCopy(gGlobalVariables.trace_plane_normal, ptr->vecPlaneNormal);
	ptr->pHit = gGlobalVariables.trace_ent;
}

void PF_TraceToss(void)
{
	edict_t *ent;
	edict_t *ignore;

	ent = G_EDICT(OFS_PARM0);
	ignore = G_EDICT(OFS_PARM1);

	PF_TraceToss_Shared(ent, ignore);
}

void PF_checkpos(void)
{

	;
}

int PF_newcheckclient(int check)
{
	int i;
	byte *pvs;
	edict_t *ent;
	mleaf_t *leaf;
	vec3_t org;

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;

	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for (; ; i++)
	{
		if (i == svs.maxclients + 1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;

		if (ent->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int)ent->v.flags & FL_NOTARGET)
			continue;

		break;
	}

	VectorAdd(ent->v.origin, ent->v.view_ofs, org);
	leaf = Mod_PointInLeaf(org, sv.worldmodel);
	pvs = Mod_LeafPVS(leaf, sv.worldmodel);

	memcpy(checkpvs, pvs, (sv.worldmodel->numleafs + 7) >> 3);

	return i;
}

edict_t *PF_checkclient_I(void)
{
	edict_t *ent, *self;
	mleaf_t *leaf;
	int l;
	vec3_t view;

	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient(sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

	ent = EDICT_NUM(sv.lastcheck);
	if (ent->free || ent->v.health <= 0)
	{
		return sv.edicts;
	}

	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd(self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf(view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if ((l < 0) || !(checkpvs[l >> 3] & (1 << (l & 7))))
	{
		return sv.edicts;
	}

	return ent;
}

void PF_checkclient(void)
{
	edict_t *ent;

	ent = PF_checkclient_I();
	RETURN_EDICT(ent);
}

void PF_stuffcmd_I(edict_t *pEdict, char *szFmt, ...)
{
	va_list argptr;
	char szOut[1024];
	int entnum;
	server_client_t *oldhost;

	entnum = NUM_FOR_EDICT(pEdict);

	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError("stuffcmd: supplied bad client");

	va_start(argptr, szFmt);
	vsprintf(szOut, szFmt, argptr);
	va_end(argptr);

	oldhost = host_client;
	host_client = &svs.clients[entnum - 1];
	SV_ClientCommand("%s", szOut);
	host_client = oldhost;
}

void PF_stuffcmd(void)
{
	int entnum;
	char *str;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError("stuffcmd: supplied bad client");

	str = G_STRING(OFS_PARM1);
	PF_stuffcmd_I(EDICT_NUM(entnum), str);
}

void PF_localcmd_I(char *str)
{
	Cbuf_AddText(str);
}

void PF_localcmd(void)
{
	char *str;

	str = G_STRING(OFS_PARM0);
	Cbuf_AddText(str);
}

void PF_cvar(void)
{
	char *str;

	str = G_STRING(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = Cvar_VariableValue(str);
}

void PF_cvar_set(void)
{
	char *var, *val;

	var = G_STRING(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	Cvar_Set(var, val);
}

edict_t *FindEntityInSphere(vec3_t org, float rad)
{
	edict_t *chain;
	int entIndex;
	float radiusSq;

	chain = sv_edicts;
	entIndex = 1;
	radiusSq = rad * rad;

	for (; entIndex < *sv_num_edicts; entIndex++)
	{
		edict_t *ent;
		float distSq;
		int axis;

		ent = (edict_t *)((byte *)sv_edicts + entIndex * pr_edict_size);

		if (ent->free)
			continue;

		if (ent->v.solid == 0.0f)
			continue;

		distSq = 0.0f;
		for (axis = 0; axis < 3; axis++)
		{
			float center;
			float delta;

			center = ent->v.origin[axis] + (ent->v.mins[axis] + ent->v.maxs[axis]) * 0.5f;
			delta = org[axis] - center;
			distSq += delta * delta;
		}

		if (radiusSq >= distSq)
		{
			ent->v.chain = EDICT_TO_PROG(chain);
			chain = ent;
		}
	}

	return chain;
}

void PF_findradius(void)
{
	edict_t *ent;
	float *org;
	float rad;

	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);

	ent = FindEntityInSphere(org, rad);

	RETURN_EDICT(ent);
}

void PF_dprint(void)
{
	Con_DPrintf("%s", PF_VarString(0));
}

void PF_ftos(void)
{
	float v;
	static char pr_string_temp[64];

	v = G_FLOAT(OFS_PARM0);

	if (v == (int)v)
		sprintf(pr_string_temp, "%d", (int)v);
	else
		sprintf(pr_string_temp, "%5.1f", v);

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_fabs(void)
{
	float v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabs(v);
}

void PF_vtos(void)
{
	static char pr_string_temp[64];

	sprintf(pr_string_temp, "'%5.1f %5.1f %5.1f'",
		G_VECTOR(OFS_PARM0)[0],
		G_VECTOR(OFS_PARM0)[1],
		G_VECTOR(OFS_PARM0)[2]);

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_etos(void)
{
	static char pr_string_temp[16];

	sprintf(pr_string_temp, "entity %i", G_EDICTNUM(OFS_PARM0));
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

edict_t *PF_Spawn_I(void)
{
	return ED_Alloc();
}

void PF_Spawn(void)
{
	edict_t *ed;

	ed = PF_Spawn_I();
	RETURN_EDICT(ed);
}

void PF_Remove_I(edict_t *ed)
{
	ED_Free(ed);
}

void PF_Remove(void)
{
	PF_Remove_I(G_EDICT(OFS_PARM0));
}

int iGetIndex(const char *pszField)
{
	char fieldLower[512];
	char *p;

	if (!pszField)
		return -1;

	strncpy(fieldLower, pszField, sizeof(fieldLower) - 1);
	fieldLower[sizeof(fieldLower) - 1] = '\0';

	for (p = fieldLower; *p; p++)
		*p = (char)tolower((unsigned char)*p);

	if (!strcmp(fieldLower, "classname"))
		return 124;
	if (!strcmp(fieldLower, "model"))
		return 128;
	if (!strcmp(fieldLower, "weaponmodel"))
		return 280;
	if (!strcmp(fieldLower, "netname"))
		return 372;
	if (!strcmp(fieldLower, "target"))
		return 436;
	if (!strcmp(fieldLower, "targetname"))
		return 440;
	if (!strcmp(fieldLower, "message"))
		return 472;
	if (!strcmp(fieldLower, "noise"))
		return 480;
	if (!strcmp(fieldLower, "noise1"))
		return 484;
	if (!strcmp(fieldLower, "noise2"))
		return 488;
	if (!strcmp(fieldLower, "noise3"))
		return 492;

	return -1;
}

edict_t *FindEntityByString(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue)
{
	int fieldOffset;
	int startEntNum;

	fieldOffset = iGetIndex(pszField);
	if (fieldOffset == -1 || !pszValue)
		return NULL;

	if (pEdictStartSearchAfter)
		startEntNum = NUM_FOR_EDICT(pEdictStartSearchAfter);
	else
		startEntNum = 0;

	return PF_find_Shared(startEntNum, fieldOffset, pszValue);
}

static edict_t *PF_find_Shared(int startEntNum, int fieldOffset, const char *szValue)
{
	edict_t *lastMatch;
	edict_t *secondMatch;
	edict_t *firstMatch;
	int i;

	lastMatch = sv.edicts;
	secondMatch = sv.edicts;
	firstMatch = sv.edicts;

	for (i = startEntNum + 1; i < sv.num_edicts; i++)
	{
		edict_t *ent;
		int stringOfs;
		const char *s;

		ent = EDICT_NUM(i);
		if (ent->free)
			continue;

		stringOfs = *(int *)((byte *)ent + 120 + fieldOffset);
		s = pr_strings + stringOfs;

		if (!strcmp(s, szValue))
		{
			if (sv.edicts == firstMatch)
			{
				firstMatch = ent;
			}
			else if (sv.edicts == secondMatch)
			{
				secondMatch = ent;
			}

			*(int *)((byte *)ent + 440) = (int)((byte *)lastMatch - (byte *)sv.edicts);
			lastMatch = ent;
		}
	}

	if (lastMatch != firstMatch)
	{
		int nextOffset;

		if (lastMatch == secondMatch)
			nextOffset = (int)((byte *)lastMatch - (byte *)sv.edicts);
		else
			nextOffset = *(int *)((byte *)lastMatch + 440);

		*(int *)((byte *)firstMatch + 440) = nextOffset;
		*(int *)((byte *)lastMatch + 440) = 0;

		if (secondMatch && lastMatch != secondMatch)
			*(int *)((byte *)secondMatch + 440) = (int)((byte *)lastMatch - (byte *)sv.edicts);
	}

	return firstMatch;
}

void PF_Find(void)
{
	int startEntNum;
	int fieldOffset;
	const char *value;
	edict_t *found;

	startEntNum = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
	fieldOffset = G_INT(OFS_PARM1);
	value = G_STRING(OFS_PARM2);
	if (!value)
		PR_RunError("PF_Find: bad search string");

	found = PF_find_Shared(startEntNum, fieldOffset, value);
	RETURN_EDICT(found);
}

int GetEntityIllum(edict_t *pEnt)
{
	int entnum;
	int *illum;

	if (!pEnt)
		return -1;

	entnum = NUM_FOR_EDICT(pEnt);
	illum = (int *)((byte *)cl_entities + sizeof(cl_entities[0]) * entnum);

	return (illum[0] + illum[1] + illum[2]) / 3;
}

void PR_CheckEmptyString(char *s)
{
	if (s[0] <= ' ')
		PR_RunError("Bad string");
}

void PF_precache_file(void)
{

	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound_internal(const char *s)
{
	int i;

	if (sv.state)
		PR_RunError("PF_Precache: precache only valid during spawn functions");

	PR_CheckEmptyString((char *)s);

	for (i = 0; i < MAX_SOUNDS; i++)
	{
		if (!sv.sound_precache[i])
		{
			sv.sound_precache[i] = (char *)s;
			return;
		}

		if (!strcmp(sv.sound_precache[i], s))
			return;
	}

	PR_RunError("PF_precache_sound: overflow");
}

void PF_precache_sound(void)
{
	PF_precache_sound_internal(G_STRING(OFS_PARM0));
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

int PF_precache_model_internal(const char *s)
{
	int i;

	if (sv.state)
		PR_RunError("PF_Precache: precache only valid during spawn functions");

	PR_CheckEmptyString((char *)s);

	for (i = 0; i < MAX_MODELS; i++)
	{
		if (!sv.model_precache[i])
		{
			sv.model_precache[i] = (char *)s;
			sv.models[i] = Mod_ForName((char *)s, true);
			return i;
		}

		if (!strcmp(sv.model_precache[i], s))
			return i;
	}

	PR_RunError("PF_precache_model: overflow");
	return 0;
}

void PF_precache_model(void)
{
	PF_precache_model_internal(G_STRING(OFS_PARM0));
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_eprint(void)
{
	ED_Print(G_EDICT(OFS_PARM0));
}

float PF_walkmove_I(edict_t *ent, float yaw, float dist)
{
	vec3_t move;
	dfunction_t *saved_xfunction;
	int saved_self;
	float result;

	if (((int)ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)) == 0)
		return 0.0f;

	yaw = (float)(yaw * (M_PI * 2.0) / 360.0);

	move[0] = cos(yaw) * dist;
	move[1] = sin(yaw) * dist;
	move[2] = 0.0f;

	saved_xfunction = pr_xfunction;
	saved_self = pr_global_struct->self;

	result = (float)SV_movestep(ent, move, 1);

	pr_xfunction = saved_xfunction;
	pr_global_struct->self = saved_self;

	return result;
}

void PF_walkmove(void)
{
	edict_t *ent;
	float yaw, dist;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);

	G_FLOAT(OFS_RETURN) = PF_walkmove_I(ent, yaw, dist);
}

float PF_droptofloor_I(edict_t *ent)
{
	trace_t trace;
	vec3_t end;

	VectorCopy(ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

	if (trace.fraction == 1 || trace.allsolid)
		return 0.0f;
	else
	{
		VectorCopy(trace.endpos, ent->v.origin);
		SV_LinkEdict(ent, false);
		ent->v.flags = (float)((int)ent->v.flags | FL_ONGROUND);
		ent->v.groundentity = EDICT_TO_PROG(trace.ent);
		return 1.0f;
	}
}

void PF_droptofloor(void)
{
	edict_t *ent;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	G_FLOAT(OFS_RETURN) = PF_droptofloor_I(ent);
}

char *SV_DecalSetName(int decal, const char *name)
{
	char *dest;

	dest = sv_decalnames[decal];
	strncpy(dest, name, 15);
	dest[15] = 0;

	if (sv_decalnamecount < decal + 1)
		sv_decalnamecount = decal + 1;

	return dest;
}

void SV_BroadcastLightStyle(int style, const char *value)
{
	int i;
	server_client_t *client;

	sv.lightstyles[style] = (char *)value;

	if (sv.state == ss_active)
	{
		client = svs.clients;
		for (i = 0; i < svs.maxclients; i++, client++)
		{
			if (client->active || client->spawned)
			{
				MSG_WriteByte(&client->message, svc_lightstyle);
				MSG_WriteByte(&client->message, style);
				MSG_WriteString(&client->message, (char *)value);
			}
		}
	}
}

void PF_lightstyle(void)
{
	int style;
	char *value;

	style = (int)G_FLOAT(OFS_PARM0);
	value = G_STRING(OFS_PARM1);

	SV_BroadcastLightStyle(style, value);
}

void PF_rint(void)
{
	float f;

	f = G_FLOAT(OFS_PARM0);
	if (f <= 0.0f)
		G_FLOAT(OFS_RETURN) = (float)(int)(f - 0.5f);
	else
		G_FLOAT(OFS_RETURN) = (float)(int)(f + 0.5f);
}

void PF_floor(void)
{
	G_FLOAT(OFS_RETURN) = (float)floor(G_FLOAT(OFS_PARM0));
}

void PF_ceil(void)
{
	G_FLOAT(OFS_RETURN) = (float)ceil(G_FLOAT(OFS_PARM0));
}

int PF_checkbottom_I(edict_t *ent)
{
	return SV_CheckBottom(ent);
}

void PF_checkbottom(void)
{
	edict_t *ent;

	ent = G_EDICT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = (float)PF_checkbottom_I(ent);
}

int PF_pointcontents_I(vec3_t p)
{
	return SV_PointContents(p);
}

void PF_pointcontents(void)
{
	G_FLOAT(OFS_RETURN) = (float)PF_pointcontents_I(G_VECTOR(OFS_PARM0));
}

void PF_nextent(void)
{
	int i;
	edict_t *ent;

	i = G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		i++;
		if (i == sv.num_edicts)
		{
			RETURN_EDICT(sv.edicts);
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->free)
		{
			RETURN_EDICT(ent);
			return;
		}
	}
}

void PF_aim_I(edict_t *ent, float speed, float *dir)
{
	edict_t *check, *bestent;
	vec3_t start, end, dir_vec;
	int i, j;
	trace_t tr;
	float dist, bestdist;

	VectorCopy(ent->v.origin, start);
	start[2] += 20;

	VectorCopy(pr_global_struct->v_forward, dir_vec);
	VectorMA(start, 2048, dir_vec, end);
	tr = SV_Move(start, vec3_origin, vec3_origin, end, 0, ent);
	if (tr.ent && ((edict_t *)tr.ent)->v.takedamage == DAMAGE_AIM
		&& (!teamplay.value || ent->v.team <= 0 || ent->v.team != ((edict_t *)tr.ent)->v.team))
	{
		VectorCopy(pr_global_struct->v_forward, dir);
		return;
	}

	VectorCopy(dir_vec, dir);
	bestdist = sv_aim.value;
	bestent = NULL;

	check = NEXT_EDICT(sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, check = NEXT_EDICT(check))
	{
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;
		for (j = 0; j < 3; j++)
			end[j] = check->v.origin[j]
			+ 0.5 * (check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract(end, start, dir_vec);
		VectorNormalize(dir_vec);
		dist = DotProduct(dir_vec, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;
		tr = SV_Move(start, vec3_origin, vec3_origin, end, 0, ent);
		if (tr.ent == check)
		{

			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent)
	{
		VectorSubtract(bestent->v.origin, ent->v.origin, dir_vec);
		dist = DotProduct(dir_vec, pr_global_struct->v_forward);
		VectorScale(pr_global_struct->v_forward, dist, end);
		end[2] = dir_vec[2];
		VectorNormalize(end);
		VectorCopy(end, dir);
	}
	else
	{
		VectorCopy(pr_global_struct->v_forward, dir);
	}
}

void PF_aim(void)
{
	edict_t *ent;
	float speed;

	ent = G_EDICT(OFS_PARM0);
	speed = G_FLOAT(OFS_PARM1);

	PF_aim_I(ent, speed, G_VECTOR(OFS_RETURN));
}

static sizebuf_t *WriteDest(int dest)
{
	edict_t *ent;
	int entnum;

	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > svs.maxclients)
			PR_RunError("WriteDest: not a client");
		return &svs.clients[entnum - 1].message;

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		return &sv.signon;

	default:
		PR_RunError("WriteDest: bad destination");
		break;
	}

	return NULL;
}

static sizebuf_t *GetWriteDest(void)
{
	return WriteDest((int)G_FLOAT(OFS_PARM0));
}

void PF_WriteByte(void)
{
	MSG_WriteByte(GetWriteDest(), (int)G_FLOAT(OFS_PARM1));
}

void PF_WriteChar(void)
{
	MSG_WriteChar(GetWriteDest(), (int)G_FLOAT(OFS_PARM1));
}

void PF_WriteShort(void)
{
	MSG_WriteShort(GetWriteDest(), (int)G_FLOAT(OFS_PARM1));
}

void PF_WriteLong(void)
{
	MSG_WriteLong(GetWriteDest(), (int)G_FLOAT(OFS_PARM1));
}

void PF_WriteAngle(void)
{
	MSG_WriteAngle(GetWriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteCoord(void)
{
	MSG_WriteCoord(GetWriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteString(void)
{
	MSG_WriteString(GetWriteDest(), G_STRING(OFS_PARM1));
}

void PF_WriteEntity(void)
{
	MSG_WriteShort(GetWriteDest(), NUM_FOR_EDICT(G_EDICT(OFS_PARM1)));
}

byte *MSG_WriteByte_Dest(int dest, int c)
{
	return MSG_WriteByte(WriteDest(dest), c);
}

byte *MSG_WriteChar_Dest(int dest, int c)
{
	return MSG_WriteChar(WriteDest(dest), c);
}

short *MSG_WriteShort_Dest(int dest, int i)
{
	return MSG_WriteShort(WriteDest(dest), i);
}

int *MSG_WriteLong_Dest(int dest, int i)
{
	return MSG_WriteLong(WriteDest(dest), i);
}

byte *MSG_WriteAngle_Dest(int dest, float f)
{
	return MSG_WriteAngle(WriteDest(dest), f);
}

short *MSG_WriteCoord_Dest(int dest, float f)
{
	return MSG_WriteCoord(WriteDest(dest), f);
}

int MSG_WriteString_Dest(int dest, char *s)
{
	return MSG_WriteString(WriteDest(dest), s);
}

short *MSG_WriteEntity_Dest(int dest, int i)
{
	return MSG_WriteShort(WriteDest(dest), i);
}

void PF_makestatic_I(edict_t *ent)
{
	float *org;
	int modelindex;
	int i;

	MSG_WriteByte(&sv.signon, svc_spawnstatic);

	modelindex = SV_ModelIndex(pr_strings + *(int *)((byte *)ent + 248));
	MSG_WriteByte(&sv.signon, modelindex);
	MSG_WriteByte(&sv.signon, *(int *)((byte *)ent + 276));
	MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 284));
	MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 504));
	MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 252));

	org = (float *)((byte *)ent + 160);
	for (i = 0; i < 3; i++)
	{
		MSG_WriteCoord(&sv.signon, org[i]);
		MSG_WriteAngle(&sv.signon, org[9 + i]);
	}

	MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 360));
	if (*(float *)((byte *)ent + 360) != 0.0f)
	{
		MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 364));
		MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 368));
		MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 372));
		MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 376));
		MSG_WriteByte(&sv.signon, (int)*(float *)((byte *)ent + 380));
	}

	ED_Free(ent);
}

void PF_makestatic(void)
{
	PF_makestatic_I(G_EDICT(OFS_PARM0));
}

int PF_setspawnparms(edict_t *ent)
{
	int entnum;

	entnum = NUM_FOR_EDICT(ent);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError("Entity is not a client");

	memcpy(&pr_global_struct->parm1, svs.clients[entnum - 1].spawn_parms, sizeof(svs.clients[entnum - 1].spawn_parms));
	return (int)svs.clients;
}

extern void PR_trace_on(void);
extern void PR_trace_off(void);
extern int PF_MoveToGoal(void);
extern void PF_changeyaw(void);
extern int SV_EntityFirstChangeLevelCallback(void);

void (*pr_builtins[])(void) = {
	PF_Fixme,
	PF_makevectors,
	PF_setorigin,
	PF_setmodel,
	PF_setsize,
	PF_Fixme,
	PF_break,
	PF_random,
	PF_sound,
	PF_normalize,
	PF_error,
	PF_objerror,
	PF_vlen,
	PF_vectoyaw,
	PF_Spawn,
	PF_Remove,
	PF_traceline,
	PF_checkclient,
	PF_Find,
	PF_precache_sound,
	PF_precache_model,
	PF_stuffcmd,
	PF_findradius,
	PF_bprint,
	PF_sprint,
	PF_dprint,
	PF_ftos,
	PF_vtos,
	PF_etos,
	PR_trace_on,
	PR_trace_off,
	PF_eprint,
	PF_walkmove,
	PF_Fixme,
	PF_droptofloor,
	PF_lightstyle,
	PF_rint,
	PF_floor,
	PF_ceil,
	PF_Fixme,
	PF_checkbottom,
	PF_pointcontents,
	PF_Fixme,
	PF_fabs,
	PF_aim,
	PF_cvar,
	PF_localcmd,
	PF_nextent,
	PF_particle,
	PF_changeyaw,
	PF_Fixme,
	PF_vectoangles,
	PF_WriteByte,
	PF_WriteChar,
	PF_WriteShort,
	PF_WriteLong,
	PF_WriteCoord,
	PF_WriteAngle,
	PF_WriteString,
	PF_WriteEntity,

	PF_Fixme,
	PF_Fixme,
	PF_Fixme,
	PF_Fixme,
	PF_Fixme,
	PF_Fixme,
	PF_Fixme,

	(void (*)(void))PF_MoveToGoal,
	PF_precache_file,
	PF_makestatic,
	PF_Fixme,
	PF_Fixme,
	PF_cvar_set,
	PF_Fixme,
	PF_ambientsound,
	PF_precache_model,
	PF_precache_sound,
	PF_precache_file,
	(void (*)(void))SV_EntityFirstChangeLevelCallback,
};

int pr_numbuiltins = sizeof(pr_builtins) / sizeof(pr_builtins[0]);
