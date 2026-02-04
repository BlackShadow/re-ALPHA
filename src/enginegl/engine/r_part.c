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
#include "glquake.h"
#include <math.h>

int					free_particles;
particle_t			*active_particles;
particle_t			*active_tracer_particles;
int					max_particles;
particle_t			*particle_pool;
int					particletexture;
extern vec3_t		r_origin;
extern vec3_t		vpn;
extern vec3_t		vup;
extern vec3_t		vright;
int					ramp1[8] = { 0x6F, 0x6D, 0x6B, 0x69, 0x67, 0x65, 0x63, 0x61 };
int					ramp2[8] = { 0x6F, 0x6E, 0x6D, 0x6C, 0x6B, 0x6A, 0x68, 0x66 };
int					ramp3[8] = { 0x6D, 0x6B, 6, 5, 4, 3, 0, 0 };
int					ramp4[9] = { 0xFE, 0xFD, 0xFC, 0x6F, 0x6E, 0x6D, 0x6C, 0x67, 0x60 };
extern mplane_t		frustum[4];

extern cvar_t tracerLength;
extern cvar_t tracerRed;
extern cvar_t tracerGreen;
extern cvar_t tracerBlue;
extern cvar_t tracerAlpha;

static vec3_t avelocities[162];

static const vec3_t r_avertexnormals[162] = {
	{ -0.5257310271263123f, 0.0f, 0.8506510257720947f },
	{ -0.44286298751831055f, 0.2388560026884079f, 0.864188015460968f },
	{ -0.2952420115470886f, 0.0f, 0.9554229974746704f },
	{ -0.30901700258255005f, 0.5f, 0.80901700258255f },
	{ -0.16245999932289124f, 0.26286599040031433f, 0.9510560035705566f },
	{ 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.8506510257720947f, 0.5257310271263123f },
	{ -0.1476210057735443f, 0.7165669798851013f, 0.6817179918289185f },
	{ 0.1476210057735443f, 0.7165669798851013f, 0.6817179918289185f },
	{ 0.0f, 0.5257310271263123f, 0.8506510257720947f },
	{ 0.30901700258255005f, 0.5f, 0.80901700258255f },
	{ 0.5257310271263123f, 0.0f, 0.8506510257720947f },
	{ 0.2952420115470886f, 0.0f, 0.9554229974746704f },
	{ 0.44286298751831055f, 0.2388560026884079f, 0.864188015460968f },
	{ 0.16245999932289124f, 0.26286599040031433f, 0.9510560035705566f },
	{ -0.6817179918289185f, 0.1476210057735443f, 0.7165669798851013f },
	{ -0.80901700258255f, 0.30901700258255005f, 0.5f },
	{ -0.587785005569458f, 0.4253250062465668f, 0.6881909966468811f },
	{ -0.8506510257720947f, 0.5257310271263123f, 0.0f },
	{ -0.864188015460968f, 0.44286298751831055f, 0.2388560026884079f },
	{ -0.7165669798851013f, 0.6817179918289185f, 0.1476210057735443f },
	{ -0.6881909966468811f, 0.587785005569458f, 0.4253250062465668f },
	{ -0.5f, 0.80901700258255f, 0.30901700258255005f },
	{ -0.2388560026884079f, 0.864188015460968f, 0.44286298751831055f },
	{ -0.4253250062465668f, 0.6881909966468811f, 0.587785005569458f },
	{ -0.7165669798851013f, 0.6817179918289185f, -0.1476210057735443f },
	{ -0.5f, 0.80901700258255f, -0.30901700258255005f },
	{ -0.5257310271263123f, 0.8506510257720947f, 0.0f },
	{ 0.0f, 0.8506510257720947f, -0.5257310271263123f },
	{ -0.2388560026884079f, 0.864188015460968f, -0.44286298751831055f },
	{ 0.0f, 0.9554229974746704f, -0.2952420115470886f },
	{ -0.26286599040031433f, 0.9510560035705566f, -0.16245999932289124f },
	{ 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.9554229974746704f, 0.2952420115470886f },
	{ -0.26286599040031433f, 0.9510560035705566f, 0.16245999932289124f },
	{ 0.2388560026884079f, 0.864188015460968f, 0.44286298751831055f },
	{ 0.26286599040031433f, 0.9510560035705566f, 0.16245999932289124f },
	{ 0.5f, 0.80901700258255f, 0.30901700258255005f },
	{ 0.2388560026884079f, 0.864188015460968f, -0.44286298751831055f },
	{ 0.26286599040031433f, 0.9510560035705566f, -0.16245999932289124f },
	{ 0.5f, 0.80901700258255f, -0.30901700258255005f },
	{ 0.8506510257720947f, 0.5257310271263123f, 0.0f },
	{ 0.7165669798851013f, 0.6817179918289185f, 0.1476210057735443f },
	{ 0.7165669798851013f, 0.6817179918289185f, -0.1476210057735443f },
	{ 0.5257310271263123f, 0.8506510257720947f, 0.0f },
	{ 0.4253250062465668f, 0.6881909966468811f, 0.587785005569458f },
	{ 0.864188015460968f, 0.44286298751831055f, 0.2388560026884079f },
	{ 0.6881909966468811f, 0.587785005569458f, 0.4253250062465668f },
	{ 0.80901700258255f, 0.30901700258255005f, 0.5f },
	{ 0.6817179918289185f, 0.1476210057735443f, 0.7165669798851013f },
	{ 0.587785005569458f, 0.4253250062465668f, 0.6881909966468811f },
	{ 0.9554229974746704f, 0.2952420115470886f, 0.0f },
	{ 1.0f, 0.0f, 0.0f },
	{ 0.9510560035705566f, 0.16245999932289124f, 0.26286599040031433f },
	{ 0.8506510257720947f, -0.5257310271263123f, 0.0f },
	{ 0.9554229974746704f, -0.2952420115470886f, 0.0f },
	{ 0.864188015460968f, -0.44286298751831055f, 0.2388560026884079f },
	{ 0.9510560035705566f, -0.16245999932289124f, 0.26286599040031433f },
	{ 0.80901700258255f, -0.30901700258255005f, 0.5f },
	{ 0.6817179918289185f, -0.1476210057735443f, 0.7165669798851013f },
	{ 0.8506510257720947f, 0.0f, 0.5257310271263123f },
	{ 0.864188015460968f, 0.44286298751831055f, -0.2388560026884079f },
	{ 0.80901700258255f, 0.30901700258255005f, -0.5f },
	{ 0.9510560035705566f, 0.16245999932289124f, -0.26286599040031433f },
	{ 0.5257310271263123f, 0.0f, -0.8506510257720947f },
	{ 0.6817179918289185f, 0.1476210057735443f, -0.7165669798851013f },
	{ 0.6817179918289185f, -0.1476210057735443f, -0.7165669798851013f },
	{ 0.8506510257720947f, 0.0f, -0.5257310271263123f },
	{ 0.80901700258255f, -0.30901700258255005f, -0.5f },
	{ 0.864188015460968f, -0.44286298751831055f, -0.2388560026884079f },
	{ 0.9510560035705566f, -0.16245999932289124f, -0.26286599040031433f },
	{ 0.1476210057735443f, 0.7165669798851013f, -0.6817179918289185f },
	{ 0.30901700258255005f, 0.5f, -0.80901700258255f },
	{ 0.4253250062465668f, 0.6881909966468811f, -0.587785005569458f },
	{ 0.44286298751831055f, 0.2388560026884079f, -0.864188015460968f },
	{ 0.587785005569458f, 0.4253250062465668f, -0.6881909966468811f },
	{ 0.6881909966468811f, 0.587785005569458f, -0.4253250062465668f },
	{ -0.1476210057735443f, 0.7165669798851013f, -0.6817179918289185f },
	{ -0.30901700258255005f, 0.5f, -0.80901700258255f },
	{ 0.0f, 0.5257310271263123f, -0.8506510257720947f },
	{ -0.5257310271263123f, 0.0f, -0.8506510257720947f },
	{ -0.44286298751831055f, 0.2388560026884079f, -0.864188015460968f },
	{ -0.2952420115470886f, 0.0f, -0.9554229974746704f },
	{ -0.16245999932289124f, 0.26286599040031433f, -0.9510560035705566f },
	{ 0.0f, 0.0f, -1.0f },
	{ 0.2952420115470886f, 0.0f, -0.9554229974746704f },
	{ 0.16245999932289124f, 0.26286599040031433f, -0.9510560035705566f },
	{ -0.44286298751831055f, -0.2388560026884079f, -0.864188015460968f },
	{ -0.30901700258255005f, -0.5f, -0.80901700258255f },
	{ -0.16245999932289124f, -0.26286599040031433f, -0.9510560035705566f },
	{ 0.0f, -0.8506510257720947f, -0.5257310271263123f },
	{ -0.1476210057735443f, -0.7165669798851013f, -0.6817179918289185f },
	{ 0.1476210057735443f, -0.7165669798851013f, -0.6817179918289185f },
	{ 0.0f, -0.5257310271263123f, -0.8506510257720947f },
	{ 0.30901700258255005f, -0.5f, -0.80901700258255f },
	{ 0.44286298751831055f, -0.2388560026884079f, -0.864188015460968f },
	{ 0.16245999932289124f, -0.26286599040031433f, -0.9510560035705566f },
	{ 0.2388560026884079f, -0.864188015460968f, -0.44286298751831055f },
	{ 0.5f, -0.80901700258255f, -0.30901700258255005f },
	{ 0.4253250062465668f, -0.6881909966468811f, -0.587785005569458f },
	{ 0.7165669798851013f, -0.6817179918289185f, -0.1476210057735443f },
	{ 0.6881909966468811f, -0.587785005569458f, -0.4253250062465668f },
	{ 0.587785005569458f, -0.4253250062465668f, -0.6881909966468811f },
	{ 0.0f, -0.9554229974746704f, -0.2952420115470886f },
	{ 0.0f, -1.0f, 0.0f },
	{ 0.26286599040031433f, -0.9510560035705566f, -0.16245999932289124f },
	{ 0.0f, -0.8506510257720947f, 0.5257310271263123f },
	{ 0.0f, -0.9554229974746704f, 0.2952420115470886f },
	{ 0.2388560026884079f, -0.864188015460968f, 0.44286298751831055f },
	{ 0.26286599040031433f, -0.9510560035705566f, 0.16245999932289124f },
	{ 0.5f, -0.80901700258255f, 0.30901700258255005f },
	{ 0.7165669798851013f, -0.6817179918289185f, 0.1476210057735443f },
	{ 0.5257310271263123f, -0.8506510257720947f, 0.0f },
	{ -0.2388560026884079f, -0.864188015460968f, -0.44286298751831055f },
	{ -0.5f, -0.80901700258255f, -0.30901700258255005f },
	{ -0.26286599040031433f, -0.9510560035705566f, -0.16245999932289124f },
	{ -0.8506510257720947f, -0.5257310271263123f, 0.0f },
	{ -0.7165669798851013f, -0.6817179918289185f, -0.1476210057735443f },
	{ -0.7165669798851013f, -0.6817179918289185f, 0.1476210057735443f },
	{ -0.5257310271263123f, -0.8506510257720947f, 0.0f },
	{ -0.5f, -0.80901700258255f, 0.30901700258255005f },
	{ -0.2388560026884079f, -0.864188015460968f, 0.44286298751831055f },
	{ -0.26286599040031433f, -0.9510560035705566f, 0.16245999932289124f },
	{ -0.864188015460968f, -0.44286298751831055f, 0.2388560026884079f },
	{ -0.80901700258255f, -0.30901700258255005f, 0.5f },
	{ -0.6881909966468811f, -0.587785005569458f, 0.4253250062465668f },
	{ -0.6817179918289185f, -0.1476210057735443f, 0.7165669798851013f },
	{ -0.44286298751831055f, -0.2388560026884079f, 0.864188015460968f },
	{ -0.587785005569458f, -0.4253250062465668f, 0.6881909966468811f },
	{ -0.30901700258255005f, -0.5f, 0.80901700258255f },
	{ -0.1476210057735443f, -0.7165669798851013f, 0.6817179918289185f },
	{ -0.4253250062465668f, -0.6881909966468811f, 0.587785005569458f },
	{ -0.16245999932289124f, -0.26286599040031433f, 0.9510560035705566f },
	{ 0.44286298751831055f, -0.2388560026884079f, 0.864188015460968f },
	{ 0.16245999932289124f, -0.26286599040031433f, 0.9510560035705566f },
	{ 0.30901700258255005f, -0.5f, 0.80901700258255f },
	{ 0.1476210057735443f, -0.7165669798851013f, 0.6817179918289185f },
	{ 0.0f, -0.5257310271263123f, 0.8506510257720947f },
	{ 0.4253250062465668f, -0.6881909966468811f, 0.587785005569458f },
	{ 0.587785005569458f, -0.4253250062465668f, 0.6881909966468811f },
	{ 0.6881909966468811f, -0.587785005569458f, 0.4253250062465668f },
	{ -0.9554229974746704f, 0.2952420115470886f, 0.0f },
	{ -0.9510560035705566f, 0.16245999932289124f, 0.26286599040031433f },
	{ -1.0f, 0.0f, 0.0f },
	{ -0.8506510257720947f, 0.0f, 0.5257310271263123f },
	{ -0.9554229974746704f, -0.2952420115470886f, 0.0f },
	{ -0.9510560035705566f, -0.16245999932289124f, 0.26286599040031433f },
	{ -0.864188015460968f, 0.44286298751831055f, -0.2388560026884079f },
	{ -0.9510560035705566f, 0.16245999932289124f, -0.26286599040031433f },
	{ -0.80901700258255f, 0.30901700258255005f, -0.5f },
	{ -0.864188015460968f, -0.44286298751831055f, -0.2388560026884079f },
	{ -0.9510560035705566f, -0.16245999932289124f, -0.26286599040031433f },
	{ -0.80901700258255f, -0.30901700258255005f, -0.5f },
	{ -0.6817179918289185f, 0.1476210057735443f, -0.7165669798851013f },
	{ -0.6817179918289185f, -0.1476210057735443f, -0.7165669798851013f },
	{ -0.8506510257720947f, 0.0f, -0.5257310271263123f },
	{ -0.6881909966468811f, 0.587785005569458f, -0.4253250062465668f },
	{ -0.587785005569458f, 0.4253250062465668f, -0.6881909966468811f },
	{ -0.4253250062465668f, 0.6881909966468811f, -0.587785005569458f },
	{ -0.4253250062465668f, -0.6881909966468811f, -0.587785005569458f },
	{ -0.587785005569458f, -0.4253250062465668f, -0.6881909966468811f },
	{ -0.6881909966468811f, -0.587785005569458f, -0.4253250062465668f },
};

