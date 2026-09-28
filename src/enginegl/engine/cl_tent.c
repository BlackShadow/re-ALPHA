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

// cl_tent.c -- client side temporary entities

#include "quakedef.h"

#define MAX_TEMP_ENTITIES	350
#define TENT_FLICKER_FRAMES	32		// a flickering temp entity lights up once every 32 frames

// palette indices of the blood colors
#define BLOOD_YELLOW		195
#define BLOOD_RED			247

static tempent_t	cl_tempents[MAX_TEMP_ENTITIES];
static tempent_t	*cl_active_tents;
static tempent_t	*cl_free_tents;
static int			cl_tent_lightframe;

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

/*
=================
CL_InitTEnts
=================
*/
void CL_InitTEnts(void)
{
	int		i;

	memset(cl_tempents, 0, sizeof(cl_tempents));

	for (i = 0; i < MAX_TEMP_ENTITIES - 1; i++)
		cl_tempents[i].next = &cl_tempents[i + 1];
	cl_tempents[MAX_TEMP_ENTITIES - 1].next = NULL;

	cl_active_tents = NULL;
	cl_free_tents = &cl_tempents[0];

	cl_tent_lightframe = 0;
}

/*
=================
R_AllocTempEntity

Links a new temp entity at origin, with a random offset and velocity
=================
*/
tempent_t *R_AllocTempEntity(vec3_t origin, model_t *model)
{
	int			i;
	tempent_t	*te;

	if (!cl_free_tents || !model)
	{
		Con_DPrintf("Out of bubbles!\n");
		return NULL;
	}

	te = cl_free_tents;
	cl_free_tents = te->next;

	memset(&te->entity, 0, sizeof(te->entity));

	te->flags = 0;
	te->entity.model = model;
	te->die = cl_time + 0.75;
	te->entity.rendermode = kRenderNormal;
	te->entity.renderfx = kRenderFxNone;
	te->entity.colormap = host_colormap;

	for (i = 0; i < 3; i++)
	{
		te->entity.origin[i] = origin[i] + (rand() % 6 - 3);
		te->entity.baseline.origin[i] = 4 * (rand() % 100) - 200;
	}

	te->next = cl_active_tents;
	cl_active_tents = te;

	return te;
}

/*
=================
CL_UpdateTEnts

Moves the temp entities and adds them to the visible list
=================
*/
void CL_UpdateTEnts(void)
{
	tempent_t	*te, *prev, *next;
	dlight_t	*dl;
	float		frametime = cl_time - cl_oldtime;
	float		time = cl_time * 0.01;
	float		gravity = sv_gravity.value * frametime;
	float		gravitySlow = gravity * 0.5f;

	if (!cl_active_tents || !cl_worldmodel)
		return;

	cl_tent_lightframe = (cl_tent_lightframe + 1) & (TENT_FLICKER_FRAMES - 1);

	prev = NULL;
	te = cl_active_tents;

	while (te)
	{
		next = te->next;

		if (te->die < cl_time)
		{	// free it
			te->next = cl_free_tents;
			cl_free_tents = te;

			if (prev)
				prev->next = next;
			else
				cl_active_tents = next;

			te = next;
			continue;
		}

		VectorCopy(te->entity.origin, te->entity.msg_origins[0]);

		if (!(te->flags & FTENT_SINEWAVE))
		{
			VectorMA(te->entity.origin, frametime, te->entity.baseline.origin, te->entity.origin);
		}
		else
		{	// wobble around entity.syncbase on x
			double	t = te->entity.syncbase * (double)time + te->entity.baseline.origin[2];

			te->entity.origin[2] = te->entity.origin[2] + te->entity.baseline.origin[2] * frametime;
			te->entity.origin[0] = sin(t) * 8.0 + te->entity.syncbase;
		}

		if (te->flags & FTENT_GRAVITY)
			te->entity.baseline.origin[2] -= gravity;
		else if (te->flags & FTENT_SLOWGRAVITY)
			te->entity.baseline.origin[2] -= gravitySlow;

		if (te->flags & FTENT_ROTATE)
			VectorAdd(te->entity.angles, te->entity.msg_angles[0], te->entity.angles);

		if (te->flags & FTENT_COLLIDEWORLD)
		{
			trace_t		trace;

			memset(&trace, 0, sizeof(trace));
			trace.allsolid = true;
			trace.fraction = 1.0f;

			SV_RecursiveHullCheck(&cl_worldmodel->hulls[0], cl_worldmodel->hulls[0].firstclipnode, 0, 1,
				te->entity.msg_origins[0], te->entity.origin, &trace);

			if (trace.fraction != 1.0f)
			{	// bounce off the wall, losing half the speed
				float	dot;

				VectorMA(te->entity.msg_origins[0], trace.fraction * frametime, te->entity.baseline.origin, te->entity.origin);

				dot = DotProduct(te->entity.baseline.origin, trace.plane.normal);
				VectorMA(te->entity.baseline.origin, -2.0f * dot, trace.plane.normal, te->entity.baseline.origin);
				VectorScale(te->entity.baseline.origin, 0.5f, te->entity.baseline.origin);
			}
		}

		if ((te->flags & FTENT_FLICKER) && te->entity.effects == cl_tent_lightframe)
		{
			dl = CL_AllocDlight(0);
			VectorCopy(te->entity.origin, dl->origin);
			dl->radius = 60;
			dl->color.r = 255;
			dl->color.g = 120;
			dl->color.b = 0;
			dl->die = cl_time + 0.01;
		}

		if (te->flags & FTENT_SMOKETRAIL)
			R_RocketTrail(te->entity.msg_origins[0], te->entity.origin, 1);

		if (cl_numvisedicts < MAX_VISEDICTS)
			cl_visedicts[cl_numvisedicts++] = &te->entity;

		prev = te;
		te = next;
	}
}

