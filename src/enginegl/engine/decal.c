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
// decal.c -- decals projected onto brush surfaces

#include "quakedef.h"

#define DECAL_DISTANCE		4.0f	// a surface this close to the impact point gets the decal
#define DECAL_OVERLAP_DIST	8.0f	// decals nearer than this overlap
#define MAX_OVERLAP_DECALS	4		// with this many overlapping, the closest one is reused

#define MAX_DECALCLIPVERT	64

// clip edges in texture space
#define CLIP_LEFT		0
#define CLIP_RIGHT		1
#define CLIP_TOP		2
#define CLIP_BOTTOM		3

static decal_t		gDecalPool[MAX_DECALS];
static int			gDecalCount;

static vec3_t		g_decal_origin;
static int			g_decal_radius;
static int			g_decal_texture;
static int			g_decal_flags;
static model_t		*g_decal_model;
static texture_t	*g_decal_texture_data;

/*
=============================================================================

  POLYGON CLIPPING

  Vertices are VERTEXSIZE floats, the decal texture s, t in [3], [4].
  The decal covers 0..1 in both.

=============================================================================
*/

/*
================
Inside
================
*/
static int Inside(float *vert, int edge)
{
	switch (edge)
	{
	case CLIP_LEFT:
		return vert[3] > 0.0f;
	case CLIP_RIGHT:
		return vert[3] < 1.0f;
	case CLIP_TOP:
		return vert[4] > 0.0f;
	case CLIP_BOTTOM:
		return vert[4] < 1.0f;
	}
	return 0;
}

/*
================
Intersect

The vertex where the edge from one to two crosses a clip edge
================
*/
static void Intersect(float *one, float *two, int edge, float *out)
{
	float	t;

	if (edge >= CLIP_TOP)
	{
		if (edge == CLIP_TOP)
		{
			out[4] = 0.0f;
			t = one[4] / (one[4] - two[4]);
		}
		else
		{
			out[4] = 1.0f;
			t = (one[4] - 1.0f) / (one[4] - two[4]);
		}

		out[3] = (two[3] - one[3]) * t + one[3];
	}
	else
	{
		if (edge == CLIP_LEFT)
		{
			out[3] = 0.0f;
			t = one[3] / (one[3] - two[3]);
		}
		else
		{
			out[3] = 1.0f;
			t = (one[3] - 1.0f) / (one[3] - two[3]);
		}

		out[4] = (two[4] - one[4]) * t + one[4];
	}

	out[0] = (two[0] - one[0]) * t + one[0];
	out[1] = (two[1] - one[1]) * t + one[1];
	out[2] = (two[2] - one[2]) * t + one[2];

	out[5] = 0.0f;
	out[6] = 0.0f;
}

/*
================
SHClip

Sutherland-Hodgman clip of a polygon against one edge
================
*/
static int SHClip(float *vert, int vertCount, float *out, int edge)
{
	int		j, outCount;
	float	*s, *p;

	outCount = 0;
	s = &vert[(vertCount - 1) * VERTEXSIZE];

	for (j = 0; j < vertCount; j++)
	{
		p = &vert[j * VERTEXSIZE];

		if (Inside(p, edge))
		{
			if (Inside(s, edge))
			{
				// both inside: add p
				memcpy(out, p, VERTEXSIZE * sizeof(float));
				outCount++;
				out += VERTEXSIZE;
			}
			else
			{
				// coming in: add the intersection and p
				Intersect(s, p, edge, out);
				out += VERTEXSIZE;
				outCount++;
				memcpy(out, p, VERTEXSIZE * sizeof(float));
				outCount++;
				out += VERTEXSIZE;
			}
		}
		else
		{
			if (Inside(s, edge))
			{
				// going out: add the intersection
				Intersect(s, p, edge, out);
				out += VERTEXSIZE;
				outCount++;
			}
		}
		s = p;
	}

	return outCount;
}

