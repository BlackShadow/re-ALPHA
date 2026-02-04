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
#include "progdefs.h"

int pr_xstatement;
int pr_depth;
dfunction_t *pr_xfunction;
int pr_localstack_used;
int pr_argc;
int pr_trace;
int pr_stack_s[2 * MAX_STACK_DEPTH];
dfunction_t *pr_stack_f[2 * MAX_STACK_DEPTH];
int pr_localstack[LOCALSTACK_SIZE];

extern int pr_numbuiltins;
extern void (*pr_builtins[])();

extern int type_size[];

char *pr_opnames[] = {
	"DONE",
	"MUL_F", "MUL_V", "MUL_FV", "MUL_VF",
	"DIV_F",
	"ADD_F", "ADD_V",
	"SUB_F", "SUB_V",
	"EQ_F", "EQ_V", "EQ_S", "EQ_E", "EQ_FNC",
	"NE_F", "NE_V", "NE_S", "NE_E", "NE_FNC",
	"LE", "GE", "LT", "GT",
	"INDIRECT", "INDIRECT", "INDIRECT", "INDIRECT", "INDIRECT", "INDIRECT",
	"ADDRESS",
	"STORE_F", "STORE_V", "STORE_S", "STORE_ENT", "STORE_FLD", "STORE_FNC",
	"STOREP_F", "STOREP_V", "STOREP_S", "STOREP_ENT", "STOREP_FLD", "STOREP_FNC",
	"RETURN",
	"NOT_F", "NOT_V", "NOT_S", "NOT_ENT", "NOT_FNC",
	"IF", "IFNOT",
	"CALL0", "CALL1", "CALL2", "CALL3", "CALL4", "CALL5", "CALL6", "CALL7", "CALL8",
	"STATE",
	"GOTO",
	"AND", "OR",
	"BITAND", "BITOR",
};

extern char pr_string_temp[];
extern char pr_string_temp2[];
extern char pr_global_str[];
extern char pr_global_str2[];

extern void Con_Printf(const char *fmt, ...);
extern void Host_Error(const char *error, ...);
extern int ED_NUM_FOR_EDICT(int *ent);
extern void ED_Print(void *ed);
extern int ED_FieldAtOfs(int ofs);
extern int ED_GlobalAtOfs(int ofs);
extern int ED_FindFunction(char *name);

int PR_PrintStatement(unsigned short *st)
{
	unsigned int len;
	int i;

	if (*st < 66)
	{
		Con_Printf("%s ", pr_opnames[*st]);
		len = strlen(pr_opnames[*st]) + 1;

		if (len - 1 < 10)
		{
			i = 10 - (len - 1);
			do
			{
				Con_Printf(" ");
				--i;
			} while (i);
		}
	}

	if (*st == 49 || *st == 50)
	{
		Con_Printf("%s branch %i", PR_GlobalString(st[1]), st[2]);
	}
	else if (*st == 61)
	{
		Con_Printf("branch %i", st[1]);
	}
	else if (*st - 31 >= 6)
	{
		if (st[1])
			Con_Printf("%s", PR_GlobalString(st[1]));
		if (st[2])
			Con_Printf("%s", PR_GlobalString(st[2]));
		if (st[3])
			Con_Printf("%s", PR_GlobalStringNoContents(st[3]));
	}
	else
	{
		Con_Printf("%s", PR_GlobalString(st[1]));
		Con_Printf("%s", PR_GlobalStringNoContents(st[2]));
	}

	Con_Printf("\n");
	return 0;
}

int PR_StackTrace(void)
{
	int depth;
	dfunction_t **stack;
	dfunction_t *func;

	depth = pr_depth;
	if (!pr_depth)
		Con_Printf("<NO STACK>\n");
		return 0;

	stack = &pr_stack_f[2 * pr_depth];
	*stack = pr_xfunction;

	if (depth >= 0)
	{
		do
		{
			func = *stack;
			if (func)
				Con_Printf("%12s : %s\n", pr_strings + func->s_name, pr_strings + func->s_file);
			else
				Con_Printf("<NO FUNCTION>\n");
			stack -= 2;
		} while (stack >= pr_stack_f);
	}

	return 0;
}

