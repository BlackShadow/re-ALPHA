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
#include <math.h>

typedef struct temp_entity_s
{
	int flags;
	float die;
	struct temp_entity_s *next;
	struct temp_entity_s *prev;

	byte pad16[8];
	vec3_t velocity;
	byte pad36[60];
	vec3_t prev_origin;
	byte pad108[12];
	vec3_t origin;
	vec3_t avelocity;
	byte pad144[12];
	vec3_t angles;

	int field_168;
	int field_172;
	int field_176;
	int field_180;
	model_t *model;
	int pad188;
	float field_192;
	float field_196;

	byte *colormap;
	int light_frame;
	byte pad208[36];
	int frame_index;
	byte pad248[0x140 - 248];

} temp_entity_t;

#define MAX_TEMP_ENTITIES 350
static temp_entity_t g_temp_entities[MAX_TEMP_ENTITIES];
static temp_entity_t *g_tempent_active;
static temp_entity_t *g_tempent_free;
static int g_tempent_lightframe;

extern vec3_t		vec3_origin;
extern double		cl_time;
extern double		cl_oldtime;

sfx_t	*cl_sfx_explosion = NULL;
sfx_t	*cl_sfx_spark1 = NULL;
sfx_t	*cl_sfx_spark2 = NULL;
sfx_t	*cl_sfx_wizbang = NULL;
sfx_t	*cl_sfx_ric1 = NULL;
sfx_t	*cl_sfx_ric2 = NULL;
sfx_t	*cl_sfx_ric3 = NULL;
sfx_t	*cl_sfx_ric4 = NULL;
sfx_t	*cl_sfx_ric5 = NULL;
sfx_t	*cl_sfx_lavasizzle = NULL;
sfx_t	*cl_sfx_teleport = NULL;
sfx_t	*cl_sfx_implosion = NULL;
sfx_t	*cl_sfx_tink1 = NULL;

extern model_t *cl_model_precache[MAX_MODELS];
extern cvar_t tracerSpeed;
extern cvar_t tracerOffset;
extern cvar_t r_decals;
extern cl_entity_t cl_entities[];
extern particle_t *particle_freelist;
extern particle_t *particle_activelist;

void CL_InitTEnts(void)
{
	int i;

	memset(g_temp_entities, 0, sizeof(g_temp_entities));

	for (i = 0; i < MAX_TEMP_ENTITIES - 1; ++i)
		g_temp_entities[i].next = &g_temp_entities[i + 1];

	g_temp_entities[MAX_TEMP_ENTITIES - 1].next = NULL;

	g_tempent_active = NULL;
	g_tempent_free = &g_temp_entities[0];

	g_tempent_lightframe = 0;
}

temp_entity_t *R_AllocTempEntity(vec_t *origin, model_t *model)
{
	int i;

	if (!g_tempent_free || !model)
	{
		Con_DPrintf("Out of bubbles!\n");
		return NULL;
	}

	temp_entity_t *te = g_tempent_free;
	g_tempent_free = te->next;

	memset(&te->pad16, 0, sizeof(*te) - 16);

	te->flags = 0;
	te->model = model;
	te->die = (float)(cl_time + 0.75);
	te->field_168 = 0;
	te->field_180 = 0;
	te->colormap = host_colormap;

	for (i = 0; i < 3; ++i)
	{
		te->origin[i] = origin[i] + (float)(rand() % 6 - 3);
		te->velocity[i] = (float)(4 * (rand() % 100) - 200);
	}

	te->next = g_tempent_active;
	g_tempent_active = te;

	return te;
}

