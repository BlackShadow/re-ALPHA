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
#include "studio.h"

#include <gl/gl.h>
#include <math.h>
#include <string.h>

typedef float vec4_t[4];

typedef struct alight_s
{
	int ambient;
	int shade;
	float color[3];
	vec3_t *plightvec;
} alight_t;

#define ENT_ORIGIN_OFFSET 104
#define ENT_YAW_OFFSET 144
#define ENT_RENDERMODE_OFFSET 152
#define ENT_RENDERAMT_OFFSET 156
#define ENT_RENDERFX_OFFSET 164
#define ENT_MODEL_OFFSET 168
#define ENT_EFFECTS_OFFSET 188
#define ENT_UNKNOWN_208_OFFSET 208
#define ENT_BODY_OFFSET 228
#define ENT_SEQUENCE_OFFSET 232
#define ENT_CONTROLLER_OFFSET 236
#define ENT_FRAME_OFFSET 176
#define ENT_ANIMTIME_OFFSET 220
#define ENT_FRAMERATE_OFFSET 224
#define ENT_LATCH_ANIMTIME_OFFSET 244
#define ENT_BLEND_OLDFRAME_OFFSET 272
#define ENT_BLEND_TIME_OFFSET 276
#define ENT_BLEND_OLDSEQ_OFFSET 280
#define ENT_LATCH_CONTROLLER_OFFSET 284
#define ENT_LIGHTCOLOR_OFFSET 292

#define ENT_PTR(ent, offset, type) ((type *)((byte *)(ent) + (offset)))
#define ENT_FLOAT(ent, offset) (*ENT_PTR(ent, offset, float))
#define ENT_INT(ent, offset) (*ENT_PTR(ent, offset, int))
#define ENT_BYTE(ent, offset) (*ENT_PTR(ent, offset, byte))

extern entity_t *currententity;
extern double cl_time;

extern vec3_t r_origin;
extern vec3_t lightspot;
extern vec3_t r_light_rgb;

extern cvar_t cv_lambert;
extern cvar_t cv_direct;
extern float r_plightvec[3];

extern dlight_t cl_dlights[MAX_DLIGHTS];

extern cvar_t r_fullbright;
extern cvar_t r_drawentities;
extern cvar_t r_shadows;
extern cvar_t gl_smoothmodels;
extern cvar_t gl_affinemodels;
extern cvar_t gl_ztrick;

extern int lightgammatable[1024];

extern void R_RotateForEntity(int entity_ptr);
extern unsigned int *R_GetLightmap(unsigned int *lightrgb, vec3_t point, vec3_t end);

extern void R_DrawSpriteModel(entity_t *entity);
extern void R_DrawAliasModel(entity_t *entity);
extern void R_DrawBrushModel(entity_t *entity);

extern int CL_FxBlend(int entity_ptr);
extern vec3_t vpn;

static studiohdr_t *g_pStudioHeader;
static mstudiobodyparts_t *g_pBodypart;
static mstudiomodel_t *g_pStudioModel;

static int g_r_studio_rendercount;
static int g_r_studio_render_flag;
static int g_r_studio_num_verts;
static int g_r_studio_mode;
static int g_r_studio_hardware_color;

static int g_r_studio_ambient;
static float g_r_studio_shade;
static int g_r_studio_lightcolor_r;
static int g_r_studio_lightcolor_g;
static int g_r_studio_lightcolor_b;

static float g_shadow_dir[3];

static vec3_t g_bonepos[MAXSTUDIOBONES];
static vec4_t g_bonequat[MAXSTUDIOBONES];
static vec3_t g_bonepos2[MAXSTUDIOBONES];
static vec4_t g_bonequat2[MAXSTUDIOBONES];

static float g_bonetransform[MAXSTUDIOBONES][3][4];
static float g_lighttransform[MAXSTUDIOBONES][3][4];

static float g_transformedverts[MAXSTUDIOVERTS][3];
static float g_lightvalues[MAXSTUDIOVERTS];
static int g_chrome[MAXSTUDIOVERTS][2];

