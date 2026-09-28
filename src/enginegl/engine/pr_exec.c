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

#define MAX_RUNAWAY		100000

// progs field and pointer access, relative to sv_edicts_base
#define EDICT_FIELD(e, ofs)	((float *)&((edict_t *)(sv_edicts_base + (e)))->v + (ofs))
#define PROG_POINTER(p)		((float *)(sv_edicts_base + (p)))

typedef struct
{
	int				s;
	dfunction_t		*f;
} prstack_t;

int				pr_xstatement;
int				pr_depth;
dfunction_t		*pr_xfunction;
int				pr_localstack_used;
int				pr_argc;
qboolean		pr_trace;
prstack_t		pr_stack[MAX_STACK_DEPTH];
int				pr_localstack[LOCALSTACK_SIZE];

char *pr_opnames[] =
{
	"DONE",

	"MUL_F",
	"MUL_V",
	"MUL_FV",
	"MUL_VF",

	"DIV_F",

	"ADD_F",
	"ADD_V",

	"SUB_F",
	"SUB_V",

	"EQ_F",
	"EQ_V",
	"EQ_S",
	"EQ_E",
	"EQ_FNC",

	"NE_F",
	"NE_V",
	"NE_S",
	"NE_E",
	"NE_FNC",

	"LE",
	"GE",
	"LT",
	"GT",

	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",
	"INDIRECT",

	"ADDRESS",

	"STORE_F",
	"STORE_V",
	"STORE_S",
	"STORE_ENT",
	"STORE_FLD",
	"STORE_FNC",

	"STOREP_F",
	"STOREP_V",
	"STOREP_S",
	"STOREP_ENT",
	"STOREP_FLD",
	"STOREP_FNC",

	"RETURN",

	"NOT_F",
	"NOT_V",
	"NOT_S",
	"NOT_ENT",
	"NOT_FNC",

	"IF",
	"IFNOT",

	"CALL0",
	"CALL1",
	"CALL2",
	"CALL3",
	"CALL4",
	"CALL5",
	"CALL6",
	"CALL7",
	"CALL8",

	"STATE",

	"GOTO",

	"AND",
	"OR",

	"BITAND",
	"BITOR"
};

//=============================================================================

/*
=================
PR_PrintStatement

Every opcode below OP_STOREP_F is printed in the store form.
=================
*/
void PR_PrintStatement(dstatement_t *s)
{
	unsigned int i;

	if (s->op < NUM_OPCODES)
	{
		Con_Printf("%s ", pr_opnames[s->op]);
		i = strlen(pr_opnames[s->op]);
		for ( ; i < 10; i++)
			Con_Printf(" ");
	}

	// operands are printed as unsigned
	if (s->op == OP_IF || s->op == OP_IFNOT)
		Con_Printf("%s branch %i", PR_GlobalString((unsigned short)s->a), (unsigned short)s->b);
	else if (s->op == OP_GOTO)
		Con_Printf("branch %i", (unsigned short)s->a);
	else if (s->op >= OP_STOREP_F)
	{
		if (s->a)
			Con_Printf("%s", PR_GlobalString((unsigned short)s->a));
		if (s->b)
			Con_Printf("%s", PR_GlobalString((unsigned short)s->b));
		if (s->c)
			Con_Printf("%s", PR_GlobalStringNoContents((unsigned short)s->c));
	}
	else
	{
		Con_Printf("%s", PR_GlobalString((unsigned short)s->a));
		Con_Printf("%s", PR_GlobalStringNoContents((unsigned short)s->b));
	}
	Con_Printf("\n");
}

/*
============
PR_StackTrace
============
*/
void PR_StackTrace(void)
{
	dfunction_t	*f;
	int			i;

	if (pr_depth == 0)
		Con_Printf("<NO STACK>\n");
	return;		// FIXME: the stack is never printed

	pr_stack[pr_depth].f = pr_xfunction;
	for (i = pr_depth; i >= 0; i--)
	{
		f = pr_stack[i].f;

		if (!f)
			Con_Printf("<NO FUNCTION>\n");
		else
			Con_Printf("%12s : %s\n", pr_strings + f->s_name, pr_strings + f->s_file);
	}
}

