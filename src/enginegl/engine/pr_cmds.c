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
#include "eiface.h"
#include <ctype.h>

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/

static byte checkpvs[MAX_MAP_LEAFS / 8];

void PF_Fixme(void)
{
	PR_RunError("unimplemented bulitin");
}

char *PF_VarString(int first)
{
	int			i;
	static char	out[2048];

	out[0] = 0;
	for (i = first; i < pr_argc; i++)
		strcat(out, G_STRING(OFS_PARM0 + i * 3));
	return out;
}

/*
=================
PF_error

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
void PF_error(void)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(0);
	Con_Printf("======SERVER ERROR in %s:\n%s\n", pr_strings + pr_xfunction->s_name, s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print(ed);

	Host_Error("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void PF_objerror(void)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(0);
	Con_Printf("======OBJECT ERROR in %s:\n%s\n", pr_strings + pr_xfunction->s_name, s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print(ed);
	ED_Free(ed);

	Host_Error("Program error");
}

/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors_I(float *angles)
{
	AngleVectors(angles, pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

void PF_makevectors(void)
{
	PF_makevectors_I(G_VECTOR(OFS_PARM0));
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics
of the world (setting velocity and waiting). Directly changing origin
will not set internal links correctly, so clipping would be messed up.

setorigin (entity, origin)
=================
*/
void PF_setorigin_I(edict_t *e, float *org)
{
	VectorCopy(org, e->v.origin);
	SV_LinkEdict(e, false);
}

void PF_setorigin(void)
{
	edict_t	*e;
	float	*org;

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

	// set derived values
	VectorCopy(min, e->v.mins);
	VectorCopy(max, e->v.maxs);
	VectorSubtract(max, min, e->v.size);

	SV_LinkEdict(e, false);
}

/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize_I(edict_t *e, float *min, float *max)
{
	SetMinMaxSize(e, min, max);
}

void PF_setsize(void)
{
	edict_t	*e;
	float	*min, *max;

	e = G_EDICT(OFS_PARM0);
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	PF_setsize_I(e, min, max);
}

