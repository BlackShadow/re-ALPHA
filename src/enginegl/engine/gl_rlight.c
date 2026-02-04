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

int r_dlightframecount;

vec3_t lightspot;
mplane_t *lightplane;

int d_lightstylevalue[256];

float v_blend[4];

extern vec3_t r_origin;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t vup;

extern int r_framecount;

extern double cl_time;

extern dlight_t cl_dlights[MAX_DLIGHTS];

extern model_t *cl_worldmodel;

cvar_t gl_flashblend = {"gl_flashblend", "0"};

lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
char cl_lightstyle_value[MAX_LIGHTSTYLES][64];
vec3_t r_light_rgb;

byte ambientlight_r = 0;
byte ambientlight_g = 0;
byte ambientlight_b = 0;

void R_AnimateLight(void)
{
	int i, j, k;

	i = (int)(cl_time * 10.0);

	for (j = 0; j < MAX_LIGHTSTYLES; j++)
	{
		if (!cl_lightstyle_value[j][0])
		{
			d_lightstylevalue[j] = 256;
			continue;
		}
		int len = strlen(cl_lightstyle_value[j]);
		k = i % len;
		k = cl_lightstyle_value[j][k] - 'a';
		k = k * 22;
		d_lightstylevalue[j] = k;
	}
}

void AddLightBlend(float r, float g, float b, float a2)
{
	float a;
	float mix;

	a = v_blend[3] + a2 * (1.0f - v_blend[3]);
	v_blend[3] = a;

	mix = a2 / a;

	v_blend[0] = v_blend[0] * (1.0f - mix) + r * mix;
	v_blend[1] = v_blend[1] * (1.0f - mix) + g * mix;
	v_blend[2] = v_blend[2] * (1.0f - mix) + b * mix;
}

void R_RenderDlight(dlight_t *light)
{
	int i, j;
	float a;
	vec3_t v;
	float rad;

	rad = light->radius * 0.35f;

	VectorSubtract(light->origin, r_origin, v);

	if (VectorLength(v) < rad)
	{
		AddLightBlend(1.0f, 0.5f, 0.0f, light->radius * 0.0003f);
		return;
	}

	glBegin(GL_TRIANGLE_FAN);
	glColor3f(0.2f, 0.1f, 0.0f);

	for (i = 0; i < 3; i++)
		v[i] = light->origin[i] - vpn[i] * rad;
	glVertex3fv(v);

	glColor3f(0.0f, 0.0f, 0.0f);

	for (i = 16; i >= 0; i--)
	{
		a = (float)i / 16.0f * M_PI * 2.0f;
		for (j = 0; j < 3; j++)
			v[j] = light->origin[j] + vright[j] * cos(a) * rad + vup[j] * sin(a) * rad;
		glVertex3fv(v);
	}

	glEnd();
}

void R_RenderDlights(void)
{
	int i;
	dlight_t *l;

	if (gl_flashblend.value == 0.0f)
		return;

	r_dlightframecount = r_framecount + 1;

	glDepthMask(0);
	glDisable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	l = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, l++)
	{
		if (l->die < cl_time || l->radius == 0.0f)
			continue;
		R_RenderDlight(l);
	}

	glColor3f(1.0f, 1.0f, 1.0f);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(1);
}

void R_MarkLights(dlight_t *light, int bit, mnode_t *node)
{
	mplane_t *splitplane;
	float dist;
	msurface_t *surf;
	int i;
	float maxdist;
	int s, t;
	mtexinfo_t *tex;
	vec3_t lightorigin;
	float radius;

	if (node->contents < 0)
		return;

	VectorCopy(light->origin, lightorigin);
	radius = light->radius;

	splitplane = node->plane;
	dist = DotProduct(lightorigin, splitplane->normal) - splitplane->dist;

	if (dist > radius)
	{
		R_MarkLights(light, bit, node->children[0]);
		return;
	}
	if (dist < -radius)
	{
		R_MarkLights(light, bit, node->children[1]);
		return;
	}

	maxdist = radius - fabs(dist);

	surf = cl_worldmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (maxdist < light->minlight)
			continue;

		tex = surf->texinfo;

		s = (int)(DotProduct(lightorigin, tex->vecs[0]) + tex->vecs[0][3]) - surf->texturemins[0];
		t = (int)(DotProduct(lightorigin, tex->vecs[1]) + tex->vecs[1][3]) - surf->texturemins[1];

		if (s >= -(int)maxdist && t >= -(int)maxdist &&
			s <= (int)maxdist + surf->extents[0] &&
			t <= (int)maxdist + surf->extents[1])
		{
			if (surf->dlightframe != r_dlightframecount)
			{
				surf->dlightbits = 0;
				surf->dlightframe = r_dlightframecount;
			}
			surf->dlightbits |= bit;
		}
	}

	R_MarkLights(light, bit, node->children[0]);
	R_MarkLights(light, bit, node->children[1]);
}

