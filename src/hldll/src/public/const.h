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
#ifndef CONST_H
#define CONST_H

// entvars_t::flags
#define FL_FLY					(1<<0)
#define FL_SWIM					(1<<1)
#define FL_CLIENT				(1<<3)
#define FL_INWATER				(1<<4)
#define FL_MONSTER				(1<<5)
#define FL_GODMODE				(1<<6)
#define FL_NOTARGET				(1<<7)
#define FL_ONGROUND				(1<<9)
#define FL_PARTIALGROUND		(1<<10)
#define FL_WATERJUMP			(1<<11)
#define FL_JUMPRELEASED			(1<<12)	// jump key released since the last jump
#define FL_DUCKING				(1<<14)

// entvars_t::movetype
#define MOVETYPE_NONE			0
#define MOVETYPE_WALK			3
#define MOVETYPE_STEP			4
#define MOVETYPE_FLY			5
#define MOVETYPE_TOSS			6
#define MOVETYPE_PUSH			7
#define MOVETYPE_NOCLIP			8
#define MOVETYPE_FLYMISSILE		9
#define MOVETYPE_BOUNCE			10
#define MOVETYPE_PUSHSTEP		13

// entvars_t::solid
#define SOLID_NOT				0
#define SOLID_TRIGGER			1
#define SOLID_BBOX				2
#define SOLID_SLIDEBOX			3
#define SOLID_BSP				4

// entvars_t::deadflag
#define DEAD_NO					0
#define DEAD_DYING				1
#define DEAD_DEAD				2
#define DEAD_RESPAWNABLE		3

// entvars_t::takedamage
#define DAMAGE_NO				0
#define DAMAGE_YES				1
#define DAMAGE_AIM				2

// entvars_t::effects
#define EF_BRIGHTFIELD			1
#define EF_MUZZLEFLASH			2
#define EF_DIMLIGHT				8

// entvars_t::rendermode
#define kRenderNormal			0
#define kRenderTransColor		1	// solid color, blended by renderamt
#define kRenderTransTexture		2	// texture, blended by renderamt

// point contents / entvars_t::watertype
#define CONTENTS_EMPTY			-1
#define CONTENTS_SOLID			-2
#define CONTENTS_WATER			-3
#define CONTENTS_SLIME			-4
#define CONTENTS_LAVA			-5

// entvars_t::waterlevel
#define WATERLEVEL_DRY			0
#define WATERLEVEL_FEET			1
#define WATERLEVEL_WAIST		2
#define WATERLEVEL_HEAD			3

// entvars_t::button
#define IN_ATTACK				(1<<0)
#define IN_JUMP					(1<<1)
#define IN_DUCK					(1<<2)
#define IN_FORWARD				(1<<3)
#define IN_BACK					(1<<4)
#define IN_USE					(1<<5)
#define IN_CANCEL				(1<<6)
#define IN_LEFT					(1<<7)
#define IN_RIGHT				(1<<8)
#define IN_MOVELEFT				(1<<9)
#define IN_MOVERIGHT			(1<<10)
#define IN_ATTACK2				(1<<11)

// sound channels
#define CHAN_AUTO				0
#define CHAN_WEAPON				1
#define CHAN_VOICE				2
#define CHAN_ITEM				3
#define CHAN_BODY				4

// sound volume and attenuation
#define VOL_NORM				1.0f
#define ATTN_NONE				0.0f
#define ATTN_NORM				0.8f
#define ATTN_IDLE				2.0f
#define ATTN_STATIC				2.25f

// message destinations for the WRITE_* functions
#define MSG_BROADCAST			0	// unreliable to all
#define MSG_ONE					1	// reliable to gpGlobals->msg_entity
#define MSG_ALL					2	// reliable to all

// server to client messages
#define SVC_TEMPENTITY			23
#define SVC_KILLEDMONSTER		27
#define SVC_FOUNDSECRET			28
#define SVC_WEAPONANIM			35
#define SVC_ROOMTYPE			37

// SVC_TEMPENTITY types
#define TE_GUNSHOT				2	// coord[3] pos
#define TE_EXPLOSION			3	// coord[3] pos
#define TE_WATERCOLOR			5	// byte r, byte g, byte b, byte (unused)
#define TE_TRACER				6	// coord[3] start, coord[3] end
#define TE_SPARKS				9	// coord[3] pos
#define TE_BLOODSTREAM			101	// coord[3] pos, coord[3] dir, byte color, byte speed
#define TE_SHOWLINE				102	// coord[3] start, coord[3] end
#define TE_BLOOD				103	// coord[3] pos, coord[3] dir, byte color, byte speed
#define TE_DECAL				104	// coord[3] pos, short entity index, byte decal index
#define TE_SPRITE_SPRAY			107	// coord[3] pos, coord speed, short model, short count, byte life in 0.1's
#define TE_BREAKMODEL			108	// coord[3] pos, coord[3] size, coord[3] velocity, short model, byte count, byte life in 0.1's, byte flags

#endif // CONST_H
