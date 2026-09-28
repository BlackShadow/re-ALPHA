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
// pr_edict.c -- entity dictionary

#include "quakedef.h"
#include "crc.h"
#include "eiface.h"

// entity spawnflags checked while loading the map
#define SPAWNFLAG_NOT_EASY			256
#define SPAWNFLAG_NOT_MEDIUM		512
#define SPAWNFLAG_NOT_HARD			1024
#define SPAWNFLAG_NOT_DEATHMATCH	2048

#define MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

#define HULL_CACHE_SPAN	0x10000

#if defined(_DEBUG)
#define MAX_DEFERRED_PRIVATEDATA	4096

static void *g_deferredPrivateData[MAX_DEFERRED_PRIVATEDATA];
static int g_deferredPrivateDataCount;
static int g_dispatchDepth;

static void ED_FlushDeferredPrivateData(void)
{
	while (g_deferredPrivateDataCount > 0)
		free(g_deferredPrivateData[--g_deferredPrivateDataCount]);
}

void ED_DispatchEnter(void)
{
	++g_dispatchDepth;
}

void ED_DispatchExit(void)
{
	if (g_dispatchDepth > 0)
		--g_dispatchDepth;

	if (g_dispatchDepth == 0)
		ED_FlushDeferredPrivateData();
}
#endif

unsigned short pr_crc;

int type_size[8] = {1, sizeof(string_t) / 4, 1, 3, 1, 1, sizeof(func_t) / 4, sizeof(void *) / 4};