/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
void PF_setmodel_I(edict_t *e, const char *m)
{
	int		i;
	model_t	*mod;

	// check to see if model was properly precached
	for (i = 0; i < MAX_MODELS && sv.model_precache[i]; i++)
		if (!strcmp(sv.model_precache[i], m))
			break;

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
	edict_t	*e;
	char	*m;

	e = G_EDICT(OFS_PARM0);
	m = G_STRING(OFS_PARM1);
	PF_setmodel_I(e, m);
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
void PF_bprint(void)
{
	char *s;

	s = PF_VarString(0);
	SV_BroadcastPrintf("%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint(void)
{
	char				*s;
	server_client_t		*client;
	int					entnum;

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

/*
=================
PF_centerprint

single print to a specific client
=================
*/
void PF_centerprint_I(edict_t *ent, const char *s)
{
	server_client_t		*client;
	int					entnum;

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

/*
=================
PF_normalize

vector normalize(vector)
=================
*/
void PF_normalize(void)
{
	float	*value1;
	vec3_t	newvalue;
	float	new;

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

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
void PF_vlen(void)
{
	float	*value1;
	float	new;

	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2] * value1[2];
	new = sqrt(new);

	G_FLOAT(OFS_RETURN) = new;
}

float VectorToYaw(float *value1)
{
	float yaw;

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (int)(atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	return yaw;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
void PF_vectoyaw(void)
{
	float	*value1;
	float	yaw;

	value1 = G_VECTOR(OFS_PARM0);

	yaw = VectorToYaw(value1);

	G_FLOAT(OFS_RETURN) = yaw;
}

void VectorAngles(float *forward, float *angles)
{
	float	tmp, yaw, pitch;

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

/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
void PF_vectoangles(void)
{
	float *value1;

	value1 = G_VECTOR(OFS_PARM0);

	VectorAngles(value1, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
void PF_random(void)
{
	float num;

	num = (rand() & 0x7fff) / ((float)0x7fff);

	G_FLOAT(OFS_RETURN) = num;
}

/*
==================
SV_WriteParticleEffect

Make sure the event gets sent to all clients
==================
*/
static void SV_WriteParticleEffect(float *org, float *dir, char color, char count)
{
	int		i, v;

	if (sv.datagram.cursize > MAX_DATAGRAM - 16)
		return;

	MSG_WriteByte(&sv.datagram, svc_particle);
	MSG_WriteCoord(&sv.datagram, org[0]);
	MSG_WriteCoord(&sv.datagram, org[1]);
	MSG_WriteCoord(&sv.datagram, org[2]);
	for (i = 0; i < 3; i++)
	{
		v = dir[i] * 16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteChar(&sv.datagram, v);
	}
	MSG_WriteByte(&sv.datagram, count);
	MSG_WriteByte(&sv.datagram, color);
}

void SV_StartParticle(float *org, float *dir, float color, float count)
{
	SV_WriteParticleEffect(org, dir, (int)color, (int)count);
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
void PF_particle(void)
{
	float	*org, *dir;
	float	color;
	float	count;

	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);
	SV_StartParticle(org, dir, color, count);
}

/*
=================
PF_ambientsound
=================
*/
void PF_ambientsound_I(float *pos, const char *samp, float vol, float attenuation)
{
	int		i, soundnum;

	// check to see if samp was properly precached
	for (soundnum = 0; soundnum < MAX_SOUNDS && sv.sound_precache[soundnum]; soundnum++)
		if (!strcmp(sv.sound_precache[soundnum], samp))
			break;

	if (!sv.sound_precache[soundnum])
	{
		Con_Printf("no precache: %s\n", samp);
		return;
	}

	// add an svc_spawnambient command to the level signon packet
	MSG_WriteByte(&sv.signon, svc_spawnstaticsound);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte(&sv.signon, soundnum);

	MSG_WriteByte(&sv.signon, vol * 255);
	MSG_WriteByte(&sv.signon, attenuation * 64);
}

void PF_ambientsound(void)
{
	char	*samp;
	float	*pos;
	float	vol, attenuation;

	pos = G_VECTOR(OFS_PARM0);
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

	PF_ambientsound_I(pos, samp, vol, attenuation);
}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.
=================
*/
void PF_sound_I(edict_t *entity, int channel, const char *sample, float volume, float attenuation)
{
	if (volume < 0 || volume > 255)
		Sys_Error("SV_StartSound: volume = %i", (int)volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error("SV_StartSound: channel = %i", channel);

	SV_StartSound(entity, channel, sample, volume * 255, attenuation);
}

void PF_sound(void)
{
	char	*sample;
	int		channel;
	edict_t	*entity;
	float	volume;
	float	attenuation;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3);
	attenuation = G_FLOAT(OFS_PARM4);

	PF_sound_I(entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
void PF_break(void)
{
	Con_Printf("break statement\n");
	*(int *)-4 = 0;	// dump to debugger
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
void PF_traceline_Shared(float *v1, float *v2, int nomonsters, edict_t *ent)
{
	trace_t	trace;

	trace = SV_Move(v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy(trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy(trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist = trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = 0;
}

/*
=================
PF_traceline_DLL

The game DLL version: a NULL skip entity means the world, and the result
goes into a TraceResult as well as the trace globals.
=================
*/
void PF_traceline_DLL(float *v1, float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr)
{
	if (!pentToSkip)
		pentToSkip = sv_edicts;

	PF_traceline_Shared(v1, v2, fNoMonsters, pentToSkip);

	ptr->fAllSolid = pr_global_struct->trace_allsolid;
	ptr->fStartSolid = pr_global_struct->trace_startsolid;
	ptr->fInOpen = pr_global_struct->trace_inopen;
	ptr->fInWater = pr_global_struct->trace_inwater;
	ptr->flFraction = pr_global_struct->trace_fraction;
	VectorCopy(pr_global_struct->trace_endpos, ptr->vecEndPos);
	ptr->flPlaneDist = pr_global_struct->trace_plane_dist;
	VectorCopy(pr_global_struct->trace_plane_normal, ptr->vecPlaneNormal);
	ptr->pHit = pr_global_struct->trace_ent;
}

void PF_traceline(void)
{
	float	*v1, *v2;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	PF_traceline_Shared(v1, v2, nomonsters, ent);
}

void PF_TraceToss_Shared(edict_t *ent, edict_t *ignore)
{
	trace_t	trace;

	SV_Trace_Toss(&trace, ent, ignore);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy(trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy(trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist = trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = 0;
}

void PF_TraceToss_DLL(edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr)
{
	if (!pentToIgnore)
		pentToIgnore = sv_edicts;

	PF_TraceToss_Shared(pent, pentToIgnore);

	ptr->fAllSolid = pr_global_struct->trace_allsolid;
	ptr->fStartSolid = pr_global_struct->trace_startsolid;
	ptr->fInOpen = pr_global_struct->trace_inopen;
	ptr->fInWater = pr_global_struct->trace_inwater;
	ptr->flFraction = pr_global_struct->trace_fraction;
	VectorCopy(pr_global_struct->trace_endpos, ptr->vecEndPos);
	ptr->flPlaneDist = pr_global_struct->trace_plane_dist;
	VectorCopy(pr_global_struct->trace_plane_normal, ptr->vecPlaneNormal);
	ptr->pHit = pr_global_struct->trace_ent;
}

void PF_TraceToss(void)
{
	edict_t	*ent;
	edict_t	*ignore;

	ent = G_EDICT(OFS_PARM0);
	ignore = G_EDICT(OFS_PARM1);

	PF_TraceToss_Shared(ent, ignore);
}

/*
=================
PF_checkpos

Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
void PF_checkpos(void)
{
}

//============================================================================

int PF_newcheckclient(int check)
{
	int		i;
	byte	*pvs;
	edict_t	*ent;
	mleaf_t	*leaf;
	vec3_t	org;

	// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;

	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for ( ; ; i++)
	{
		if (i == svs.maxclients + 1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int)ent->v.flags & FL_NOTARGET)
			continue;

		break;
	}

	// get the PVS for the entity
	VectorAdd(ent->v.origin, ent->v.view_ofs, org);
	leaf = Mod_PointInLeaf(org, sv.worldmodel);
	pvs = Mod_LeafPVS(leaf, sv.worldmodel);
	memcpy(checkpvs, pvs, (sv.worldmodel->numleafs + 7) >> 3);

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
edict_t *PF_checkclient_I(void)
{
	edict_t	*ent, *self;
	mleaf_t	*leaf;
	int		l;
	vec3_t	view;

	// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient(sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

	// return check if it might be visible
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->free || ent->v.health <= 0)
		return sv.edicts;

	// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd(self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf(view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if ((l < 0) || !(checkpvs[l >> 3] & (1 << (l & 7))))
		return sv.edicts;

	// might be able to see it
	return ent;
}

void PF_checkclient(void)
{
	RETURN_EDICT(PF_checkclient_I());
}

//============================================================================

/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
void PF_stuffcmd_I(edict_t *pEdict, char *szFmt, ...)
{
	va_list				argptr;
	char				szOut[1024];
	int					entnum;
	server_client_t		*old;

	entnum = NUM_FOR_EDICT(pEdict);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError("stuffcmd: supplied bad client");

	va_start(argptr, szFmt);
	vsprintf(szOut, szFmt, argptr);
	va_end(argptr);

	old = host_client;
	host_client = &svs.clients[entnum - 1];
	SV_ClientCommand("%s", szOut);
	host_client = old;
}

void PF_stuffcmd(void)
{
	int		entnum;
	char	*str;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError("stuffcmd: supplied bad client");

	str = G_STRING(OFS_PARM1);
	PF_stuffcmd_I(EDICT_NUM(entnum), str);
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
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

/*
=================
PF_cvar

float cvar (string)
=================
*/
void PF_cvar(void)
{
	char *str;

	str = G_STRING(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = Cvar_VariableValue(str);
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
void PF_cvar_set(void)
{
	char	*var, *val;

	var = G_STRING(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	Cvar_Set(var, val);
}

/*
=================
FindEntityInSphere

Returns a chain of entities that have origins within a spherical area
=================
*/
edict_t *FindEntityInSphere(vec3_t org, float rad)
{
	edict_t	*ent, *chain;
	float	radius2, dist2, eorg;
	int		i, j;

	chain = sv_edicts;
	radius2 = rad * rad;

	for (i = 1; i < *sv_num_edicts; i++)
	{
		ent = (edict_t *)((byte *)sv_edicts + i * pr_edict_size);
		if (ent->free)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;

		dist2 = 0;
		for (j = 0; j < 3; j++)
		{
			eorg = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j]) * 0.5f);
			dist2 += eorg * eorg;
		}

		if (dist2 <= radius2)
		{
			ent->v.chain = EDICT_TO_PROG(chain);
			chain = ent;
		}
	}

	return chain;
}

/*
=================
PF_findradius

findradius (origin, radius)
=================
*/
void PF_findradius(void)
{
	edict_t	*ent;
	float	*org;
	float	rad;

	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);

	ent = FindEntityInSphere(org, rad);

	RETURN_EDICT(ent);
}

/*
=========
PF_dprint
=========
*/
void PF_dprint(void)
{
	Con_DPrintf("%s", PF_VarString(0));
}

void PF_ftos(void)
{
	float		v;
	static char	pr_string_temp[64];

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

	sprintf(pr_string_temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
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
	RETURN_EDICT(PF_Spawn_I());
}

void PF_Remove_I(edict_t *ed)
{
	ED_Free(ed);
}

void PF_Remove(void)
{
	PF_Remove_I(G_EDICT(OFS_PARM0));
}

/*
=================
iGetIndex

Returns the entvars offset of a string field the game DLL may search on, or -1.
=================
*/
int iGetIndex(const char *pszField)
{
	char	sz[512];
	char	*p;

	if (!pszField)
		return -1;

	strncpy(sz, pszField, sizeof(sz) - 1);
	sz[sizeof(sz) - 1] = 0;

	for (p = sz; *p; p++)
		*p = tolower((unsigned char)*p);

	if (!strcmp(sz, "classname"))
		return offsetof(entvars_t, classname);
	if (!strcmp(sz, "model"))
		return offsetof(entvars_t, model);
	if (!strcmp(sz, "weaponmodel"))
		return offsetof(entvars_t, weaponmodel);
	if (!strcmp(sz, "netname"))
		return offsetof(entvars_t, netname);
	if (!strcmp(sz, "target"))
		return offsetof(entvars_t, target);
	if (!strcmp(sz, "targetname"))
		return offsetof(entvars_t, targetname);
	if (!strcmp(sz, "message"))
		return offsetof(entvars_t, message);
	if (!strcmp(sz, "noise"))
		return offsetof(entvars_t, noise);
	if (!strcmp(sz, "noise1"))
		return offsetof(entvars_t, noise1);
	if (!strcmp(sz, "noise2"))
		return offsetof(entvars_t, noise2);
	if (!strcmp(sz, "noise3"))
		return offsetof(entvars_t, noise3);

	return -1;
}

/*
=================
PF_find_Shared

Finds the entities after startEntNum whose string field (an entvars offset)
equals szValue. Returns the first one; the matches are linked through v.chain.
=================
*/
static edict_t *PF_find_Shared(int startEntNum, int fieldOffset, const char *szValue)
{
	edict_t		*ent;
	edict_t		*first, *second, *last;
	string_t	str;
	int			e;
	int			next;

	last = sv.edicts;
	second = sv.edicts;
	first = sv.edicts;

	for (e = startEntNum + 1; e < sv.num_edicts; e++)
	{
		ent = EDICT_NUM(e);
		if (ent->free)
			continue;

		str = *(string_t *)((byte *)&ent->v + fieldOffset);
		if (!strcmp(pr_strings + str, szValue))
		{
			if (sv.edicts == first)
				first = ent;
			else if (sv.edicts == second)
				second = ent;

			ent->v.chain = (byte *)last - (byte *)sv.edicts;
			last = ent;
		}
	}

	if (last != first)
	{
		if (last == second)
			next = (byte *)last - (byte *)sv.edicts;
		else
			next = last->v.chain;

		first->v.chain = next;
		last->v.chain = 0;

		if (second && last != second)
			second->v.chain = (byte *)last - (byte *)sv.edicts;
	}

	return first;
}

edict_t *FindEntityByString(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue)
{
	int		fieldOffset;
	int		e;

	fieldOffset = iGetIndex(pszField);
	if (fieldOffset == -1 || !pszValue)
		return NULL;

	if (pEdictStartSearchAfter)
		e = NUM_FOR_EDICT(pEdictStartSearchAfter);
	else
		e = 0;

	return PF_find_Shared(e, fieldOffset, pszValue);
}

/*
=================
PF_Find

entity find (entity start, .string field, string match)
=================
*/
void PF_Find(void)
{
	int			e;
	int			f;
	const char	*s;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);
	if (!s)
		PR_RunError("PF_Find: bad search string");

	RETURN_EDICT(PF_find_Shared(e, f, s));
}

int GetEntityIllum(edict_t *pEnt)
{
	int *illum;

	if (!pEnt)
		return -1;

	illum = (int *)&cl_entities[NUM_FOR_EDICT(pEnt)];
	return (illum[0] + illum[1] + illum[2]) / 3;
}

void PR_CheckEmptyString(char *s)
{
	if (s[0] <= ' ')
		PR_RunError("Bad string");
}

void PF_precache_file(void)
{
	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound_internal(const char *s)
{
	int i;

	if (sv.state != ss_loading)
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

	if (sv.state != ss_loading)
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

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
float PF_walkmove_I(edict_t *ent, float yaw, float dist)
{
	vec3_t		move;
	dfunction_t	*oldf;
	int			oldself;
	float		ret;

	if (!((int)ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
		return 0;

	yaw = yaw * (M_PI * 2) / 360;

	move[0] = cos(yaw) * dist;
	move[1] = sin(yaw) * dist;
	move[2] = 0;

	// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	ret = SV_movestep(ent, move, true);

	// restore program state
	pr_xfunction = oldf;
	pr_global_struct->self = oldself;

	return ret;
}

void PF_walkmove(void)
{
	edict_t	*ent;
	float	yaw, dist;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);

	G_FLOAT(OFS_RETURN) = PF_walkmove_I(ent, yaw, dist);
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
float PF_droptofloor_I(edict_t *ent)
{
	vec3_t	end;
	trace_t	trace;

	VectorCopy(ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

	if (trace.fraction == 1 || trace.allsolid)
		return 0;

	VectorCopy(trace.endpos, ent->v.origin);
	SV_LinkEdict(ent, false);
	ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
	ent->v.groundentity = EDICT_TO_PROG(trace.ent);
	return 1;
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
	strncpy(dest, name, sizeof(sv_decalnames[0]) - 1);
	dest[sizeof(sv_decalnames[0]) - 1] = 0;

	if (sv_decalnamecount < decal + 1)
		sv_decalnamecount = decal + 1;

	return dest;
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
void SV_BroadcastLightStyle(int style, const char *val)
{
	server_client_t	*client;
	int				j;

	// change the string in sv
	sv.lightstyles[style] = (char *)val;

	// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (client->active || client->spawned)
		{
			MSG_WriteByte(&client->message, svc_lightstyle);
			MSG_WriteByte(&client->message, style);
			MSG_WriteString(&client->message, (char *)val);
		}
	}
}

void PF_lightstyle(void)
{
	int		style;
	char	*val;

	style = G_FLOAT(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	SV_BroadcastLightStyle(style, val);
}

void PF_rint(void)
{
	float f;

	f = G_FLOAT(OFS_PARM0);
	if (f <= 0)
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5f);
	else
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5f);
}

void PF_floor(void)
{
	G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}

void PF_ceil(void)
{
	G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}

/*
=============
PF_checkbottom
=============
*/
int PF_checkbottom_I(edict_t *ent)
{
	return SV_CheckBottom(ent);
}

void PF_checkbottom(void)
{
	edict_t *ent;

	ent = G_EDICT(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = PF_checkbottom_I(ent);
}

/*
=============
PF_pointcontents
=============
*/
int PF_pointcontents_I(vec3_t p)
{
	return SV_PointContents(p);
}

void PF_pointcontents(void)
{
	G_FLOAT(OFS_RETURN) = PF_pointcontents_I(G_VECTOR(OFS_PARM0));
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
void PF_nextent(void)
{
	int		i;
	edict_t	*ent;

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

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
void PF_aim_I(edict_t *ent, float speed, float *rgflReturn)
{
	edict_t	*check, *bestent;
	vec3_t	start, dir, end;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;

	VectorCopy(ent->v.origin, start);
	start[2] += 20;

	// try sending a trace straight
	VectorCopy(pr_global_struct->v_forward, dir);
	VectorMA(start, 2048, dir, end);
	tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent);
	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM
		&& (!teamplay.value || ent->v.team <= 0 || ent->v.team != tr.ent->v.team))
	{
		VectorCopy(pr_global_struct->v_forward, rgflReturn);
		return;
	}

	// try all possible entities
	VectorCopy(dir, rgflReturn);
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
			continue;	// don't aim at teammate
		for (j = 0; j < 3; j++)
			end[j] = check->v.origin[j] + 0.5 * (check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract(end, start, dir);
		VectorNormalize(dir);
		dist = DotProduct(dir, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent);
		if (tr.ent == check)
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent)
	{
		VectorSubtract(bestent->v.origin, ent->v.origin, dir);
		dist = DotProduct(dir, pr_global_struct->v_forward);
		VectorScale(pr_global_struct->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize(end);
		VectorCopy(end, rgflReturn);
	}
	else
	{
		VectorCopy(pr_global_struct->v_forward, rgflReturn);
	}
}

void PF_aim(void)
{
	edict_t	*ent;
	float	speed;

	ent = G_EDICT(OFS_PARM0);
	speed = G_FLOAT(OFS_PARM1);

	PF_aim_I(ent, speed, G_VECTOR(OFS_RETURN));
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

static sizebuf_t *WriteDest(int dest)
{
	int		entnum;
	edict_t	*ent;

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
	MSG_WriteByte(GetWriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteChar(void)
{
	MSG_WriteChar(GetWriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteShort(void)
{
	MSG_WriteShort(GetWriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteLong(void)
{
	MSG_WriteLong(GetWriteDest(), G_FLOAT(OFS_PARM1));
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
	MSG_WriteShort(GetWriteDest(), G_EDICTNUM(OFS_PARM1));
}

// the same writes for the game DLL, with the destination as an argument

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

//=============================================================================

/*
==================
PF_makestatic

Writes the entity into the signon as a static entity and frees it.
==================
*/
void PF_makestatic_I(edict_t *ent)
{
	int i;

	MSG_WriteByte(&sv.signon, svc_spawnstatic);

	MSG_WriteByte(&sv.signon, SV_ModelIndex(pr_strings + ent->v.model));
	MSG_WriteByte(&sv.signon, ent->v.sequence);
	MSG_WriteByte(&sv.signon, ent->v.frame);
	MSG_WriteByte(&sv.signon, ent->v.colormap);
	MSG_WriteByte(&sv.signon, ent->v.skin);
	for (i = 0; i < 3; i++)
	{
		MSG_WriteCoord(&sv.signon, ent->v.origin[i]);
		MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
	}

	MSG_WriteByte(&sv.signon, ent->v.rendermode);
	if (ent->v.rendermode != kRenderNormal)
	{
		MSG_WriteByte(&sv.signon, ent->v.renderamt);
		MSG_WriteByte(&sv.signon, ent->v.rendercolor[0]);
		MSG_WriteByte(&sv.signon, ent->v.rendercolor[1]);
		MSG_WriteByte(&sv.signon, ent->v.rendercolor[2]);
		MSG_WriteByte(&sv.signon, ent->v.renderfx);
	}

	// throw the entity away now
	ED_Free(ent);
}

void PF_makestatic(void)
{
	PF_makestatic_I(G_EDICT(OFS_PARM0));
}

//=============================================================================

/*
==============
PF_setspawnparms

Loads the client's spawn parms into the progs parms.
==============
*/
int PF_setspawnparms(edict_t *ent)
{
	int i;

	i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > svs.maxclients)
		PR_RunError("Entity is not a client");

	memcpy(&pr_global_struct->parm1, svs.clients[i - 1].spawn_parms, sizeof(svs.clients[i - 1].spawn_parms));
	return (int)svs.clients;
}

void (*pr_builtins[])(void) =
{
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

	(void (*)(void))SV_EntityFirstChangeLevelCallback
};

int pr_numbuiltins = sizeof(pr_builtins) / sizeof(pr_builtins[0]);
