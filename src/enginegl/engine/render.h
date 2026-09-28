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

// render.h -- public interface to the refresh module

#ifndef RENDER_H
#define RENDER_H

#define MAXCLIPPLANES	11

#define TOP_RANGE		16			// player uniform colors
#define BOTTOM_RANGE	96

#define VERTEXSIZE		7			// xyz s1t1 s2t2

//=============================================================================

typedef struct mplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<2)
	byte	pad[2];
} mplane_t;

extern mplane_t	frustum[4];

//=============================================================================

typedef enum
{
	pt_static,
	pt_grav,
	pt_slowgrav,
	pt_fire,
	pt_explode,
	pt_explode2,
	pt_blob,
	pt_blob2,
	pt_vox_slowgrav,
	pt_vox_grav
} ptype_t;

typedef struct particle_s
{
	vec3_t				org;
	float				color;
	struct particle_s	*next;
	vec3_t				vel;
	float				ramp;
	float				die;
	ptype_t				type;
} particle_t;

//=============================================================================

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
	int				type;		// SPR_SINGLE or SPR_GROUP
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

//=============================================================================

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;

#define MAX_EFRAGS		640

typedef struct entity_s
{
	qboolean			forcelink;		// model changed
	int					update_type;
	entity_state_t		baseline;		// to fill in defaults in updates
	double				msgtime;		// time of last update
	vec3_t				msg_origins[2];	// last two updates (0 is newest)
	vec3_t				origin;
	vec3_t				msg_angles[2];	// last two updates (0 is newest)
	vec3_t				angles;
	int					rendermode;
	int					renderamt;
	byte				rendercolor[4];
	int					renderfx;
	struct model_s		*model;			// NULL = no model
	struct efrag_s		*efrag;			// linked list of efrags
	float				frame;
	float				syncbase;		// for client-side animations
	byte				*colormap;
	int					effects;		// light, particles, etc
	int					skinnum;		// for Alias models
	int					visframe;		// last frame this entity was found in an active leaf
	int					dlightframe;	// dynamic lighting
	int					dlightbits;
	int					trivial_accept;
	struct mnode_s		*topnode;		// for bmodels, first world node that splits bmodel, or NULL if not split
	int					has_lerpdata;
	float				anim_time;
	float				framerate;
	int					body;
	int					sequence;
	byte				controller[4];
	byte				blending[2];
	byte				pad_ctrl[2];
	float				latched_anim_time;
	vec3_t				lerp_origin;
	vec3_t				lerp_angles;
	float				blend_oldframe;
	float				blend_time;
	int					blend_oldseq;
	byte				latched_controller[4];
	byte				latched_blending[2];
	byte				pad_latched[2];
	float				lightcolor[3];
} entity_t;

// studio model lighting
typedef struct alight_s
{
	int			ambient;
	int			shade;
	float		color[3];
	vec3_t		*plightvec;
} alight_t;

//=============================================================================

// temp entity flags
#define FTENT_SINEWAVE		0x0001
#define FTENT_GRAVITY		0x0002
#define FTENT_ROTATE		0x0004
#define FTENT_SLOWGRAVITY	0x0008
#define FTENT_SMOKETRAIL	0x0010
#define FTENT_COLLIDEWORLD	0x0020
#define FTENT_FLICKER		0x0040

#define TENT_FLICKER_FRAMES	32		// a flickering temp entity lights up once every 32 frames

// entity.baseline.origin is the velocity, entity.msg_angles[0] the angular velocity
typedef struct tempent_s
{
	int					flags;
	float				die;
	struct tempent_s	*next;
	struct tempent_s	*prev;
	entity_t			entity;
} tempent_t;

//=============================================================================

typedef struct
{
	int		x, y, width, height;
} vrect_t;

typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh
	vrect_t		aliasvrect;			// scaled Alias version
	int			vrectright, vrectbottom;
	int			aliasvrectright, aliasvrectbottom;
	float		vrectrightedge;
	float		fvrectx, fvrecty;
	float		fvrectx_adj, fvrecty_adj;
	int			vrect_x_adj_shift20;
	int			vrectright_adj_shift20;
	float		fvrectright_adj, fvrectbottom_adj;
	float		fvrectright;
	float		fvrectbottom;
	float		horizontalFieldOfView;
	float		xOrigin;
	float		yOrigin;
	vec3_t		vieworg;
	vec3_t		viewangles;
	float		fov_x, fov_y;
	int			ambientlight;
} refdef_t;

