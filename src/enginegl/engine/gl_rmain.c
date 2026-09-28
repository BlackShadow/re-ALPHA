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
// gl_rmain.c

#include "quakedef.h"

// r_refdef_vrect is in 320x200 virtual screen units
#define VIRTUAL_WIDTH	320
#define VIRTUAL_HEIGHT	200

static void R_RenderScene(void);
static void R_SetupGL(void);

entity_t	*currententity = NULL;
float		modelorg[3];
float		r_refdef_vieworg[3];
float		r_refdef_viewangles[3];
int			r_refdef_vrect_x = 0;
int			r_refdef_vrect_y = 0;
int			r_refdef_vrect_width = 0;
int			r_refdef_vrect_height = 0;
int			envmap = 0;					// true during envmap command capture
int			r_framecount = 0;			// used for dlight push checking

mleaf_t		*oldviewleaf = NULL;
mleaf_t		*viewleaf = NULL;

int			r_mirror = 0;
mplane_t	*r_mirror_plane = NULL;

int			c_brush_polys = 0;
int			c_alias_polys = 0;

int			skytexturenum = -1;
int			mirrortexturenum = -1;		// quake texturenum, not gltexturenum

byte		*r_colormap = NULL;

/*
===============
R_NewMap
===============
*/
void R_NewMap(void)
{
	int			i;
	texture_t	*texture;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset(&cl_entities[0], 0, sizeof(cl_entities[0]));
	cl_entities[0].model = cl_worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	if (cl_worldmodel && cl_worldmodel->numleafs > 0)
	{
		for (i = 0; i < cl_worldmodel->numleafs; i++)
			cl_worldmodel->leafs[i].efrags = NULL;
	}

	viewleaf = NULL;

	R_ClearParticles();
	R_DecalInit();
	GL_BuildLightmaps();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;

	if (cl_worldmodel && cl_worldmodel->numtextures > 0)
	{
		for (i = 0; i < cl_worldmodel->numtextures; i++)
		{
			texture = cl_worldmodel->textures[i];
			if (!texture)
				continue;

			if (!Q_strncmp(texture->name, "sky", 3))
				skytexturenum = i;

			if (!Q_strncmp(texture->name, "window02", 10))
				mirrortexturenum = i;
		}
	}

	R_LoadSkys();
}

cvar_t	r_drawviewmodel = { "r_drawviewmodel", "1" };

cvar_t	r_norefresh = { "r_norefresh", "0" };
cvar_t	r_drawentities = { "r_drawentities", "1" };
cvar_t	r_drawworld = { "r_drawworld", "1" };
cvar_t	r_speeds = { "r_speeds", "0" };
cvar_t	r_timegraph = { "r_timegraph", "0" };
cvar_t	r_fullbright = { "r_fullbright", "0" };
cvar_t	r_lightmap = { "r_lightmap", "0" };
cvar_t	r_shadows = { "r_shadows", "1" };
cvar_t	r_drawflat = { "r_drawflat", "0" };
cvar_t	r_flowmap = { "r_flowmap", "0" };
cvar_t	r_mirroralpha = { "r_mirroralpha", "1" };
cvar_t	r_wateralpha = { "r_wateralpha", "1" };
cvar_t	r_dynamic = { "r_dynamic", "1" };
cvar_t	r_novis = { "r_novis", "0" };
cvar_t	r_decals = { "r_decals", "1" };

cvar_t	r_part_explode = { "r_part_explode", "1" };
cvar_t	r_part_trails = { "r_part_trails", "1" };
cvar_t	r_part_sparks = { "r_part_sparks", "1" };
cvar_t	r_part_gunshots = { "r_part_gunshots", "1" };
cvar_t	r_part_blood = { "r_part_blood", "1" };
cvar_t	r_part_telesplash = { "r_part_telesplash", "1" };

float		ambientlight = 0.0f;
float		shadelight = 0.0f;
float		shadevector[3];
int			lastposenum = 0;
int			view_lighting = 0;
int			view_lighting_ambient = 0;
float		*lighting_direction = NULL;

float		*r_avertexnormal_dots_ptr = NULL;
dlight_t	*light_list = NULL;
dlight_t	*light_list_end = NULL;
entity_t	viewent_entity;

