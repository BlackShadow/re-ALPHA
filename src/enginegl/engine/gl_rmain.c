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
#include <string.h>

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
int			envmap = 0;
int			r_framecount = 0;

mleaf_t		*oldviewleaf = NULL;
mleaf_t		*viewleaf = NULL;

int			r_mirror = 0;
mplane_t	*r_mirror_plane = NULL;

int			c_brush_polys = 0;
int			c_alias_polys = 0;

int			skytexturenum = -1;
int			mirrortexturenum = -1;

byte		*r_colormap = NULL;
void R_NewMap(void)
{
	int i;
	texture_t *texture;

	extern int d_lightstylevalue[256];
	extern entity_t cl_entities[MAX_EDICTS];
	extern model_t *cl_worldmodel;

	extern void R_ClearParticles(void);
	extern int R_DecalInit(void);
	extern void GL_BuildLightmaps(void);
	extern void R_LoadSkys(void);

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;

	memset(&cl_entities[0], 0, 0x130u);
	cl_entities[0].model = cl_worldmodel;

	if (cl_worldmodel && cl_worldmodel->numleafs > 0)
	{
		for (i = 0; i < cl_worldmodel->numleafs; i++)
			cl_worldmodel->leafs[i].efrags = NULL;
	}

	viewleaf = NULL;

	R_ClearParticles();
	R_DecalInit();
	GL_BuildLightmaps();

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

cvar_t		r_drawviewmodel = {"r_drawviewmodel", "1"};
extern cvar_t	chase_active;

cvar_t r_norefresh = { "r_norefresh", "0" };
cvar_t r_drawentities = { "r_drawentities", "1" };
cvar_t r_drawworld = { "r_drawworld", "1" };
cvar_t r_speeds = { "r_speeds", "0" };
cvar_t r_timegraph = { "r_timegraph", "0" };
cvar_t r_fullbright = { "r_fullbright", "0" };
cvar_t r_lightmap = { "r_lightmap", "0" };
cvar_t r_shadows = { "r_shadows", "1" };
cvar_t r_drawflat = { "r_drawflat", "0" };
cvar_t r_flowmap = { "r_flowmap", "0" };
cvar_t r_mirroralpha = { "r_mirroralpha", "1" };
cvar_t r_wateralpha = { "r_wateralpha", "1" };
cvar_t r_dynamic = { "r_dynamic", "1" };
cvar_t r_novis = { "r_novis", "0" };
cvar_t r_decals = { "r_decals", "1" };

cvar_t r_part_explode = { "r_part_explode", "1" };
cvar_t r_part_trails = { "r_part_trails", "1" };
cvar_t r_part_sparks = { "r_part_sparks", "1" };
cvar_t r_part_gunshots = { "r_part_gunshots", "1" };
cvar_t r_part_blood = { "r_part_blood", "1" };
cvar_t r_part_telesplash = { "r_part_telesplash", "1" };

float		ambientlight = 0.0f;
float		shadelight = 0.0f;
float		shadevector[3];
int			lastposenum = 0;
int			view_lighting = 0;
int			view_lighting_ambient = 0;
int			lighting_direction = 0;

typedef struct alight_s
{
	int ambient;
	int shade;
	float color[3];
	vec3_t *plightvec;
} alight_t;

extern float	v_blend[4];

extern double		cl_time;
extern int			cl_health;
extern int			cl_numvisedicts;
extern cl_entity_t	*cl_visedicts[MAX_VISEDICTS];
extern int			cl_viewent_valid;
extern float		cl_viewent_animtime;
extern int			cl_viewent_sequence;
extern int			cl_viewent_light;
extern int			cl_intermission;
extern int			cl_maxclients;
extern model_t*		cl_worldmodel;
extern int			cl_viewentity;

extern char			input_flags;

float*		r_avertexnormal_dots_ptr = NULL;
dlight_t*	light_list = NULL;
dlight_t*	light_list_end = NULL;
entity_t		viewent_entity;

int			playertextures = 0;

extern int			BoxOnPlaneSide(float *mins, float *maxs, mplane_t *plane);
extern int			R_AddToTranslucentList(int entity_ptr);
extern void			R_DrawAliasModel(entity_t *entity);
extern void			R_DrawBrushModel(entity_t *entity);
extern int			R_StudioEntityLight(int entity_ptr, int lightptr);
extern void			R_DrawStudioModel(int entity_ptr, int lightptr);
extern int			R_StudioCheckBBox(void);
extern void			R_DrawTransEntitiesOnList(void);
extern mleaf_t		*Mod_PointInLeaf(vec3_t origin, model_t *worldmodel);
extern void			R_PushDlights(void);
extern void			R_RecursiveWorldNode(mnode_t *node);
extern void			R_DrawSkyChain(msurface_t *surface);
extern void			R_BlendLightmaps(void);
extern void			R_RenderDlights(void);
extern void			S_ExtraUpdate(void);

float		view_height = 0.0f;

extern vec3_t		lightspot;
vec3_t		lightvec;

mplane_t frustum[4];
int R_CullBox(float *mins, float *maxs)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return 1;
	}

	return 0;
}

