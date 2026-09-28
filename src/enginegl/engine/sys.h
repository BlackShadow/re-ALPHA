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
// sys.h -- non-portable functions

#ifndef SYS_H
#define SYS_H

#include <stddef.h>

//
// file IO
//

// returns the file size
// return -1 if file is not present
// the file should be in BINARY mode for stupid OSs that care
int Sys_FileOpenRead(char *path, int *hndl);

int Sys_FileOpenWrite(char *path);
void Sys_FileClose(int handle);
void Sys_FileSeek(int handle, int position);
size_t Sys_FileRead(int handle, void *dest, size_t count);
size_t Sys_FileWrite(int handle, void *data, size_t count);
int Sys_FileTime(char *path);
int Sys_mkdir(char *path);

//
// system IO
//
__declspec(noreturn) void Sys_Error(const char *error, ...);
// an error will cause the entire program to exit

void Sys_Printf(char *fmt, ...);
// send text to the console

void Sys_Quit(void);

void Sys_Init(void);
void Sys_InitFloatTime(void);
double Sys_FloatTime(void);

char *Sys_ConsoleInput(void);

void Sys_Sleep(void);

void Sys_SendKeyEvents(void);
// Perform Key_Event () callbacks until the input que is empty

extern char	Buffer[1024];	// scratch text buffer (EngineFprintf)

#endif // SYS_H
