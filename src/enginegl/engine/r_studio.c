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
// r_studio.c -- studio model rendering

#include "quakedef.h"
#include "studio.h"

// bone controllers of these types move along or rotate about an axis
#define STUDIO_CONTROLLER_TYPES	(STUDIO_X | STUDIO_Y | STUDIO_Z | STUDIO_XR | STUDIO_YR | STUDIO_ZR | STUDIO_LX | STUDIO_LY | STUDIO_LZ)

#define CONTROLLER_LERP_TIME	0.1		// controllers move to their new values over this time
#define SEQUENCE_BLEND_TIME		0.2		// a new sequence blends in from the old one over this time

#define MAX_TRANS_ENTITIES		100

#define LIGHTCOLOR_SCALE		49407.0f	// light color 0..1 to 8.8 fixed point

typedef float vec4_t[4];

static studiohdr_t			*g_pStudioHeader;
static mstudiobodyparts_t	*g_pBodypart;
static mstudiomodel_t		*g_pStudioModel;

static int		g_r_studio_rendercount;
static int		g_r_studio_render_flag;
static int		g_r_studio_num_verts;
static int		g_r_studio_mode;
static int		g_r_studio_hardware_color;

static int		g_r_studio_ambient;
static float	g_r_studio_shade;
static int		g_r_studio_lightcolor_r;
static int		g_r_studio_lightcolor_g;
static int		g_r_studio_lightcolor_b;

static float	g_shadow_dir[3];

static vec3_t	g_bonepos[MAXSTUDIOBONES];
static vec4_t	g_bonequat[MAXSTUDIOBONES];
static vec3_t	g_bonepos2[MAXSTUDIOBONES];
static vec4_t	g_bonequat2[MAXSTUDIOBONES];

static float	g_bonetransform[MAXSTUDIOBONES][3][4];
static float	g_lighttransform[MAXSTUDIOBONES][3][4];

static float	g_transformedverts[MAXSTUDIOVERTS][3];
static float	g_lightvalues[MAXSTUDIOVERTS];
static int		g_chrome[MAXSTUDIOVERTS][2];

static int		r_numtranslucent;
static entity_t	*r_translucent_entities[MAX_TRANS_ENTITIES];
static float	r_translucent_dist[MAX_TRANS_ENTITIES];

int R_StudioCheckBBox(void)
{
	return 1;
}

/*
================
R_StudioGetFrameCount

Number of sub-models in the first body part.
================
*/
int R_StudioGetFrameCount(model_t *model)
{
	studiohdr_t			*pstudio;
	mstudiobodyparts_t	*pbodypart;

	if (!model || model->type != mod_studio)
		return 0;

	pstudio = (studiohdr_t *)Mod_Extradata(model);
	if (!pstudio)
		return 0;

	pbodypart = (mstudiobodyparts_t *)((byte *)pstudio + pstudio->bodypartindex);
	return pbodypart->nummodels;
}

/*
================
R_StudioSetupLighting
================
*/
void R_StudioSetupLighting(alight_t *plight)
{
	vec3_t		*plightvec;
	float		shade;

	plightvec = plight->plightvec;

	g_r_studio_ambient = plight->ambient;
	shade = (float)plight->shade;
	g_r_studio_shade = shade;

	r_plightvec[0] = (*plightvec)[0];
	r_plightvec[1] = (*plightvec)[1];
	r_plightvec[2] = (*plightvec)[2];

	g_r_studio_lightcolor_r = ((int)(plight->color[0] * LIGHTCOLOR_SCALE)) & 0xFF00;
	g_r_studio_lightcolor_g = ((int)(plight->color[1] * LIGHTCOLOR_SCALE)) & 0xFF00;
	g_r_studio_lightcolor_b = ((int)(plight->color[2] * LIGHTCOLOR_SCALE)) & 0xFF00;
}