void R_RotateForEntity(int entity_ptr)
{
	entity_t *ent = (entity_t *)entity_ptr;
	GLfloat x = ent->origin[0];
	GLfloat y = ent->origin[1];
	GLfloat z = ent->origin[2];
	GLfloat angles[3];

	angles[0] = ent->angles[0];
	angles[1] = ent->angles[1];
	angles[2] = ent->angles[2];

	if (ent->has_lerpdata)
	{
		const float lerptime = ent->anim_time;
		float interpolation = 0.0f;

		if ((double)lerptime + 0.2 > cl_time && ent->latched_anim_time != lerptime)
			interpolation = (float)((cl_time - (double)lerptime) / ((double)lerptime - (double)ent->latched_anim_time));

		x = x - (ent->lerp_origin[0] - ent->origin[0]) * interpolation;
		y = y - (ent->lerp_origin[1] - ent->origin[1]) * interpolation;
		z = z - (ent->lerp_origin[2] - ent->origin[2]) * interpolation;

		if (interpolation > 0.0f && interpolation < 1.5f)
		{
			const float lerp_factor = 1.0f - interpolation;

			for (int i = 0; i < 3; i++)
			{
				float delta = ent->lerp_angles[i] - ent->angles[i];

				if (delta > 180.0f)
					delta -= 360.0f;
				else if (delta < -180.0f)
					delta += 360.0f;

				angles[i] = lerp_factor * delta + angles[i];
			}
		}
	}

	angles[0] = -angles[0];
	glTranslatef(x, y, z);
	glRotatef(angles[1], 0.0f, 0.0f, 1.0f);
	glRotatef(angles[0], 0.0f, 1.0f, 0.0f);
	glRotatef(-angles[2], 1.0f, 0.0f, 0.0f);
}