void CL_UpdateTEnts(void)
{
	temp_entity_t *te;
	temp_entity_t *prev;

	const float frame_delta = (float)(cl_time - cl_oldtime);
	const float time_scale = (float)(cl_time * 0.01);
	const float gravity_step = (float)(sv_gravity.value * frame_delta);
	const float slow_gravity_step = gravity_step * 0.5f;

	if (!g_tempent_active || !cl_worldmodel)
		return;

	g_tempent_lightframe = (g_tempent_lightframe + 1) & 0x1F;

	prev = NULL;
	te = g_tempent_active;

	while (te)
	{
		temp_entity_t *next = te->next;

		if (te->die < cl_time)
		{
			te->next = g_tempent_free;
			g_tempent_free = te;

			if (prev)
				prev->next = next;
			else
				g_tempent_active = next;

			te = next;
			continue;
		}

		VectorCopy(te->origin, te->prev_origin);

		if ((te->flags & 1) == 0)
		{
			VectorMA(te->origin, frame_delta, te->velocity, te->origin);
		}
		else
		{
			const double t = (double)te->field_196 * (double)time_scale + (double)te->velocity[2];
			te->origin[2] = te->origin[2] + te->velocity[2] * frame_delta;
			te->origin[0] = (float)(sin(t) * 8.0 + (double)te->field_196);
		}

		if (te->flags & 2)
			te->velocity[2] -= gravity_step;
		else if (te->flags & 8)
			te->velocity[2] -= slow_gravity_step;

		if (te->flags & 4)
		{
			te->angles[0] += te->avelocity[0];
			te->angles[1] += te->avelocity[1];
			te->angles[2] += te->avelocity[2];
		}

		if (te->flags & 0x20)
		{
			trace_t trace;
			memset(&trace, 0, sizeof(trace));
			trace.allsolid = true;
			trace.fraction = 1.0f;

			SV_RecursiveHullCheck(
				&cl_worldmodel->hulls[0],
				cl_worldmodel->hulls[0].firstclipnode,
				0.0f,
				1.0f,
				te->prev_origin,
				te->origin,
				&trace);

			if (trace.fraction != 1.0f)
			{
				float dot;
				VectorMA(te->prev_origin, trace.fraction * frame_delta, te->velocity, te->origin);

				dot = DotProduct(te->velocity, trace.plane.normal);
				VectorMA(te->velocity, -2.0f * dot, trace.plane.normal, te->velocity);
				VectorScale(te->velocity, 0.5f, te->velocity);
			}
		}

		if ((te->flags & 0x40) && te->light_frame == g_tempent_lightframe)
		{
			dlight_t *dl = CL_AllocDlight(0);
			VectorCopy(te->origin, dl->origin);
			dl->radius = 60.0f;
			dl->color.r = 255;
			dl->color.g = 120;
			dl->color.b = 0;
			dl->die = (float)(cl_time + 0.01);
		}

		if (te->flags & 0x10)
			R_RocketTrail(te->prev_origin, te->origin, 1);

		if (cl_numvisedicts < MAX_VISEDICTS)
			cl_visedicts[cl_numvisedicts++] = (cl_entity_t *)((byte *)te + 16);

		prev = te;
		te = next;
	}
}

int CL_FxBlend(int entity_ptr)
{
	int renderfx;
	int blend;
	int *renderamt;
	double v;

	if (!entity_ptr)
		return 0;

	renderfx = *(int *)(entity_ptr + 164);
	renderamt = (int *)(entity_ptr + 156);

	switch (renderfx)
	{
		case 1:
			blend = (int)(cos(cl_time * 2.0) * 16.0 + (double)*renderamt);
			break;
		case 2:
			blend = (int)(cos(cl_time * 8.0) * 16.0 + (double)*renderamt);
			break;
		case 3:
			blend = (int)(cos(cl_time * 2.0) * 64.0 + (double)*renderamt);
			break;
		case 4:
			blend = (int)(cos(cl_time * 8.0) * 64.0 + (double)*renderamt);
			break;
		case 5:
			if (*renderamt <= 0)
			{
				blend = 0;
			}
			else
			{
				blend = *renderamt - 1;
				*renderamt = blend;
			}
			break;
		case 6:
			if (*renderamt <= 3)
			{
				blend = 0;
			}
			else
			{
				blend = *renderamt - 4;
				*renderamt = blend;
			}
			break;
		case 7:
			if (*renderamt >= 255)
			{
				blend = 255;
			}
			else
			{
				blend = *renderamt + 1;
				*renderamt = blend;
			}
			break;
		case 8:
			if (*renderamt >= 252)
			{
				blend = 255;
			}
			else
			{
				blend = *renderamt + 4;
				*renderamt = blend;
			}
			break;
		case 9:
			v = cos(cl_time * 4.0);
			blend = (((int)(v * 20.0) < 0) ? 0 : 255);
			break;
		case 10:
			v = cos(cl_time * 16.0);
			blend = (((int)(v * 20.0) < 0) ? 0 : 255);
			break;
		case 11:
			v = cos(cl_time * 36.0);
			blend = (((int)(v * 20.0) < 0) ? 0 : 255);
			break;
		case 12:
			v = cos(cl_time * 17.0) + cos(cl_time * 2.0);
			blend = (((int)(v * 20.0) < 0) ? 0 : 255);
			break;
		case 13:
			v = cos(cl_time * 23.0) + cos(cl_time * 16.0);
			blend = (((int)(v * 20.0) < 0) ? 0 : 255);
			break;
		default:
			blend = *renderamt;
			break;
	}

	if (blend > 255)
		return 255;
	if (blend < 0)
		return 0;
	return blend;
}