/*
=================
CL_FxBlend

Returns the blend amount of an entity for its renderfx
=================
*/
int CL_FxBlend(cl_entity_t *ent)
{
	int		blend;
	double	f;

	if (!ent)
		return 0;

	switch (ent->renderfx)
	{
	case kRenderFxPulseSlow:
		blend = cos(cl_time * 2.0) * 16.0 + ent->renderamt;
		break;

	case kRenderFxPulseFast:
		blend = cos(cl_time * 8.0) * 16.0 + ent->renderamt;
		break;

	case kRenderFxPulseSlowWide:
		blend = cos(cl_time * 2.0) * 64.0 + ent->renderamt;
		break;

	case kRenderFxPulseFastWide:
		blend = cos(cl_time * 8.0) * 64.0 + ent->renderamt;
		break;

	case kRenderFxFadeSlow:
		if (ent->renderamt <= 0)
		{
			blend = 0;
		}
		else
		{
			blend = ent->renderamt - 1;
			ent->renderamt = blend;
		}
		break;

	case kRenderFxFadeFast:
		if (ent->renderamt <= 3)
		{
			blend = 0;
		}
		else
		{
			blend = ent->renderamt - 4;
			ent->renderamt = blend;
		}
		break;

	case kRenderFxSolidSlow:
		if (ent->renderamt >= 255)
		{
			blend = 255;
		}
		else
		{
			blend = ent->renderamt + 1;
			ent->renderamt = blend;
		}
		break;

	case kRenderFxSolidFast:
		if (ent->renderamt >= 252)
		{
			blend = 255;
		}
		else
		{
			blend = ent->renderamt + 4;
			ent->renderamt = blend;
		}
		break;

	case kRenderFxStrobeSlow:
		f = cos(cl_time * 4.0);
		blend = ((int)(f * 20.0) < 0) ? 0 : 255;
		break;

	case kRenderFxStrobeFast:
		f = cos(cl_time * 16.0);
		blend = ((int)(f * 20.0) < 0) ? 0 : 255;
		break;

	case kRenderFxStrobeFaster:
		f = cos(cl_time * 36.0);
		blend = ((int)(f * 20.0) < 0) ? 0 : 255;
		break;

	case kRenderFxFlickerSlow:
		f = cos(cl_time * 17.0) + cos(cl_time * 2.0);
		blend = ((int)(f * 20.0) < 0) ? 0 : 255;
		break;

	case kRenderFxFlickerFast:
		f = cos(cl_time * 23.0) + cos(cl_time * 16.0);
		blend = ((int)(f * 20.0) < 0) ? 0 : 255;
		break;

	default:
		blend = ent->renderamt;
		break;
	}

	if (blend > 255)
		return 255;
	if (blend < 0)
		return 0;
	return blend;
}