mspriteframe_t *R_GetSpriteFrame(entity_t *entity)
{
	model_t *model;
	msprite_t *spriteHeader;
	int frame;

	mspritegroupframe_t *frameGroup;

	model = *(model_t **)((byte *)entity + 168);
	spriteHeader = (msprite_t *)model->cache.data;
	frame = (int)*(float *)((byte *)entity + 176);

	if (spriteHeader->numframes <= frame || frame < 0)
	{
		Con_Printf("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	frameGroup = &spriteHeader->frames[frame];
	if (frameGroup->type == SPR_SINGLE)
		return frameGroup->frameptr;

	{
		mspritegroup_t *group = (mspritegroup_t *)frameGroup->frameptr;

		float *intervals = group->intervals;
		const float fullinterval = intervals[group->numframes - 1];

		const float time = *(float *)((byte *)entity + 180) + (float)cl_time;
		const float targettime = time - (float)((int)(time / fullinterval)) * fullinterval;

		int i;
		for (i = 0; i < group->numframes - 1; i++)
		{
			if (intervals[i] > targettime)
				break;
		}

		return group->frames[i];
	}
}

void R_DrawSpriteModel(entity_t *entity)
{
	mspriteframe_t *spriteFrame;
	int render_mode;
	GLfloat v[3];

	spriteFrame = R_GetSpriteFrame(entity);
	render_mode = *(int *)((byte *)currententity + 152);

	if (render_mode)
	{
		if (render_mode == 1)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
		}
		else
		{
			if (render_mode == 5)
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

	GL_Bind(spriteFrame->gl_texturenum);

	glEnable(GL_ALPHA_TEST);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0, 0.0);
	VectorMA((float *)((byte *)entity + 104), spriteFrame->down, vup, v);
	VectorMA(v, spriteFrame->left, vright, v);
	glVertex3fv(v);

	glTexCoord2f(0.0, 1.0);
	VectorMA((float *)((byte *)entity + 104), spriteFrame->up, vup, v);
	VectorMA(v, spriteFrame->left, vright, v);
	glVertex3fv(v);

	glTexCoord2f(1.0, 1.0);
	VectorMA((float *)((byte *)entity + 104), spriteFrame->up, vup, v);
	VectorMA(v, spriteFrame->right, vright, v);
	glVertex3fv(v);

	glTexCoord2f(1.0, 0.0);
	VectorMA((float *)((byte *)entity + 104), spriteFrame->down, vup, v);
	VectorMA(v, spriteFrame->right, vright, v);
	glVertex3fv(v);

	glEnd();

	glDisable(GL_ALPHA_TEST);

	if (render_mode)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glDisable(GL_BLEND);
	}
}

void GL_DrawAliasFrame(int *aliasheader, int posenum)
{
	int		*commands;
	unsigned char *verts;
	int		count;
	float	*texcoords;
	float	l;

	lastposenum = posenum;

	commands = (int*)((char*)aliasheader + aliasheader[24]);
	verts = (unsigned char*)((char*)aliasheader + posenum * aliasheader[22] + aliasheader[23]);

	count = *commands;
	texcoords = (float*)(commands + 1);

	while (count)
	{
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
			glTexCoord2f(texcoords[0], texcoords[1]);
			texcoords += 2;

			l = *((float*)r_avertexnormal_dots_ptr + verts[3]) * shadelight;
			glColor3f(l, l, l);

			glVertex3f((float)verts[0], (float)verts[1], (float)verts[2]);
			verts += 4;

			count--;
		} while (count);

		glEnd();

		count = *(int*)texcoords;
		texcoords++;
	}
}

void GL_DrawAliasShadow(int aliasheader, int posenum)
{
	int		*commands;
	unsigned char *verts;
	int		count;
	float	point[3];
	float	height;
	float	lheight;

	lheight = *(float*)(currententity + 112) - view_height;
	height = 1.0f - lheight;

	verts = (unsigned char*)(aliasheader + posenum * *(int*)(aliasheader + 88) + *(int*)(aliasheader + 92));
	commands = (int*)(aliasheader + *(int*)(aliasheader + 96));

	count = *commands;
	commands++;

	while (count)
	{
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
			commands += 2;

			point[0] = (float)verts[0] * *(float*)(aliasheader + 8) + *(float*)(aliasheader + 20);
			point[1] = (float)verts[1] * *(float*)(aliasheader + 12) + *(float*)(aliasheader + 24);
			point[2] = (float)verts[2] * *(float*)(aliasheader + 16) + *(float*)(aliasheader + 28);

			point[2] = lheight + height;
			point[0] -= shadevector[0] * point[2];
			point[1] -= shadevector[1] * point[2];

			glVertex3fv(point);

			verts += 4;
			count--;
		} while (count);

		glEnd();

		count = *commands;
		commands++;
	}
}

