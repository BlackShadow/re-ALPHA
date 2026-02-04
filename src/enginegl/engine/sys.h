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

 // sys.h -- system interface
 // NOTE: This is a skeleton header derived from reverse engineering.
 // Platform-specific functions will be filled during translation.

#ifndef SYS_H
#define SYS_H

#include <stddef.h>

 // =============================================================================
 // System initialization and shutdown
 // =============================================================================

void Sys_Init(void);
void Sys_Shutdown(void);
__declspec(noreturn) void Sys_Error(const char *error, ...);
void Sys_Quit(void);

 // =============================================================================
 // Console and debug output
 // =============================================================================

int Sys_Printf(char *fmt, ...);
char *Sys_ConsoleInput(void);

 // =============================================================================
 // Timing
 // =============================================================================

void Sys_InitFloatTime(void);
double Sys_FloatTime(void);
void Sys_Sleep(void);
void Sys_SendKeyEvents(void);

 // =============================================================================
 // File I/ O
 // =============================================================================

int Sys_FileOpenRead(char *path, int *handle);
int Sys_FileOpenWrite(char *path);
void Sys_FileClose(int handle);
void Sys_FileSeek(int handle, int position);
size_t Sys_FileRead(int handle, void *dst, size_t count);
size_t Sys_FileWrite(int handle, void *src, size_t count);
int Sys_FileTime(char *path);
int Sys_mkdir(char *path);

 // Main entry point for Windows
 // Main entry point for Windows
 // defined in sys_win.c

 // =============================================================================

#endif // SYS_H
