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

extern model_t		*cl_worldmodel;
extern efrag_t		*cl_free_efrags;

extern vec3_t		r_emins;
extern vec3_t		r_emaxs;
extern mnode_t		*r_pefragtopnode;
extern efrag_t		**r_lastlink;
extern entity_t		*r_addent;

void R_RemoveEfrags(entity_t *ent)
{
	efrag_t *efrag = ent->efrag;
	while (efrag)
	{
		efrag_t **leaf_link = &efrag->leaf->efrags;
		while (*leaf_link && *leaf_link != efrag)
		{
			leaf_link = &(*leaf_link)->leafnext;
		}

		if (*leaf_link == efrag)
			*leaf_link = efrag->leafnext;

		efrag_t *next = efrag->entnext;
		efrag->entnext = cl_free_efrags;
		cl_free_efrags = efrag;
		efrag = next;
	}

	ent->efrag = NULL;
}

void R_SplitEntityOnNode(mnode_t *node)
{
	mnode_t *current = node;
	if (current->contents == CONTENTS_SOLID)
		return;

	while (current->contents >= 0)
	{
		mplane_t *plane = current->plane;
		int sides;

		if (plane->type < 3)
		{
			sides = 1;
			if (r_emins[plane->type] < plane->dist)
				sides = (r_emaxs[plane->type] > plane->dist) + 2;
		}
		else
		{
			sides = BoxOnPlaneSide(r_emins, r_emaxs, plane);
		}

		if (sides == 3 && !r_pefragtopnode)
			r_pefragtopnode = current;

		if (sides & 1)
			R_SplitEntityOnNode(current->children[0]);

		if (!(sides & 2))
			return;

		current = current->children[1];
		if (current->contents == CONTENTS_SOLID)
			return;
	}

	if (!r_pefragtopnode)
		r_pefragtopnode = current;

	efrag_t *efrag = cl_free_efrags;
	if (!efrag)
	{
		Con_Printf("Too many efrags!\n");
		return;
	}

	cl_free_efrags = efrag->entnext;

	efrag->entity = r_addent;
	*r_lastlink = efrag;
	r_lastlink = &efrag->entnext;
	efrag->entnext = NULL;

	efrag->leaf = (mleaf_t *)current;
	efrag->leafnext = efrag->leaf->efrags;
	efrag->leaf->efrags = efrag;
}

void R_AddEfrags(entity_t *ent)
{
	if (!ent->model)
		return;

	r_addent = ent;
	r_lastlink = &ent->efrag;
	r_pefragtopnode = NULL;

	r_emins[0] = ent->origin[0] + ent->model->mins[0];
	r_emins[1] = ent->origin[1] + ent->model->mins[1];
	r_emins[2] = ent->origin[2] + ent->model->mins[2];

	r_emaxs[0] = ent->origin[0] + ent->model->maxs[0];
	r_emaxs[1] = ent->origin[1] + ent->model->maxs[1];
	r_emaxs[2] = ent->origin[2] + ent->model->maxs[2];

	R_SplitEntityOnNode(cl_worldmodel->nodes);
	ent->topnode = r_pefragtopnode;
}