typedef struct
{
	ddef_t	*pcache;
	char	field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache gefvCache[GEFV_CACHESIZE];

cvar_t nomonsters = {"nomonsters", "0"};
cvar_t gamecfg = {"gamecfg", "0"};
cvar_t scratch1 = {"scratch1", "0"};
cvar_t scratch2 = {"scratch2", "0"};
cvar_t scratch3 = {"scratch3", "0"};
cvar_t scratch4 = {"scratch4", "0"};
cvar_t savedgamecfg = {"savedgamecfg", "0", true};
cvar_t saved1 = {"saved1", "0", true};
cvar_t saved2 = {"saved2", "0", true};
cvar_t saved3 = {"saved3", "0", true};
cvar_t saved4 = {"saved4", "0", true};

int gefv_cache_index = 0;

ddef_t *ED_FindField(const char *name);
ddef_t *ED_FindGlobal(const char *name);
dfunction_t *ED_FindFunction(const char *name);
qboolean ED_ParseEpair(void *base, ddef_t *key, char *s);

edict_t *EDICT_NUM(int n)
{
	if (n < 0 || n >= sv_max_edicts)
		Sys_Error("EDICT_NUM: bad number %i", n);
	return (edict_t *)((byte *)sv_edicts + n * pr_edict_size);
}

int NUM_FOR_EDICT(edict_t *e)
{
	int b;

	b = (byte *)e - (byte *)sv_edicts;
	b = b / pr_edict_size;

	if (b < 0 || b >= *sv_num_edicts)
		Sys_Error("NUM_FOR_EDICT: bad pointer");
	return b;
}

edict_t *PROG_TO_EDICT(int e)
{
	return (edict_t *)((byte *)sv_edicts + e);
}

int EDICT_TO_PROG(edict_t *e)
{
	return (byte *)e - (byte *)sv_edicts;
}

int EDICT_INDEX(int offset)
{
	return offset / pr_edict_size;
}

/*
=================
ED_AllocPrivateData

Replaces the game DLL object attached to the edict with a zeroed block of size bytes.
=================
*/
void *ED_AllocPrivateData(edict_t *ent, int size)
{
	void *result;

	if (!ent)
		return NULL;

	ED_FreePrivateData(ent);

	if (size <= 0)
		return NULL;

	result = calloc(1, size);
	ent->pvPrivateData = result;
	return result;
}

void *ED_GetPrivateData(edict_t *ent)
{
	if (!ent)
		return NULL;
	return ent->pvPrivateData;
}

/*
=================
ED_FreePrivateData

Debug builds keep the block alive until the current game DLL dispatch returns.
=================
*/
void ED_FreePrivateData(edict_t *ent)
{
	void *block;

	if (!ent)
		return;

	block = ent->pvPrivateData;
	if (block)
	{
#if defined(_DEBUG)
		if (g_dispatchDepth > 0)
		{
			if (g_deferredPrivateDataCount < MAX_DEFERRED_PRIVATEDATA)
				g_deferredPrivateData[g_deferredPrivateDataCount++] = block;
			else
				free(block);
		}
		else
#endif
		{
			free(block);
		}
	}
	ent->pvPrivateData = NULL;
}

/*
=============
ED_NewString

Copies the string onto the hunk, converting "\n" escapes.
=============
*/
char *ED_NewString(const char *string)
{
	char	*new_str;
	int		i, j, l;
	char	c;

	l = strlen(string) + 1;
	new_str = Hunk_Alloc(l);

	for (i = 0, j = 0; l > i; i++)
	{
		c = string[i];
		if (c == '\\' && l - 1 > i)
		{
			i++;
			if (string[i] == 'n')
				new_str[j] = '\n';
			else
				new_str[j] = '\\';
		}
		else
		{
			new_str[j] = c;
		}
		j++;
	}
	return new_str;
}

/*
=============
ED_EntityValueIsRawInt

Entity fields that hold plain numbers instead of edict offsets.
=============
*/
static qboolean ED_EntityValueIsRawInt(const ddef_t *def)
{
	const char *name;

	if (!def)
		return false;

	name = pr_strings + def->s_name;
	if (!name || !name[0])
		return false;

	if (!strcmp(name, "weapon") ||
		!strcmp(name, "weapons") ||
		!strcmp(name, "ammo_1") ||
		!strcmp(name, "ammo_2") ||
		!strcmp(name, "ammo_3") ||
		!strcmp(name, "ammo_4") ||
		!strcmp(name, "items") ||
		!strcmp(name, "items2") ||
		!strcmp(name, "sequence") ||
		!strcmp(name, "controller") ||
		!strcmp(name, "blending") ||
		!strcmp(name, "button"))
	{
		return true;
	}

	return false;
}

/*
=============
ED_ParseEpair

Can parse either fields or globals
returns false if error
=============
*/
qboolean ED_ParseEpair(void *base, ddef_t *key, char *s)
{
	int			*d;
	int			type;
	char		string[128];
	char		*v, *w;
	int			i;
	ddef_t		*def;
	dfunction_t	*func;
	char		*end, *end2;
	long		dll_index, value;
	unsigned long offset;

	d = (int *)base + key->ofs;

	type = key->type;
	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		*d = ED_NewString(s) - pr_strings;
		return true;

	case ev_float:
		*(float *)d = (float)atof(s);
		return true;

	case ev_vector:
		strcpy(string, s);
		v = string;
		w = string;
		for (i = 0; i < 3; i++)
		{
			while (*v && *v != ' ')
				v++;
			*v++ = 0;
			((float *)d)[i] = (float)atof(w);
			w = v;
		}
		return true;

	case ev_entity:
		i = atoi(s);
		if (ED_EntityValueIsRawInt(key))
			*d = i;
		else
			*d = (byte *)EDICT_NUM(i) - (byte *)sv_edicts;
		return true;

	case ev_field:
		def = ED_FindField(s);
		if (!def)
		{
			Con_Printf("Can't find field %s\n", s);
			return false;
		}
		*d = G_INT(def->ofs);
		return true;

	case ev_function:
		if (g_iextdllcount > 0)
		{
			// with a game DLL loaded, function fields hold code addresses:
			// "dll:<index>:<offset>" or a plain number
			if (!strncmp(s, "dll:", 4))
			{
				end = NULL;
				dll_index = strtol(s + 4, &end, 10);
				if (end && *end == ':')
				{
					end2 = NULL;
					offset = strtoul(end + 1, &end2, 10);
					if (end2 && !*end2 && dll_index >= 0 && dll_index < g_iextdllcount)
					{
						*d = (int)((byte *)g_rgextdll[dll_index] + offset);
						return true;
					}
				}
			}

			end = NULL;
			value = strtol(s, &end, 10);
			if (end && !*end)
			{
				*d = (int)value;
				return true;
			}
		}

		func = ED_FindFunction(s);
		if (!func)
		{
			if (g_iextdllcount > 0)
			{
				*d = 0;
				return true;
			}
			Con_Printf("Can't find function %s\n", s);
			return false;
		}
		*d = ((byte *)func - (byte *)pr_functions) / (int)sizeof(dfunction_t);
		return true;
	}

	return true;
}

