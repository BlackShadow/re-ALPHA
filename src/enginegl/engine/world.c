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

#ifndef DIST_EPSILON
#define DIST_EPSILON (0.03125f)
#endif

extern void *ED_CallDispatch(void *ent, int callback_type, void *param);

static hull_t box_hull;
static dclipnode_t box_clipnodes[6];
static mplane_t box_planes[6];

#define AREA_DEPTH 4
#define AREA_NODES 32

static areanode_t sv_areanodes[AREA_NODES];
static int sv_numareanodes;

void SV_InitBoxHull(void);

void SV_InitBoxHull(void)
{
	int i;
	int side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i = 0; i < 6; i++)
	{
		box_clipnodes[i].planenum = i;

		side = i & 1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side ^ 1] = i + 1;
		else
			box_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;

		box_planes[i].type = i >> 1;
		box_planes[i].normal[i >> 1] = 1.0;
	}
}

hull_t *SV_HullForBox(vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

hull_t *SV_HullForEntity(edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset)
{
	model_t *model;
	vec3_t size;
	vec3_t hullmins, hullmaxs;
	hull_t *hull;

	if (ent->v.solid == SOLID_BSP)
	{

		if (ent->v.movetype != MOVETYPE_PUSH)
			Sys_Error("SOLID_BSP without MOVETYPE_PUSH");

		model = sv.models[(int)ent->v.modelindex];

		if (!model || model->type != mod_brush)
			Sys_Error("Hit a %s with no model (%s)", pr_strings + ent->v.classname, pr_strings + ent->v.model);

		VectorSubtract(maxs, mins, size);
		if (size[0] < 3.0)
			hull = &model->hulls[0];
		else if (size[0] <= 36.0)
		{
			if (size[2] <= 36.0)
				hull = &model->hulls[3];
			else
				hull = &model->hulls[1];
		}
		else
			hull = &model->hulls[2];

		VectorSubtract(hull->clip_mins, mins, offset);
		VectorAdd(offset, ent->v.origin, offset);
	}
	else
	{

		VectorSubtract(ent->v.mins, maxs, hullmins);
		VectorSubtract(ent->v.maxs, mins, hullmaxs);
		hull = SV_HullForBox(hullmins, hullmaxs);

		VectorCopy(ent->v.origin, offset);
	}

	return hull;
}

int SV_HullPointContents(hull_t *hull, int num, vec3_t p)
{
	int result;
	dclipnode_t *clipnodes;
	mplane_t *planes;

	union
	{
		float f;
		int i;
	} bits;

	result = num;
	if (num >= 0)
	{
		clipnodes = hull->clipnodes;
		planes = hull->planes;

		do
		{
			dclipnode_t *node;
			mplane_t *plane;
			float d;
			int mask;

			node = &clipnodes[result];
			plane = &planes[node->planenum];

			if (plane->type < 3u)
				d = p[plane->type] - plane->dist;
			else
				d = plane->normal[0] * p[0] + plane->normal[1] * p[1] + plane->normal[2] * p[2] - plane->dist;

			bits.f = d;
			mask = bits.i >> 31;
			result = (mask & node->children[1]) | (~mask & node->children[0]);
		} while (result >= 0);
	}

	return result;
}

int SV_LinkContents(areanode_t *node, vec3_t p)
{
	link_t *l;
	edict_t *touch;

	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = l->next)
	{
		touch = EDICT_FROM_AREA(l);

		if (touch->v.solid != SOLID_NOT)
			continue;

		if (touch->v.absmax[0] < p[0] || touch->v.absmax[1] < p[1] || touch->v.absmax[2] < p[2] ||
			touch->v.absmin[0] > p[0] || touch->v.absmin[1] > p[1] || touch->v.absmin[2] > p[2])
			continue;

		return (int)touch->v.skin;
	}

	if (node->axis == -1)
		return CONTENTS_EMPTY;

	if (p[node->axis] > node->dist)
		return SV_LinkContents(node->children[0], p);

	if (p[node->axis] < node->dist)
		return SV_LinkContents(node->children[1], p);

	return CONTENTS_EMPTY;
}

/*
==================
SV_AreaEdicts
==================
*/
typedef struct
{
	vec3_t		mins, maxs;
	edict_t		**list;
	int			count, maxcount;
	int			type;
} area_parms_t;

