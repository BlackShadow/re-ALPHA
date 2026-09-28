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
// r_efx.c -- efrag storing, tracer cvars and sprite temp entity effects

#include "quakedef.h"

cvar_t	tracerSpeed = { "tracerspeed", "3000" };
cvar_t	tracerOffset = { "traceroffset", "35" };
cvar_t	tracerLength = { "tracerlength", "1.0" };
cvar_t	tracerRed = { "tracerred", "0.6" };
cvar_t	tracerGreen = { "tracergreen", "0.8" };
cvar_t	tracerBlue = { "tracerblue", "0.1" };
cvar_t	tracerAlpha = { "traceralpha", "0.2" };

/*
================
R_StoreEfrags

Adds the entities of a leaf to the visible list
================
*/
void R_StoreEfrags(efrag_t **ppefrag)
{
	efrag_t		*pefrag;
	int			framecount;

	pefrag = *ppefrag;
	if (!pefrag)
		return;

	framecount = r_framecount;

	do
	{
		entity_t *pent = pefrag->entity;

		if (pent->model->type > mod_alias)
			Sys_Error("R_StoreEfrags: Bad entity type %d\n", pent->model->type);

		if (pent->visframe != framecount && cl_numvisedicts < MAX_VISEDICTS)
		{
			pent->visframe = framecount;
			cl_visedicts[cl_numvisedicts++] = pent;
		}

		pefrag = pefrag->leafnext;
	}
	while (pefrag);
}

/*
================
R_PrecacheWeaponSounds

Also registers the tracer cvars and clears the temp entities.
================
*/
void R_PrecacheWeaponSounds(void)
{
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
}

/*
================
R_SpraySprite

Rises bubbles from random points in the box of sprite model modelIndex.
Like R_Bubbles, which uses the box of the entity's model.
================
*/
void R_SpraySprite(entity_t *ent, int modelIndex, int count)
{
	model_t		*model;
	float		width, height, depth;
	int			frameCount;
	int			i;
	vec3_t		pos;
	tempent_t	*te;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	width = model->maxs[0] - model->mins[0];
	height = model->maxs[2] - model->mins[2];
	depth = model->maxs[1] - model->mins[1];

	frameCount = R_GetSpriteFrameCount(model);

	for (i = 0; i < count + 1; i++)
	{
		pos[0] = (float)(rand() % (int)width) + model->mins[0];
		pos[1] = (float)(rand() % (int)depth) + model->mins[1];
		pos[2] = model->mins[2];

		te = R_AllocTempEntity(pos, model);
		if (!te)
			break;

		te->flags |= FTENT_SINEWAVE;
		te->entity.syncbase = pos[0];

		te->entity.baseline.origin[2] = (float)(3 * (count + 3) + (rand() & 31));
		te->die = height / te->entity.baseline.origin[2] + (float)cl_time;

		te->entity.frame = (float)(rand() % frameCount);
	}
}

/*
================
R_GetParticleFrameInfo

Throws sprites in random directions, like R_Sprite_Spray.
================
*/
void R_GetParticleFrameInfo(vec3_t org, float speed, float life, int count, int modelIndex)
{
	model_t		*model;
	int			frameCount;
	int			i;
	tempent_t	*te;

	if (modelIndex)
	{
		model = cl_model_precache[modelIndex];
		if (model != NULL)
		{
			frameCount = R_GetSpriteFrameCount(model);

			for (i = 0; i < count; i++)
			{
				te = R_AllocTempEntity(org, model);
				if (!te)
					break;

				te->entity.body = rand() % frameCount;

				if ((rand() & 255) >= 200)
					te->flags |= FTENT_GRAVITY;
				else
					te->flags |= FTENT_SLOWGRAVITY;

				if ((rand() & 255) < 220)
				{
					te->flags |= FTENT_ROTATE;
					te->entity.msg_angles[0][0] = ((float)(rand() & 7) - 4.0f) * 2.0f;
					te->entity.msg_angles[0][1] = ((float)(rand() & 7) - 4.0f) * 4.0f;
					te->entity.msg_angles[0][2] = (float)(rand() & 7) - 4.0f;
				}

				if ((rand() & 255) < 100)
					te->flags |= FTENT_SMOKETRAIL;

				te->flags |= FTENT_COLLIDEWORLD | FTENT_FLICKER;

				te->entity.effects = i & (TENT_FLICKER_FRAMES - 1);
				te->entity.rendermode = kRenderNormal;

				te->entity.baseline.origin[0] = (float)((rand() & 4095) - 2048) * (1.0f / 2048);
				te->entity.baseline.origin[1] = (float)((rand() & 4095) - 2048) * (1.0f / 2048);
				te->entity.baseline.origin[2] = (float)((rand() & 4095) - 2048) * (1.0f / 2048);

				VectorNormalize(te->entity.baseline.origin);
				VectorScale(te->entity.baseline.origin, speed, te->entity.baseline.origin);
				te->die = (float)cl_time + life;
			}
		}
	}
}

