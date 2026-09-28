#ifndef PROGDEFS_H
#define PROGDEFS_H

#include "common.h"
#include "mathlib.h"

typedef int	func_t;
typedef int	string_t;

#define PROG_VERSION	6
#define PROGHEADER_CRC	58783

#define MAX_STACK_DEPTH		32
#define LOCALSTACK_SIZE		2048
#define MAX_PARMS			8

typedef enum {ev_void, ev_string, ev_float, ev_vector, ev_entity, ev_field, ev_function, ev_pointer} etype_t;

#define DEF_SAVEGLOBAL	(1<<15)

#define OFS_NULL		0
#define OFS_RETURN		1
#define OFS_PARM0		4		// leave 3 ofs for each parm to hold vectors
#define OFS_PARM1		7
#define OFS_PARM2		10
#define OFS_PARM3		13
#define OFS_PARM4		16
#define OFS_PARM5		19
#define OFS_PARM6		22
#define OFS_PARM7		25

enum {
	OP_DONE,
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,

	OP_EQ_F,
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,

	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,

	OP_LE,
	OP_GE,
	OP_LT,
	OP_GT,

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,
	OP_CALL0,
	OP_CALL1,
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,
	OP_GOTO,
	OP_AND,
	OP_OR,

	OP_BITAND,
	OP_BITOR,

	NUM_OPCODES
};

typedef struct statement_s
{
	unsigned short	op;
	short			a, b, c;
} dstatement_t;

typedef struct
{
	unsigned short	type;		// if DEF_SAVEGLOBAL bit is set
								// the variable needs to be saved in savegames
	unsigned short	ofs;
	int				s_name;
} ddef_t;

typedef struct
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;				// total ints of parms + locals

	int		profile;			// runtime

	int		s_name;
	int		s_file;				// source file defined in

	int		numparms;
	byte	parm_size[MAX_PARMS];
} dfunction_t;

typedef struct
{
	int		version;
	int		crc;				// check of header file

	int		ofs_statements;
	int		numstatements;		// statement 0 is an error

	int		ofs_globaldefs;
	int		numglobaldefs;

	int		ofs_fielddefs;
	int		numfielddefs;

	int		ofs_functions;
	int		numfunctions;		// function 0 is an empty

	int		ofs_strings;
	int		numstrings;			// first string is a null string

	int		ofs_globals;
	int		numglobals;

	int		entityfields;
} dprograms_t;

typedef union eval_s
{
	string_t	string;
	float		_float;
	float		vector[3];
	func_t		function;
	int			_int;
	int			edict;
} eval_t;

typedef struct globalvars_s
{
	float		pad0[1];
	float		v_return[3];
	float		arg0[3];
	float		arg1[3];
	float		arg2[3];
	float		arg3[3];
	float		arg4[3];
	float		arg5[3];
	float		arg6[3];
	float		arg7[3];
	int			self;
	int			other;
	int			world;
	float		time;
	float		frametime;
	float		force_retouch;
	string_t	mapname;
	string_t	startspot;
	float		deathmatch;
	float		coop;
	float		teamplay;
	float		serverflags;
	float		total_secrets;
	float		total_monsters;
	float		found_secrets;
	float		killed_monsters;
	float		parm1;
	float		parm2;
	float		parm3;
	float		parm4;
	float		parm5;
	float		parm6;
	float		parm7;
	float		parm8;
	float		parm9;
	float		parm10;
	float		parm11;
	float		parm12;
	float		parm13;
	float		parm14;
	float		parm15;
	float		parm16;
	vec3_t		v_forward;
	vec3_t		v_up;
	vec3_t		v_right;
	float		trace_allsolid;
	float		trace_startsolid;
	float		trace_fraction;
	vec3_t		trace_endpos;
	vec3_t		trace_plane_normal;
	float		trace_plane_dist;
	int			trace_ent;
	float		trace_inopen;
	float		trace_inwater;
	int			msg_entity;
	func_t		main;
	func_t		StartFrame;
	func_t		PlayerPreThink;
	func_t		PlayerPostThink;
	func_t		ClientKill;
	func_t		ClientConnect;
	func_t		PutClientInServer;
	func_t		ClientDisconnect;
	func_t		SetNewParms;
	func_t		SetChangeParms;
} globalvars_t;

//============================================================================

#define G_FLOAT(o)		(pr_globals[o])
#define G_INT(o)		(*(int *)&pr_globals[o])
#define G_EDICT(o)		((edict_t *)((byte *)sv.edicts + *(int *)&pr_globals[o]))
#define G_EDICTNUM(o)	NUM_FOR_EDICT(G_EDICT(o))
#define G_VECTOR(o)		(&pr_globals[o])
#define G_STRING(o)		(PROG_TO_STRING(*(int *)&pr_globals[o]))
#define G_FUNCTION(o)	(*(func_t *)&pr_globals[o])

#define RETURN_EDICT(e)	(*(int *)&pr_globals[OFS_RETURN] = EDICT_TO_PROG(e))

#define PR_GetString(o)	PROG_TO_STRING(o)

extern unsigned short	pr_crc;
extern int				type_size[8];

extern void			(*pr_builtins[])(void);
extern int			pr_numbuiltins;

extern int			pr_argc;
extern qboolean		pr_trace;
extern dfunction_t	*pr_xfunction;
extern int			pr_xstatement;

void PR_LoadProgs(void);
void PR_Profile_f(void);

void PR_ExecuteProgram(func_t fnum);
void PR_RunError(char *error, ...);

void PR_trace_on(void);
void PR_trace_off(void);

char *PROG_TO_STRING(int offset);
int ED_AllocString(const char *string);

char *PR_ValueString(int type, void *val);
char *PR_UglyValueString(int type, void *val);
char *PR_GlobalString(int ofs);
char *PR_GlobalStringNoContents(int ofs);

#endif // PROGDEFS_H