void SV_AreaEdicts_r(areanode_t *node, area_parms_t *parms)
{
	link_t		*l, *next, *start;
	edict_t		*touch;
	int			i;

	start = &node->trigger_edicts;
	for (l = start->next; l != start; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch->v.solid == SOLID_NOT)
			continue;

		for (i = 0; i < 3; i++)
		{
			if (touch->v.absmin[i] > parms->maxs[i] ||
				touch->v.absmax[i] < parms->mins[i])
				break;
		}
		if (i != 3)
			continue;

		if (parms->count == parms->maxcount)
		{
			Con_Printf("SV_AreaEdicts: overflow\n");
			return;
		}

		parms->list[parms->count++] = touch;
	}

	start = &node->solid_edicts;
	for (l = start->next; l != start; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch->v.solid == SOLID_NOT)
			continue;

		for (i = 0; i < 3; i++)
		{
			if (touch->v.absmin[i] > parms->maxs[i] ||
				touch->v.absmax[i] < parms->mins[i])
				break;
		}
		if (i != 3)
			continue;

		if (parms->count == parms->maxcount)
		{
			Con_Printf("SV_AreaEdicts: overflow\n");
			return;
		}

		parms->list[parms->count++] = touch;
	}

	if (node->axis == -1)
		return;

	if (parms->maxs[node->axis] > node->dist)
		SV_AreaEdicts_r(node->children[0], parms);
	if (parms->mins[node->axis] < node->dist)
		SV_AreaEdicts_r(node->children[1], parms);
}

int SV_AreaEdicts(vec3_t mins, vec3_t maxs, edict_t **list, int maxcount, int type)
{
	area_parms_t parms;

	VectorCopy(mins, parms.mins);
	VectorCopy(maxs, parms.maxs);
	parms.list = list;
	parms.count = 0;
	parms.maxcount = maxcount;
	parms.type = type;

	SV_AreaEdicts_r(sv_areanodes, &parms);

	return parms.count;
}

int SV_PointContents(vec3_t p)
{
	int world_contents;
	int entity_contents;

	world_contents = SV_HullPointContents(&sv.worldmodel->hulls[0], 0, p);

	if (world_contents <= CONTENTS_CURRENT_0 && world_contents >= CONTENTS_CURRENT_DOWN)
		world_contents = CONTENTS_WATER;

	entity_contents = SV_LinkContents(sv_areanodes, p);
	if (entity_contents == CONTENTS_EMPTY)
		return world_contents;

	return entity_contents;
}

int SV_TruePointContents(vec3_t p)
{
	return SV_HullPointContents(&sv.worldmodel->hulls[0], 0, p);
}

edict_t *SV_TestEntityPosition(edict_t *ent)
{
	trace_t trace;

	trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, 0, ent);

	if (trace.startsolid)
		return sv_edicts;

	return NULL;
}

