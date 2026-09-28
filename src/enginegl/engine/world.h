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
// world.h

#ifndef WORLD_H
#define WORLD_H

#include "mathlib.h"
#include "bspfile.h"
#include "model.h"

typedef struct edict_s edict_t;

#define MOVE_NORMAL		0
#define MOVE_NOMONSTERS	1		// only clip against bmodels
#define MOVE_MISSILE	2		// extra size for monsters

typedef struct moveclip_s
{
	vec3_t		boxmins, boxmaxs;	// enclose the test object along entire move
	vec3_t		mins, maxs;			// size of the moving object
	vec3_t		mins2, maxs2;		// size when clipping against monsters
	vec3_t		start, end;
	trace_t		trace;
	int			type;
	edict_t		*passedict;
	qboolean	monsterclip;		// passedict has FL_MONSTERCLIP
} moveclip_t;

typedef struct areanode_s
{
	int			axis;				// -1 = leaf node
	float		dist;
	struct areanode_s	*children[2];
	link_t		trigger_edicts;
	link_t		solid_edicts;
} areanode_t;

void SV_ClearWorld(void);
// called after the world model has been loaded, before linking any entities

areanode_t *SV_CreateAreaNode(int depth, vec3_t mins, vec3_t maxs);

void SV_UnlinkEdict(edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void SV_LinkEdict(edict_t *ent, qboolean touch_triggers);
// Needs to be called any time an entity changes origin, mins, maxs, or solid
// sets ent->v.absmin and ent->v.absmax
// if touchtriggers, calls the touch functions of the intersected triggers

int SV_PointContents(vec3_t p);
int SV_TruePointContents(vec3_t p);
// returns the CONTENTS_* value at the given point.
// the non-true version remaps the water current contents to CONTENTS_WATER
// and also checks non-solid entities that carry a contents value in v.skin

edict_t *SV_TestEntityPosition(edict_t *ent);

int SV_HullPointContents(hull_t *hull, int num, vec3_t p);
hull_t *SV_HullForBox(vec3_t mins, vec3_t maxs);
hull_t *SV_HullForEntity(edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset);

trace_t SV_Move(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict);
// mins and maxs are relative
// if the entire move stays in a solid volume, trace.allsolid will be set
// if the starting point is in a solid, it will be allowed to move out
// to an open area
// passedict is explicitly excluded from clipping checks (normally NULL)

trace_t SV_ClipMoveToEntity(edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);

qboolean SV_RecursiveHullCheck(hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace);

#endif // WORLD_H