/*
================
R_StudioSetupModel
================
*/
static mstudiomodel_t *R_StudioSetupModel(int bodypart)
{
	mstudiobodyparts_t	*pbodypart;
	mstudiomodel_t		*psubmodel;
	int					index;

	if (!g_pStudioHeader)
		return NULL;

	if (bodypart > g_pStudioHeader->numbodyparts)
	{
		Con_DPrintf("R_StudioSetupModel: no such bodypart %d\n", bodypart);
		bodypart = 0;
	}

	pbodypart = (mstudiobodyparts_t *)((byte *)g_pStudioHeader + g_pStudioHeader->bodypartindex) + bodypart;

	index = (currententity->body / pbodypart->base) % pbodypart->nummodels;
	psubmodel = (mstudiomodel_t *)((byte *)g_pStudioHeader + pbodypart->modelindex) + index;

	g_pBodypart = pbodypart;
	g_pStudioModel = psubmodel;
	return psubmodel;
}

/*
================
R_StudioTransformVertex
================
*/
void R_StudioTransformVertex(float *out, int bone, float *in)
{
	float	(*m)[4] = g_bonetransform[bone];

	out[0] = m[0][0] * in[0] + m[0][1] * in[1] + m[0][2] * in[2] + m[0][3];
	out[1] = m[1][0] * in[0] + m[1][1] * in[1] + m[1][2] * in[2] + m[1][3];
	out[2] = m[2][0] * in[0] + m[2][1] * in[1] + m[2][2] * in[2] + m[2][3];
}

/*
================
R_StudioLighting

Light value of one normal, and its chrome texture coordinates.
================
*/
void R_StudioLighting(float *lv, int bone, char flags, float *normal)
{
	float	(*m)[4] = g_lighttransform[bone];
	float	nx, ny, nz;
	float	illum;
	float	lambert;
	float	dot;

	nx = m[0][0] * normal[0] + m[0][1] * normal[1] + m[0][2] * normal[2];
	ny = m[1][0] * normal[0] + m[1][1] * normal[1] + m[1][2] * normal[2];
	nz = m[2][0] * normal[0] + m[2][1] * normal[1] + m[2][2] * normal[2];

	illum = (float)g_r_studio_ambient;

	if (!(flags & STUDIO_NF_FLATSHADE) || g_r_studio_mode == 1)
	{
		lambert = cv_lambert.value;
		if (lambert <= 1.0f)
			lambert = 1.0f;

		dot = r_plightvec[0] * nx + r_plightvec[1] * ny + r_plightvec[2] * nz;
		dot = (dot - (lambert - 1.0f)) / lambert;
		if (dot < 0.0f)
			illum = illum - g_r_studio_shade * dot;
	}
	else
	{
		illum = g_r_studio_shade * 0.5f + illum;
	}

	if (flags & STUDIO_NF_CHROME)
	{
		int		i = (int)(lv - g_lightvalues);
		float	n = (vpn[1] * ny + vpn[2] * nz + vpn[0] * nx) * 2.0f;
		float	s = n * nx - vpn[0];
		float	t = n * nz - vpn[2];

		// reflection vector (-1..1) to 0..64 texels
		g_chrome[i][0] = (int)((s + 1.0f) * 32.0f);
		g_chrome[i][1] = (int)((t + 1.0f) * 32.0f);
	}

	if (illum > 255.0f)
		illum = 255.0f;

	*lv = (float)lightgammatable[(int)(illum * 4.0f)] * (1.0f / 1024);
}

