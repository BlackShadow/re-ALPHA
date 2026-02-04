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

#define MAX_ALIAS_NAME		32
#define MAX_ARGS			80

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char				name[MAX_ALIAS_NAME];
	char				*value;
} cmdalias_t;

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	void					(*function)(void);
} cmd_function_t;

static char				cmd_text_data[8192];
static int				cmd_text_cursize = 0;
static int				cmd_text_maxsize = 8192;

cmdalias_t				*cmd_alias = NULL;
cmd_function_t			*cmd_functions = NULL;
int						cmd_argc;
char					*cmd_argv[MAX_ARGS];
char					*cmd_args;
int						cmd_source;

static int				cmd_wait = 0;

void Cbuf_Init(void)
{
	cmd_text_cursize = 0;
	cmd_text_maxsize = 8192;
}

void Cbuf_AddText(const char *text)
{
	int len;

	len = strlen(text);

	if (cmd_text_cursize + len >= cmd_text_maxsize)
	{
		Con_Printf("Cbuf_AddText: overflow\n");
		return;
	}

	memcpy(&cmd_text_data[cmd_text_cursize], text, len);
	cmd_text_cursize += len;
}

void Cbuf_InsertText(const char *text)
{
	char	*temp;
	int		templen;

	templen = cmd_text_cursize;
	if (templen)
	{
		temp = Z_Malloc(templen);
		memcpy(temp, cmd_text_data, templen);
		cmd_text_cursize = 0;
	}

	Cbuf_AddText(text);

	if (templen)
	{
		memcpy(&cmd_text_data[cmd_text_cursize], temp, templen);
		cmd_text_cursize += templen;
		Z_Free(temp);
	}
}

void Cbuf_Execute(void)
{
	int		i;
	char	*text;
	char	line[1024];
	int		quotes;

	while (cmd_text_cursize)
	{
		text = cmd_text_data;
		quotes = 0;

		for (i = 0; i < cmd_text_cursize; i++)
		{
			if (text[i] == '"')
				quotes++;
			if (!(quotes & 1) && text[i] == ';')
				break;
			if (text[i] == '\n')
				break;
		}

		memcpy(line, text, i);
		line[i] = 0;

		if (i == cmd_text_cursize)
		{
			cmd_text_cursize = 0;
		}
		else
		{
			i++;
			cmd_text_cursize -= i;
			memcpy(text, text + i, cmd_text_cursize);
		}

		Cmd_ExecuteString(line, src_command);

		if (cmd_wait)
		{
			cmd_wait = 0;
			break;
		}
	}
}

void Cmd_StuffCmds_f(void)
{
	int		i, j;
	int		s;
	char	*text, *build, c;

	if (Cmd_Argc() != 1)
	{
		Con_Printf("stuffcmds : execute command line parameters\n");
		return;
	}

	s = 0;
	for (i = 1; i < com_argc; i++)
	{
		if (com_argv[i])
		{
			s += strlen(com_argv[i]) + 1;
		}
	}

	if (!s)
		return;

	text = Z_Malloc(s + 1);
	text[0] = 0;
	for (i = 1; i < com_argc; i++)
	{
		if (com_argv[i])
		{
			strcat(text, com_argv[i]);
			if (i != com_argc - 1)
				strcat(text, " ");
		}
	}

	build = Z_Malloc(s + 1);
	build[0] = 0;

	for (i = 0; i < s - 1; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j = i; (text[i] != '+') && (text[i] != '-') && (text[i] != 0); i++)
				;

			c = text[i];
			text[i] = 0;

			strcat(build, &text[j]);
			strcat(build, "\n");

			text[i] = c;
			i--;
		}
	}

	if (build[0])
		Cbuf_InsertText(build);

	Z_Free(text);
	Z_Free(build);
}

//=========================================================
// Cmd_StuffCmds_Null - empty stub
//=========================================================
void Cmd_StuffCmds_Null(void)
{
}

void Cmd_Exec_f(void)
{
	char	*f;
	int		mark;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("exec <filename> : execute a script file\n");
		return;
	}

	mark = Hunk_LowMark();
	f = (char *)COM_LoadFile(Cmd_Argv(1), 1);
	if (!f)
	{
		Hunk_FreeToLowMark(mark);
		Con_Printf("couldn't exec %s\n", Cmd_Argv(1));
		return;
	}
	Con_DPrintf("execing %s\n", Cmd_Argv(1));

	Cbuf_InsertText(f);
	Hunk_FreeToLowMark(mark);
}

void Cmd_Wait_f(void)
{
	cmd_wait = 1;
}

int Cmd_Argc(void)
{
	return cmd_argc;
}

char *Cmd_Argv(int arg)
{
	if (arg >= cmd_argc)
		return "";
	return cmd_argv[arg];
}

char *Cmd_Args(void)
{
	return cmd_args;
}

