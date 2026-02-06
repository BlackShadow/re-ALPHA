/*
==============================================================

    Minimal mathlib for MDLDec (Half-Life Alpha)

==============================================================
*/

#include "mathlib.h"

float VectorNormalize(vec3_t v)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 0.0f)
    {
        float inv = 1.0f / length;
        v[0] *= inv;
        v[1] *= inv;
        v[2] *= inv;
    }
    return length;
}

void VectorTransform(vec3_t in, float matrix[3][4], vec3_t out)
{
    out[0] = matrix[0][0] * in[0] + matrix[0][1] * in[1] + matrix[0][2] * in[2] + matrix[0][3];
    out[1] = matrix[1][0] * in[0] + matrix[1][1] * in[1] + matrix[1][2] * in[2] + matrix[1][3];
    out[2] = matrix[2][0] * in[0] + matrix[2][1] * in[1] + matrix[2][2] * in[2] + matrix[2][3];
}

void VectorRotate(vec3_t in, float matrix[3][4], vec3_t out)
{
    out[0] = matrix[0][0] * in[0] + matrix[0][1] * in[1] + matrix[0][2] * in[2];
    out[1] = matrix[1][0] * in[0] + matrix[1][1] * in[1] + matrix[1][2] * in[2];
    out[2] = matrix[2][0] * in[0] + matrix[2][1] * in[1] + matrix[2][2] * in[2];
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

// Half-Life Alpha mathlib uses degree inputs for angles.
float *AngleQuaternion(float *angles, float *quaternion)
{
    const float deg2rad = 0.017453292519943295f;

    float angle = angles[1] * deg2rad;
    double halfAngle = angle * 0.5;
    float sy = (float)sin(halfAngle);
    float cy = (float)cos(halfAngle);

    angle = angles[0] * deg2rad;
    halfAngle = angle * 0.5;
    float sp = (float)sin(halfAngle);
    float cp = (float)cos(halfAngle);

    angle = angles[2] * deg2rad;
    halfAngle = angle * 0.5;
    float sr = (float)sin(halfAngle);
    float cr = (float)cos(halfAngle);

    quaternion[0] = sr * cp * cy - cr * sp * sy;
    quaternion[1] = cr * sp * cy + sr * cp * sy;
    quaternion[2] = cr * cp * sy - sr * sp * cy;
    quaternion[3] = cr * cp * cy + sr * sp * sy;

    return quaternion;
}

int QuaternionMatrix(float *quaternion, float *matrix)
{
    float x = quaternion[0];
    float y = quaternion[1];
    float z = quaternion[2];
    float w = quaternion[3];

    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

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
    float q2[4] = {q[0], q[1], q[2], q[3]};
    float cosom = p[0] * q2[0] + p[1] * q2[1] + p[2] * q2[2] + p[3] * q2[3];

    if (cosom < 0.0f)
    {
        cosom = -cosom;
        q2[0] = -q2[0];
        q2[1] = -q2[1];
        q2[2] = -q2[2];
        q2[3] = -q2[3];
    }

    float sclp;
    float sclq;
    if ((1.0f - cosom) > 1e-6f)
    {
        float omega = acosf(cosom);
        float sinom = sinf(omega);
        sclp = sinf((1.0f - t) * omega) / sinom;
        sclq = sinf(t * omega) / sinom;
    }
    else
    {
        sclp = 1.0f - t;
        sclq = t;
    }

    qt[0] = sclp * p[0] + sclq * q2[0];
    qt[1] = sclp * p[1] + sclq * q2[1];
    qt[2] = sclp * p[2] + sclq * q2[2];
    qt[3] = sclp * p[3] + sclq * q2[3];
    return 4;
}

void QuaternionAngles(float *quaternion, vec3_t angles)
{
    float x = quaternion[0];
    float y = quaternion[1];
    float z = quaternion[2];
    float w = quaternion[3];

    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    float roll = atan2f(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (w * y - z * x);
    float pitch;
    if (fabsf(sinp) >= 1.0f)
        pitch = copysignf((float)M_PI / 2.0f, sinp);
    else
        pitch = asinf(sinp);

    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    float yaw = atan2f(siny_cosp, cosy_cosp);

    const float rad2deg = 57.295779513082320876f;
    angles[0] = pitch * rad2deg;
    angles[1] = yaw * rad2deg;
    angles[2] = roll * rad2deg;
}
