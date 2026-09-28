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
// gl_refrag.c

#include "quakedef.h"

/*
===============================================================================

					ENTITY FRAGMENT FUNCTIONS

===============================================================================
*/

/*
================
R_RemoveEfrags

Call when removing an object from the world or moving it to another position
================
*/
void R_RemoveEfrags(entity_t *ent)
{
	efrag_t		*ef, *old, **prev;

	ef = ent->efrag;
	while (ef)
	{
		prev = &ef->leaf->efrags;
		while (*prev && *prev != ef)
			prev = &(*prev)->leafnext;

		if (*prev == ef)
			*prev = ef->leafnext;

		old = ef;
		ef = ef->entnext;

		// put it on the free list
		old->entnext = cl_free_efrags;
		cl_free_efrags = old;
	}

	ent->efrag = NULL;
}

/*
===================
R_SplitEntityOnNode
===================
*/
void R_SplitEntityOnNode(mnode_t *node)
{
	efrag_t		*ef;
	int			sides;

	if (node->contents == CONTENTS_SOLID)
		return;

	while (node->contents >= 0)
	{
		// NODE_MIXED
		mplane_t	*splitplane = node->plane;

		if (splitplane->type < PLANE_ANYX)
		{
			// axial plane
			sides = 1;
			if (r_emins[splitplane->type] < splitplane->dist)
				sides = (r_emaxs[splitplane->type] > splitplane->dist) + 2;
		}
		else
		{
			sides = BoxOnPlaneSide(r_emins, r_emaxs, splitplane);
		}

		if (sides == 3 && !r_pefragtopnode)
			r_pefragtopnode = node;		// this is the first splitter of this bmodel

		// recurse down the contacted sides
		if (sides & 1)
			R_SplitEntityOnNode(node->children[0]);

		if (!(sides & 2))
			return;

		node = node->children[1];
		if (node->contents == CONTENTS_SOLID)
			return;
	}

	// add an efrag if the node is a leaf
	if (!r_pefragtopnode)
		r_pefragtopnode = node;

	// grab an efrag off the free list
	ef = cl_free_efrags;
	if (!ef)
	{
		Con_Printf("Too many efrags!\n");
		return;		// no free fragments...
	}

	cl_free_efrags = ef->entnext;

	ef->entity = r_addent;

	// add the entity link
	*r_lastlink = ef;
	r_lastlink = &ef->entnext;
	ef->entnext = NULL;

	// set the leaf links
	ef->leaf = (mleaf_t *)node;
	ef->leafnext = ef->leaf->efrags;
	ef->leaf->efrags = ef;
}

/*
===========
R_AddEfrags
===========
*/
void R_AddEfrags(entity_t *ent)
{
	model_t		*entmodel;

	if (!ent->model)
		return;

	r_addent = ent;
	r_lastlink = &ent->efrag;
	r_pefragtopnode = NULL;

	entmodel = ent->model;

	r_emins[0] = ent->origin[0] + entmodel->mins[0];
	r_emins[1] = ent->origin[1] + entmodel->mins[1];
	r_emins[2] = ent->origin[2] + entmodel->mins[2];

	r_emaxs[0] = ent->origin[0] + entmodel->maxs[0];
	r_emaxs[1] = ent->origin[1] + entmodel->maxs[1];
	r_emaxs[2] = ent->origin[2] + entmodel->maxs[2];

	R_SplitEntityOnNode(cl_worldmodel->nodes);

	ent->topnode = r_pefragtopnode;
}
