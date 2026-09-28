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

// client.h

#ifndef CLIENT_H
#define CLIENT_H

#include "usercmd.h"

typedef union color24_s
{
	struct
	{
		byte	r, g, b;
	};
	byte	rgb[3];
} color24;

#define	MAX_CLIENTS			16
#define	MAX_SCOREBOARDNAME	32
#define	VID_GRADES			64	// light levels in the colormap

// player color ranges in the palette
#define	TOP_RANGE		16
#define	BOTTOM_RANGE	96

typedef struct
{
	char	name[MAX_SCOREBOARDNAME];
	float	entertime;
	int		frags;
	int		colors;			// two 4 bit fields
	byte	translations[VID_GRADES*256];
} scoreboard_t;

#define	MAX_DLIGHTS		32
#define	MAX_ELIGHTS		64

typedef struct dlight_s
{
	vec3_t		origin;
	float		radius;
	color24		color;
	float		die;			// stop lighting after this time
	float		decay;			// drop this each second
	float		minlight;		// don't add when contributing less
	int			ramp;
	int			key;
	qboolean	dark;			// subtracts light instead of adding
} dlight_t;

#ifndef MAX_LIGHTSTYLES
#define	MAX_LIGHTSTYLES	64
#endif
#define	MAX_STYLESTRING	64

typedef struct lightstyle_s
{
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

typedef struct entity_state_s
{
	vec3_t	origin;
	vec3_t	angles;

	int		modelindex;
	int		sequence;
	int		frame;
	int		colormap;
	union
	{
		int		skinnum;
		int		skin;
	};
	int		effects;

	int		rendermode;
	int		renderamt;
	byte	rendercolor[4];
	int		renderfx;
} entity_state_t;

typedef enum
{
	ca_dedicated,		// a dedicated server with no ability to start a client
	ca_disconnected,	// full screen console with no connection
	ca_connected,		// valid netcon, talking to a server
	ca_active
} connstate_t;

//
// the client_t structure is wiped completely at every server signon
//
typedef struct client_s
{
	int			movemessages;
	usercmd_t	lastcmd;

	double		time;
	double		mtime[2];

	vec3_t		viewangles;
	vec3_t		oldviewangles;
	float		mviewangles[2][3];	// during demo playback viewangles is lerped
									// between these
	vec3_t		punchangle;

	double		last_received_message;	// (realtime) for net trouble icon
	int			num_visedicts;
	int			snapshot_number;	// next SCR_Screenshot_f file number
	qboolean	movie_recording;
	int			viewentity;

	char		levelname[40];
	int			nointerp;
	int			free_entities;

	int			num_entities;	// held in cl_entities array
	struct model_s	*worldmodel;	// cl_entities[0].model
	float		oldtime;

	void		*pmove;
	int			message_size;

	byte		unused[2852];	// the rest of the state is kept in cl_* globals
} client_t;

//
// the client_static_t structure is persistent through an arbitrary number
// of server connections
//
typedef struct client_static_s
{
	connstate_t	state;
	int			signon;			// 0 to SIGNONS

// demo loop control
	qboolean	demorecording;
	qboolean	demoplayback;
	qboolean	timedemo;
	FILE		*demofile;
	int			demotrack;
	int			forcetrack;		// -1 = use normal cd track
	int			tracknum;
	int			demonum;		// -1 = don't play demos
	char		demos[MAX_DEMOS][64];	// when not playing

// timedemo
	int			td_lastframe;	// to meter out one message a frame
	int			td_startframe;	// host_framecount at start
	double		td_starttime;	// realtime at second frame of timedemo

// connection information
	struct qsocket_s	*netcon;
	qboolean	netconnection;

	sizebuf_t	message;		// writing buffer to send to server

	float		spawnparms[NUM_SPAWN_PARMS];

	qboolean	movierecording;
} client_static_t;

extern client_static_t	cls;
extern client_t			cl;

#define	MAX_STATIC_ENTITIES	128		// torches, etc
#define	MAX_CL_STATS		32

extern int	cl_stats_monsters;
extern int	cl_stats_totalmonsters;

//
// cvars
//
extern cvar_t	cl_name;
extern cvar_t	cl_color;

extern cvar_t	cl_upspeed;
extern cvar_t	cl_forwardspeed;
extern cvar_t	cl_backspeed;
extern cvar_t	cl_sidespeed;
extern cvar_t	cl_movespeedkey;
extern cvar_t	cl_yawspeed;
extern cvar_t	cl_pitchspeed;
extern cvar_t	cl_anglespeedkey;

extern cvar_t	cl_shownet;
extern cvar_t	cl_nolerp;
extern cvar_t	cl_lerplocal;
extern cvar_t	cl_lerpstep;
extern cvar_t	cl_pitchdriftspeed;
extern cvar_t	cl_pitchdrift;

extern cvar_t	cam_command;
extern cvar_t	cam_snapto;
extern cvar_t	cam_idealyaw;
extern cvar_t	cam_idealpitch;
extern cvar_t	cam_idealdist;
extern cvar_t	cam_contain;
extern cvar_t	c_maxpitch;
extern cvar_t	c_minpitch;
extern cvar_t	c_maxyaw;
extern cvar_t	c_minyaw;
extern cvar_t	c_maxdistance;
extern cvar_t	c_mindistance;

extern cvar_t	chase_active;

struct entity_s;
typedef struct entity_s cl_entity_t;

//
// cl_main
//
dlight_t *CL_AllocDlight(int key);
void CL_DecayLights(void);

void CL_Init(void);

void CL_EstablishConnection(char *host);

void CL_Disconnect(void);
void CL_Disconnect_f(void);
void CL_NextDemo(void);

void CL_ReadFromServer(void);
void CL_SendCmd(void);
void CL_SignonReply(void);
void CL_ClearState(void);

void V_BuildGammaTables(float gamma);

//
// cl_input
//
struct kbutton_s;

// kbutton_t state bits
#define	KB_DOWN			1	// held down
#define	KB_IMPULSEDOWN	2	// went down this frame
#define	KB_IMPULSEUP	4	// went up this frame

extern struct kbutton_s	in_mlook, in_strafe;

void CL_InitInput(void);
void CL_BaseMove(usercmd_t *cmd);
void CL_SendMove(usercmd_t *cmd);

void KeyDown(struct kbutton_s *b);
void KeyUp(struct kbutton_s *b);
float CL_KeyState(struct kbutton_s *key);

//
// cl_demo.c
//
void CL_StopPlayback(void);
int CL_GetMessage(void);
void CL_WriteDemoMessage(void);

void CL_Stop_f(void);
void CL_Record_f(void);
void CL_PlayDemo_f(void);
void CL_TimeDemo_f(void);
void CL_StartMovie_f(void);

//
// cl_parse.c
//
void CL_ParseServerMessage(void);
void CL_KeepaliveMessage(void);

//
// cl_tent
//
extern struct sfx_s	*cl_sfx_ric1, *cl_sfx_ric2, *cl_sfx_ric3, *cl_sfx_ric4, *cl_sfx_ric5;
extern struct sfx_s	*cl_sfx_explosion, *cl_sfx_spark1, *cl_sfx_spark2;

void CL_InitTEnts(void);
void CL_UpdateTEnts(void);
void CL_ParseTEnt(void);
int CL_FxBlend(cl_entity_t *ent);
int R_GetSpriteFrameCount(struct model_s *model);

//
// cam
//
extern int	cam_thirdperson;

void CAM_Think(void);
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

//
// chase
//
void Chase_Init(void);
void Chase_Update(void);

#endif // CLIENT_H