/*
=================
R_GetSpriteFrameCount

Number of frames of a sprite, or of bodies of a studio model
=================
*/
int R_GetSpriteFrameCount(model_t *model)
{
	int		count;

	count = 1;
	if (model)
	{
		if (model->type == mod_sprite)
			count = ((msprite_t *)model->cache.data)->numframes;
		else if (model->type == mod_studio)
			count = R_StudioGetFrameCount(model);

		if (count < 1)
			return 1;
	}

	return count;
}

/*
=================
CL_ParseWaterColor
=================
*/
static void CL_ParseWaterColor(void)
{
	int		r, g, b;

	r = MSG_ReadByte();
	g = MSG_ReadByte();
	b = MSG_ReadByte();
	MSG_ReadByte();		// unused

	SetupSkyPolygonClipping(r, g, b);
}

/*
=================
R_TracerEffect
=================
*/
void R_TracerEffect(vec3_t start, vec3_t end)
{
	vec3_t	dir, vel;
	float	len, scale;
	float	offset;
	float	die;

	if (tracerSpeed.value <= 0)
		tracerSpeed.value = 3;

	VectorSubtract(end, start, dir);

	len = VectorLength(dir);
	scale = 1.0f / len;
	VectorScale(dir, scale, dir);

	// start a random distance down the line
	offset = (rand() % 20) + tracerOffset.value - 10.0f;
	VectorScale(dir, offset, vel);
	VectorAdd(start, vel, start);

	VectorScale(dir, tracerSpeed.value, vel);
	die = len / tracerSpeed.value;

	R_ParticleStatic((vec3_t *)start, &vel, die);
}

/*
=================
R_Bubbles

Rises bubbles from random points in the box of a brush entity
=================
*/
void R_Bubbles(cl_entity_t *ent, int modelIndex, int count)
{
	model_t		*box;
	model_t		*model;
	int			frameCount;
	int			speed;
	int			i;
	int			width, depth;
	float		height;
	vec3_t		pos;
	tempent_t	*te;

	box = ent->model;
	if (!box)
		return;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	width = box->maxs[0] - box->mins[0];
	depth = box->maxs[1] - box->mins[1];
	height = box->maxs[2] - box->mins[2];

	frameCount = R_GetSpriteFrameCount(model);
	speed = 3 * (count + 3);

	for (i = 0; i < count + 1; i++)
	{
		pos[0] = (rand() % width) + box->mins[0];
		pos[1] = (rand() % depth) + box->mins[1];
		pos[2] = box->mins[2];

		te = R_AllocTempEntity(pos, model);
		if (!te)
			break;

		te->flags |= FTENT_SINEWAVE;
		te->entity.syncbase = pos[0];

		te->entity.baseline.origin[2] = speed + (rand() & 31);
		te->die = height / te->entity.baseline.origin[2] + (float)cl_time;

		te->entity.frame = rand() % frameCount;
	}
}

