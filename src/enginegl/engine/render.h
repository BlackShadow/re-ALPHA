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

 // render.h -- render interface
 // NOTE: Render structures populated from Quake/ Half-Life sources

#ifndef RENDER_H
#define RENDER_H

 // =============================================================================
 // Basic types (must be defined before structures)
 // =============================================================================

 // Redundant basic types removed (already in common.h or mathlib.h)


 // =============================================================================
 // Constants
 // =============================================================================

#define MAXCLIPPLANES       11
#define TOP_RANGE           16 // player uniform colors
#define BOTTOM_RANGE        96
#define VERTEXSIZE          7

 // =============================================================================
 // Plane structure (mplane_t)
 // =============================================================================

 // Runtime plane with fast culling support
 // Size: 20 bytes (12 + 4 + 1 + 1 + 2 padding)
typedef struct mplane_s
{
	vec3_t	normal; // Plane normal vector
	float	dist; // Distance from origin
	byte	type; // For fast side tests (0=x, 1=y, 2=z, 3+=non-axial)
	byte	signbits; // signx + (signy<<1) + (signz<<2) for fast box tests
	byte	pad[2]; // Alignment padding
} mplane_t;

 // =============================================================================
 // Particle system
 // =============================================================================

 // synctype_t is defined in bspfile.h/ model.h


 // Base particle types (Half-Life extensions defined in glquake.h as macros)
typedef enum
{
	pt_static,
	pt_grav,
	pt_slowgrav,
	pt_fire,
	pt_explode,
	pt_explode2,
	pt_blob,
	pt_blob2
} ptype_t;

typedef struct particle_s
{
	vec3_t				org; // Position
	float				color; // Color index
	struct particle_s	*next; // Next in free list or active list
	vec3_t				vel; // Velocity
	float				ramp; // Ramp index for color animation
	float				die; // Time when particle expires
	ptype_t				type; // Particle behavior type
} particle_t;

 // =============================================================================
 // View frustum
 // =============================================================================

 // Global frustum planes for visibility culling
 // 4 planes: left, right, top, bottom (near/ far handled separately)
extern mplane_t	frustum[4];

 // =============================================================================
 // Sprite model structures
 // =============================================================================

#define SPRITE_VERSION	2

 // spriteframetype_t is defined in bspfile.h


typedef struct
{
	int			width;
	int			height;
	float		up, down, left, right;
	int			gl_texturenum;
} mspriteframe_t;

typedef struct
{
	int				numframes;
	float			*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	int				type; // SPR_SINGLE or SPR_GROUP
	mspriteframe_t	*frameptr;
} mspritegroupframe_t;

typedef struct
{
	int					type;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	int					paloffset;
	float				beamlength;
	int					pad;
	mspritegroupframe_t	frames[1];
} msprite_t;

 // Disk format for sprite header
 // dsprite_t is defined in bspfile.h


 // =============================================================================
 // Entity fragment (efrag) system
 // =============================================================================

 // NOTE: Structure to be filled from reverse engineering
 // Links entities to BSP leaves for visibility

typedef struct efrag_s
{
	struct mleaf_s      *leaf;
	struct efrag_s      *leafnext;
	struct entity_s     *entity;
	struct efrag_s      *entnext;
} efrag_t;

#define MAX_EFRAGS 640

 // =============================================================================
 // Entity structure
 // =============================================================================

 // NOTE: Main entity structure to be filled from reverse engineering
 // This is the client-side representation of entities

typedef struct entity_s
{
	qboolean            forcelink; // model changed
	int                 update_type;
	entity_state_t      baseline; // default values
	double              msgtime; // time of last update
	vec3_t              msg_origins[2]; // last two updates (0 = newest)
	vec3_t              origin;
	vec3_t              msg_angles[2]; // last two updates (0 = newest)
	vec3_t              angles;
	int                 rendermode;
	int                 renderamt;
	byte                rendercolor[4];
	int                 renderfx;
	struct model_s      *model; // NULL = no model
	struct efrag_s      *efrag; // linked efrags
	float               frame;
	float               syncbase; // client animations
	byte                *colormap;
	int                 effects; // light, particles, etc
	int                 skinnum; // Alias models
	int                 visframe; // last visible frame
	int                 dlightframe; // dynamic lighting
	int                 dlightbits;
	int                 trivial_accept;
	struct mnode_s      *topnode; // bmodels: first world node split
	int                 has_lerpdata;
	float               anim_time;
	float               framerate;
	int                 body;
	int                 sequence;
	byte                controller[4];
	byte                blending[2];
	byte                pad_ctrl[2];
	float               latched_anim_time;
	vec3_t              lerp_origin;
	vec3_t              lerp_angles;
	float               blend_oldframe;
	float               blend_time;
	int                 blend_oldseq;
	byte                latched_controller[4];
	byte                latched_blending[2];
	byte                pad_latched[2];
	float               lightcolor[3];
} entity_t;

 // =============================================================================
 // Refresh definition (refdef)
 // =============================================================================

 // NOTE: Main view/ camera structure to be filled from reverse engineering
 // Contains viewport, FOV, view position, etc.

