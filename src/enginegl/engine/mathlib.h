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

 // mathlib.h -- math library
 // NOTE: This is a skeleton header derived from reverse engineering.
 // Math types and functions will be filled during translation.

#ifndef MATHLIB_H
#define MATHLIB_H

 // =============================================================================
 // Vector and fixed-point types
 // =============================================================================

 // NOTE: Core vector types to be filled from reverse engineering

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

 // =============================================================================
 // Math constants
 // =============================================================================

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

 // Angle indices for vec3_t angles
#define PITCH   0
#define YAW     1
#define ROLL    2

 // =============================================================================
 // Forward declarations
 // =============================================================================

struct mplane_s;

 // =============================================================================
 // Global variables
 // =============================================================================

 // NOTE: To be filled from reverse engineering

extern vec3_t vec3_origin;
 // extern int nanmask;

 // =============================================================================
 // NaN detection
 // =============================================================================

#define IS_NAN(x) ((x) != (x))

 // =============================================================================
 // Vector macros
 // =============================================================================

 // NOTE: Common vector operation macros
 // To be filled from reverse engineering

#define DotProduct(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorSubtract(a, b, c) do { (c)[0] = (a)[0] - (b)[0]; (c)[1] = (a)[1] - (b)[1]; (c)[2] = (a)[2] - (b)[2]; } while (0)
#define VectorAdd(a, b, c) do { (c)[0] = (a)[0] + (b)[0]; (c)[1] = (a)[1] + (b)[1]; (c)[2] = (a)[2] + (b)[2]; } while (0)
#define VectorCopy(a, b) do { (b)[0] = (a)[0]; (b)[1] = (a)[1]; (b)[2] = (a)[2]; } while (0)
#define VectorScale(a, b, c) do { (c)[0] = (a)[0] * (b); (c)[1] = (a)[1] * (b); (c)[2] = (a)[2] * (b); } while (0)
#define VectorMA(a, b, c, d) do { (d)[0] = (a)[0] + (b) * (c)[0]; (d)[1] = (a)[1] + (b) * (c)[1]; (d)[2] = (a)[2] + (b) * (c)[2]; } while (0)
#define VectorClear(a) do { (a)[0] = 0.0f; (a)[1] = 0.0f; (a)[2] = 0.0f; } while (0)

 // =============================================================================
 // Vector functions
 // =============================================================================

 // NOTE: Vector operation functions to be filled from reverse engineering

 // void VectorMA(vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);
 // vec_t _DotProduct(vec3_t v1, vec3_t v2);
 // void _VectorSubtract(vec3_t veca, vec3_t vecb, vec3_t out);
 // void _VectorAdd(vec3_t veca, vec3_t vecb, vec3_t out);
 // void _VectorCopy(vec3_t in, vec3_t out);
int VectorCompare(vec3_t v1, vec3_t v2);
vec_t Length(vec3_t v);

 // Some translated code uses the Quake name VectorLength.
#define VectorLength Length
void CrossProduct(vec3_t v1, vec3_t v2, vec3_t cross);
float VectorNormalize(vec3_t v); // returns vector length
 // void VectorInverse(vec3_t v);
 // void VectorScale(vec3_t in, vec_t scale, vec3_t out);

 // =============================================================================
 // Matrix/ rotation functions
 // =============================================================================

 // NOTE: Matrix and rotation functions to be filled

 // void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);
void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
float anglemod(float a);

float *AngleQuaternion(float *angles, float *quaternion);
int QuaternionMatrix(float *quaternion, float *matrix);
int QuaternionSlerp(float *p, float *q, float t, float *qt);
void VectorTransform(vec3_t in, float matrix[3][4], vec3_t out);

 // =============================================================================
 // Math utilities
 // =============================================================================

 // NOTE: Utility math functions to be filled

 // int Q_log2(int val);
 // void FloorDivMod(double numer, double denom, int *quotient, int *rem);
 // fixed16_t Invert24To16(fixed16_t val);
 // int GreatestCommonDivisor(int i1, int i2);

 // =============================================================================
 // Plane/ box testing
 // =============================================================================

int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct mplane_s *plane);

 // NOTE: Optimized macro version of BoxOnPlaneSide
 // To be filled from reverse engineering

 // #define BOX_ON_PLANE_SIDE(emins, emaxs, p) \
 // (((p)->type < 3)? \
 // (((p)->dist <= (emins)[(p)->type])? 1 : \
 // (((p)->dist >= (emaxs)[(p)->type])? 2 : 3)) \
 // : BoxOnPlaneSide((emins), (emaxs), (p)))

 // =============================================================================

#endif // MATHLIB_H