int PR_Profile(void)
{
	int i;
	int maxProfile;
	int numFuncs;
	dfunction_t *maxFunc;
	dfunction_t *func;

	for (i = 0; ; ++i)
	{
		maxProfile = 0;
		maxFunc = NULL;
		numFuncs = ((dprograms_t *)progs)->numfunctions;

		if (numFuncs > 0)
		{
			func = (dfunction_t *)pr_functions;
			do
			{
				if (maxProfile < func->profile)
				{
					maxProfile = func->profile;
					maxFunc = func;
				}
				++func;
				--numFuncs;
			} while (numFuncs);
		}

		if (!maxFunc)
			break;

		if (i < 10)
			Con_Printf("%7i %s\n", maxFunc->profile, pr_strings + maxFunc->s_name);

		maxFunc->profile = 0;
	}

	return 0;
}

void PR_RunError(char *fmt, ...)
{
	va_list args;
	char buffer[1024];

	va_start(args, fmt);
	vsprintf(buffer, fmt, args);
	va_end(args);

	PR_PrintStatement((unsigned short *)((byte *)pr_statements + 8 * pr_xstatement));
	PR_StackTrace();
	Con_Printf("%s\n", buffer);

	pr_depth = 0;
	Host_Error("Program error", buffer);
}

int PR_EnterFunction(dfunction_t *func)
{
	int depth;
	int numLocals;
	int firstLocal;
	int numParms;
	int i;
	int parmSize;
	int *src;
	int *locals;

	depth = pr_depth;

	pr_stack_s[2 * pr_depth] = pr_xstatement;
	pr_stack_f[2 * depth] = pr_xfunction;
	pr_depth = depth + 1;

	if (depth + 1 >= MAX_STACK_DEPTH)
		PR_RunError("stack overflow");

	numLocals = func->locals;
	if (numLocals + pr_localstack_used > LOCALSTACK_SIZE)
		PR_RunError("PR_ExecuteProgram: locals stack overflow");

	if (numLocals > 0)
		memcpy(&pr_localstack[pr_localstack_used], (int *)pr_globals + func->parm_start, sizeof(int) * numLocals);

	firstLocal = func->parm_start;
	pr_localstack_used += numLocals;

	numParms = func->num_parms;
	if (numParms > 0)
	{
		for (i = 0; i < numParms; i++)
		{
			parmSize = func->parm_size[i];
			if (parmSize)
			{
				src = (int *)((byte *)pr_globals + 16 + 12 * i);
				locals = (int *)pr_globals + firstLocal;
				firstLocal += parmSize;
				do
				{
					*locals++ = *src++;
					--parmSize;
				} while (parmSize);
			}
		}
	}

	pr_xfunction = func;
	return func->first_statement - 1;
}

int PR_LeaveFunction(void)
{
	int numLocals;

	if (pr_depth <= 0)
		Sys_Error("prog stack underflow");

	numLocals = pr_xfunction->locals;
	pr_localstack_used -= numLocals;

	if (pr_localstack_used < 0)
		PR_RunError("PR_ExecuteProgram: locals stack underflow");

	if (numLocals > 0)
		memcpy((int *)pr_globals + pr_xfunction->parm_start, &pr_localstack[pr_localstack_used], sizeof(int) * numLocals);

	--pr_depth;
	pr_xfunction = pr_stack_f[2 * pr_depth];

	return pr_stack_s[2 * pr_depth];
}