/*
================
R_StudioCalcRotations

Bone positions and quaternions of a sequence at the given frame,
with the entity's bone controllers applied.
================
*/
void R_StudioCalcRotations(float *pos, float *quat, int seq, float frame)
{
	mstudioseqdesc_t		*pseqdesc;
	mstudioboneanim_t		*panim;
	mstudiobonecontroller_t	*pbonecontroller;
	int						numbonecontrollers;
	int						iframe;
	float					blend;
	int						bone;
	byte					*controller;
	byte					*latched;
	float					adjX, adjY, adjZ;

	if (!g_pStudioHeader)
		return;

	iframe = (int)frame;

	pseqdesc = (mstudioseqdesc_t *)((byte *)g_pStudioHeader + g_pStudioHeader->seqindex) + seq;
	panim = (mstudioboneanim_t *)((byte *)g_pStudioHeader + pseqdesc->animindex);

	blend = 1.0f;
	if (currententity->latched_anim_time + 0.01f <= currententity->anim_time)
	{
		blend = (float)((cl_time - (double)currententity->anim_time) / CONTROLLER_LERP_TIME);
		if (blend > 2.0f)
			blend = 2.0f;
	}

	numbonecontrollers = g_pStudioHeader->numbonecontrollers;
	pbonecontroller = (mstudiobonecontroller_t *)((byte *)g_pStudioHeader + g_pStudioHeader->bonecontrollerindex);

	controller = currententity->controller;
	latched = currententity->latched_controller;

	for (bone = 0; bone < g_pStudioHeader->numbones; ++bone)
	{
		mstudioposkey_t	*pposkey;
		mstudiorotkey_t	*protkey;
		int				numposkeys;
		int				numrotkeys;
		int				key;

		adjX = 0.0f;
		adjY = 0.0f;
		adjZ = 0.0f;

		pposkey = (mstudioposkey_t *)((byte *)g_pStudioHeader + panim->poskeyindex);
		protkey = (mstudiorotkey_t *)((byte *)g_pStudioHeader + panim->rotkeyindex);

		numposkeys = panim->numposkeys;
		numrotkeys = panim->numrotkeys;

		for (int i = 0; i < numbonecontrollers; ++i)
		{
			mstudiobonecontroller_t	*pbc = &pbonecontroller[i];
			float					value;

			if (pbc->bone != bone)
				continue;

			if (!(pbc->type & STUDIO_RLOOP))
			{
				float t = ((1.0f - blend) * (float)latched[i] + (float)controller[i] * blend) / 255.0f;
				if (t < 0.0f)
					t = 0.0f;
				if (t > 1.0f)
					t = 1.0f;

				value = pbc->end * t + (1.0f - t) * pbc->start;
			}
			else
			{
				int c0 = controller[i];
				int c1 = latched[i];

				// wrapping controllers take the short way around
				if (abs(c0 - c1) <= 128)
				{
					value = ((1.0f - blend) * (float)c1 + (float)c0 * blend) * (360.0f / 256) + pbc->start;
				}
				else
				{
					int a0 = (c0 + 128) & 0xFF;
					int a1 = (c1 + 128) & 0xFF;
					value = ((1.0f - blend) * (float)a1 + (float)a0 * blend - 128.0f) * (360.0f / 256) + pbc->start;
				}
			}

			switch (pbc->type & STUDIO_CONTROLLER_TYPES)
			{
			case STUDIO_XR:
				adjX += value;
				break;
			case STUDIO_YR:
				adjY += value;
				break;
			case STUDIO_ZR:
				adjZ += value;
				break;
			}
		}

		// rotation
		key = 1;
		for (mstudiorotkey_t *pkey = &protkey[1]; key < numrotkeys; ++pkey, ++key)
		{
			if (pkey->frame > iframe)
				break;
		}

		if (key >= numrotkeys)
		{
			float			angles[3];
			float			*q = quat + 4 * bone;
			mstudiorotkey_t	*k = &protkey[key - 1];

			angles[0] = (float)k->pitch / ROTKEY_SCALE + adjY;
			angles[1] = (float)k->yaw / ROTKEY_SCALE + adjZ;
			angles[2] = (float)k->roll / ROTKEY_SCALE + adjX;
			AngleQuaternion(angles, q);
		}
		else
		{
			float			angles1[3];
			float			angles2[3];
			float			q1[4];
			float			q2[4];
			float			*q = quat + 4 * bone;
			mstudiorotkey_t	*k = &protkey[key - 1];
			int				numframes;
			float			t;

			angles1[0] = (float)k[0].pitch / ROTKEY_SCALE + adjY;
			angles1[1] = (float)k[0].yaw / ROTKEY_SCALE + adjZ;
			angles1[2] = (float)k[0].roll / ROTKEY_SCALE + adjX;
			AngleQuaternion(angles1, q1);

			angles2[0] = (float)k[1].pitch / ROTKEY_SCALE + adjY;
			angles2[1] = (float)k[1].yaw / ROTKEY_SCALE + adjZ;
			angles2[2] = (float)k[1].roll / ROTKEY_SCALE + adjX;
			AngleQuaternion(angles2, q2);

			numframes = k[1].frame - k[0].frame;
			t = (numframes != 0) ? ((frame - (float)k[0].frame) / (float)numframes) : 0.0f;
			QuaternionSlerp(q1, q2, t, q);
		}

		// position
		key = 1;
		for (mstudioposkey_t *pkey = &pposkey[1]; key < numposkeys; ++pkey, ++key)
		{
			if (pkey->frame > iframe)
				break;
		}

		if (key >= numposkeys)
		{
			mstudioposkey_t	*k = &pposkey[key - 1];
			float			*p = pos + 3 * bone;

			p[0] = k->pos[0];
			p[1] = k->pos[1];
			p[2] = k->pos[2];
		}
		else
		{
			mstudioposkey_t	*k = &pposkey[key - 1];
			int				numframes = k[1].frame - k[0].frame;
			float			t = (numframes != 0) ? ((frame - (float)k[0].frame) / (float)numframes) : 0.0f;
			float			it = 1.0f - t;
			float			*p = pos + 3 * bone;

			p[0] = k[0].pos[0] * it + k[1].pos[0] * t;
			p[1] = k[0].pos[1] * it + k[1].pos[1] * t;
			p[2] = k[0].pos[2] * it + k[1].pos[2] * t;
		}

		panim++;
	}

	// position controllers
	for (int i = 0; i < numbonecontrollers; ++i)
	{
		mstudiobonecontroller_t	*pbc = &pbonecontroller[i];
		float					t = (float)controller[i] / 255.0f;
		float					value = pbc->end * t + (1.0f - t) * pbc->start;
		float					*p = NULL;

		switch (pbc->type)
		{
		case STUDIO_X:
			p = &pos[3 * pbc->bone];
			break;
		case STUDIO_Y:
			p = &pos[3 * pbc->bone + 1];
			break;
		case STUDIO_Z:
			p = &pos[3 * pbc->bone + 2];
			break;
		}

		if (p)
			*p += value;
	}

	// the motion bone stays in place, the entity moves instead
	if (pseqdesc->motiontype & STUDIO_X)
		pos[3 * pseqdesc->motionbone] = 0.0f;
	if (pseqdesc->motiontype & STUDIO_Y)
		pos[3 * pseqdesc->motionbone + 1] = 0.0f;
	if (pseqdesc->motiontype & STUDIO_Z)
		pos[3 * pseqdesc->motionbone + 2] = 0.0f;
}