static int	tracercount;

void R_DrawTracerParticles(void);

void R_TeleportSplash(vec3_t org)
{
	int			i, j;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i = -450; i < 450; i += 25)
	{
		for (j = -450; j < 450; j += 25)
		{
			if (!free_particles)
				return;

			p = (particle_t *)free_particles;
			free_particles = (int)p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)cl_time + 6.0f + (rand() & 0x1F) * 0.02f;
			p->type = pt_static;
			p->color = (float)(6 * ((rand() & 7) + 25));

			dir[0] = (float)(j + (rand() & 7));
			dir[1] = (float)(i + (rand() & 7));
			dir[2] = (float)(rand() >> 5);

			p->org[0] = org[0] + dir[0];
			p->org[1] = org[1] + dir[1];
			p->org[2] = org[2] + dir[2] + 100.0f;

			VectorSubtract(org, p->org, dir);
			VectorNormalize(dir);
			vel = (float)((rand() & 0x3F) + 100);
			VectorScale(dir, vel, p->vel);
		}
	}
}

void R_DarkFieldParticles2(vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	vec3_t		dir;
	float		vel;

	for (i = -16; i < 16; i += 4)
	{
		for (j = -16; j < 16; j += 4)
		{
			for (k = -24; k < 32; k += 4)
			{
				if (!free_particles)
					return;

				p = (particle_t *)free_particles;
				free_particles = (int)p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = (float)cl_time + 0.2f + (rand() & 7) * 0.02f;
				p->type = pt_slowgrav;
				p->color = (float)((rand() & 7) + 7);

				dir[0] = (float)(8 * j);
				dir[1] = (float)(8 * i);
				dir[2] = (float)(8 * k);

				p->org[0] = org[0] + (float)i + (rand() & 3);
				p->org[1] = org[1] + (float)j + (rand() & 3);
				p->org[2] = org[2] + (float)k + (rand() & 3);

				VectorNormalize(dir);
				vel = (float)((rand() & 0x3F) + 50);
				VectorScale(dir, vel, p->vel);
			}
		}
	}
}

