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

// host.h -- host (frame loop, local server control and host commands)

#ifndef HOST_H
#define HOST_H

#define SAVEGAME_VERSION	16

extern cvar_t host_framerate;
extern cvar_t host_speeds;
extern cvar_t sys_ticrate;
extern cvar_t serverprofile;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t teamplay;
extern cvar_t samelevel;
extern cvar_t noexit;
extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t pausable;
extern cvar_t developer;

void Host_Init(quakeparms_t *parms);
void Host_Shutdown(void);
void Host_Frame(float time);
void Host_Error(const char *error, ...);
void Host_EndGame(const char *message, ...);
void Host_ShutdownServer(qboolean crash);
void Host_ClearMemory(void);
void Host_InitCommands(void);

// savegame output (used by ED_Write / ED_WriteGlobals)
void Savegame_WriteInt(savebuf_t *buf, int value);
void Savegame_WriteInt2(savebuf_t *buf, int value);
void Savegame_WriteString(savebuf_t *buf, const char *str);

#endif // HOST_H
