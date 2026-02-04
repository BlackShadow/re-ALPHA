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

 // cmd.h -- command buffer and execution

#ifndef CMD_H
#define CMD_H

 // =========================================================
 // Types
 // =========================================================

typedef void (*xcommand_t)(void);

typedef enum
{
	src_client = 0,
	src_command = 1
} cmd_source_t;

 // =========================================================
 // Function Prototypes
 // =========================================================

void Cbuf_Init(void);
void Cbuf_AddText(const char *text);
void Cbuf_InsertText(const char *text);
void Cbuf_Execute(void);

void Cmd_Init(void);
void Cmd_AddCommand(const char *cmd_name, xcommand_t function);
int  Cmd_Exists(const char *cmd_name);
void Cmd_RemoveCommand(const char *cmd_name);
int  Cmd_Argc(void);
char *Cmd_Argv(int arg);
char *Cmd_Args(void);
void Cmd_TokenizeString(char *text);
void Cmd_ExecuteString(char *text, cmd_source_t src);
void Cmd_ForwardToServer(void);
int  Cmd_CheckParm(const char *parm);

char *Cmd_CompleteCommand(const char *partial);

#endif // CMD_H

