/*
==============================================================

    Minimal mathlib for MDLDec (Half-Life Alpha)

==============================================================
*/

#pragma once

#ifndef _MATHLIB_H
#define _MATHLIB_H

#include <math.h>

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef Q_PI
#define Q_PI 3.14159265358979323846f
#endif

#define DotProduct(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorAdd(a, b, c)   \
    do                       \
    {                        \
        (c)[0] = (a)[0] + (b)[0]; \
        (c)[1] = (a)[1] + (b)[1]; \
        (c)[2] = (a)[2] + (b)[2]; \
    } while (0)
#define VectorScale(a, b, c) \
    do                       \
    {                        \
        (c)[0] = (a)[0] * (b); \
        (c)[1] = (a)[1] * (b); \
        (c)[2] = (a)[2] * (b); \
    } while (0)
#define VectorCopy(a, b)     \
    do                       \
    {                        \
        (b)[0] = (a)[0];     \
        (b)[1] = (a)[1];     \
        (b)[2] = (a)[2];     \
    } while (0)

float VectorNormalize(vec3_t v);
void VectorTransform(vec3_t in, float matrix[3][4], vec3_t out);
void VectorRotate(vec3_t in, float matrix[3][4], vec3_t out);
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);

float *AngleQuaternion(float *angles, float *quaternion);
int QuaternionMatrix(float *quaternion, float *matrix);
int QuaternionSlerp(float *p, float *q, float t, float *qt);
void QuaternionAngles(float *quaternion, vec3_t angles);

#endif
