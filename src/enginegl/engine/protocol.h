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

 // protocol.h -- network protocol constants
 // NOTE: This is a skeleton header derived from reverse engineering.
 // Protocol constants will be filled during translation from reverse-engineered export.

#ifndef PROTOCOL_H
#define PROTOCOL_H

 // =============================================================================
 // Protocol version
 // =============================================================================

 // Protocol version to be determined from reverse engineering
#define PROTOCOL_VERSION    15

 // =============================================================================
 // Entity update flags
 // =============================================================================

 // Fast update bits (low byte)
#define U_MOREBITS      (1<<0)
#define U_ORIGIN1       (1<<1)
#define U_ORIGIN2       (1<<2)
#define U_ORIGIN3       (1<<3)
#define U_ANGLE2        (1<<4)
#define U_NOLERP        (1<<5) // don't interpolate movement
#define U_FRAME         (1<<6)
#define U_SIGNAL        (1<<7) // differentiates from other updates

 // Extended update bits (morebits)
#define U_ANGLE1        (1<<8)
#define U_ANGLE3        (1<<9)
#define U_MODEL         (1<<10)
#define U_COLORMAP      (1<<11)
#define U_SKIN          (1<<12)
#define U_EFFECTS       (1<<13)
#define U_LONGENTITY    (1<<14)
#define U_MOREBITS2		(1<<15) // guess

 // Morebits2
#define U_RENDERMODE	(1<<0)
#define U_RENDERAMT		(1<<1)
#define U_RENDERCOLOR	(1<<2)
#define U_RENDERFX		(1<<3)
#define U_SCALE			(1<<4)
#define U_SOLID			(1<<5)
#define U_SEQUENCE		(1<<6)


 // =============================================================================
 // Client update flags (svc_clientdata)
 // =============================================================================

#define SU_VIEWHEIGHT   (1<<0)
#define SU_IDEALPITCH   (1<<1)
#define SU_PUNCH1       (1<<2)
#define SU_PUNCH2       (1<<3)
#define SU_PUNCH3       (1<<4)
#define SU_VELOCITY1    (1<<5)
#define SU_VELOCITY2    (1<<6)
#define SU_VELOCITY3    (1<<7)
#define SU_ITEMS        (1<<9)
#define SU_ONGROUND     (1<<10)
#define SU_INWATER      (1<<11)
#define SU_WEAPONFRAME  (1<<12)
#define SU_ARMOR        (1<<13)
#define SU_WEAPON       (1<<14)

 // =============================================================================
 // Sound flags
 // =============================================================================

#define SND_VOLUME          (1<<0) // byte follows
#define SND_ATTENUATION     (1<<1) // byte follows
#define SND_LOOPING         (1<<2) // long follows

 // =============================================================================
 // Default values
 // =============================================================================

 // NOTE: Default values for various protocol fields
#define DEFAULT_VIEWHEIGHT  22
#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0f

 // =============================================================================
 // Game types
 // =============================================================================

 // NOTE: Game type constants for intermission selection
#define GAME_COOP           0
#define GAME_DEATHMATCH     1

 // =============================================================================
 // Server to client messages (svc_*)
typedef enum {
	svc_bad,
	svc_nop,
	svc_disconnect,
	svc_updatestat,
	svc_version,
	svc_setview,
	svc_sound,
	svc_time,
	svc_print,
	svc_stufftext,
	svc_setangle,
	svc_serverinfo,
	svc_lightstyle,
	svc_updatename,
	svc_updatefrags,
	svc_clientdata,
	svc_stopsound,
	svc_updatecolors,
	svc_particle,
	svc_damage,
	svc_spawnstatic,
	svc_spawnbinary, // 21 (unused/ reserved)
	svc_spawnbaseline,
	svc_temp_entity,
	svc_setpause,
	svc_signonnum,
	svc_centerprint,
	svc_killedmonster,
	svc_foundsecret,
	svc_spawnstaticsound,
	svc_intermission,
	svc_finale,
	svc_cdtrack,
	svc_sellscreen,
	svc_cutscene,

 // Half-Life extensions (seen in cl_parse.c/ sv_main.c)
	svc_weaponanim,
	svc_decalname,
	svc_roomtype
} svc_t;

 // Client to server messages (clc_*)
typedef enum {
	clc_bad,
	clc_nop,
	clc_disconnect,
	clc_move,
	clc_stringcmd
} clc_t;

 // =============================================================================
 // Player stats indices (svc_updatestat)
 // =============================================================================

#define STAT_TOTALSECRETS   11
#define STAT_TOTALMONSTERS  12
#define STAT_SECRETS        13
#define STAT_MONSTERS       14


 // =============================================================================
 // Temporary entity events (TE_*)
 // =============================================================================

 // NOTE: Temp entity types to be filled from effects and particle code

 // Expected temp entities
 // TE_SPIKE, TE_SUPERSPIKE, TE_GUNSHOT
 // TE_EXPLOSION, TE_TAREXPLOSION
 // TE_LIGHTNING1, TE_LIGHTNING2, TE_LIGHTNING3
 // TE_WIZSPIKE, TE_KNIGHTSPIKE
 // TE_LAVASPLASH, TE_TELEPORT
 // TE_EXPLOSION2, TE_BEAM

 // =============================================================================

#endif // PROTOCOL_H
