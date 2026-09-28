#ifndef EDICT_H
#define EDICT_H

#include "common.h"
#include "mathlib.h"
#include "progdefs.h"
#include <stddef.h>

// Entity flags (v.flags)
#define FL_FLY					(1<<0)
#define FL_SWIM					(1<<1)
#define FL_CONVEYOR				(1<<2)
#define FL_CLIENT				(1<<3)
#define FL_INWATER				(1<<4)
#define FL_MONSTER				(1<<5)
#define FL_GODMODE				(1<<6)
#define FL_NOTARGET				(1<<7)
#define FL_ITEM					(1<<8)
#define FL_ONGROUND				(1<<9)
#define FL_PARTIALGROUND		(1<<10)
#define FL_WATERJUMP			(1<<11)
#define FL_FROZEN				(1<<12)
#define FL_FAKECLIENT			(1<<13)
#define FL_DUCKING				(1<<14)
#define FL_FLOAT				(1<<15)
#define FL_GRAPHED				(1<<16)
#define FL_IMMUNE_WATER			(1<<17)
#define FL_IMMUNE_SLIME			(1<<18)
#define FL_IMMUNE_LAVA			(1<<19)
#define FL_ALWAYSTHINK			(1<<21)
#define FL_BASEVELOCITY			(1<<22)
#define FL_MONSTERCLIP			(1<<23)
#define FL_ONTRAIN				(1<<24)
#define FL_WORLDBRUSH			(1<<25)
#define FL_SPECTATOR			(1<<26)
#define FL_CUSTOMENTITY			(1<<29)
#define FL_KILLME				(1<<30)
#define FL_DORMANT				(1<<31)

// Trace structure
typedef struct
{
	qboolean	allsolid;
	qboolean	startsolid;
	qboolean	inopen;
	qboolean	inwater;
	float		fraction;
	vec3_t		endpos;
	struct {
		vec3_t	normal;
		float	dist;
	} plane;
	struct edict_s *ent;
} trace_t;

// entvars_t.takedamage values
#define DAMAGE_NO		0
#define DAMAGE_YES		1
#define DAMAGE_AIM		2

#define MAX_ENT_LEAFS	16

// Entity variables, shared with progs.dat and the game DLL
typedef struct entvars_s
{
	float		modelindex;
	vec3_t		absmin;
	vec3_t		absmax;
	float		ltime;
	float		movetype;
	float		solid;
	vec3_t		origin;
	vec3_t		oldorigin;
	vec3_t		velocity;
	vec3_t		angles;
	vec3_t		avelocity;
	vec3_t		basevelocity;
	vec3_t		punchangle;
	string_t	classname;
	string_t	model;
	float		skin;
	float		body;
	float		effects;
	float		gravity;
	float		friction;
	float		light_level;
	int			sequence;
	float		animtime;
	float		frame;
	float		framerate;
	int			controller;
	int			blending;
	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		size;
	func_t		touch;
	func_t		use;
	func_t		think;
	func_t		blocked;
	float		nextthink;
	int			groundentity;
	float		rendermode;
	float		renderamt;
	vec3_t		rendercolor;
	float		renderfx;
	float		health;
	float		frags;
	int			weapon;
	int			weapons;
	string_t	weaponmodel;
	float		weaponframe;
	float		currentammo;
	int			ammo_1;
	int			ammo_2;
	int			ammo_3;
	int			ammo_4;
	int			items;
	int			items2;
	float		takedamage;
	int			chain;
	float		deadflag;
	vec3_t		view_ofs;
	int			button;
	float		impulse;
	float		fixangle;
	vec3_t		v_angle;
	float		idealpitch;
	float		pitch_speed;
	string_t	netname;
	int			enemy;
	float		flags;
	float		colormap;
	float		team;
	float		max_health;
	float		teleport_time;
	float		armortype;
	float		armorvalue;
	float		waterlevel;
	float		watertype;
	float		ideal_yaw;
	float		yaw_speed;
	int			aiment;
	int			goalentity;
	float		spawnflags;
	string_t	target;
	string_t	targetname;
	float		dmg_take;
	float		dmg_save;
	int			dmg_inflictor;
	int			owner;
	vec3_t		movedir;
	string_t	message;
	float		sounds;
	string_t	noise;
	string_t	noise1;
	string_t	noise2;
	string_t	noise3;
	float		speed;
	float		dmg;
	float		dmgtime;
	float		air_finished;
	float		pain_finished;
	float		radsuit_finished;
	struct edict_s *pContainingEntity;
	void		*pSystemGlobals;
} entvars_t;

// Edict structure
typedef struct edict_s
{
	qboolean	free;
	link_t		area;				// linked to a division node or leaf

	int			num_leafs;
	short		leafnums[MAX_ENT_LEAFS];

	entity_state_t	baseline;

	float		freetime;			// sv.time when the object was freed
	void		*pvPrivateData;		// the game DLL's object, allocated by ED_AllocPrivateData
	entvars_t	v;					// C exported fields from progs
} edict_t;

// ED_CallDispatch callbacks into the game DLL
#define DISPATCH_SPAWN		0
#define DISPATCH_THINK		1
#define DISPATCH_TOUCH		2
#define DISPATCH_USE		3
#define DISPATCH_BLOCKED	4
#define DISPATCH_KEYVALUE	5
#define DISPATCH_SAVE		6

#define EDICT_FROM_AREA(l)	((edict_t *)((byte *)(l) - offsetof(edict_t, area)))
#define NEXT_EDICT(e)		((edict_t *)((byte *)(e) + pr_edict_size))

edict_t *EDICT_NUM(int n);
int NUM_FOR_EDICT(edict_t *e);
edict_t *PROG_TO_EDICT(int e);
int EDICT_TO_PROG(edict_t *e);
int EDICT_INDEX(int offset);
void *EDICT_TO_ENTVAR(edict_t *ent);
edict_t *ENTVAR_TO_EDICT(void *entvar);

void ED_Init(void);
edict_t *ED_Alloc(void);
edict_t *ED_ClearEdict(edict_t *ed);
void ED_Free(edict_t *ed);
edict_t *ED_SetEdictPointers(edict_t *e);

void *ED_AllocPrivateData(edict_t *ent, int size);
void *ED_GetPrivateData(edict_t *ent);
void ED_FreePrivateData(edict_t *ent);

#if defined(_DEBUG)
void ED_DispatchEnter(void);
void ED_DispatchExit(void);
#endif
void *ED_GetDispatch(void *ent, int callback_type);
void *ED_CallDispatch(void *ent, int callback_type, void *param);

void ED_Print(edict_t *ed);
void ED_Write(savebuf_t *sb, edict_t *ed);
void ED_WriteGlobals(savebuf_t *sb);
void ED_PrintNum(int entnum);
void ED_LoadFromFile(char *data);
void ED_ParseGlobals(char *data);
char *ED_ParseEdict(char *data, edict_t *ent);

void *GetEdictFieldValue(void *ent, const char *field);

#endif // EDICT_H
