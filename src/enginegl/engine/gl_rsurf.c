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
// gl_rsurf.c: surface-related refresh code

#include "quakedef.h"

#define BLOCK_WIDTH		128
#define BLOCK_HEIGHT	128

#define MAX_LIGHTMAPS	64

#define RTABLE_SIZE		20		// random tiling of '-' textures

#define COLINEAR_EPSILON	0.001f

int		blocklights[18*18*3];

int		rtable[RTABLE_SIZE*RTABLE_SIZE];

int		gl_lightmap_bytes;			// 1, 2, or 4
int		gl_lightmap_components;

static int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
static glpoly_t		*lightmap_polys[MAX_LIGHTMAPS];
static qboolean		lightmap_modified[MAX_LIGHTMAPS];
static byte			lightmaps[MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT * 4];

model_t		*currentmodel;
int			gl_c_tjunctions;

int			lightmap_textures;

int			r_k_polys = 0;

cvar_t		r_fastturb = { "r_fastturb", "0" };

msurface_t	*decal_list[MAX_DECAL_SURFS];
int			decal_list_count;

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights(msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;
	dlight_t	*dl;
	float		*lightorigin;
	float		*surfnormal;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
			continue;		// not lit by this light

		dl = &cl_dlights[lnum];
		lightorigin = dl->origin;
		rad = dl->radius;
		surfnormal = surf->plane->normal;

		dist = DotProduct(lightorigin, surfnormal) - surf->plane->dist;
		rad -= fabs(dist);
		minlight = dl->minlight;
		if (rad < minlight)
			continue;

		minlight = rad - minlight;

		impact[0] = tex->vecs[0][0] * lightorigin[0] +
					tex->vecs[0][1] * lightorigin[1] +
					tex->vecs[0][2] * lightorigin[2] + tex->vecs[0][3];
		impact[1] = tex->vecs[1][0] * lightorigin[0] +
					tex->vecs[1][1] * lightorigin[1] +
					tex->vecs[1][2] * lightorigin[2] + tex->vecs[1][3];

		local[0] = impact[0] - surf->texturemins[0];
		local[1] = impact[1] - surf->texturemins[1];

		for (t = 0; t < tmax; t++)
		{
			td = (int)local[1] - t * 16;
			if (td < 0)
				td = -td;

			for (s = 0; s < smax; s++)
			{
				sd = (int)local[0] - s * 16;
				if (sd < 0)
					sd = -sd;

				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);

				if (dist < minlight)
				{
					i = (int)((rad - dist) * 256.0f);
					blocklights[(t * smax + s) * 3 + 0] += (i * dl->color.rgb[0]) >> 8;
					blocklights[(t * smax + s) * 3 + 1] += (i * dl->color.rgb[1]) >> 8;
					blocklights[(t * smax + s) * 3 + 2] += (i * dl->color.rgb[2]) >> 8;
				}
			}
		}
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			t, s, i;
	int			size;
	byte		*lightmap;
	int			scale;
	int			maps;
	int			*bl;
	int			r;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (r_fullbright_value || !cl_worldmodel->lightdata)
	{
		for (i = 0; i < size; i++)
		{
			blocklights[i * 3 + 0] = 255 << 8;
			blocklights[i * 3 + 1] = 255 << 8;
			blocklights[i * 3 + 2] = 255 << 8;
		}
	}
	else
	{
		// clear to no light
		for (i = 0; i < size; i++)
		{
			blocklights[i * 3 + 0] = 0;
			blocklights[i * 3 + 1] = 0;
			blocklights[i * 3 + 2] = 0;
		}

		// add all the lightmaps
		if (lightmap)
		{
			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				surf->cached_light[maps] = scale;	// 8.8 fraction

				for (i = 0; i < size; i++)
				{
					blocklights[i * 3 + 0] += lightmap[i * 3 + 0] * scale;
					blocklights[i * 3 + 1] += lightmap[i * 3 + 1] * scale;
					blocklights[i * 3 + 2] += lightmap[i * 3 + 2] * scale;
				}
				lightmap += size * 3;	// skip to next lightmap
			}
		}

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights(surf);
	}

	// bound, invert, and shift
	if (gl_lightmap_format == GL_LUMINANCE)
	{
		bl = blocklights;
		for (t = 0; t < tmax; t++)
		{
			for (s = 0; s < smax; s++)
			{
				i = *bl >> 8;
				bl += 3;
				if (i > 255)
					i = 255;
				dest[s] = 255 - i;
			}
			dest += stride;
		}
	}
	else if (gl_lightmap_format == GL_RGBA)
	{
		bl = blocklights;
		for (t = 0; t < tmax; t++)
		{
			for (s = 0; s < smax; s++)
			{
				for (i = 0; i < 3; i++)
				{
					r = bl[i] >> 6;
					if (r > 1023)
						r = 1023;
					dest[i] = lightgammatable[r] >> 2;
				}
				dest[3] = 255;
				bl += 3;
				dest += 4;
			}
			dest += stride - smax * 4;
		}
	}
	else
	{
		Sys_Error("Bad lightmap format");
	}
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation(msurface_t *surf)
{
	texture_t	*base;
	int			reletive;
	int			count;
	int			i, j;

	base = surf->texinfo->texture;

	if (!rtable[0])
	{
		srand(0x12D3);
		for (i = 0; i < RTABLE_SIZE; i++)
		{
			for (j = 0; j < RTABLE_SIZE; j++)
				rtable[i * RTABLE_SIZE + j] = rand();
		}
	}

	if (currententity->frame != 0.0f && base->alternate_anims)
		base = base->alternate_anims;

	if (!base->anim_total)
		return base;

	if (base->name[0] == '-')
	{
		// random tiling, picked by the surface position
		int u = ((base->width << 16) + surf->texturemins[0]) / base->width;
		int v = ((base->height << 16) + surf->texturemins[1]) / base->height;
		reletive = rtable[(u % RTABLE_SIZE) * RTABLE_SIZE + (v % RTABLE_SIZE)] % base->anim_total;
	}
	else
	{
		reletive = (int)(cl_time * 10.0) % base->anim_total;
	}

	count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
================
DrawGLPoly
================
*/
void DrawGLPoly(glpoly_t *p)
{
	int		i;
	float	*v;

	glBegin(GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		glTexCoord2f(v[3], v[4]);
		glVertex3fv(v);
	}
	glEnd();
}

/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly(glpoly_t *p)
{
	int		i;
	float	*v;
	vec3_t	nv;
	float	s, t;

	glBegin(GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		glTexCoord2f(v[3], v[4]);

		s = sin(v[1] * 0.05f + realtime);
		t = sin(v[2] * 0.05f + realtime);
		nv[0] = v[0] + s * t * 8.0f;

		s = sin(v[0] * 0.05f + realtime);
		t = sin(v[2] * 0.05f + realtime);
		nv[1] = v[1] + s * t * 8.0f;

		nv[2] = v[2];

		glVertex3fv(nv);
	}
	glEnd();
}

/*
================
DrawGLWaterPolyLightmap
================
*/
void DrawGLWaterPolyLightmap(glpoly_t *p)
{
	int		i;
	float	*v;
	vec3_t	nv;
	float	s, t;

	glBegin(GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		glTexCoord2f(v[5], v[6]);

		s = sin(v[1] * 0.05f + realtime);
		t = sin(v[2] * 0.05f + realtime);
		nv[0] = v[0] + s * t * 8.0f;

		s = sin(v[0] * 0.05f + realtime);
		t = sin(v[2] * 0.05f + realtime);
		nv[1] = v[1] + s * t * 8.0f;

		nv[2] = v[2];

		glVertex3fv(nv);
	}
	glEnd();
}

/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly(msurface_t *s)
{
	glpoly_t	*p;
	texture_t	*t;
	int			i;
	float		*v;

	p = s->polys;

	if (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER))
	{
		if (s->flags & SURF_DRAWTURB)
		{
			GL_Bind(s->texinfo->texture->gl_texturenum);
			EmitWaterPolys(s, false);
		}
		else if (s->flags & SURF_DRAWSKY)
		{
			GL_Bind(solidskytexture);
			speedscale = realtime * 8.0f;
			speedscale = speedscale - (float)(int)speedscale;
			R_DrawSkyChain(s);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			GL_Bind(alphaskytexture);
			speedscale = realtime * 16.0f;
			speedscale = speedscale - (float)(int)speedscale;
			R_DrawSkyChain(s);

			if (gl_lightmap_format == GL_LUMINANCE)
				glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

			glDisable(GL_BLEND);
		}

		t = R_TextureAnimation(s);
		GL_Bind(t->gl_texturenum);
		DrawGLWaterPoly(p);

		GL_Bind(lightmap_textures + s->lightmaptexturenum);
		glEnable(GL_BLEND);
		DrawGLWaterPolyLightmap(p);
		glDisable(GL_BLEND);
	}
	else
	{
		t = R_TextureAnimation(s);
		GL_Bind(t->gl_texturenum);

		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
		{
			glTexCoord2f(v[3], v[4]);
			glVertex3fv(v);
		}
		glEnd();

		GL_Bind(lightmap_textures + s->lightmaptexturenum);
		glEnable(GL_BLEND);

		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
		{
			glTexCoord2f(v[5], v[6]);
			glVertex3fv(v);
		}
		glEnd();

		glDisable(GL_BLEND);
	}
}