enum { MAX_TRANS_ENTITIES = 100 };
static int r_numtranslucent;
static int r_translucent_entities[MAX_TRANS_ENTITIES];
static float r_translucent_dist[MAX_TRANS_ENTITIES];

static int abs32(int x)
{
	return (x < 0) ? -x : x;
}

int R_StudioCheckBBox(void)
{
	return 1;
}

int R_StudioGetFrameCount(model_t *model)
{
	studiohdr_t *pstudio;
	mstudiobodyparts_t *pbodypart;

	if (!model || model->type != mod_studio)
		return 0;

	pstudio = (studiohdr_t *)Mod_Extradata(model);
	if (!pstudio)
		return 0;

	pbodypart = (mstudiobodyparts_t *)((byte *)pstudio + pstudio->bodypartindex);
	return pbodypart->nummodels;
}

int R_StudioSetupLighting(int lightptr)
{
	alight_t *plight;
	vec3_t *plightvec;
	float shade;
	int result;

	plight = (alight_t *)lightptr;
	plightvec = plight->plightvec;

	g_r_studio_ambient = plight->ambient;
	shade = (float)plight->shade;
	g_r_studio_shade = shade;

	r_plightvec[0] = (*plightvec)[0];
	r_plightvec[1] = (*plightvec)[1];
	r_plightvec[2] = (*plightvec)[2];

	g_r_studio_lightcolor_r = ((int)(plight->color[0] * 49407.0f)) & 0xFF00;
	g_r_studio_lightcolor_g = ((int)(plight->color[1] * 49407.0f)) & 0xFF00;
	result = ((int)(plight->color[2] * 49407.0f)) & 0xFF00;
	g_r_studio_lightcolor_b = result;
	return result;
}

static mstudiomodel_t *R_StudioSetupModel(int bodypart)
{
	mstudiobodyparts_t *pbodypart;
	int body;
	int modelOffset;

	if (!g_pStudioHeader)
		return NULL;

	if (bodypart > g_pStudioHeader->numbodyparts)
	{
		Con_DPrintf("R_StudioSetupModel: no such bodypart %d\n", bodypart);
		bodypart = 0;
	}

	pbodypart = (mstudiobodyparts_t *)((byte *)g_pStudioHeader + g_pStudioHeader->bodypartindex) + bodypart;
	body = ENT_INT(currententity, ENT_BODY_OFFSET);

	modelOffset = pbodypart->modelindex + 108 * ((body / pbodypart->base) % pbodypart->nummodels);
	g_pBodypart = pbodypart;
	g_pStudioModel = (mstudiomodel_t *)((byte *)g_pStudioHeader + modelOffset);
	return g_pStudioModel;
}

float *R_StudioTransformVertex(float *out, int boneIndex, float *in)
{
	float (*m)[4] = g_bonetransform[boneIndex];

	out[0] = m[0][0] * in[0] + m[0][1] * in[1] + m[0][2] * in[2] + m[0][3];
	out[1] = m[1][0] * in[0] + m[1][1] * in[1] + m[1][2] * in[2] + m[1][3];
	out[2] = m[2][0] * in[0] + m[2][1] * in[1] + m[2][2] * in[2] + m[2][3];
	return out;
}