void PR_ExecuteProgram(int fnum)
{
	int st;
	int exitdepth;
	int runaway;
	dfunction_t *func;
	unsigned short *statement;
	short *opa_short;
	short *opb_short;
	float *opa;
	float *opb;
	float *opc;
	int funcnum;
	int i;

	if (!fnum || ((dprograms_t *)progs)->numfunctions <= fnum)
	{
		if (pr_global_struct->self)
			ED_Print(PROG_TO_EDICT(pr_global_struct->self));
		Host_Error("PR_ExecuteProgram: NULL function");
	}

	exitdepth = pr_depth;
	runaway = 100000;
	pr_trace = 0;

	func = &((dfunction_t *)pr_functions)[fnum];
	if (func->first_statement < 0)
	{

		i = -func->first_statement;
		if (i >= pr_numbuiltins)
			PR_RunError("Bad builtin call number");
		pr_builtins[i]();
		return;
	}

	st = PR_EnterFunction(func);

	while (1)
	{
		++st;
		statement = (unsigned short *)((byte *)pr_statements + 8 * st);
		opa_short = (short *)(statement + 1);
		opa = &pr_globals[(short)statement[1]];
		opb_short = (short *)(statement + 2);
		opb = &pr_globals[(short)statement[2]];
		opc = &pr_globals[(short)statement[3]];

		if (--runaway == 0)
			break;

		pr_xstatement = st;
		pr_xfunction->profile++;

		if (pr_trace)
			PR_PrintStatement(statement);

		switch (*statement)
		{
		case 0:
		case 43:
			((int *)&pr_globals[OFS_RETURN])[0] = ((int *)opa)[0];
			((int *)&pr_globals[OFS_RETURN])[1] = ((int *)opa)[1];
			((int *)&pr_globals[OFS_RETURN])[2] = ((int *)opa)[2];
			st = PR_LeaveFunction();
			if (pr_depth == exitdepth)
				return;
			continue;

		case 1:
			*opc = *opb * *opa;
			continue;

		case 2:
			*opc = opb[0] * opa[0] + opb[1] * opa[1] + opb[2] * opa[2];
			continue;

		case 3:
			opc[0] = opb[0] * *opa;
			opc[1] = opb[1] * *opa;
			opc[2] = opb[2] * *opa;
			continue;

		case 4:
			opc[0] = opa[0] * *opb;
			opc[1] = opa[1] * *opb;
			opc[2] = opa[2] * *opb;
			continue;

		case 5:
			*opc = *opa / *opb;
			continue;

		case 6:
			*opc = *opa + *opb;
			continue;

		case 7:
			opc[0] = opa[0] + opb[0];
			opc[1] = opa[1] + opb[1];
			opc[2] = opa[2] + opb[2];
			continue;

		case 8:
			*opc = *opa - *opb;
			continue;

		case 9:
			opc[0] = opa[0] - opb[0];
			opc[1] = opa[1] - opb[1];
			opc[2] = opa[2] - opb[2];
			continue;

		case 10:
			*opc = (float)(*opa == *opb);
			continue;

		case 11:
			if (opa[0] != opb[0] || opa[1] != opb[1] || opa[2] != opb[2])
				*opc = 0.0f;
			else
				*opc = 1.0f;
			continue;

		case 12:
			*opc = (float)(strcmp((char *)(pr_strings + *(int *)opa), (char *)(pr_strings + *(int *)opb)) == 0);
			continue;

		case 13:
			*opc = (float)(*(int *)opa == *(int *)opb);
			continue;

		case 14:
			*opc = (float)(*(int *)opa == *(int *)opb);
			continue;

		case 15:
			*opc = (float)(*opa != *opb);
			continue;

		case 16:
			if (opa[0] != opb[0] || opa[1] != opb[1] || opa[2] != opb[2])
				*opc = 1.0f;
			else
				*opc = 0.0f;
			continue;

		case 17:
			*opc = (float)strcmp((char *)(pr_strings + *(int *)opa), (char *)(pr_strings + *(int *)opb));
			continue;

		case 18:
			*opc = (float)(*(int *)opa != *(int *)opb);
			continue;

		case 19:
			*opc = (float)(*(int *)opa != *(int *)opb);
			continue;

		case 20:
			*opc = (float)(*opb <= *opa);
			continue;

		case 21:
			*opc = (float)(*opb >= *opa);
			continue;

		case 22:
			*opc = (float)(*opb < *opa);
			continue;

		case 23:
			*opc = (float)(*opb > *opa);
			continue;

		case 24:
		case 26:
		case 27:
		case 28:
		case 29:
			*opc = *(float *)(*(int *)opa + 4 * *(int *)opb + sv_edicts_base + 120);
			continue;

		case 25:
			opc[0] = *(float *)(*(int *)opa + 4 * *(int *)opb + sv_edicts_base + 120);
			opc[1] = *(float *)(*(int *)opa + 4 * *(int *)opb + sv_edicts_base + 124);
			opc[2] = *(float *)(*(int *)opa + 4 * *(int *)opb + sv_edicts_base + 128);
			continue;

		case 30:
			i = sv_edicts_base + *(int *)opa;
			if (sv_edicts_base == i && sv_edicts_active == 1)
				PR_RunError("assignment to world entity");
			*(int *)opc = i + 4 * *(int *)opb + 120 - sv_edicts_base;
			continue;

		case 31:
		case 33:
		case 34:
		case 35:
		case 36:
			*opb = *opa;
			continue;

		case 32:
			opb[0] = opa[0];
			opb[1] = opa[1];
			opb[2] = opa[2];
			continue;

		case 37:
		case 39:
		case 40:
		case 41:
		case 42:
			*(float *)(sv_edicts_base + *(int *)opb) = *opa;
			continue;

		case 38:
			*(float *)(sv_edicts_base + *(int *)opb) = opa[0];
			*(float *)(sv_edicts_base + *(int *)opb + 4) = opa[1];
			*(float *)(sv_edicts_base + *(int *)opb + 8) = opa[2];
			continue;

		case 44:
			*opc = (float)(*opa == 0.0f);
			continue;

		case 45:
			if (opa[0] != 0.0f || opa[1] != 0.0f || opa[2] != 0.0f)
				*opc = 0.0f;
			else
				*opc = 1.0f;
			continue;

		case 46:
			if (!*(int *)opa || !*(char *)(*(int *)opa + pr_strings))
				*opc = 1.0f;
			else
				*opc = 0.0f;
			continue;

		case 47:
			*opc = (float)(*(int *)opa == 0);
			continue;

		case 48:
			*opc = (float)(*(int *)opa == 0);
			continue;

		case 49:
			if (*(int *)opa)
				st += *opb_short - 1;
			continue;

		case 50:
			if (!*(int *)opa)
				st += *opb_short - 1;
			continue;

		case 51:
		case 52:
		case 53:
		case 54:
		case 55:
		case 56:
		case 57:
		case 58:
		case 59:
			pr_argc = *statement - 51;
			funcnum = *(int *)opa;
			if (funcnum == 0)
				PR_RunError("NULL function");

			{
				dfunction_t *func = &((dfunction_t *)pr_functions)[funcnum];
				if (func->first_statement >= 0)
				{
					st = PR_EnterFunction(func);
				}
				else
				{

					i = -func->first_statement;
					if (i >= pr_numbuiltins)
						PR_RunError("Bad builtin call number");
					pr_builtins[i]();
				}
			}
			break;

		case 60:
			{
				float frametime = pr_global_struct->time + 0.1f;
				int self = sv_edicts_base + pr_global_struct->self;
				*(float *)(self + 352) = frametime;
				if (*opa != *(float *)(self + 284))
					*(float *)(self + 284) = *opa;
				*(float *)(self + 344) = *opb;
			}
			continue;

		case 61:
			st += *opa_short - 1;
			continue;

		case 62:
			if (*opa == 0.0f || *opb == 0.0f)
				*opc = 0.0f;
			else
				*opc = 1.0f;
			continue;

		case 63:
			if (*opa != 0.0f || *opb != 0.0f)
				*opc = 1.0f;
			else
				*opc = 0.0f;
			continue;

		case 64:
			*opc = (float)((int)*opa & (int)*opb);
			continue;

		case 65:
			*opc = (float)((int)*opa | (int)*opb);
			continue;

		default:
			PR_RunError("Bad opcode %i", *statement);
		}
	}

	PR_RunError("runaway loop error");
	return;
}

void PR_trace_on(void)
{
	pr_trace = 1;
}

void PR_trace_off(void)
{
	pr_trace = 0;
}
