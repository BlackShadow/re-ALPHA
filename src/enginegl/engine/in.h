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
// in.h -- external (non-keyboard) input devices

#ifndef IN_H
#define IN_H

#include "usercmd.h"

extern cvar_t	sensitivity;
extern cvar_t	m_pitch;
extern cvar_t	lookspring;
extern cvar_t	lookstrafe;

void IN_RegisterCvars(void);
void IN_Shutdown(void);

// add additional movement on top of the keyboard move cmd
void IN_Move(usercmd_t *cmd);

// accumulate mouse movement between frames
void IN_Accumulate(void);

extern int	cam_mousemove;		// the mouse is steering the third person camera

#endif // IN_H