int R_GetSpriteFrameCount(model_t *model)
{
	int result;
	modtype_t type;

	result = 1;
	if (model)
	{
		type = model->type;
		if (type == mod_sprite)
		{
			result = ((msprite_t *)model->cache.data)->numframes;
		}
		else if (type == mod_studio)
		{
			result = R_StudioGetFrameCount(model);
		}
		if (result < 1)
			return 1;
	}
	return result;
}

static void CL_ParseSkyColor(void)
{
	int r;
	int g;
	int b;

	r = MSG_ReadByte();
	g = MSG_ReadByte();
	b = MSG_ReadByte();
	MSG_ReadByte();

	SetupSkyPolygonClipping(r, g, b);
}

void R_TracerEffect(vec_t *start, vec_t *end)
{
	vec3_t dir;
	vec3_t vel;
	float len;
	float inv_len;
	float offset;
	float die;

	if (tracerSpeed.value <= 0.0f)
		tracerSpeed.value = 3.0f;

	dir[0] = end[0] - start[0];
	dir[1] = end[1] - start[1];
	dir[2] = end[2] - start[2];

	len = VectorLength(dir);
	inv_len = 1.0f / len;
	VectorScale(dir, inv_len, dir);

	offset = (float)(rand() % 20) + tracerOffset.value - 10.0f;
	VectorScale(dir, offset, vel);

	start[0] = start[0] + vel[0];
	start[1] = start[1] + vel[1];
	start[2] = start[2] + vel[2];

	VectorScale(dir, tracerSpeed.value, vel);
	die = len / tracerSpeed.value;

	R_ParticleStatic((vec3_t *)start, (vec3_t *)&vel, die);
}

void R_Bubbles(cl_entity_t *ent, int modelIndex, int count)
{
	float *bubblechain;
	model_t *model;
	int frameCount;
	int bubbleCount;
	int speedBase;
	int i;
	int boundsX;
	int boundsY;
	float height;
	vec3_t pos;
	temp_entity_t *te;

	bubblechain = *(float **)((byte *)ent + 168);
	if (!bubblechain)
		return;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	boundsX = (int)(bubblechain[24] - bubblechain[21]);
	boundsY = (int)(bubblechain[25] - bubblechain[22]);
	height = bubblechain[26] - bubblechain[23];

	frameCount = R_GetSpriteFrameCount(model);
	bubbleCount = count + 1;
	speedBase = 3 * (count + 3);

	for (i = 0; i < bubbleCount; ++i)
	{
		pos[0] = (float)(rand() % boundsX) + bubblechain[21];
		pos[1] = (float)(rand() % boundsY) + bubblechain[22];
		pos[2] = bubblechain[23];

		te = R_AllocTempEntity(pos, model);
		if (!te)
			break;

		te->flags |= 1;
		te->field_196 = pos[0];

		te->velocity[2] = (float)(speedBase + (rand() & 0x1F));
		te->die = height / te->velocity[2] + (float)cl_time;

		te->field_192 = (float)(rand() % frameCount);
	}
}

void R_Sprite_Spray(vec_t *org, float speed, float life, int count, int modelIndex)
{
	model_t *model;
	int frameCount;
	int lightFrame;
	int i;
	temp_entity_t *te;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	frameCount = R_GetSpriteFrameCount(model);
	lightFrame = 0;

	for (i = 0; i < count; ++i)
	{
		te = R_AllocTempEntity(org, model);
		if (!te)
			break;

		te->frame_index = rand() % frameCount;

		if ((byte)rand() >= 0xC8)
			te->flags |= 2;
		else
			te->flags |= 8;

		if ((byte)rand() < 0xDC)
		{
			te->flags |= 4;
			te->avelocity[0] = ((float)(rand() & 7) - 4.0f) * 2.0f;
			te->avelocity[1] = ((float)(rand() & 7) - 4.0f) * 4.0f;
			te->avelocity[2] = (float)(rand() & 7) - 4.0f;
		}

		if ((byte)rand() < 0x64)
			te->flags |= 0x10;

		te->flags |= 0x60;
		te->light_frame = (lightFrame++) & 0x1F;
		te->field_168 = 0;

		te->velocity[0] = (float)((rand() & 0xFFF) - 2048) * 0.00048828125f;
		te->velocity[1] = (float)((rand() & 0xFFF) - 2048) * 0.00048828125f;
		te->velocity[2] = (float)((rand() & 0xFFF) - 2048) * 0.00048828125f;
		VectorNormalize(te->velocity);
		VectorScale(te->velocity, speed, te->velocity);

		te->die = (float)cl_time + life;
	}
}

