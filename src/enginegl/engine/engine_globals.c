/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
****/

#include "quakedef.h"

quakeparms_t host_parms;
int			host_initialized;
double		realtime;
double		oldrealtime;
double		host_frametime;
double		host_time;
int			host_framecount;
int			host_hunklevel;
int			host_in_intermission;
server_client_t *host_client;
int			standard_quake;
int			minimum_memory;

server_t	sv;
server_static_t svs;
int			sv_max_edicts;
edict_t		*sv_edicts;
int			*sv_num_edicts = &sv.num_edicts;
int         sv_edicts_base;
int         sv_edicts_active;
char		sv_decalnames[255][16];
int			sv_decalnamecount;
edict_t		*sv_player;

void		*progs;
void		*pr_functions;
void		*pr_globaldefs;
void		*pr_fielddefs;
void		*pr_statements;
globalvars_t *pr_global_struct;
float		*pr_globals;
char		*pr_strings;
int			pr_edict_size;

int			current_skill;

client_static_t	cls;
char			cls_spawnparms[1024];
client_t		cl;
entity_t		cl_entities[MAX_EDICTS];
int				cl_num_entities;
entity_t			cl_static_entities[128];
int					cl_num_statics;
efrag_t			cl_efrags[MAX_EFRAGS];
efrag_t			*cl_free_efrags;
dlight_t		cl_dlights[MAX_DLIGHTS];
char			cl_lightstyle_value[MAX_LIGHTSTYLES][64];
lightstyle_t		cl_lightstyle[MAX_LIGHTSTYLES];
double		cl_time;
double		cl_mtime[2];
double		cl_oldtime;
struct model_s *cl_model_precache[256];
sfx_t		*cl_sound_precache[256];
struct model_s *cl_worldmodel;
void		*cl_scores;
vec3_t		cl_punchangle;
vec3_t		cl_punchangle_old;
vec3_t		cl_velocity;
float		cl_viewheight;
float		cl_idealpitch;
int			cl_lightlevel;
int			cl_onground;
int			cl_inwater;
int			cl_waterlevel;
int			cl_intermission;
int			cl_intermission_state;
int			cl_dead;
int			cl_smoothstairs;
float		cl_oldz;
float		cl_waterwarp;
float		cl_yawoffset;
float		cl_fov;
float		cl_forwardmove;
float		cl_forward_velocity;
float		cl_viewangles_pitch;
int			cl_viewent_sequence;
int			cl_viewent_valid;
float		cl_viewent_animtime;
int			cl_viewent_light;
int			cl_slotmask;
int			cl_spectator;
int			cl_targetentity;
int			cl_maxspectators;
int			cl_autoaim;
double		cl_completed_time;

float		old_gamma;
float		old_lightgamma;
float		old_brightness;
int			vid_gamma_changed;
float		cl_viewangles[3];
float		cl_origin[3];
model_t		*cl_viewmodel;
int			cl_numvisedicts;
cl_entity_t	*cl_visedicts[MAX_VISEDICTS];

int			cl_playernum;
int			cl_viewentity;
int			cl_maxclients;
int			cl_gametype;
char		cl_levelname[40];

int			cl_items;
int			cl_health;
int			cl_armorvalue;
int			cl_weaponmodel;
int			cl_weaponframe;
int			cl_activeweapon;
int			cl_weaponbits[32];
int			cl_stats[32];
int			cl_cdtrack;
int			cl_looptrack;

void		(*vid_menudrawfn)(void);
int			(*vid_menukeyfn)(int key);
int			vid_fullscreen;

qboolean	serialAvailable;
qboolean	ipxAvailable;
qboolean	tcpipAvailable;
unsigned short	hostshort;

int			hostCacheCount;
hostcache_t	hostcache[HOSTCACHESIZE];
int			net_activeconnections;
sizebuf_t	net_message;

float		r_ambient;
float		r_lightscale;
float		r_plightvec[3];
float		r_origin[3];
float		vpn[3], vright[3], vup[3];
refdef_t	r_refdef;
float		r_fov;
float		default_fov;
int			r_refdef_flags;
float		r_refdef_weaponalpha;
int			ztrick_frame;
edict_t		*r_refdef_onlyents;
qboolean	r_cache_thrash;
int			r_visframecount;
float		r_fullbright_value;
int			c_lightmaps;
int			r_dowarp;
float		r_blend_alpha = 1.0f;
float		r_entorigin[3];
float		r_modeldist[3];
float		r_ambientlight;
float		r_shadelight;
float		*r_lightvec;
int			r_posenum;
mplane_t	*mirror_plane;
float		r_world_matrix[16];
vec3_t		r_emins;
vec3_t		r_emaxs;
mnode_t		*r_pefragtopnode;
efrag_t		**r_lastlink;
entity_t	*r_addent;
int			r_interpolate_frames;
char		input_flags;

cvar_t		gl_clear = { "gl_clear", "0" };
cvar_t		gl_cull = { "gl_cull", "1" };
cvar_t		gl_texsort = { "gl_texsort", "1" };
cvar_t		gl_smoothmodels = { "gl_smoothmodels", "1" };
cvar_t		gl_affinemodels = { "gl_affinemodels", "0" };
cvar_t		gl_polyblend = { "gl_polyblend", "1" };
cvar_t		gl_keeptjunctions = { "gl_keeptjunctions", "1" };
cvar_t		gl_playermip = { "gl_playermip", "0" };
cvar_t		gl_nocolors = { "gl_nocolors", "0" };
cvar_t		gl_reporttjunctions = { "gl_reporttjunctions", "0" };
cvar_t		gl_wateramp = { "gl_wateramp", "0.3" };

void		*hunk_base;
byte		*host_basepal;
byte		*host_colormap;

int			g_iextdllcount;
void		*g_rgextdll[50];
void		*g_rgextinit[50];
int			g_deltaHullCacheChecksum;
int			g_engineHandleTable[9];
globalvars_t	*gpGlobals;

int			cl_stats_monsters;
int			cl_stats_totalmonsters;

qboolean	is_dedicated;

int			vid_suppressModeChangePrint;

edict_t		*g_pmove;
vec3_t		g_forward;
vec3_t		g_right;
vec3_t		g_up;
float		g_forwardmove;
float		g_sidemove;
float		g_upmove;
int			g_onground;
vec3_t		g_wishvel;
float		g_wishspeed;
vec3_t		*g_velocity;
vec3_t		*g_origin;
void		*g_physents;