/*
================
R_StudioSetupBones

Builds the bone and lighting transforms of currententity,
blending in its previous sequence for a moment after a change.
================
*/
void R_StudioSetupBones(void)
{
	mstudioseqdesc_t	*pseqdesc;
	mstudiobone_t		*pbones;
	int					sequence;
	int					numframes;
	float				frame;

	if (!g_pStudioHeader || !currententity)
		return;

	sequence = currententity->sequence;
	if (g_pStudioHeader->numseq <= sequence)
	{
		currententity->sequence = 0;
		sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)g_pStudioHeader + g_pStudioHeader->seqindex) + sequence;
	numframes = pseqdesc->numframes;

	// entity frame is 0..255 over the whole sequence
	frame = currententity->frame * (float)(numframes - 1) / 256.0f;
	frame = frame + (float)((cl_time - (double)currententity->anim_time)
							* (double)currententity->framerate
							* (double)pseqdesc->fps);

	if (pseqdesc->flags & STUDIO_LOOPING)
	{
		if (numframes > 1)
			frame = (float)((1 - numframes) * (int)(frame / (float)(numframes - 1)) + frame);
	}
	else
	{
		if (frame < 0.0f)
			frame = 0.0f;
		if (frame > (float)numframes - 1.001f)
			frame = (float)numframes - 1.001f;
	}

	R_StudioCalcRotations((float *)g_bonepos, (float *)g_bonequat, sequence, frame);

	if (currententity->blend_time != 0.0f && currententity->blend_time + (float)SEQUENCE_BLEND_TIME > (float)cl_time)
	{
		int oldseq = currententity->blend_oldseq;
		if (g_pStudioHeader->numseq > oldseq)
		{
			float s = (float)((cl_time - (double)currententity->blend_time) / SEQUENCE_BLEND_TIME);
			float t = 1.0f - s;

			R_StudioCalcRotations((float *)g_bonepos2, (float *)g_bonequat2, oldseq, currententity->blend_oldframe);

			for (int i = 0; i < g_pStudioHeader->numbones; ++i)
			{
				QuaternionSlerp(g_bonequat[i], g_bonequat2[i], t, g_bonequat[i]);

				g_bonepos[i][0] = g_bonepos2[i][0] * (1.0f - s) + g_bonepos[i][0] * s;
				g_bonepos[i][1] = g_bonepos2[i][1] * (1.0f - s) + g_bonepos[i][1] * s;
				g_bonepos[i][2] = g_bonepos2[i][2] * (1.0f - s) + g_bonepos[i][2] * s;
			}
		}
		else
		{
			currententity->blend_oldframe = frame;
		}
	}
	else
	{
		currententity->blend_oldframe = frame;
	}

	pbones = (mstudiobone_t *)((byte *)g_pStudioHeader + g_pStudioHeader->boneindex);
	for (int i = 0; i < g_pStudioHeader->numbones; ++i)
	{
		float	bonematrix[3][4];

		QuaternionMatrix(g_bonequat[i], (float *)bonematrix);
		bonematrix[0][3] = g_bonepos[i][0];
		bonematrix[1][3] = g_bonepos[i][1];
		bonematrix[2][3] = g_bonepos[i][2];

		if (pbones[i].parent == -1)
		{
			memcpy(g_bonetransform[i], bonematrix, sizeof(bonematrix));
			memcpy(g_lighttransform[i], bonematrix, sizeof(bonematrix));
		}
		else
		{
			R_ConcatTransforms(g_bonetransform[pbones[i].parent], bonematrix, g_bonetransform[i]);
			R_ConcatTransforms(g_lighttransform[pbones[i].parent], bonematrix, g_lighttransform[i]);
		}
	}
}

