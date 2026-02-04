
#ifndef PROGDEFS_H
#define PROGDEFS_H

#include "mathlib.h"

 // QuakeC VM types
typedef int func_t; // function index in progs
typedef int string_t; // string offset in progs string table

 // QuakeC VM constants
#define MAX_STACK_DEPTH     32
#define LOCALSTACK_SIZE     2048
#define MAX_REGS            2048

 // QuakeC message destinations (used by PF_Write* builtins)
#define MSG_BROADCAST 0
#define MSG_ONE       1
#define MSG_ALL       2
#define MSG_INIT      3

 // QuakeC damage flags (entvars_t.takedamage)
#define DAMAGE_NO     0
#define DAMAGE_YES    1
#define DAMAGE_AIM    2

 // dfunction_t - Function definition in progs.dat
typedef struct
{
	int		first_statement;
	int		parm_start;
	int		locals;
	int		profile;
	int		s_name;
	int		s_file;
	int		num_parms;
	unsigned char parm_size[8];
} dfunction_t;

 // dprograms_t - Progs.dat header
typedef struct
{
	int		version;
	int		crc;
	int		ofs_statements;
	int		numstatements;
	int		ofs_globaldefs;
	int		numglobaldefs;
	int		ofs_fielddefs;
	int		numfielddefs;
	int		ofs_functions;
	int		numfunctions;
	int		ofs_strings;
	int		numstrings;
	int		ofs_globals;
	int		numglobals;
	int		entityfields;
} dprograms_t;

 // ddef_t - Variable definition in progs.dat
typedef struct
{
	unsigned short	type;
	unsigned short	ofs;
	int				s_name;
} ddef_t;

 // dstatement_t - Statement in progs.dat
typedef struct
{
	unsigned short	op;
	short	a, b, c;
} dstatement_t;

 // =========================================================
 // QuakeC VM Parameter Offsets
 // =========================================================

#define OFS_NULL        0
#define OFS_RETURN      1
#define OFS_PARM0       4 // leave 3 ofs for each parm to hold vectors
#define OFS_PARM1       7
#define OFS_PARM2       10
#define OFS_PARM3       13
#define OFS_PARM4       16
#define OFS_PARM5       19
#define OFS_PARM6       22
#define OFS_PARM7       25

 // =========================================================
 // QuakeC VM Global Access Macros
 // =========================================================

 // (Moved to end of file)

 // String functions (in pr_edict.c)
char *PROG_TO_STRING(int offset);
int ED_AllocString(const char *string);
char *PR_GlobalString(int ofs);
char *PR_GlobalStringNoContents(int ofs);

 // Progs/ VM entry points (pr_exec.c/ pr_edict.c)
void PR_ExecuteProgram(int fnum);
void PR_RunError(char *fmt, ...);
void PR_LoadProgs(void);

 // Aliases for standard Quake naming
#define PR_GetString(o)  PROG_TO_STRING(o)
#define PR_SetString(s)  ED_AllocString(s)

 // Access progs globals as different types
#define G_FLOAT(o)      (pr_globals[o])
#define G_INT(o)        (*(int *)&pr_globals[o])
#define G_EDICT(o)      ((edict_t *)((byte *)sv.edicts + *(int *)&pr_globals[o]))
#define G_EDICTNUM(o)   NUM_FOR_EDICT(G_EDICT(o))
#define G_VECTOR(o)     (&pr_globals[o])
#define G_STRING(o)     (PR_GetString(*(int *)&pr_globals[o]))
#define G_FUNCTION(o)   (*(func_t *)&pr_globals[o])

 // Return value helpers
#define RETURN_EDICT(e) (*(int *)&pr_globals[OFS_RETURN] = EDICT_TO_PROG(e))
#define RETURN_STRING(s) (*(int *)&pr_globals[OFS_RETURN] = PR_SetString(s))


 // global variables

typedef struct globalvars_s
{
	float		pad0[1]; // 0: OFS_NULL
	float		v_return[3]; // 1: OFS_RETURN (vector or float)
	float		arg0[3]; // 4: OFS_PARM0
	float		arg1[3]; // 7: OFS_PARM1
	float		arg2[3]; // 10: OFS_PARM2
	float		arg3[3]; // 13: OFS_PARM3
	float		arg4[3]; // 16: OFS_PARM4
	float		arg5[3]; // 19: OFS_PARM5
	float		arg6[3]; // 22: OFS_PARM6
	float		arg7[3]; // 25: OFS_PARM7
	int			self; // 28
	int			other; // 29
	int			world; // 30
	float		time; // 31
	float		frametime; // 32
	float		force_retouch; // 33
	string_t	mapname; // 34
	string_t	startspot; // 35
	float		deathmatch; // 36
	float		coop; // 37
	float		teamplay; // 38
	float		serverflags; // 39
	float		total_secrets; // 40
	float		total_monsters; // 41
	float		found_secrets; // 42
	float		killed_monsters; // 43
	float		parm1; // 44
	float		parm2; // 45
	float		parm3; // 46
	float		parm4; // 47
	float		parm5; // 48
	float		parm6; // 49
	float		parm7; // 50
	float		parm8; // 51
	float		parm9; // 52
	float		parm10; // 53
	float		parm11; // 54
	float		parm12; // 55
	float		parm13; // 56
	float		parm14; // 57
	float		parm15; // 58
	float		parm16; // 59
	vec3_t		v_forward; // 60
	vec3_t		v_up; // 63
	vec3_t		v_right; // 66
	float		trace_allsolid; // 69
	float		trace_startsolid; // 70
	float		trace_fraction; // 71
	vec3_t		trace_endpos; // 72
	vec3_t		trace_plane_normal; // 75
	float		trace_plane_dist; // 78
	int			trace_ent; // 79
	float		trace_inopen; // 80
	float		trace_inwater; // 81
	int			msg_entity; // 82
	func_t		main; // 83
	func_t		StartFrame; // 84
	func_t		PlayerPreThink; // 85
	func_t		PlayerPostThink; // 86
	func_t		ClientKill; // 87
	func_t		ClientConnect; // 88
	func_t		PutClientInServer; // 89
	func_t		ClientDisconnect; // 90
	func_t		SetNewParms; // 91
	func_t		SetChangeParms; // 92
} globalvars_t;

extern globalvars_t *pr_global_struct;
extern float *pr_globals;
extern char *pr_strings;

 // Compatibility alias used by some translations.
#define gGlobalVariables (*pr_global_struct)

#endif // PROGDEFS_H
