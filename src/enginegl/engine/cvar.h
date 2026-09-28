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

// cvar.h -- console variables

#ifndef CVAR_H
#define CVAR_H

typedef struct cvar_s
{
	char			*name;
	char			*string;
	qboolean		archive;	// set to true to cause it to be saved to config.cfg
	qboolean		server;		// notifies players when changed
	float			value;
	struct cvar_s	*next;
} cvar_t;

extern cvar_t	*cvar_vars;

void Cvar_Init(void);

void Cvar_RegisterVariable(cvar_t *variable);
// registers a cvar that already has the name, string, and optionally the
// archive elements set.

void Cvar_Set(const char *var_name, const char *value);
// equivalent to "<name> <variable>" typed at the console

void Cvar_SetValue(const char *var_name, float value);
// expands value to a string and calls Cvar_Set

float Cvar_VariableValue(const char *var_name);
// returns 0 if not defined or non numeric

char *Cvar_VariableString(const char *var_name);
// returns an empty string if not defined

char *Cvar_CompleteVariable(const char *partial);
// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits

qboolean Cvar_Command(void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void Cvar_WriteVariables(FILE *f);
// Writes lines containing "variable value" for all variables
// with the archive flag set to true.

cvar_t *Cvar_FindVar(const char *var_name);

#endif // CVAR_H

