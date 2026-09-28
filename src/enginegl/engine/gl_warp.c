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
// gl_warp.c -- sky and water polygons

#include "quakedef.h"

#define TURBSCALE		(256.0 / (2 * M_PI))

#define SKY_TEX_START	2000	// texture numbers of the six sky box faces
#define SKYBOX_SIZE		256		// sky box face image width and height
#define SKYBOX_DIST		2048.0f

#define SUBDIVIDE_SIZE	64.0f

#define MAX_CLIP_VERTS	64
#define ON_EPSILON		0.1

#define SIDE_FRONT		0
#define SIDE_BACK		1
#define SIDE_ON			2

// sin(i * 2 * pi / 256) * 8
float turbsin[256] =
{
	0, 0.19633, 0.392541, 0.588517, 0.784137, 0.979285, 1.17384, 1.3677,
	1.56072, 1.75281, 1.94384, 2.1337, 2.32228, 2.50945, 2.69512, 2.87916,
	3.06147, 3.24193, 3.42044, 3.59689, 3.77117, 3.94319, 4.11282, 4.27998,
	4.44456, 4.60647, 4.76559, 4.92185, 5.07515, 5.22538, 5.37247, 5.51632,
	5.65685, 5.79398, 5.92761, 6.05767, 6.18408, 6.30677, 6.42566, 6.54068,
	6.65176, 6.75883, 6.86183, 6.9607, 7.05537, 7.14579, 7.23191, 7.31368,
	7.39104, 7.46394, 7.53235, 7.59623, 7.65552, 7.71021, 7.76025, 7.80562,
	7.84628, 7.88222, 7.91341, 7.93984, 7.96148, 7.97832, 7.99036, 7.99759,
	8, 7.99759, 7.99036, 7.97832, 7.96148, 7.93984, 7.91341, 7.88222,
	7.84628, 7.80562, 7.76025, 7.71021, 7.65552, 7.59623, 7.53235, 7.46394,
	7.39104, 7.31368, 7.23191, 7.14579, 7.05537, 6.9607, 6.86183, 6.75883,
	6.65176, 6.54068, 6.42566, 6.30677, 6.18408, 6.05767, 5.92761, 5.79398,
	5.65685, 5.51632, 5.37247, 5.22538, 5.07515, 4.92185, 4.76559, 4.60647,
	4.44456, 4.27998, 4.11282, 3.94319, 3.77117, 3.59689, 3.42044, 3.24193,
	3.06147, 2.87916, 2.69512, 2.50945, 2.32228, 2.1337, 1.94384, 1.75281,
	1.56072, 1.3677, 1.17384, 0.979285, 0.784137, 0.588517, 0.392541, 0.19633,
	9.79717e-16, -0.19633, -0.392541, -0.588517, -0.784137, -0.979285, -1.17384, -1.3677,
	-1.56072, -1.75281, -1.94384, -2.1337, -2.32228, -2.50945, -2.69512, -2.87916,
	-3.06147, -3.24193, -3.42044, -3.59689, -3.77117, -3.94319, -4.11282, -4.27998,
	-4.44456, -4.60647, -4.76559, -4.92185, -5.07515, -5.22538, -5.37247, -5.51632,
	-5.65685, -5.79398, -5.92761, -6.05767, -6.18408, -6.30677, -6.42566, -6.54068,
	-6.65176, -6.75883, -6.86183, -6.9607, -7.05537, -7.14579, -7.23191, -7.31368,
	-7.39104, -7.46394, -7.53235, -7.59623, -7.65552, -7.71021, -7.76025, -7.80562,
	-7.84628, -7.88222, -7.91341, -7.93984, -7.96148, -7.97832, -7.99036, -7.99759,
	-8, -7.99759, -7.99036, -7.97832, -7.96148, -7.93984, -7.91341, -7.88222,
	-7.84628, -7.80562, -7.76025, -7.71021, -7.65552, -7.59623, -7.53235, -7.46394,
	-7.39104, -7.31368, -7.23191, -7.14579, -7.05537, -6.9607, -6.86183, -6.75883,
	-6.65176, -6.54068, -6.42566, -6.30677, -6.18408, -6.05767, -5.92761, -5.79398,
	-5.65685, -5.51632, -5.37247, -5.22538, -5.07515, -4.92185, -4.76559, -4.60647,
	-4.44456, -4.27998, -4.11282, -3.94319, -3.77117, -3.59689, -3.42044, -3.24193,
	-3.06147, -2.87916, -2.69512, -2.50945, -2.32228, -2.1337, -1.94384, -1.75281,
	-1.56072, -1.3677, -1.17384, -0.979285, -0.784137, -0.588517, -0.392541, -0.19633
};