void R_BeamParticles(vec3_t start, vec3_t end)
{
	vec3_t		vec;
	float		len;
	particle_t	*p;

	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);
	VectorScale(vec, 5.0f, vec);

	while (len > 0.0f)
	{
		len -= 5.0f;
		if (!free_particles)
			break;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy(vec3_origin, p->vel);
		p->die = (float)cl_time + 120.0f;
		p->type = pt_static;
		VectorCopy(start, p->org);
		p->color = 70.0f;

		VectorAdd(start, vec, start);
	}
}

void R_SparkStreaks(vec3_t pos, vec3_t dir, int color, int speed)
{
	int i;
	float zBias;

	VectorNormalize(dir);

	zBias = 0.05f;
	for (i = 0; i < 100; ++i)
	{
		vec3_t velDir;
		particle_t *p;

		if (!free_particles)
			return;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)cl_time + 2.0f;
		p->type = (ptype_t)9;
		p->color = (float)(rand() % 10 + color);
		VectorCopy(pos, p->org);

		VectorCopy(dir, velDir);
		velDir[2] -= zBias;
		zBias -= 0.005f;

		VectorScale(velDir, (float)speed, p->vel);
	}

	zBias = 0.075000003f;
	for (i = 0; i < speed / 5; ++i)
	{
		vec3_t velDir;
		float randNorm;
		float randSpeed;
		float dirScale;
		float velScale;
		int j;
		particle_t *p;

		if (!free_particles)
			return;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)cl_time + 3.0f;
		p->type = (ptype_t)8;
		p->color = (float)(rand() % 10 + color);
		VectorCopy(pos, p->org);

		VectorCopy(dir, velDir);
		velDir[2] -= zBias;
		zBias -= 0.005f;

		randNorm = (float)(rand() & 0x7FFF) / 32767.0f;
		randSpeed = (float)speed * randNorm;
		dirScale = randNorm * 1.7f;

		VectorScale(velDir, dirScale, velDir);
		VectorScale(velDir, randSpeed, p->vel);

		velScale = randSpeed;
		for (j = 0; j < 2; ++j)
		{
			if (!free_particles)
				return;

			p = (particle_t *)free_particles;
			free_particles = (int)p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)cl_time + 3.0f;
			p->type = (ptype_t)8;
			p->color = (float)(rand() % 10 + color);

			p->org[0] = (float)(rand() & 2) + pos[0] - 1.0f;
			p->org[1] = (float)(rand() & 2) + pos[1] - 1.0f;
			p->org[2] = (float)(rand() & 2) + pos[2] - 1.0f;

			VectorCopy(dir, velDir);
			velDir[2] -= zBias;
			VectorScale(velDir, dirScale, velDir);
			VectorScale(velDir, velScale, p->vel);
		}
	}
}