/*
============
PR_Profile
============
*/
void PR_Profile(void)
{
	dfunction_t	*f, *best;
	int			max;
	int			num;
	int			i, c;

	for (num = 0; ; num++)
	{
		max = 0;
		best = NULL;
		c = ((dprograms_t *)progs)->numfunctions;
		f = pr_functions;
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
============
PR_RunError

Aborts the currently executing function
============
*/
void PR_RunError(char *error, ...)
{
	va_list	argptr;
	char	string[1024];

	va_start(argptr, error);
	vsprintf(string, error, argptr);
	va_end(argptr);

	PR_PrintStatement((dstatement_t *)pr_statements + pr_xstatement);
	PR_StackTrace();
	Con_Printf("%s\n", string);

	pr_depth = 0;		// dump the stack so host_error can shutdown functions

	Host_Error("Program error", string);
}

/*
============================================================================
PR_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PR_EnterFunction

Returns the new program statement counter
====================
*/
int PR_EnterFunction(dfunction_t *f)
{
	int		i, c, o;
	int		numparms, size;
	int		*src, *dest;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_depth++;
	if (pr_depth >= MAX_STACK_DEPTH)
		PR_RunError("stack overflow");

	// save off any locals that the new function steps on
	c = f->locals;
	if (c + pr_localstack_used > LOCALSTACK_SIZE)
		PR_RunError("PR_ExecuteProgram: locals stack overflow");

	if (c > 0)
		memcpy(&pr_localstack[pr_localstack_used], (int *)pr_globals + f->parm_start, c * sizeof(int));

	o = f->parm_start;
	pr_localstack_used += c;

	// copy parameters
	numparms = f->numparms;
	for (i = 0; i < numparms; i++)
	{
		size = f->parm_size[i];
		if (!size)
			continue;

		src = (int *)&pr_globals[OFS_PARM0 + i * 3];
		dest = (int *)pr_globals + o;
		o += size;
		do
		{
			*dest++ = *src++;
		} while (--size);
	}

	pr_xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PR_LeaveFunction
====================
*/
int PR_LeaveFunction(void)
{
	int c;

	if (pr_depth <= 0)
		Sys_Error("prog stack underflow");

	// restore locals from the stack
	c = pr_xfunction->locals;
	pr_localstack_used -= c;
	if (pr_localstack_used < 0)
		PR_RunError("PR_ExecuteProgram: locals stack underflow");

	if (c > 0)
		memcpy((int *)pr_globals + pr_xfunction->parm_start, &pr_localstack[pr_localstack_used], c * sizeof(int));

	// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;
	return pr_stack[pr_depth].s;
}

/*
====================
PR_ExecuteProgram
====================
*/
void PR_ExecuteProgram(func_t fnum)
{
	eval_t			*a, *b, *c;
	int				s;
	dstatement_t	*st;
	dfunction_t		*f, *newf;
	int				runaway;
	int				i;
	func_t			func;
	edict_t			*ed;
	int				exitdepth;
	float			nextthink;

	if (!fnum || fnum >= ((dprograms_t *)progs)->numfunctions)
	{
		if (pr_global_struct->self)
			ED_Print(PROG_TO_EDICT(pr_global_struct->self));
		Host_Error("PR_ExecuteProgram: NULL function");
	}

	exitdepth = pr_depth;
	runaway = MAX_RUNAWAY;
	pr_trace = false;

	f = (dfunction_t *)pr_functions + fnum;

	// a builtin can be called directly
	if (f->first_statement < 0)
	{
		i = -f->first_statement;
		if (i >= pr_numbuiltins)
			PR_RunError("Bad builtin call number");
		pr_builtins[i]();
		return;
	}

	// make a stack frame
	s = PR_EnterFunction(f);

	while (1)
	{
		s++;	// next statement

		st = (dstatement_t *)pr_statements + s;
		a = (eval_t *)&pr_globals[st->a];
		b = (eval_t *)&pr_globals[st->b];
		c = (eval_t *)&pr_globals[st->c];

		if (!--runaway)
			break;

		pr_xstatement = s;
		pr_xfunction->profile++;

		if (pr_trace)
			PR_PrintStatement(st);

		switch (st->op)
		{
		case OP_DONE:
		case OP_RETURN:
			G_INT(OFS_RETURN) = ((int *)a)[0];
			G_INT(OFS_RETURN + 1) = ((int *)a)[1];
			G_INT(OFS_RETURN + 2) = ((int *)a)[2];

			s = PR_LeaveFunction();
			if (pr_depth == exitdepth)
				return;		// all done
			continue;

		case OP_MUL_F:
			c->_float = b->_float * a->_float;
			continue;
		case OP_MUL_V:
			c->_float = b->vector[0] * a->vector[0]
				+ b->vector[1] * a->vector[1]
				+ b->vector[2] * a->vector[2];
			continue;
		case OP_MUL_FV:
			c->vector[0] = b->vector[0] * a->_float;
			c->vector[1] = b->vector[1] * a->_float;
			c->vector[2] = b->vector[2] * a->_float;
			continue;
		case OP_MUL_VF:
			c->vector[0] = a->vector[0] * b->_float;
			c->vector[1] = a->vector[1] * b->_float;
			c->vector[2] = a->vector[2] * b->_float;
			continue;

		case OP_DIV_F:
			c->_float = a->_float / b->_float;
			continue;

		case OP_ADD_F:
			c->_float = a->_float + b->_float;
			continue;
		case OP_ADD_V:
			c->vector[0] = a->vector[0] + b->vector[0];
			c->vector[1] = a->vector[1] + b->vector[1];
			c->vector[2] = a->vector[2] + b->vector[2];
			continue;

		case OP_SUB_F:
			c->_float = a->_float - b->_float;
			continue;
		case OP_SUB_V:
			c->vector[0] = a->vector[0] - b->vector[0];
			c->vector[1] = a->vector[1] - b->vector[1];
			c->vector[2] = a->vector[2] - b->vector[2];
			continue;

		case OP_EQ_F:
			c->_float = a->_float == b->_float;
			continue;
		case OP_EQ_V:
			if (a->vector[0] != b->vector[0] || a->vector[1] != b->vector[1] || a->vector[2] != b->vector[2])
				c->_float = 0;
			else
				c->_float = 1;
			continue;
		case OP_EQ_S:
			c->_float = strcmp(pr_strings + a->string, pr_strings + b->string) == 0;
			continue;
		case OP_EQ_E:
			c->_float = a->_int == b->_int;
			continue;
		case OP_EQ_FNC:
			c->_float = a->function == b->function;
			continue;

		case OP_NE_F:
			c->_float = a->_float != b->_float;
			continue;
		case OP_NE_V:
			if (a->vector[0] != b->vector[0] || a->vector[1] != b->vector[1] || a->vector[2] != b->vector[2])
				c->_float = 1;
			else
				c->_float = 0;
			continue;
		case OP_NE_S:
			c->_float = strcmp(pr_strings + a->string, pr_strings + b->string);
			continue;
		case OP_NE_E:
			c->_float = a->_int != b->_int;
			continue;
		case OP_NE_FNC:
			c->_float = a->function != b->function;
			continue;

		case OP_LE:
			c->_float = b->_float <= a->_float;
			continue;
		case OP_GE:
			c->_float = b->_float >= a->_float;
			continue;
		case OP_LT:
			c->_float = b->_float < a->_float;
			continue;
		case OP_GT:
			c->_float = b->_float > a->_float;
			continue;

		case OP_LOAD_F:
		case OP_LOAD_S:
		case OP_LOAD_ENT:
		case OP_LOAD_FLD:
		case OP_LOAD_FNC:
			c->_float = *EDICT_FIELD(a->edict, b->_int);
			continue;

		case OP_LOAD_V:
			c->vector[0] = EDICT_FIELD(a->edict, b->_int)[0];
			c->vector[1] = EDICT_FIELD(a->edict, b->_int)[1];
			c->vector[2] = EDICT_FIELD(a->edict, b->_int)[2];
			continue;

		case OP_ADDRESS:
			ed = (edict_t *)(sv_edicts_base + a->edict);
			if ((edict_t *)sv_edicts_base == ed && sv_edicts_active == 1)
				PR_RunError("assignment to world entity");
			c->_int = (byte *)((int *)&ed->v + b->_int) - (byte *)sv_edicts_base;
			continue;

		case OP_STORE_F:
		case OP_STORE_S:
		case OP_STORE_ENT:
		case OP_STORE_FLD:
		case OP_STORE_FNC:
			b->_float = a->_float;
			continue;
		case OP_STORE_V:
			b->vector[0] = a->vector[0];
			b->vector[1] = a->vector[1];
			b->vector[2] = a->vector[2];
			continue;

		case OP_STOREP_F:
		case OP_STOREP_S:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:
		case OP_STOREP_FNC:
			*PROG_POINTER(b->_int) = a->_float;
			continue;
		case OP_STOREP_V:
			PROG_POINTER(b->_int)[0] = a->vector[0];
			PROG_POINTER(b->_int)[1] = a->vector[1];
			PROG_POINTER(b->_int)[2] = a->vector[2];
			continue;

		case OP_NOT_F:
			c->_float = a->_float == 0;
			continue;
		case OP_NOT_V:
			if (a->vector[0] != 0 || a->vector[1] != 0 || a->vector[2] != 0)
				c->_float = 0;
			else
				c->_float = 1;
			continue;
		case OP_NOT_S:
			if (!a->string || !pr_strings[a->string])
				c->_float = 1;
			else
				c->_float = 0;
			continue;
		case OP_NOT_ENT:
			c->_float = a->edict == 0;
			continue;
		case OP_NOT_FNC:
			c->_float = a->function == 0;
			continue;

		case OP_IF:
			if (a->_int)
				s += st->b - 1;	// offset the s++
			continue;

		case OP_IFNOT:
			if (!a->_int)
				s += st->b - 1;	// offset the s++
			continue;

		case OP_CALL0:
		case OP_CALL1:
		case OP_CALL2:
		case OP_CALL3:
		case OP_CALL4:
		case OP_CALL5:
		case OP_CALL6:
		case OP_CALL7:
		case OP_CALL8:
			pr_argc = st->op - OP_CALL0;
			func = a->function;
			if (!func)
				PR_RunError("NULL function");

			newf = (dfunction_t *)pr_functions + func;

			if (newf->first_statement >= 0)
			{
				s = PR_EnterFunction(newf);
			}
			else
			{
				// negative statements are built in functions
				i = -newf->first_statement;
				if (i >= pr_numbuiltins)
					PR_RunError("Bad builtin call number");
				pr_builtins[i]();
			}
			continue;

		case OP_STATE:
			nextthink = pr_global_struct->time + 0.1f;
			ed = (edict_t *)(sv_edicts_base + pr_global_struct->self);
			ed->v.nextthink = nextthink;
			if (a->_float != ed->v.frame)
				ed->v.frame = a->_float;
			ed->v.think = b->function;
			continue;

		case OP_GOTO:
			s += st->a - 1;	// offset the s++
			continue;

		case OP_AND:
			if (a->_float == 0 || b->_float == 0)
				c->_float = 0;
			else
				c->_float = 1;
			continue;
		case OP_OR:
			if (a->_float != 0 || b->_float != 0)
				c->_float = 1;
			else
				c->_float = 0;
			continue;

		case OP_BITAND:
			c->_float = (int)a->_float & (int)b->_float;
			continue;
		case OP_BITOR:
			c->_float = (int)a->_float | (int)b->_float;
			continue;

		default:
			PR_RunError("Bad opcode %i", st->op);
		}
	}

	PR_RunError("runaway loop error");
}

void PR_trace_on(void)
{
	pr_trace = true;
}

void PR_trace_off(void)
{
	pr_trace = false;
}