/*
================
DrawGLSolidPoly

Untextured, in the entity's render color
================
*/
void DrawGLSolidPoly(msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		r, g, b;

	p = fa->polys;

	r = (float)currententity->rendercolor[0] / 256.0f;
	g = (float)currententity->rendercolor[1] / 256.0f;
	b = (float)currententity->rendercolor[2] / 256.0f;

	glColor4f(r, g, b, r_blend_alpha);
	glBegin(GL_POLYGON);

	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
		glVertex3fv(v);

	glEnd();
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps(void)
{
	int			i;
	glpoly_t	*p;
	glpoly_t	*p2;

	if (r_fullbright.value != 0.0f || gl_texsort.value == 0.0f)
		return;

	glDepthMask(GL_FALSE);		// don't bother writing Z

	switch (gl_lightmap_format)
	{
	case GL_LUMINANCE:
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		break;
	case GL_INTENSITY:
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case GL_RGBA:
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		glColor4f(2.0f / 3, 2.0f / 3, 2.0f / 3, 1.0f);
		break;
	}

	if (r_lightmap.value == 0.0f)
		glEnable(GL_BLEND);

	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		p = lightmap_polys[i];
		if (!p)
			continue;

		GL_Bind(lightmap_textures + i);

		if (lightmap_modified[i])
		{
			lightmap_modified[i] = false;
			glTexImage2D(GL_TEXTURE_2D, 0, gl_lightmap_components,
				BLOCK_WIDTH, BLOCK_HEIGHT, 0, gl_lightmap_format, GL_UNSIGNED_BYTE,
				lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * gl_lightmap_bytes);
		}

		do
		{
			if (!(p->flags & SURF_UNDERWATER))
			{
				if (p->flags & SURF_DRAWTURB)
				{
					// same waves as the water surface
					p2 = p;
					do
					{
						float *v = p2->verts[0];

						glBegin(GL_POLYGON);
						glColor3f(2.0f, 2.0f, 2.0f);

						for (int j = 0; j < p2->numverts; j++, v += VERTEXSIZE)
						{
							float x = v[0];
							float y = v[1];
							float z = v[2];

							glTexCoord2f(v[5], v[6]);

							z = turbsin[(int)(cl_time * 160.0 + x + y) & 255] * gl_wateramp.value + z;
							z = turbsin[(int)(x * 5.0 + cl_time * 171.0 - y) & 255] * gl_wateramp.value * 0.8f + z;

							glVertex3f(x, y, z);
						}

						glEnd();

						p2 = p2->next;
					}
					while (p2);
				}
				else
				{
					float *v = p->verts[0];

					glBegin(GL_POLYGON);

					for (int j = 0; j < p->numverts; j++, v += VERTEXSIZE)
					{
						glTexCoord2f(v[5], v[6]);
						glVertex3fv(v);
					}

					glEnd();
				}
			}
			else
			{
				DrawGLWaterPolyLightmap(p);
			}

			p = p->chain;
		}
		while (p);
	}

	glDisable(GL_BLEND);

	if (gl_lightmap_format == GL_LUMINANCE)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	}

	glDepthMask(GL_TRUE);		// back to normal Z buffering
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly(msurface_t *fa)
{
	texture_t	*t;
	int			maps;
	glpoly_t	*p;
	byte		*base;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY)
	{
		// warp texture, no lightmaps
		EmitSkyPolys(fa);
		return;
	}

	t = R_TextureAnimation(fa);
	GL_Bind(t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB)
	{
		// warp texture, no lightmaps
		EmitWaterPolys(fa, false);
	}
	else
	{
		p = fa->polys;

		if (currententity->rendermode == kRenderTransColor)
		{
			glDisable(GL_TEXTURE_2D);
			DrawGLSolidPoly(fa);
			glEnable(GL_TEXTURE_2D);
		}
		else
		{
			DrawGLPoly(p);
		}

		// add the poly to the proper lightmap chain
		base = lightmaps + fa->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * gl_lightmap_bytes;
		p->chain = lightmap_polys[fa->lightmaptexturenum];
		lightmap_polys[fa->lightmaptexturenum] = p;

		if (fa->pdecals)
		{
			if (decal_list_count >= MAX_DECAL_SURFS)
				Sys_Error("Too many decal surfaces");

			decal_list[decal_list_count++] = fa;
		}

		// check for lightmap modification
		for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
		{
			if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
				goto dynamic;
		}

		if (fa->dlightframe == r_framecount	// dynamic this frame
			|| fa->cached_dlight)			// dynamic previously
		{
dynamic:
			if (r_dynamic.value)
			{
				lightmap_modified[fa->lightmaptexturenum] = true;
				R_BuildLightMap(fa, base + (fa->light_s + fa->light_t * BLOCK_WIDTH) * gl_lightmap_bytes, BLOCK_WIDTH * gl_lightmap_bytes);
			}
		}
	}
}