void R_StreakSplash(vec3_t pos, vec3_t dir, int color, int speed)
{
	int			i, j;
	particle_t	*p;
	vec3_t		velocity;
	vec3_t		basePos;
	int			waveSpeed;
	int			waveCount;

	waveSpeed = 3 * speed;
	VectorNormalize(dir);
	waveCount = speed / 2;

	for (i = 0; i < waveCount; i++)
	{

		basePos[0] = (float)(rand() & 6) + pos[0] - 3.0f;
		basePos[1] = (float)(rand() & 6) + pos[1] - 3.0f;
		basePos[2] = (float)(rand() & 6) + pos[2] - 3.0f;

		velocity[0] = (float)(rand() & 6) * 0.1f - 0.3f + dir[0];
		velocity[1] = (float)(rand() & 6) * 0.1f - 0.3f + dir[1];
		velocity[2] = (float)(rand() & 6) * 0.1f - 0.3f + dir[2];

		for (j = 0; j < 8; j++)
		{
			if (!free_particles)
				return;
			p = (particle_t *)free_particles;
			free_particles = (int)p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)cl_time + 1.5f;
			p->type = (ptype_t)9;
			p->color = (float)(rand() % 10 + color);

			p->org[0] = (float)((rand() & 2) - 1) * 0.5f + basePos[0];
			p->org[1] = (float)((rand() & 2) - 1) * 0.5f + basePos[1];
			p->org[2] = (float)((rand() & 2) - 1) * 0.5f + basePos[2];

			VectorScale(velocity, (float)waveSpeed, p->vel);
		}

		waveSpeed -= speed;
	}
}

