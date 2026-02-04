#ifndef QUAKEDEF_H
#define QUAKEDEF_H

 // =========================================================
 // Platform and configuration
 // =========================================================

#ifndef GLQUAKE
#define GLQUAKE
#endif
#define VERSION         0.52

 // Constants
#define MAX_LIGHTSTYLES 64
#define MAX_SCOREBOARD  64
#define MAXLIGHTMAPS    4
#define MAX_EDICTS      600
#define MAX_MODELS      256
#define MAX_SOUNDS      256
#define MAX_VISEDICTS   256
#define NUM_SPAWN_PARMS 16
#define SAVEGAME_COMMENT_LENGTH 40
#define SIGNONS			4
#define MAX_DEMOS		8
#define MINIMUM_MEMORY	0x700000
#define MINIMUM_MEMORY_LEVELPAK 0xC00000

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#if defined(_WIN32)
#if defined(_M_IX86)
#define __i386__    1
#endif
#endif

 // =========================================================
 // Core subsystem headers
 // =========================================================

#include "common.h"
#include "zone.h"
#include "mathlib.h"
 // #include "globals.h"/ / Moved to end

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

#include "globals.h"
#include "host.h"

 // =========================================================
 // Global state
 // =========================================================

typedef struct quakeparms_s
{
    char    *basedir;
    char    *cachedir;
    int     argc;
    char    **argv;
    void    *membase;
    int     memsize;
} quakeparms_t;

extern quakeparms_t host_parms;
extern cvar_t hostname;
extern int host_initialized;

 // =========================================================
 // Host system parameters
 // =========================================================

int LoadEntityDLLs(const char *szBasePath);

extern qboolean is_dedicated;
extern int standard_quake;
extern int minimum_memory;

 // Centralized engine globals (declared in engine_globals.c)
#include "globals.h"

#endif // QUAKEDEF_H

