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

#include "quakedef.h"
#include "crc.h"

#if defined(_DEBUG)
static void *g_deferredPrivateData[4096];
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

extern int current_skill;
extern cvar_t deathmatch;

int type_size[8] = {1, 1, 1, 3, 1, 1, 1, 1};

#define MAX_FIELD_LEN   64
#define GEFV_CACHESIZE  2

typedef struct {
    void *pcache;
    char field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache gefvCache[GEFV_CACHESIZE];

cvar_t nomonsters = {"nomonsters", "0"};
cvar_t gamecfg = {"gamecfg", "0"};
cvar_t scratch1 = {"scratch1", "0"};
cvar_t scratch2 = {"scratch2", "0"};
cvar_t scratch3 = {"scratch3", "0"};
cvar_t scratch4 = {"scratch4", "0"};
cvar_t savedgamecfg = {"savedgamecfg", "0", true, false};
cvar_t saved1 = {"saved1", "0", true, false};
cvar_t saved2 = {"saved2", "0", true, false};
cvar_t saved3 = {"saved3", "0", true, false};
cvar_t saved4 = {"saved4", "0", true, false};

extern void Cmd_AddCommand(const char *cmd_name, void (*function)(void));
extern void Cvar_RegisterVariable(cvar_t *variable);
extern void Sys_Error(const char *error, ...);
extern void Con_Printf(const char *fmt, ...);
extern void Con_DPrintf(const char *fmt, ...);
extern void Host_Error(const char *error, ...);
extern char *COM_Parse(char *data);
extern int (*LittleLong)(int l);
extern short (*LittleShort)(short s);
extern void *Hunk_Alloc(int size);
extern void *GetEntityInit(const char *classname);
extern void ED_Print(void *ed);
extern void PR_ExecuteProgram(int fnum);
extern void PR_Profile_f(void);
extern float Cvar_VariableValue(const char *var_name);
extern char *Cvar_VariableString(const char *var_name);
extern void Cvar_Set(const char *var_name, const char *value);
extern void Cvar_SetValue(const char *var_name, float value);
extern void PR_RunError(char *fmt, ...);

extern char com_token[];
extern int com_filesize;
int gefv_cache_index = 0;

ddef_t *ED_FindField(const char *name);
ddef_t *ED_FindGlobal(const char *name);
dfunction_t *ED_FindFunction(const char *name);
edict_t *ED_Alloc(void);
edict_t *ED_ClearEdict(edict_t *ed);
void ED_Free(edict_t *ed);
char *PR_UglyValueString(int type, void *val);
int ED_ParseEpair(void *base, ddef_t *key, char *s);
void ED_PrintNum(int entnum);
void *ED_CallDispatch(void *ent, int callback_type, void *param);
void ED_FreePrivateData(edict_t *ent);

edict_t *EDICT_NUM(int n)
{
    if (n < 0 || sv_max_edicts <= n)
        Sys_Error("EDICT_NUM: bad number %i", n);
    return (edict_t *)((byte *)sv_edicts + n * pr_edict_size);
}

int NUM_FOR_EDICT(void *e)
{
    int b;

    b = (int)(((byte *)e - (byte *)sv_edicts) / pr_edict_size);

    if (b < 0 || b >= *sv_num_edicts)
        Sys_Error("NUM_FOR_EDICT: bad pointer");
    return b;
}

edict_t *PROG_TO_EDICT(int e)
{
    return (edict_t *)((byte *)sv_edicts + e);
}

int EDICT_TO_PROG(void *e)
{
    return (int)((byte *)e - (byte *)sv_edicts);
}

int EDICT_INDEX(int offset)
{
    return offset / pr_edict_size;
}

void *ED_AllocPrivateData(edict_t *ent, int size)
{
    void *result;

	if (!ent)
		return NULL;

    ED_FreePrivateData(ent);

    if (size <= 0)
        return NULL;

    result = calloc(1, size);
    *(void **)((byte *)ent + 116) = result;
    return result;
}

void *ED_GetPrivateData(edict_t *ent)
{
	if (!ent)
		return NULL;
    return *(void **)((byte *)ent + 116);
}

void ED_FreePrivateData(edict_t *ent)
{
    void *block;

	if (!ent)
		return;

    block = *(void **)((byte *)ent + 116);
    if (block)
    {
#if defined(_DEBUG)
		if (g_dispatchDepth > 0)
		{
			if (g_deferredPrivateDataCount < (int)(sizeof(g_deferredPrivateData) / sizeof(g_deferredPrivateData[0])))
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
    *(void **)((byte *)ent + 116) = NULL;
}

char *ED_NewString(const char *string)
{
    int len;
    int i;
    int j;
    char *new_str;
    char c;

    len = strlen(string) + 1;
    new_str = (char *)Hunk_Alloc(len);

    for (i = 0, j = 0; len > i; i++)
    {
        c = string[i];
        if (c == '\\' && len - 1 > i)
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

int ED_ParseEpair(void *base, ddef_t *key, char *s)
{
    int *dest;
    ddef_t *def;
    int type;
    char temp[128];
    char *v;
    char *w;
    int i;
    float fval;
    int ival;
    ddef_t *field;
    dfunction_t *func;

    def = (ddef_t *)key;
    dest = (int *)((byte *)base + 4 * def->ofs);

    type = def->type;
    type &= ~0x8000;

    switch (type)
    {
        case 1:
            ival = (int)ED_NewString(s) - (int)pr_strings;
            *dest = ival;
            return 1;

        case 2:
            fval = (float)atof(s);
            *(float *)dest = fval;
            return 1;

        case 3:
            strcpy(temp, s);
            v = temp;
            w = temp;
            for (i = 0; i < 3; i++)
            {
                while (*v && *v != ' ')
                    v++;
                *v++ = 0;
                ((float *)dest)[i] = (float)atof(w);
                w = v;
            }
            return 1;

        case 4:
            ival = atoi(s);
            if (ED_EntityValueIsRawInt(def))
            {
                *dest = ival;
            }
            else
            {
                *dest = (int)EDICT_NUM(ival) - (int)sv_edicts;
            }
            return 1;

        case 5:
            field = ED_FindField(s);
            if (!field)
            {
                Con_Printf("Can't find field %s\n", s);
                return 0;
            }
            *dest = *(int *)((byte *)pr_globals + 4 * field->ofs);
            return 1;

        case 6:
            if (g_iextdllcount > 0)
            {
                // In game-DLL builds, entvars function slots (think/use/touch/etc.) hold raw 32-bit
                // function pointers, not QuakeC function indices. Preserve numeric values from saves.
                if (!strncmp(s, "dll:", 4))
                {
                    char *endptr;
                    char *endptr2;
                    long dll_index;
                    unsigned long offset;

                    endptr = NULL;
                    dll_index = strtol(s + 4, &endptr, 10);
                    if (endptr && *endptr == ':')
                    {
                        endptr2 = NULL;
                        offset = strtoul(endptr + 1, &endptr2, 10);
                        if (endptr2 && !*endptr2 && dll_index >= 0 && dll_index < g_iextdllcount)
                        {
                            *dest = (int)((byte *)g_rgextdll[dll_index] + offset);
                            return 1;
                        }
                    }
                }

                char *endptr;
                long lval;

                endptr = NULL;
                lval = strtol(s, &endptr, 10);
                if (endptr && !*endptr)
                {
                    *dest = (int)lval;
                    return 1;
                }
            }

            func = ED_FindFunction(s);
            if (!func)
            {
                if (g_iextdllcount > 0)
                {
                    *dest = 0;
                    return 1;
                }
                Con_Printf("Can't find function %s\n", s);
                return 0;
            }
            *dest = (int)(((byte *)func - (byte *)pr_functions) / 36);
            return 1;
    }

    return 1;
}

int ED_SuckOutClassname(char *data, edict_t *ent)
{
	char keyname[256];
	ddef_t *key;

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
			if (!ED_ParseEpair((byte *)ent + 120, key, com_token))
				Host_Error("SuckOutClassname: parse error");
			return 1;
		}
	}

	return 0;
}

edict_t *ED_SetEdictPointers(edict_t *e)
{
	*(edict_t **)((byte *)e + 640) = e;
	*(globalvars_t **)((byte *)e + 644) = pr_global_struct;
	return e;
}

void ED_FreePrivateDataWrapper(edict_t *ed)
{
	ED_FreePrivateData(ed);
}

void *ED_GetDispatchFunction(void *ent, int callback_type)
{
    extern void *g_pfnDispatchSpawn;
    extern void *g_pfnDispatchThink;
    extern void *g_pfnDispatchUse;
    extern void *g_pfnDispatchTouch;
    extern void *g_pfnDispatchBlocked;
    extern void *g_pfnDispatchKeyValue;
    extern void *g_pfnDispatchSave;

    switch (callback_type)
    {
        case 0:
            return g_pfnDispatchSpawn;
        case 1:
            return g_pfnDispatchThink;
        case 2:
            return g_pfnDispatchTouch;
        case 3:
            return g_pfnDispatchUse;
        case 4:
            return g_pfnDispatchBlocked;
        case 5:
            return g_pfnDispatchKeyValue;
        case 6:
            return g_pfnDispatchSave;
    }
    return NULL;
}

void *ED_CallDispatch(void *ent, int callback_type, void *param)
{
    typedef void *(*dispatch_fn)(void *, void *);
    dispatch_fn func;
	void *result;

    func = (dispatch_fn)ED_GetDispatchFunction(ent, callback_type);
    if (func)
	{
#if defined(_DEBUG)
		ED_DispatchEnter();
#endif
		result = func((char *)ent + 120, param);
#if defined(_DEBUG)
		ED_DispatchExit();
#endif
		return result;
	}

    if (!callback_type)
        Con_Printf("ASSERT FAILURE: entity method (spawn) is null\n");

    return NULL;
}

void ED_PrintEdict_f(void)
{
	int entnum;

	entnum = atoi(Cmd_Argv(1));
	ED_PrintNum(entnum);
}

void ED_PrintEdicts(void)
{
    int i;

    Con_Printf("%i entities\n", *sv_num_edicts);
    for (i = 0; i < *sv_num_edicts; i++)
        ED_Print(EDICT_NUM(i));
}

void ED_Count(void)
{
    int i;
    int active;
    int view;
    int touch;
    int step;
    void *ent;
    float *ent_v;

    active = 0;
    view = 0;
    touch = 0;
    step = 0;

    for (i = 0; i < *sv_num_edicts; i++)
    {
        ent = EDICT_NUM(i);
        ent_v = (float *)ent;

        if (*(int *)ent)
            continue;

        active++;
        if (ent_v[39] != 0.0f)
            touch++;
        if (*(int *)((char *)ent + 248))
            view++;
        if (ent_v[38] == 4.0f)
            step++;
    }

    Con_Printf("num_edicts:%3i\n", *sv_num_edicts);
    Con_Printf("active    :%3i\n", active);
    Con_Printf("view      :%3i\n", view);
    Con_Printf("touch     :%3i\n", touch);
    Con_Printf("step      :%3i\n", step);
}

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

void PR_Profile_f(void)
{
	int i;

	for (i = 0; ; ++i)
	{
		int best_profile = 0;
		dfunction_t *best = NULL;
		dfunction_t *func = (dfunction_t *)pr_functions;
		int numfunctions = ((dprograms_t *)progs)->numfunctions;

		for (int j = 0; j < numfunctions; ++j, ++func)
		{
			if (best_profile < func->profile)
			{
				best_profile = func->profile;
				best = func;
			}
		}

		if (!best)
			break;

		if (i < 10)
			Con_Printf("%7i %s\n", best->profile, pr_strings + best->s_name);

		best->profile = 0;
	}
}

void ED_WriteGlobals(savebuf_t *sb)
{
	int i;
	int numglobaldefs;
	ddef_t *def;
	int type;
	float *val;
	const char *name;
	char *value;

	Savegame_WriteInt(sb, 0);

	numglobaldefs = ((dprograms_t *)progs)->numglobaldefs;
	def = (ddef_t *)pr_globaldefs;
	for (i = 0; i < numglobaldefs; ++i, ++def)
	{
		type = def->type;
		if (!(type & 0x8000))
			continue;

		type &= 0x7FFF;
		if (type != 1 && type != 2 && type != 4)
			continue;

		name = pr_strings + def->s_name;
		val = (float *)((byte *)pr_globals + 4 * def->ofs);

		Savegame_WriteString(sb, name);
		value = PR_UglyValueString(type, val);
		Savegame_WriteString(sb, value);
	}
}

void ED_ParseGlobals(char *data)
{
    char keyname[64];
    ddef_t *key;

    while (1)
    {
        data = COM_Parse(data);
        if (com_token[0] == '}')
            break;
        if (!data)
            Sys_Error("ED_ParseEntity: EOF without closing brace");

        strcpy(keyname, com_token);

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
        {
            Con_Printf("'%s' is not a global\n", keyname);
        }
    }
}

char *ED_ParseEdict(char *data, edict_t *ent)
{
	typedef struct
	{
		char *szClassName;
		char *szKeyName;
		char *szValue;
		int fHandled;
	} KeyValueData;

	const char *classname;
	void (*init_func)(void *, int);
	int init;
	ddef_t *field;
	char keyname[256];
	char anglebuf[32];
	KeyValueData kvd;

	init = 0;

	if (sv_edicts != (void *)ent)
		memset((byte *)ent + 120, 0, 4 * ((dprograms_t *)progs)->entityfields);

	ED_SetEdictPointers(ent);
	ED_SuckOutClassname(data, ent);

	classname = pr_strings + *(int *)((byte *)ent + 244);
	init_func = (void (*)(void *, int))GetEntityInit(classname);
	if (init_func)
		init_func((byte *)ent + 120, 0);
	else
		Con_Printf("Can't init %s\n", classname);

	while (1)
	{
		data = COM_Parse(data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");

		strcpy(keyname, com_token);

		{
			int len = strlen(keyname);
			while (len > 0 && keyname[len - 1] == ' ')
				keyname[--len] = 0;
		}

		data = COM_Parse(data);
		if (!data)
			Sys_Error("ED_ParseEntity: EOF without closing brace");
		if (com_token[0] == '}')
			Sys_Error("ED_ParseEntity: closing brace without data");

		init = 1;

		if (keyname[0] == '_')
			continue;

		classname = pr_strings + *(int *)((byte *)ent + 244);
		if (!classname || strcmp(classname, com_token))
		{
			if (!strcmp(keyname, "angle"))
			{
				strcpy(anglebuf, com_token);
				sprintf(com_token, "0 %s 0", anglebuf);
				strcpy(keyname, "angles");
			}

			field = ED_FindField(keyname);
			if (field)
			{
				if (!ED_ParseEpair((byte *)ent + 120, field, com_token))
					Host_Error("ED_ParseEdict: parse error");
			}
			else
			{
				kvd.szClassName = (char *)classname;
				kvd.szKeyName = keyname;
				kvd.szValue = com_token;
				kvd.fHandled = 0;

				ED_CallDispatch(ent, 5, &kvd);

				if (!kvd.fHandled)
				{
					/* Con_Printf("'%s' is not a field of %s\n", keyname, classname); */
				}
			}
		}
	}

	if (!init)
		*(int *)ent = 1;

	return data;
}

void ED_LoadFromFile(char *data)
{
	edict_t *ent;
	int inhibit;
	int spawnflags;
	float *ent_v;

	ent = NULL;
	inhibit = 0;

	pr_global_struct->time = (float)sv.time;

	while (1)
	{
		data = COM_Parse(data);
		if (!data)
		{
			Con_DPrintf("%i entities inhibited\n", inhibit);
			return;
		}

		if (com_token[0] != '{')
			Sys_Error("ED_LoadFromFile: found %s when expecting {", com_token);

		if (ent)
		{
			ent = ED_Alloc();
		}
		else
		{
			ent = EDICT_NUM(0);
			ED_FreePrivateDataWrapper(ent);
			ED_SetEdictPointers(ent);
		}

		data = ED_ParseEdict(data, ent);

		ent_v = (float *)ent;
		spawnflags = (int)ent_v[138];

		if (deathmatch.value == 0.0f)
		{
			if ((current_skill == 0 && (spawnflags & 0x100)) ||
			    (current_skill == 1 && (spawnflags & 0x200)) ||
			    (current_skill >= 2 && (spawnflags & 0x400)))
			{
				++inhibit;
				ED_Free(ent);
				continue;
			}
		}
		else
		{
			if (spawnflags & 0x800)
			{
				++inhibit;
				ED_Free(ent);
				continue;
			}
		}

		if (*(int *)((byte *)ent + 244))
		{
			pr_global_struct->self = (byte *)ent - (byte *)sv_edicts;
			ED_CallDispatch(ent, 0, 0);
		}
		else
		{
			Con_Printf("No classname for:\n");
			ED_Print(ent);
			ED_Free(ent);
		}
	}
}

void PR_LoadProgs(void)
{
    int i;
    int *progs_ptr;
    unsigned char *progs_bytes;
    short *stmt;
    int *func;
    short *def;

    for (i = 0; i < GEFV_CACHESIZE; i++)
        gefvCache[i].field[0] = 0;

    CRC_Init(&pr_crc);

    progs = COM_LoadHunkFile("progs.dat");
    if (!progs)
        Sys_Error("PR_LoadProgs: couldn't load progs.dat");

    Con_DPrintf("Programs occupy %iK.\n", com_filesize / 1024);

    progs_bytes = (unsigned char *)progs;
    for (i = 0; i < com_filesize; i++)
        CRC_ProcessByte(&pr_crc, progs_bytes[i]);

    progs_ptr = (int *)progs;

    for (i = 0; i < 15; i++)
        progs_ptr[i] = LittleLong(progs_ptr[i]);

    if (progs_ptr[0] != 6)
        Sys_Error("progs.dat has wrong version number (%i should be %i)", progs_ptr[0], 6);

    if (progs_ptr[1] != 58783)
        Sys_Error("progs.dat system vars have been modified, progdefs.h is out of date");

    pr_functions = (char *)progs + progs_ptr[8];
    pr_globaldefs = (char *)progs + progs_ptr[4];
    pr_statements = (char *)progs + progs_ptr[2];
    pr_strings = (char *)progs + progs_ptr[10];
    pr_fielddefs = (char *)progs + progs_ptr[6];
    pr_global_struct = (globalvars_t *)((char *)progs + progs_ptr[12]);
    pr_globals = (float *)pr_global_struct;
    pr_edict_size = 4 * progs_ptr[14] + 120;

    for (i = 0; i < progs_ptr[3]; i++)
    {
        stmt = (short *)((char *)pr_statements + i * 8);
        stmt[0] = LittleShort(stmt[0]);
        stmt[1] = LittleShort(stmt[1]);
        stmt[2] = LittleShort(stmt[2]);
        stmt[3] = LittleShort(stmt[3]);
    }

    for (i = 0; i < progs_ptr[9]; i++)
    {
        func = (int *)((char *)pr_functions + i * 36);
        func[0] = LittleLong(func[0]);
        func[1] = LittleLong(func[1]);
        func[2] = LittleLong(func[2]);
        func[4] = LittleLong(func[4]);
        func[5] = LittleLong(func[5]);
        func[6] = LittleLong(func[6]);
    }

    for (i = 0; i < progs_ptr[5]; i++)
    {
        def = (short *)((char *)pr_globaldefs + i * 8);
        def[0] = LittleShort(def[0]);
        def[1] = LittleShort(def[1]);
        *(int *)&def[2] = LittleLong(*(int *)&def[2]);
    }

    for (i = 0; i < progs_ptr[7]; i++)
    {
        def = (short *)((char *)pr_fielddefs + i * 8);
        def[0] = LittleShort(def[0]);

        if ((signed char)((char *)def)[1] < 0)
            Sys_Error("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");

        def[1] = LittleShort(def[1]);
        *(int *)&def[2] = LittleLong(*(int *)&def[2]);
    }

    for (i = 0; i < progs_ptr[13]; i++)
    {
        ((int *)pr_globals)[i] = LittleLong(((int *)pr_globals)[i]);
    }
}

ddef_t *ED_FindField(const char *name)
{
	int i;
	int numfielddefs;
	ddef_t *def;

	numfielddefs = ((dprograms_t *)progs)->numfielddefs;
	if (numfielddefs <= 0)
		return NULL;

	def = (ddef_t *)pr_fielddefs;
	for (i = 0; i < numfielddefs; ++i, ++def)
	{
		if (!strcmp(pr_strings + def->s_name, name))
			return def;
	}

	return NULL;
}

ddef_t *ED_FindGlobal(const char *name)
{
	int i;
	int numglobaldefs;
	ddef_t *def;

	numglobaldefs = ((dprograms_t *)progs)->numglobaldefs;
	if (numglobaldefs <= 0)
		return NULL;

	def = (ddef_t *)pr_globaldefs;
	for (i = 0; i < numglobaldefs; ++i, ++def)
	{
		if (!strcmp(pr_strings + def->s_name, name))
			return def;
	}

	return NULL;
}

dfunction_t *ED_FindFunction(const char *name)
{
	int i;
	int numfunctions;
	dfunction_t *func;

	numfunctions = ((dprograms_t *)progs)->numfunctions;
	if (numfunctions <= 0)
		return NULL;

	func = (dfunction_t *)pr_functions;
	for (i = 0; i < numfunctions; ++i, ++func)
	{
		if (!strcmp(pr_strings + func->s_name, name))
			return func;
	}

	return NULL;
}

edict_t *ED_Alloc(void)
{
	int i;
	edict_t *ent;

	i = svs.maxclients + 1;
	if (i >= *sv_num_edicts)
		goto allocate_new;

	while (1)
	{
		ent = EDICT_NUM(i);
		if (*(int *)ent)
		{
			if (*(float *)((byte *)ent + 112) < 2.0f || (float)sv.time - *(float *)((byte *)ent + 112) > 0.5f)
				break;
		}

		if (++i >= *sv_num_edicts)
			goto allocate_new;
	}

	ED_ClearEdict(ent);
	return ent;

allocate_new:
	if (i == 600)
		Sys_Error("ED_Alloc: no free edicts");

	(*sv_num_edicts)++;
	ent = EDICT_NUM(i);
	ED_ClearEdict(ent);
	return ent;
}

edict_t *ED_ClearEdict(edict_t *ed)
{
	int entityfields;

	entityfields = ((dprograms_t *)progs)->entityfields;
	memset((byte *)ed + 120, 0, 4 * entityfields);

	*(int *)ed = 0;
	ED_FreePrivateData(ed);
	return ED_SetEdictPointers(ed);
}

void ED_Free(edict_t *ed)
{
	SV_UnlinkEdict(ed);
	ED_FreePrivateData(ed);

	*(int *)ed = 1;
	*(int *)((byte *)ed + 248) = 0;
	*(int *)((byte *)ed + 436) = 0;
	*(int *)((byte *)ed + 120) = 0;

	*(int *)((byte *)ed + 504) = 0;
	*(int *)((byte *)ed + 252) = 0;
	*(int *)((byte *)ed + 284) = 0;

	*(int *)((byte *)ed + 160) = 0;
	*(int *)((byte *)ed + 164) = 0;
	*(int *)((byte *)ed + 168) = 0;

	*(int *)((byte *)ed + 196) = 0;
	*(int *)((byte *)ed + 200) = 0;
	*(int *)((byte *)ed + 204) = 0;

	*(int *)((byte *)ed + 156) = 0;
	*(int *)((byte *)ed + 352) = -1082130432;
	*(float *)((byte *)ed + 112) = (float)sv.time;
}

ddef_t *ED_GlobalAtOfs(int ofs)
{
	int i;
	int numglobaldefs;
	ddef_t *def;

	numglobaldefs = ((dprograms_t *)progs)->numglobaldefs;
	if (numglobaldefs <= 0)
		return NULL;

	def = (ddef_t *)pr_globaldefs;
	for (i = 0; i < numglobaldefs; i++, def++)
	{
		if (def->ofs == ofs)
			return def;
	}

	return NULL;
}

ddef_t *ED_FieldAtOfs(int ofs)
{
	int i;
	int numfielddefs;
	ddef_t *def;

	numfielddefs = ((dprograms_t *)progs)->numfielddefs;
	if (numfielddefs <= 0)
		return NULL;

	def = (ddef_t *)pr_fielddefs;
	for (i = 0; i < numfielddefs; i++, def++)
	{
		if (def->ofs == ofs)
			return def;
	}

	return NULL;
}

void *GetEdictFieldValue(void *ent, const char *field)
{
    int i;
    void *def;
    unsigned short *field_def;

    for (i = 0; i < GEFV_CACHESIZE; i++)
    {
        if (!strcmp(field, gefvCache[i].field))
        {
            def = gefvCache[i].pcache;
            goto found;
        }
    }

    def = ED_FindField(field);
    if (strlen(field) < MAX_FIELD_LEN)
    {

        gefvCache[gefv_cache_index].pcache = def;
        strcpy(gefvCache[gefv_cache_index].field, field);
        gefv_cache_index ^= 1;
    }

found:
    if (def)
    {
        field_def = (unsigned short *)def;
        return (char *)ent + 120 + 4 * field_def[1];
    }
    return NULL;
}

char *PR_ValueString(int type, void *val)
{
    static char line[256];
    float *fval;
    ddef_t *def;
    int entnum;
    char *funcname;
    int type_clean;

    fval = (float *)val;
    type_clean = type & ~0x8000;

    switch (type_clean)
    {
        case 0:
            sprintf(line, "void");
            break;

        case 1:
            sprintf(line, "%s", pr_strings + *(int *)val);
            break;

        case 2:
            sprintf(line, "%5.1f", *fval);
            break;

        case 3:
            sprintf(line, "'%5.1f %5.1f %5.1f'", fval[0], fval[1], fval[2]);
            break;

        case 4:
            entnum = NUM_FOR_EDICT((char *)sv_edicts + *(int *)val);
            sprintf(line, "entity %i", entnum);
            break;

        case 5:
            def = ED_FieldAtOfs(*(int *)val);
            sprintf(line, ".%s", pr_strings + def->s_name);
            break;

        case 6:
            funcname = pr_strings + *(int *)((char *)pr_functions + 36 * *(int *)val + 16);
            sprintf(line, "%s()", funcname);
            break;

        case 7:
            sprintf(line, "pointer");
            break;

        default:
            sprintf(line, "bad type %i", type_clean);
            break;
    }

    return line;
}

char *PR_UglyValueString(int type, void *val)
{
    static char line[256];
    float *fval;
    ddef_t *def;
    int entnum;
    char *funcname;
    char *fieldname;
    int type_clean;

    fval = (float *)val;
    type_clean = type & ~0x8000;

    if (type_clean == 0)
    {
        sprintf(line, "void");
        return line;
    }

    switch (type_clean)
    {
        case 1:
            sprintf(line, "%s", pr_strings + *(int *)val);
            break;

        case 2:
            sprintf(line, "%f", *fval);
            break;

        case 3:
            sprintf(line, "%f %f %f", fval[0], fval[1], fval[2]);
            break;

        case 4:
            entnum = NUM_FOR_EDICT((char *)sv_edicts + *(int *)val);
            sprintf(line, "%i", entnum);
            break;

        case 5:
            def = ED_FieldAtOfs(*(int *)val);
            fieldname = pr_strings + def->s_name;
            sprintf(line, "%s", fieldname);
            break;

        case 6:
            funcname = pr_strings + *(int *)((char *)pr_functions + 36 * *(int *)val + 16);
            sprintf(line, "%s", funcname);
            break;

        default:
            sprintf(line, "bad type %i", type_clean);
            break;
    }

    return line;
}

char *PR_GlobalString(int ofs)
{
    static char line[128];
    ddef_t *def;
    char *s;
    void *val;
    int i;
    int len;

    val = (char *)pr_globals + 4 * ofs;
    def = ED_GlobalAtOfs(ofs);

    if (def)
    {
        s = PR_ValueString(def->type, val);
        sprintf(line, "%i(%s)%s", ofs, pr_strings + def->s_name, s);
    }
    else
    {
        sprintf(line, "%i(?]", ofs);
    }

    len = strlen(line);
    if (len < 20)
    {
        for (i = len; i < 20; i++)
            strcat(line, " ");
    }
    strcat(line, " ");

    return line;
}

char *PR_GlobalStringNoContents(int ofs)
{
    static char line[128];
    ddef_t *def;
    int i;
    int len;

    def = ED_GlobalAtOfs(ofs);

    if (def)
        sprintf(line, "%i(%s)", ofs, pr_strings + def->s_name);
    else
        sprintf(line, "%i(?]", ofs);

    len = strlen(line);
    if (len < 20)
    {
        for (i = len; i < 20; i++)
            strcat(line, " ");
    }
    strcat(line, " ");

    return line;
}

void ED_Print(void *ed)
{
    int i;
    int *progs_ptr;
    unsigned short *def;
    char *fieldname;
    void *val;
    int type;
    int type_size_local;
    int j;
    int *field_data;
    int numfielddefs;
    int len;

    if (*(int *)ed)
    {
        Con_Printf("FREE\n");
        return;
    }

    Con_Printf("EDICT %i:\n", NUM_FOR_EDICT(ed));

    progs_ptr = (int *)progs;
    numfielddefs = progs_ptr[7];

    for (i = 1; i < numfielddefs; i++)
    {
        def = (unsigned short *)((char *)pr_fielddefs + i * 8);
        fieldname = pr_strings + *(int *)((char *)def + 4);

        len = strlen(fieldname);
        if (len >= 2 && fieldname[len - 2] == '_')
            continue;

        type = def[0] & ~0x8000;
        type_size_local = type_size[type];
        val = (char *)ed + 120 + 4 * def[1];

        field_data = (int *)val;
        for (j = 0; j < type_size_local; j++)
        {
            if (field_data[j])
                break;
        }

        if (j == type_size_local)
            continue;

        Con_Printf("%s", fieldname);
        len = strlen(fieldname);
        for (; len < 15; len++)
            Con_Printf(" ");

        Con_Printf("%s\n", PR_ValueString(def[0], val));
    }
}

void ED_Write(savebuf_t *sb, edict_t *ed)
{
    int i, j;
    int *progs_ptr;
    unsigned short *def;
    char *fieldname;
    void *val;
    int type;
    int type_size_local;
    int *field_data;
    char *valstring;
    int numfielddefs;

    Savegame_WriteInt(sb, 0);

    if (ed->free)
        return;

    progs_ptr = (int *)progs;
    numfielddefs = progs_ptr[7];

    for (i = 1; i < numfielddefs; i++)
    {
        def = (unsigned short *)((char *)pr_fielddefs + i * 8);
        fieldname = pr_strings + *(int *)((char *)def + 4);

        int len = strlen(fieldname);
        if (len >= 2 && fieldname[len - 2] == '_')
            continue;

        type = def[0] & 0x7FFF;
        type_size_local = type_size[type];
        val = (char *)ed + 120 + 4 * def[1];

        field_data = (int *)val;
        for (j = 0; j < type_size_local; j++)
        {
            if (field_data[j])
                break;
        }

        if (j == type_size_local)
            continue;

        Savegame_WriteString(sb, fieldname);
        valstring = PR_UglyValueString(type, val);
        Savegame_WriteString(sb, valstring);
    }

    ED_CallDispatch(ed, 6, sb);
}

void ED_PrintNum(int entnum)
{
	edict_t *ent;

	ent = EDICT_NUM(entnum);
	ED_Print(ent);
}

char *PROG_TO_STRING(int offset)
{
    return pr_strings + offset;
}

void *EDICT_TO_ENTVAR(edict_t *ent)
{
    return (void *)&ent->v;
}

edict_t *ENTVAR_TO_EDICT(void *entvar)
{
    int i;
    edict_t *ent;

    for (i = 0; i < *sv_num_edicts; i++)
    {
        ent = EDICT_NUM(i);
        if ((void *)&ent->v == entvar)
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
    char *new_str;

    new_str = ED_NewString(string);
    return (int)new_str - (int)pr_strings;
}

int ED_EntVarsToCl(edict_t *ent)
{
	int entnum;

	entnum = NUM_FOR_EDICT(ent);

	if (entnum < 1 || svs.maxclients < entnum)
		PR_RunError("Entity is not a client");

	memcpy((byte *)svs.clients + 8272 * entnum - 8272 + 8204, (byte *)pr_global_struct + 176, 0x40);
	return (int)svs.clients;
}

int ED_FindModel(edict_t *ent)
{
	int modelindex;
	model_t *model;

	if (!ent)
		return 0;

	modelindex = (int)*(float *)((byte *)ent + 120);
	if (modelindex <= 0 || modelindex >= MAX_MODELS)
		return 0;

	model = sv.models[modelindex];
	if (!model)
		return 0;

	return (int)Mod_Extradata(model);
}

int ED_UpdateHullCache(void *base, int size)
{
	int end;
	int loops;
	int i;
	int value;
	int result;

	end = size - 0x10000;
	loops = 4;
	do
	{
		for (i = 0; i < end; g_deltaHullCacheChecksum += result)
		{
			value = *(int *)((byte *)base + i);
			i += 4;
			g_deltaHullCacheChecksum += value;
			result = *(int *)((byte *)base + i + 65532);
		}
		--loops;
	} while (loops);

	return result;
}

int ED_AllocEngineHandle(void)
{
	int handle = 1;
	int *p = &g_engineHandleTable[0];

	while (*p != 0)
	{
		p++;
		handle++;
		if (p >= &g_engineHandleTable[9])
		{
			Sys_Error("out of handles");
			return -1;
		}
	}

	return handle;
}