void R_RocketTrail(vec3_t start, vec3_t end, int type)
{
	vec3_t		vec;
	float		len;
	int			j;
	particle_t	*p;
	float		dec;

	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	dec = 3.0f;
	if (type >= 128)
	{
		type -= 128;
		dec = 1.0f;
	}

	while (len > 0.0f)
	{
		len -= dec;

		if (!free_particles)
			return;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy(vec3_origin, p->vel);
		p->die = (float)cl_time + 2.0f;

		switch (type)
		{
		case 0:
			p->ramp = (float)(rand() & 3);
			p->color = (float)ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + (float)(rand() % 6 - 3);
			break;

		case 1:
			p->ramp = (float)((rand() & 3) + 2);
			p->color = (float)ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + (float)(rand() % 6 - 3);
			break;

		case 2:
			p->type = pt_grav;
			p->color = (float)(67 + (rand() & 3));
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + (float)(rand() % 6 - 3);
			break;

		case 3:
		case 5:
			p->die = (float)cl_time + 0.5f;
			p->type = pt_static;
			if (type == 3)
				p->color = (float)(52 + 2 * (tracercount & 4));
			else
				p->color = (float)(230 + 2 * (tracercount & 4));

			tracercount++;

			VectorCopy(start, p->org);

			if (tracercount & 1)
			{
				p->vel[0] = 30.0f * vec[1];
				p->vel[1] = 30.0f * -vec[0];
			}
			else
			{
				p->vel[0] = 30.0f * -vec[1];
				p->vel[1] = 30.0f * vec[0];
			}
			break;

		case 4:
			p->type = pt_grav;
			p->color = (float)(67 + (rand() & 3));
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + (float)(rand() % 6 - 3);
			len -= 3.0f;
			break;

		case 6:
			p->color = (float)(152 + (rand() & 3));
			p->type = pt_static;
			p->die = (float)cl_time + 0.3f;
			for (j = 0; j < 3; j++)
				p->org[j] = start[j] + (float)((rand() & 0xF) - 8);
			break;
		}

		VectorAdd(start, vec, start);
	}
}

int R_CullParticle(vec3_t point)
{
	int		i;

	for (i = 0; i < 4; i++)
	{
		float dot = DotProduct(point, frustum[i].normal) - frustum[i].dist;
		if (dot <= -10.0f)
			return 1;
	}

	return 0;
}