/*
=============================================================================

  DECAL LISTS

=============================================================================
*/

/*
================
R_DecalInit
================
*/
void R_DecalInit(void)
{
	memset(gDecalPool, 0, sizeof(gDecalPool));
	gDecalCount = 0;
}

/*
================
R_AllocateDecal

Unlinks pdecal from its surface, or the next non-permanent
decal from the pool when pdecal is NULL.
================
*/
decal_t *R_AllocateDecal(decal_t *pdecal)
{
	int		count;

	if (!pdecal)
	{
		count = MAX_DECALS;
		do
		{
			++gDecalCount;
			if (gDecalCount >= MAX_DECALS)
				gDecalCount = 0;

			pdecal = &gDecalPool[gDecalCount];

			if (!(pdecal->flags & FDECAL_PERMANENT))
				break;

			count--;
		}
		while (count);
	}

	msurface_t *surf = pdecal->psurface;
	if (surf != NULL)
	{
		decal_t *head = surf->pdecals;

		if (pdecal == head)
		{
			surf->pdecals = pdecal->pnext;
		}
		else
		{
			if (!head)
				Sys_Error("Bad decal list");

			decal_t *prev = head;
			if (prev->pnext)
			{
				while (true)
				{
					decal_t *next = prev->pnext;
					if (pdecal == next)
						break;

					prev = prev->pnext;
					if (!prev->pnext)
						goto done;		// not in the list
				}

				prev->pnext = pdecal->pnext;
			}
		}
	}

done:
	pdecal->psurface = NULL;
	return pdecal;
}

/*
================
R_FindDecalSurface

Places the decal on every surface near g_decal_origin that it overlaps.
================
*/
void R_FindDecalSurface(mnode_t *node)
{
	while (node)
	{
		if (node->contents < 0)
			break;

		mplane_t	*plane = node->plane;
		const float	dist = DotProduct(plane->normal, g_decal_origin) - plane->dist;
		const float	radius = (float)g_decal_radius;

		if (radius >= dist)
		{
			if (-radius <= dist)
			{
				if (dist < DECAL_DISTANCE && dist > -DECAL_DISTANCE)
				{
					msurface_t *surf = g_decal_model->surfaces + node->firstsurface;

					for (int i = 0; i < node->numsurfaces; ++i, ++surf)
					{
						mtexinfo_t	*tex = surf->texinfo;
						const float	texAxisLength = Length(tex->vecs[0]);

						if (texAxisLength == 0.0f)
							continue;

						const float s = DotProduct(tex->vecs[0], g_decal_origin)
							+ tex->vecs[0][3]
							- (float)surf->texturemins[0];

						const float t = DotProduct(tex->vecs[1], g_decal_origin)
							+ tex->vecs[1][3]
							- (float)surf->texturemins[1];

						// decal size in surface texels
						const int w = (int)((float)g_decal_texture_data->width * texAxisLength);
						const int h = (int)((float)g_decal_texture_data->height * texAxisLength);

						// top left corner of the decal
						const float decalS = (float)(s - (double)w * 0.5);
						const float decalT = (float)(t - (double)h * 0.5);

						if ((float)-w < decalS && (float)-h < decalT)
						{
							if ((float)(w + surf->extents[0]) >= decalS && (float)(h + surf->extents[1]) >= decalT)
							{
								const float scale = 1.0f / texAxisLength;

								texture_t	*base = tex->texture;
								const float	sNorm = (decalS + (float)surf->texturemins[0]) / (float)base->width;
								const float	tNorm = (decalT + (float)surf->texturemins[1]) / (float)base->height;

								R_PlaceDecal(surf, g_decal_texture, scale, sNorm, tNorm);
							}
						}
					}
				}

				R_FindDecalSurface(node->children[0]);
			}

			node = node->children[1];
		}
		else
		{
			node = node->children[0];
		}
	}
}