qboolean SV_RecursiveHullCheck(hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace)
{
	dclipnode_t *node;
	mplane_t *plane;
	float t1, t2;
	float frac;
	float midf;
	int side;
	vec3_t mid;

	while (num >= 0)
	{
		if (hull->firstclipnode > hull->lastclipnode)
			break;

		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error("SV_RecursiveHullCheck: bad node number");

		if (!hull->planes)
			return false;

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if (plane->type < 3)
		{
			t1 = p1[plane->type] - plane->dist;
			t2 = p2[plane->type] - plane->dist;
		}
		else
		{
			t1 = DotProduct(plane->normal, p1) - plane->dist;
			t2 = DotProduct(plane->normal, p2) - plane->dist;
		}

		if (t1 < 0.0 || t2 < 0.0)
		{
			if (t1 >= 0.0 || t2 >= 0.0)
			{

				if (t1 >= 0.0)
					frac = (t1 - DIST_EPSILON) / (t1 - t2);
				else
					frac = (t1 + DIST_EPSILON) / (t1 - t2);

				if (frac < 0.0)
					frac = 0.0;
				if (frac > 1.0)
					frac = 1.0;

				midf = p1f + frac * (p2f - p1f);

				mid[0] = p1[0] + frac * (p2[0] - p1[0]);
				mid[1] = p1[1] + frac * (p2[1] - p1[1]);
				mid[2] = p1[2] + frac * (p2[2] - p1[2]);

				side = (t1 < 0.0);

				if (!SV_RecursiveHullCheck(hull, node->children[side], p1f, midf, p1, mid, trace))
					return false;

				if (SV_HullPointContents(hull, node->children[side ^ 1], mid) != CONTENTS_SOLID)
					return SV_RecursiveHullCheck(hull, node->children[side ^ 1], midf, p2f, mid, p2, trace);

				if (trace->allsolid)
					return false;

				if (side)
				{
					trace->plane.normal[0] = -plane->normal[0];
					trace->plane.normal[1] = -plane->normal[1];
					trace->plane.normal[2] = -plane->normal[2];
					trace->plane.dist = -plane->dist;
				}
				else
				{
					VectorCopy(plane->normal, trace->plane.normal);
					trace->plane.dist = plane->dist;
				}

				while (SV_HullPointContents(hull, hull->firstclipnode, mid) == CONTENTS_SOLID)
				{

					frac -= 0.1;
					if (frac < 0.0)
					{
						trace->fraction = midf;
						VectorCopy(mid, trace->endpos);
						Con_DPrintf("backup past 0\n");
						return false;
					}

					midf = p1f + frac * (p2f - p1f);

					mid[0] = p1[0] + frac * (p2[0] - p1[0]);
					mid[1] = p1[1] + frac * (p2[1] - p1[1]);
					mid[2] = p1[2] + frac * (p2[2] - p1[2]);
				}

				trace->fraction = midf;
				VectorCopy(mid, trace->endpos);

				return false;
			}

			num = node->children[1];
			continue;
		}

		num = node->children[0];
		continue;
	}

	if (num == CONTENTS_SOLID)
	{
		trace->startsolid = true;
	}
	else
	{
		trace->allsolid = false;
		if (num == CONTENTS_EMPTY)
			trace->inopen = true;
		else if (num != CONTENTS_TRANSLUCENT)
			trace->inwater = true;
	}

	return true;
}

trace_t SV_ClipMoveToEntity(edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t trace;
	vec3_t offset;
	vec3_t start_l, end_l;
	hull_t *hull;

	memset(&trace, 0, sizeof(trace_t));
	trace.fraction = 1.0;
	trace.allsolid = true;
	VectorCopy(end, trace.endpos);

	hull = SV_HullForEntity(ent, mins, maxs, offset);

	VectorSubtract(start, offset, start_l);
	VectorSubtract(end, offset, end_l);

	if (ent->v.solid == SOLID_BSP && (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2]))
	{
		vec3_t forward, right, up;
		vec3_t temp;

		AngleVectors(ent->v.angles, forward, right, up);

		VectorCopy(start_l, temp);
		start_l[0] = DotProduct(temp, forward);
		start_l[1] = -DotProduct(temp, right);
		start_l[2] = DotProduct(temp, up);

		VectorCopy(end_l, temp);
		end_l[0] = DotProduct(temp, forward);
		end_l[1] = -DotProduct(temp, right);
		end_l[2] = DotProduct(temp, up);
	}

	SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.0, 1.0, start_l, end_l, &trace);

	if (ent->v.solid == SOLID_BSP && (ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2]) && trace.fraction != 1.0)
	{
		vec3_t forward, right, up;
		vec3_t temp;
		vec3_t angles;

		VectorSubtract(vec3_origin, ent->v.angles, angles);
		AngleVectors(angles, forward, right, up);

		VectorCopy(trace.endpos, temp);
		trace.endpos[0] = DotProduct(temp, forward);
		trace.endpos[1] = -DotProduct(temp, right);
		trace.endpos[2] = DotProduct(temp, up);

		VectorCopy(trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct(temp, forward);
		trace.plane.normal[1] = -DotProduct(temp, right);
		trace.plane.normal[2] = DotProduct(temp, up);
	}

	if (trace.fraction != 1.0)
		VectorAdd(trace.endpos, offset, trace.endpos);

	if (trace.fraction < 1.0 || trace.startsolid)
		trace.ent = ent;

	return trace;
}