void Cmd_TokenizeString(char *text)
{
	int		i;

	for (i = 0; i < cmd_argc; i++)
	{
		Z_Free(cmd_argv[i]);
	}

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{

		while (*text && *text <= 32 && *text != '\n')
		{
			text++;
		}

		if (*text == '\n' || !*text)
			return;

		if (cmd_argc == 1)
			cmd_args = text;

		text = COM_Parse(text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			int len = strlen(com_token);
			cmd_argv[cmd_argc] = Z_Malloc(len + 1);
			strcpy(cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}
}

void Cmd_AddCommand(const char *cmd_name, void (*function)(void))
{
	cmd_function_t	*cmd;

	if (host_initialized)
		Sys_Error("Cmd_AddCommand after host_initialized\n");

	if (Cvar_FindVar(cmd_name))
	{
		Con_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!stricmp(cmd_name, cmd->name))
		{
			Con_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Hunk_Alloc(sizeof(cmd_function_t));
	cmd->name = (char*)cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

int Cmd_Exists(const char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!stricmp(cmd_name, cmd->name))
			return 1;
	}

	return 0;
}

char *Cmd_CompleteCommand(const char *partial)
{
	cmd_function_t	*cmd;
	int				len;

	len = strlen(partial);
	if (!len)
		return NULL;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strncmp(partial, cmd->name, len))
			return cmd->name;
	}

	return NULL;
}

void Cmd_ExecuteString(char *text, cmd_source_t src)
{
	cmd_function_t	*cmd;
	cmdalias_t		*a;

	cmd_source = (int)src;
	Cmd_TokenizeString(text);

	if (!Cmd_Argc())
		return;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!stricmp(cmd_argv[0], cmd->name))
		{
			cmd->function();
			return;
		}
	}

	for (a = cmd_alias; a; a = a->next)
	{
		if (!stricmp(cmd_argv[0], a->name))
		{
			Cbuf_InsertText(a->value);
			return;
		}
	}

	if (Cvar_Command())
		return;

	Con_Printf("Unknown command \"%s\"\n", Cmd_Argv(0));
}

void Cmd_Echo_f(void)
{
	int		i;

	for (i = 1; i < Cmd_Argc(); i++)
	{
		Con_Printf("%s ", Cmd_Argv(i));
	}
	Con_Printf("\n");
}

void Cmd_Alias_f(void)
{
	cmdalias_t	*a;
	const char	*s;
	char		cmd[1024];
	int			i, c;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("Current alias commands:\n");
		for (a = cmd_alias; a; a = a->next)
		{
			Con_Printf("%s : %s\n", a->name, a->value);
		}
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Con_Printf("Alias name is too long\n");
		return;
	}

	for (a = cmd_alias; a; a = a->next)
	{
		if (!strcmp(s, a->name))
		{
			Z_Free(a->value);
			break;
		}
	}

	if (!a)
	{
		a = Z_Malloc(sizeof(cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}

	strcpy(a->name, s);

	cmd[0] = 0;
	c = Cmd_Argc();
	for (i = 2; i < c; i++)
	{
		strcat(cmd, Cmd_Argv(i));
		if (i != c - 1)
			strcat(cmd, " ");
	}
	strcat(cmd, "\n");

	a->value = Z_Malloc(strlen(cmd) + 1);
	strcpy(a->value, cmd);
}

void Cmd_List_f(void)
{
	cmd_function_t	*cmd;
	int				i;

	i = 0;
	for (cmd = cmd_functions; cmd; cmd = cmd->next, i++)
		Con_Printf("%s\n", cmd->name);
	Con_Printf("%i commands\n", i);
}

void Cmd_ForwardToServer(void)
{
	const char *cmd_name;

	if (cls.state != ca_connected)
	{
		Con_Printf("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (cls.demoplayback)
		return;

	MSG_WriteByte(&cls.message, clc_stringcmd);

	cmd_name = Cmd_Argv(0);
	if (Q_strcasecmp(cmd_name, "cmd"))
	{
		SZ_Print(&cls.message, (char *)cmd_name);
		SZ_Print(&cls.message, " ");
	}

	if (Cmd_Argc() <= 1)
	{
		SZ_Print(&cls.message, "\n");
	}
	else
	{
		SZ_Print(&cls.message, Cmd_Args());
	}
}

int Cmd_CheckParm(const char *parm)
{
	int		i;

	if (!parm)
		Sys_Error("Cmd_CheckParm: NULL\n");

	for (i = 1; i < Cmd_Argc(); i++)
	{
		if (!stricmp(parm, Cmd_Argv(i)))
			return i;
	}

	return 0;
}

void Cmd_Init(void)
{
	Cbuf_Init();
	Cmd_AddCommand("stuffcmds", Cmd_StuffCmds_f);
	Cmd_AddCommand("exec", Cmd_Exec_f);
	Cmd_AddCommand("echo", Cmd_Echo_f);
	Cmd_AddCommand("alias", Cmd_Alias_f);
	Cmd_AddCommand("cmd", Cmd_ForwardToServer);
	Cmd_AddCommand("wait", Cmd_Wait_f);
	Cmd_AddCommand("cmdlist", Cmd_List_f);
}