/*
================
R_DrawAlphaSurfaces

Translucent water, drawn after everything else
================
*/
void R_DrawAlphaSurfaces(void)
{
	int			i;
	model_t		*world;
	texture_t	*tex;
	msurface_t	*chain;

	if (r_wateralpha.value == 1.0f)
		return;

	glLoadMatrixf(r_world_matrix);
	glEnable(GL_BLEND);
	glColor4f(1, 1, 1, r_wateralpha.value);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	world = cl_worldmodel;
	if (world->numtextures > 0)
	{
		for (i = 0; i < world->numtextures; i++)
		{
			tex = world->textures[i];
			if (!tex)
				continue;

			chain = tex->texturechain;
			if (!chain)
				continue;

			if (!(chain->flags & SURF_DRAWTURB))
				continue;

			GL_Bind(tex->gl_texturenum);
			for (; chain; chain = chain->texturechain)
				R_RenderBrushPoly(chain);

			tex->texturechain = NULL;
		}
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);
	glDisable(GL_BLEND);
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel(entity_t *e)
{
	int			i, k;
	vec3_t		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	texture_t	*t;
	qboolean	rotated;
	mplane_t	*pplane;
	float		dot;

	currententity = e;
	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i = 0; i < 3; i++)
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd(e->origin, clmodel->mins, mins);
		VectorAdd(e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	glColor3f(1, 1, 1);
	memset(lightmap_polys, 0, sizeof(lightmap_polys));

	VectorSubtract(r_refdef_vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	k = clmodel->firstmodelsurface;
	psurf = &clmodel->surfaces[k];

	// calculate dynamic lighting for bmodel if it's not an
	// instanced model
	if (clmodel->nummodelsurfaces && gl_flashblend.value == 0.0f)
	{
		dlight_t *dl = cl_dlights;

		for (i = 0; i < MAX_DLIGHTS; i++, dl++)
		{
			if (dl->die >= cl_time && dl->radius)
				R_MarkLights(dl, 1 << i, clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	glPushMatrix();

	e->angles[0] = -e->angles[0];	// stupid quake bug
	e->angles[2] = -e->angles[2];	// stupid quake bug
	R_RotateForEntity(e);
	e->angles[0] = -e->angles[0];	// stupid quake bug
	e->angles[2] = -e->angles[2];	// stupid quake bug

	if (e->rendermode)
	{
		if (e->rendermode == kRenderTransColor)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
		}
		else
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
			if (e->rendermode == kRenderTransAdd)
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			else
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glColor4f(1.0f, 1.0f, 1.0f, r_blend_alpha);
		}

		glEnable(GL_BLEND);
	}

	//
	// draw texture
	//
	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

		if ((!(psurf->flags & SURF_PLANEBACK) || dot >= -(float)BACKFACE_EPSILON) &&
			((psurf->flags & SURF_PLANEBACK) || dot <= (float)BACKFACE_EPSILON))
		{
			// facing away: only the underside of flat water is drawn
			if ((psurf->flags & SURF_DRAWTURB) && pplane->type == PLANE_Z)
			{
				c_brush_polys++;
				t = R_TextureAnimation(psurf);
				GL_Bind(t->gl_texturenum);
				EmitWaterPolys(psurf, true);
			}
		}
		else if (!(psurf->flags & SURF_DRAWTURB) || r_fastturb.value || pplane->type == PLANE_Z)
		{
			if (gl_texsort.value == 0.0f)
				R_DrawSequentialPoly(psurf);
			else
				R_RenderBrushPoly(psurf);
		}
	}

	if (e->rendermode)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glDisable(GL_BLEND);
	}

	R_DrawDecals();
	if (e->rendermode != kRenderTransAdd)
		R_BlendLightmaps();

	glPopMatrix();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_MirrorChain
================
*/
void R_MirrorChain(msurface_t *s)
{
	if (!r_mirror)
	{
		r_mirror = true;
		r_mirror_plane = s->plane;
	}
}

/*
================
DrawTextureChains
================
*/
void DrawTextureChains(void)
{
	model_t		*world;
	texture_t	*tex;
	msurface_t	*chain;
	int			i;
	int			count;

	world = cl_worldmodel;

	count = 100;

	for (i = 0; i < world->numtextures; i++)
	{
		tex = world->textures[i];
		if (!tex)
			continue;

		chain = tex->texturechain;
		if (!chain)
			continue;

		if (i == skytexturenum)
		{
			R_DrawSkyChain(chain);
			tex->texturechain = NULL;
			if (!count--)
			{
				count = 100;
				S_ExtraUpdate();
			}
			continue;
		}

		if (i == mirrortexturenum && r_mirroralpha.value != 1.0f)
		{
			R_MirrorChain(chain);
			continue;
		}

		if ((chain->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0f)
			continue;	// draw translucent water later

		for (; chain; chain = chain->texturechain)
			R_RenderBrushPoly(chain);

		tex->texturechain = NULL;
		if (!count--)
		{
			count = 100;
			S_ExtraUpdate();
		}
	}
}

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode(mnode_t *node)
{
	mnode_t		*n;
	int			side;
	mplane_t	*plane;
	double		dot;
	int			count;
	msurface_t	*surf;

	n = node;
	if (n->contents == CONTENTS_SOLID)
		return;		// solid

	while (n->visframe == r_visframecount && !R_CullBox(n->minmaxs, n->minmaxs + 3))
	{
		// if a leaf node, draw stuff
		if (n->contents < 0)
		{
			mleaf_t		*pleaf = (mleaf_t *)n;
			msurface_t	**mark = pleaf->firstmarksurface;
			int			c = pleaf->nummarksurfaces;

			if (c)
			{
				do
				{
					(*mark)->visframe = r_framecount;
					mark++;
				}
				while (--c);
			}

			// deal with model fragments in this leaf
			if (pleaf->efrags)
				R_StoreEfrags(&pleaf->efrags);

			return;
		}

		// node is just a decision point, so go down the apropriate sides

		// find which side of the node we are on
		plane = n->plane;
		switch (plane->type)
		{
		case PLANE_X:
			dot = modelorg[0];
			break;
		case PLANE_Y:
			dot = modelorg[1];
			break;
		case PLANE_Z:
			dot = modelorg[2];
			break;
		default:
			dot = DotProduct(modelorg, plane->normal);
			break;
		}

		dot -= plane->dist;
		side = (dot < 0.0);

		// recurse down the children, front side first
		R_RecursiveWorldNode(n->children[side]);

		// draw stuff
		count = n->numsurfaces;
		if (count)
		{
			surf = cl_worldmodel->surfaces + n->firstsurface;
			do
			{
				if (surf->visframe == r_framecount &&
					((surf->flags & SURF_UNDERWATER) ||
					 ((surf->flags & SURF_PLANEBACK) != 0) == (dot < 0.0)))
				{
					if (gl_texsort.value == 0.0f)
					{
						R_RenderBrushPoly(surf);
					}
					else if (!r_mirror || cl_worldmodel->textures[mirrortexturenum] != surf->texinfo->texture)
					{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				}

				surf++;
			}
			while (--count);
		}

		// recurse down the back side
		n = n->children[side ^ 1];
		if (n->contents == CONTENTS_SOLID)
			return;
	}
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld(void)
{
	entity_t	ent;

	memset(&ent, 0, sizeof(ent));
	ent.model = cl_worldmodel;

	VectorCopy(r_refdef_vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

	ent.colormap = r_colormap;

	decal_list_count = 0;
	glColor3f(1, 1, 1);

	memset(lightmap_polys, 0, sizeof(lightmap_polys));

	InitSkyPolygonBounds();

	R_RecursiveWorldNode(cl_worldmodel->nodes);

	r_k_polys = 0;

	if (gl_texsort.value != 0.0f)
		DrawTextureChains();

	S_ExtraUpdate();
	R_DrawDecals();
	R_BlendLightmaps();
	R_DrawSkyBox();
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves(void)
{
	byte		*vis;
	mnode_t		*node;
	int			i;
	byte		solid[4096];

	if ((oldviewleaf == viewleaf && r_novis.value == 0.0f) || r_mirror)
		return;

	r_visframecount++;
	oldviewleaf = viewleaf;

	if (r_novis.value == 0.0f)
	{
		vis = Mod_LeafPVS(viewleaf, cl_worldmodel);
	}
	else
	{
		vis = solid;
		memset(solid, 0xFF, (cl_worldmodel->numleafs + 7) >> 3);
	}

	for (i = 0; i < cl_worldmodel->numleafs; i++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			node = (mnode_t *)&cl_worldmodel->leafs[i + 1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			}
			while (node);
		}
	}
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

/*
================
AllocBlock

Returns a texture number and the position inside it
================
*/
int AllocBlock(int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	texnum = 0;

	while (1)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (best <= allocated[texnum][i + j])
					break;
				if (best2 < allocated[texnum][i + j])
					best2 = allocated[texnum][i + j];
			}

			if (j == w)
			{
				// this is a valid spot
				best = best2;
				*x = i;
				*y = best2;
			}
		}

		if (best + h <= BLOCK_HEIGHT)
			break;

		texnum++;
		if (texnum >= MAX_LIGHTMAPS)
			Sys_Error("AllocBlock: full");
	}

	for (i = 0; i < w; i++)
		allocated[texnum][*x + i] = best + h;

	return texnum;
}

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList(msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges;
	medge_t		*r_pedge;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;
	texture_t	*tex;
	float		texwidth, texheight;
	float		mins, mint;

	// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	//
	// draw texture
	//
	poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));

	poly->next = fa->polys;
	fa->polys = poly;
	poly->numverts = lnumverts;
	poly->flags = fa->flags;

	tex = fa->texinfo->texture;
	texwidth = tex->width;
	texheight = tex->height;

	mins = fa->texturemins[0];
	mint = fa->texturemins[1];

	for (i = 0; i < lnumverts; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		const float s_raw = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s = s_raw / texwidth;

		const float t_raw = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t = t_raw / texheight;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		poly->verts[i][5] = (s_raw - mins + (float)(fa->light_s * 16) + 8.0f) / (BLOCK_WIDTH * 16.0f);
		poly->verts[i][6] = (t_raw - mint + (float)(fa->light_t * 16) + 8.0f) / (BLOCK_HEIGHT * 16.0f);
	}

	//
	// remove co-linear points - Ed
	//
	if (gl_keeptjunctions.value == 0.0f && !(fa->flags & SURF_UNDERWATER))
	{
		for (i = 0; i < lnumverts; i++)
		{
			vec3_t	v1, v2;
			float	*prev, *this, *next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract(this, prev, v1);
			VectorNormalize(v1);
			VectorSubtract(next, prev, v2);
			VectorNormalize(v2);

			// skip co-linear points
			if (fabs(v1[0] - v2[0]) <= COLINEAR_EPSILON &&
				fabs(v1[1] - v2[1]) <= COLINEAR_EPSILON &&
				fabs(v1[2] - v2[2]) <= COLINEAR_EPSILON)
			{
				int		j;

				for (j = i + 1; j < lnumverts; j++)
				{
					int		k;

					for (k = 0; k < VERTEXSIZE; k++)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				lnumverts--;
				gl_c_tjunctions++;
				// retry next vertex next time, which is now current vertex
				i--;
			}
		}
	}

	poly->numverts = lnumverts;
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap(msurface_t *surf)
{
	int		smax, tmax;
	int		texnum;
	int		stride;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	texnum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);

	stride = gl_lightmap_bytes;
	surf->lightmaptexturenum = texnum;

	R_BuildLightMap(surf, lightmaps + stride * (surf->light_s + (surf->light_t + texnum * BLOCK_HEIGHT) * BLOCK_WIDTH), stride * BLOCK_WIDTH);
}

/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps(void)
{
	int		i, j;
	model_t	*m;
	int		texnum;
	int		size;

	memset(allocated, 0, sizeof(allocated));

	r_framecount = 1;		// no dlightcache

	if (!lightmap_textures)
	{
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	gl_lightmap_format = GL_RGBA;

	if (COM_CheckParm("-lm1"))
		gl_lightmap_format = GL_LUMINANCE;
	if (COM_CheckParm("-lma"))
		gl_lightmap_format = GL_RGB;
	if (COM_CheckParm("-lmi"))
		gl_lightmap_format = GL_INTENSITY;
	if (COM_CheckParm("-lm2"))
		gl_lightmap_format = GL_RGBA4;
	if (COM_CheckParm("-lm4"))
		gl_lightmap_format = GL_RGBA;

	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		gl_lightmap_bytes = 4;
		gl_lightmap_components = 3;
		break;
	case GL_RGB:
	case GL_LUMINANCE:
	case GL_INTENSITY:
		gl_lightmap_bytes = 1;
		gl_lightmap_components = 1;
		break;
	case GL_RGBA4:
		gl_lightmap_bytes = 2;
		gl_lightmap_components = 2;
		break;
	}

	for (j = 1; j < MAX_MODELS; j++)
	{
		m = cl_model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;

		currentmodel = m;

		for (i = 0; i < m->numsurfaces; i++)
		{
			GL_CreateSurfaceLightmap(m->surfaces + i);
			if (!(m->surfaces[i].flags & SURF_DRAWTURB))
				BuildSurfaceDisplayList(m->surfaces + i);
		}
	}

	//
	// upload all lightmaps that were filled
	//
	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break;		// no more used

		lightmap_modified[i] = false;
		texnum = lightmap_textures + i;

		GL_Bind(texnum);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		size = i * gl_lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
		glTexImage2D(GL_TEXTURE_2D, 0, gl_lightmap_components, BLOCK_WIDTH, BLOCK_HEIGHT, 0,
			gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps + size);
	}
}