void R_DrawParticles(void)
{
	particle_t	*p;
	particle_t	**pp;
	float		scale;
	float		frametime;
	float		time1, time2, time3;
	float		dvel;
	float		grav;
	vec3_t		up, right;
	int			i;
	byte		color[4];

	GL_Bind(particletexture);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_TRIANGLES);

	VectorScale(vup, 1.5f, up);
	VectorScale(vright, 1.5f, right);

	frametime = (float)(cl_time - cl_oldtime);
	time3 = frametime * 15.0f;
	time2 = frametime * 10.0f;
	time1 = frametime * 5.0f;
	dvel = frametime * 4.0f;
	grav = frametime * sv_gravity.value * 0.05f;

	pp = &active_particles;
	while (1)
	{
		p = *pp;
		if (!p)
			break;

		if (p->die < cl_time)
		{

			*pp = p->next;
			p->next = (particle_t *)free_particles;
			free_particles = (int)p;
			continue;
		}

		scale = (p->org[0] - r_origin[0]) * vpn[0] +
				(p->org[1] - r_origin[1]) * vpn[1] +
				(p->org[2] - r_origin[2]) * vpn[2];
		if (scale >= 20.0f)
			scale = scale * 0.004f + 1.0f;
		else
			scale = 1.0f;

	*(int *)color = *(int *)(host_basepal + 3 * (int)p->color);
	color[3] = 255;
	glColor3ubv(color);

		glTexCoord2f(0.0f, 0.0f);
		glVertex3fv(p->org);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(p->org[0] + up[0] * scale, p->org[1] + up[1] * scale, p->org[2] + up[2] * scale);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(p->org[0] + right[0] * scale, p->org[1] + right[1] * scale, p->org[2] + right[2] * scale);

		VectorMA(p->org, frametime, p->vel, p->org);

		switch (p->type)
		{
		case pt_static:
			break;

		case pt_grav:
			p->vel[2] -= grav * 20.0f;
			break;

		case pt_slowgrav:
			p->vel[2] -= grav;
			break;

		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6.0f)
				p->die = -1.0f;
			else
				p->color = (float)ramp3[(int)p->ramp];
			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;
			if (p->ramp >= 8.0f)
				p->die = -1.0f;
			else
				p->color = (float)ramp1[(int)p->ramp];
			for (i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >= 8.0f)
				p->die = -1.0f;
			else
				p->color = (float)ramp2[(int)p->ramp];
			for (i = 0; i < 3; i++)
				p->vel[i] -= p->vel[i] * frametime;
			p->vel[2] -= grav;
			break;

		case pt_blob:
			for (i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;

		case pt_blob2:
			p->ramp += time2;
			if (p->ramp >= 9.0f)
				p->die = -1.0f;
			else
				p->color = (float)ramp4[(int)p->ramp];
			p->vel[2] -= grav * 5.0f;
			break;

		case (ptype_t)8:
			p->vel[2] -= grav * 4.0f;
			break;

		case (ptype_t)9:
			p->vel[2] -= grav * 8.0f;
			break;
		}

		pp = &p->next;
	}

	glEnd();

	R_DrawTracerParticles();

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void R_DrawTracerParticles(void)
{
	particle_t *p;
	float frametime;
	vec3_t up, right;

	if (!active_tracer_particles)
		return;

	frametime = (float)(cl_time - cl_oldtime);
	VectorScale(vup, 1.5f, up);
	VectorScale(vright, 1.5f, right);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(tracerRed.value, tracerGreen.value, tracerBlue.value, tracerAlpha.value);
	glCullFace(GL_FRONT);

	while (active_tracer_particles && active_tracer_particles->die < (float)cl_time)
	{
		p = active_tracer_particles;
		active_tracer_particles = p->next;
		p->next = (particle_t *)free_particles;
		free_particles = (int)p;
	}

	glBegin(GL_QUADS);

	for (p = active_tracer_particles; p; p = p->next)
	{
		float scale;
		vec3_t base;
		vec3_t corner_right;

		while (p->next && p->next->die < (float)cl_time)
		{
			particle_t *expired = p->next;
			p->next = expired->next;
			expired->next = (particle_t *)free_particles;
			free_particles = (int)expired;
		}

		scale = (p->org[0] - r_origin[0]) * vpn[0] +
				(p->org[1] - r_origin[1]) * vpn[1] +
				(p->org[2] - r_origin[2]) * vpn[2];
		if (scale >= 20.0f)
			scale = scale * 0.004f + 1.0f;
		else
			scale = 1.0f;

		base[0] = p->vel[0] * tracerLength.value * frametime + p->org[0];
		base[1] = p->vel[1] * tracerLength.value * frametime + p->org[1];
		base[2] = p->vel[2] * tracerLength.value * frametime + p->org[2];

		corner_right[0] = right[0] * scale + p->org[0];
		corner_right[1] = right[1] * scale + p->org[1];
		corner_right[2] = right[2] * scale + p->org[2];

		glTexCoord2f(0.0f, 0.0f);
		glVertex3fv(p->org);

		VectorMA(p->org, frametime, p->vel, p->org);

		glTexCoord2f(0.0f, 0.5f);
		glVertex3f(up[0] * scale + base[0], up[1] * scale + base[1], up[2] * scale + base[2]);

		glTexCoord2f(0.5f, 0.5f);
		glVertex3f(
			(up[0] + right[0]) * scale + base[0],
			(up[1] + right[1]) * scale + base[1],
			(up[2] + right[2]) * scale + base[2]);

		glTexCoord2f(0.5f, 0.0f);
		glVertex3fv(corner_right);
	}

	glEnd();
	glCullFace(GL_FRONT);
}

int R_InitParticles(void)
{
	int particleCount;
	void *hunk_res;
	particle_t *p;
	int i;

	extern char *Cvar_VariableString(const char *var_name);
	extern int atoi(const char *str);

	const char *particleCountStr = Cvar_VariableString("particles");
	if (particleCountStr && particleCountStr[0])
	{
		particleCount = atoi(particleCountStr);
		if (particleCount < 512)
			particleCount = 512;
	}
	else
	{
		particleCount = 8192;
	}

	extern int max_particles;
	extern particle_t *particle_pool;
	max_particles = particleCount;

	hunk_res = Hunk_AllocName(sizeof(particle_t) * particleCount, "particles");
	particle_pool = (particle_t *)hunk_res;

	free_particles = (int)particle_pool;
	p = particle_pool;
	for (i = 0; i < particleCount - 1; i++)
	{
		p->next = p + 1;
		p++;
	}
	p->next = NULL;

	return (int)hunk_res;
}

void R_DarkFieldParticles(vec3_t org)
{
	int i, j, k;
	particle_t *p;
	vec3_t dir;
	float vel;

	for (i = -16; i < 16; i += 8)
	{
		for (j = -16; j < 16; j += 8)
		{
			for (k = -24; k < 32; k += 8)
			{
				if (!free_particles)
					return;

				p = (particle_t *)free_particles;
				free_particles = (int)p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = (float)cl_time + 0.2f + (rand() & 7) * 0.02f;
				p->type = pt_slowgrav;
				p->color = (float)((rand() & 7) + 7);

				dir[0] = (float)(8 * j);
				dir[1] = (float)(8 * i);
				dir[2] = (float)(8 * k);

				p->org[0] = org[0] + (float)i + (rand() & 3);
				p->org[1] = org[1] + (float)j + (rand() & 3);
				p->org[2] = org[2] + (float)k + (rand() & 3);

				VectorNormalize(dir);
				vel = (float)((rand() & 0x3F) + 50);
				VectorScale(dir, vel, p->vel);
			}
		}
	}
}

