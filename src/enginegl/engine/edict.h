#ifndef EDICT_H
#define EDICT_H

#include "common.h"
#include "mathlib.h"
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

 // Maximum BSP leaves tracked per edict (leafnums[]).
#define MAX_ENT_LEAFS 16

 // Entity variables (edict_t::v)
 // Layout is defined by `progs.dat` fielddefs for this build (entityfields = 132).
typedef struct entvars_s
{
	float		modelindex; // 0
	vec3_t		absmin; // 1..3
	vec3_t		absmax; // 4..6
	float		ltime; // 7
	float		movetype; // 8
	float		solid; // 9
	vec3_t		origin; // 10..12
	vec3_t		oldorigin; // 13..15
	vec3_t		velocity; // 16..18
	vec3_t		angles; // 19..21
	vec3_t		avelocity; // 22..24
	vec3_t		basevelocity; // 25..27
	vec3_t		punchangle; // 28..30
	int			classname; // 31
	int			model; // 32
	float		skin; // 33
	float		body; // 34
	float		effects; // 35
	float		gravity; // 36
	float		friction; // 37
	float		light_level; // 38
	int			sequence; // 39
	float		animtime; // 40
	float		frame; // 41
	float		framerate; // 42
	int			controller; // 43
	int			blending; // 44
	vec3_t		mins; // 45..47
	vec3_t		maxs; // 48..50
	vec3_t		size; // 51..53
	int			touch; // 54
	int			use; // 55
	int			think; // 56
	int			blocked; // 57
	float		nextthink; // 58
	int			groundentity; // 59
	float		rendermode; // 60
	float		renderamt; // 61
	vec3_t		rendercolor; // 62..64
	float		renderfx; // 65
	float		health; // 66
	float		frags; // 67
	int			weapon; // 68
	int			weapons; // 69
	int			weaponmodel; // 70
	float		weaponframe; // 71
	float		currentammo; // 72
	int			ammo_1; // 73
	int			ammo_2; // 74
	int			ammo_3; // 75
	int			ammo_4; // 76
	int			items; // 77
	int			items2; // 78
	float		takedamage; // 79
	int			chain; // 80
	float		deadflag; // 81
	vec3_t		view_ofs; // 82..84
	int			button; // 85
	float		impulse; // 86
	float		fixangle; // 87
	vec3_t		v_angle; // 88..90
	float		idealpitch; // 91
	float		pitch_speed; // 92
	int			netname; // 93
	int			enemy; // 94
	float		flags; // 95
	float		colormap; // 96
	float		team; // 97
	float		max_health; // 98
	float		teleport_time; // 99
	float		armortype; // 100
	float		armorvalue; // 101
	float		waterlevel; // 102
	float		watertype; // 103
	float		ideal_yaw; // 104
	float		yaw_speed; // 105
	int			aiment; // 106
	int			goalentity; // 107
	float		spawnflags; // 108
	int			target; // 109
	int			targetname; // 110
	float		dmg_take; // 111
	float		dmg_save; // 112
	int			dmg_inflictor; // 113
	int			owner; // 114
	vec3_t		movedir; // 115..117
	int			message; // 118
	float		sounds; // 119
	int			noise; // 120
	int			noise1; // 121
	int			noise2; // 122
	int			noise3; // 123
	float		speed; // 124
	float		dmg; // 125
	float		dmgtime; // 126
	float		air_finished; // 127
	float		pain_finished; // 128
	float		radsuit_finished; // 129
	struct edict_s *pContainingEntity; // 130
	void		*pSystemGlobals; // 131
} entvars_t;

 // Edict structure
typedef struct edict_s
{
	int			free;
	link_t		area; // world link
	int			num_leafs; // 0..MAX_ENT_LEAFS
	short		leafnums[MAX_ENT_LEAFS];
	// Extra header state used by this build (baseline/ misc).
	byte		header_data[68];
	void		*pvPrivateData; // allocated via ED_AllocPrivateData
	entvars_t	v; // progs fields
} edict_t;

#if defined(_DEBUG)
void ED_DispatchEnter(void);
void ED_DispatchExit(void);
#endif

// Edict accessors (implemented in pr_edict.c)
edict_t *EDICT_NUM(int n);
int NUM_FOR_EDICT(void *e);
edict_t *PROG_TO_EDICT(int e);
int EDICT_TO_PROG(void *e);
int EDICT_INDEX(int offset);
void *EDICT_TO_ENTVAR(edict_t *ent);
edict_t *ENTVAR_TO_EDICT(void *entvar);

void ED_Init(void);
edict_t *ED_Alloc(void);
void ED_Free(edict_t *ed);
void ED_FreePrivateData(edict_t *ent);
void ED_LoadFromFile(char *data);
void ED_ParseGlobals(char *data);
char *ED_ParseEdict(char *data, edict_t *ent);
void ED_Print(void *ed);

 // Link back from link_t::area to owning edict.
#define EDICT_FROM_AREA(l) ((edict_t *)((byte *)(l) - offsetof(edict_t, area)))

 // Iterate edicts in the contiguous edict buffer (stride = pr_edict_size).
extern int pr_edict_size;
#define NEXT_EDICT(e) ((edict_t *)((byte *)(e) + pr_edict_size))

#endif // EDICT_H