/*
================
R_StudioEntityLight

Samples the world light below the entity and adds the dynamic lights.
================
*/
void R_StudioEntityLight(entity_t *ent, alight_t *plight)
{
	vec3_t			*plightvec;
	vec3_t			start, end;
	unsigned int	lightrgb[3];
	float			r, g, b;
	float			maxc;
	float			total;
	vec3_t			lightvec;

	if (r_fullbright.value == 1.0f)
	{
		plightvec = plight->plightvec;

		plight->ambient = 192;
		plight->shade = 0;
		(*plightvec)[0] = 0.0f;
		(*plightvec)[1] = 0.0f;
		(*plightvec)[2] = -1.0f;
		plight->color[0] = 1.0f;
		plight->color[1] = 1.0f;
		plight->color[2] = 1.0f;
		return;
	}

	start[0] = ent->origin[0];
	start[1] = ent->origin[1];
	start[2] = ent->origin[2] + 8.0f;

	end[0] = start[0];
	end[1] = start[1];
	end[2] = start[2] - 2048.0f;

	R_GetLightmap(lightrgb, start, end);

	r = (float)lightrgb[0];
	g = (float)lightrgb[1];
	b = (float)lightrgb[2];

	ent->lightcolor[0] = r;
	ent->lightcolor[1] = g;
	ent->lightcolor[2] = b;

	maxc = r;
	if (g > maxc)
		maxc = g;
	if (b > maxc)
		maxc = b;
	if (maxc == 0.0f)
		maxc = 1.0f;

	lightvec[0] = 0.0f;
	lightvec[1] = 0.0f;
	lightvec[2] = -1.0f;
	VectorScale(lightvec, maxc, lightvec);

	total = maxc;

	for (int i = 0; i < MAX_DLIGHTS; ++i)
	{
		dlight_t	*dl = &cl_dlights[i];
		vec3_t		delta;
		float		dist;
		float		add;
		float		scale;

		if (dl->die < cl_time)
			continue;

		delta[0] = start[0] - dl->origin[0];
		delta[1] = start[1] - dl->origin[1];
		delta[2] = start[2] - dl->origin[2];

		dist = Length(delta);
		add = dl->radius - dist;
		if (add <= 0.0f)
			continue;

		total += add;

		if (dist > 1.0f)
			scale = add / dist;
		else
			scale = add;

		VectorScale(delta, scale, delta);
		lightvec[0] += delta[0];
		lightvec[1] += delta[1];
		lightvec[2] += delta[2];

		r += (float)dl->color.r * add / 256.0f;
		g += (float)dl->color.g * add / 256.0f;
		b += (float)dl->color.b * add / 256.0f;
	}

	{
		float scale = (total >= 128.0f) ? cv_direct.value : (total / 128.0f) * cv_direct.value;
		VectorScale(lightvec, scale, lightvec);
	}

	plightvec = plight->plightvec;

	plight->shade = (int)Length(lightvec);
	plight->ambient = (int)(total - (float)plight->shade);

	maxc = r;
	if (g > maxc)
		maxc = g;
	if (b > maxc)
		maxc = b;
	if (maxc == 0.0f)
		maxc = 1.0f;

	plight->color[0] = r / maxc;
	plight->color[1] = g / maxc;
	plight->color[2] = b / maxc;

	if (plight->ambient > 128)
		plight->ambient = 128;
	if (plight->ambient + plight->shade > 256)
		plight->shade = 256 - plight->ambient;

	if (ent->effects & EF_MUZZLEFLASH)
	{
		ent->effects &= ~EF_MUZZLEFLASH;

		plight->ambient = plight->shade;
		plight->shade = 80;
		plight->color[0] = 1.0f;
		plight->color[1] = 1.0f;
		plight->color[2] = 0.65f;
	}

	VectorNormalize(lightvec);
	(*plightvec)[0] = lightvec[0];
	(*plightvec)[1] = lightvec[1];
	(*plightvec)[2] = lightvec[2];
}