/*
=============
ED_SuckOutClassname

Parses only the classname key of an entity block into ent.
=============
*/
qboolean ED_SuckOutClassname(char *data, edict_t *ent)
{
	char	keyname[256];
	ddef_t	*key;

	while (1)
	{
		data = COM_Parse(data);
		if (com_token[0] == '}')
			break;

		strcpy(keyname, com_token);
		data = COM_Parse(data);

		if (!strcmp(keyname, "classname"))
		{
			key = ED_FindField(keyname);
			if (!key)
				Host_Error("SuckOutClassname: classname not found in field table");
			if (!ED_ParseEpair(&ent->v, key, com_token))
				Host_Error("SuckOutClassname: parse error");
			return true;
		}
	}

	return false;
}

/*
=============
ED_SetEdictPointers

Links the entvars back to their edict and the progs globals for the game DLL.
=============
*/
edict_t *ED_SetEdictPointers(edict_t *e)
{
	e->v.pContainingEntity = e;
	e->v.pSystemGlobals = pr_global_struct;
	return e;
}

void ED_FreePrivateDataWrapper(edict_t *ed)
{
	ED_FreePrivateData(ed);
}

void *ED_GetDispatchFunction(void *ent, int callback_type)
{
	switch (callback_type)
	{
	case DISPATCH_SPAWN:
		return g_pfnDispatchSpawn;
	case DISPATCH_THINK:
		return g_pfnDispatchThink;
	case DISPATCH_TOUCH:
		return g_pfnDispatchTouch;
	case DISPATCH_USE:
		return g_pfnDispatchUse;
	case DISPATCH_BLOCKED:
		return g_pfnDispatchBlocked;
	case DISPATCH_KEYVALUE:
		return g_pfnDispatchKeyValue;
	case DISPATCH_SAVE:
		return g_pfnDispatchSave;
	}
	return NULL;
}

