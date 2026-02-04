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

 // bspfile.h -- BSP file format definitions

#ifndef BSPFILE_H
#define BSPFILE_H

 // =============================================================================
 // BSP file format constants
 // =============================================================================


#define BSPVERSION      29
#define TOOLVERSION     2

#define MAX_MAP_LEAFS   8192 // HL limit
#define MAX_MAP_HULLS	4


 // Lump indices
#define LUMP_ENTITIES       0
#define LUMP_PLANES         1
#define LUMP_TEXTURES       2
#define LUMP_VERTEXES       3
#define LUMP_VISIBILITY     4
#define LUMP_NODES          5
#define LUMP_TEXINFO        6
#define LUMP_FACES          7
#define LUMP_LIGHTING       8
#define LUMP_CLIPNODES      9
#define LUMP_LEAFS          10
#define LUMP_MARKSURFACES   11
#define LUMP_EDGES          12
#define LUMP_SURFEDGES      13
#define LUMP_MODELS         14
#define HEADER_LUMPS        15

 // =============================================================================
 // BSP file structures
 // =============================================================================

 // Lump directory entry
typedef struct
{
    int fileofs;
    int filelen;
} lump_t;

 // BSP header
typedef struct
{
    int version;
    lump_t lumps[HEADER_LUMPS];
} dheader_t;

 // Model (submodel)
typedef struct
{
    float mins[3], maxs[3];
    float origin[3];
    int headnode[4];
    int visleafs;
    int firstface, numfaces;
} dmodel_t;

 // Vertex
typedef struct
{
    float point[3];
} dvertex_t;

 // Plane (disk format)
typedef struct
{
    float normal[3];
    float dist;
    int type;
} dplane_t;


 // Node
typedef struct
{
    int planenum;
    short children[2];
    short mins[3];
    short maxs[3];
    unsigned short firstface;
    unsigned short numfaces;
} dnode_t;

 // Leaf
typedef struct
{
    int contents;
    int visofs;
    short mins[3];
    short maxs[3];
    unsigned short firstmarksurface;
    unsigned short nummarksurfaces;
    byte ambient_level[4];
} dleaf_t;

 // Clipnode
typedef struct
{
    int planenum;
    short children[2];
} dclipnode_t;

 // Texinfo
typedef struct
{
    float vecs[2][4];
    int miptex;
    int flags;
} texinfo_t;

 // Face
typedef struct
{
    short planenum;
    short side;
    int firstedge;
    short numedges;
    short texinfo;
    byte styles[4];
    int lightofs;
} dface_t;

 // Edge
typedef struct
{
    unsigned short v[2];
} dedge_t;

 // =============================================================================
 // Texture structures
 // =============================================================================


#define MIPLEVELS 4

typedef struct
{
    int nummiptex;
    int dataofs[4]; // variable sized
} dmiptexlump_t;

typedef struct miptex_s
{
    char name[16];
    unsigned width, height;
    unsigned offsets[4]; // four mip maps stored
} miptex_t;

 // =============================================================================
 // Ambient sound types
 // =============================================================================


#define AMBIENT_WATER   0
#define AMBIENT_SKY     1
#define AMBIENT_SLIME   2
#define AMBIENT_LAVA    3
#define NUM_AMBIENTS    4

 // =============================================================================
 // Contents types
 // =============================================================================


#define CONTENTS_EMPTY      -1
#define CONTENTS_SOLID      -2
#define CONTENTS_WATER      -3
#define CONTENTS_SLIME      -4
#define CONTENTS_LAVA       -5
#define CONTENTS_SKY        -6
#define CONTENTS_ORIGIN     -7
#define CONTENTS_CLIP       -8
#define CONTENTS_CURRENT_0  -9
#define CONTENTS_CURRENT_90 -10
#define CONTENTS_CURRENT_180 -11
#define CONTENTS_CURRENT_270 -12
#define CONTENTS_CURRENT_UP  -13
#define CONTENTS_CURRENT_DOWN -14
#define CONTENTS_TRANSLUCENT -15

 // =============================================================================

 // =============================================================================
 // Alias Model (MDL) structures
 // =============================================================================

#define IDPOLYHEADER	(('O'<<24)+('P'<<16)+('D'<<8)+'I')
#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I')
#define IDSTUDIOHEADER	(('T'<<24)+('S'<<16)+('D'<<8)+'I')

#define ALIASVERSION	6
#define ALIAS_SINGLE	0
#define ALIAS_GROUP		1

typedef enum { ST_SYNC=0, ST_RAND } synctype_t;

typedef struct
{
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		translate;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;
} mdl_t;

typedef struct
{
	int			onseam;
	int			s;
	int			t;
} stvert_t;

typedef struct
{
	int			facesfront;
	int			vertindex[3];
} dtriangle_t;

typedef struct
{
	byte		v[3];
	byte		lightnormalindex;
} trivertx_t;

typedef struct
{
	trivertx_t	bboxmin;
	trivertx_t	bboxmax;
	char		name[16];
} daliasframe_t;

typedef struct
{
	int			numframes;
	trivertx_t	bboxmin;
	trivertx_t	bboxmax;
} daliasgroup_t;

typedef struct
{
	int			type;
} daliasframetype_t;

typedef struct
{
	float		interval;
} daliasinterval_t;

 // =============================================================================
 // Sprite Model structures
 // =============================================================================

typedef struct
{
	int			ident;
	int			version;
	int			type;
	float		boundingradius;
	int			width;
	int			height;
	int			numframes;
	float		beamlength;
	int			synctype;
} dsprite_t;


typedef struct
{
	int			origin[2];
	int			width;
	int			height;
} dspriteframe_t;

typedef struct
{
	int			numframes;
} dspritegroup_t;

typedef struct
{
	float		interval;
} dspriteinterval_t;

typedef enum { SPR_SINGLE=0, SPR_GROUP } spriteframetype_t;

typedef struct
{
	int			type;
} dspriteframetype_t;

 // =============================================================================

#endif // BSPFILE_H
