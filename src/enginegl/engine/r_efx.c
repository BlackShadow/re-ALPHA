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

extern struct model_s	*cl_model_precache[256];
extern double			cl_time;

extern void			Cvar_RegisterVariable(cvar_t *variable);
extern sfx_t		*S_PrecacheSound(const char *sample);
extern void			R_ParticleStatic(vec3_t *pos, vec3_t *vel, float die);
extern void			*R_AllocTempEntity(vec_t *origin, model_t *model);
extern int			R_GetSpriteFrameCount(model_t *model);
extern void			CL_InitTEnts(void);

cvar_t tracerSpeed = { "tracerspeed", "3000" };
cvar_t tracerOffset = { "traceroffset", "35" };
cvar_t tracerLength = { "tracerlength", "1.0" };
cvar_t tracerRed = { "tracerred", "0.6" };
cvar_t tracerGreen = { "tracergreen", "0.8" };
cvar_t tracerBlue = { "tracerblue", "0.1" };
cvar_t tracerAlpha = { "traceralpha", "0.2" };

extern int r_framecount;
extern int cl_numvisedicts;
extern cl_entity_t *cl_visedicts[MAX_VISEDICTS];

void R_StoreEfrags(efrag_t **efrag_list)
{
	efrag_t *efrag;
	int frame;

	efrag = *efrag_list;
	if (!efrag)
		return;

	frame = r_framecount;

	do
	{
		entity_t *ent = efrag->entity;

		if (ent->model->type > mod_alias)
			Sys_Error("R_StoreEfrags: Bad entity type %d\n", ent->model->type);

		if (ent->visframe != frame && cl_numvisedicts < MAX_VISEDICTS)
		{
			ent->visframe = frame;
			cl_visedicts[cl_numvisedicts++] = ent;
		}

		efrag = efrag->leafnext;
	}
	while (efrag);
}