int			playertextures = 0;			// up to 16 color translated skins

float		view_height = 0.0f;

vec3_t		lightvec;

mplane_t	frustum[4];

/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
int R_CullBox(float *mins, float *maxs)
{
	int		i;

	for (i = 0; i < 4; i++)
	{
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return 1;
	}

	return 0;
}

/*
=============
R_RotateForEntity

Interpolates the origin and angles of entities with lerp data.
=============
*/
void R_RotateForEntity(entity_t *ent)
{
	GLfloat	x = ent->origin[0];
	GLfloat	y = ent->origin[1];
	GLfloat	z = ent->origin[2];
	GLfloat	angles[3];

	angles[0] = ent->angles[0];
	angles[1] = ent->angles[1];
	angles[2] = ent->angles[2];

	if (ent->has_lerpdata)
	{
		const float	lerptime = ent->anim_time;
		float		frac = 0.0f;

		if ((double)lerptime + 0.2 > cl_time && ent->latched_anim_time != lerptime)
			frac = (float)((cl_time - (double)lerptime) / ((double)lerptime - (double)ent->latched_anim_time));

		x = x - (ent->lerp_origin[0] - ent->origin[0]) * frac;
		y = y - (ent->lerp_origin[1] - ent->origin[1]) * frac;
		z = z - (ent->lerp_origin[2] - ent->origin[2]) * frac;

		if (frac > 0.0f && frac < 1.5f)
		{
			const float	back = 1.0f - frac;

			for (int i = 0; i < 3; i++)
			{
				float	delta = ent->lerp_angles[i] - ent->angles[i];

				if (delta > 180.0f)
					delta -= 360.0f;
				else if (delta < -180.0f)
					delta += 360.0f;

				angles[i] = back * delta + angles[i];
			}
		}
	}

	angles[0] = -angles[0];
	glTranslatef(x, y, z);
	glRotatef(angles[1], 0.0f, 0.0f, 1.0f);
	glRotatef(angles[0], 0.0f, 1.0f, 0.0f);
	glRotatef(-angles[2], 1.0f, 0.0f, 0.0f);
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame(entity_t *entity)
{
	msprite_t			*psprite;
	mspritegroup_t		*pspritegroup;
	mspritegroupframe_t	*pframe;
	int					frame;

	psprite = (msprite_t *)entity->model->cache.data;
	frame = (int)entity->frame;

	if (psprite->numframes <= frame || frame < 0)
	{
		Con_Printf("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	pframe = &psprite->frames[frame];
	if (pframe->type == SPR_SINGLE)
		return pframe->frameptr;

	{
		float		*pintervals;
		float		fullinterval, targettime, time;
		int			i;

		pspritegroup = (mspritegroup_t *)pframe->frameptr;
		pintervals = pspritegroup->intervals;
		fullinterval = pintervals[pspritegroup->numframes - 1];

		time = entity->syncbase + (float)cl_time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - (float)((int)(time / fullinterval)) * fullinterval;

		for (i = 0; i < pspritegroup->numframes - 1; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		return pspritegroup->frames[i];
	}
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel(entity_t *entity)
{
	mspriteframe_t	*frame;
	int				rendermode;
	GLfloat			v[3];

	frame = R_GetSpriteFrame(entity);
	rendermode = currententity->rendermode;

	if (rendermode)
	{
		if (rendermode == kRenderTransColor)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
		}
		else
		{
			if (rendermode == kRenderTransAdd)
			{
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			}
			else
			{
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			glColor4f(1.0, 1.0, 1.0, r_blend_alpha);
		}

		glEnable(GL_BLEND);
	}
	else
	{
		glColor3f(1.0, 1.0, 1.0);
	}

	GL_Bind(frame->gl_texturenum);

	glEnable(GL_ALPHA_TEST);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0, 0.0);
	VectorMA(entity->origin, frame->down, vup, v);
	VectorMA(v, frame->left, vright, v);
	glVertex3fv(v);

	glTexCoord2f(0.0, 1.0);
	VectorMA(entity->origin, frame->up, vup, v);
	VectorMA(v, frame->left, vright, v);
	glVertex3fv(v);

	glTexCoord2f(1.0, 1.0);
	VectorMA(entity->origin, frame->up, vup, v);
	VectorMA(v, frame->right, vright, v);
	glVertex3fv(v);

	glTexCoord2f(1.0, 0.0);
	VectorMA(entity->origin, frame->down, vup, v);
	VectorMA(v, frame->right, vright, v);
	glVertex3fv(v);

	glEnd();

	glDisable(GL_ALPHA_TEST);

	if (rendermode)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glDisable(GL_BLEND);
	}
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame(aliashdr_t *paliashdr, int posenum)
{
	int			*order;
	trivertx_t	*verts;
	int			count;
	float		l;

	lastposenum = posenum;

	order = (int *)((byte *)paliashdr + paliashdr->commands);
	verts = (trivertx_t *)((byte *)paliashdr + posenum * paliashdr->poseverts + paliashdr->posedata);

	count = *order++;

	while (count)
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin(GL_TRIANGLE_STRIP);
		}

		do
		{
			// texture coordinates come from the draw list
			glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
			order += 2;

			// normals and vertexes come from the frame list
			l = r_avertexnormal_dots_ptr[verts->lightnormalindex] * shadelight;
			glColor3f(l, l, l);

			glVertex3f((float)verts->v[0], (float)verts->v[1], (float)verts->v[2]);
			verts++;

			count--;
		} while (count);

		glEnd();

		count = *order++;
	}
}

/*
=============
GL_DrawAliasShadow
=============
*/
void GL_DrawAliasShadow(aliashdr_t *paliashdr, int posenum)
{
	int			*order;
	trivertx_t	*verts;
	int			count;
	float		point[3];
	float		height;
	float		lheight;

	lheight = currententity->origin[2] - view_height;
	height = 1.0f - lheight;

	verts = (trivertx_t *)((byte *)paliashdr + posenum * paliashdr->poseverts + paliashdr->posedata);
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	count = *order;
	order++;

	while (count)
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin(GL_TRIANGLE_STRIP);
		}

		do
		{
			// texture coordinates come from the draw list
			order += 2;

			point[0] = (float)verts->v[0] * paliashdr->scale[0] + paliashdr->translate[0];
			point[1] = (float)verts->v[1] * paliashdr->scale[1] + paliashdr->translate[1];
			point[2] = lheight + height;
			point[0] -= shadevector[0] * point[2];
			point[1] -= shadevector[1] * point[2];

			glVertex3fv(point);

			verts++;
			count--;
		} while (count);

		glEnd();

		count = *order;
		order++;
	}
}