/*
================
R_StudioDrawShadow

Projects the model onto the ground along g_shadow_dir.
================
*/
void R_StudioDrawShadow(void)
{
	mstudiomesh_t	*pmesh;
	float			height;
	float			lheight;

	if (!g_pStudioHeader || !g_pStudioModel || !currententity)
		return;

	height = currententity->origin[2] - lightspot[2];
	lheight = 1.0f - height;

	pmesh = (mstudiomesh_t *)((byte *)g_pStudioHeader + g_pStudioModel->meshindex);

	for (int m = 0; m < g_pStudioModel->nummesh; ++m)
	{
		mstudiotrivert_t *ptri = (mstudiotrivert_t *)((byte *)g_pStudioHeader + pmesh[m].triindex);

		for (int j = 0; j < pmesh[m].numtris; ++j, ptri += 3)
		{
			glBegin(GL_TRIANGLES);
			for (int k = 0; k < 3; ++k)
			{
				float	*v = g_transformedverts[ptri[k].vertindex];
				float	x = v[0];
				float	y = v[1];
				float	z = v[2];
				float	dist = height + z;
				float	point[3];

				point[0] = x - dist * g_shadow_dir[0];
				point[1] = y - dist * g_shadow_dir[1];
				point[2] = lheight;
				glVertex3fv(point);
			}
			glEnd();
		}
	}
}