float *R_StudioLighting(float *lv, int boneIndex, char flags, float *normal)
{
	float (*m)[4] = g_lighttransform[boneIndex];
	float nx, ny, nz;
	float illum;
	float ambientScale;
	float dot;

	nx = m[0][0] * normal[0] + m[0][1] * normal[1] + m[0][2] * normal[2];
	ny = m[1][0] * normal[0] + m[1][1] * normal[1] + m[1][2] * normal[2];
	nz = m[2][0] * normal[0] + m[2][1] * normal[1] + m[2][2] * normal[2];

	illum = (float)g_r_studio_ambient;

	if ((flags & 1) == 0 || g_r_studio_mode == 1)
	{
		ambientScale = cv_lambert.value;
		if (ambientScale <= 1.0f)
			ambientScale = 1.0f;

		dot = r_plightvec[0] * nx + r_plightvec[1] * ny + r_plightvec[2] * nz;
		dot = (dot - (ambientScale - 1.0f)) / ambientScale;
		if (dot < 0.0f)
			illum = illum - g_r_studio_shade * dot;
	}
	else
	{
		illum = g_r_studio_shade * 0.5f + illum;
	}

	if ((flags & 2) != 0)
	{
		int normalIndex = (int)(lv - g_lightvalues);
		float v = (vpn[1] * ny + vpn[2] * nz + vpn[0] * nx) * 2.0f;
		float s = v * nx - vpn[0];
		float t = v * nz - vpn[2];

		g_chrome[normalIndex][0] = (int)((s + 1.0f) * 32.0f);
		g_chrome[normalIndex][1] = (int)((t + 1.0f) * 32.0f);
	}

	if (illum > 255.0f)
		illum = 255.0f;

	*lv = (float)lightgammatable[(int)(illum * 4.0f)] * 0.0009765625f;
	return lv;
}