int		solidskytexture;
int		alphaskytexture;
float	speedscale;		// for top sky and bottom sky

byte	g_WaterColor[4];

static int		c_sky;
static vec3_t	sky_color;

static float	skymins[2][6], skymaxs[2][6];

static int vec_to_st[6] = {3, -3, 1, -1, -3, -3};
static int vec_to_t[6] = {-1, 1, 3, -3, -1, 1};

static char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

static int st_to_vec[6][3] =
{
	{3, -1, 2},
	{-3, 1, 2},

	{1, 3, 2},
	{-1, -3, 2},

	{-3, -1, -2},
	{-3, 1, -2}
};

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys(msurface_t *fa, int direction)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, t, os, ot;
	vec3_t		nv;

	g_WaterColor[0] = currententity->rendercolor[0];
	g_WaterColor[1] = currententity->rendercolor[1];
	g_WaterColor[2] = currententity->rendercolor[2];
	g_WaterColor[3] = 128;

	for (p = fa->polys; p; p = p->next)
	{
		if (direction)
			v = p->verts[p->numverts - 1];
		else
			v = p->verts[0];

		glBegin(GL_POLYGON);
		for (i = 0; i < p->numverts; i++)
		{
			os = v[3];
			ot = v[4];

			s = os + turbsin[(int)((ot * 0.125 + realtime) * TURBSCALE) & 255];
			s *= (1.0f / 64);

			t = ot + turbsin[(int)((os * 0.125 + realtime) * TURBSCALE) & 255];
			t *= (1.0f / 64);

			glTexCoord2f(s, t);

			// wave the surface
			VectorCopy(v, nv);
			nv[2] = turbsin[(int)(cl_time * 160.0 + nv[0] + nv[1]) & 255] * gl_wateramp.value + nv[2];
			nv[2] = turbsin[(int)(nv[0] * 5.0 + cl_time * 171.0 - nv[1]) & 255] * gl_wateramp.value * 0.8f + nv[2];

			glVertex3fv(nv);

			if (direction)
				v -= VERTEXSIZE;
			else
				v += VERTEXSIZE;
		}
		glEnd();
	}
}