/*
================
R_DecalGetClosest

The non-permanent decal on surf nearest to s, t,
and how many decals overlap that spot.
================
*/
static decal_t *R_DecalGetClosest(msurface_t *surf, int *decalCount, float s, float t)
{
	texture_t	*base;
	float		baseW;
	float		baseH;
	float		centerS;
	float		centerT;
	float		maxWidth;
	decal_t		*best;
	int			bestDist;

	best = NULL;
	bestDist = 0xFFFF;

	base = surf->texinfo->texture;
	*decalCount = 0;

	baseW = (float)base->width;
	baseH = (float)base->height;

	centerS = s * baseW + (float)(g_decal_texture_data->width >> 1);
	centerT = t * baseH + (float)(g_decal_texture_data->height >> 1);

	// much bigger decals are never replaced by a smaller one
	maxWidth = (float)g_decal_texture_data->width * 1.5f;

	for (decal_t *decal = surf->pdecals; decal; decal = decal->pnext)
	{
		texture_t *decalTexture = (texture_t *)Draw_GetDecal(decal->texture);

		if (decal->flags & FDECAL_PERMANENT)
			continue;

		if ((float)decalTexture->width > maxWidth)
			continue;

		const float decalHalfW = (float)(decalTexture->width >> 1);
		const float decalHalfH = (float)(decalTexture->height >> 1);

		int diffS = (int)(centerS - (decal->dx * baseW + decalHalfW));
		if (diffS < 0)
			diffS = -diffS;

		int diffT = (int)(centerT - (decal->dy * baseH + decalHalfH));
		if (diffT < 0)
			diffT = -diffT;

		// approximate distance
		const int major = (diffT <= diffS) ? diffS : diffT;
		const int minor = (diffT <= diffS) ? diffT : diffS;
		const int dist = major + (minor / 2);

		if (decal->scale * (float)dist < DECAL_OVERLAP_DIST)
		{
			++*decalCount;
			if (!best || bestDist >= dist)
			{
				best = decal;
				bestDist = dist;
			}
		}
	}

	return best;
}

/*
================
R_PlaceDecal

Adds the decal to the end of the surface's list.
================
*/
void R_PlaceDecal(msurface_t *surf, int texture, float scale, float s, float t)
{
	int		decalCount;
	decal_t	*reuse = R_DecalGetClosest(surf, &decalCount, s, t);

	if (decalCount < MAX_OVERLAP_DECALS)
		reuse = NULL;

	decal_t *decal = R_AllocateDecal(reuse);

	decal->flags = (short)g_decal_flags;
	decal->dx = s;
	decal->dy = t;
	decal->texture = (short)texture;

	decal->pnext = NULL;
	if (surf->pdecals)
	{
		decal_t *walk = surf->pdecals;
		while (walk->pnext)
			walk = walk->pnext;
		walk->pnext = decal;
	}
	else
	{
		surf->pdecals = decal;
	}

	decal->psurface = surf;
	decal->scale = scale;
}