/*
================
R_StudioDrawPoints

Transforms, lights and draws the meshes of the current sub-model.
================
*/
void R_StudioDrawPoints(alight_t *plight)
{
	mstudiomesh_t		*pmesh;
	mstudiotexture_t	*ptexture;
	mstudiomodeldata_t	*pmodeldata;
	byte				*pvertbone;
	byte				*pnormbone;
	float				*pstudioverts;
	float				*pstudionorms;
	float				*lv;

	if (!g_pStudioHeader || !g_pStudioModel)
		return;

	ptexture = (mstudiotexture_t *)((byte *)g_pStudioHeader + g_pStudioHeader->textureindex);
	pmesh = (mstudiomesh_t *)((byte *)g_pStudioHeader + g_pStudioModel->meshindex);

	pvertbone = (byte *)g_pStudioHeader + g_pStudioModel->vertinfoindex;
	pnormbone = (byte *)g_pStudioHeader + g_pStudioModel->norminfoindex;

	pmodeldata = (mstudiomodeldata_t *)((byte *)g_pStudioHeader + g_pStudioModel->modeldataindex);
	pstudioverts = (float *)((byte *)g_pStudioHeader + pmodeldata->vertindex);
	pstudionorms = (float *)((byte *)g_pStudioHeader + pmodeldata->normindex);

	g_r_studio_num_verts = g_pStudioModel->numverts;

	for (int i = 0; i < g_r_studio_num_verts; ++i)
		R_StudioTransformVertex(g_transformedverts[i], pvertbone[i], &pstudioverts[i * 3]);

	lv = g_lightvalues;
	for (int m = 0; m < g_pStudioModel->nummesh; ++m)
	{
		int flags = ptexture[pmesh[m].skinref].flags;

		for (int n = 0; n < pmesh[m].numnorms; ++n)
		{
			R_StudioLighting(lv, *pnormbone, (char)flags, pstudionorms);
			++lv;
			++pnormbone;
			pstudionorms += 3;
		}
	}

	g_r_studio_render_flag = 1;

	for (int m = 0; m < g_pStudioModel->nummesh; ++m)
	{
		mstudiotexture_t	*ptex = &ptexture[pmesh[m].skinref];
		float				s = 1.0f / (float)ptex->width;
		float				t = 1.0f / (float)ptex->height;
		mstudiotrivert_t	*ptri;
		int					flags;

		GL_Bind(ptex->index);

		ptri = (mstudiotrivert_t *)((byte *)g_pStudioHeader + pmesh[m].triindex);
		flags = ptex->flags;

		for (int j = 0; j < pmesh[m].numtris; ++j, ptri += 3)
		{
			glBegin(GL_TRIANGLES);
			for (int k = 0; k < 3; ++k)
			{
				int		vertindex = ptri[k].vertindex;
				int		normindex = ptri[k].normindex;
				float	lightvalue = g_lightvalues[normindex];

				if (flags & STUDIO_NF_CHROME)
					glTexCoord2f((float)g_chrome[normindex][0] * s, (float)g_chrome[normindex][1] * t);
				else
					glTexCoord2f((float)ptri[k].s * s, (float)ptri[k].t * t);

				glColor4f(
					plight->color[0] * lightvalue,
					plight->color[1] * lightvalue,
					plight->color[2] * lightvalue,
					r_blend_alpha);

				glVertex3f(
					g_transformedverts[vertindex][0],
					g_transformedverts[vertindex][1],
					g_transformedverts[vertindex][2]);
			}
			glEnd();
		}
	}
}