void R_StudioCalcRotations(float *pos, float *quat, int seq, float frame)
{
	mstudioseqdesc_t *pseqdesc;
	int frameInt;
	float controllerBlend;

	int bone;
	byte *controller;
	byte *controllerLatched;

	int numBoneControllers;
	mstudiobonecontroller_t *pcontrollers;

	byte *pAnim;
	int *pAnimBone;

	float adjX, adjY, adjZ;

	if (!g_pStudioHeader)
		return;

	frameInt = (int)frame;

	pseqdesc = (mstudioseqdesc_t *)((byte *)g_pStudioHeader + g_pStudioHeader->seqindex) + seq;
	pAnim = (byte *)g_pStudioHeader + pseqdesc->animindex;
	pAnimBone = (int *)(pAnim + 8);

	controllerBlend = 1.0f;
	if (ENT_FLOAT(currententity, ENT_LATCH_ANIMTIME_OFFSET) + 0.01f <= ENT_FLOAT(currententity, ENT_ANIMTIME_OFFSET))
	{
		controllerBlend = (float)((cl_time - (double)ENT_FLOAT(currententity, ENT_ANIMTIME_OFFSET)) / 0.1);
		if (controllerBlend > 2.0f)
			controllerBlend = 2.0f;
	}

	numBoneControllers = g_pStudioHeader->numbonecontrollers;
	pcontrollers = (mstudiobonecontroller_t *)((byte *)g_pStudioHeader + g_pStudioHeader->bonecontrollerindex);

	controller = (byte *)currententity + ENT_CONTROLLER_OFFSET;
	controllerLatched = (byte *)currententity + ENT_LATCH_CONTROLLER_OFFSET;

	for (bone = 0; bone < g_pStudioHeader->numbones; ++bone)
	{
		int numPosKeys;
		int numRotKeys;
		byte *pPos;
		byte *pRot;

		adjX = 0.0f;
		adjY = 0.0f;
		adjZ = 0.0f;

		pPos = (byte *)g_pStudioHeader + *(pAnimBone - 1);
		pRot = (byte *)g_pStudioHeader + pAnimBone[1];

		numPosKeys = *(pAnimBone - 2);
		numRotKeys = *pAnimBone;

		for (int i = 0; i < numBoneControllers; ++i)
		{
			mstudiobonecontroller_t *bc = &pcontrollers[i];
			float value;

			if (bc->bone != bone)
				continue;

			if ((bc->type & STUDIO_RLOOP) == 0)
			{
				float t = ((1.0f - controllerBlend) * (float)controllerLatched[i] + (float)controller[i] * controllerBlend) / 255.0f;
				if (t < 0.0f)
					t = 0.0f;
				if (t > 1.0f)
					t = 1.0f;

				value = bc->end * t + (1.0f - t) * bc->start;
			}
			else
			{
				int c0 = controller[i];
				int c1 = controllerLatched[i];

				if (abs32(c0 - c1) <= 128)
				{
					value = ((1.0f - controllerBlend) * (float)c1 + (float)c0 * controllerBlend) * 1.40625f + bc->start;
				}
				else
				{
					int a0 = (c0 + 128) & 0xFF;
					int a1 = (c1 + 128) & 0xFF;
					value = ((1.0f - controllerBlend) * (float)a1 + (float)a0 * controllerBlend - 128.0f) * 1.40625f + bc->start;
				}
			}

			switch (bc->type & 0x1FF)
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

		{
			int key = 1;
			__int16 *keys = (__int16 *)(pRot + 8);
			while (key < numRotKeys)
			{
				if (*keys > frameInt)
					break;
				keys += 4;
				++key;
			}

			if (key >= numRotKeys)
			{
				float angles[3];
				float *outQuat = quat + 4 * bone;
				__int16 *k = (__int16 *)(pRot + 8 * (key - 1));

				angles[0] = (float)k[2] / 100.0f + adjY;
				angles[1] = (float)k[3] / 100.0f + adjZ;
				angles[2] = (float)k[1] / 100.0f + adjX;
				AngleQuaternion(angles, outQuat);
			}
			else
			{
				float angles1[3];
				float angles2[3];
				float q1[4];
				float q2[4];
				float *outQuat = quat + 4 * bone;
				__int16 *k = (__int16 *)(pRot + 8 * (key - 1));

				angles1[0] = (float)k[2] / 100.0f + adjY;
				angles1[1] = (float)k[3] / 100.0f + adjZ;
				angles1[2] = (float)k[1] / 100.0f + adjX;
				AngleQuaternion(angles1, q1);

				angles2[0] = (float)k[6] / 100.0f + adjY;
				angles2[1] = (float)k[7] / 100.0f + adjZ;
				angles2[2] = (float)k[5] / 100.0f + adjX;
				AngleQuaternion(angles2, q2);

				{
					int timeDelta = k[4] - k[0];
					float t = (timeDelta != 0) ? ((frame - (float)k[0]) / (float)timeDelta) : 0.0f;
					QuaternionSlerp(q1, q2, t, outQuat);
				}
			}
		}

		{
			int key = 1;
			byte *kptr = pPos + 16;
			while (key < numPosKeys)
			{
				if (*(__int16 *)kptr > frameInt)
					break;
				kptr += 16;
				++key;
			}

			if (key >= numPosKeys)
			{
				float *k = (float *)(pPos + 16 * (key - 1));
				float *dst = pos + 3 * bone;
				dst[0] = k[1];
				dst[1] = k[2];
				dst[2] = k[3];
			}
			else
			{
				float *k = (float *)(pPos + 16 * (key - 1));
				int timeDelta = *((__int16 *)k + 8) - *(__int16 *)k;
				float t = (timeDelta != 0) ? ((frame - (float)*(__int16 *)k) / (float)timeDelta) : 0.0f;
				float it = 1.0f - t;
				float *dst = pos + 3 * bone;

				dst[0] = k[1] * it + k[5] * t;
				dst[1] = k[2] * it + k[6] * t;
				dst[2] = k[3] * it + k[7] * t;
			}
		}

		pAnimBone += 4;
	}

	for (int i = 0; i < numBoneControllers; ++i)
	{
		mstudiobonecontroller_t *bc = &pcontrollers[i];
		float t = (float)controller[i] / 255.0f;
		float value = bc->end * t + (1.0f - t) * bc->start;
		float *dst = NULL;

		switch (bc->type)
		{
			case STUDIO_X:
				dst = &pos[3 * bc->bone];
				break;
			case STUDIO_Y:
				dst = &pos[3 * bc->bone + 1];
				break;
			case STUDIO_Z:
				dst = &pos[3 * bc->bone + 2];
				break;
		}

		if (dst)
			*dst += value;
	}

	if (pseqdesc->motiontype & STUDIO_X)
		pos[3 * pseqdesc->motionbone] = 0.0f;
	if (pseqdesc->motiontype & STUDIO_Y)
		pos[3 * pseqdesc->motionbone + 1] = 0.0f;
	if (pseqdesc->motiontype & STUDIO_Z)
		pos[3 * pseqdesc->motionbone + 2] = 0.0f;
}

int R_StudioSetupBones(void)
{
	int sequence;
	mstudioseqdesc_t *pseqdesc;
	int numframes;
	float baseFrame;
	float frame;

	if (!g_pStudioHeader || !currententity)
		return 0;

	sequence = ENT_INT(currententity, ENT_SEQUENCE_OFFSET);
	if (g_pStudioHeader->numseq <= sequence)
	{
		ENT_INT(currententity, ENT_SEQUENCE_OFFSET) = 0;
		sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)g_pStudioHeader + g_pStudioHeader->seqindex) + sequence;
	numframes = pseqdesc->numframes;

	baseFrame = ENT_FLOAT(currententity, ENT_FRAME_OFFSET) * (float)(numframes - 1) / 256.0f;
	frame = baseFrame + (float)((cl_time - (double)ENT_FLOAT(currententity, ENT_ANIMTIME_OFFSET))
								* (double)ENT_FLOAT(currententity, ENT_FRAMERATE_OFFSET)
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

	if (ENT_FLOAT(currententity, ENT_BLEND_TIME_OFFSET) != 0.0f && ENT_FLOAT(currententity, ENT_BLEND_TIME_OFFSET) + 0.2f > (float)cl_time)
	{
		int oldseq = ENT_INT(currententity, ENT_BLEND_OLDSEQ_OFFSET);
		if (g_pStudioHeader->numseq > oldseq)
		{
			float blend = (float)((cl_time - (double)ENT_FLOAT(currententity, ENT_BLEND_TIME_OFFSET)) / 0.2);
			float slerpT = 1.0f - blend;

			R_StudioCalcRotations((float *)g_bonepos2, (float *)g_bonequat2, oldseq, ENT_FLOAT(currententity, ENT_BLEND_OLDFRAME_OFFSET));

			for (int i = 0; i < g_pStudioHeader->numbones; ++i)
			{
				QuaternionSlerp((float *)g_bonequat[i], (float *)g_bonequat2[i], slerpT, (float *)g_bonequat[i]);

				g_bonepos[i][0] = g_bonepos2[i][0] * (1.0f - blend) + g_bonepos[i][0] * blend;
				g_bonepos[i][1] = g_bonepos2[i][1] * (1.0f - blend) + g_bonepos[i][1] * blend;
				g_bonepos[i][2] = g_bonepos2[i][2] * (1.0f - blend) + g_bonepos[i][2] * blend;
			}
		}
		else
		{
			ENT_FLOAT(currententity, ENT_BLEND_OLDFRAME_OFFSET) = frame;
		}
	}
	else
	{
		ENT_FLOAT(currententity, ENT_BLEND_OLDFRAME_OFFSET) = frame;
	}

	{
		mstudiobone_t *pbones = (mstudiobone_t *)((byte *)g_pStudioHeader + g_pStudioHeader->boneindex);
		for (int i = 0; i < g_pStudioHeader->numbones; ++i)
		{
			float m[3][4];
			QuaternionMatrix((float *)g_bonequat[i], (float *)m);
			m[0][3] = g_bonepos[i][0];
			m[1][3] = g_bonepos[i][1];
			m[2][3] = g_bonepos[i][2];

			if (pbones[i].parent == -1)
			{
				memcpy(g_bonetransform[i], m, sizeof(m));
				memcpy(g_lighttransform[i], m, sizeof(m));
			}
			else
			{
				R_ConcatTransforms(g_bonetransform[pbones[i].parent], m, g_bonetransform[i]);
				R_ConcatTransforms(g_lighttransform[pbones[i].parent], m, g_lighttransform[i]);
			}
		}
	}

	return (int)g_pStudioHeader;
}

int R_StudioEntityLight(int entity_ptr, int lightptr)
{
	alight_t *plight;
	vec3_t *plightvec;
	vec3_t start, end;
	unsigned int lightrgb[3];

	float r, g, b;
	float maxc;
	float total;
	vec3_t lightvec;

	if (r_fullbright.value == 1.0f)
	{
		plight = (alight_t *)lightptr;
		plightvec = plight->plightvec;

		plight->ambient = 192;
		plight->shade = 0;
		(*plightvec)[0] = 0.0f;
		(*plightvec)[1] = 0.0f;
		(*plightvec)[2] = -1.0f;
		plight->color[0] = 1.0f;
		plight->color[1] = 1.0f;
		plight->color[2] = 1.0f;

		return 0x3F800000;
	}

	start[0] = ENT_FLOAT((void *)entity_ptr, ENT_ORIGIN_OFFSET);
	start[1] = ENT_FLOAT((void *)entity_ptr, ENT_ORIGIN_OFFSET + 4);
	start[2] = ENT_FLOAT((void *)entity_ptr, ENT_ORIGIN_OFFSET + 8) + 8.0f;

	end[0] = start[0];
	end[1] = start[1];
	end[2] = start[2] - 2048.0f;

	R_GetLightmap(lightrgb, start, end);

	r = (float)lightrgb[0];
	g = (float)lightrgb[1];
	b = (float)lightrgb[2];

	ENT_FLOAT((void *)entity_ptr, ENT_LIGHTCOLOR_OFFSET) = r;
	ENT_FLOAT((void *)entity_ptr, ENT_LIGHTCOLOR_OFFSET + 4) = g;
	ENT_FLOAT((void *)entity_ptr, ENT_LIGHTCOLOR_OFFSET + 8) = b;

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
		dlight_t *dl = &cl_dlights[i];
		vec3_t delta;
		float dist;
		float add;
		float scaledAdd;

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
			scaledAdd = add / dist;
		else
			scaledAdd = add;

		VectorScale(delta, scaledAdd, delta);
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

	plight = (alight_t *)lightptr;
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

	if (ENT_INT((void *)entity_ptr, ENT_EFFECTS_OFFSET) & 2)
	{
		ENT_INT((void *)entity_ptr, ENT_EFFECTS_OFFSET) &= ~2;

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

	{
		int ret;
		memcpy(&ret, &lightvec[0], sizeof(ret));
		return ret;
	}
}

int R_StudioDrawShadow(void)
{
	mstudiomesh_t *pmesh;
	int meshIndex;
	float zOffset;
	float pointZ;

	if (!g_pStudioHeader || !g_pStudioModel || !currententity)
		return 0;

	zOffset = ENT_FLOAT(currententity, ENT_ORIGIN_OFFSET + 8) - lightspot[2];
	pointZ = 1.0f - zOffset;

	pmesh = (mstudiomesh_t *)((byte *)g_pStudioHeader + g_pStudioModel->meshindex);

	for (meshIndex = 0; meshIndex < g_pStudioModel->nummesh; ++meshIndex)
	{
		short *tri = (short *)((byte *)g_pStudioHeader + pmesh[meshIndex].triindex);

		for (int t = 0; t < pmesh[meshIndex].numtris; ++t, tri += 12)
		{
			glBegin(GL_TRIANGLES);
			for (int k = 0; k < 3; ++k)
			{
				int vindex = tri[k * 4 + 0];
				float x = g_transformedverts[vindex][0];
				float y = g_transformedverts[vindex][1];
				float z = g_transformedverts[vindex][2];

				float worldZDiff = zOffset + z;
				float point[3];
				point[0] = x - worldZDiff * g_shadow_dir[0];
				point[1] = y - worldZDiff * g_shadow_dir[1];
				point[2] = pointZ;
				glVertex3fv(point);
			}
			glEnd();
		}
	}

	return (int)g_pStudioHeader;
}

int R_StudioDrawPoints(int entity_ptr, int lightptr)
{
	alight_t *plight;
	mstudiomesh_t *pmesh;
	mstudiotexture_t *ptextures;
	mstudiomodeldata_t *pmodeldata;

	byte *pvertbone;
	byte *pnormbone;
	float *pverts;
	float *pnorms;
	float *lv;

	if (!g_pStudioHeader || !g_pStudioModel)
		return 0;

	plight = (alight_t *)lightptr;

	ptextures = (mstudiotexture_t *)((byte *)g_pStudioHeader + g_pStudioHeader->textureindex);
	pmesh = (mstudiomesh_t *)((byte *)g_pStudioHeader + g_pStudioModel->meshindex);

	pvertbone = (byte *)g_pStudioHeader + g_pStudioModel->vertinfoindex;
	pnormbone = (byte *)g_pStudioHeader + g_pStudioModel->norminfoindex;

	pmodeldata = (mstudiomodeldata_t *)((byte *)g_pStudioHeader + g_pStudioModel->modeldataindex);
	pverts = (float *)((byte *)g_pStudioHeader + pmodeldata->vertindex);
	pnorms = (float *)((byte *)g_pStudioHeader + pmodeldata->normindex);

	g_r_studio_num_verts = g_pStudioModel->numverts;

	for (int i = 0; i < g_r_studio_num_verts; ++i)
	{
		R_StudioTransformVertex(g_transformedverts[i], pvertbone[i], &pverts[i * 3]);
	}

	lv = g_lightvalues;
	for (int m = 0; m < g_pStudioModel->nummesh; ++m)
	{
		int flags = ptextures[pmesh[m].skinref].flags;
		for (int n = 0; n < pmesh[m].numnorms; ++n)
		{
			R_StudioLighting(lv, *pnormbone, (char)flags, pnorms);
			++lv;
			++pnormbone;
			pnorms += 3;
		}
	}

	g_r_studio_render_flag = 1;

	for (int m = 0; m < g_pStudioModel->nummesh; ++m)
	{
		mstudiotexture_t *tex = &ptextures[pmesh[m].skinref];
		float sScale = 1.0f / (float)tex->width;
		float tScale = 1.0f / (float)tex->height;

		GL_Bind(tex->index);

		{
			short *tri = (short *)((byte *)g_pStudioHeader + pmesh[m].triindex);
			int flags = tex->flags;

			for (int t = 0; t < pmesh[m].numtris; ++t, tri += 12)
			{
				glBegin(GL_TRIANGLES);
				for (int k = 0; k < 3; ++k)
				{
					int vindex = tri[k * 4 + 0];
					int nindex = tri[k * 4 + 1];
					float intensity = g_lightvalues[nindex];

					if ((flags & 2) != 0)
					{
						glTexCoord2f((float)g_chrome[nindex][0] * sScale, (float)g_chrome[nindex][1] * tScale);
					}
					else
					{
						glTexCoord2f((float)tri[k * 4 + 2] * sScale, (float)tri[k * 4 + 3] * tScale);
					}

					glColor4f(
						plight->color[0] * intensity,
						plight->color[1] * intensity,
						plight->color[2] * intensity,
						r_blend_alpha);

					glVertex3f(
						g_transformedverts[vindex][0],
						g_transformedverts[vindex][1],
						g_transformedverts[vindex][2]);
				}
				glEnd();
			}
		}
	}

	return (int)g_pStudioHeader;
}

void R_DrawStudioModel(int entity_ptr, int lightptr)
{
	alight_t *plight;
	float yawRad;

	++g_r_studio_rendercount;

	g_pStudioHeader = (studiohdr_t *)Mod_Extradata(*(model_t **)((byte *)currententity + ENT_MODEL_OFFSET));
	if (!g_pStudioHeader)
		return;

	yawRad = -ENT_FLOAT((void *)entity_ptr, ENT_YAW_OFFSET) * (3.14159265358979323846f / 180.0f);
	g_shadow_dir[0] = sinf(yawRad);
	g_shadow_dir[1] = cosf(yawRad);
	g_shadow_dir[2] = 1.0f;
	VectorNormalize(g_shadow_dir);

	plight = (alight_t *)lightptr;
	R_StudioSetupLighting((int)plight);

	glPushMatrix();
	R_RotateForEntity(entity_ptr);

	if (gl_smoothmodels.value != 0.0f)
		glShadeModel(GL_SMOOTH);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value != 0.0f)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	R_StudioSetupBones();

	for (int i = 0; i < g_pStudioHeader->numbodyparts; ++i)
	{
		R_StudioSetupModel(i);

		if (g_r_studio_hardware_color)
			ENT_INT((void *)entity_ptr, ENT_UNKNOWN_208_OFFSET) = 0;

		{
			int rendermode = ENT_INT((void *)entity_ptr, ENT_RENDERMODE_OFFSET);
			if (rendermode)
			{
				if (rendermode == 1)
				{
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
				}
				else
				{
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
					if (rendermode == 5)
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					else
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

					glColor4f(1.0f, 1.0f, 1.0f, r_blend_alpha);
				}

				glEnable(GL_BLEND);
			}

			R_StudioDrawPoints(entity_ptr, (int)plight);

			if (r_shadows.value != 0.0f && rendermode != 5)
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
	}

	if (ENT_INT((void *)entity_ptr, ENT_RENDERMODE_OFFSET))
		glDisable(GL_BLEND);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glShadeModel(GL_FLAT);

	if (gl_affinemodels.value != 0.0f)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);

	glPopMatrix();
}

int R_AddToTranslucentList(int entity_ptr)
{
	model_t *model;
	vec3_t center;
	vec3_t worldCenter;
	vec3_t delta;
	float distSq;
	int insertPos;

	if (r_numtranslucent == MAX_TRANS_ENTITIES)
		Sys_Error("AddTentity: too many translucent entities\n");

	model = *(model_t **)((byte *)entity_ptr + ENT_MODEL_OFFSET);
	center[0] = model->maxs[0] + model->mins[0];
	center[1] = model->mins[1] + model->maxs[1];
	center[2] = model->maxs[2] + model->mins[2];
	VectorScale(center, 0.5f, center);

	worldCenter[0] = ENT_FLOAT((void *)entity_ptr, ENT_ORIGIN_OFFSET) + center[0];
	worldCenter[1] = ENT_FLOAT((void *)entity_ptr, ENT_ORIGIN_OFFSET + 4) + center[1];
	worldCenter[2] = ENT_FLOAT((void *)entity_ptr, ENT_ORIGIN_OFFSET + 8) + center[2];

	delta[0] = r_origin[0] - worldCenter[0];
	delta[1] = r_origin[1] - worldCenter[1];
	delta[2] = r_origin[2] - worldCenter[2];
	distSq = delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];

	insertPos = r_numtranslucent;
	while (insertPos > 0)
	{
		if (r_translucent_dist[insertPos - 1] >= distSq)
			break;
		r_translucent_entities[insertPos] = r_translucent_entities[insertPos - 1];
		r_translucent_dist[insertPos] = r_translucent_dist[insertPos - 1];
		--insertPos;
	}

	r_translucent_entities[insertPos] = entity_ptr;
	r_translucent_dist[insertPos] = distSq;
	++r_numtranslucent;

	return *(int *)&distSq;
}

