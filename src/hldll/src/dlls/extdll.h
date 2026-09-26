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
#ifndef EXTDLL_H
#define EXTDLL_H

//
// Global header file for extension DLLs
//

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef int BOOL;
typedef unsigned long DWORD;
typedef void *HINSTANCE;
typedef void *LPVOID;

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

// Exports are listed in hl.def
#define DLLEXPORT

#ifndef WINAPI
#define WINAPI	__stdcall
#endif

// Shared engine/DLL constants
#include "../public/const.h"

// Vector class
#include "../public/vector.h"

// Shared engine/DLL interface
#include "../public/progdefs.h"
#include "../public/edict.h"
#include "../public/eiface.h"

#endif // EXTDLL_H