/*
================
R_CreateExplosionSprites

Breaks a brush into pieces of sprite or studio model modelIndex, like R_Sprite_Trail.
================
*/
void R_CreateExplosionSprites(vec3_t org, float *dims, float *dir, float life, int count, int modelIndex, char flags)
{
	model_t		*model;
	int			frameCount;
	int			i;
	tempent_t	*te;
	char		type;
	int			pieces;
	float		speed;

	model = cl_model_precache[modelIndex];
	type = flags & BREAK_TYPEMASK;

	if (model)
	{
		frameCount = R_GetSpriteFrameCount(model);

		pieces = count;
		if (!count)
			pieces = (int)(dims[0] * dims[1] * dims[2] / (30 * 30 * 30.0f));	// one piece per 30 unit cube

		if (type == BREAK_WOOD)
			pieces *= 4;

		for (i = 0; i < pieces; i++)
		{
			rand();
			rand();
			rand();

			te = R_AllocTempEntity(org, model);
			if (!te)
				break;

			if (model->type == mod_sprite)
				te->entity.frame = (float)(rand() % frameCount);
			else if (model->type == mod_studio)
				te->entity.body = rand() % frameCount;

			te->flags |= FTENT_COLLIDEWORLD;

			if ((rand() & 255) >= 200)
				te->flags |= FTENT_GRAVITY;
			else
				te->flags |= FTENT_SLOWGRAVITY;

			if ((rand() & 255) < 200)
			{
				te->flags |= FTENT_ROTATE;
				te->entity.msg_angles[0][0] = (float)(rand() & 7) * 2.0f - 4.0f;
				te->entity.msg_angles[0][1] = (float)(rand() & 7) * 4.0f - 4.0f;
				te->entity.msg_angles[0][2] = (float)(rand() & 7) - 4.0f;
			}

			if ((rand() & 255) < 100 && (type == BREAK_METAL || (flags & BREAK_SMOKE)))
				te->flags |= FTENT_SMOKETRAIL;

			if (type == BREAK_GLASS || (flags & BREAK_TRANS))
			{
				te->entity.rendermode = kRenderTransTexture;
				te->entity.renderamt = 100;
				te->entity.renderfx = kRenderFxFadeSlow;
			}
			else
			{
				te->entity.rendermode = kRenderNormal;
			}

			te->entity.baseline.origin[0] = (float)((rand() & 4095) - 2048) * dir[0] * (1.0f / 2048);
			te->entity.baseline.origin[1] = (float)((rand() & 4095) - 2048) * dir[1] * (1.0f / 2048);
			te->entity.baseline.origin[2] = (float)(rand() & 4095) * dir[2] * (1.0f / 4096);

			speed = VectorLength(dir) * 100.0f;
			VectorScale(te->entity.baseline.origin, speed, te->entity.baseline.origin);

			te->die = (float)cl_time + life;
		}
	}
}

/*
================
R_CreateBloodSprite

One falling sprite with the given velocity, like R_BubbleTrail.
================
*/
void R_CreateBloodSprite(vec3_t org, vec3_t vel, float life, int modelIndex)
{
	model_t		*model;
	int			frameCount;
	tempent_t	*te;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	frameCount = R_GetSpriteFrameCount(model);
	te = R_AllocTempEntity(org, model);
	if (!te)
		return;

	te->flags |= FTENT_GRAVITY;

	te->entity.baseline.origin[0] = vel[0];
	te->entity.baseline.origin[1] = vel[1];
	te->entity.baseline.origin[2] = vel[2];

	te->die = life + (float)cl_time;

	if (model->type == mod_sprite)
		te->entity.frame = (float)(rand() % frameCount);
	else
		te->entity.body = rand() % frameCount;
}