/*
=================
R_AliasSetupFrame
=================
*/
void R_AliasSetupFrame(int frame, aliashdr_t *paliashdr)
{
	int		posenum;
	int		numposes;

	if (paliashdr->numframes <= frame || frame < 0)
	{
		Con_Printf("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	posenum = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
		posenum += (int)((float)cl_time / paliashdr->frames[frame].interval) % numposes;

	GL_DrawAliasFrame(paliashdr, posenum);
}

/*
=============
R_DrawEntitiesOnList

Translucent entities are drawn later, from R_DrawTransEntitiesOnList.
=============
*/
void R_DrawEntitiesOnList(void)
{
	int		i;
	int		modeltype;

	if (r_drawentities.value == 0.0f)
		return;

	for (i = 0; i < cl_numvisedicts; ++i)
	{
		currententity = cl_visedicts[i];

		if (currententity->rendermode)
		{
			R_AddToTranslucentList(currententity);
		}
		else
		{
			modeltype = currententity->model->type;
			if (modeltype != mod_brush)
			{
				if (modeltype == mod_alias)
				{
					R_DrawAliasModel(currententity);
				}
				else if (modeltype == mod_studio && R_StudioCheckBBox())
				{
					alight_t	lighting;
					vec3_t		dir;

					lighting.plightvec = &dir;

					R_StudioEntityLight(currententity, &lighting);
					R_DrawStudioModel(currententity, &lighting);
				}
			}
			else
			{
				R_DrawBrushModel(currententity);
			}
		}
	}

	// sprites are drawn after everything else
	for (i = 0; i < cl_numvisedicts; ++i)
	{
		currententity = cl_visedicts[i];
		if (currententity->model->type == mod_sprite)
			R_DrawSpriteModel(currententity);
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel(void)
{
	vec3_t			lightdir = { -1.0f, 0.0f, 0.0f };
	unsigned int	lightrgb[3];
	int				modeltype;

	if (r_drawviewmodel.value == 0.0f)
		return;
	if (chase_active.value != 0.0f)
		return;
	if (envmap)
		return;
	if (r_drawentities.value == 0.0f)
		return;
	if (input_flags & 8)
		return;
	if (cl_health <= 0)
		return;

	currententity = &viewent_entity;
	if (!cl_viewent_valid)
		return;

	// hack the depth range to prevent view model from poking into walls
	glDepthRange(gldepthmin, (gldepthmax - gldepthmin) * 0.3f + gldepthmin);

	modeltype = currententity->model->type;
	if (modeltype != mod_brush)
	{
		if (modeltype == mod_alias)
		{
			int		j;

			R_LightPoint(lightrgb, currententity->origin);
			j = (int)((lightrgb[0] + lightrgb[1] + lightrgb[2]) / 3u);
			if (j < 24)
				j = 24;		// always give some light on gun

			view_lighting = j;
			view_lighting_ambient = j;

			// add dynamic lights
			for (int i = 0; i < MAX_DLIGHTS; ++i)
			{
				const dlight_t *dl = &cl_dlights[i];

				if (dl->radius != 0.0f && dl->die >= cl_time)
				{
					vec3_t	dist;
					float	add;

					dist[0] = currententity->origin[0] - dl->origin[0];
					dist[1] = currententity->origin[1] - dl->origin[1];
					dist[2] = currententity->origin[2] - dl->origin[2];
					add = dl->radius - VectorLength(dist);

					if (add > 0.0f)
						view_lighting = (int)((float)view_lighting + add);
				}
			}

			if (view_lighting > 128)
				view_lighting = 128;
			if (view_lighting + view_lighting_ambient > 192)
				view_lighting_ambient = 192 - view_lighting;

			lighting_direction = lightdir;
			R_DrawAliasModel(currententity);
		}
		else if (modeltype == mod_studio)
		{
			alight_t	lighting;

			currententity->sequence = cl_viewent_sequence;
			currententity->frame = 0.0f;
			currententity->anim_time = cl_viewent_animtime;
			currententity->framerate = 1.0f;

			lighting.plightvec = &lightdir;
			R_StudioEntityLight(currententity, &lighting);
			cl_viewent_light = lighting.ambient;
			R_DrawStudioModel(currententity, &lighting);
		}
	}
	else
	{
		R_DrawBrushModel(currententity);
	}

	glDepthRange(gldepthmin, gldepthmax);
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend(void)
{
	if (gl_polyblend.value == 0.0f)
		return;
	if (v_blend[3] == 0.0f)
		return;

	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glLoadIdentity();
	glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);	// put Z going up
	glRotatef(90.0f, 0.0f, 0.0f, 1.0f);		// put Z going up

	glColor4fv(v_blend);

	glBegin(GL_QUADS);
	glVertex3f(10.0f, 10.0f, 10.0f);
	glVertex3f(10.0f, -10.0f, 10.0f);
	glVertex3f(10.0f, -10.0f, -10.0f);
	glVertex3f(10.0f, 10.0f, -10.0f);
	glEnd();

	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
}

/*
===============
SignbitsForPlane

For the fast box on planeside test
===============
*/
int SignbitsForPlane(float *normal)
{
	int		bits = 0;
	int		i;

	for (i = 0; i < 3; i++)
	{
		if (normal[i] < 0.0f)
			bits |= (1 << i);
	}

	return bits;
}

/*
===============
R_SetFrustum

A fixed 90 degree frustum around the view direction.
===============
*/
void R_SetFrustum(void)
{
	int		i;

	frustum[0].normal[0] = vright[0] + vpn[0];
	frustum[0].normal[1] = vright[1] + vpn[1];
	frustum[0].normal[2] = vright[2] + vpn[2];

	frustum[1].normal[0] = vpn[0] - vright[0];
	frustum[1].normal[1] = vpn[1] - vright[1];
	frustum[1].normal[2] = vpn[2] - vright[2];

	frustum[2].normal[0] = vup[0] + vpn[0];
	frustum[2].normal[1] = vup[1] + vpn[1];
	frustum[2].normal[2] = vup[2] + vpn[2];

	frustum[3].normal[0] = vpn[0] - vup[0];
	frustum[3].normal[1] = vpn[1] - vup[1];
	frustum[3].normal[2] = vpn[2] - vup[2];

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(frustum[i].normal, r_origin);
		frustum[i].signbits = SignbitsForPlane(frustum[i].normal);
	}
}

/*
===============
MYgluPerspective
===============
*/
void MYgluPerspective(double fovy, double aspect, double zNear, double zFar)
{
	double	ymax;

	ymax = tan(fovy * M_PI / 360.0);

	glFrustum(
		-ymax * zNear * aspect,
		ymax * zNear * aspect,
		-ymax * zNear,
		ymax * zNear,
		zNear,
		zFar);
}

/*
=============
R_Clear
=============
*/
void R_Clear(void)
{
	if (r_mirroralpha.value != 1.0f)
	{
		if (gl_clear.value == 0.0f)
			glClear(GL_DEPTH_BUFFER_BIT);
		else
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0.0f;
		gldepthmax = 0.5f;
		glDepthFunc(GL_LEQUAL);
	}
	else if (gl_ztrick.value == 0.0f)
	{
		if (gl_clear.value == 0.0f)
			glClear(GL_DEPTH_BUFFER_BIT);
		else
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0.0f;
		gldepthmax = 1.0f;
		glDepthFunc(GL_LEQUAL);
	}
	else
	{
		// alternate between the two halves of the depth range
		ztrick_frame++;
		if (ztrick_frame & 1)
		{
			gldepthmin = 0.0f;
			gldepthmax = 0.49999f;
			glDepthFunc(GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1.0f;
			gldepthmax = 0.5f;
			glDepthFunc(GL_GEQUAL);
		}

		glClearDepth(gldepthmin == 0.0f ? 1.0 : 0.0);
		if (gl_clear.value != 0.0f)
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear(GL_DEPTH_BUFFER_BIT);
		glClearDepth(1.0);
	}

	glDepthRange(gldepthmin, gldepthmax);
}

/*
=============
R_Mirror
=============
*/
void R_Mirror(void)
{
	float	m[16];
	float	d;

	if (!mirror_plane)
		return;

	memcpy(m, r_world_matrix, sizeof(m));

	d = DotProduct(r_origin, mirror_plane->normal) - mirror_plane->dist;
	VectorMA(r_origin, -2*d, mirror_plane->normal, r_origin);

	d = DotProduct(vpn, mirror_plane->normal);
	VectorMA(vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef_viewangles[0] = -asin(vpn[2]) / M_PI * 180.0;
	r_refdef_viewangles[1] = atan2(vpn[1], vpn[0]) / M_PI * 180.0;
	r_refdef_viewangles[2] = -r_refdef_viewangles[2];

	if (cl_numvisedicts < MAX_VISEDICTS && cl_viewentity > 0)
		cl_visedicts[cl_numvisedicts++] = &cl_entities[cl_viewentity];

	gldepthmin = 0.5f;
	gldepthmax = 1.0f;
	glDepthRange(0.5, 1.0);
	glDepthFunc(GL_LEQUAL);

	R_RenderScene();

	gldepthmin = 0.0f;
	gldepthmax = 0.5f;
	glDepthRange(0.0, 0.5);
	glDepthFunc(GL_LEQUAL);

	// blend on top
	glEnable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	if (mirror_plane->normal[2] == 0.0f)
		glScalef(-1.0f, 1.0f, 1.0f);
	else
		glScalef(1.0f, -1.0f, 1.0f);
	glCullFace(GL_FRONT);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(m);
	glColor4f(1.0f, 1.0f, 1.0f, r_mirroralpha.value);

	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/*
================
R_RenderScene
================
*/
static void R_RenderScene(void)
{
	R_SetupGL();
	R_SetFrustum();

	R_PushDlights();
	R_RecursiveWorldNode(NULL);

	R_DrawEntitiesOnList();

	R_BlendLightmaps();
}

/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel(entity_t *entity)
{
	model_t		*clmodel;
	vec3_t		mins, maxs;
	vec3_t		dist;
	float		add;
	aliashdr_t	*paliashdr;
	int			skinnum;
	int			entindex;

	clmodel = entity->model;

	mins[0] = clmodel->mins[0] + entity->origin[0];
	mins[1] = clmodel->mins[1] + entity->origin[1];
	mins[2] = clmodel->mins[2] + entity->origin[2];

	maxs[0] = clmodel->maxs[0] + entity->origin[0];
	maxs[1] = clmodel->maxs[1] + entity->origin[1];
	maxs[2] = clmodel->maxs[2] + entity->origin[2];

	if (R_CullBox(mins, maxs))
		return;

	r_entorigin[0] = entity->origin[0];
	r_entorigin[1] = entity->origin[1];
	r_entorigin[2] = entity->origin[2];

	r_modeldist[0] = r_origin[0] - r_entorigin[0];
	r_modeldist[1] = r_origin[1] - r_entorigin[1];
	r_modeldist[2] = r_origin[2] - r_entorigin[2];

	//
	// get lighting information
	//
	if (entity == &viewent_entity && r_ambientlight < 24.0f)
	{
		r_shadelight = 24.0f;
		r_ambientlight = 24.0f;
	}

	for (int lnum = 0; lnum < MAX_DLIGHTS; ++lnum)
	{
		dlight_t *dl = &cl_dlights[lnum];

		if (dl->die >= cl_time)
		{
			dist[0] = r_entorigin[0] - dl->origin[0];
			dist[1] = r_entorigin[1] - dl->origin[1];
			dist[2] = r_entorigin[2] - dl->origin[2];

			add = dl->radius - VectorLength(dist);
			if (add > 0.0f)
				r_ambientlight = r_ambientlight + add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (r_ambientlight > 128.0f)
		r_ambientlight = 128.0f;
	if (r_ambientlight + r_shadelight > 192.0f)
		r_shadelight = 192.0f - r_ambientlight;

	// flames are always fullbright
	if (!strcmp(clmodel->name, "progs/flame2.mdl") ||
		!strcmp(clmodel->name, "progs/flame.mdl"))
	{
		r_shadelight = 256.0f;
		r_ambientlight = 256.0f;
	}

	r_lightvec = lightvec;
	{
		float	yaw = entity->angles[1];
		float	angle = -yaw / 180.0f * (float)M_PI;

		lightvec[0] = (float)sin(angle);
		lightvec[1] = (float)cos(angle);
		lightvec[2] = 1.0f;
	}
	VectorNormalize(lightvec);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata(clmodel);
	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//
	glPushMatrix();
	R_RotateForEntity(entity);
	glTranslatef(paliashdr->translate[0], paliashdr->translate[1], paliashdr->translate[2]);
	glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	skinnum = entity->skinnum;

	GL_Bind(paliashdr->gl_texturenum[skinnum]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players. Heads are just uncolored.
	entindex = (int)(entity - cl_entities);
	if (entindex != cl_viewentity && r_fullbright.value == 0.0f)
	{
		if (entindex >= 1 && cl_maxclients >= entindex &&
			!strcmp(clmodel->name, "progs/player.mdl"))
		{
			GL_Bind(playertextures + entindex - 1);
		}
	}

	if (gl_smoothmodels.value != 0.0f)
		glShadeModel(GL_SMOOTH);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value != 0.0f)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	R_AliasSetupFrame((int)entity->frame, paliashdr);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glShadeModel(GL_FLAT);

	if (gl_affinemodels.value != 0.0f)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix();

	if (r_shadows.value != 0.0f)
	{
		glPushMatrix();
		R_RotateForEntity(entity);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
		GL_DrawAliasShadow(paliashdr, r_posenum);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glPopMatrix();
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame(void)
{
	// don't allow cheats in multiplayer
	if (cl_maxclients > 1)
		Cvar_Set("r_fullbright", "0");

	R_AnimateLight();
	r_framecount++;

	// build the transformation matrix for the given view angles
	r_origin[0] = r_refdef_vieworg[0];
	r_origin[1] = r_refdef_vieworg[1];
	r_origin[2] = r_refdef_vieworg[2];

	AngleVectors(r_refdef_viewangles, vpn, vright, vup);

	// current viewleaf
	oldviewleaf = viewleaf;
	viewleaf = Mod_PointInLeaf(r_origin, cl_worldmodel);

	V_SetContentsColor(viewleaf ? viewleaf->contents : CONTENTS_EMPTY);

	if (r_dowarp > 2)
		V_SetContentsColor(CONTENTS_WATER);

	R_MarkLeaves();

	c_brush_polys = 0;
	c_alias_polys = 0;
	c_lightmaps = 0;
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL(void)
{
	int		x, x2, y, y2;
	GLsizei	w, h;
	float	width, height;
	float	aspect;
	float	fov_y;

	//
	// set up viewpoint
	//
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	x = r_refdef_vrect_x * glwidth / VIRTUAL_WIDTH;
	x2 = glwidth * (r_refdef_vrect_x + r_refdef_vrect_width) / VIRTUAL_WIDTH;
	y2 = (VIRTUAL_HEIGHT - r_refdef_vrect_y) * glheight / VIRTUAL_HEIGHT;
	y = glheight * (VIRTUAL_HEIGHT - r_refdef_vrect_y - r_refdef_vrect_height) / VIRTUAL_HEIGHT;

	// fudge around because of frac screen scale
	if (x > 0)
		--x;
	if (glwidth > x2)
		++x2;
	if (y < 0)
		--y;
	if (glheight > y2)
		++y2;

	w = x2 - x;
	h = y2 - y;

	if (envmap)
	{
		x = 0;
		y = 0;
		w = 256;
		h = 256;
	}

	glViewport(x + glx, gly + y, w, h);

	width = (float)r_refdef_vrect_width;
	height = (float)r_refdef_vrect_height;
	aspect = width / height;
	fov_y = (float)(atan(height / width) * 360.0 / M_PI);

	MYgluPerspective(fov_y, aspect, 4.0f, 4096.0f);

	if (r_mirror)
	{
		if (r_mirror_plane && r_mirror_plane->normal[2] == 0.0f)
			glScalef(-1.0f, 1.0f, 1.0f);
		else
			glScalef(1.0f, -1.0f, 1.0f);
		glCullFace(GL_BACK);
	}
	else
	{
		glCullFace(GL_FRONT);
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);	// put Z going up
	glRotatef(90.0f, 0.0f, 0.0f, 1.0f);		// put Z going up

	glRotatef(-r_refdef_viewangles[2], 1.0f, 0.0f, 0.0f);
	glRotatef(-r_refdef_viewangles[0], 0.0f, 1.0f, 0.0f);
	glRotatef(-r_refdef_viewangles[1], 0.0f, 0.0f, 1.0f);

	glTranslatef(-r_refdef_vieworg[0], -r_refdef_vieworg[1], -r_refdef_vieworg[2]);

	glGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value == 0.0f)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

/*
================
R_RenderViewPass
================
*/
void R_RenderViewPass(void)
{
	R_SetupFrame();
	R_SetFrustum();
	R_SetupGL();
	R_MarkLeaves();		// done here so we know if we're in water
	R_DrawWorld();		// adds static entities to the list
	S_ExtraUpdate();	// don't let sound get messed up if going slow
	R_DrawEntitiesOnList();
	R_DrawTransEntitiesOnList();
	S_ExtraUpdate();
	R_RenderDlights();
	R_DrawParticles();
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView(void)
{
	double	time1, time2;

	if (r_norefresh.value != 0.0f)
		return;

	if (!cl.worldmodel || !cl_worldmodel)
		Sys_Error("R_RenderView: NULL worldmodel\n");

	if (r_speeds.value != 0.0f)
	{
		glFinish();
		time1 = Sys_FloatTime();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	r_mirror = 0;

	R_Clear();

	// render normal view
	R_RenderViewPass();
	R_DrawViewModel();

	// render mirror view
	R_Mirror();

	R_PolyBlend();

	S_ExtraUpdate();

	if (r_speeds.value != 0.0f)
	{
		time2 = Sys_FloatTime();
		Con_Printf("%3i ms  %4i wpoly %4i epoly\n", (int)((time2 - time1) * 1000.0), c_brush_polys, c_alias_polys);
	}
}
