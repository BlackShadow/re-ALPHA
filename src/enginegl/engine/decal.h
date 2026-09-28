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
// decal.h -- decals on brush surfaces

#ifndef DECAL_H
#define DECAL_H

#define MAX_DECALS			4096

// decal_t flags
#define FDECAL_PERMANENT	0x01	// never reused for a new decal

typedef struct decal_s
{
	struct decal_s		*pnext;		// next decal on the surface
	struct msurface_s	*psurface;	// surface the decal is on
	float				dx;			// position in surface texture space (0..1)
	float				dy;
	float				scale;		// 1 / length of the surface texture axis
	short				texture;	// decal texture index
	short				flags;
} decal_t;

void R_DecalInit(void);
void R_DecalShoot(int texture, int entityIndex, vec3_t position, int flags);
void R_DrawDecals(void);
void R_PlaceDecal(msurface_t *surf, int texture, float scale, float s, float t);

#endif // DECAL_H