void SV_ClipToLinks(areanode_t *node, moveclip_t *clip)
{
	link_t *l, *next;
	edict_t *touch;
	trace_t trace;

	for (l = node->solid_edicts.next; l != &node->solid_edicts; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);

		if (touch->v.solid == SOLID_NOT)
			continue;

		if (touch == clip->passedict)
			continue;

		if (touch->v.solid == SOLID_TRIGGER)
			Sys_Error("Trigger in clipping list");

		if (clip->type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP)
			continue;

		if (clip->monsterclip && touch->v.rendermode != 0.0f)
			continue;

		if (clip->boxmins[0] > touch->v.absmax[0]
			|| clip->boxmins[1] > touch->v.absmax[1]
			|| clip->boxmins[2] > touch->v.absmax[2]
			|| clip->boxmaxs[0] < touch->v.absmin[0]
			|| clip->boxmaxs[1] < touch->v.absmin[1]
			|| clip->boxmaxs[2] < touch->v.absmin[2])
			continue;

		if (clip->passedict && clip->passedict->v.size[0] && !touch->v.size[0])
			continue;

		if (clip->trace.allsolid)
			return;

		if (clip->passedict)
		{
			if (PROG_TO_EDICT(touch->v.owner) == clip->passedict)
				continue;
			if (PROG_TO_EDICT(clip->passedict->v.owner) == touch)
				continue;
		}

		if ((int)touch->v.flags & FL_MONSTER)
			trace = SV_ClipMoveToEntity(touch, clip->start, clip->mins2, clip->maxs2, clip->end);
		else
			trace = SV_ClipMoveToEntity(touch, clip->start, clip->mins, clip->maxs, clip->end);

		if (trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
			if (clip->trace.startsolid)
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
				clip->trace = trace;
		}
		else if (trace.startsolid)
			clip->trace.startsolid = true;
	}

	if (node->axis == -1)
		return;

	if (clip->boxmaxs[node->axis] > node->dist)
		SV_ClipToLinks(node->children[0], clip);

	if (clip->boxmins[node->axis] < node->dist)
		SV_ClipToLinks(node->children[1], clip);
}

void SV_MoveBounds(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1.0;
			boxmaxs[i] = end[i] + maxs[i] + 1.0;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1.0;
			boxmaxs[i] = start[i] + maxs[i] + 1.0;
		}
	}
}

trace_t SV_Move(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict)
{
	moveclip_t clip;
	int i;

	memset(&clip, 0, sizeof(moveclip_t));

	clip.trace = SV_ClipMoveToEntity(sv_edicts, start, mins, maxs, end);

	if (clip.trace.fraction == 0.0)
		return clip.trace;

	VectorCopy(start, clip.start);
	VectorCopy(end, clip.end);
	VectorCopy(mins, clip.mins);
	VectorCopy(maxs, clip.maxs);
	clip.type = type;
	clip.passedict = passedict;
	clip.monsterclip = passedict && (((int)passedict->v.flags & FL_MONSTERCLIP) != 0);

	if (type == MOVE_MISSILE)
	{
		for (i = 0; i < 3; i++)
		{
			clip.mins2[i] = -15.0;
			clip.maxs2[i] = 15.0;
		}
	}
	else
	{
		VectorCopy(mins, clip.mins2);
		VectorCopy(maxs, clip.maxs2);
	}

	SV_MoveBounds(start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs);

	SV_ClipToLinks(sv_areanodes, &clip);

	return clip.trace;
}

