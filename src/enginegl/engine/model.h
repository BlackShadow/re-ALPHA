
#ifndef MODEL_H
#define MODEL_H

#include "bspfile.h"

 // Forward definition
struct decal_s;
struct msurface_s;

 // =============================================================================
 // Entity effect flags (trail/ rotate etc)
 // =============================================================================

#define EF_ROCKET   1 // leave a trail
#define EF_GRENADE  2 // leave a trail
#define EF_GIB      4 // leave a trail
#define EF_ROTATE   8 // rotate (bonus items)
#define EF_TRACER   16 // green split trail
#define EF_ZOMGIB   32 // small blood trail
#define EF_TRACER2  64 // orange split trail + rotate
#define EF_TRACER3  128 // purple trail

 // =============================================================================
 // Runtime model structures
 // =============================================================================

#define MAX_MIPS    4

typedef struct
{
	int			width, height;
	int			mips[MAX_MIPS][2]; // offsets
	char		name[16];
} miptex_runtime_t; // Miptex in memory (slightly unrelated to disk format)

typedef struct texture_s
{
	char		name[16];
	unsigned	width, height;
	int			gl_texturenum;
	struct msurface_s *texturechain;
	int			anim_total;
	int			anim_min, anim_max;
	struct texture_s *anim_next;
	struct texture_s *alternate_anims;
	unsigned	offsets[MIPLEVELS];
	byte		*palette;
} texture_t;

texture_t *R_InitNoTexture(void);

typedef struct
{
	float		vecs[2][4];
	float		mipadjust;
	texture_t	*texture;
	int			flags;
} mtexinfo_t;

#define TEX_SPECIAL	1

 // Plane types (mplane_t::type)
#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2

#include "render.h"

typedef struct hull_s
{
	dclipnode_t *clipnodes;
	mplane_t    *planes;
	int         firstclipnode;
	int         lastclipnode;
	vec3_t      clip_mins;
	vec3_t      clip_maxs;
} hull_t;

typedef struct mvertex_s
{
	vec3_t	position;
} mvertex_t;

typedef struct medge_s
{
	unsigned short v[2];
	unsigned int cachededgeoffset;
} medge_t;

typedef struct glpoly_s
{
	struct glpoly_s	*next;
	struct glpoly_s	*chain;
	int				numverts;
	int				flags; // for SURF_UNDERWATER
	float			verts[4][VERTEXSIZE]; // variable sized (xyz s1 t1 s2 t2)
} glpoly_t;

typedef struct mnode_s
{
 // common with leaf
	int			contents; // 0, to differentiate from leafs
	int			visframe; // node needs to be traversed if current
	
	float		minmaxs[6]; // mins[3], maxs[3]

	struct mnode_s	*parent;

 // node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];	
	
	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
 // common with node
	int			contents; // -1, to differentiate from nodes
	int			visframe; // node needs to be traversed if current

	float		minmaxs[6]; // mins[3], maxs[3]

	struct mnode_s	*parent;

 // leaf specific
	byte		*compressed_vis;
	struct efrag_s	*efrags;

	struct msurface_s **firstmarksurface;
	int			nummarksurfaces;
	int			key; // BSP sequence number for leaf
	byte		ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct msurface_s
{
	int			visframe; // should be drawn when node is crossed

	mplane_t	*plane;
	int			flags;

	int			firstedge; // look up in model->surfedges[], negative = backwards
	int			numedges;
	
	short		texturemins[2];
	short		extents[2];

	int			light_s, light_t; // gl lightmap coordinates

	glpoly_t	*polys; // multiple if warped
	struct msurface_s	*texturechain;

	mtexinfo_t	*texinfo;

 // lighting info
	int			dlightframe;
	int			dlightbits;

	int			lightmaptexturenum;
	byte		styles[MAXLIGHTMAPS];
	int			cached_light[MAXLIGHTMAPS]; // values currently used in lightmap
	int			cached_dlight; // dynamic lighting currently used involved

	byte		*samples; // [numstyles*surfsize]
	struct decal_s *pdecals; // Decals on this surface
} msurface_t;

typedef enum {
	mod_brush,
	mod_sprite,
	mod_alias,
	mod_studio
} modtype_t;

typedef struct model_s
{
	char		name[MAX_QPATH];
	int			needload; // bmodels and sprites don't load instantly

	modtype_t	type;
	int			numframes;
	synctype_t	synctype;
	
	int			flags;

 // Volume occupied by the model graphics
	vec3_t		mins, maxs;
	float		radius;

	int			pad0[7];

 // Brush model data
	int			firstmodelsurface;
	int			nummodelsurfaces;

	int			numsubmodels;
	dmodel_t	*submodels;

	int			numplanes;
	mplane_t	*planes;

	int			numleafs;
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			numclipnodes;
	dclipnode_t	*clipnodes;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	int			numtextures;
	texture_t	**textures;

	byte		*visdata;
	byte		*lightdata;
	char		*entities;

	cache_user_t cache;
} model_t;

 // Flags for msurface_t
#define SURF_PLANEBACK		2
#define SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80
#define SURF_NOTHING		0x80

#define MAXLIGHTMAPS	4

 // =============================================================================
 // Alias Model runtime structures
 // =============================================================================

typedef struct
{
	int			facesfront;
	int			vertindex[3];
} mtriangle_t;

typedef struct
{
	int			firstpose;
	int			numposes;
	float		interval;
	trivertx_t	bboxmin;
	trivertx_t	bboxmax;
	int			pad;
	char		name[16];
} maliasframedesc_t;

typedef struct aliashdr_s
{
	int				ident;
	int				version;
	vec3_t			scale;
	vec3_t			translate;
	float			boundingradius;
	vec3_t			eyeposition;
	int				numskins;
	int				skinwidth;
	int				skinheight;
	int				numverts;
	int				numtris;
	int				numframes;
	synctype_t		synctype;
	int				flags;
	float			size;
	int				numposes;
	int				poseverts;
	int				posedata;
	int				commands;
	int				gl_texturenum[32];
	maliasframedesc_t frames[1]; // variable sized
} aliashdr_t;

 // Sprite structures moved to render.h

 // =============================================================================
 // Function Prototypes
 // =============================================================================

void		Mod_Init(void);
void		Mod_ClearAll(void);
int			Mod_GetType(int type);
model_t		*Mod_ForName(char *name, qboolean crash);
void		*Mod_Extradata(model_t *mod);
void		Mod_Print(void);
void		Mod_TouchModel(char *name);
model_t		*Mod_FindName(char *name);
model_t		*Mod_LoadModel(model_t *mod, qboolean crash);

struct mleaf_s *Mod_PointInLeaf(float *p, model_t *model);
byte		*Mod_LeafPVS(struct mleaf_s *leaf, model_t *model);
byte		*Mod_DecompressVis(byte *in, model_t *model);

#endif // MODEL_H
