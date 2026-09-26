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
#ifndef STUDIO_H
#define STUDIO_H

// Studio model data, as far as the game DLL reads it (GET_MODEL_PTR).

// bone controller types
#define STUDIO_XR		0x0008
#define STUDIO_YR		0x0010
#define STUDIO_ZR		0x0020

typedef struct
{
	int		id;
	int		version;
	char	name[64];
	int		length;

	int		numbones;
	int		boneindex;

	int		numbonecontrollers;		// bone controllers
	int		bonecontrollerindex;

	int		numseq;					// animation sequences
	int		seqindex;
} studiohdr_t;

// bone controllers
typedef struct
{
	int		bone;
	int		type;					// STUDIO_XR etc.
	float	start;
	float	end;
} mstudiobonecontroller_t;

// sequence descriptions
typedef struct
{
	char	label[32];
	float	fps;					// frames per second
	int		flags;
	int		numevents;
	int		eventindex;
	int		numframes;				// number of frames per sequence
	int		unused1[6];
	Vector	linearmovement;			// movement of the root bone over the whole sequence
	int		unused2[4];
} mstudioseqdesc_t;

// animation events
typedef struct
{
	short			frame;
	unsigned char	type;			// bit number in the GetAnimationEventFlags() mask
	unsigned char	unused;
} mstudioevent_t;

#endif // STUDIO_H