/*
================
R_DrawDecals

Draws the decals of the surfaces in decal_list.
================
*/
void R_DrawDecals(void)
{
	if (!decal_list_count)
		return;

	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glEnable(GL_POLYGON_OFFSET_FILL);

	if (r_fullbright.value == 0.0f || gl_ztrick.value < 0.5f)
		glPolygonOffset(1.0f, -4.0f);
	else
		glPolygonOffset(1.0f, 4.0f);

	float clipA[MAX_DECALCLIPVERT][VERTEXSIZE];
	float clipB[MAX_DECALCLIPVERT][VERTEXSIZE];

	for (int i = 0; i < decal_list_count; ++i)
	{
		msurface_t *surf = decal_list[i];

		for (decal_t *decal = surf->pdecals; decal; decal = decal->pnext)
		{
			texture_t *decalTexture = (texture_t *)Draw_GetDecal(decal->texture);
			GL_Bind(decalTexture->gl_texturenum);

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (float)GL_CLAMP);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (float)GL_CLAMP);

			texture_t	*base = surf->texinfo->texture;
			const float	scaleS = (float)base->width * decal->scale / (float)decalTexture->width;
			const float	scaleT = (float)base->height * decal->scale / (float)decalTexture->height;

			GLfloat		*v = surf->polys->verts[0];
			const int	numVerts = surf->polys->numverts;

			// surface texture coordinates to decal texture coordinates
			for (int j = 0; j < numVerts; ++j, v += VERTEXSIZE)
			{
				clipA[j][0] = v[0];
				clipA[j][1] = v[1];
				clipA[j][2] = v[2];
				clipA[j][3] = (v[3] - decal->dx) * scaleS;
				clipA[j][4] = (v[4] - decal->dy) * scaleT;
				clipA[j][5] = 0.0f;
				clipA[j][6] = 0.0f;
			}

			int count = SHClip(clipA[0], numVerts, clipB[0], CLIP_LEFT);
			count = SHClip(clipB[0], count, clipA[0], CLIP_RIGHT);
			count = SHClip(clipA[0], count, clipB[0], CLIP_TOP);
			count = SHClip(clipB[0], count, clipA[0], CLIP_BOTTOM);

			if (count)
			{
				glBegin(GL_POLYGON);
				for (int j = 0; j < count; ++j)
				{
					glTexCoord2f(clipA[j][3], clipA[j][4]);
					glVertex3fv(clipA[j]);
				}
				glEnd();
			}

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (float)GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (float)GL_REPEAT);
		}
	}

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	decal_list_count = 0;
}

/*
================
R_DecalShoot

Puts a decal at position on the brush model of entity entityIndex.
================
*/
void R_DecalShoot(int texture, int entityIndex, vec3_t position, int flags)
{
	entity_t	*entity = &cl_entities[entityIndex];
	model_t		*model = entity->model;

	VectorCopy(position, g_decal_origin);

	texture_t *decalData = (texture_t *)Draw_GetDecal(texture);
	if (model && model->type == mod_brush && decalData)
	{
		mnode_t *node = model->nodes;

		if (entityIndex)
		{
			g_decal_origin[0] = position[0] + model->hulls[0].clip_mins[0] + entity->origin[0];
			g_decal_origin[1] = position[1] + model->hulls[0].clip_mins[1] + entity->origin[1];
			g_decal_origin[2] = position[2] + model->hulls[0].clip_mins[2] + entity->origin[2];

			if (model->firstmodelsurface)
			{
				g_decal_origin[0] = position[0] + entity->baseline.origin[0];
				g_decal_origin[1] = position[1] + entity->baseline.origin[1];
				g_decal_origin[2] = position[2] + entity->baseline.origin[2];

				g_decal_origin[0] -= entity->origin[0];
				g_decal_origin[1] -= entity->origin[1];
				g_decal_origin[2] -= entity->origin[2];

				node = &model->nodes[model->hulls[0].firstclipnode];
			}

			// into the rotated model's space
			if (entity->angles[0] != 0.0f || entity->angles[1] != 0.0f || entity->angles[2] != 0.0f)
			{
				vec3_t	forward, right, up;

				AngleVectors(entity->angles, forward, right, up);

				const float x = g_decal_origin[0];
				const float y = g_decal_origin[1];
				const float z = g_decal_origin[2];

				g_decal_origin[0] = y * forward[1] + z * forward[2] + x * forward[0];
				g_decal_origin[1] = -(y * right[1] + x * right[0] + z * right[2]);
				g_decal_origin[2] = x * up[0] + y * up[1] + z * up[2];
			}
		}

		g_decal_model = model;
		g_decal_texture_data = decalData;
		g_decal_texture = texture;
		g_decal_flags = flags;
		g_decal_radius = (int)(decalData->width >> 1);

		R_FindDecalSurface(node);
	}
	else
	{
		Con_Printf("Decals must hit mod_brush!\n");
	}
}