/*
=============
ED_CallDispatch

Calls a game DLL entity function with the edict's entvars.
=============
*/
void *ED_CallDispatch(void *ent, int callback_type, void *param)
{
	void	*(*func)(void *, void *);
	void	*result;

	func = (void *(*)(void *, void *))ED_GetDispatchFunction(ent, callback_type);
	if (func)
	{
#if defined(_DEBUG)
		ED_DispatchEnter();
#endif
		result = func(&((edict_t *)ent)->v, param);
#if defined(_DEBUG)
		ED_DispatchExit();
#endif
		return result;
	}

	if (callback_type == DISPATCH_SPAWN)
		Con_Printf("ASSERT FAILURE: entity method (spawn) is null\n");

	return NULL;
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edict
=============
*/
void ED_PrintEdict_f(void)
{
	int i;

	i = atoi(Cmd_Argv(1));
	ED_PrintNum(i);
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts(void)
{
	int i;

	Con_Printf("%i entities\n", *sv_num_edicts);
	for (i = 0; i < *sv_num_edicts; i++)
		ED_Print(EDICT_NUM(i));
}

/*
=============
ED_Count

For debugging
=============
*/
void ED_Count(void)
{
	int		i;
	edict_t	*ent;
	int		active, models, solid, step;

	active = 0;
	models = 0;
	solid = 0;
	step = 0;

	for (i = 0; i < *sv_num_edicts; i++)
	{
		ent = EDICT_NUM(i);
		if (ent->free)
			continue;

		active++;
		if (ent->v.solid)
			solid++;
		if (ent->v.model)
			models++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Con_Printf("num_edicts:%3i\n", *sv_num_edicts);
	Con_Printf("active    :%3i\n", active);
	Con_Printf("view      :%3i\n", models);
	Con_Printf("touch     :%3i\n", solid);
	Con_Printf("step      :%3i\n", step);
}

/*
===============
ED_Init
===============
*/
void ED_Init(void)
{
	Cmd_AddCommand("edict", ED_PrintEdict_f);
	Cmd_AddCommand("edicts", ED_PrintEdicts);
	Cmd_AddCommand("edictcount", ED_Count);
	Cmd_AddCommand("profile", PR_Profile_f);

	Cvar_RegisterVariable(&nomonsters);
	Cvar_RegisterVariable(&gamecfg);
	Cvar_RegisterVariable(&scratch1);
	Cvar_RegisterVariable(&scratch2);
	Cvar_RegisterVariable(&scratch3);
	Cvar_RegisterVariable(&scratch4);
	Cvar_RegisterVariable(&savedgamecfg);
	Cvar_RegisterVariable(&saved1);
	Cvar_RegisterVariable(&saved2);
	Cvar_RegisterVariable(&saved3);
	Cvar_RegisterVariable(&saved4);
}

/*
============
PR_Profile_f

Prints the ten most expensive progs functions and clears all counters.
============
*/
void PR_Profile_f(void)
{
	dfunction_t	*f, *best;
	int			max;
	int			num;
	int			i, c;

	for (num = 0; ; num++)
	{
		max = 0;
		best = NULL;
		f = pr_functions;
		c = ((dprograms_t *)progs)->numfunctions;

		for (i = 0; i < c; i++, f++)
		{
			if (max < f->profile)
			{
				max = f->profile;
				best = f;
			}
		}

		if (!best)
			break;

		if (num < 10)
			Con_Printf("%7i %s\n", best->profile, pr_strings + best->s_name);

		best->profile = 0;
	}
}

/*
=============
ED_WriteGlobals

Writes the savable string, float and entity globals.
=============
*/
void ED_WriteGlobals(savebuf_t *sb)
{
	ddef_t	*def;
	int		i;
	int		numglobaldefs;
	char	*name;
	float	*val;
	int		type;

	Savegame_WriteInt(sb, 0);

	numglobaldefs = ((dprograms_t *)progs)->numglobaldefs;
	def = pr_globaldefs;
	for (i = 0; i < numglobaldefs; i++, def++)
	{
		type = def->type;
		if (!(type & DEF_SAVEGLOBAL))
			continue;

		type &= ~DEF_SAVEGLOBAL;
		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = pr_strings + def->s_name;
		val = &pr_globals[def->ofs];
		Savegame_WriteString(sb, name);
		Savegame_WriteString(sb, PR_UglyValueString(type, val));
	}
}

/*
=============
ED_ParseGlobals
=============
*/
void ED_ParseGlobals(char *data)
{
	char	keyname[64];
	ddef_t	*key;

	while (1)
	{
		// parse key
		data = COM_Parse(data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		strcpy(keyname, com_token);

		// parse value
		data = COM_Parse(data);
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error("ED_ParseEntity: closing brace without data");

		key = ED_FindGlobal(keyname);
		if (key)
		{
			if (!ED_ParseEpair(pr_globals, key, com_token))
				Host_Error("ED_ParseGlobals: parse error");
		}
		else
			Con_Printf("'%s' is not a global\n", keyname);
	}
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Keys the progs fields don't know go to the game DLL's KeyValue.
====================
*/
char *ED_ParseEdict(char *data, edict_t *ent)
{
	char		*classname;
	void		(*spawn)(void *, int);
	qboolean	init;
	ddef_t		*key;
	char		keyname[256];
	char		temp[32];
	int			n;
	KeyValueData kvd;

	init = false;

	// clear it
	if (ent != sv_edicts)	// hack
		memset(&ent->v, 0, ((dprograms_t *)progs)->entityfields * 4);

	ED_SetEdictPointers(ent);
	ED_SuckOutClassname(data, ent);

	// let the game DLL create its object for this class
	classname = pr_strings + ent->v.classname;
	spawn = (void (*)(void *, int))GetEntityInit(classname);
	if (spawn)
		spawn(&ent->v, 0);
	else
		Con_Printf("Can't init %s\n", classname);

	// go through all the dictionary pairs
	while (1)
	{
		// parse key
		data = COM_Parse(data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		strcpy(keyname, com_token);

		// another hack to fix keynames with trailing spaces
		n = strlen(keyname);
		while (n > 0 && keyname[n - 1] == ' ')
			keyname[--n] = 0;

		// parse value
		data = COM_Parse(data);
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error("ED_ParseEntity: closing brace without data");

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		classname = pr_strings + ent->v.classname;
		if (classname && !strcmp(classname, com_token))
			continue;

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp(keyname, "angle"))
		{
			strcpy(temp, com_token);
			sprintf(com_token, "0 %s 0", temp);
			strcpy(keyname, "angles");
		}

		key = ED_FindField(keyname);
		if (key)
		{
			if (!ED_ParseEpair(&ent->v, key, com_token))
				Host_Error("ED_ParseEdict: parse error");
		}
		else
		{
			// not a progs field, give it to the game DLL
			kvd.szClassName = classname;
			kvd.szKeyName = keyname;
			kvd.szValue = com_token;
			kvd.fHandled = false;
			ED_CallDispatch(ent, DISPATCH_KEYVALUE, &kvd);
		}
	}

	if (!init)
		ent->free = true;

	return data;
}

/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
================
*/
void ED_LoadFromFile(char *data)
{
	edict_t	*ent;
	int		inhibit;
	int		spawnflags;

	ent = NULL;
	inhibit = 0;
	pr_global_struct->time = sv.time;

	// parse ents
	while (1)
	{
		// parse the opening brace
		data = COM_Parse(data);
		if (!data)
		{
			Con_DPrintf("%i entities inhibited\n", inhibit);
			return;
		}

		if (com_token[0] != '{')
			Sys_Error("ED_LoadFromFile: found %s when expecting {", com_token);

		if (ent)
			ent = ED_Alloc();
		else
		{
			ent = EDICT_NUM(0);
			ED_FreePrivateDataWrapper(ent);
			ED_SetEdictPointers(ent);
		}

		data = ED_ParseEdict(data, ent);

		// remove things from different skill levels or deathmatch
		spawnflags = (int)ent->v.spawnflags;
		if (deathmatch.value == 0)
		{
			if ((current_skill == 0 && (spawnflags & SPAWNFLAG_NOT_EASY))
				|| (current_skill == 1 && (spawnflags & SPAWNFLAG_NOT_MEDIUM))
				|| (current_skill >= 2 && (spawnflags & SPAWNFLAG_NOT_HARD)))
			{
				inhibit++;
				ED_Free(ent);
				continue;
			}
		}
		else if (spawnflags & SPAWNFLAG_NOT_DEATHMATCH)
		{
			inhibit++;
			ED_Free(ent);
			continue;
		}

		// immediately call spawn function
		if (ent->v.classname)
		{
			pr_global_struct->self = EDICT_TO_PROG(ent);
			ED_CallDispatch(ent, DISPATCH_SPAWN, NULL);
		}
		else
		{
			Con_Printf("No classname for:\n");
			ED_Print(ent);
			ED_Free(ent);
		}
	}
}

/*
===============
PR_LoadProgs
===============
*/
void PR_LoadProgs(void)
{
	int				i;
	byte			*data;
	dprograms_t		*header;
	dstatement_t	*st;
	dfunction_t		*func;
	ddef_t			*def;

	// flush the non-C variable lookup cache
	for (i = 0; i < GEFV_CACHESIZE; i++)
		gefvCache[i].field[0] = 0;

	CRC_Init(&pr_crc);

	progs = COM_LoadHunkFile("progs.dat");
	if (!progs)
		Sys_Error("PR_LoadProgs: couldn't load progs.dat");
	Con_DPrintf("Programs occupy %iK.\n", com_filesize / 1024);

	data = progs;
	for (i = 0; i < com_filesize; i++)
		CRC_ProcessByte(&pr_crc, data[i]);

	// byte swap the header
	header = progs;
	for (i = 0; i < sizeof(*header) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	if (header->version != PROG_VERSION)
		Sys_Error("progs.dat has wrong version number (%i should be %i)", header->version, PROG_VERSION);
	if (header->crc != PROGHEADER_CRC)
		Sys_Error("progs.dat system vars have been modified, progdefs.h is out of date");

	pr_functions = (byte *)progs + header->ofs_functions;
	pr_globaldefs = (byte *)progs + header->ofs_globaldefs;
	pr_statements = (byte *)progs + header->ofs_statements;
	pr_strings = (char *)progs + header->ofs_strings;
	pr_fielddefs = (byte *)progs + header->ofs_fielddefs;
	pr_global_struct = (globalvars_t *)((byte *)progs + header->ofs_globals);
	pr_globals = (float *)pr_global_struct;

	pr_edict_size = header->entityfields * 4 + offsetof(edict_t, v);

	// byte swap the lumps
	for (i = 0; i < header->numstatements; i++)
	{
		st = (dstatement_t *)pr_statements + i;
		st->op = LittleShort(st->op);
		st->a = LittleShort(st->a);
		st->b = LittleShort(st->b);
		st->c = LittleShort(st->c);
	}

	for (i = 0; i < header->numfunctions; i++)
	{
		func = (dfunction_t *)pr_functions + i;
		func->first_statement = LittleLong(func->first_statement);
		func->parm_start = LittleLong(func->parm_start);
		func->locals = LittleLong(func->locals);
		func->s_name = LittleLong(func->s_name);
		func->s_file = LittleLong(func->s_file);
		func->numparms = LittleLong(func->numparms);
	}

	for (i = 0; i < header->numglobaldefs; i++)
	{
		def = (ddef_t *)pr_globaldefs + i;
		def->type = LittleShort(def->type);
		def->ofs = LittleShort(def->ofs);
		def->s_name = LittleLong(def->s_name);
	}

	for (i = 0; i < header->numfielddefs; i++)
	{
		def = (ddef_t *)pr_fielddefs + i;
		def->type = LittleShort(def->type);
		if (def->type & DEF_SAVEGLOBAL)
			Sys_Error("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		def->ofs = LittleShort(def->ofs);
		def->s_name = LittleLong(def->s_name);
	}

	for (i = 0; i < header->numglobals; i++)
		((int *)pr_globals)[i] = LittleLong(((int *)pr_globals)[i]);
}

/*
============
ED_FindField
============
*/
ddef_t *ED_FindField(const char *name)
{
	ddef_t	*def;
	int		i;
	int		numfielddefs;

	numfielddefs = ((dprograms_t *)progs)->numfielddefs;
	def = pr_fielddefs;
	for (i = 0; i < numfielddefs; i++, def++)
	{
		if (!strcmp(pr_strings + def->s_name, name))
			return def;
	}
	return NULL;
}

/*
============
ED_FindGlobal
============
*/
ddef_t *ED_FindGlobal(const char *name)
{
	ddef_t	*def;
	int		i;
	int		numglobaldefs;

	numglobaldefs = ((dprograms_t *)progs)->numglobaldefs;
	def = pr_globaldefs;
	for (i = 0; i < numglobaldefs; i++, def++)
	{
		if (!strcmp(pr_strings + def->s_name, name))
			return def;
	}
	return NULL;
}

/*
============
ED_FindFunction
============
*/
dfunction_t *ED_FindFunction(const char *name)
{
	dfunction_t	*func;
	int			i;
	int			numfunctions;

	numfunctions = ((dprograms_t *)progs)->numfunctions;
	func = pr_functions;
	for (i = 0; i < numfunctions; i++, func++)
	{
		if (!strcmp(pr_strings + func->s_name, name))
			return func;
	}
	return NULL;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *ED_Alloc(void)
{
	int		i;
	edict_t	*e;

	for (i = svs.maxclients + 1; i < *sv_num_edicts; i++)
	{
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && (e->freetime < 2 || (float)sv.time - e->freetime > 0.5f))
		{
			ED_ClearEdict(e);
			return e;
		}
	}

	if (i == MAX_EDICTS)
		Sys_Error("ED_Alloc: no free edicts");

	(*sv_num_edicts)++;
	e = EDICT_NUM(i);
	ED_ClearEdict(e);

	return e;
}

/*
=================
ED_ClearEdict

Sets everything to NULL
=================
*/
edict_t *ED_ClearEdict(edict_t *e)
{
	memset(&e->v, 0, ((dprograms_t *)progs)->entityfields * 4);
	e->free = false;
	ED_FreePrivateData(e);
	return ED_SetEdictPointers(e);
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free(edict_t *ed)
{
	SV_UnlinkEdict(ed);		// unlink from world bsp
	ED_FreePrivateData(ed);

	ed->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorClear(ed->v.origin);
	VectorClear(ed->v.angles);
	ed->v.solid = 0;
	ed->v.nextthink = -1;

	ed->freetime = sv.time;
}

/*
============
ED_GlobalAtOfs
============
*/
ddef_t *ED_GlobalAtOfs(int ofs)
{
	ddef_t	*def;
	int		i;
	int		numglobaldefs;

	numglobaldefs = ((dprograms_t *)progs)->numglobaldefs;
	def = pr_globaldefs;
	for (i = 0; i < numglobaldefs; i++, def++)
	{
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
ED_FieldAtOfs
============
*/
ddef_t *ED_FieldAtOfs(int ofs)
{
	ddef_t	*def;
	int		i;
	int		numfielddefs;

	numfielddefs = ((dprograms_t *)progs)->numfielddefs;
	def = pr_fielddefs;
	for (i = 0; i < numfielddefs; i++, def++)
	{
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
GetEdictFieldValue

Looks up a progs field by name, caching the last two lookups.
============
*/
void *GetEdictFieldValue(void *ent, const char *field)
{
	ddef_t	*def;
	int		i;

	for (i = 0; i < GEFV_CACHESIZE; i++)
	{
		if (!strcmp(field, gefvCache[i].field))
		{
			def = gefvCache[i].pcache;
			goto Done;
		}
	}

	def = ED_FindField(field);

	if (strlen(field) < MAX_FIELD_LEN)
	{
		gefvCache[gefv_cache_index].pcache = def;
		strcpy(gefvCache[gefv_cache_index].field, field);
		gefv_cache_index ^= 1;
	}

Done:
	if (!def)
		return NULL;

	return (int *)&((edict_t *)ent)->v + def->ofs;
}

/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PR_ValueString(int type, void *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_void:
		sprintf(line, "void");
		break;
	case ev_string:
		sprintf(line, "%s", pr_strings + *(string_t *)val);
		break;
	case ev_float:
		sprintf(line, "%5.1f", *(float *)val);
		break;
	case ev_vector:
		sprintf(line, "'%5.1f %5.1f %5.1f'", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2]);
		break;
	case ev_entity:
		sprintf(line, "entity %i", NUM_FOR_EDICT(PROG_TO_EDICT(*(int *)val)));
		break;
	case ev_field:
		def = ED_FieldAtOfs(*(int *)val);
		sprintf(line, ".%s", pr_strings + def->s_name);
		break;
	case ev_function:
		f = (dfunction_t *)pr_functions + *(func_t *)val;
		sprintf(line, "%s()", pr_strings + f->s_name);
		break;
	case ev_pointer:
		sprintf(line, "pointer");
		break;
	default:
		sprintf(line, "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PR_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *PR_UglyValueString(int type, void *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;

	type &= ~DEF_SAVEGLOBAL;

	if (type == ev_void)
	{
		sprintf(line, "void");
		return line;
	}

	switch (type)
	{
	case ev_string:
		sprintf(line, "%s", pr_strings + *(string_t *)val);
		break;
	case ev_float:
		sprintf(line, "%f", *(float *)val);
		break;
	case ev_vector:
		sprintf(line, "%f %f %f", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2]);
		break;
	case ev_entity:
		sprintf(line, "%i", NUM_FOR_EDICT(PROG_TO_EDICT(*(int *)val)));
		break;
	case ev_field:
		def = ED_FieldAtOfs(*(int *)val);
		sprintf(line, "%s", pr_strings + def->s_name);
		break;
	case ev_function:
		f = (dfunction_t *)pr_functions + *(func_t *)val;
		sprintf(line, "%s", pr_strings + f->s_name);
		break;
	default:
		sprintf(line, "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *PR_GlobalString(int ofs)
{
	char		*s;
	int			i;
	ddef_t		*def;
	void		*val;
	static char	line[128];

	val = &pr_globals[ofs];
	def = ED_GlobalAtOfs(ofs);
	if (def)
	{
		s = PR_ValueString(def->type, val);
		sprintf(line, "%i(%s)%s", ofs, pr_strings + def->s_name, s);
	}
	else
		sprintf(line, "%i(?]", ofs);

	i = strlen(line);
	for (; i < 20; i++)
		strcat(line, " ");
	strcat(line, " ");

	return line;
}

char *PR_GlobalStringNoContents(int ofs)
{
	int			i;
	ddef_t		*def;
	static char	line[128];

	def = ED_GlobalAtOfs(ofs);
	if (def)
		sprintf(line, "%i(%s)", ofs, pr_strings + def->s_name);
	else
		sprintf(line, "%i(?]", ofs);

	i = strlen(line);
	for (; i < 20; i++)
		strcat(line, " ");
	strcat(line, " ");

	return line;
}

/*
=============
ED_Print

For debugging
=============
*/
void ED_Print(edict_t *ed)
{
	int		l;
	ddef_t	*d;
	int		*v;
	int		i, j;
	char	*name;
	int		type;
	int		numfielddefs;

	if (ed->free)
	{
		Con_Printf("FREE\n");
		return;
	}

	Con_Printf("EDICT %i:\n", NUM_FOR_EDICT(ed));

	numfielddefs = ((dprograms_t *)progs)->numfielddefs;
	for (i = 1; i < numfielddefs; i++)
	{
		d = (ddef_t *)pr_fielddefs + i;
		name = pr_strings + d->s_name;
		l = strlen(name);
		if (l >= 2 && name[l - 2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)&ed->v + d->ofs;

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		Con_Printf("%s", name);
		l = strlen(name);
		while (l < 15)
		{
			Con_Printf(" ");
			l++;
		}

		Con_Printf("%s\n", PR_ValueString(d->type, v));
	}
}

/*
=============
ED_Write

Writes the non-zero fields of an edict, then lets the game DLL save its own data.
=============
*/
void ED_Write(savebuf_t *sb, edict_t *ed)
{
	ddef_t	*d;
	int		*v;
	int		i, j;
	char	*name;
	int		type;
	int		l;
	int		numfielddefs;

	Savegame_WriteInt(sb, 0);

	if (ed->free)
		return;

	numfielddefs = ((dprograms_t *)progs)->numfielddefs;
	for (i = 1; i < numfielddefs; i++)
	{
		d = (ddef_t *)pr_fielddefs + i;
		name = pr_strings + d->s_name;
		l = strlen(name);
		if (l >= 2 && name[l - 2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)&ed->v + d->ofs;

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		Savegame_WriteString(sb, name);
		Savegame_WriteString(sb, PR_UglyValueString(type, v));
	}

	ED_CallDispatch(ed, DISPATCH_SAVE, sb);
}

void ED_PrintNum(int ent)
{
	ED_Print(EDICT_NUM(ent));
}

char *PROG_TO_STRING(int offset)
{
	return pr_strings + offset;
}

void *EDICT_TO_ENTVAR(edict_t *ent)
{
	return &ent->v;
}

edict_t *ENTVAR_TO_EDICT(void *entvar)
{
	int		i;
	edict_t	*ent;

	for (i = 0; i < *sv_num_edicts; i++)
	{
		ent = EDICT_NUM(i);
		if (&ent->v == entvar)
			return ent;
	}
	return NULL;
}

void *ED_GetDispatch(void *ent, int callback_type)
{
	return ED_GetDispatchFunction(ent, callback_type);
}

void ED_AssertMethodNotChanged(void)
{
	Sys_Error("CAN'T CHANGE METHODS HERE!\n");
}

float ED_GetCvarValue(const char *cvarname)
{
	return Cvar_VariableValue(cvarname);
}

char *ED_GetCvarString(const char *cvarname)
{
	return Cvar_VariableString(cvarname);
}

void ED_SetCvarValue(const char *cvarname, float value)
{
	Cvar_SetValue(cvarname, value);
}

void ED_SetCvarString(const char *cvarname, const char *value)
{
	Cvar_Set(cvarname, value);
}

int ED_AllocString(const char *string)
{
	return ED_NewString(string) - pr_strings;
}

/*
=============
ED_EntVarsToCl

Saves the progs parms into the client's spawn parms.
=============
*/
void ED_EntVarsToCl(edict_t *ent)
{
	int i;

	i = NUM_FOR_EDICT(ent);
	if (i < 1 || svs.maxclients < i)
		PR_RunError("Entity is not a client");

	memcpy(svs.clients[i - 1].spawn_parms, &pr_global_struct->parm1, sizeof(svs.clients[i - 1].spawn_parms));
}

/*
=============
ED_FindModel

Returns the cached model data (studio header) of the entity's model.
=============
*/
void *ED_FindModel(edict_t *ent)
{
	int		modelindex;
	model_t	*model;

	if (!ent)
		return NULL;

	modelindex = (int)ent->v.modelindex;
	if (modelindex <= 0 || modelindex >= MAX_MODELS)
		return NULL;

	model = sv.models[modelindex];
	if (!model)
		return NULL;

	return Mod_Extradata(model);
}

/*
=============
ED_UpdateHullCache

Adds the ints of base, and the ints HULL_CACHE_SPAN bytes further on, into
g_deltaHullCacheChecksum; the whole pass is done four times.
=============
*/
int ED_UpdateHullCache(void *base, int size)
{
	int	end;
	int	passes;
	int	i;
	int	result;

	end = size - HULL_CACHE_SPAN;
	passes = 4;
	do
	{
		for (i = 0; i < end; i += 4)
		{
			g_deltaHullCacheChecksum += *(int *)((byte *)base + i);
			result = *(int *)((byte *)base + i + HULL_CACHE_SPAN);
			g_deltaHullCacheChecksum += result;
		}
	} while (--passes);

	return result;
}

/*
=============
ED_AllocEngineHandle

Returns the 1-based number of the first free slot of g_engineHandleTable.
=============
*/
int ED_AllocEngineHandle(void)
{
	int	handle;
	int	*p;

	handle = 1;
	p = g_engineHandleTable;

	while (*p)
	{
		p++;
		handle++;
		if (p >= g_engineHandleTable + sizeof(g_engineHandleTable) / sizeof(g_engineHandleTable[0]))
			Sys_Error("out of handles");
	}

	return handle;
}