/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys(msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, t;
	vec3_t		dir;
	float		length;

	for (p = fa->polys; p; p = p->next)
	{
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE)
		{
			VectorSubtract(v, r_refdef_vieworg, dir);
			dir[2] *= 3;	// flatten the sphere

			length = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
			length = sqrt(length);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			s = (dir[0] + speedscale) * (1.0f / 128);
			t = (dir[1] + speedscale) * (1.0f / 128);

			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();
	}
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitBothSkyLayers(msurface_t *fa)
{
	GL_Bind(solidskytexture);
	speedscale = cl_time * 8;
	speedscale -= (int)speedscale & 128;

	EmitSkyPolys(fa);

	glEnable(GL_BLEND);
	GL_Bind(alphaskytexture);
	speedscale = cl_time * 16;
	speedscale -= (int)speedscale & 128;

	EmitSkyPolys(fa);

	glDisable(GL_BLEND);
}

/*
==================
R_LoadSkys
==================
*/
void R_LoadSkys(void)
{
	int		i;
	FILE	*f;
	char	name[64];

	for (i = 0; i < 6; i++)
	{
		GL_Bind(SKY_TEX_START + i);
		sprintf(name, "gfx/env/bkgtst%s.tga", suf[i]);
		COM_FOpenFile(name, &f);
		if (!f)
			continue;
		LoadTGA(f);

		glTexImage2D(GL_TEXTURE_2D, 0, gl_alpha_format, SKYBOX_SIZE, SKYBOX_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, targa_rgba);

		free(targa_rgba);
		targa_rgba = NULL;

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}

/*
==============
EmitSkyVertex
==============
*/
static void EmitSkyVertex(float s, float t, int axis)
{
	vec3_t	v, b;
	int		j, k;

	b[0] = s * SKYBOX_DIST;
	b[1] = t * SKYBOX_DIST;
	b[2] = SKYBOX_DIST;

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_refdef_vieworg[j];
	}

	glTexCoord2f((s + 1) * 0.5f, (t + 1) * 0.5f);
	glVertex3fv(v);
}

/*
==============
R_DrawSkyBox
==============
*/
void R_DrawSkyBox(void)
{
	int		i;

	for (i = 0; i < 6; i++)
	{
		GL_Bind(SKY_TEX_START + i);
		glBegin(GL_QUADS);
		EmitSkyVertex(-1, -1, i);
		EmitSkyVertex(-1, 1, i);
		EmitSkyVertex(1, 1, i);
		EmitSkyVertex(1, -1, i);
		glEnd();
	}
}

/*
==============
MakeSkyVec
==============
*/
void MakeSkyVec(float s, float t, int axis)
{
	EmitSkyVertex(s, t, axis);
}

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky(texture_t *mt)
{
	int			i, j;
	byte		*src;
	unsigned	trans[SKYSIZE * SKYSIZE];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*dest;
	unsigned	p;

	src = (byte *)mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	dest = trans;
	for (i = 0; i < SKYSIZE; i++)
	{
		for (j = 0; j < SKYSIZE; j++)
		{
			p = trans[0];	// FIXME: never reads the sky pixels
			*dest++ = p;
			r += p & 0xff;
			g += (p >> 8) & 0xff;
			b += (p >> 16) & 0xff;
		}
	}

	transpix = (r / (SKYSIZE * SKYSIZE)) | ((g / (SKYSIZE * SKYSIZE)) << 8) | ((b / (SKYSIZE * SKYSIZE)) << 16) | 0xff000000;

	if (!solidskytexture)
		solidskytexture = texture_extension_number++;
	GL_Bind(solidskytexture);
	glTexImage2D(GL_TEXTURE_2D, 0, texture_extension_number, SKYSIZE, SKYSIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (i = 0; i < SKYSIZE * SKYSIZE; i += SKYSIZE)
	{
		for (j = 0; j < SKYSIZE; j++)
		{
			if (!src[j])
				trans[i + j] = transpix;
		}
		src += 2 * SKYSIZE;
	}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	GL_Bind(alphaskytexture);
	glTexImage2D(GL_TEXTURE_2D, 0, texture_extension_number, SKYSIZE, SKYSIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*
================
BoundPoly
================
*/
void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0; i < numverts; i++)
	{
		for (j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

static msurface_t *warpface;

/*
================
SubdividePolygon
================
*/
static void SubdividePolygon(int numverts, float *verts)
{
	int			i, j, k;
	vec3_t		mins, maxs;
	float		m;
	float		*v;
	vec3_t		front[64], back[64];
	int			f, b;
	float		dist[64];
	float		frac;

	if (numverts > 60)
		Sys_Error("numverts = %i", numverts);

	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++)
	{
		m = (maxs[i] + mins[i]) * 0.5f;
		m = floor(m / SUBDIVIDE_SIZE + 0.5f) * SUBDIVIDE_SIZE;
		if (maxs[i] - m >= 8 && m - mins[i] >= 8)
			break;
	}

	if (i == 3)
	{
		glpoly_t	*poly;
		mtexinfo_t	*tex;
		float		texmins, texmint, lights, lightt;
		float		s, t;
		float		*out;

		// small enough, so emit it
		poly = Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
		poly->next = warpface->polys;
		warpface->polys = poly;
		poly->numverts = numverts;
		poly->flags = warpface->flags;

		tex = warpface->texinfo;
		texmins = warpface->texturemins[0];
		texmint = warpface->texturemins[1];
		lights = warpface->light_s * 16;
		lightt = warpface->light_t * 16;

		out = poly->verts[0];
		for (j = 0, v = verts; j < numverts; j++, v += 3, out += VERTEXSIZE)
		{
			VectorCopy(v, out);
			s = DotProduct(v, tex->vecs[0]);
			t = DotProduct(v, tex->vecs[1]);
			out[3] = s;
			out[4] = t;

			// lightmap texture coordinates
			out[5] = (s + tex->vecs[0][3] - texmins + lights + 8) / (BLOCK_WIDTH * 16);
			out[6] = (t + tex->vecs[1][3] - texmint + lightt + 8) / (BLOCK_HEIGHT * 16);
		}
		return;
	}

	// cut it
	for (j = 0; j < numverts; j++)
		dist[j] = verts[j * 3 + i] - m;

	// wrap cases
	dist[j] = dist[0];
	VectorCopy(verts, verts + numverts * 3);

	f = b = 0;
	v = verts;
	for (j = 0; j < numverts; j++, v += 3)
	{
		if (dist[j] >= 0)
		{
			VectorCopy(v, front[f]);
			f++;
		}
		if (dist[j] <= 0)
		{
			VectorCopy(v, back[b]);
			b++;
		}
		if (dist[j] == 0 || dist[j + 1] == 0)
			continue;
		if ((dist[j] > 0) != (dist[j + 1] > 0))
		{
			// clip point
			frac = dist[j] / (dist[j] - dist[j + 1]);
			for (k = 0; k < 3; k++)
				front[f][k] = back[b][k] = (v[3 + k] - v[k]) * frac + v[k];
			f++;
			b++;
		}
	}

	SubdividePolygon(f, front[0]);
	SubdividePolygon(b, back[0]);
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface(msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = fa->numedges;
	for (i = 0; i < numverts; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy(vec, verts[i]);
	}

	SubdividePolygon(numverts, verts[0]);
}

/*
=============
SetupSkyPolygonClipping

Sets the water tint color.
=============
*/
int SetupSkyPolygonClipping(int r, int g, int b)
{
	sky_color[0] = r;
	sky_color[1] = g;
	sky_color[2] = b;
	g_WaterColor[0] = r;
	g_WaterColor[1] = g;
	g_WaterColor[2] = b;
	g_WaterColor[3] = 128;

	return r;
}

/*
=============
ReadWord
=============
*/
int ReadWord(FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + (b2 << 8));
}

/*
=================
DrawSkyPolygon
=================
*/
void DrawSkyPolygon(int nump, float *vecs)
{
	int		i, j, k;
	float	vx, vy, vz;	// sum of the verts
	float	ax, ay, az;
	float	s, dv;
	int		axis;

	c_sky++;

	// decide which face it maps to
	vx = vecs[0];
	vy = vecs[1];
	vz = vecs[2];
	for (i = 0; i < nump; i++)
	{
		vx = vecs[0] + vx;
		vecs += 3;
		vy = vecs[-2] + vy;
		vz = vecs[-1] + vz;
	}

	ax = fabs(vx);
	ay = fabs(vy);
	az = fabs(vz);
	if (ay < ax && az < ax)
		axis = (vx < 0) ? 1 : 0;
	else if (az >= ay)
		axis = (vz < 0) ? 5 : 4;
	else
		axis = (vy < 0) ? 3 : 2;

	// project new texture coords
	if (nump > 0)
	{
		j = vec_to_st[axis];
		k = vec_to_t[axis];
		for (i = 0; i < nump; i++, vecs += 3)
		{
			if (j <= 0)
				dv = -vecs[-j - 1];
			else
				dv = vecs[j - 1];

			if (k >= 0)
				s = vecs[k - 1] / dv;
			else
				s = -(vecs[-k - 1] / dv);

			if (skymins[0][axis] > s)
				skymins[0][axis] = s;
		}
	}
}

/*
================
ClipSkyPolygon
================
*/
void ClipSkyPolygon(int nump, float *vecs, int stage)
{
	float		*norm;
	int			i;
	double		d;
	float		newv[2][MAX_CLIP_VERTS * 3];
	qboolean	front, back;
	int			sides[MAX_CLIP_VERTS];

	if (nump > MAX_CLIP_VERTS - 2)
		Sys_Error("ClipSkyPolygon: too many verts");

	norm = (float *)st_to_vec[stage];	// FIXME: int table read as floats

	while (1)
	{
		if (stage >= 6)
		{
			// fully clipped, so draw it
			DrawSkyPolygon(nump, vecs);
			return;
		}

		front = back = false;
		for (i = 0; i < nump; i++, vecs += 3)
		{
			d = vecs[0] * norm[0] + vecs[1] * norm[1] + vecs[2] * norm[2];
			if (d <= ON_EPSILON)
			{
				if (d >= ON_EPSILON)
					sides[i] = SIDE_ON;
				else
				{
					back = true;
					sides[i] = SIDE_BACK;
				}
			}
			else
			{
				front = true;
				sides[i] = SIDE_FRONT;
			}
		}

		if (front && back)
			break;

		// not clipped
		norm += 3;
		stage++;
	}

	// FIXME: the clipped polygons are never built, the side flags are passed as counts
	ClipSkyPolygon(front, newv[0], stage + 1);
	ClipSkyPolygon(back, newv[1], stage + 1);
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain(msurface_t *s)
{
	msurface_t	*fa;
	int			i, numverts;
	vec3_t		verts[MAX_CLIP_VERTS];
	glpoly_t	*p;
	float		*v, *out;

	c_sky = 0;
	GL_Bind(solidskytexture);

	// calculate vertex values for sky box
	for (fa = s; fa; fa = fa->texturechain)
	{
		for (p = fa->polys; p; p = p->next)
		{
			numverts = p->numverts;
			for (i = 0, v = p->verts[0], out = verts[0]; i < numverts; i++, v += VERTEXSIZE, out += 3)
			{
				VectorSubtract(v, r_origin, out);
			}
			ClipSkyPolygon(numverts, verts[0], 0);
		}
	}
}

/*
==============
InitSkyPolygonBounds
==============
*/
void InitSkyPolygonBounds(void)
{
	int		i;

	for (i = 0; i < 6; i++)
	{
		skymins[1][i] = 9999;
		skymaxs[0][i] = -9999;
		skymins[0][i] = 9999;
		skymaxs[1][i] = -9999;
	}
}
