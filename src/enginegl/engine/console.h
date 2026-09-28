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

// console.h -- the drop down console and the notify lines

#ifndef CONSOLE_H
#define CONSOLE_H

extern int	con_totallines;
extern int	con_backscroll;
extern int	con_forcedup;	// console is full screen: no level running
extern int	con_initialized;

void Con_Init(void);
void Con_CheckResize(void);
void Con_DrawConsole(int lines, qboolean drawinput);
void Con_Print(const char *txt);
void Con_Printf(const char *fmt, ...);
void Con_DPrintf(const char *fmt, ...);
void Con_SafePrintf(const char *fmt, ...);
void Con_Clear_f(void);
void Con_DrawNotify(void);
void Con_ClearNotify(void);
void Con_ToggleConsole_f(void);

#endif // CONSOLE_H
