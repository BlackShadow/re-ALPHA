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
 // decal.h - Decal system types and prototypes

#ifndef DECAL_H
#define DECAL_H

#define MAX_DECALS 4096

typedef struct decal_s
{
	struct decal_s *pnext; // 0
	struct msurface_s *psurface; // 4
	float dx; // 8 - decal position (normalized S, 0..1)
	float dy; // 12 - decal position (normalized T, 0..1)
	float scale; // 16 - scaling factor (from surface tex axis length)
	short texture; // 20 - decal wad cache index
	short flags; // 22 - FDECAL_* flags
} decal_t;

 // Decal flags
#define FDECAL_PERMANENT	0x01 // Don't remove automatically
#define FDECAL_CUSTOM		0x02 // Custom decal (logo)
#define FDECAL_HITSIGN		0x04 // Hit sign (?)

int R_DecalInit(void);
void R_DecalShoot(int texture, int entityIndex, vec3_t position, int flags);
void R_DrawDecals(void);
void R_DecalRemoveAll(int textureIndex);
int R_PlaceDecal(msurface_t *surf, int texture, float scale, float s, float t);



#endif // DECAL_H
