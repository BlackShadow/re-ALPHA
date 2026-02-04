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
***/

#ifndef CLIENT_H
#define CLIENT_H

#include "usercmd.h"

 // =============================================================================
 // Color types
 // =============================================================================

typedef union color24_s
{
	struct
	{
		byte r, g, b;
	};
	byte rgb[3];
} color24;

 // =============================================================================
 // Dynamic light structure
 // =============================================================================

#define MAX_DLIGHTS     32
#define MAX_ELIGHTS     64

typedef struct dlight_s
{
	vec3_t      origin;
	float       radius;
	color24     color;
	float       die; // stop lighting after this time
	float       decay; // drop this each second
	float       minlight; // don't add when contributing less
	int         ramp;
	int         key;
	qboolean    dark; // subtracts light instead of adding
} dlight_t;

 // Client dynamic-light allocation (cl_main.c)
dlight_t *CL_AllocDlight(int key);

 // Temp entity subsystem (cl_tent.c)
void CL_InitTEnts(void);
void CL_UpdateTEnts(void);
void CL_ParseTEnt(void);

 // =============================================================================
 // Light style structure
 // =============================================================================

#ifndef MAX_LIGHTSTYLES
#define MAX_LIGHTSTYLES     64
#endif
#define MAX_STYLESTRING     64

typedef struct lightstyle_s
{
	int     length;
	char    map[MAX_STYLESTRING];
} lightstyle_t;

 // =============================================================================
 // Entity state
 // =============================================================================

typedef struct entity_state_s
{
 // Baseline origin/ angles
	vec3_t	origin;
	vec3_t	angles;

 // Baseline model/ animation properties
	int		modelindex;
	int		sequence;
	int		frame;
	int		colormap;
	union
	{
		int skinnum;
		int skin;
	};
	int		effects;

	int		rendermode;
	int		renderamt;
	byte	rendercolor[4];
	int		renderfx;
} entity_state_t;

 // Client state
typedef enum {
	ca_dedicated,
	ca_disconnected,
	ca_connected,
	ca_active
} connstate_t;

 // Per-client state
typedef struct client_s
{
	int			movemessages; // increments in CL_SendMove
	usercmd_t	lastcmd; // 28 bytes copied from usercmd_t

	double		time;
	double		mtime[2];

	vec3_t		viewangles;
	vec3_t		oldviewangles;
	float		mviewangles[2][3];
	vec3_t		punchangle;

	double		last_received_message;
	int			num_visedicts;
	int			snapshot_number;
	qboolean	movie_recording;
	int			viewentity; // View entity index (for camera)

	char	levelname[40]; // For host.c
	int		nointerp; // For host.c
	int		free_entities; // For cl_main.c

	int     num_entities; // Current number of entities
	struct model_s *worldmodel; // World model pointer
	float   oldtime; // Previous frame time

	void*	pmove;
 // ... gap...
	int		message_size;

 // NOTE: The engine clears this struct with a single memset.
 // Keep the struct size in sync to avoid clobbering adjacent globals in engine_globals.c.
	byte	reserved_0x00D4[0xBF8 - 0x00D4];
} client_t;

typedef char client_t_size_must_be_0xBF8[(sizeof(client_t) == 0xBF8) ? 1 : -1];

struct qsocket_s;

typedef struct client_static_s
{
	connstate_t	state;
	int			signon;
	
 // Demo state
	qboolean	demorecording;
	qboolean	demoplayback;
	qboolean	timedemo;
	FILE		*demofile;
	int			demotrack;
	int			forcetrack;
	int			tracknum;
	int			demonum; // current demo index in demos[]
	char		demos[8][64]; // demo loop names from startdemos

	
 // Timedemo tracking
	int			td_lastframe;
	int			td_startframe;
	double		td_starttime;
	
 // Networking
	struct qsocket_s *netcon;
	qboolean    netconnection; // legacy/ unknown (kept for compatibility)
	
	sizebuf_t	message;
	
	float       spawnparms[NUM_SPAWN_PARMS];

 // Movie recording
	qboolean	movierecording;
} client_static_t;



extern client_static_t	cls;
extern client_t			cl;

 // Client-side stats for save games
extern int cl_stats_monsters;
extern int cl_stats_totalmonsters;

 // =============================================================================
 // Client movement cvars (defined in cl_input.c)
 // =============================================================================

extern cvar_t cl_upspeed;
extern cvar_t cl_forwardspeed;
extern cvar_t cl_backspeed;
extern cvar_t cl_sidespeed;
extern cvar_t cl_movespeedkey;
extern cvar_t cl_yawspeed;
extern cvar_t cl_pitchspeed;
extern cvar_t cl_anglespeedkey;

 // =============================================================================
 // Client cvars (defined in cl_main.c)
 // =============================================================================

extern cvar_t cl_shownet;
extern cvar_t cl_nolerp;
extern cvar_t cl_lerplocal;
extern cvar_t cl_lerpstep;
extern cvar_t cl_pitchdriftspeed;
extern cvar_t cl_pitchdrift;

 // =============================================================================
 // Camera cvars (defined in cam.c)
 // =============================================================================

extern cvar_t cam_command;
extern cvar_t cam_snapto;
extern cvar_t cam_idealyaw;
extern cvar_t cam_idealpitch;
extern cvar_t cam_idealdist;
extern cvar_t cam_contain;
extern cvar_t c_maxpitch;
extern cvar_t c_minpitch;
extern cvar_t c_maxyaw;
extern cvar_t c_minyaw;
extern cvar_t c_maxdistance;
extern cvar_t c_mindistance;

 // =============================================================================
 // Type aliases for compatibility
 // =============================================================================

struct entity_s;
typedef struct entity_s cl_entity_t;

 // =============================================================================
 // Client subsystem functions
 // =============================================================================

void CL_Init(void);
void CL_InitInput(void);
void CL_SendCmd(void);
int CL_ReadFromServer(void);
int CL_GetMessage(void);
void CL_ParseServerMessage(void);
int CL_ClearState(void);
void CL_SignonReply(void);
void CL_KeepaliveMessage(void);
int CL_Disconnect_f(void);
void CL_EstablishConnection(char *host);
void CL_Stop_f(void);
int CL_DecayLights(void);

void CAM_Think(void);

int CL_Disconnect(void);
void CL_StopPlayback(void);
void CL_WriteDemoMessage(void);
void CL_NextDemo(void);
void CL_BaseMove(usercmd_t *cmd);
int CL_SendMove(usercmd_t *cmd);

 // Third-person camera command handlers (cam.c)
void CAM_PitchUpDown(void);
void CAM_PitchUpUp(void);
void CAM_PitchDownDown(void);
void CAM_PitchDownUp(void);
void CAM_YawLeftDown(void);
void CAM_YawLeftUp(void);
void CAM_YawRightDown(void);
void CAM_YawRightUp(void);
void CAM_InDown(void);
void CAM_InUp(void);
void CAM_OutDown(void);
void CAM_OutUp(void);
void CAM_ToThirdPerson(void);
void CAM_ToFirstPerson(void);
void CAM_ToggleSnapto(void);
void CAM_StartMouseMove(void);
void CAM_EndMouseMove(void);
void CAM_StartDistance(void);
void CAM_EndDistance(void);

#endif // CLIENT_H
