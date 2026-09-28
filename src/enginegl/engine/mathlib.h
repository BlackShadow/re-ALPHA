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

// mathlib.h

#ifndef MATHLIB_H
#define MATHLIB_H

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

// angle indexes
#define PITCH	0	// up / down
#define YAW		1	// left / right
#define ROLL	2	// fall over

struct mplane_s;

extern vec3_t vec3_origin;

#define IS_NAN(x) ((x) != (x))

#define DotProduct(x, y)		((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorSubtract(a, b, c)	do { (c)[0] = (a)[0] - (b)[0]; (c)[1] = (a)[1] - (b)[1]; (c)[2] = (a)[2] - (b)[2]; } while (0)
#define VectorAdd(a, b, c)		do { (c)[0] = (a)[0] + (b)[0]; (c)[1] = (a)[1] + (b)[1]; (c)[2] = (a)[2] + (b)[2]; } while (0)
#define VectorCopy(a, b)		do { (b)[0] = (a)[0]; (b)[1] = (a)[1]; (b)[2] = (a)[2]; } while (0)
#define VectorScale(a, b, c)	do { (c)[0] = (a)[0] * (b); (c)[1] = (a)[1] * (b); (c)[2] = (a)[2] * (b); } while (0)
#define VectorMA(a, b, c, d)	do { (d)[0] = (a)[0] + (b) * (c)[0]; (d)[1] = (a)[1] + (b) * (c)[1]; (d)[2] = (a)[2] + (b) * (c)[2]; } while (0)
#define VectorClear(a)			do { (a)[0] = 0.0f; (a)[1] = 0.0f; (a)[2] = 0.0f; } while (0)

// function versions of the macros above
void _VectorScale(vec3_t in, vec_t scale, vec3_t out);
void _VectorMA(vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);
void _VectorCopy(vec3_t in, vec3_t out);

int VectorCompare(vec3_t v1, vec3_t v2);
vec_t Length(vec3_t v);
#define VectorLength Length
void CrossProduct(vec3_t v1, vec3_t v2, vec3_t cross);
float VectorNormalize(vec3_t v);		// returns vector length

void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);

void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
float anglemod(float a);
int GreatestCommonDivisor(int i1, int i2);

void AngleQuaternion(float *angles, float *quaternion);
void QuaternionMatrix(float *quaternion, float *matrix);
void QuaternionSlerp(float *p, float *q, float t, float *qt);
void VectorTransform(vec3_t in, float matrix[3][4], vec3_t out);

void BOPS_Error(void);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct mplane_s *plane);

#endif // MATHLIB_H