//
// refresh
//
extern refdef_t			r_refdef;
extern vec3_t			r_origin, vpn, vright, vup;
extern struct texture_s	*r_notexture_mip;
extern qboolean			r_cache_thrash;

// gl_rmain.c
extern entity_t		*currententity;
extern entity_t		viewent_entity;
extern float		modelorg[3];
extern int			r_refdef_vrect_x, r_refdef_vrect_y;
extern int			r_refdef_vrect_width, r_refdef_vrect_height;
extern int			envmap;
extern struct mleaf_s	*viewleaf, *oldviewleaf;
extern int			r_mirror;
extern mplane_t		*r_mirror_plane;
extern int			c_brush_polys, c_alias_polys;
extern byte			*r_colormap;

// gl_rlight.c
extern vec3_t		lightspot;
extern float		v_blend[4];

// gl_rsurf.c
#define MAX_DECAL_SURFS		500

extern struct msurface_s	*decal_list[MAX_DECAL_SURFS];	// surfaces with decals, drawn after the world
extern int			decal_list_count;

// r_efx.c
extern cvar_t		tracerSpeed, tracerOffset, tracerLength;
extern cvar_t		tracerRed, tracerGreen, tracerBlue, tracerAlpha;

// view.c
extern cvar_t		cv_lambert, cv_direct;
extern cvar_t		cv_v_gamma, cv_lightgamma, cv_texgamma, cv_brightness;
extern cvar_t		cv_v_kicktime;
extern int			lightgammatable[1024];
extern float		v_lateral_punch, v_lateral_scale;
extern float		v_vertical_punch, v_vertical_scale;
extern float		v_dmg_time;
extern float		v_punchangle_decay[4];

void R_Init(void);
void R_RenderView(void);		// must set r_refdef first
void R_InitSky(struct texture_s *mt);	// called at level load
void R_NewMap(void);
int  R_CullBox(float *mins, float *maxs);
void R_RotateForEntity(entity_t *ent);
void R_DrawSpriteModel(entity_t *entity);
void R_DrawAliasModel(entity_t *entity);

extern cvar_t crosshair;

void V_RenderView(void);
void V_CalcRefdef(void);
void V_Init(void);
void V_UpdatePalette(void);
void V_StartPitchDrift(void);
void V_StopPitchDrift(void);
void V_ParseDamage(void);
void V_SetContentsColor(int contents);
void HUD_Draw(void);
int  HUD_ViewZoomIncrease(void);
void HUD_ViewZoomDecrease(void);

void SCR_Init(void);
void SCR_BeginLoadingPlaque(void);
void SCR_CenterPrint(char *text);
qboolean SCR_DrawDialog(char *message_ptr);

// gl_rsurf.c
void R_DrawBrushModel(entity_t *e);
void R_DrawWorld(void);
void R_RecursiveWorldNode(struct mnode_s *node);
void R_MarkLeaves(void);
void R_BlendLightmaps(void);
void GL_BuildLightmaps(void);

// gl_refrag.c
void R_AddEfrags(entity_t *ent);
void R_RemoveEfrags(entity_t *ent);

// r_efx.c
void R_StoreEfrags(efrag_t **ppefrag);
void R_PrecacheWeaponSounds(void);

// cl_tent.c
tempent_t *R_AllocTempEntity(vec3_t origin, struct model_s *model);

// r_part.c
void R_InitParticles(void);
void R_ClearParticles(void);
void R_DrawParticles(void);
void R_ReadPointFile_f(void);
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

// r_studio.c
int  R_StudioGetFrameCount(struct model_s *model);
int  R_StudioCheckBBox(void);
void R_StudioEntityLight(entity_t *ent, alight_t *plight);
void R_DrawStudioModel(entity_t *ent, alight_t *plight);
void R_AddToTranslucentList(entity_t *ent);
void R_DrawTransEntitiesOnList(void);

// gl_rlight.c
void R_PushDlights(void);
void R_MarkLights(struct dlight_s *light, int bit, struct mnode_s *node);
unsigned int *R_GetLightmap(unsigned int *lightrgb, vec3_t start, vec3_t end);

#endif // RENDER_H
