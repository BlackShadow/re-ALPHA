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
#include <float.h>
#include <math.h>

vec3_t vec3_origin = { 0.0f, 0.0f, 0.0f };

#undef VectorCopy
#undef VectorScale
#undef VectorMA

void VectorScale(vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];

	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];

	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
}

void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];

	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];

	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}

int GreatestCommonDivisor(int i1, int i2)
{
	int temp;

	while (1)
	{
		while (i2 >= i1)
		{
			if (!i1)
				return i2;
			i2 = i2 % i1;
		}

		if (!i2)
			break;

		temp = i2;
		i2 = i1 % i2;
		i1 = temp;
	}

	return i1;
}

float anglemod(float a)
{
	return (float)((double)(unsigned short)((int)(a * 182.0444444444445f) & 65535) * 0.0054931640625);
}

void BOPS_Error(void)
{
	Sys_Error("BoxOnPlaneSide:  Bad signbits");
}

void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI * 2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI * 2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI * 2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;
	right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
	right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
	right[2] = -1 * sr * cp;
	up[0] = (cr * sp * cy + -sr * -sy);
	up[1] = (cr * sp * sy + -sr * cy);
	up[2] = cr * cp;
}

void VectorTransform(vec3_t in, float matrix[3][4], vec3_t out)
{
	out[0] = matrix[0][0] * in[0] + matrix[0][1] * in[1] + matrix[0][2] * in[2] + matrix[0][3];
	out[1] = matrix[1][0] * in[0] + matrix[1][1] * in[1] + matrix[1][2] * in[2] + matrix[1][3];
	out[2] = matrix[2][0] * in[0] + matrix[2][1] * in[1] + matrix[2][2] * in[2] + matrix[2][3];
}

int VectorCompare(vec3_t v1, vec3_t v2)
{
	int		i;

	for (i = 0; i < 3; i++)
		if (v1[i] != v2[i])
			return 0;

	return 1;
}

void VectorMA(vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

void VectorCopy(vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void CrossProduct(vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

vec_t Length(vec3_t v)
{
	int		i;
	float	length;

	length = 0;
	for (i = 0; i < 3; i++)
		length += v[i] * v[i];
	length = sqrt(length);

	return length;
}

float VectorNormalize(vec3_t v)
{
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt(length);

	if (length)
	{
		ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

#define M_PI_DIV_180	0.0174532925199433f

float	g_SlerpCosom;
float	s_slerp_p;
float	s_slerp_q;
float	s_slerp_omega;
float	s_slerp_sinom;

float *AngleQuaternion(float *angles, float *quaternion)
{
	float angle;
	float sr, sp, sy, cr, cp, cy;
	double halfAngle;

	angle = angles[1] * M_PI_DIV_180;
	halfAngle = angle / 2.0;
	sy = (float)sin(halfAngle);
	cy = (float)cos(halfAngle);

	angle = angles[0] * M_PI_DIV_180;
	halfAngle = angle / 2.0;
	sp = (float)sin(halfAngle);
	cp = (float)cos(halfAngle);

	angle = angles[2] * M_PI_DIV_180;
	halfAngle = angle / 2.0;
	sr = (float)sin(halfAngle);
	cr = (float)cos(halfAngle);

	quaternion[0] = sr * cp * cy - cr * sp * sy;
	quaternion[1] = cr * sp * cy + sr * cp * sy;
	quaternion[2] = cr * cp * sy - sr * sp * cy;
	quaternion[3] = cr * cp * cy + sr * sp * sy;

	return quaternion;
}

int QuaternionMatrix(float *quaternion, float *matrix)
{
	float xx, yy, zz, xy, xz, yz, wx, wy, wz;
	float x = quaternion[0];
	float y = quaternion[1];
	float z = quaternion[2];
	float w = quaternion[3];

	xx = x * x;
	yy = y * y;
	zz = z * z;
	xy = x * y;
	xz = x * z;
	yz = y * z;
	wx = w * x;
	wy = w * y;
	wz = w * z;

	matrix[0] = 1.0f - 2.0f * (yy + zz);
	matrix[4] = 2.0f * (xy + wz);
	matrix[8] = 2.0f * (xz - wy);

	matrix[1] = 2.0f * (xy - wz);
	matrix[5] = 1.0f - 2.0f * (xx + zz);
	matrix[9] = 2.0f * (yz + wx);

	matrix[2] = 2.0f * (xz + wy);
	matrix[6] = 2.0f * (yz - wx);
	matrix[10] = 1.0f - 2.0f * (xx + yy);

	return *(int *)&matrix[6];
}

int QuaternionSlerp(float *p, float *q, float t, float *qt)
{
	float *pSrc = p;
	float *qSrc = q;
	float sumDiffSq = 0.0f;
	float sumAddSq = 0.0f;
	float diff, sum;
	float omega, sinom, sclp, sclq;
	int i;

	for (i = 0; i < 4; i++)
	{
		diff = pSrc[i] - qSrc[i];
		sum = pSrc[i] + qSrc[i];
		sumDiffSq += diff * diff;
		sumAddSq += sum * sum;
	}

	if (sumAddSq < sumDiffSq)
	{
		for (i = 0; i < 4; i++)
		{
			q[i] = -q[i];
		}
	}

	g_SlerpCosom = q[2] * p[2] + q[1] * p[1] + q[3] * p[3] + q[0] * p[0];

	if (g_SlerpCosom + 1.0 <= 0.00000001)
	{
		qt[0] = -p[1];
		qt[1] = p[0];
		qt[2] = -p[3];
		qt[3] = p[2];

			s_slerp_p = sinf((1.0 - t) * 1.570796326794897);
		s_slerp_q = sinf(t * 1.570796326794897);

		for (i = 0; i < 3; i++)
		{
			qt[i] = qt[i] * s_slerp_q + pSrc[i] * s_slerp_p;
		}
		return 3;
	}
	else
	{
		if (1.0 - g_SlerpCosom <= 0.00000001)
		{
			sclq = t;
			s_slerp_p = 1.0f - t;
		}
		else
		{
			omega = acos(g_SlerpCosom);
			s_slerp_omega = omega;
			sinom = sinf(omega);
			s_slerp_sinom = sinom;

			sclp = sinf((1.0f - t) * omega) / sinom;
			s_slerp_p = sclp;

			sclq = sinf(t * omega) / sinom;
		}

		s_slerp_q = sclq;

		for (i = 0; i < 4; i++)
		{
			qt[i] = qSrc[i] * sclq + pSrc[i] * s_slerp_p;
		}
		return 4;
	}
}

int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, mplane_t *p)
{
	float distMin, distMax;

	if (p->signbits >= 8)
		BOPS_Error();

	switch (p->signbits)
	{
	case 0:
		distMin = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		distMax = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		break;
	case 1:
		distMin = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		distMax = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		break;
	case 2:
		distMin = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		distMax = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		break;
	case 3:
		distMin = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		distMax = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		break;
	case 4:
		distMin = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		distMax = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		break;
	case 5:
		distMin = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		distMax = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		break;
	case 6:
		distMin = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		distMax = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		break;
	case 7:
		distMin = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		distMax = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		break;
	}

	return (unsigned char)((2 * (distMin < p->dist)) + (distMax >= p->dist));
}

void _fpclear(void)
{
	_clearfp();
}