/*
================
R_DrawStudioModel
================
*/
void R_DrawStudioModel(entity_t *ent, alight_t *plight)
{
	float		angle;

	++g_r_studio_rendercount;

	g_pStudioHeader = (studiohdr_t *)Mod_Extradata(currententity->model);
	if (!g_pStudioHeader)
		return;

	angle = -ent->angles[YAW] * ((float)M_PI / 180.0f);
	g_shadow_dir[0] = sinf(angle);
	g_shadow_dir[1] = cosf(angle);
	g_shadow_dir[2] = 1.0f;
	VectorNormalize(g_shadow_dir);

	R_StudioSetupLighting(plight);

	glPushMatrix();
	R_RotateForEntity(ent);

	if (gl_smoothmodels.value != 0.0f)
		glShadeModel(GL_SMOOTH);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value != 0.0f)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	R_StudioSetupBones();

	for (int i = 0; i < g_pStudioHeader->numbodyparts; ++i)
	{
		int rendermode;

		R_StudioSetupModel(i);

		if (g_r_studio_hardware_color)
			ent->trivial_accept = 0;

		rendermode = ent->rendermode;
		if (rendermode)
		{
			if (rendermode == kRenderTransColor)
			{
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
			}
			else
			{
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
				if (rendermode == kRenderTransAdd)
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				else
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				glColor4f(1.0f, 1.0f, 1.0f, r_blend_alpha);
			}

			glEnable(GL_BLEND);
		}

		R_StudioDrawPoints(plight);

		if (r_shadows.value != 0.0f && rendermode != kRenderTransAdd)
		{
			float c = 1.0f - r_blend_alpha * 0.5f;

			glShadeModel(GL_FLAT);
			glDisable(GL_TEXTURE_2D);
			glBlendFunc(GL_ZERO, GL_SRC_COLOR);
			glEnable(GL_BLEND);
			glColor4f(c, c, c, 0.5f);
			glDepthMask(GL_FALSE);
			glEnable(GL_POLYGON_OFFSET_FILL);

			if (gl_ztrick.value == 0.0f || gldepthmin < 0.5f)
				glPolygonOffset(1.0f, -4.0f);
			else
				glPolygonOffset(1.0f, 4.0f);

			R_StudioDrawShadow();

			glDisable(GL_POLYGON_OFFSET_FILL);
			glEnable(GL_TEXTURE_2D);
			glDisable(GL_BLEND);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glDepthMask(GL_TRUE);

			if (gl_smoothmodels.value != 0.0f)
				glShadeModel(GL_SMOOTH);
		}
	}

	if (ent->rendermode)
		glDisable(GL_BLEND);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glShadeModel(GL_FLAT);

	if (gl_affinemodels.value != 0.0f)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);

	glPopMatrix();
}

/*
================
R_AddToTranslucentList

Keeps the list sorted far to near.
================
*/
void R_AddToTranslucentList(entity_t *ent)
{
	model_t		*model;
	vec3_t		center;
	vec3_t		org;
	vec3_t		delta;
	float		dist;
	int			i;

	if (r_numtranslucent == MAX_TRANS_ENTITIES)
		Sys_Error("AddTentity: too many translucent entities\n");

	model = ent->model;
	center[0] = model->maxs[0] + model->mins[0];
	center[1] = model->mins[1] + model->maxs[1];
	center[2] = model->maxs[2] + model->mins[2];
	VectorScale(center, 0.5f, center);

	org[0] = ent->origin[0] + center[0];
	org[1] = ent->origin[1] + center[1];
	org[2] = ent->origin[2] + center[2];

	delta[0] = r_origin[0] - org[0];
	delta[1] = r_origin[1] - org[1];
	delta[2] = r_origin[2] - org[2];
	dist = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];

	i = r_numtranslucent;
	while (i > 0)
	{
		if (r_translucent_dist[i - 1] >= dist)
			break;
		r_translucent_entities[i] = r_translucent_entities[i - 1];
		r_translucent_dist[i] = r_translucent_dist[i - 1];
		--i;
	}

	r_translucent_entities[i] = ent;
	r_translucent_dist[i] = dist;
	++r_numtranslucent;
}

/*
================
R_DrawTransEntitiesOnList
================
*/
void R_DrawTransEntitiesOnList(void)
{
	if (r_drawentities.value == 0.0f)
		return;

	for (int i = 0; i < r_numtranslucent; ++i)
	{
		entity_t	*ent = r_translucent_entities[i];
		model_t		*model;
		int			modeltype;

		currententity = ent;

		r_blend_alpha = (float)CL_FxBlend(ent) / 256.0f;
		if (r_blend_alpha == 0.0f)
			continue;

		model = ent->model;
		modeltype = model ? model->type : mod_brush;

		switch (modeltype)
		{
		case mod_sprite:
			R_DrawSpriteModel(ent);
			break;
		case mod_alias:
			R_DrawAliasModel(ent);
			break;
		case mod_studio:
			if (R_StudioCheckBBox())
			{
				alight_t	lighting;
				vec3_t		lightvec;

				lighting.plightvec = &lightvec;
				R_StudioEntityLight(ent, &lighting);
				R_DrawStudioModel(ent, &lighting);
			}
			break;
		case mod_brush:
		default:
			R_DrawBrushModel(ent);
			break;
		}
	}

	r_numtranslucent = 0;
	r_blend_alpha = 1.0f;
}