void R_PushDlights(void)
{
	int i;
	dlight_t *l;

	if (gl_flashblend.value != 0.0f)
		return;

	r_dlightframecount = r_framecount + 1;

	l = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, l++)
	{
		if (l->die < cl_time || l->radius == 0.0f)
			continue;
		R_MarkLights(l, 1 << i, cl_worldmodel->nodes);
	}
}

int RecursiveLightPoint(mnode_t *node, vec3_t start, vec3_t end)
{
	int r;
	float front, back, frac;
	int side;
	mplane_t *plane;
	vec3_t mid;
	msurface_t *surf;
	int s, t, ds, dt;
	int i;
	mtexinfo_t *tex;
	byte *lightmap;
	unsigned scale;
	int maps;
	int smax, tmax;

	if (node->contents < 0)
		return 0;

	plane = node->plane;
	front = DotProduct(start, plane->normal) - plane->dist;
	back = DotProduct(end, plane->normal) - plane->dist;
	side = front < 0;

	if ((back < 0) == side)
		return RecursiveLightPoint(node->children[side], start, end);

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	r = RecursiveLightPoint(node->children[side], start, mid);
	if (r)
		return r;

	if ((back < 0) == side)
		return 0;

	VectorCopy(mid, lightspot);
	lightplane = plane;

	surf = cl_worldmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;

		tex = surf->texinfo;

		s = (int)(DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3]);
		t = (int)(DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3]);

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		if (lightmap)
		{
			smax = (surf->extents[0] >> 4) + 1;
			tmax = (surf->extents[1] >> 4) + 1;
			lightmap += (dt * smax + ds) * 3;

			VectorClear(r_light_rgb);

			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				r_light_rgb[0] += (float)lightmap[0] * scale;
				r_light_rgb[1] += (float)lightmap[1] * scale;
				r_light_rgb[2] += (float)lightmap[2] * scale;
				lightmap += smax * tmax * 3;
			}

			r_light_rgb[0] /= 256.0f;
			r_light_rgb[1] /= 256.0f;
			r_light_rgb[2] /= 256.0f;
		}

		return 1;
	}

	return RecursiveLightPoint(node->children[!side], mid, end);
}

unsigned int *R_GetLightmap(unsigned int *lightrgb, vec3_t point, vec3_t end)
{
	if (cl_worldmodel->lightdata)
	{
		RecursiveLightPoint(cl_worldmodel->nodes, point, end);

		lightrgb[0] = (unsigned int)min(255.0f, r_light_rgb[0] + ambientlight_r);
		lightrgb[1] = (unsigned int)min(255.0f, r_light_rgb[1] + ambientlight_g);
		lightrgb[2] = (unsigned int)min(255.0f, r_light_rgb[2] + ambientlight_b);
	}
	else
	{
		lightrgb[0] = 255;
		lightrgb[1] = 255;
		lightrgb[2] = 255;
	}

	return lightrgb;
}

unsigned int *R_LightPoint(unsigned int *lightrgb, vec3_t point)
{
	vec3_t end;

	end[0] = point[0];
	end[1] = point[1];
	end[2] = point[2] - 2048.0f;

	return R_GetLightmap(lightrgb, point, end);
}

void R_InitLighting(void)
{
	Cvar_RegisterVariable(&gl_flashblend);
}
