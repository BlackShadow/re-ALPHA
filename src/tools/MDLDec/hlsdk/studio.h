/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
****/
#ifndef STUDIO_H
#define STUDIO_H

/*
==============================================================================

STUDIO MODELS

Half-Life Alpha studio model format (version 6), as the alpha engine
(r_studio.c) and game DLL read it. StudioMDL and MDLDec share this file.

==============================================================================
*/

#define STUDIO_VERSION		6

#define IDSTUDIOHEADER		(('T' << 24) + ('S' << 16) + ('D' << 8) + 'I')	// little-endian "IDST"
#define IDSTUDIOSEQHEADER	(('Q' << 24) + ('S' << 16) + ('D' << 8) + 'I')	// little-endian "IDSQ"

#define MAXSTUDIOTRIANGLES	65536
#define MAXSTUDIOVERTS		8192
#define MAXSTUDIOSEQUENCES	256		// total animation sequences
#define MAXSTUDIOSKINS		100		// total textures
#define MAXSTUDIOSRCBONES	512		// bones allowed at source movement
#define MAXSTUDIOBONES		128		// total bones actually used
#define MAXSTUDIOMODELS		32		// sub-models per model
#define MAXSTUDIOBODYPARTS	32
#define MAXSTUDIOGROUPS		16
#define MAXSTUDIOANIMATIONS	2048
#define MAXSTUDIOMESHES		256
#define MAXSTUDIOEVENTS		1024
#define MAXSTUDIOPIVOTS		256
#define MAXSTUDIOCONTROLLERS 8

typedef struct
{
	int		id;
	int		version;

	char	name[64];
	int		length;

	int		numbones;				// bones
	int		boneindex;

	int		numbonecontrollers;		// bone controllers
	int		bonecontrollerindex;

	int		numseq;					// animation sequences
	int		seqindex;

	int		numtextures;			// raw textures
	int		textureindex;
	int		texturedataindex;

	int		numskinref;				// replaceable textures
	int		numskinfamilies;
	int		skinindex;

	int		numbodyparts;
	int		bodypartindex;

	int		numattachments;			// attachable points
	int		attachmentindex;

	int		soundtable;
	int		soundindex;
	int		soundgroups;
	int		soundgroupindex;

	int		numtransitions;			// animation node to animation node transition graph
	int		transitionindex;
} studiohdr_t;

// bones
typedef struct
{
	char	name[32];				// bone name for symbolic links
	int		parent;					// parent bone
	int		unused[6];
} mstudiobone_t;

// bone controllers
typedef struct
{
	int		bone;					// -1 == 0
	int		type;					// X, Y, Z, XR, YR, ZR, RLOOP
	float	start;
	float	end;
} mstudiobonecontroller_t;

// attachments
typedef struct
{
	int		bone;
	vec3_t	org;					// attachment point
} mstudioattachment_t;

// sequence descriptions
typedef struct
{
	char	label[32];				// sequence label
	float	fps;					// frames per second
	int		flags;					// looping/non-looping flags

	int		numevents;				// mstudioevent_t
	int		eventindex;

	int		numframes;				// number of frames per sequence

	int		numfullevents;			// mstudiofullevent_t, only StudioMDL and MDLDec use them
	int		fulleventindex;

	int		animindex2;				// StudioMDL repeats animindex here, the engine doesn't read it

	int		motiontype;
	int		motionbone;
	int		unused1;
	vec3_t	linearmovement;			// movement of the motion bone over the whole sequence

	int		numblends;				// always 1, the engine plays the first blend only
	int		animindex;				// mstudioboneanim_t for each bone
	int		unused2[2];
} mstudioseqdesc_t;

// events
typedef struct
{
	short			frame;
	unsigned char	event;
	unsigned char	type;
} mstudioevent_t;

// the same events with their options, for decompiling
typedef struct
{
	int		frame;
	int		event;
	int		type;
	char	options[64];
} mstudiofullevent_t;

// key frames of one bone, at animindex
typedef struct
{
	int		numposkeys;				// mstudioposkey_t
	int		poskeyindex;
	int		numrotkeys;				// mstudiorotkey_t
	int		rotkeyindex;
} mstudioboneanim_t;

typedef struct
{
	short	frame;
	short	unused;
	vec3_t	pos;
} mstudioposkey_t;

typedef struct
{
	short	frame;
	short	roll;					// angles in 1 / ROTKEY_SCALE degrees
	short	pitch;
	short	yaw;
} mstudiorotkey_t;

#define ROTKEY_SCALE		100.0f

// StudioMDL's animation compression, not stored in the alpha format
typedef union
{
	struct
	{
		byte	valid;
		byte	total;
	} num;
	short		value;
} mstudioanimvalue_t;

// body part index
typedef struct
{
	char	name[64];
	int		nummodels;
	int		base;
	int		modelindex;				// index into models array
} mstudiobodyparts_t;

// skin info
typedef struct
{
	char	name[64];
	int		flags;
	int		width;
	int		height;
	int		index;
} mstudiotexture_t;

// studio models
typedef struct
{
	char	name[64];

	int		type;

	float	boundingradius;

	int		unused1;
	int		nummesh;
	int		meshindex;

	int		numverts;				// number of unique vertices
	int		vertinfoindex;			// vertex bone info
	int		unused2;
	int		norminfoindex;			// normal bone info
	int		unused3;

	int		modeldataindex;			// mstudiomodeldata_t
} mstudiomodel_t;

typedef struct
{
	int		unused1[4];
	int		vertindex;				// vertex vec3_t
	int		unused2;
	int		normindex;				// normal vec3_t
} mstudiomodeldata_t;

// meshes
typedef struct
{
	int		numtris;				// three mstudiotrivert_t each
	int		triindex;
	int		skinref;
	int		numnorms;				// per mesh normals
	int		normindex;				// normal vec3_t
} mstudiomesh_t;

typedef struct
{
	short	vertindex;
	short	normindex;
	short	s, t;
} mstudiotrivert_t;

// lighting options
#define STUDIO_NF_FLATSHADE		0x0001
#define STUDIO_NF_CHROME		0x0002
#define STUDIO_NF_FULLBRIGHT	0x0004
#define STUDIO_NF_NOMIPS		0x0008
#define STUDIO_NF_ALPHA			0x0010
#define STUDIO_NF_ADDITIVE		0x0020
#define STUDIO_NF_MASKED		0x0040

// motion flags
#define STUDIO_X		0x0001
#define STUDIO_Y		0x0002
#define STUDIO_Z		0x0004
#define STUDIO_XR		0x0008
#define STUDIO_YR		0x0010
#define STUDIO_ZR		0x0020
#define STUDIO_LX		0x0040
#define STUDIO_LY		0x0080
#define STUDIO_LZ		0x0100
#define STUDIO_AX		0x0200
#define STUDIO_AY		0x0400
#define STUDIO_AZ		0x0800
#define STUDIO_AXR		0x1000
#define STUDIO_AYR		0x2000
#define STUDIO_AZR		0x4000
#define STUDIO_TYPES	0x7FFF
#define STUDIO_RLOOP	0x8000	// controller that wraps shortest distance

// sequence flags
#define STUDIO_LOOPING	0x0001

// model flags
#define STUDIO_HAS_NORMALS	0x0001
#define STUDIO_HAS_VERTICES	0x0002
#define STUDIO_HAS_BBOX		0x0004
#define STUDIO_HAS_CHROME	0x0008	// if any of the textures have chrome on them

#endif // STUDIO_H