typedef struct
{
	int		x,y,width,height;
} vrect_t;

typedef struct
{
	vrect_t     vrect; // viewport rectangle
	vrect_t     aliasvrect; // scaled alias viewport
	int         vrectright, vrectbottom;
	int         aliasvrectright, aliasvrectbottom;
	float       vrectrightedge;
	float       fvrectx, fvrecty;
	float       fvrectx_adj, fvrecty_adj;
	int         vrect_x_adj_shift20;
	int         vrectright_adj_shift20;
	float       fvrectright_adj, fvrectbottom_adj;
	float       fvrectright;
	float       fvrectbottom;
	float       horizontalFieldOfView;
	float       xOrigin;
	float       yOrigin;
	vec3_t      vieworg;
	vec3_t      viewangles;
	float       fov_x, fov_y;
	int         ambientlight;
} refdef_t;

 // =============================================================================
 // Global variables
 // =============================================================================

 // NOTE: Renderer globals to be filled from reverse engineering

extern int          reinit_surfcache;
extern refdef_t     r_refdef;
extern vec3_t       r_origin, vpn, vright, vup;
extern struct texture_s *r_notexture_mip;
extern qboolean     r_cache_thrash;

 // =============================================================================
 // Renderer initialization and frame functions
 // =============================================================================

 // NOTE: Function declarations to be filled from reverse engineering

void R_Init(void);
void R_InitTextures(void);
void R_InitEfrags(void);
void R_RenderView(void);
void R_ViewChanged(vrect_t *pvrect, int lineadj, float aspect);
void R_InitSky(struct texture_s *mt);
void R_NewMap(void);
int  R_CullBox(float *mins, float *maxs);
void R_RotateForEntity(int entity_ptr);

void V_RenderView(void);
void V_Init(void);
void V_UpdatePalette(void);
void V_StartPitchDrift(void);
void V_StopPitchDrift(void);
void V_ParseDamage(void);
void HUD_Draw(void);
int  HUD_ViewZoomIncrease(void);
void HUD_ViewZoomDecrease(void);

void SCR_Init(void);
void SCR_BeginLoadingPlaque(void);
void SCR_CenterPrint(char *text);
qboolean SCR_DrawDialog(char *message_ptr);

 // =============================================================================
 // Entity fragment management
 // =============================================================================

 // NOTE: Efrag functions to be filled

void R_AddEfrags(entity_t *ent);
void R_RemoveEfrags(entity_t *ent);
void R_StoreEfrags(efrag_t **efrag_list);

 // =============================================================================
 // Particle system
 // =============================================================================

 // NOTE: Particle functions to be filled from r_part.c

void R_ParseParticleEffect(void);
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail(vec3_t start, vec3_t end, int type);
void R_EntityParticles(entity_t *ent);
void R_BlobExplosion(vec3_t org);
void R_ParticleExplosion(vec3_t org);
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength);
void R_LavaSplash(vec3_t org);
void R_TeleportSplash(vec3_t org);
void R_DarkFieldParticles(vec3_t org);
void R_DarkFieldParticles2(vec3_t org);
void R_BeamParticles(vec3_t start, vec3_t end);
void R_SparkStreaks(vec3_t pos, vec3_t dir, int color, int speed);
void R_StreakSplash(vec3_t pos, vec3_t dir, int color, int speed);
void R_SparkShower(vec3_t org);
void R_ParticleStatic(vec3_t *pos, vec3_t *vel, float die);
int  R_StudioGetFrameCount(struct model_s *model);

 // =============================================================================
 // Dynamic lights
 // =============================================================================

 // NOTE: Dynamic light functions to be filled

void R_PushDlights(void);
void R_MarkLights(struct dlight_s *light, int bit, struct mnode_s *node);

 // =============================================================================
 // Surface cache (software renderer)
 // =============================================================================

 // NOTE: Surface cache functions (may not be used in GL renderer)
 // To be verified during reverse engineering

int  D_SurfaceCacheForRes(int width, int height);
void D_FlushCaches(void);
void D_DeleteSurfaceCache(void);
void D_InitCaches(void *buffer, int size);

 // =============================================================================
 // View rectangle
 // =============================================================================

 // NOTE: View rectangle function to be filled

void R_SetVrect(vrect_t *pvrect, vrect_t *pvrectin, int lineadj);

 // =============================================================================

#endif // RENDER_H
