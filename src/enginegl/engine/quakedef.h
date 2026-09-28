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

// quakedef.h -- primary header for client and server

#ifndef QUAKEDEF_H
#define QUAKEDEF_H

#ifndef GLQUAKE
#define GLQUAKE
#endif

#define VERSION				0.52

#define MINIMUM_MEMORY			0x700000
#define MINIMUM_MEMORY_LEVELPAK	0xC00000

#define MAX_LIGHTSTYLES		64
#define MAX_SCOREBOARD		64
#define MAXLIGHTMAPS		4
#define MAX_EDICTS			600		// FIXME: ouch! ouch! ouch!
#define MAX_MODELS			256		// these are sent over the net as bytes
#define MAX_SOUNDS			256		// so they cannot be blindly increased
#define MAX_VISEDICTS		256

#define NUM_SPAWN_PARMS		16

#define SAVEGAME_COMMENT_LENGTH	40	// including the terminating 0

#define SIGNONS				4		// signon messages to receive before connected

#define MAX_DEMOS			8

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#if defined(_WIN32) && defined(_M_IX86)
#define __i386__	1
#endif

#include "common.h"
#include "zone.h"
#include "mathlib.h"

#include "bspfile.h"
#include "wad.h"
#include "sys.h"

#include "cmd.h"
#include "cvar.h"

#include "client.h"
#include "server.h"

#include "render.h"
#include "draw.h"
#include "net.h"
#include "protocol.h"
#include "const.h"

#include "progdefs.h"
#include "model.h"
#include "keys.h"
#include "in.h"
#include "console.h"
#include "menu.h"
#include "sound.h"
#include "edict.h"
#include "world.h"
#include "decal.h"

#ifdef GLQUAKE
#include "glquake.h"
#endif

//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

typedef struct quakeparms_s
{
	char	*basedir;
	char	*cachedir;		// for development over ISDN lines
	int		argc;
	char	**argv;
	void	*membase;
	int		memsize;
} quakeparms_t;

//=============================================================================

#include "globals.h"
#include "host.h"

extern cvar_t hostname;

extern qboolean is_dedicated;
extern int standard_quake;
extern int minimum_memory;

int LoadEntityDLLs(const char *szBasePath);

#endif // QUAKEDEF_H
