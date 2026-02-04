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
 // pmove.h -- Player movement definitions

#ifndef PMOVE_H
#define PMOVE_H

 // =========================================================
 // Player trace result
 // =========================================================
typedef struct pmtrace_s
{
	qboolean	allsolid; // if true, plane is not valid
	qboolean	startsolid; // if true, the initial point was in a solid area
	qboolean	inopen, inwater;
	float		fraction; // time completed, 1.0 = didn't hit anything
	vec3_t		endpos; // final position
	mplane_t	plane; // surface normal at impact
	int			ent; // entity the surface is on
	vec3_t		deltavelocity; // velocity change from impact
	int			hitgroup; // 0 = generic, 1-8 = body part
} pmtrace_t;

 // =========================================================
 // Physics entity for player movement
 // =========================================================
typedef struct physent_s
{
	char		name[32]; // name of model or "player"
	int			player; // player index, 0 if not player
	vec3_t		origin; // entity origin
	struct model_s *model; // NULL = point entity
	struct model_s *studiomodel; // studio model pointer
	vec3_t		mins, maxs; // bounding box (only if model == NULL)
	int			info; // for client, which entity
	vec3_t		angles; // entity angles
	int			solid; // solid type
	int			skin; // skin number
	int			rendermode; // render mode
	float		frame; // animation frame
	int			sequence; // animation sequence
	byte		controller[4]; // bone controller 0-3
	byte		blending[2]; // animation blending
	int			movetype; // move type
	int			takedamage; // take damage flag
	int			blooddecal; // blood decal index
	int			team; // team number
	int			classnumber; // class number
	int			iuser1; // user int 1
	int			iuser2; // user int 2
	int			iuser3; // user int 3
	int			iuser4; // user int 4
	float		fuser1; // user float 1
	float		fuser2; // user float 2
	float		fuser3; // user float 3
	float		fuser4; // user float 4
	vec3_t		vuser1; // user vector 1
	vec3_t		vuser2; // user vector 2
	vec3_t		vuser3; // user vector 3
	vec3_t		vuser4; // user vector 4
} physent_t;

 // =========================================================
 // Player move structure
 // =========================================================
#define MAX_PHYSENTS    600
#define MAX_MOVEENTS    64

typedef struct playermove_s
{
	int			player_index; // player entity index
	qboolean	server; // true = server, false = client
	qboolean	multiplayer; // true = multiplayer
	float		time; // current time
	float		frametime; // frame time
	vec3_t		forward, right, up; // player view vectors
	vec3_t		origin; // player origin
	vec3_t		angles; // player angles
	vec3_t		oldangles; // previous angles
	vec3_t		velocity; // player velocity
	vec3_t		movedir; // move direction
	vec3_t		basevelocity; // base velocity (conveyor, push)
	vec3_t		view_ofs; // view offset
	float		flDuckTime; // ducking transition time
	qboolean	bInDuck; // is ducking
	int			flTimeStepSound; // time for next footstep
	int			iStepLeft; // left/ right foot toggle
	float		flFallVelocity; // falling velocity
	vec3_t		punchangle; // view punch from damage
	float		flSwimTime; // time underwater
	float		flNextPrimaryAttack; // weapon timing
	int			effects; // EF_xxx
	int			flags; // FL_xxx
	int			usehull; // 0 = normal, 1 = ducked, 2 = point
	float		gravity; // gravity multiplier
	float		friction; // friction multiplier
	int			oldbuttons; // previous button state
	float		waterjumptime; // time until waterjump ends
	qboolean	dead; // player is dead
	int			deadflag; // dead state flags
	int			spectator; // spectator mode
	int			movetype; // move type
	int			onground; // entity we're standing on
	int			waterlevel; // 0=not in water, 1-3=depth
	int			watertype; // CONTENTS_xxx
	int			oldwaterlevel; // previous water level
	char		sztexturename[256]; // current texture
	char		chtexturetype; // texture type (metal, concrete, etc)
	float		maxspeed; // max player speed
	float		clientmaxspeed; // client-predicted max speed
	int			iuser1;
	int			iuser2;
	int			iuser3;
	int			iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
	vec3_t		vuser1;
	vec3_t		vuser2;
	vec3_t		vuser3;
	vec3_t		vuser4;
	int			numphysent; // number of physents
	physent_t	physents[MAX_PHYSENTS]; // physics entities
	int			nummoveent; // number of moveents
	physent_t	moveents[MAX_MOVEENTS]; // moving entities
	int			numvisent; // number of visible entities
	physent_t	visents[MAX_PHYSENTS]; // visible entities
 // ... cmd, runfuncs, etc would follow
} playermove_t;

 // =========================================================
 // Player movement functions
 // =========================================================
void PM_Init(playermove_t *pm);
void PM_Move(playermove_t *pm, qboolean server);
void PM_PreventMegaBunnyJumping(void);
pmtrace_t PM_PlayerTrace(vec3_t start, vec3_t end, int flags, int ignore_pe);
int PM_PointContents(vec3_t p, int *truecontents);
int PM_WaterEntity(vec3_t p);
int PM_TruePointContents(vec3_t p);

#endif // PMOVE_H