void R_Sprite_Trail(vec_t *org, vec_t *dims, vec_t *dir, float life, int count, int modelIndex, byte flags)
{
	model_t *model;
	int frameCount;
	int particleCount;
	int i;
	byte mode;
	float velocityScale;
	temp_entity_t *te;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	mode = flags & 0xF;
	frameCount = R_GetSpriteFrameCount(model);

	particleCount = count;
	if (!count)
		particleCount = (int)((dims[0] * dims[1] * dims[2]) / 27000.0f);

	if (mode == 8)
		particleCount *= 4;

	for (i = 0; i < particleCount; ++i)
	{
		rand();
		rand();
		rand();

		te = R_AllocTempEntity(org, model);
		if (!te)
			break;

		if (model->type == mod_sprite)
			te->field_192 = (float)(rand() % frameCount);
		else if (model->type == mod_studio)
			te->frame_index = rand() % frameCount;

		te->flags |= 0x20;

		if ((byte)rand() >= 0xC8)
			te->flags |= 2;
		else
			te->flags |= 8;

		if ((byte)rand() < 0xC8)
		{
			te->flags |= 4;
			te->avelocity[0] = (float)(rand() & 7) * 2.0f - 4.0f;
			te->avelocity[1] = (float)(rand() & 7) * 4.0f - 4.0f;
			te->avelocity[2] = (float)(rand() & 7) - 4.0f;
		}

		if ((byte)rand() < 0x64 && (mode == 2 || (flags & 0x10) != 0))
			te->flags |= 0x10;

		if (mode == 1 || (flags & 0x20) != 0)
		{
			te->field_168 = 2;
			te->field_172 = 100;
			te->field_180 = 5;
		}
		else
		{
			te->field_168 = 0;
		}

		te->velocity[0] = (float)((rand() & 0xFFF) - 2048) * dir[0] * 0.00048828125f;
		te->velocity[1] = (float)((rand() & 0xFFF) - 2048) * dir[0] * 0.00048828125f;
		te->velocity[2] = (float)(rand() & 0xFFF) * dir[0] * 0.000244140625f;

		velocityScale = VectorLength(dir) * 100.0f;
		VectorScale(te->velocity, velocityScale, te->velocity);

		te->die = (float)cl_time + life;
	}
}

void R_BubbleTrail(vec_t *org, vec_t *vel, float life, int modelIndex)
{
	model_t *model;
	int frameCount;
	temp_entity_t *te;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	frameCount = R_GetSpriteFrameCount(model);
	te = R_AllocTempEntity(org, model);
	if (!te)
		return;

	te->flags |= 2;
	te->velocity[0] = vel[0];
	te->velocity[1] = vel[1];
	te->velocity[2] = vel[2];
	te->die = (float)cl_time + life;

	if (model->type == mod_sprite)
		te->field_192 = (float)(rand() % frameCount);
	else
		te->frame_index = rand() % frameCount;
}