/*
=================
R_Sprite_Spray

Throws sprites in random directions
=================
*/
void R_Sprite_Spray(vec3_t org, float speed, float life, int count, int modelIndex)
{
	model_t		*model;
	int			frameCount;
	int			lightFrame;
	int			i;
	tempent_t	*te;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	frameCount = R_GetSpriteFrameCount(model);
	lightFrame = 0;

	for (i = 0; i < count; i++)
	{
		te = R_AllocTempEntity(org, model);
		if (!te)
			break;

		te->entity.body = rand() % frameCount;

		if ((byte)rand() >= 200)
			te->flags |= FTENT_GRAVITY;
		else
			te->flags |= FTENT_SLOWGRAVITY;

		if ((byte)rand() < 220)
		{
			te->flags |= FTENT_ROTATE;
			te->entity.msg_angles[0][0] = ((rand() & 7) - 4.0f) * 2.0f;
			te->entity.msg_angles[0][1] = ((rand() & 7) - 4.0f) * 4.0f;
			te->entity.msg_angles[0][2] = (rand() & 7) - 4.0f;
		}

		if ((byte)rand() < 100)
			te->flags |= FTENT_SMOKETRAIL;

		te->flags |= FTENT_COLLIDEWORLD | FTENT_FLICKER;
		te->entity.effects = (lightFrame++) & (TENT_FLICKER_FRAMES - 1);
		te->entity.rendermode = kRenderNormal;

		te->entity.baseline.origin[0] = ((rand() & 4095) - 2048) * (1.0f / 2048);
		te->entity.baseline.origin[1] = ((rand() & 4095) - 2048) * (1.0f / 2048);
		te->entity.baseline.origin[2] = ((rand() & 4095) - 2048) * (1.0f / 2048);
		VectorNormalize(te->entity.baseline.origin);
		VectorScale(te->entity.baseline.origin, speed, te->entity.baseline.origin);

		te->die = (float)cl_time + life;
	}
}

/*
=================
R_Sprite_Trail

Breaks a brush into pieces of the given model
=================
*/
void R_Sprite_Trail(vec3_t org, vec3_t dims, vec3_t dir, float life, int count, int modelIndex, byte flags)
{
	model_t		*model;
	int			frameCount;
	int			pieces;
	int			i;
	byte		type;
	float		speed;
	tempent_t	*te;

	if (!modelIndex)
		return;

	model = cl_model_precache[modelIndex];
	if (!model)
		return;

	type = flags & BREAK_TYPEMASK;
	frameCount = R_GetSpriteFrameCount(model);

	pieces = count;
	if (!count)
		pieces = (dims[0] * dims[1] * dims[2]) / (30 * 30 * 30.0f);	// one piece per 30 unit cube

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
			te->entity.frame = rand() % frameCount;
		else if (model->type == mod_studio)
			te->entity.body = rand() % frameCount;

		te->flags |= FTENT_COLLIDEWORLD;

		if ((byte)rand() >= 200)
			te->flags |= FTENT_GRAVITY;
		else
			te->flags |= FTENT_SLOWGRAVITY;

		if ((byte)rand() < 200)
		{
			te->flags |= FTENT_ROTATE;
			te->entity.msg_angles[0][0] = (rand() & 7) * 2.0f - 4.0f;
			te->entity.msg_angles[0][1] = (rand() & 7) * 4.0f - 4.0f;
			te->entity.msg_angles[0][2] = (rand() & 7) - 4.0f;
		}

		if ((byte)rand() < 100 && (type == BREAK_METAL || (flags & BREAK_SMOKE)))
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

		te->entity.baseline.origin[0] = ((rand() & 4095) - 2048) * dir[0] * (1.0f / 2048);
		te->entity.baseline.origin[1] = ((rand() & 4095) - 2048) * dir[0] * (1.0f / 2048);
		te->entity.baseline.origin[2] = (rand() & 4095) * dir[0] * (1.0f / 4096);

		speed = VectorLength(dir) * 100.0f;
		VectorScale(te->entity.baseline.origin, speed, te->entity.baseline.origin);

		te->die = (float)cl_time + life;
	}
}

/*
=================
R_BubbleTrail
=================
*/
void R_BubbleTrail(vec3_t org, vec3_t vel, float life, int modelIndex)
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
	VectorCopy(vel, te->entity.baseline.origin);
	te->die = (float)cl_time + life;

	if (model->type == mod_sprite)
		te->entity.frame = rand() % frameCount;
	else
		te->entity.body = rand() % frameCount;
}

