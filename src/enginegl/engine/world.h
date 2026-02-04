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

#ifndef WORLD_H
#define WORLD_H

#include "mathlib.h"
#include "bspfile.h"
#include "model.h"

 // =============================================================================
 // Forward declarations
 // =============================================================================

typedef struct edict_s edict_t;

 // =============================================================================
 // Trace types
 // =============================================================================

#define MOVE_NORMAL			0 // normal trace
#define MOVE_NOMONSTERS		1 // ignore monsters (edicts with FL_MONSTER/ FL_FAKECLIENT/ FL_CLIENT)
#define MOVE_MISSILE		2 // extra size for monsters

 // =============================================================================
 // Move clip structure for tracing
 // =============================================================================

typedef struct moveclip_s
{
	vec3_t      boxmins, boxmaxs; // Enclose the test object along entire move
	vec3_t      mins, maxs; // Size of the moving object
	vec3_t      mins2, maxs2; // Size when clipping against monsters
	vec3_t      start, end;
	trace_t     trace;
	int         type;
	edict_t     *passedict;
	qboolean    monsterclip;
} moveclip_t;

 // =============================================================================
 // Area node structure for spatial partitioning
 // =============================================================================

typedef struct areanode_s
{
	int         axis; // -1 = leaf node
	float       dist;
	struct areanode_s *children[2];
	link_t      trigger_edicts;
	link_t      solid_edicts;
} areanode_t;

 // =============================================================================
 // World collision and tracing functions
 // =============================================================================

void        SV_ClearWorld(void);
areanode_t  *SV_CreateAreaNode(int depth, vec3_t mins, vec3_t maxs);
void        SV_UnlinkEdict(edict_t *ent);
void        SV_LinkEdict(edict_t *ent, qboolean touch_triggers);
int         SV_HullPointContents(hull_t *hull, int num, vec3_t p);
qboolean    SV_RecursiveHullCheck(hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace);
hull_t      *SV_HullForBox(vec3_t mins, vec3_t maxs);
hull_t      *SV_HullForEntity(edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset);
hull_t      *SV_HullForBsp(edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset);
trace_t     SV_ClipMoveToEntity(edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
trace_t     SV_Move(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict);
trace_t    *SV_Trace_Toss(trace_t *result, edict_t *ent_start, edict_t *passent);
int         SV_PointContents(vec3_t p);
int         SV_TruePointContents(vec3_t p);
int         SV_CheckBottom(edict_t *ent);
int         SV_movestep(edict_t *ent, float *move, int relink);

#endif // WORLD_H