void R_AliasSetupFrame(int frame, int *aliasheader)
{
	int		posenum;
	int		numposes;
	float	*frametime;

	if (aliasheader[17] <= frame || frame < 0)
	{
		Con_Printf("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	frametime = (float*)&aliasheader[10 * frame];
	posenum = *((int*)frametime + 57);
	numposes = *((int*)frametime + 58);

	if (numposes > 1)
	{
		posenum += (int)((float)cl_time / frametime[59]) % numposes;
	}

	GL_DrawAliasFrame(aliasheader, posenum);
}

void R_DrawEntitiesOnList(void)
{
	int i;
	int modeltype;

	if (r_drawentities.value == 0.0f)
		return;

	for (i = 0; i < cl_numvisedicts; ++i)
	{
		currententity = cl_visedicts[i];

		if (currententity->rendermode)
		{
			R_AddToTranslucentList((int)currententity);
		}
		else
		{
			modeltype = currententity->model->type;
			if (modeltype)
			{
				if (modeltype == 2)
				{
					R_DrawAliasModel(currententity);
				}
				else if (modeltype == 3 && R_StudioCheckBBox())
				{
					alight_t lighting;
					vec3_t studio_lightvec;

					lighting.plightvec = &studio_lightvec;

					R_StudioEntityLight((int)currententity, (int)&lighting);
					R_DrawStudioModel((int)currententity, (int)&lighting);
				}
			}
			else
			{
				R_DrawBrushModel(currententity);
			}
		}
	}

	for (i = 0; i < cl_numvisedicts; ++i)
	{
		currententity = cl_visedicts[i];
		if (currententity->model->type == 1)
			R_DrawSpriteModel(currententity);
	}
}

void R_DrawViewModel(void)
{
	vec3_t lightdir = {-1.0f, 0.0f, 0.0f};
	unsigned int lightrgb[3];
	int modeltype;

	if (r_drawviewmodel.value == 0.0f)
		return;
	if (chase_active.value != 0.0f)
		return;
	if (envmap)
		return;
	if (r_drawentities.value == 0.0f)
		return;
	if ((input_flags & 8) != 0)
		return;
	if (cl_health <= 0)
		return;

	currententity = &viewent_entity;
	if (!cl_viewent_valid)
		return;

	glDepthRange(gldepthmin, (gldepthmax - gldepthmin) * 0.3f + gldepthmin);

	modeltype = currententity->model->type;
	if (modeltype)
	{
		if (modeltype == 2)
		{
			int lightavg;

			R_LightPoint(lightrgb, currententity->origin);
			lightavg = (int)((lightrgb[0] + lightrgb[1] + lightrgb[2]) / 3u);
			if (lightavg < 24)
				lightavg = 24;

			view_lighting = lightavg;
			view_lighting_ambient = lightavg;

			for (int i = 0; i < MAX_DLIGHTS; ++i)
			{
				const dlight_t *dl = &cl_dlights[i];
				if (dl->radius != 0.0f && dl->die >= cl_time)
				{
					vec3_t delta;
					float lightadd;

					delta[0] = currententity->origin[0] - dl->origin[0];
					delta[1] = currententity->origin[1] - dl->origin[1];
					delta[2] = currententity->origin[2] - dl->origin[2];
					lightadd = dl->radius - VectorLength(delta);

					if (lightadd > 0.0f)
						view_lighting = (int)((float)view_lighting + lightadd);
				}
			}

			if (view_lighting > 128)
				view_lighting = 128;
			if (view_lighting + view_lighting_ambient > 192)
				view_lighting_ambient = 192 - view_lighting;

			lighting_direction = (int)lightdir;
			R_DrawAliasModel(currententity);
		}
		else if (modeltype == 3)
		{
			alight_t lighting;

			currententity->sequence = cl_viewent_sequence;
			currententity->frame = 0.0f;
			currententity->anim_time = cl_viewent_animtime;
			currententity->framerate = 1.0f;

			lighting.plightvec = &lightdir;
			R_StudioEntityLight((int)currententity, (int)&lighting);
			cl_viewent_light = lighting.ambient;
			R_DrawStudioModel((int)currententity, (int)&lighting);
		}
	}
	else
	{
		R_DrawBrushModel(currententity);
	}

	glDepthRange(gldepthmin, gldepthmax);
}

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
	glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(90.0f, 0.0f, 0.0f, 1.0f);

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

int SignbitsForPlane(float *normal)
{
	int bits = 0;
	int i;

	for (i = 0; i < 3; i++)
	{
		if (normal[i] < 0.0f)
			bits |= (1 << i);
	}

	return bits;
}

void R_SetFrustum(void)
{
	int i;

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
		frustum[i].type = 5;
		frustum[i].dist = frustum[i].normal[0] * r_origin[0] + frustum[i].normal[1] * r_origin[1] + frustum[i].normal[2] * r_origin[2];
		frustum[i].signbits = SignbitsForPlane(frustum[i].normal);
	}
}

void MYgluPerspective(double fovy, double aspect, double zNear, double zFar)
{
	double	ymax;

	ymax = tan(fovy * 3.14159265358979 / 360.0);

	glFrustum(
		-ymax * zNear * aspect,
		ymax * zNear * aspect,
		-ymax * zNear,
		ymax * zNear,
		zNear,
		zFar
	);
}

void R_Clear(void)
{
	extern int		ztrick_frame;

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
		if (gl_clear.value != 0.0f)
			glClear(GL_COLOR_BUFFER_BIT);

		ztrick_frame++;
		if (ztrick_frame & 1)
		{
			gldepthmin = 0.0f;
			gldepthmax = 0.49998999f;
			glDepthFunc(GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1.0f;
			gldepthmax = 0.5f;
			glDepthFunc(GL_GEQUAL);
		}
	}

	glDepthRange(gldepthmin, gldepthmax);
}

void R_Mirror(void)
{
	float	m[16];
	float	d;

	if (!mirror_plane)
		return;

	memcpy(m, r_world_matrix, 0x40);

	d = DotProduct(r_origin, ((mplane_t*)mirror_plane)->normal) - ((mplane_t*)mirror_plane)->dist;
	VectorMA(r_origin, -2*d, ((mplane_t*)mirror_plane)->normal, r_origin);

	d = DotProduct(vpn, ((mplane_t*)mirror_plane)->normal);
	VectorMA(vpn, -2*d, ((mplane_t*)mirror_plane)->normal, vpn);

	r_refdef_viewangles[0] = -asin(vpn[2]) / 3.141592653589793 * 180.0;
	r_refdef_viewangles[1] = atan2(vpn[1], vpn[0]) / 3.141592653589793 * 180.0;
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

	glEnable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	if (((mplane_t*)mirror_plane)->normal[2] == 0.0f)
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

void R_RenderScene(void)
{
	R_SetupGL();
	R_SetFrustum();

	R_PushDlights();
	R_RecursiveWorldNode(0);

	R_DrawEntitiesOnList();

	R_BlendLightmaps();
}

extern float		r_entorigin[3];
extern float		r_modeldist[3];
extern float		r_ambientlight;
extern float		r_shadelight;
extern float*		r_lightvec;
extern float*		r_avertexnormal_dots;
extern int			r_posenum;
extern int			c_alias_polys;
extern int			playertextures;

extern int			cl_maxclients;
static const char model_flame2[] = "progs/flame2.mdl";
static const char model_flame[] = "progs/flame.mdl";
static const char model_player[] = "progs/player.mdl";

extern void			GL_Bind(int texture);
extern void			R_AliasSetupFrame(int frame, int *aliasheader);
extern void			GL_DrawAliasShadow(int aliasheader, int posenum);

void R_DrawAliasModel(entity_t *entity)
{
	model_t	*model;
	float	mins[3], maxs[3];
	float	dist[3];
	float	lightadd;
	int		aliasheader;
	int		skinnum;
	int		entindex;

	model = entity->model;

	mins[0] = model->mins[0] + entity->origin[0];
	mins[1] = model->mins[1] + entity->origin[1];
	mins[2] = model->mins[2] + entity->origin[2];

	maxs[0] = model->maxs[0] + entity->origin[0];
	maxs[1] = model->maxs[1] + entity->origin[1];
	maxs[2] = model->maxs[2] + entity->origin[2];

	if (R_CullBox(mins, maxs))
		return;

	r_entorigin[0] = entity->origin[0];
	r_entorigin[1] = entity->origin[1];
	r_entorigin[2] = entity->origin[2];

	r_modeldist[0] = r_origin[0] - r_entorigin[0];
	r_modeldist[1] = r_origin[1] - r_entorigin[1];
	r_modeldist[2] = r_origin[2] - r_entorigin[2];

	if (entity == &viewent_entity && r_ambientlight < 24.0f)
	{
		r_shadelight = 24.0f;
		r_ambientlight = 24.0f;
	}

	for (int dlightIndex = 0; dlightIndex < MAX_DLIGHTS; ++dlightIndex)
	{
		dlight_t *dl = &cl_dlights[dlightIndex];
		if (dl->die >= cl_time)
		{
			dist[0] = r_entorigin[0] - dl->origin[0];
			dist[1] = r_entorigin[1] - dl->origin[1];
			dist[2] = r_entorigin[2] - dl->origin[2];

			lightadd = dl->radius - VectorLength(dist);
			if (lightadd > 0.0f)
				r_ambientlight = r_ambientlight + lightadd;
		}
	}

	if (r_ambientlight > 128.0f)
		r_ambientlight = 128.0f;
	if (r_ambientlight + r_shadelight > 192.0f)
		r_shadelight = 192.0f - r_ambientlight;

	if (!strcmp(model->name, model_flame2) ||
		!strcmp(model->name, model_flame))
	{
		r_shadelight = 256.0f;
		r_ambientlight = 256.0f;
	}

	r_lightvec = (float*)&lightvec;
	{
		float yaw = entity->angles[1];
		float angle = -yaw / 180.0f * 3.14159265f;
		lightvec[0] = (float)sin(angle);
		lightvec[1] = (float)cos(angle);
		lightvec[2] = 1.0f;
	}
	VectorNormalize(lightvec);

	aliasheader = (int)Mod_Extradata(model);
	c_alias_polys += *(int*)(aliasheader + 64);

	glPushMatrix();
	R_RotateForEntity((int)entity);
	glTranslatef(*(float*)(aliasheader + 20), *(float*)(aliasheader + 24), *(float*)(aliasheader + 28));
	glScalef(*(float*)(aliasheader + 8), *(float*)(aliasheader + 12), *(float*)(aliasheader + 16));

	skinnum = entity->skinnum;

	GL_Bind(*(int*)(aliasheader + 4 * skinnum + 100));

	entindex = (int)(entity - cl_entities);
	if (entindex != cl_viewentity && r_fullbright.value == 0.0f)
	{
		if (entindex >= 1 && cl_maxclients >= entindex &&
			!strcmp(model->name, model_player))
		{
			GL_Bind(playertextures + entindex - 1);
		}
	}

	if (gl_smoothmodels.value != 0.0f)
		glShadeModel(GL_SMOOTH);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value != 0.0f)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	R_AliasSetupFrame((int)entity->frame, (int*)aliasheader);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glShadeModel(GL_FLAT);

	if (gl_affinemodels.value != 0.0f)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix();

	if (r_shadows.value != 0.0f)
	{
		glPushMatrix();
		R_RotateForEntity((int)entity);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
		GL_DrawAliasShadow(aliasheader, r_posenum);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glPopMatrix();
	}
}

extern float		r_refdef_vieworg[3];
extern float		r_refdef_viewangles[3];
extern int			r_refdef_vrect_x;
extern int			r_refdef_vrect_y;
extern int			r_refdef_vrect_width;
extern int			r_refdef_vrect_height;
extern int			r_refdef_fov_x;
extern int			r_refdef_fov_y;
extern int			envmap;
extern int			r_mirror;
extern mplane_t		*r_mirror_plane;

extern int			r_framecount;
extern int			r_visframecount;
extern mleaf_t		*oldviewleaf;
extern mleaf_t		*viewleaf;
extern model_t*		cl_worldmodel;
extern int			r_dowarp;

extern int			c_brush_polys;
extern int			c_alias_polys;
extern int			c_lightmaps;

extern void			Cvar_Set(const char *var, const char *value);

extern void			R_AnimateLight(void);
extern void			AngleVectors(float *angles, float *forward, float *right, float *up);
extern mleaf_t*		Mod_PointInLeaf(vec3_t origin, model_t *worldmodel);
extern void			V_SetContentsColor(int contents);
extern void			R_MarkLeaves(void);

int R_SetupFrame(void)
{
	if (cl_maxclients > 1)
		Cvar_Set("r_fullbright", "0");

	R_AnimateLight();
	r_framecount++;

	r_origin[0] = r_refdef_vieworg[0];
	r_origin[1] = r_refdef_vieworg[1];
	r_origin[2] = r_refdef_vieworg[2];

	AngleVectors(r_refdef_viewangles, vpn, vright, vup);

	oldviewleaf = viewleaf;
	viewleaf = Mod_PointInLeaf(r_origin, cl_worldmodel);

	V_SetContentsColor(viewleaf ? viewleaf->contents : CONTENTS_EMPTY);

	if (r_dowarp > 2)
		V_SetContentsColor(-3);

	R_MarkLeaves();

	c_brush_polys = 0;
	c_alias_polys = 0;
	c_lightmaps = 0;

	return 0;
}

void R_SetupGL(void)
{
	int x;
	int x2;
	int y;
	int y2;
	GLsizei w;
	GLsizei h;
	float width_f;
	float height_f;
	float aspect;
	float fov_angle;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	x = r_refdef_vrect_x * glwidth / 320;
	x2 = glwidth * (r_refdef_vrect_x + r_refdef_vrect_width) / 320;
	y2 = (200 - r_refdef_vrect_y) * glheight / 200;
	y = glheight * (200 - r_refdef_vrect_y - r_refdef_vrect_height) / 200;

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

	width_f = (float)r_refdef_vrect_width;
	height_f = (float)r_refdef_vrect_height;
	aspect = width_f / height_f;
	fov_angle = (float)(atan(height_f / width_f) * 360.0 / 3.141592653589793);

	MYgluPerspective(fov_angle, aspect, 4.0f, 4096.0f);

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

	glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(90.0f, 0.0f, 0.0f, 1.0f);

	glRotatef(-r_refdef_viewangles[2], 1.0f, 0.0f, 0.0f);
	glRotatef(-r_refdef_viewangles[0], 0.0f, 1.0f, 0.0f);
	glRotatef(-r_refdef_viewangles[1], 0.0f, 0.0f, 1.0f);

	glTranslatef(-r_refdef_vieworg[0], -r_refdef_vieworg[1], -r_refdef_vieworg[2]);

	glGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	if (gl_cull.value == 0.0f)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

extern void			R_SetFrustum(void);
extern void			R_MarkLeaves(void);
extern void			R_DrawWorld(void);
extern void			R_DrawEntitiesOnList(void);
extern void			R_RenderDlights(void);
extern void			R_DrawParticles(void);

void R_RenderViewPass(void)
{
	R_SetupFrame();
	R_SetFrustum();
	R_SetupGL();
	R_MarkLeaves();
	R_DrawWorld();
	S_ExtraUpdate();
	R_DrawEntitiesOnList();
	R_DrawTransEntitiesOnList();
	S_ExtraUpdate();
	R_RenderDlights();
	R_DrawParticles();
}

void R_RenderView(void)
{
	double start;
	double end;

	if (r_norefresh.value != 0.0f)
		return;

	if (!cl.worldmodel || !cl_worldmodel)
	{
		Sys_Error("R_RenderView: NULL worldmodel\n");
	}

	if (r_speeds.value != 0.0f)
	{
		glFinish();
		start = Sys_FloatTime();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	r_mirror = 0;

	R_Clear();
	R_RenderViewPass();
	R_DrawViewModel();
	R_Mirror();
	R_PolyBlend();
	S_ExtraUpdate();

	if (r_speeds.value != 0.0f)
	{
		end = Sys_FloatTime();
		Con_Printf("%3i ms  %4i wpoly %4i epoly\n", (int)((end - start) * 1000.0), c_brush_polys, c_alias_polys);
	}
}
