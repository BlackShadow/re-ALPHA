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

// cvar.c -- dynamic variable tracking

#include "quakedef.h"

cvar_t	*cvar_vars;
char	*cvar_null_string = "";

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar(const char *var_name)
{
	cvar_t	*var;

	for (var = cvar_vars; var; var = var->next)
	{
		if (!Q_strcmp(var_name, var->name))
			return var;
	}

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue(const char *var_name)
{
	cvar_t	*var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return 0.0f;

	return Q_atof(var->string);
}

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString(const char *var_name)
{
	cvar_t	*var;

	var = Cvar_FindVar(var_name);
	if (!var)
		return cvar_null_string;

	return var->string;
}

/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable(const char *partial)
{
	cvar_t	*cvar;
	int		len;

	len = Q_strlen(partial);
	if (!len)
		return NULL;

	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if (!Q_strncmp(partial, cvar->name, len))
			return cvar->name;
	}

	return NULL;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set(const char *var_name, const char *value)
{
	cvar_t		*var;
	qboolean	changed;

	var = Cvar_FindVar(var_name);
	if (!var)
	{
		Con_Printf("Cvar_Set: variable %s not found\n", var_name);
		return;
	}

	changed = Q_strcmp(var->string, value);

	Z_Free(var->string);

	var->string = Z_Malloc(Q_strlen(value) + 1);
	Q_strcpy(var->string, value);
	var->value = Q_atof(var->string);

	if (var->server && changed)
	{
		if (sv.active)
			SV_BroadcastPrintf("\"%s\" changed to \"%s\"\n", var->name, var->string);
	}
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue(const char *var_name, float value)
{
	char	val[32];

	sprintf(val, "%f", value);
	Cvar_Set(var_name, val);
}

/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable(cvar_t *variable)
{
	char	*oldstr;

	// first check to see if it has allready been defined
	if (Cvar_FindVar(variable->name))
	{
		Con_Printf("Can't register variable %s, allready defined\n", variable->name);
		return;
	}

	// check for overlap with a command
	if (Cmd_Exists(variable->name))
	{
		Con_Printf("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

	// copy the value off, because future sets will Z_Free it
	oldstr = variable->string;
	variable->string = Z_Malloc(Q_strlen(variable->string) + 1);
	Q_strcpy(variable->string, oldstr);
	variable->value = Q_atof(variable->string);

	// link the variable in
	variable->next = cvar_vars;
	cvar_vars = variable;
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command(void)
{
	cvar_t	*v;

	// check variables
	v = Cvar_FindVar(Cmd_Argv(0));
	if (!v)
		return false;

	// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set(v->name, Cmd_Argv(1));
	return true;
}

/*
============
Cvar_WriteVariables

Writes lines containing "variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables(FILE *f)
{
	cvar_t	*var;

	for (var = cvar_vars; var; var = var->next)
	{
		if (var->archive)
			fprintf(f, "%s \"%s\"\n", var->name, var->string);
	}
}

/*
============
Cvar_Set_f
============
*/
static void Cvar_Set_f(void)
{
	if (Cmd_Argc() != 3)
	{
		Con_Printf("set <variable> <value>\n");
		return;
	}
	Cvar_Set(Cmd_Argv(1), Cmd_Argv(2));
}

/*
============
Cvar_Toggle_f
============
*/
static void Cvar_Toggle_f(void)
{
	int		v;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("toggle <variable>\n");
		return;
	}

	v = Cvar_VariableValue(Cmd_Argv(1));
	v = !v;

	Cvar_SetValue(Cmd_Argv(1), v);
}

/*
============
Cvar_Inc_f
============
*/
static void Cvar_Inc_f(void)
{
	float	v;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("inc <variable>\n");
		return;
	}

	v = Cvar_VariableValue(Cmd_Argv(1));
	v += 1.0f;

	Cvar_SetValue(Cmd_Argv(1), v);
}

/*
============
Cvar_Init
============
*/
void Cvar_Init(void)
{
	Cmd_AddCommand("set", Cvar_Set_f);
	Cmd_AddCommand("toggle", Cvar_Toggle_f);
	Cmd_AddCommand("inc", Cvar_Inc_f);
}
