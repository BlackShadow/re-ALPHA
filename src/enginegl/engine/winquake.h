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
 // winquake.h -- Windows-specific Quake/ Half-Life definitions

#ifndef WINQUAKE_H
#define WINQUAKE_H

#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>

 // Windows input
extern HWND mainwindow;
extern qboolean ActiveApp, Minimized;

 // DirectInput definitions (if used)
#ifndef DINPUT_BUFFERSIZE
#define DINPUT_BUFFERSIZE 16
#endif

 // Mouse state
extern int window_center_x, window_center_y;
extern RECT window_rect;

 // Video
extern DEVMODE gdevmode;
extern qboolean DDActive;

#endif // _WIN32

#endif // WINQUAKE_H
