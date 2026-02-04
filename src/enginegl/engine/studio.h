/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

Studio models are position independent, so the cache manager can move them.
==============================================================================
*/

#define MAXSTUDIOTRIANGLES	65536
#define MAXSTUDIOVERTS		8192
#define MAXSTUDIOSEQUENCES	256 // total animation sequences
#define MAXSTUDIOSSKINS		100 // total textures
#define MAXSTUDIOSRCBONES	512 // bones allowed at source movement
#define MAXSTUDIOBONES		128 // total bones actually used
#define MAXSTUDIOMODELS		32 // sub-models per model
#define MAXSTUDIOBODYPARTS	32
#define MAXSTUDIOGROUPS		16
#define MAXSTUDIOANIMATIONS	2048
#define MAXSTUDIOMESHES		256
#define MAXSTUDIOEVENTS		1024
#define MAXSTUDIOPIVOTS		256
#define MAXSTUDIOCONTROLLERS 8

typedef struct
{
	int					id;
	int					version;

	char				name[64];
	int					length;

	int					numbones; // 76
	int					boneindex; // 80

	int					numbonecontrollers; // 84 (alpha: studio bone controllers)
	int					bonecontrollerindex; // 88

	int					numseq; // 92
	int					seqindex; // 96

	int					numtextures; // 100
	int					textureindex; // 104

	int					texturedataindex; // 108
	int					numskinref; // 112
	int					numskinfamilies; // 116
	int					skinindex; // 120

	int					numbodyparts; // 124
	int					bodypartindex; // 128

	int					numattachments; // 132
	int					attachmentindex; // 136

	int					soundtable;
	int					soundindex;
	int					soundgroups;
	int					soundgroupindex;

	int					numtransitions;
	int					transitionindex;
} studiohdr_t;

typedef struct
{
	char				name[64];
	int					nummodels;
	int					base;
	int					modelindex; // index into models
} mstudiobodyparts_t;

typedef struct
{
	char				name[64];
	int					flags; // 64
	int					width; // 68
	int					height; // 72
	int					index; // 76
} mstudiotexture_t; // stride 80

typedef struct
{
	int					numtris; // 0
	int					triindex; // 4
	int					skinref; // 8
	int					numnorms; // 12
	int					normindex; // 16
} mstudiomesh_t; // stride 20

typedef struct
{
	int					unused0[4]; // 0..15
	int					vertindex; // 16
	int					unused1; // 20
	int					normindex; // 24
} mstudiomodeldata_t;

typedef struct
{
	char				name[64];
	int					type;
	float				boundingradius;
	int					unused0; // 72
	int					nummesh; // 76
	int					meshindex; // 80
	int					numverts; // 84
	int					vertinfoindex; // 88
	int					unused1; // 92
	int					norminfoindex; // 96
	int					unused2; // 100
	int					modeldataindex; // 104
} mstudiomodel_t; // stride 108

typedef struct
{
	char				label[32]; // 0
	float				fps; // 32
	int					flags; // 36
	int					activity;
	int					actweight;
	int					numframes; // 48
	int					numevents; // 52
	int					eventindex; // 56
	int					unused0; // 60
	int					motiontype; // 64
	int					motionbone; // 68
	int					unused1[5]; // 72...88
	int					animindex; // 92
	int					unused2[2]; // 96, 100
} mstudioseqdesc_t; // stride 104

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
#define STUDIO_XRT		0x1000
#define STUDIO_YRT		0x2000
#define STUDIO_ZRT		0x4000
#define STUDIO_QT		0x8000 // quaternion rather than angle used for orientation
#define STUDIO_RLOOP	0x8000 // bonecontroller wrap-around

#define STUDIO_LOOPING	0x0001

 // bone flags
#define STUDIO_HAS_NORMALS	0x0001
#define STUDIO_HAS_VERTICES 0x0002
#define STUDIO_HAS_BBOX		0x0004
#define STUDIO_HAS_CHROME	0x0008 // if any of the textures have chrome on them

typedef struct 
{
	char				name[32]; // 0
	int					parent; // 32
	int					unused[6]; // 36..59
} mstudiobone_t;

typedef struct 
{
	int					bone; // 0
	int					type; // 4 (STUDIO_* flags, includes STUDIO_RLOOP)
	float				start; // 8
	float				end; // 12
} mstudiobonecontroller_t;

typedef struct 
{
	int					bone;
	int					group; // intersection group
	vec3_t				bbmin; // bounding box
	vec3_t				bbmax;		
} mstudiobbox_t;

#endif // STUDIO_H
