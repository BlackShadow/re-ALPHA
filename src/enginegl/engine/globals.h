/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
****/

// globals.h -- engine state shared by the host, server, client and renderer
// (most of them are defined in engine_globals.c)

#ifndef GLOBALS_H
#define GLOBALS_H

struct server_s;
struct server_static_s;
struct client_s;
struct client_static_s;
struct edict_s;
struct entity_s;

//
// host
//
extern quakeparms_t host_parms;
extern int			host_initialized;
extern double		realtime;
extern double		oldrealtime;
extern double		host_frametime;
extern double		host_time;
extern int			host_framecount;
extern int			host_hunklevel;
extern int			host_in_intermission;
extern struct server_client_s	*host_client;

//
// server
//
extern struct server_s			sv;
extern struct server_static_s	svs;
extern int			sv_max_edicts;
extern edict_t		*sv_edicts;
extern int			*sv_num_edicts;
extern int			sv_edicts_base;
extern int			sv_edicts_active;
extern char			sv_decalnames[255][16];
extern int			sv_decalnamecount;
extern edict_t		*sv_player;
extern void			*g_physents;

//
// progs
//
extern void			*progs;
extern void			*pr_functions;
extern void			*pr_globaldefs;
extern void			*pr_fielddefs;
extern void			*pr_statements;
extern globalvars_t	*pr_global_struct;
extern float		*pr_globals;
extern char			*pr_strings;
extern int			pr_edict_size;

//
// game rules
//
extern qboolean		noclip_anglehack;
extern int			current_skill;

//
// client
//
extern struct client_static_s	cls;
extern struct client_s			cl;
extern char						cls_spawnparms[1024];

extern entity_t		cl_entities[MAX_EDICTS];
extern entity_t		cl_static_entities[128];
extern int			cl_num_entities;
extern efrag_t		cl_efrags[MAX_EFRAGS];
extern efrag_t		*cl_free_efrags;
extern dlight_t		cl_dlights[MAX_DLIGHTS];
extern char			cl_lightstyle_value[MAX_LIGHTSTYLES][64];

extern int			cl_num_statics;
extern double		cl_mtime[2];
extern double		cl_time;
extern double		cl_oldtime;
extern struct model_s *cl_model_precache[256];
extern sfx_t		*cl_sound_precache[256];
extern struct model_s *cl_worldmodel;
extern lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
extern scoreboard_t	*cl_scores;	// [cl_maxclients]
extern vec3_t		cl_punchangle;
extern vec3_t		cl_punchangle_old;
extern vec3_t		cl_velocity;
extern float		cl_viewheight;
extern float		cl_idealpitch;
extern int			cl_onground;
extern int			cl_inwater;
extern int			cl_waterlevel;	// 0 - 3
extern int			cl_intermission;
extern int			cl_paused;	// svc_setpause
extern double		cl_completed_time;
extern int			cl_lightlevel;	// sent in the usercmd
extern float		cl_oldz;	// for smoothing stair steps
extern int			cl_dead;
extern float		cl_forwardmove;
extern float		cl_forward_velocity;
extern float		cl_viewangles_pitch;
extern float		cl_waterwarp;
extern float		cl_yawoffset;
extern float		r_fov;

// gamma (V_BuildGammaTables)
extern float		old_gamma;
extern float		old_lightgamma;
extern float		old_brightness;
extern int			vid_gamma_changed;

extern vec3_t		cl_viewangles;

// screen (gl_screen.c)
extern float		scr_centertime_off;
extern cvar_t		scr_centertime;
extern int			scr_disabled_for_loading;
extern double		scr_disabled_time;
extern int			scr_drawloading;
extern float		scr_con_current;
extern int			scr_copyeverything;
extern int			scr_fullupdate;
extern cvar_t		scr_viewsize;
extern vrect_t		scr_vrect;

extern int			scr_disk_state;
extern int			scr_disk_counter;
extern int			scr_disk_offset;
extern float		scr_disk_time;

void SCR_UpdateScreen(void);
void SCR_EndLoadingPlaque(void);

extern struct viddef_s vid;
extern void			(*vid_menudrawfn)(void);
extern int			(*vid_menukeyfn)(int key);
extern int			vid_fullscreen;
extern int			vid_suppressModeChangePrint;
extern int			cl_playernum;
extern int			cl_viewentity;
extern struct model_s *cl_viewmodel;
extern int			cl_viewent_valid;
extern int			cl_viewent_sequence;
extern float		cl_viewent_animtime;
extern int			cl_viewent_light;
extern int			cl_maxclients;
extern int			cl_gametype;
extern char			cl_levelname[40];

extern int			cl_items;
extern int			cl_health;
extern int			cl_armorvalue;
extern int			cl_weaponmodel;
extern int			cl_weaponframe;
extern int			cl_activeweapon;
extern int			cl_weaponbits[32];
extern int			cl_stats[32];
extern int			cl_slotmask;

extern int			cl_cdtrack;
extern int			cl_looptrack;

extern int			r_dlightframecount;
extern int			r_dowarp;	// water level of the view, set by svc_clientdata
extern vec3_t		r_light_rgb;	// result of R_LightPoint
extern int			d_lightstylevalue[256];
extern edict_t		*r_refdef_onlyents;

// entities to draw this frame
extern int			cl_numvisedicts;
extern struct entity_s *cl_visedicts[MAX_VISEDICTS];

extern void R_AnimateLight(void);
extern void R_PushDlights(void);
extern void R_RenderDlights(void);
extern unsigned int *R_LightPoint(unsigned int *lightrgb, vec3_t point);

//
// renderer
//
extern float		r_ambient;
extern float		r_lightscale;
extern float		r_plightvec[3];
extern float		r_origin[3];
extern float		vpn[3], vright[3], vup[3];
extern float		r_blend_alpha;
extern int			r_interpolate_frames;
extern float		r_fullbright_value;
extern int			ztrick_frame;
extern int			c_lightmaps;
extern int			r_dowarp;
extern char			input_flags;

// alias model lighting
extern float		r_entorigin[3];
extern float		r_modeldist[3];
extern float		r_ambientlight;
extern float		r_shadelight;
extern float		*r_lightvec;
extern int			r_posenum;

// efrag building (gl_refrag.c)
extern vec3_t		r_emins, r_emaxs;
extern mnode_t		*r_pefragtopnode;
extern efrag_t		**r_lastlink;
extern entity_t		*r_addent;

// chase camera
extern vec3_t		chase_dest;
extern vec3_t		r_refdef_vieworg;
extern vec3_t		r_refdef_viewangles;

//
// palette
//
extern byte			*host_basepal;
extern byte			*host_colormap;

//
// game DLLs
//
extern int			g_iextdllcount;
extern void			*g_rgextdll[50];
extern void			*g_rgextinit[50];
extern int			g_deltaHullCacheChecksum;
extern int			g_engineHandleTable[9];

//
// network
//
extern sizebuf_t	net_message;

//
// protocol
//
extern char			*svc_strings[];

#endif // GLOBALS_H
