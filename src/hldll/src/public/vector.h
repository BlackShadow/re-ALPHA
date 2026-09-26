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
#ifndef VECTOR_H
#define VECTOR_H

#include <math.h>

//=========================================================
// 3D Vector - same data layout as the engine's vec3_t,
// which is a float[3].
//=========================================================
class Vector
{
public:
	// Construction
	Vector() = default;
	Vector(const Vector& v) = default;
	Vector& operator=(const Vector& v) = default;
	constexpr Vector(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
	explicit Vector(const float* rgfl) : x(rgfl[0]), y(rgfl[1]), z(rgfl[2]) {}

	// Operators
	Vector operator-() const { return Vector(-x, -y, -z); }
	bool operator==(const Vector& v) const { return x == v.x && y == v.y && z == v.z; }
	bool operator!=(const Vector& v) const { return !(*this == v); }
	Vector operator+(const Vector& v) const { return Vector(x + v.x, y + v.y, z + v.z); }
	Vector operator-(const Vector& v) const { return Vector(x - v.x, y - v.y, z - v.z); }
	Vector operator*(float fl) const { return Vector(x * fl, y * fl, z * fl); }
	Vector operator/(float fl) const { return Vector(x / fl, y / fl, z / fl); }

	Vector& operator+=(const Vector& v) { x += v.x; y += v.y; z += v.z; return *this; }
	Vector& operator-=(const Vector& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	Vector& operator*=(float fl) { x *= fl; y *= fl; z *= fl; return *this; }

	// Vectors automatically convert to float * when handed to the engine
	operator float*() { return &x; }
	operator const float*() const { return &x; }

	// Methods
	void CopyToArray(float* rgfl) const { rgfl[0] = x; rgfl[1] = y; rgfl[2] = z; }
	float Length() const { return sqrtf(x * x + y * y + z * z); }
	float Length2D() const { return sqrtf(x * x + y * y); }

	// Returns the unit vector, or the zero vector when this vector has no length.
	Vector Normalize() const
	{
		float flLen = Length();
		if (flLen == 0.0f)
			return Vector(0.0f, 0.0f, 0.0f);

		flLen = 1.0f / flLen;
		return Vector(x * flLen, y * flLen, z * flLen);
	}

	// Members
	float x, y, z;
};

inline Vector operator*(float fl, const Vector& v) { return v * fl; }
inline float DotProduct(const Vector& a, const Vector& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vector CrossProduct(const Vector& a, const Vector& b) { return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

#endif // VECTOR_H
