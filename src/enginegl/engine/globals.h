/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
****/

#ifndef GLOBALS_H
#define GLOBALS_H

 // =============================================================================

 // CENTRALIZED ENGINE GLOBALS

 // Shared variables across Server, Client, and Renderer subsystems.

 // =============================================================================

 // ------------------------------------------------------
 // Forward Declarations
 // ------------------------------------------------------
struct server_s;
struct server_static_s;
struct client_s;
struct client_static_s;
struct edict_s;
struct globalvars_s;
struct quakeparms_s;
struct entity_s;
typedef struct globalvars_s globalvars_t;
typedef struct quakeparms_s quakeparms_t;

 // ------------------------------------------------------
 // Engine Flow & Timing
 // ------------------------------------------------------
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

 // ------------------------------------------------------
 // Server State
 // ------------------------------------------------------
extern struct server_s			sv;
extern struct server_static_s	svs;
extern int			sv_max_edicts;
extern edict_t		*sv_edicts;
extern int			*sv_num_edicts;
extern int          sv_edicts_base;
extern int          sv_edicts_active;
extern char			sv_decalnames[255][16];
extern int			sv_decalnamecount;
extern edict_t		*sv_player;

 // ------------------------------------------------------
 // Progs/ VM State
 // ------------------------------------------------------
extern void			*progs;
extern void			*pr_functions;
extern void			*pr_globaldefs;
extern void			*pr_fielddefs;
extern void			*pr_statements;
extern globalvars_t	*pr_global_struct;
extern float		*pr_globals;
extern char			*pr_strings;
extern int			pr_edict_size;

 // ------------------------------------------------------
 // Game Rules/ Skill
 // ------------------------------------------------------
extern qboolean		noclip_anglehack;
extern int			current_skill;

 // ------------------------------------------------------
 // Client State
 // ------------------------------------------------------
extern struct client_static_s	cls;
extern struct client_s			cl;
extern char						cls_spawnparms[1024];

 // Client entity and state arrays
extern entity_t cl_entities[MAX_EDICTS];
extern entity_t cl_static_entities[128];
extern int cl_num_entities;
extern efrag_t cl_efrags[MAX_EFRAGS];
extern efrag_t *cl_free_efrags;
extern dlight_t cl_dlights[MAX_DLIGHTS];
extern char cl_lightstyle_value[MAX_LIGHTSTYLES][64];

 // Client subsystem globals
extern int			cl_num_statics; // Number of static entities
extern double		cl_mtime[2]; // Message time
extern double		cl_time; // Client time
extern double		cl_oldtime; // Previous client time
extern struct model_s *cl_model_precache[256]; // Model precache list
extern sfx_t		*cl_sound_precache[256]; // Sound precache list
extern struct model_s *cl_worldmodel; // World model pointer
extern lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES]; // Light style data
extern void			*cl_scores; // Player scores
extern vec3_t		cl_punchangle; // View punch angle
extern vec3_t		cl_punchangle_old; // Previous punch angle
extern vec3_t		cl_velocity; // Player velocity
extern float		cl_viewheight; // View height
extern float		cl_idealpitch; // Ideal pitch angle
extern int			cl_onground; // On ground flag
extern int			cl_inwater; // In water flag
extern int			cl_waterlevel; // Water level (0-3)
extern int			cl_intermission; // Intermission flag
extern int			cl_paused; // Pause flag (svc_setpause)
extern double		cl_completed_time; // Level completion time
extern int			cl_lightlevel; // Light level written into usercmd
extern float		cl_oldz; // Smooth stair height accumulator

 // Gamma control (used by V_BuildGammaTables/ palette updates)
extern float		old_gamma;
extern float		old_lightgamma;
extern float		old_brightness;
extern int			vid_gamma_changed;

 // Client view angles (shared by input, view, and parsing code)
extern vec3_t		cl_viewangles;

 // Screen/ centerprint timing (defined in gl_screen.c)
extern float		scr_centertime_off;
extern int			scr_disabled_for_loading;
extern double		scr_disabled_time;
extern int			scr_drawloading;
extern float		scr_con_current;
extern int			scr_copyeverything;
extern vrect_t		scr_vrect;

extern int			scr_disk_state;
extern int			scr_disk_counter;
extern int			scr_disk_offset;
extern float		scr_disk_time;

 // Screen system entry points (gl_screen.c)
void SCR_UpdateScreen(void);
void SCR_EndLoadingPlaque(void);


extern struct viddef_s vid; // viddef_t
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

extern int			cl_cdtrack;
extern int			cl_looptrack;

extern int			r_dlightframecount;
extern vec3_t		r_light_rgb; // Result of light point sampling
extern int			d_lightstylevalue[256];
extern edict_t      *r_refdef_onlyents;

 // Visible entity list for renderer (stores entity pointers)
extern int          cl_numvisedicts;
extern struct entity_s *cl_visedicts[MAX_VISEDICTS];

extern void R_AnimateLight(void);
extern void R_PushDlights(void);
extern void R_RenderDlights(void);
extern unsigned int *R_LightPoint(unsigned int *lightrgb, vec3_t point);


extern int			cmd_argc;
extern char*		cmd_argv[80];
extern char*		cmd_args;
extern int			cmd_source;

extern void Cmd_ExecuteString(char *text, cmd_source_t src);
extern int  Cmd_Argc(void);
extern char *Cmd_Argv(int arg);
extern char *Cmd_Args(void);

 // ------------------------------------------------------
 // Renderer State
 // ------------------------------------------------------
extern float		r_ambient;
extern float		r_lightscale;
extern float		r_plightvec[3];
extern float		r_origin[3];
extern float		vpn[3], vright[3], vup[3];
extern float		r_blend_alpha;

 // Camera/ chase globals
extern vec3_t		chase_dest; // Chase camera destination
extern vec3_t		r_refdef_vieworg; // View origin
extern vec3_t		r_refdef_viewangles; // View angles

 // ------------------------------------------------------
 // Resource/ Memory
 // ------------------------------------------------------
extern void			*hunk_base;
extern byte			*host_basepal;
extern byte			*host_colormap;

 // ------------------------------------------------------
 // Game DLL Integration
 // ------------------------------------------------------
extern int			g_iextdllcount;
extern void			*g_rgextdll[50];
extern void			*g_rgextinit[50];
extern int			g_deltaHullCacheChecksum; // DELTA hull cache checksum
extern int			g_engineHandleTable[9]; // Engine handle table

 // ------------------------------------------------------
 // Network Globals
 // ------------------------------------------------------
extern sizebuf_t	net_message; // Network message buffer

 // ------------------------------------------------------
 // SVC Protocol String Table
 // ------------------------------------------------------
extern char			*svc_strings[]; // SVC command name strings

#endif // GLOBALS_H
