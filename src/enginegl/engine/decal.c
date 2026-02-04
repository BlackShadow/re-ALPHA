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
#include "decal.h"

static decal_t gDecalPool[MAX_DECALS];

static int gDecalCount;

static vec3_t g_decal_origin;
static int g_decal_radius;
static int g_decal_texture;
static int g_decal_flags;
static model_t *g_decal_model;
static texture_t *g_decal_texture_data;

extern msurface_t *decal_list[500];
extern int decal_list_count;

#define CLIP_LEFT   0
#define CLIP_RIGHT  1
#define CLIP_TOP    2
#define CLIP_BOTTOM 3

#define VERTEXSIZE 7

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

static void Intersect(float *one, float *two, int edge, float *out)
{
	float t;

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

static int SHClip(float *vert, int vertCount, float *out, int edge)
{
	int j, outCount;
	float *s, *p;

	outCount = 0;
	s = &vert[(vertCount - 1) * VERTEXSIZE];

	for (j = 0; j < vertCount; j++)
	{
		p = &vert[j * VERTEXSIZE];

		if (Inside(p, edge))
		{
			if (Inside(s, edge))
			{
				memcpy(out, p, VERTEXSIZE * sizeof(float));
				outCount++;
				out += VERTEXSIZE;
			}
			else
			{
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
				Intersect(s, p, edge, out);
				out += VERTEXSIZE;
				outCount++;
			}
		}
		s = p;
	}

	return outCount;
}

int R_DecalInit(void)
{
	memset(gDecalPool, 0, sizeof(gDecalPool));
	gDecalCount = 0;
	return 0;
}

decal_t *R_AllocateDecal(decal_t *pdecal)
{
	int count;

	if (!pdecal)
	{
		count = MAX_DECALS;
		do
		{
			++gDecalCount;
			if (gDecalCount >= MAX_DECALS)
				gDecalCount = 0;

			pdecal = &gDecalPool[gDecalCount];

			if ((pdecal->flags & FDECAL_PERMANENT) == 0)
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
						goto cleanup;
				}

				prev->pnext = pdecal->pnext;
			}
		}
	}

cleanup:
	pdecal->psurface = NULL;
	return pdecal;
}

void R_FindDecalSurface(mnode_t *node)
{
	mnode_t *current = node;

	while (current)
	{
		if (current->contents < 0)
			break;

		mplane_t *plane = current->plane;
		const float dist = plane->normal[0] * g_decal_origin[0] + plane->normal[1] * g_decal_origin[1] + plane->normal[2] * g_decal_origin[2] - plane->dist;
		const float radius = (float)g_decal_radius;

		if (radius >= dist)
		{
			if (-radius <= dist)
			{
				if (dist < 4.0f && dist > -4.0f)
				{
					msurface_t *surf = g_decal_model->surfaces + current->firstsurface;
					for (int surfaceIndex = 0; surfaceIndex < current->numsurfaces; ++surfaceIndex, ++surf)
					{
						mtexinfo_t *texinfo = surf->texinfo;
						const float texAxisLength = Length(texinfo->vecs[0]);
						if (texAxisLength == 0.0f)
							continue;

						const float s = texinfo->vecs[0][0] * g_decal_origin[0]
							+ texinfo->vecs[0][1] * g_decal_origin[1]
							+ texinfo->vecs[0][2] * g_decal_origin[2]
							+ texinfo->vecs[0][3]
							- (float)surf->texturemins[0];

						const float t = texinfo->vecs[1][0] * g_decal_origin[0]
							+ texinfo->vecs[1][1] * g_decal_origin[1]
							+ texinfo->vecs[1][2] * g_decal_origin[2]
							+ texinfo->vecs[1][3]
							- (float)surf->texturemins[1];

						const int scaledDecalW = (int)((float)g_decal_texture_data->width * texAxisLength);
						const int scaledDecalH = (int)((float)g_decal_texture_data->height * texAxisLength);

						const float decalS = (float)(s - (double)scaledDecalW * 0.5);
						const float decalT = (float)(t - (double)scaledDecalH * 0.5);

						if ((float)-scaledDecalW < decalS && (float)-scaledDecalH < decalT)
						{
							if ((float)(scaledDecalW + surf->extents[0]) >= decalS && (float)(scaledDecalH + surf->extents[1]) >= decalT)
							{
								const float scale = 1.0f / texAxisLength;

								texture_t *baseTexture = texinfo->texture;
								const float sNorm = (decalS + (float)surf->texturemins[0]) / (float)baseTexture->width;
								const float tNorm = (decalT + (float)surf->texturemins[1]) / (float)baseTexture->height;

								R_PlaceDecal(surf, g_decal_texture, scale, sNorm, tNorm);
							}
						}
					}
				}

				R_FindDecalSurface(current->children[0]);
			}

			current = current->children[1];
		}
		else
		{
			current = current->children[0];
		}
	}
}