int R_PrecacheWeaponSounds(void)
{
	extern sfx_t *cl_sfx_ric1;
	extern sfx_t *cl_sfx_ric2;
	extern sfx_t *cl_sfx_ric3;
	extern sfx_t *cl_sfx_ric4;
	extern sfx_t *cl_sfx_ric5;
	extern sfx_t *cl_sfx_explosion;
	extern sfx_t *cl_sfx_spark1;
	extern sfx_t *cl_sfx_spark2;

	Cvar_RegisterVariable(&tracerSpeed);
	Cvar_RegisterVariable(&tracerOffset);
	Cvar_RegisterVariable(&tracerLength);
	Cvar_RegisterVariable(&tracerRed);
	Cvar_RegisterVariable(&tracerGreen);
	Cvar_RegisterVariable(&tracerBlue);
	Cvar_RegisterVariable(&tracerAlpha);

	cl_sfx_ric1 = S_PrecacheSound("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound("weapons/ric3.wav");
	cl_sfx_ric4 = S_PrecacheSound("weapons/ric4.wav");
	cl_sfx_ric5 = S_PrecacheSound("weapons/ric5.wav");

	cl_sfx_explosion = S_PrecacheSound("weapons/explode3.wav");
	cl_sfx_spark1 = S_PrecacheSound("weapons/explode4.wav");
	cl_sfx_spark2 = S_PrecacheSound("weapons/explode5.wav");

	CL_InitTEnts();
	return 0;
}

void R_SpraySprite(int entity_ptr, int sprite_idx, int count)
{
	float bounds_x, bounds_y, bounds_z;
	int frame_count;
	int i;
	vec3_t particle_pos;
	int particle_ptr;
	int rand_frame;
	int modelptr;

	if (!sprite_idx)
		return;

	modelptr = (int)cl_model_precache[sprite_idx];
	if (!modelptr)
		return;

	bounds_x = *(float *)(modelptr + 96) - *(float *)(modelptr + 84);
	bounds_y = *(float *)(modelptr + 104) - *(float *)(modelptr + 92);
	bounds_z = *(float *)(modelptr + 100) - *(float *)(modelptr + 88);

	frame_count = R_GetSpriteFrameCount((model_t *)modelptr);

	for (i = 0; i < (count + 1); i++)
	{
		particle_pos[0] = (float)(rand() % (int)bounds_x) + *(float *)(modelptr + 84);
		particle_pos[1] = (float)(rand() % (int)bounds_z) + *(float *)(modelptr + 88);
		particle_pos[2] = *(float *)(modelptr + 92);

		particle_ptr = (int)R_AllocTempEntity(particle_pos, (model_t *)modelptr);
		if (!particle_ptr)
			break;

		*(byte *)particle_ptr |= 1;
		*(float *)(particle_ptr + 196) = particle_pos[0];

		rand_frame = rand();
		*(float *)(particle_ptr + 32) = (float)(3 * (count + 3) + (rand_frame & 0x1F));
		*(float *)(particle_ptr + 4) = bounds_y / *(float *)(particle_ptr + 32) + (float)cl_time;

		*(float *)(particle_ptr + 192) = (float)(rand() % frame_count);
	}
}

float *R_GetParticleFrameInfo(int base, float velocity, float lifetime, int num_frames, int modelIndex)
{
	float *result;
	int frame_idx;
	int i;
	float *particle;
	int rand_val;
	float vx, vy, vz;

	result = NULL;
	if (modelIndex)
	{
		result = (float *)cl_model_precache[modelIndex];
		if (result != NULL)
		{
			frame_idx = R_GetSpriteFrameCount((model_t *)result);

			for (i = 0; i < num_frames; i++)
			{
				particle = (float *)R_AllocTempEntity((vec_t *)base, (model_t *)result);
				if (!particle)
					break;

				rand_val = rand() % (int)frame_idx;
				*(int*)(particle + 244) = rand_val;

				if ((rand() & 0xFF) >= 200)
					*(char*)particle |= 2;
				else
					*(char*)particle |= 8;

				if ((rand() & 0xFF) < 220)
				{
					*(char*)particle |= 4;
					vx = (float)(rand() & 7);
					particle[33] = (vx - 4.0f) * 2.0f;
					vy = (float)(rand() & 7);
					particle[34] = (vy - 4.0f) * 4.0f;
					vz = (float)(rand() & 7);
					particle[35] = vz - 4.0f;
				}

				if ((rand() & 0xFF) < 100)
					*(char*)particle |= 16;

				*(char*)particle |= 96;

				rand_val = i & 0x1F;
				*(int*)(particle + 204) = rand_val;

				particle[42] = 0.0f;

				vx = (float)((rand() & 0xFFF) - 2048) * 0.00048828125f;
				particle[6] = vx;
				vy = (float)((rand() & 0xFFF) - 2048) * 0.00048828125f;
				particle[7] = vy;
				vz = (float)((rand() & 0xFFF) - 2048) * 0.00048828125f;
				particle[8] = vz;

				VectorNormalize(particle + 6);
				VectorScale(particle + 6, velocity, particle + 6);
				particle[1] = (float)cl_time + lifetime;
			}
		}
	}

	return result;
}

int R_CreateExplosionSprites(int origin, float *size, float *velocity, float lifetime, int num_sprites, int sprite_idx, char flags)
{
	int sprite_base;
	int frame_count;
	int i;
	int particle;
	char vel_type;
	int default_count;
	float explosion_size;
	int entity_type;
	float rand_val;

	sprite_base = (int)cl_model_precache[sprite_idx];
	vel_type = flags & 0x0F;

	if (sprite_base)
	{
		frame_count = R_GetSpriteFrameCount((model_t *)sprite_base);

		default_count = num_sprites;
		if (!num_sprites)
		{
			explosion_size = size[0] * size[1] * size[2] / 27000.0f;
			default_count = (int)explosion_size;
		}

		if (vel_type == 8)
			default_count *= 4;

		for (i = 0; i < default_count; i++)
		{
			rand();
			rand();
			rand();

			particle = (int)R_AllocTempEntity((vec_t *)origin, (model_t *)sprite_base);
			if (!particle)
				break;

			entity_type = *(int*)(sprite_base + 68);
			if (entity_type == 1)
			{

				rand_val = (float)(rand() % frame_count);
				*(float*)(particle + 192) = rand_val;
			}
			else if (entity_type == 3)
			{

				*(int*)(particle + 244) = rand() % frame_count;
			}

			*(char*)particle |= 0x20;

			if ((rand() & 0xFF) >= 200)
				*(char*)particle |= 2;
			else
				*(char*)particle |= 8;

			if ((rand() & 0xFF) < 200)
			{
				*(char*)particle |= 4;
				rand_val = (float)(rand() & 7);
				*(float*)(particle + 132) = rand_val * 2.0f - 4.0f;
				rand_val = (float)(rand() & 7);
				*(float*)(particle + 136) = rand_val * 4.0f - 4.0f;
				rand_val = (float)(rand() & 7);
				*(float*)(particle + 140) = rand_val - 4.0f;
			}

			if ((rand() & 0xFF) < 100 && (vel_type == 2 || (flags & 0x10)))
				*(char*)particle |= 0x10;

			if (vel_type == 1 || (flags & 0x20))
			{
				*(int*)(particle + 168) = 2;
				*(int*)(particle + 172) = 100;
				*(int*)(particle + 180) = 5;
			}
			else
			{
				*(int*)(particle + 168) = 0;
			}

			rand_val = (float)((rand() & 0xFFF) - 2048) * velocity[0] * 0.00048828125f;
			*(float*)(particle + 24) = rand_val;
			rand_val = (float)((rand() & 0xFFF) - 2048) * velocity[1] * 0.00048828125f;
			*(float*)(particle + 28) = rand_val;
			rand_val = (float)(rand() & 0xFFF) * velocity[2] * 0.000244140625f;
			*(float*)(particle + 32) = rand_val;

			rand_val = VectorLength(velocity) * 100.0f;
			VectorScale((float*)(particle + 24), rand_val, (float*)(particle + 24));

			*(float *)(particle + 4) = (float)cl_time + lifetime;
		}
	}

	return (int)particle;
}

int R_CreateBloodSprite(int origin, int *velocity, float lifetime, int sprite_idx)
{
	int sprite_base;
	int frame_count;
	int particle;
	int entity_type;
	float rand_frame;

	particle = sprite_idx;
	if (sprite_idx)
	{
		sprite_base = (int)cl_model_precache[sprite_idx];
		if (sprite_base)
		{
			frame_count = R_GetSpriteFrameCount((model_t *)sprite_base);
			particle = (int)R_AllocTempEntity((vec_t *)origin, (model_t *)sprite_base);

			if (particle)
			{
				*(char*)particle |= 2;

				*(int*)(particle + 24) = velocity[0];
				*(int*)(particle + 28) = velocity[1];
				*(int*)(particle + 32) = velocity[2];

				*(float *)(particle + 4) = lifetime + (float)cl_time;

				entity_type = *(int*)(sprite_base + 68);
				if (entity_type == 1)
				{

					rand_frame = (float)(rand() % frame_count);
					*(float*)(particle + 192) = rand_frame;
					return (int)rand_frame;
				}
				else
				{

					*(int*)(particle + 244) = rand() % frame_count;
					return rand() / frame_count;
				}
			}
		}
	}

	return (int)particle;
}