areanode_t *SV_CreateAreaNode(int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t *anode;
	vec3_t size;
	vec3_t mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink(&anode->trigger_edicts);
	ClearLink(&anode->solid_edicts);

	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract(maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy(mins, mins1);
	VectorCopy(mins, mins2);
	VectorCopy(maxs, maxs1);
	VectorCopy(maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode(depth + 1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode(depth + 1, mins1, maxs1);

	return anode;
}

void SV_ClearWorld(void)
{
	SV_InitBoxHull();

	memset(sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode(0, sv.worldmodel->mins, sv.worldmodel->maxs);
}

void SV_UnlinkEdict(edict_t *ent)
{
	if (!ent->area.prev)
		return;

	RemoveLink(&ent->area);
	ent->area.prev = ent->area.next = NULL;
}

void SV_TouchLinks(edict_t *ent, areanode_t *node)
{
	link_t *l, *next;
	edict_t *touch;
	int old_self;
	int old_other;

	for (l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);

		if (touch == ent)
			continue;

		if (touch->v.solid != SOLID_TRIGGER)
			continue;

		if (ent->v.absmin[0] > touch->v.absmax[0]
			|| ent->v.absmin[1] > touch->v.absmax[1]
			|| ent->v.absmin[2] > touch->v.absmax[2]
			|| ent->v.absmax[0] < touch->v.absmin[0]
			|| ent->v.absmax[1] < touch->v.absmin[1]
			|| ent->v.absmax[2] < touch->v.absmin[2])
			continue;

		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;
		pr_global_struct->self = EDICT_TO_PROG(touch);
		pr_global_struct->other = EDICT_TO_PROG(ent);
		pr_global_struct->time = (float)sv.time;

		ED_CallDispatch(touch, 2, 0);

		pr_global_struct->self = old_self;
		pr_global_struct->other = old_other;
	}

	if (node->axis == -1)
		return;

	if (ent->v.absmax[node->axis] > node->dist)
		SV_TouchLinks(ent, node->children[0]);

	if (ent->v.absmin[node->axis] < node->dist)
		SV_TouchLinks(ent, node->children[1]);
}

void SV_FindTouchedLeafs(edict_t *ent, mnode_t *node)
{
	mplane_t *splitplane;
	mleaf_t *leaf;
	int sides;
	int leafnum;

	if (node->contents == CONTENTS_SOLID)
		return;

	if (node->contents < 0)
	{
		if (ent->num_leafs == MAX_ENT_LEAFS)
			return;

		leaf = (mleaf_t *)node;
		leafnum = leaf - sv.worldmodel->leafs - 1;

		ent->leafnums[ent->num_leafs] = leafnum;
		ent->num_leafs++;
		return;
	}

	splitplane = node->plane;
	sides = BoxOnPlaneSide(ent->v.absmin, ent->v.absmax, splitplane);

	if (sides & 1)
		SV_FindTouchedLeafs(ent, node->children[0]);

	if (sides & 2)
		SV_FindTouchedLeafs(ent, node->children[1]);
}

void SV_LinkEdict(edict_t *ent, qboolean touch_triggers)
{
	areanode_t *node;

	if (ent->area.prev)
		SV_UnlinkEdict(ent);

	if (ent == sv_edicts)
		return;

	if (ent->free)
		return;

	if (ent->v.solid == SOLID_BSP &&
		(ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2]))
	{

		float	max, v;
		int		i;

		max = 0;
		for (i = 0; i < 3; i++)
		{
			v = fabs(ent->v.mins[i]);
			if (v > max)
				max = v;
			v = fabs(ent->v.maxs[i]);
			if (v > max)
				max = v;
		}

		for (i = 0; i < 3; i++)
		{
			ent->v.absmin[i] = ent->v.origin[i] - max;
			ent->v.absmax[i] = ent->v.origin[i] + max;
		}
	}
	else
	{
		VectorAdd(ent->v.origin, ent->v.mins, ent->v.absmin);
		VectorAdd(ent->v.origin, ent->v.maxs, ent->v.absmax);
	}

	if ((int)ent->v.flags & FL_ITEM)
	{
		ent->v.absmin[0] -= 15.0;
		ent->v.absmin[1] -= 15.0;
		ent->v.absmax[0] += 15.0;
		ent->v.absmax[1] += 15.0;
	}
	else
	{

		ent->v.absmin[0] -= 1.0;
		ent->v.absmin[1] -= 1.0;
		ent->v.absmin[2] -= 1.0;
		ent->v.absmax[0] += 1.0;
		ent->v.absmax[1] += 1.0;
		ent->v.absmax[2] += 1.0;
	}

	ent->num_leafs = 0;
	if (ent->v.modelindex)
		SV_FindTouchedLeafs(ent, sv.worldmodel->nodes);

	if (ent->v.solid == SOLID_NOT && (int)ent->v.skin == -1)
		return;

	if (ent->v.solid == SOLID_BSP
		&& !sv.models[(int)ent->v.modelindex]
		&& !strlen(pr_strings + (int)ent->v.model))
	{
		return;
	}

	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;

		if (ent->v.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->v.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;
	}

	if (ent->v.solid == SOLID_TRIGGER)
		InsertLinkBefore(&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore(&ent->area, &node->solid_edicts);

	if (touch_triggers)
		SV_TouchLinks(ent, sv_areanodes);
}