void R_EntityParticles(entity_t *ent)
{
	const double time = cl_time;
	float *ent_f;
	int i;
	particle_t *p;

	if (avelocities[0][0] == 0.0f)
	{
		float *v = (float *)avelocities;
		float *end = v + (162 * 3);

		for (; v < end; ++v)
			*v = (float)(rand() & 0xFF) * 0.01f;
	}

	ent_f = (float *)ent;

	for (i = 0; i < 162; i++)
	{
		if (!free_particles)
			break;

		{
			const double yaw = (double)avelocities[i][0] * time;
			const float cy = (float)cos(yaw);
			const float sy = (float)sin(yaw);
			const double pitch = (double)avelocities[i][1] * time;
			const float cp = (float)cos(pitch);
			const float sp = (float)sin(pitch);
			const double roll = (double)avelocities[i][2] * time;
			const float forward_x = sp * sy;
			const float forward_y = sp * cy;
			const float forward_z = -cp;

			(void)cos(roll);
			(void)sin(roll);

			p = (particle_t *)free_particles;
			free_particles = (int)p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)cl_time + 0.01f;
			p->color = 96.0f;
			p->type = pt_explode;

			p->org[0] = forward_x * 16.0f + r_avertexnormals[i][0] * 64.0f + ent_f[26];
			p->org[1] = forward_y * 16.0f + r_avertexnormals[i][1] * 64.0f + ent_f[27];
			p->org[2] = forward_z * 16.0f + r_avertexnormals[i][2] * 64.0f + ent_f[28];
		}
	}
}

void R_ClearParticles(void)
{
	extern int max_particles;
	extern particle_t *particle_pool;

	int i;
	particle_t *p;

	free_particles = (int)particle_pool;
	active_particles = NULL;
	active_tracer_particles = NULL;

	p = particle_pool;
	for (i = 0; i < max_particles - 1; i++)
	{
		p->next = p + 1;
		p++;
	}
	if (p)
		p->next = NULL;
}

void R_ReadPointFile_f(void)
{
	extern char loadname[32];
	char filename[256];
	FILE *f;
	int x, y, z;
	int count = 0;
	particle_t *p;

	sprintf(filename, "maps/%s.pts", loadname);

	f = fopen(filename, "r");
	if (!f)
	{
		Con_Printf("Couldn't open %s\n", filename);
		return;
	}

	Con_Printf("Reading %s...\n", filename);

	while (fscanf(f, "%d %d %d\n", &x, &y, &z) == 3)
	{
		count++;

		if (!free_particles)
		{
			Con_Printf("Not enough free particles\n");
			break;
		}

		p = (particle_t *)free_particles;
		free_particles = *(int *)(free_particles + 16);
		*(int *)((int)p + 16) = (int)active_particles;
		active_particles = p;

		p->die = cl_time + 99999.0f;
		p->type = 0;
		p->color = 15.0f;

		p->org[0] = (float)x;
		p->org[1] = (float)y;
		p->org[2] = (float)z;

		p->vel[0] = 0.0f;
		p->vel[1] = 0.0f;
		p->vel[2] = 0.0f;
	}

	fclose(f);
	Con_Printf("%d points read\n", count);
}

void R_ParseParticleEffect(void)
{
	extern float MSG_ReadCoord(void);
	extern int MSG_ReadChar(void);
	extern int MSG_ReadByte(void);

	vec3_t org, velocity;
	int count;
	int color;

	org[0] = MSG_ReadCoord();
	org[1] = MSG_ReadCoord();
	org[2] = MSG_ReadCoord();

	velocity[0] = (float)MSG_ReadChar() * 0.0625f;
	velocity[1] = (float)MSG_ReadChar() * 0.0625f;
	velocity[2] = (float)MSG_ReadChar() * 0.0625f;

	count = MSG_ReadByte();
	if (count == 255)
		count = 1024;

	color = MSG_ReadByte();

	R_RunParticleEffect(org, velocity, color, count);
}

void R_ParticleStatic(vec3_t *pos, vec3_t *vel, float die)
{
	particle_t *p;

	if (!free_particles)
		return;

	p = (particle_t *)free_particles;
	free_particles = *(int *)(free_particles + 16);
	p->next = active_tracer_particles;
	active_tracer_particles = p;

	p->die = die + cl_time;
	p->type = pt_static;
	p->color = 109.0f;

	p->org[0] = pos[0][0];
	p->org[1] = pos[0][1];
	p->org[2] = pos[0][2];

	p->vel[0] = vel[0][0];
	p->vel[1] = vel[0][1];
	p->vel[2] = vel[0][2];
}

void __cdecl R_ParticleExplosion(float *org)
{
	int i;
	particle_t *p;

	for (i = 0; i < 1024; ++i)
	{
		if (!free_particles)
			break;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)cl_time + 5.0;
		p->color = (float)ramp1[0];
		p->ramp = (float)(rand() & 3);

		p->type = pt_explode;
		if ((i & 1) == 0)
			p->type = pt_explode2;

		do
		{
			p->vel[0] = (float)(rand() % 1024 - 512);
			p->vel[1] = (float)(rand() % 1024 - 512);
			p->vel[2] = (float)(rand() % 1024 - 512);
		} while (p->vel[0] * p->vel[0] +
				 p->vel[1] * p->vel[1] +
				 p->vel[2] * p->vel[2] > 262144.0);

		p->org[0] = org[0] + p->vel[0] / 4.0;
		p->org[1] = org[1] + p->vel[1] / 4.0;
		p->org[2] = org[2] + p->vel[2] / 4.0;
	}
}