static decal_t *R_DecalGetClosest(msurface_t *surf, int *decalCount, float s, float t)
{
	texture_t *baseTexture;
	float baseW;
	float baseH;
	float centerS;
	float centerT;
	float maxWidth;
	decal_t *best;
	int bestDist;

	best = NULL;
	bestDist = 0xFFFF;

	baseTexture = surf->texinfo->texture;
	*decalCount = 0;

	baseW = (float)baseTexture->width;
	baseH = (float)baseTexture->height;

	centerS = s * baseW + (float)(g_decal_texture_data->width >> 1);
	centerT = t * baseH + (float)(g_decal_texture_data->height >> 1);

	maxWidth = (float)g_decal_texture_data->width * 1.5f;

	for (decal_t *decal = surf->pdecals; decal; decal = decal->pnext)
	{
		texture_t *decalTexture = (texture_t *)Draw_GetDecal(decal->texture);
		if ((decal->flags & FDECAL_PERMANENT) != 0)
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

		const int major = (diffT <= diffS) ? diffS : diffT;
		const int minor = (diffT <= diffS) ? diffT : diffS;
		const int dist = major + (minor / 2);

		if (decal->scale * (float)dist < 8.0f)
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

int R_PlaceDecal(msurface_t *surf, int texture, float scale, float s, float t)
{
	int decalCount;
	decal_t *reuse = R_DecalGetClosest(surf, &decalCount, s, t);
	if (decalCount < 4)
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
	return 0;
}

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

	float clipA[64][VERTEXSIZE];
	float clipB[64][VERTEXSIZE];

	for (int surfaceIndex = 0; surfaceIndex < decal_list_count; ++surfaceIndex)
	{
		msurface_t *surf = decal_list[surfaceIndex];
		for (decal_t *decal = surf->pdecals; decal; decal = decal->pnext)
		{
			texture_t *decalTexture = (texture_t *)Draw_GetDecal(decal->texture);
			GL_Bind(decalTexture->gl_texturenum);

			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (float)GL_CLAMP);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (float)GL_CLAMP);

			texture_t *baseTexture = surf->texinfo->texture;
			const float scaleS = (float)baseTexture->width * decal->scale / (float)decalTexture->width;
			const float scaleT = (float)baseTexture->height * decal->scale / (float)decalTexture->height;

			GLfloat *v = (GLfloat *)((byte *)surf->polys + 16);
			const int numVerts = surf->polys->numverts;

			for (int i = 0; i < numVerts; ++i, v += VERTEXSIZE)
			{
				clipA[i][0] = v[0];
				clipA[i][1] = v[1];
				clipA[i][2] = v[2];
				clipA[i][3] = (v[3] - decal->dx) * scaleS;
				clipA[i][4] = (v[4] - decal->dy) * scaleT;
				clipA[i][5] = 0.0f;
				clipA[i][6] = 0.0f;
			}

			int count = SHClip(clipA[0], numVerts, clipB[0], CLIP_LEFT);
			count = SHClip(clipB[0], count, clipA[0], CLIP_RIGHT);
			count = SHClip(clipA[0], count, clipB[0], CLIP_TOP);
			count = SHClip(clipB[0], count, clipA[0], CLIP_BOTTOM);

			if (count)
			{
				glBegin(GL_POLYGON);
				for (int i = 0; i < count; ++i)
				{
					glTexCoord2f(clipA[i][3], clipA[i][4]);
					glVertex3fv(clipA[i]);
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

void R_DecalShoot(int texture, int entityIndex, vec3_t position, int flags)
{
	entity_t *entity = &cl_entities[entityIndex];
	model_t *model = *(model_t **)((byte *)entity + 168);

	VectorCopy(position, g_decal_origin);

	texture_t *decalData = (texture_t *)Draw_GetDecal(texture);
	if (entity && model && model->type == mod_brush && decalData)
	{
		mnode_t *node = model->nodes;
		if (entityIndex)
		{
			float *ent = (float *)entity;

			g_decal_origin[0] = position[0] + model->hulls[0].clip_mins[0] + ent[26];
			g_decal_origin[1] = position[1] + model->hulls[0].clip_mins[1] + ent[27];
			g_decal_origin[2] = position[2] + model->hulls[0].clip_mins[2] + ent[28];

			if (model->firstmodelsurface)
			{
				g_decal_origin[0] = position[0] + ent[2];
				g_decal_origin[1] = position[1] + ent[3];
				g_decal_origin[2] = position[2] + ent[4];

				g_decal_origin[0] -= ent[26];
				g_decal_origin[1] -= ent[27];
				g_decal_origin[2] -= ent[28];

				node = &model->nodes[model->hulls[0].firstclipnode];
			}

			if (ent[35] != 0.0f || ent[36] != 0.0f || ent[37] != 0.0f)
			{
				vec3_t forward;
				vec3_t right;
				vec3_t up;
				AngleVectors(ent + 35, forward, right, up);

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
