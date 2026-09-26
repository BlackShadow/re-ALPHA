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
#ifndef PROGDEFS_H
#define PROGDEFS_H

#include "vector.h"

typedef int string_t;		// offset into the engine string table
typedef int func_t;			// QuakeC function index
typedef int EOFFSET;		// byte offset of an edict in the engine edict array, 0 is the world

typedef struct edict_s edict_t;

typedef struct globalvars_s
{
	float		vmglobals[28];		// QuakeC return value and parameters, unused by the DLL
	EOFFSET		self;
	EOFFSET		other;
	EOFFSET		world;
	float		time;
	float		frametime;
	float		force_retouch;
	string_t	mapname;
	string_t	startspot;
	float		deathmatch;
	float		coop;
	float		teamplay;
	float		serverflags;
	float		total_secrets;
	float		total_monsters;
	float		found_secrets;
	float		killed_monsters;
	void		*pSpawnParms;		// engine parm1: spawn parms block kept across level changes
	float		parms[15];			// engine parm2 .. parm16, unused by the DLL
	Vector		v_forward;
	Vector		v_up;
	Vector		v_right;
	float		trace_allsolid;
	float		trace_startsolid;
	float		trace_fraction;
	Vector		trace_endpos;
	Vector		trace_plane_normal;
	float		trace_plane_dist;
	EOFFSET		trace_ent;
	float		trace_inopen;
	float		trace_inwater;
	EOFFSET		msg_entity;
	func_t		main;
	func_t		StartFrame;
	func_t		PlayerPreThink;
	func_t		PlayerPostThink;
	func_t		ClientKill;
	func_t		ClientConnect;
	func_t		PutClientInServer;
	func_t		ClientDisconnect;
	func_t		SetNewParms;
	func_t		SetChangeParms;
} globalvars_t;

typedef struct entvars_s
{
	float		modelindex;
	Vector		absmin;
	Vector		absmax;
	float		ltime;
	float		movetype;
	float		solid;
	Vector		origin;
	Vector		oldorigin;
	Vector		velocity;
	Vector		angles;
	Vector		avelocity;
	Vector		basevelocity;
	Vector		punchangle;
	string_t	classname;
	string_t	model;
	float		skin;
	float		body;
	float		effects;
	float		gravity;
	float		friction;
	float		light_level;
	int			sequence;
	float		animtime;
	float		frame;
	float		framerate;
	unsigned char controller[4];
	unsigned char blending[4];
	Vector		mins;
	Vector		maxs;
	Vector		size;
	func_t		touch;
	func_t		use;
	func_t		think;
	func_t		blocked;
	float		nextthink;
	EOFFSET		groundentity;
	float		rendermode;
	float		renderamt;
	Vector		rendercolor;
	float		renderfx;
	float		health;
	float		frags;
	int			weapon;
	int			weapons;
	string_t	weaponmodel;
	float		weaponframe;
	float		currentammo;
	int			ammo_1;
	int			ammo_2;
	int			ammo_3;
	int			ammo_4;
	int			items;
	int			items2;
	float		takedamage;
	EOFFSET		chain;
	float		deadflag;
	Vector		view_ofs;
	int			button;
	float		impulse;
	float		fixangle;
	Vector		v_angle;
	float		idealpitch;
	float		pitch_speed;
	string_t	netname;
	EOFFSET		enemy;
	float		flags;
	float		colormap;
	float		team;
	float		max_health;
	float		teleport_time;
	float		armortype;
	float		armorvalue;
	float		waterlevel;
	float		watertype;
	float		ideal_yaw;
	float		yaw_speed;
	EOFFSET		aiment;
	EOFFSET		goalentity;
	float		spawnflags;
	string_t	target;
	string_t	targetname;
	float		dmg_take;
	float		dmg_save;
	EOFFSET		dmg_inflictor;
	EOFFSET		owner;
	Vector		movedir;
	string_t	message;
	float		sounds;
	string_t	noise;
	string_t	noise1;
	string_t	noise2;
	string_t	noise3;
	float		speed;
	float		dmg;
	float		dmgtime;
	float		air_finished;
	float		pain_finished;
	float		radsuit_finished;
	edict_t		*pContainingEntity;
	globalvars_t *pSystemGlobals;
} entvars_t;

#endif // PROGDEFS_H