void __cdecl R_ParticleExplosion2(float *org, int m, int n)
{
	int i;
	particle_t *p;
	int count;

	count = 0;

	for (i = 0; i < 512; ++i)
	{
		if (!free_particles)
			break;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)cl_time + 0.3;
		p->type = pt_blob;
		p->color = (float)(count % n + m);
		count++;

		p->org[0] = (float)(rand() % 32 - 16) + org[0];
		p->vel[0] = (float)(rand() % 512 - 256);
		p->org[1] = (float)(rand() % 32 - 16) + org[1];
		p->vel[1] = (float)(rand() % 512 - 256);
		p->org[2] = (float)(rand() % 32 - 16) + org[2];
		p->vel[2] = (float)(rand() % 512 - 256);
	}
}

void __cdecl R_BlobExplosion(float *org)
{
	int i;
	particle_t *p;

	for (i = 0; i < 1024; ++i)
	{
		if (!free_particles)
			break;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)cl_time + (double)(rand() & 8) * 0.05 + 1.0;

		if ((i & 1) != 0)
		{

			p->type = pt_blob;
			p->color = (float)(rand() % 6 + 66);

			p->org[0] = (float)(rand() % 32 - 16) + org[0];
			p->org[1] = (float)(rand() % 32 - 16) + org[1];
			p->org[2] = (float)(rand() % 32 - 16) + org[2];

			p->vel[0] = (float)(rand() % 32 - 16);
			p->vel[1] = (float)(rand() % 512 - 256);
			p->vel[2] = (float)(rand() % 32 - 16);
		}
		else
		{

			p->type = pt_blob2;
			p->color = (float)(rand() % 6 + 150);

			p->org[0] = (float)(rand() % 32 - 16) + org[0];
			p->org[1] = (float)(rand() % 32 - 16) + org[1];
			p->org[2] = (float)(rand() % 32 - 16) + org[2];

			p->vel[0] = (float)(rand() % 32 - 16);
			p->vel[1] = (float)(rand() % 512 - 256);
			p->vel[2] = (float)(rand() % 32 - 16);
		}
	}
}

void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
	int i;
	particle_t *p;

	for (i = 0; i < count; ++i)
	{
		if (!free_particles)
			break;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		if (count == 1024)
		{
			p->die = (float)cl_time + 5.0f;
			p->color = (float)ramp2[0];
			p->ramp = (float)(rand() & 3);

			if ((i & 1) != 0)
			{
				p->type = pt_explode;
			}
			else
			{
				p->type = pt_explode2;
			}

			p->org[0] = (float)(rand() % 32 - 16) + org[0];
			p->org[1] = (float)(rand() % 32 - 16) + org[1];
			p->org[2] = (float)(rand() % 32 - 16) + org[2];

			p->vel[0] = (float)(rand() % 512 - 256);
			p->vel[1] = (float)(rand() % 512 - 256);
			p->vel[2] = (float)(rand() % 512 - 256);
		}
		else
		{
			p->die = (float)cl_time + (float)((double)(rand() % 5) * 0.1);
			p->type = pt_grav;
			p->color = (float)(color ^ (((unsigned char)color ^ (unsigned char)rand()) & 7));

			p->org[0] = (float)((rand() & 0xF) - 8) + org[0];
			p->org[1] = (float)((rand() & 0xF) - 8) + org[1];
			p->org[2] = (float)((rand() & 0xF) - 8) + org[2];

			p->vel[0] = dir[0] * 15.0f;
			p->vel[1] = dir[1] * 15.0f;
			p->vel[2] = dir[2] * 15.0f;
		}
	}
}

void R_SparkShower(vec3_t org)
{
	int i;
	particle_t *p;

	for (i = 0; i < 15; ++i)
	{
		if (!free_particles)
			break;

		p = (particle_t *)free_particles;
		free_particles = (int)p->next;
		p->next = active_particles;
		active_particles = p;

		p->org[0] = org[0];
		p->org[1] = org[1];
		p->org[2] = org[2];

		p->vel[0] = (float)((rand() & 0x1F) - 16);
		p->vel[1] = (float)((rand() & 0x1F) - 16);
		p->vel[2] = (float)(rand() & 0x3F);

		p->ramp = 0.0f;
		p->color = 254.0f;
		p->die = (float)cl_time + 3.0f;
		p->type = pt_blob2;
	}
}

void __cdecl R_LavaSplash(float *org)
{
	int x, y;
	particle_t *p;
	vec3_t dir;

	for (y = -128; y < 128; y += 8)
	{
		for (x = -128; x < 128; x += 8)
		{
			if (!free_particles)
				return;

			p = (particle_t *)free_particles;
			free_particles = (int)p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)cl_time + (float)(rand() & 0x1F) * 0.02f + 2.0f;
			p->color = (float)((rand() & 7) + 224);
			p->type = pt_slowgrav;

			dir[0] = (float)(x + (rand() & 7));
			dir[1] = (float)(y + (rand() & 7));
			dir[2] = 255.0f;

			p->org[0] = org[0] + dir[0];
			p->org[1] = org[1] + dir[1];
			p->org[2] = org[2] + (float)(rand() & 0x3F);

			VectorNormalize(dir);
			VectorScale(dir, (float)((rand() & 0x3F) + 50), p->vel);
		}
	}
}