void R_DrawTransEntitiesOnList(void)
{
	if (r_drawentities.value == 0.0f)
		return;

	for (int i = 0; i < r_numtranslucent; ++i)
	{
		int ent = r_translucent_entities[i];
		model_t *model;
		int modeltype;

		currententity = (entity_t *)ent;

		r_blend_alpha = (float)CL_FxBlend(ent) / 256.0f;
		if (r_blend_alpha == 0.0f)
			continue;

		model = *(model_t **)((byte *)ent + ENT_MODEL_OFFSET);
		modeltype = model ? model->type : 0;

		switch (modeltype)
		{
			case mod_sprite:
				R_DrawSpriteModel((entity_t *)ent);
				break;
			case mod_alias:
				R_DrawAliasModel((entity_t *)ent);
				break;
			case mod_studio:
				if (R_StudioCheckBBox())
				{
					alight_t lighting;
					vec3_t lightvec;
					lighting.plightvec = &lightvec;
					R_StudioEntityLight(ent, (int)&lighting);
					R_DrawStudioModel(ent, (int)&lighting);
				}
				break;
			case mod_brush:
			default:
				R_DrawBrushModel((entity_t *)ent);
				break;
		}
	}

	r_numtranslucent = 0;
	r_blend_alpha = 1.0f;
}
