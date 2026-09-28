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

// protocol.h -- communications protocols

#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PROTOCOL_VERSION	15

// if the high bit of the servercmd is set, the low bits are fast update flags:
#define U_MOREBITS		(1<<0)
#define U_ORIGIN1		(1<<1)
#define U_ORIGIN2		(1<<2)
#define U_ORIGIN3		(1<<3)
#define U_ANGLE2		(1<<4)
#define U_NOLERP		(1<<5)	// don't interpolate movement
#define U_FRAME			(1<<6)
#define U_SIGNAL		(1<<7)	// just differentiates from other updates

// svc_update can pass all of the fast update bits, plus more
#define U_ANGLE1		(1<<8)
#define U_ANGLE3		(1<<9)
#define U_MODEL			(1<<10)
#define U_COLORMAP		(1<<11)
#define U_SKIN			(1<<12)
#define U_EFFECTS		(1<<13)
#define U_LONGENTITY	(1<<14)
#define U_MOREBITS2		(1<<15)	// a third byte of bits follows

#define U_SEQUENCE		(1<<17)	// byte sequence, byte animation time
#define U_CONTROLLER	(1<<18)	// four bone controller bytes
#define U_RENDER		(1<<19)	// rendermode, renderamt, rendercolor, renderfx
#define U_BLENDING		(1<<20)	// two blending bytes
#define U_BODY			(1<<21)
#define U_FRAMERATE		(1<<22)
#define U_STEP			(1<<23)	// animating in place, interpolate as MOVETYPE_STEP

#define SU_VIEWHEIGHT	(1<<0)
#define SU_IDEALPITCH	(1<<1)
#define SU_PUNCH1		(1<<2)
#define SU_PUNCH2		(1<<3)
#define SU_PUNCH3		(1<<4)
#define SU_VELOCITY1	(1<<5)
#define SU_VELOCITY2	(1<<6)
#define SU_VELOCITY3	(1<<7)
#define SU_WEAPONS		(1<<8)	// long weapons follows
#define SU_ITEMS		(1<<9)
#define SU_ONGROUND		(1<<10)	// no data follows, the bit is it
#define SU_INWATER		(1<<11)	// no data follows, the bit is it
#define SU_WEAPONFRAME	(1<<12)
#define SU_ARMOR		(1<<13)
#define SU_WEAPON		(1<<14)
#define SU_UNDERWATER	(1<<15)	// eyes are under water, no data follows

// a sound with no channel is a local only sound
#define SND_VOLUME		(1<<0)	// a byte
#define SND_ATTENUATION	(1<<1)	// a byte
#define SND_LOOPING		(1<<2)	// a long

#define DEFAULT_VIEWHEIGHT	22
#define DEFAULT_SOUND_PACKET_VOLUME			255
#define DEFAULT_SOUND_PACKET_ATTENUATION	1.0f

// game types sent by serverinfo
// these determine which intermission screen plays
#define GAME_COOP			0
#define GAME_DEATHMATCH		1

//
// server to client
//
typedef enum
{
	svc_bad,
	svc_nop,
	svc_disconnect,
	svc_updatestat,			// [byte] [long]
	svc_version,			// [long] server version
	svc_setview,			// [short] entity number
	svc_sound,				// <see code>
	svc_time,				// [float] server time
	svc_print,				// [string] null terminated string
	svc_stufftext,			// [string] stuffed into client's console buffer
							// the string should be \n terminated
	svc_setangle,			// [angle3] set the view angle to this absolute value
	svc_serverinfo,			// [long] version [byte] maxclients [byte] gametype
							// [string] level name
							// [string]..[0]model cache
							// [string]...[0]sounds cache
	svc_lightstyle,			// [byte] [string]
	svc_updatename,			// [byte] [string]
	svc_updatefrags,		// [byte] [short]
	svc_clientdata,			// <shortbits + data>
	svc_stopsound,			// <see code>
	svc_updatecolors,		// [byte] [byte]
	svc_particle,			// [vec3] <variable>
	svc_damage,
	svc_spawnstatic,
	svc_spawnbinary,
	svc_spawnbaseline,
	svc_temp_entity,
	svc_setpause,			// [byte] on / off
	svc_signonnum,			// [byte] used for the signon sequence
	svc_centerprint,		// [string] to put in center of the screen
	svc_killedmonster,
	svc_foundsecret,
	svc_spawnstaticsound,	// [coord3] [byte] samp [byte] vol [byte] aten
	svc_intermission,
	svc_finale,				// [string] text
	svc_cdtrack,			// [byte] track [byte] looptrack
	svc_sellscreen,
	svc_cutscene,			// [string] text
	svc_weaponanim,			// [byte] sequence
	svc_decalname,			// [byte] index [string] name
	svc_roomtype			// [short] room type
} svc_t;

//
// client to server
//
typedef enum
{
	clc_bad,
	clc_nop,
	clc_disconnect,
	clc_move,				// [usercmd_t]
	clc_stringcmd			// [string] message
} clc_t;

//
// stats are integers communicated to the client by the server
//
#define STAT_TOTALSECRETS	11
#define STAT_TOTALMONSTERS	12
#define STAT_SECRETS		13
#define STAT_MONSTERS		14

//
// temp entity events
//
#define TE_SPIKE			0	// coord[3] pos
#define TE_SUPERSPIKE		1	// coord[3] pos
#define TE_GUNSHOT			2	// coord[3] pos
#define TE_EXPLOSION		3	// coord[3] pos
#define TE_TAREXPLOSION		4	// coord[3] pos
#define TE_WATERCOLOR		5	// byte r, byte g, byte b, byte (unused)
#define TE_TRACER			6	// coord[3] start, coord[3] end
#define TE_WIZSPIKE			7	// coord[3] pos
#define TE_KNIGHTSPIKE		8	// coord[3] pos
#define TE_SPARKS			9	// coord[3] pos
#define TE_LAVASPLASH		10	// coord[3] pos
#define TE_TELEPORT			11	// coord[3] pos
#define TE_EXPLOSION2		12	// coord[3] pos, byte color start, byte color length
#define TE_IMPLOSION		14	// coord[3] pos
#define TE_RAILTRAIL		15	// coord[3] start, coord[3] end
#define TE_TELEPORTSPLASH	100	// coord[3] pos
#define TE_BLOODSTREAM		101	// coord[3] pos, coord[3] dir, byte color, byte speed
#define TE_SHOWLINE			102	// coord[3] start, coord[3] end
#define TE_BLOOD			103	// coord[3] pos, coord[3] dir, byte color, byte speed
#define TE_DECAL			104	// coord[3] pos, short entity index, byte decal index
#define TE_BUBBLES			105	// short entity index, short model, byte count
#define TE_BUBBLETRAIL		106	// coord[3] pos, coord[3] velocity, short model, byte life in 0.1's
#define TE_SPRITE_SPRAY		107	// coord[3] pos, coord speed, short model, short count, byte life in 0.1's
#define TE_BREAKMODEL		108	// coord[3] pos, coord[3] size, coord[3] velocity, short model, byte count, byte life in 0.1's, byte flags

// TE_BREAKMODEL flags
#define BREAK_TYPEMASK		0x0f
#define BREAK_GLASS			0x01	// translucent pieces
#define BREAK_METAL			0x02	// pieces leave smoke trails
#define BREAK_FLESH			0x04
#define BREAK_WOOD			0x08	// four times as many pieces
#define BREAK_SMOKE			0x10	// pieces leave smoke trails
#define BREAK_TRANS			0x20	// translucent pieces

#endif // PROTOCOL_H