void CL_ParseTEnt(void)
{
	int type;
	vec3_t pos;
	vec3_t dir;

	type = MSG_ReadByte();

	if (type > 100)
	{
		switch (type)
		{
			case 101:
				pos[0] = MSG_ReadCoord();
				pos[1] = MSG_ReadCoord();
				pos[2] = MSG_ReadCoord();
				dir[0] = MSG_ReadCoord();
				dir[1] = MSG_ReadCoord();
				dir[2] = MSG_ReadCoord();
				{
					int color = MSG_ReadByte();
					int speed = MSG_ReadByte();

					if (color <= 64 && (speed == 195 || speed == 247))
					{
						const int tmp = color;
						color = speed;
						speed = tmp;
					}

					R_SparkStreaks(pos, dir, color, speed);
				}
				return;
			case 102:
				pos[0] = MSG_ReadCoord();
				pos[1] = MSG_ReadCoord();
				pos[2] = MSG_ReadCoord();
				dir[0] = MSG_ReadCoord();
				dir[1] = MSG_ReadCoord();
				dir[2] = MSG_ReadCoord();
				R_BeamParticles(pos, dir);
				return;
			case 103:
				pos[0] = MSG_ReadCoord();
				pos[1] = MSG_ReadCoord();
				pos[2] = MSG_ReadCoord();
				dir[0] = MSG_ReadCoord();
				dir[1] = MSG_ReadCoord();
				dir[2] = MSG_ReadCoord();
				{
					int color = MSG_ReadByte();
					int speed = MSG_ReadByte();

					if (color <= 64 && (speed == 195 || speed == 247))
					{
						const int tmp = color;
						color = speed;
						speed = tmp;
					}

					R_StreakSplash(pos, dir, color, speed);
				}
				return;
			case 104:
			{
				pos[0] = MSG_ReadCoord();
				pos[1] = MSG_ReadCoord();
				pos[2] = MSG_ReadCoord();

				const int entityIndex = MSG_ReadShort();
				const int decalIndex = MSG_ReadByte();

				if (entityIndex > 600)
					Sys_Error("Decal: entity = %i", entityIndex);

				if (r_decals.value != 0.0f)
				{
					const int texture = Draw_DecalIndex(decalIndex);
					R_DecalShoot(texture, entityIndex, pos, 0);
				}
				return;
			}
			case 105:
			{
				const int entityIndex = MSG_ReadShort();
				if (entityIndex > 600)
					Sys_Error("Bubble: entity = %i", entityIndex);

				const int modelIndex = MSG_ReadShort();
				const int count = MSG_ReadByte();
				R_Bubbles(&cl_entities[entityIndex], modelIndex, count);
				return;
			}
			case 106:
			{
				pos[0] = MSG_ReadCoord();
				pos[1] = MSG_ReadCoord();
				pos[2] = MSG_ReadCoord();
				dir[0] = MSG_ReadCoord();
				dir[1] = MSG_ReadCoord();
				dir[2] = MSG_ReadCoord();

				const int modelIndex = MSG_ReadShort();
				const float life = (float)MSG_ReadByte() * 0.1f;
				R_BubbleTrail(pos, dir, life, modelIndex);
				return;
			}
			case 107:
			{
				pos[0] = MSG_ReadCoord();
				pos[1] = MSG_ReadCoord();
				pos[2] = MSG_ReadCoord();

				const float speed = MSG_ReadCoord();
				const int modelIndex = MSG_ReadShort();
				const int count = MSG_ReadShort();
				const float life = (float)MSG_ReadByte() * 0.1f;

				R_Sprite_Spray(pos, speed, life, count, modelIndex);
				return;
			}
			case 108:
			{
				vec3_t size;

				pos[0] = MSG_ReadCoord();
				pos[1] = MSG_ReadCoord();
				pos[2] = MSG_ReadCoord();
				size[0] = MSG_ReadCoord();
				size[1] = MSG_ReadCoord();
				size[2] = MSG_ReadCoord();
				dir[0] = MSG_ReadCoord();
				dir[1] = MSG_ReadCoord();
				dir[2] = MSG_ReadCoord();

				const int modelIndex = MSG_ReadShort();
				const int count = MSG_ReadByte();
				const float life = (float)MSG_ReadByte() * 0.1f;
				const byte flags = (byte)MSG_ReadByte();

				R_Sprite_Trail(pos, size, dir, life, count, modelIndex, flags);
				return;
			}
			default:
				Sys_Error("CL_ParseTEnt: bad type");
		}
	}

	if (type == 100)
	{
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_TeleportSplash(pos);
		return;
	}

	switch (type)
	{
		case 0:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_RunParticleEffect(pos, vec3_origin, 0, 10);

			if (rand() % 5)
			{
				S_StartSound(-1, 0, cl_sfx_tink1, pos, 1.0f, 1.0f);
			}
			else
			{
				switch (rand() & 3)
				{
					case 1:
						S_StartSound(-1, 0, cl_sfx_ric3, pos, 1.0f, 1.0f);
						break;
					case 2:
						S_StartSound(-1, 0, cl_sfx_ric5, pos, 1.0f, 1.0f);
						break;
					default:
						S_StartSound(-1, 0, cl_sfx_ric4, pos, 1.0f, 1.0f);
						break;
				}
			}
			break;
		case 1:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_RunParticleEffect(pos, vec3_origin, 0, 20);

			if (rand() % 5)
			{
				S_StartSound(-1, 0, cl_sfx_tink1, pos, 1.0f, 1.0f);
			}
			else
			{
				switch (rand() & 3)
				{
					case 1:
						S_StartSound(-1, 0, cl_sfx_ric3, pos, 1.0f, 1.0f);
						break;
					case 2:
						S_StartSound(-1, 0, cl_sfx_ric5, pos, 1.0f, 1.0f);
						break;
					default:
						S_StartSound(-1, 0, cl_sfx_ric4, pos, 1.0f, 1.0f);
						break;
				}
			}
			break;
		case 2:
		{
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_RunParticleEffect(pos, vec3_origin, 0, 20);

			const int r = rand();
			if (r >= 0x3FFF)
				return;

			switch (r % 5)
			{
				case 0:
					S_StartSound(-1, 0, cl_sfx_ric3, pos, 1.0f, 1.0f);
					break;
				case 1:
					S_StartSound(-1, 0, cl_sfx_ric5, pos, 1.0f, 1.0f);
					break;
				case 2:
					S_StartSound(-1, 0, cl_sfx_ric4, pos, 1.0f, 1.0f);
					break;
				case 3:
					S_StartSound(-1, 0, cl_sfx_ric2, pos, 1.0f, 1.0f);
					break;
				case 4:
					S_StartSound(-1, 0, cl_sfx_ric1, pos, 1.0f, 1.0f);
					break;
			}
			break;
		}
		case 3:
		{
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();

			dlight_t *dl = CL_AllocDlight(0);
			VectorCopy(pos, dl->origin);
			dl->radius = 256.0f;
			dl->color.r = 250;
			dl->color.g = 250;
			dl->color.b = 150;
			dl->die = (float)(cl_time + 0.01);
			dl->decay = 768.0f;

			const int which = rand() % 3;
			if (!which)
				S_StartSound(-1, 0, cl_sfx_explosion, pos, 1.0f, 1.0f);
			else if (which == 1)
				S_StartSound(-1, 0, cl_sfx_spark1, pos, 1.0f, 1.0f);
			else
				S_StartSound(-1, 0, cl_sfx_spark2, pos, 1.0f, 1.0f);
			return;
		}
		case 4:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_BlobExplosion(pos);
			S_StartSound(-1, 0, cl_sfx_explosion, pos, 1.0f, 1.0f);
			return;
		case 5:
			CL_ParseSkyColor();
			return;
		case 6:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();
			R_TracerEffect(pos, dir);
			return;
		case 7:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_RunParticleEffect(pos, vec3_origin, 20, 30);
			S_StartSound(-1, 0, cl_sfx_lavasizzle, pos, 1.0f, 1.0f);
			return;
		case 8:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_RunParticleEffect(pos, vec3_origin, 226, 20);
			S_StartSound(-1, 0, cl_sfx_wizbang, pos, 1.0f, 1.0f);
			return;
		case 9:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_SparkShower(pos);
			return;
		case 10:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_LavaSplash(pos);
			return;
		case 11:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			R_DarkFieldParticles2(pos);
			return;
		case 12:
		{
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();

			const int colorStart = MSG_ReadByte();
			const int colorLength = MSG_ReadByte();
			R_ParticleExplosion2(pos, colorStart, colorLength);

			dlight_t *dl = CL_AllocDlight(0);
			VectorCopy(pos, dl->origin);
			dl->radius = 352.0f;
			dl->die = (float)(cl_time + 0.5);
			dl->decay = 304.0f;

			S_StartSound(-1, 0, cl_sfx_explosion, pos, 1.0f, 1.0f);
			return;
		}
		case 14:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			S_StartSound(-1, 0, cl_sfx_implosion, pos, 1.0f, 1.0f);
			return;
		case 15:
		{
			vec3_t end;

			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			end[0] = MSG_ReadCoord();
			end[1] = MSG_ReadCoord();
			end[2] = MSG_ReadCoord();

			S_StartSound(-1, 0, cl_sfx_teleport, pos, 1.0f, 1.0f);
			S_StartSound(-1, 1, cl_sfx_explosion, end, 1.0f, 1.0f);

			R_RocketTrail(pos, end, 128);
			R_ParticleExplosion(end);

			dlight_t *dl = CL_AllocDlight(0);
			VectorCopy(end, dl->origin);
			dl->radius = 352.0f;
			dl->die = (float)(cl_time + 0.5);
			dl->decay = 304.0f;
			return;
		}
		default:
			Sys_Error("CL_ParseTEnt: bad type");
	}
}