/*
=================
CL_ParseTEnt
=================
*/
void CL_ParseTEnt(void)
{
	int			type;
	vec3_t		pos;
	vec3_t		dir;
	dlight_t	*dl;

	type = MSG_ReadByte();

	if (type > TE_TELEPORTSPLASH)
	{
		switch (type)
		{
		case TE_BLOODSTREAM:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();
			{
				int		color = MSG_ReadByte();
				int		speed = MSG_ReadByte();

				// some senders put the speed before the color
				if (color <= 64 && (speed == BLOOD_YELLOW || speed == BLOOD_RED))
				{
					int		swap = color;

					color = speed;
					speed = swap;
				}

				R_SparkStreaks(pos, dir, color, speed);
			}
			return;

		case TE_SHOWLINE:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();
			R_BeamParticles(pos, dir);
			return;

		case TE_BLOOD:
			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();
			{
				int		color = MSG_ReadByte();
				int		speed = MSG_ReadByte();

				// some senders put the speed before the color
				if (color <= 64 && (speed == BLOOD_YELLOW || speed == BLOOD_RED))
				{
					int		swap = color;

					color = speed;
					speed = swap;
				}

				R_StreakSplash(pos, dir, color, speed);
			}
			return;

		case TE_DECAL:
		{
			int		entnum, decal;

			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();

			entnum = MSG_ReadShort();
			decal = MSG_ReadByte();

			if (entnum > MAX_EDICTS)
				Sys_Error("Decal: entity = %i", entnum);

			if (r_decals.value)
				R_DecalShoot(Draw_DecalIndex(decal), entnum, pos, 0);
			return;
		}

		case TE_BUBBLES:
		{
			int		entnum, modelIndex, count;

			entnum = MSG_ReadShort();
			if (entnum > MAX_EDICTS)
				Sys_Error("Bubble: entity = %i", entnum);

			modelIndex = MSG_ReadShort();
			count = MSG_ReadByte();
			R_Bubbles(&cl_entities[entnum], modelIndex, count);
			return;
		}

		case TE_BUBBLETRAIL:
		{
			int		modelIndex;
			float	life;

			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();

			modelIndex = MSG_ReadShort();
			life = MSG_ReadByte() * 0.1f;
			R_BubbleTrail(pos, dir, life, modelIndex);
			return;
		}

		case TE_SPRITE_SPRAY:
		{
			float	speed;
			int		modelIndex, count;
			float	life;

			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();

			speed = MSG_ReadCoord();
			modelIndex = MSG_ReadShort();
			count = MSG_ReadShort();
			life = MSG_ReadByte() * 0.1f;

			R_Sprite_Spray(pos, speed, life, count, modelIndex);
			return;
		}

		case TE_BREAKMODEL:
		{
			vec3_t	size;
			int		modelIndex, count;
			float	life;
			byte	flags;

			pos[0] = MSG_ReadCoord();
			pos[1] = MSG_ReadCoord();
			pos[2] = MSG_ReadCoord();
			size[0] = MSG_ReadCoord();
			size[1] = MSG_ReadCoord();
			size[2] = MSG_ReadCoord();
			dir[0] = MSG_ReadCoord();
			dir[1] = MSG_ReadCoord();
			dir[2] = MSG_ReadCoord();

			modelIndex = MSG_ReadShort();
			count = MSG_ReadByte();
			life = MSG_ReadByte() * 0.1f;
			flags = MSG_ReadByte();

			R_Sprite_Trail(pos, size, dir, life, count, modelIndex, flags);
			return;
		}

		default:
			Sys_Error("CL_ParseTEnt: bad type");
		}
	}

	if (type == TE_TELEPORTSPLASH)
	{
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_TeleportSplash(pos);
		return;
	}

	switch (type)
	{
	case TE_SPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 0, 10);

		if (rand() % 5)
		{
			S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
		}
		else
		{
			switch (rand() & 3)
			{
			case 1:
				S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
				break;
			case 2:
				S_StartSound(-1, 0, cl_sfx_ric5, pos, 1, 1);
				break;
			default:
				S_StartSound(-1, 0, cl_sfx_ric4, pos, 1, 1);
				break;
			}
		}
		break;

	case TE_SUPERSPIKE:		// super spike hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 0, 20);

		if (rand() % 5)
		{
			S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
		}
		else
		{
			switch (rand() & 3)
			{
			case 1:
				S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
				break;
			case 2:
				S_StartSound(-1, 0, cl_sfx_ric5, pos, 1, 1);
				break;
			default:
				S_StartSound(-1, 0, cl_sfx_ric4, pos, 1, 1);
				break;
			}
		}
		break;

	case TE_GUNSHOT:		// bullet hitting wall
	{
		int		r;

		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 0, 20);

		// ricochet sound half of the time
		r = rand();
		if (r >= RAND_MAX / 2)
			return;

		switch (r % 5)
		{
		case 0:
			S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
			break;
		case 1:
			S_StartSound(-1, 0, cl_sfx_ric5, pos, 1, 1);
			break;
		case 2:
			S_StartSound(-1, 0, cl_sfx_ric4, pos, 1, 1);
			break;
		case 3:
			S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
			break;
		case 4:
			S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
			break;
		}
		break;
	}

	case TE_EXPLOSION:		// rocket explosion
	{
		int		r;

		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();

		dl = CL_AllocDlight(0);
		VectorCopy(pos, dl->origin);
		dl->radius = 256;
		dl->color.r = 250;
		dl->color.g = 250;
		dl->color.b = 150;
		dl->die = cl_time + 0.01;
		dl->decay = 768;

		r = rand() % 3;
		if (r == 0)
			S_StartSound(-1, 0, cl_sfx_explosion, pos, 1, 1);
		else if (r == 1)
			S_StartSound(-1, 0, cl_sfx_spark1, pos, 1, 1);
		else
			S_StartSound(-1, 0, cl_sfx_spark2, pos, 1, 1);
		return;
	}

	case TE_TAREXPLOSION:	// tarbaby explosion
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_BlobExplosion(pos);

		S_StartSound(-1, 0, cl_sfx_explosion, pos, 1, 1);
		return;

	case TE_WATERCOLOR:
		CL_ParseWaterColor();
		return;

	case TE_TRACER:
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		dir[0] = MSG_ReadCoord();
		dir[1] = MSG_ReadCoord();
		dir[2] = MSG_ReadCoord();
		R_TracerEffect(pos, dir);
		return;

	case TE_WIZSPIKE:		// spike hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 20, 30);
		S_StartSound(-1, 0, cl_sfx_lavasizzle, pos, 1, 1);
		return;

	case TE_KNIGHTSPIKE:	// spike hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 226, 20);
		S_StartSound(-1, 0, cl_sfx_wizbang, pos, 1, 1);
		return;

	case TE_SPARKS:
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_SparkShower(pos);
		return;

	case TE_LAVASPLASH:
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_LavaSplash(pos);
		return;

	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_DarkFieldParticles2(pos);
		return;

	case TE_EXPLOSION2:		// color mapped explosion
	{
		int		colorStart, colorLength;

		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();

		colorStart = MSG_ReadByte();
		colorLength = MSG_ReadByte();
		R_ParticleExplosion2(pos, colorStart, colorLength);

		dl = CL_AllocDlight(0);
		VectorCopy(pos, dl->origin);
		dl->radius = 352;
		dl->die = cl_time + 0.5;
		dl->decay = 304;

		S_StartSound(-1, 0, cl_sfx_explosion, pos, 1, 1);
		return;
	}

	case TE_IMPLOSION:
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		S_StartSound(-1, 0, cl_sfx_implosion, pos, 1, 1);
		return;

	case TE_RAILTRAIL:
	{
		vec3_t	end;

		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		end[0] = MSG_ReadCoord();
		end[1] = MSG_ReadCoord();
		end[2] = MSG_ReadCoord();

		S_StartSound(-1, 0, cl_sfx_teleport, pos, 1, 1);
		S_StartSound(-1, 1, cl_sfx_explosion, end, 1, 1);

		R_RocketTrail(pos, end, 128);
		R_ParticleExplosion(end);

		dl = CL_AllocDlight(0);
		VectorCopy(end, dl->origin);
		dl->radius = 352;
		dl->die = cl_time + 0.5;
		dl->decay = 304;
		return;
	}

	default:
		Sys_Error("CL_ParseTEnt: bad type");
	}
}
