/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
****/
#ifndef MATHLIB_H
#define MATHLIB_H

// mathlib.h: the vector and quaternion math MDLDec needs, with the alpha
// engine's angle order (pitch, yaw, roll)

#include <math.h>

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];	// x,y,z,w

#ifndef M_PI
#define M_PI	3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#define Q_PI	3.14159265358979323846f

#define DotProduct(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorAdd(a, b, c)			do { (c)[0] = (a)[0] + (b)[0]; (c)[1] = (a)[1] + (b)[1]; (c)[2] = (a)[2] + (b)[2]; } while (0)
#define VectorScale(a, b, c)		do { (c)[0] = (a)[0] * (b); (c)[1] = (a)[1] * (b); (c)[2] = (a)[2] * (b); } while (0)
#define VectorCopy(a, b)			do { (b)[0] = (a)[0]; (b)[1] = (a)[1]; (b)[2] = (a)[2]; } while (0)

float VectorNormalize(vec3_t v);
void VectorTransform(const vec3_t in, const float matrix[3][4], vec3_t out);
void VectorRotate(const vec3_t in, const float matrix[3][4], vec3_t out);
void R_ConcatTransforms(const float in1[3][4], const float in2[3][4], float out[3][4]);

void AngleQuaternion(const vec3_t angles, vec4_t quaternion);
void QuaternionMatrix(const vec4_t quaternion, float matrix[3][4]);
void QuaternionSlerp(const vec4_t p, const vec4_t q, float t, vec4_t qt);
void QuaternionAngles(const vec4_t quaternion, vec3_t angles);

#endif // MATHLIB_H
