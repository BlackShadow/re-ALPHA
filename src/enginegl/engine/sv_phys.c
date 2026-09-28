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
// sv_phys.c

#include "quakedef.h"
#include "eiface.h"

/*

pushmove objects do not obey gravity, and do not interact with each other or
trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set
for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

#define STOP_EPSILON	0.1
#define MAX_CLIP_PLANES	5
#define STEPSIZE		18
#define FLOOR_NORMAL_Z	0.7f	// steeper planes are walls
#define TOSS_FRAMETIME	0.05	// SV_Trace_Toss simulation step
#define CURRENT_SPEED	150		// water current push with the head under water

// SV_FlyMove blocked flags
#define BLOCKED_FLOOR	1
#define BLOCKED_STEP	2
#define BLOCKED_SOLID	4		// started in a solid
#define BLOCKED_ANY		(BLOCKED_FLOOR | BLOCKED_STEP | BLOCKED_SOLID)

static void SV_Physics_Pusher(edict_t *ent);
static void SV_Physics_Client(edict_t *ent, int num);
static void SV_Physics_None(edict_t *ent);
static void SV_Physics_Follow(edict_t *ent);
static void SV_Physics_Noclip(edict_t *ent);
static void SV_Physics_Toss(edict_t *ent);
static void SV_Physics_Step(edict_t *ent);

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
static void SV_Impact(edict_t *e1, edict_t *e2)
{
	int		old_self, old_other;

	pr_global_struct->time = sv.time;

	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;

	if (e1->v.solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e1);
		pr_global_struct->other = EDICT_TO_PROG(e2);
		ED_CallDispatch(e1, DISPATCH_TOUCH, NULL);
	}

	if (e2->v.solid != SOLID_NOT)
	{
		pr_global_struct->self = EDICT_TO_PROG(e2);
		pr_global_struct->other = EDICT_TO_PROG(e1);
		ED_CallDispatch(e2, DISPATCH_TOUCH, NULL);
	}

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
}

/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
static int ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float	backoff;
	float	change;
	int		i, blocked;

	blocked = 0;
	if (normal[2] > 0)
		blocked |= BLOCKED_FLOOR;
	if (normal[2] == 0)
		blocked |= BLOCKED_STEP;

	backoff = DotProduct(in, normal) * overbounce;

	for (i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}

/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
============
*/
static int SV_FlyMove(edict_t *ent, float time, trace_t *steptrace)
{
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	int			i, j;
	trace_t		trace;
	vec3_t		end;
	float		time_left;
	int			blocked;

	numbumps = 4;

	blocked = 0;
	VectorCopy(ent->v.velocity, original_velocity);
	VectorCopy(ent->v.velocity, primal_velocity);
	numplanes = 0;

	time_left = time;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (ent->v.velocity[0] == 0 && ent->v.velocity[1] == 0 && ent->v.velocity[2] == 0)
			break;

		VectorMA(ent->v.origin, time_left, ent->v.velocity, end);

		trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

		if (trace.allsolid)
		{
			// entity is trapped in another solid
			VectorCopy(vec3_origin, ent->v.velocity);
			return BLOCKED_SOLID;
		}

		if (trace.fraction > 0)
		{
			// actually covered some distance
			VectorCopy(trace.endpos, ent->v.origin);
			VectorCopy(ent->v.velocity, primal_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break;		// moved the entire distance

		if (!trace.ent)
			Sys_Error("SV_FlyMove: trace.ent == NULL");

		if (trace.plane.normal[2] > FLOOR_NORMAL_Z)
		{
			blocked |= BLOCKED_FLOOR;		// floor
			if (trace.ent->v.solid == SOLID_BSP || trace.ent->v.movetype == MOVETYPE_PUSHSTEP)
			{
				ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
				ent->v.groundentity = EDICT_TO_PROG(trace.ent);
			}
		}

		if (trace.plane.normal[2] == 0)
		{
			blocked |= BLOCKED_STEP;		// step
			if (steptrace)
				*steptrace = trace;	// save for player extrafriction
		}

		// run the impact function
		SV_Impact(ent, trace.ent);
		if (ent->free)
			break;		// removed by the impact function

		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			VectorCopy(vec3_origin, ent->v.velocity);
			return blocked;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < numplanes; i++)
		{
			ClipVelocity(primal_velocity, planes[i], new_velocity, 1);
			for (j = 0; j < numplanes; j++)
			{
				if (j != i)
				{
					if (DotProduct(planes[j], new_velocity) < 0)
						break;	// not ok
				}
			}
			if (j == numplanes)
				break;
		}

		if (i != numplanes)
		{
			// go along this plane
			VectorCopy(new_velocity, ent->v.velocity);
		}
		else
		{
			// go along the crease
			if (numplanes != 2)
				return blocked;

			CrossProduct(planes[0], planes[1], dir);
			d = DotProduct(ent->v.velocity, dir);
			VectorScale(dir, d, ent->v.velocity);
		}

		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if (DotProduct(ent->v.velocity, original_velocity) <= 0)
		{
			VectorCopy(vec3_origin, ent->v.velocity);
			return blocked;
		}
	}

	return blocked;
}

/*
============
SV_AddGravity

v.armortype is the gravity scale, 0 means normal gravity
============
*/
static void SV_AddGravity(edict_t *ent)
{
	float	ent_gravity;
	float	gravity;

	if (ent->v.armortype == 0)
		ent_gravity = 1;
	else
		ent_gravity = ent->v.armortype;

	gravity = ent_gravity * sv_gravity.value * (float)host_frametime;
	ent->v.velocity[2] -= gravity;
}

/*
================
SV_CheckVelocity
================
*/
static void SV_CheckVelocity(edict_t *ent)
{
	int		i;

	// bound velocity
	for (i = 0; i < 3; i++)
	{
		if (IS_NAN(ent->v.velocity[i]))
		{
			Con_Printf("Got a NaN velocity\n");
			ent->v.velocity[i] = 0;
		}

		if (IS_NAN(ent->v.origin[i]))
		{
			Con_Printf("Got a NaN origin\n");
			ent->v.origin[i] = 0;
		}

		if (ent->v.velocity[i] > sv_maxvelocity.value)
			ent->v.velocity[i] = sv_maxvelocity.value;
		else if (ent->v.velocity[i] < -sv_maxvelocity.value)
			ent->v.velocity[i] = -sv_maxvelocity.value;
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
static qboolean SV_RunThink(edict_t *ent)
{
	float	thinktime;

	thinktime = ent->v.nextthink;
	if (thinktime <= 0)
		return true;
	if (thinktime > sv.time + host_frametime)
		return true;

	if (thinktime < sv.time)
		thinktime = sv.time;	// don't let things stay in the past.
								// it is possible to start that way
								// by a trigger with a local time.
	ent->v.nextthink = 0;
	pr_global_struct->time = thinktime;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	pr_global_struct->other = 0;
	ED_CallDispatch(ent, DISPATCH_THINK, NULL);

	return !ent->free;
}

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
static trace_t SV_PushEntity(edict_t *ent, vec3_t push)
{
	trace_t	trace;
	vec3_t	end;
	int		type;

	VectorAdd(ent->v.origin, push, end);

	if (ent->v.movetype == MOVETYPE_FLYMISSILE)
		type = MOVE_MISSILE;
	else if (ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT)
		type = MOVE_NOMONSTERS;	// only clip against bmodels
	else
		type = MOVE_NORMAL;

	trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, type, ent);

	VectorCopy(trace.endpos, ent->v.origin);
	SV_LinkEdict(ent, true);

	if (trace.ent)
		SV_Impact(ent, trace.ent);

	return trace;
}

/*
============
SV_PushMove
============
*/
static void SV_PushMove(edict_t *pusher, float movetime)
{
	int		i, e;
	edict_t	*check;
	vec3_t	mins, maxs, move;
	vec3_t	entorig, pushorig;
	int		num_moved;
	edict_t	*moved_edict[MAX_EDICTS];
	vec3_t	moved_from[MAX_EDICTS];

	if (!pusher->v.velocity[0] && !pusher->v.velocity[1] && !pusher->v.velocity[2])
	{
		pusher->v.ltime += movetime;
		return;
	}

	for (i = 0; i < 3; i++)
		move[i] = pusher->v.velocity[i] * movetime;

	for (i = 0; i < 3; i++)
	{
		mins[i] = pusher->v.absmin[i] + move[i];
		maxs[i] = pusher->v.absmax[i] + move[i];
	}

	VectorCopy(pusher->v.origin, pushorig);

	// move the pusher to it's final position
	VectorAdd(pusher->v.origin, move, pusher->v.origin);
	pusher->v.ltime += movetime;
	SV_LinkEdict(pusher, false);

	// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT(sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT(check))
	{
		if (check->free)
			continue;

		if (check->v.movetype == MOVETYPE_PUSH
			|| check->v.movetype == MOVETYPE_NONE
			|| check->v.movetype == MOVETYPE_FOLLOW
			|| check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		// if the entity is standing on the pusher, it will definately be moved
		if (!((int)check->v.flags & FL_ONGROUND) || PROG_TO_EDICT(check->v.groundentity) != pusher)
		{
			if (check->v.absmin[0] >= maxs[0]
				|| check->v.absmin[1] >= maxs[1]
				|| check->v.absmin[2] >= maxs[2]
				|| check->v.absmax[0] <= mins[0]
				|| check->v.absmax[1] <= mins[1]
				|| check->v.absmax[2] <= mins[2])
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition(check))
				continue;
		}

		// remove the onground flag for non-players
		if (check->v.movetype != MOVETYPE_WALK)
			check->v.flags = (int)check->v.flags & ~FL_ONGROUND;

		VectorCopy(check->v.origin, entorig);
		VectorCopy(check->v.origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity
		pusher->v.solid = SOLID_NOT;
		SV_PushEntity(check, move);
		pusher->v.solid = SOLID_BSP;

		// if it is still inside the pusher, block
		if (SV_TestEntityPosition(check))
		{
			if (check->v.mins[0] != check->v.maxs[0])
			{
				if (check->v.solid != SOLID_NOT && check->v.solid != SOLID_TRIGGER)
				{
					// fail the move
					VectorCopy(entorig, check->v.origin);
					SV_LinkEdict(check, true);

					VectorCopy(pushorig, pusher->v.origin);
					pusher->v.ltime -= movetime;
					SV_LinkEdict(pusher, false);

					// if the pusher has a "blocked" function, call it
					// otherwise, just stay in place until the obstacle is gone
					pr_global_struct->self = EDICT_TO_PROG(pusher);
					pr_global_struct->other = EDICT_TO_PROG(check);
					ED_CallDispatch(pusher, DISPATCH_BLOCKED, NULL);

					// move back any entities we already moved
					for (i = 0; i < num_moved; i++)
					{
						VectorCopy(moved_from[i], moved_edict[i]->v.origin);
						SV_LinkEdict(moved_edict[i], false);
					}
					return;
				}

				// corpse, shrink it to a point at its bottom
				check->v.mins[0] = 0;
				check->v.maxs[0] = 0;
				check->v.maxs[2] = check->v.mins[2];
				check->v.mins[1] = 0;
				check->v.maxs[1] = 0;
			}
		}
	}
}

/*
============
SV_PushRotate
============
*/
static void SV_PushRotate(edict_t *pusher, float movetime)
{
	int		i, e;
	edict_t	*check;
	vec3_t	move, amove;
	vec3_t	org, org2;
	vec3_t	forward, right, up;
	vec3_t	entorig, pushorig, pushang;
	int		num_moved;
	edict_t	*moved_edict[MAX_EDICTS];
	vec3_t	moved_from[MAX_EDICTS];

	if (!pusher->v.avelocity[0] && !pusher->v.avelocity[1] && !pusher->v.avelocity[2])
	{
		pusher->v.ltime += movetime;
		return;
	}

	for (i = 0; i < 3; i++)
		amove[i] = pusher->v.avelocity[i] * movetime;

	VectorSubtract(vec3_origin, amove, move);
	AngleVectors(move, forward, right, up);

	VectorCopy(pusher->v.angles, pushang);
	VectorCopy(pusher->v.origin, pushorig);

	// move the pusher to it's final position
	VectorAdd(pusher->v.angles, amove, pusher->v.angles);
	pusher->v.ltime += movetime;
	SV_LinkEdict(pusher, false);

	// see if any solid entities are inside the final position
	num_moved = 0;
	check = NEXT_EDICT(sv.edicts);
	for (e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT(check))
	{
		if (check->free)
			continue;

		if (check->v.movetype == MOVETYPE_PUSH
			|| check->v.movetype == MOVETYPE_NONE
			|| check->v.movetype == MOVETYPE_FOLLOW
			|| check->v.movetype == MOVETYPE_NOCLIP)
			continue;

		// if the entity is standing on the pusher, it will definately be moved
		if (!((int)check->v.flags & FL_ONGROUND) || PROG_TO_EDICT(check->v.groundentity) != pusher)
		{
			if (check->v.absmin[0] >= pusher->v.absmax[0]
				|| check->v.absmin[1] >= pusher->v.absmax[1]
				|| check->v.absmin[2] >= pusher->v.absmax[2]
				|| check->v.absmax[0] <= pusher->v.absmin[0]
				|| check->v.absmax[1] <= pusher->v.absmin[1]
				|| check->v.absmax[2] <= pusher->v.absmin[2])
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition(check))
				continue;
		}

		// remove the onground flag for non-players
		if (check->v.movetype != MOVETYPE_WALK)
			check->v.flags = (int)check->v.flags & ~FL_ONGROUND;

		VectorCopy(check->v.origin, entorig);
		VectorCopy(check->v.origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		// calculate destination position
		VectorSubtract(check->v.origin, pusher->v.origin, org);
		org2[0] = DotProduct(org, forward);
		org2[1] = -DotProduct(org, right);
		org2[2] = DotProduct(org, up);
		VectorSubtract(org2, org, move);

		// try moving the contacted entity
		pusher->v.solid = SOLID_NOT;
		SV_PushEntity(check, move);
		pusher->v.solid = SOLID_BSP;

		// if it is still inside the pusher, block
		if (SV_TestEntityPosition(check))
		{
			if (check->v.mins[0] != check->v.maxs[0])
			{
				if (check->v.solid != SOLID_NOT && check->v.solid != SOLID_TRIGGER)
				{
					// fail the move
					VectorCopy(entorig, check->v.origin);
					SV_LinkEdict(check, true);

					VectorCopy(pushang, pusher->v.angles);
					VectorCopy(pushorig, pusher->v.origin);
					pusher->v.ltime -= movetime;
					SV_LinkEdict(pusher, false);

					// if the pusher has a "blocked" function, call it
					// otherwise, just stay in place until the obstacle is gone
					pr_global_struct->self = EDICT_TO_PROG(pusher);
					pr_global_struct->other = EDICT_TO_PROG(check);
					ED_CallDispatch(pusher, DISPATCH_BLOCKED, NULL);

					// move back any entities we already moved
					for (i = 0; i < num_moved; i++)
					{
						VectorCopy(moved_from[i], moved_edict[i]->v.origin);
						VectorSubtract(moved_edict[i]->v.angles, amove, moved_edict[i]->v.angles);
						SV_LinkEdict(moved_edict[i], false);
					}
					return;
				}

				// corpse, shrink it to a point at its bottom
				check->v.mins[0] = 0;
				check->v.maxs[0] = 0;
				check->v.maxs[2] = check->v.mins[2];
				check->v.mins[1] = 0;
				check->v.maxs[1] = 0;
			}
		}
		else
		{
			// rotate the entity with the pusher
			VectorAdd(check->v.angles, amove, check->v.angles);
		}
	}
}

/*
================
SV_Physics
================
*/
void SV_Physics(void)
{
	int		i;
	edict_t	*ent;

	// let the progs know that a new frame has started
	pr_global_struct->self = 0;
	pr_global_struct->other = 0;
	pr_global_struct->time = sv.time;
	DispatchEntityCallback(ENTITYFUNC_STARTFRAME);

	// treat each object in turn
	ent = sv_edicts;
	for (i = 0; i < *sv_num_edicts; i++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;

		if (pr_global_struct->force_retouch)
			SV_LinkEdict(ent, true);	// force retouch even for stationary

		if (i > 0 && i <= svs.maxclients)
		{
			SV_Physics_Client(ent, i);
			continue;
		}

		switch ((int)ent->v.movetype)
		{
		case MOVETYPE_PUSH:
			SV_Physics_Pusher(ent);
			break;
		case MOVETYPE_NONE:
			SV_Physics_None(ent);
			break;
		case MOVETYPE_FOLLOW:
			SV_Physics_Follow(ent);
			break;
		case MOVETYPE_NOCLIP:
			SV_Physics_Noclip(ent);
			break;
		case MOVETYPE_STEP:
			SV_Physics_Step(ent);
			break;
		case MOVETYPE_PUSHSTEP:
			SV_Physics_Step(ent);
			SV_Physics_Pusher(ent);
			break;
		case MOVETYPE_TOSS:
		case MOVETYPE_BOUNCE:
		case MOVETYPE_BOUNCEMISSILE:
		case MOVETYPE_FLY:
		case MOVETYPE_FLYMISSILE:
			SV_Physics_Toss(ent);
			break;
		default:
			Sys_Error("SV_Physics: bad movetype %i", (int)ent->v.movetype);
		}
	}

	if (pr_global_struct->force_retouch)
		pr_global_struct->force_retouch--;

	sv.time += host_frametime;
}

/*
================
SV_Trace_Toss

Runs a copy of ent as a toss object until it hits something other than passent
================
*/
trace_t *SV_Trace_Toss(trace_t *result, edict_t *ent, edict_t *passent)
{
	double	save_frametime;
	edict_t	tempent;
	trace_t	trace;
	vec3_t	move;
	vec3_t	end;
	float	frametime;

	save_frametime = host_frametime;
	host_frametime = TOSS_FRAMETIME;

	memcpy(&tempent, ent, sizeof(edict_t));

	// run until it hits something other than passent
	do
	{
		do
		{
			SV_CheckVelocity(&tempent);
			SV_AddGravity(&tempent);

			frametime = host_frametime;
			VectorMA(tempent.v.angles, frametime, tempent.v.avelocity, tempent.v.angles);
			VectorScale(tempent.v.velocity, frametime, move);
			VectorAdd(move, tempent.v.origin, end);

			trace = SV_Move(tempent.v.origin, tempent.v.mins, tempent.v.maxs, end, MOVE_NORMAL, &tempent);
			VectorCopy(trace.endpos, tempent.v.origin);
		} while (!trace.ent);
	} while (trace.ent == passent);

	host_frametime = save_frametime;

	if (result)
		*result = trace;

	return result;
}

/*
=============
SV_Physics_Pusher
=============
*/
static void SV_Physics_Pusher(edict_t *ent)
{
	float	oldltime;
	float	movetime;

	oldltime = ent->v.ltime;

	if (ent->v.nextthink < ent->v.ltime + host_frametime)
	{
		movetime = ent->v.nextthink - ent->v.ltime;
		if (movetime < 0)
			movetime = 0;
	}
	else
		movetime = host_frametime;

	if (movetime)
	{
		if (ent->v.avelocity[0] || ent->v.avelocity[1] || ent->v.avelocity[2])
			SV_PushRotate(ent, movetime);
		else
			SV_PushMove(ent, movetime);	// advances ent->v.ltime if not blocked
	}

	if (ent->v.nextthink > oldltime && ent->v.nextthink <= ent->v.ltime)
	{
		ent->v.nextthink = 0;
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(ent);
		pr_global_struct->other = 0;
		ED_CallDispatch(ent, DISPATCH_THINK, NULL);
	}
}

/*
===============================================================================

CLIENT MOVEMENT

===============================================================================
*/

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
static void SV_CheckStuck(edict_t *ent)
{
	int		i, j;
	int		z;
	vec3_t	org;

	if (!SV_TestEntityPosition(ent))
	{
		VectorCopy(ent->v.origin, ent->v.oldorigin);
		return;
	}

	VectorCopy(ent->v.origin, org);
	VectorCopy(ent->v.oldorigin, ent->v.origin);
	if (!SV_TestEntityPosition(ent))
	{
		Con_Printf("Unstuck.\n");
		SV_LinkEdict(ent, true);
		return;
	}

	for (z = 0; z < STEPSIZE; z++)
	{
		for (i = -1; i <= 1; i++)
		{
			for (j = -1; j <= 1; j++)
			{
				ent->v.origin[0] = org[0] + i;
				ent->v.origin[1] = org[1] + j;
				ent->v.origin[2] = org[2] + z;
				if (!SV_TestEntityPosition(ent))
				{
					Con_Printf("Unstuck.\n");
					SV_LinkEdict(ent, true);
					return;
				}
			}
		}
	}

	VectorCopy(org, ent->v.origin);
	Con_Printf("player is stuck.\n");
}

/*
=============
SV_CheckWater

Sets waterlevel and watertype, and adds the push of water currents.
Returns true if the waist is under water.
=============
*/
static qboolean SV_CheckWater(edict_t *ent)
{
	static const vec3_t current_table[] =
	{
		{ 1, 0, 0 },	// CONTENTS_CURRENT_0
		{ 0, 1, 0 },	// CONTENTS_CURRENT_90
		{ -1, 0, 0 },	// CONTENTS_CURRENT_180
		{ 0, -1, 0 },	// CONTENTS_CURRENT_270
		{ 0, 0, 1 },	// CONTENTS_CURRENT_UP
		{ 0, 0, -1 },	// CONTENTS_CURRENT_DOWN
	};

	vec3_t	point;
	int		cont;
	int		truecont;

	ent->v.waterlevel = WATERLEVEL_DRY;
	ent->v.watertype = CONTENTS_EMPTY;

	point[0] = ent->v.origin[0];
	point[1] = ent->v.origin[1];
	point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;

	cont = SV_PointContents(point);
	if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
	{
		truecont = SV_TruePointContents(point);

		ent->v.waterlevel = WATERLEVEL_FEET;
		ent->v.watertype = cont;

		point[2] = ent->v.origin[2] + (ent->v.maxs[2] + ent->v.mins[2]) * 0.5f;
		cont = SV_PointContents(point);
		if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
		{
			ent->v.waterlevel = WATERLEVEL_WAIST;

			point[2] = ent->v.origin[2] + ent->v.view_ofs[2];
			cont = SV_PointContents(point);
			if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
				ent->v.waterlevel = WATERLEVEL_HEAD;
		}

		if (truecont <= CONTENTS_CURRENT_0 && truecont >= CONTENTS_CURRENT_DOWN)
		{
			VectorMA(ent->v.basevelocity, ent->v.waterlevel * CURRENT_SPEED / WATERLEVEL_HEAD,
				current_table[CONTENTS_CURRENT_0 - truecont], ent->v.basevelocity);
		}
	}

	return ent->v.waterlevel > WATERLEVEL_FEET;
}

/*
============
SV_WallFriction
============
*/
static void SV_WallFriction(edict_t *ent, trace_t *trace)
{
	vec3_t	forward, right, up;
	float	d, dot;
	vec3_t	into, side;

	AngleVectors(ent->v.v_angle, forward, right, up);
	d = DotProduct(trace->plane.normal, forward);

	d += 0.5f;
	if (d < 0)
	{
		// cut the tangential velocity
		dot = DotProduct(ent->v.velocity, trace->plane.normal);
		VectorScale(trace->plane.normal, dot, into);
		VectorSubtract(ent->v.velocity, into, side);

		d += 1;
		ent->v.velocity[0] = side[0] * d;
		ent->v.velocity[1] = side[1] * d;

		SV_CheckVelocity(ent);
	}
}

/*
=====================
SV_TryUnstick

Player has come to a dead stop, possibly due to the problem with limited
float precision at some angle joins in the BSP hull.

Try fixing by pushing one pixel in each direction.

This is a hack, but in the interest of good gameplay...
======================
*/
static int SV_TryUnstick(edict_t *ent, vec3_t oldvel)
{
	int		i;
	vec3_t	oldorg;
	vec3_t	dir;
	int		clip;
	trace_t	steptrace;

	VectorCopy(ent->v.origin, oldorg);
	VectorClear(dir);

	for (i = 0; i < 8; i++)
	{
		// try pushing a little in an axial direction
		switch (i)
		{
		case 0:	dir[0] = 2; dir[1] = 0; break;
		case 1:	dir[0] = 0; dir[1] = 2; break;
		case 2:	dir[0] = -2; dir[1] = 0; break;
		case 3:	dir[0] = 0; dir[1] = -2; break;
		case 4:	dir[0] = 2; dir[1] = 2; break;
		case 5:	dir[0] = -2; dir[1] = 2; break;
		case 6:	dir[0] = 2; dir[1] = -2; break;
		case 7:	dir[0] = -2; dir[1] = -2; break;
		}

		SV_PushEntity(ent, dir);

		// retry the original move
		ent->v.velocity[0] = oldvel[0];
		ent->v.velocity[1] = oldvel[1];
		ent->v.velocity[2] = 0;

		SV_CheckVelocity(ent);
		clip = SV_FlyMove(ent, 0.1f, &steptrace);
		SV_CheckVelocity(ent);

		if (fabs(oldorg[1] - ent->v.origin[1]) > 4 || fabs(oldorg[0] - ent->v.origin[0]) > 4)
			return clip;

		// go back to the original pos and try again
		VectorCopy(oldorg, ent->v.origin);
	}

	VectorClear(ent->v.velocity);
	return BLOCKED_ANY;		// still not moving
}

/*
=====================
SV_WalkMove

Only used by players
======================
*/
static void SV_WalkMove(edict_t *ent)
{
	vec3_t	upmove, downmove;
	vec3_t	oldorg, oldvel;
	vec3_t	nosteporg, nostepvel;
	int		clip;
	int		oldflags;
	trace_t	steptrace, downtrace;

	// do a regular slide move unless it looks like you ran into a step
	oldflags = (int)ent->v.flags;
	ent->v.flags = oldflags & ~FL_ONGROUND;

	VectorCopy(ent->v.origin, oldorg);
	VectorCopy(ent->v.velocity, oldvel);

	clip = SV_FlyMove(ent, host_frametime, &steptrace);

	if (!(clip & BLOCKED_STEP))
		return;		// move didn't block on a step

	SV_CheckVelocity(ent);

	// don't stair up while jumping, when gibbed by a trigger or while waterjumping
	if (!((oldflags & FL_ONGROUND) || ent->v.waterlevel != WATERLEVEL_DRY)
		|| ent->v.movetype != MOVETYPE_WALK
		|| sv_nostep.value
		|| ((int)sv_player->v.flags & FL_WATERJUMP))
		return;

	VectorCopy(ent->v.origin, nosteporg);
	VectorCopy(ent->v.velocity, nostepvel);

	// try moving up and forward to go up a step
	VectorCopy(oldorg, ent->v.origin);	// back to start pos

	upmove[0] = 0;
	upmove[1] = 0;
	upmove[2] = STEPSIZE;

	downmove[0] = 0;
	downmove[1] = 0;
	downmove[2] = oldvel[2] * host_frametime - STEPSIZE;

	// move up
	SV_PushEntity(ent, upmove);

	// move forward
	ent->v.velocity[0] = oldvel[0];
	ent->v.velocity[1] = oldvel[1];
	ent->v.velocity[2] = 0;

	SV_CheckVelocity(ent);
	clip = SV_FlyMove(ent, host_frametime, &steptrace);
	SV_CheckVelocity(ent);

	// check for stuckness, possibly due to the limited precision of floats
	// in the clipping hulls
	if (clip
		&& fabs(oldorg[1] - ent->v.origin[1]) < 0.03125f
		&& fabs(oldorg[0] - ent->v.origin[0]) < 0.03125f)
	{
		// stepping up didn't make any progress
		clip = SV_TryUnstick(ent, oldvel);
	}

	// extra friction based on view angle
	if (clip & BLOCKED_STEP)
		SV_WallFriction(ent, &steptrace);

	// move down
	downtrace = SV_PushEntity(ent, downmove);

	if (downtrace.plane.normal[2] <= FLOOR_NORMAL_Z)
	{
		// if the push down didn't end up on good ground, use the move without
		// the step up.  This happens near wall / slope combinations, and can
		// cause the player to hop up higher on a slope too steep to climb
		VectorCopy(nosteporg, ent->v.origin);
		VectorCopy(nostepvel, ent->v.velocity);
		SV_CheckVelocity(ent);
	}
	else if (ent->v.solid == SOLID_BSP || ent->v.movetype == MOVETYPE_PUSHSTEP)
	{
		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(downtrace.ent);
	}
}

/*
================
SV_Physics_Client

Player character actions
================
*/
static void SV_Physics_Client(edict_t *ent, int num)
{
	if (!svs.clients[num - 1].active)
		return;		// unconnected slot

	// call standard client pre-think
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	DispatchEntityCallback(ENTITYFUNC_PLAYERPRETHINK);

	// do a move
	SV_CheckVelocity(ent);

	// decide which move function to call
	switch ((int)ent->v.movetype)
	{
	case MOVETYPE_NONE:
		if (!SV_RunThink(ent))
			return;
		break;

	case MOVETYPE_WALK:
		if (!SV_RunThink(ent))
			return;
		if (!SV_CheckWater(ent) && !((int)ent->v.flags & FL_WATERJUMP))
			SV_AddGravity(ent);
		SV_CheckStuck(ent);

		VectorAdd(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);
		SV_CheckVelocity(ent);
		SV_WalkMove(ent);
		SV_CheckVelocity(ent);
		VectorSubtract(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);
		SV_CheckVelocity(ent);
		break;

	case MOVETYPE_FLY:
		if (!SV_RunThink(ent))
			return;
		SV_FlyMove(ent, host_frametime, NULL);
		break;

	case MOVETYPE_NOCLIP:
		if (!SV_RunThink(ent))
			return;
		VectorMA(ent->v.origin, (float)host_frametime, ent->v.velocity, ent->v.origin);
		break;

	default:
		if ((int)ent->v.movetype != MOVETYPE_TOSS && (int)ent->v.movetype != MOVETYPE_BOUNCE)
			Sys_Error("SV_Physics_client: bad movetype %i", (int)ent->v.movetype);
		SV_Physics_Toss(ent);
		break;
	}

	// call standard player post-think
	SV_LinkEdict(ent, true);

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	DispatchEntityCallback(ENTITYFUNC_PLAYERPOSTTHINK);
}

//============================================================================

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
static void SV_Physics_None(edict_t *ent)
{
	// regular thinking
	SV_RunThink(ent);
}

/*
=============
SV_Physics_Follow

Entities that are "stuck" to another entity
=============
*/
static void SV_Physics_Follow(edict_t *ent)
{
	edict_t	*aiment;

	// regular thinking
	SV_RunThink(ent);

	aiment = EDICT_NUM(ent->v.aiment);
	if (aiment == sv_edicts)
	{
		ent->v.movetype = MOVETYPE_NONE;
		return;
	}

	VectorAdd(aiment->v.origin, ent->v.view_ofs, ent->v.origin);
	VectorCopy(aiment->v.angles, ent->v.angles);

	SV_LinkEdict(ent, true);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
static void SV_Physics_Noclip(edict_t *ent)
{
	float	frametime;

	frametime = host_frametime;

	// regular thinking
	if (!SV_RunThink(ent))
		return;

	VectorMA(ent->v.angles, frametime, ent->v.avelocity, ent->v.angles);
	VectorMA(ent->v.origin, frametime, ent->v.velocity, ent->v.origin);

	SV_LinkEdict(ent, false);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition
=============
*/
static void SV_CheckWaterTransition(edict_t *ent)
{
	vec3_t	point;
	int		cont;

	point[0] = ent->v.origin[0];
	point[1] = ent->v.origin[1];
	point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;

	cont = SV_PointContents(point);

	if (ent->v.watertype == 0)
	{
		// just spawned here
		ent->v.waterlevel = WATERLEVEL_FEET;
		ent->v.watertype = cont;
		return;
	}

	if (cont > CONTENTS_WATER || cont <= CONTENTS_TRANSLUCENT)
	{
		if (ent->v.watertype != CONTENTS_EMPTY)
		{
			// just crossed into open
			SV_StartSound(ent, CHAN_AUTO, "common/h2ohit1.wav", 255, ATTN_NORM);
		}
		ent->v.watertype = CONTENTS_EMPTY;
		ent->v.waterlevel = cont;
	}
	else
	{
		if (ent->v.watertype == CONTENTS_EMPTY)
		{
			// just crossed into water
			SV_StartSound(ent, CHAN_AUTO, "common/h2ohit1.wav", 255, ATTN_NORM);
		}
		ent->v.waterlevel = WATERLEVEL_FEET;
		ent->v.watertype = cont;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
static void SV_Physics_Toss(edict_t *ent)
{
	trace_t	trace;
	vec3_t	move;
	float	backoff;
	edict_t	*groundentity;

	// ride a conveyor
	groundentity = PROG_TO_EDICT(ent->v.groundentity);
	if (groundentity && ((int)groundentity->v.flags & FL_CONVEYOR))
		VectorCopy(groundentity->v.velocity, ent->v.basevelocity);
	else
		VectorClear(ent->v.basevelocity);

	SV_CheckWater(ent);

	// regular thinking
	if (!SV_RunThink(ent))
		return;

	if (ent->v.velocity[2] > 0)
		ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;

	// if onground, return without moving
	if (((int)ent->v.flags & FL_ONGROUND) && VectorCompare(ent->v.basevelocity, vec3_origin))
		return;

	SV_LinkEdict(ent, false);

	// add gravity
	if (!((int)ent->v.flags & FL_ONGROUND)
		&& ent->v.movetype != MOVETYPE_FLY
		&& ent->v.movetype != MOVETYPE_FLYMISSILE
		&& ent->v.movetype != MOVETYPE_BOUNCEMISSILE)
		SV_AddGravity(ent);

	// move angles
	VectorMA(ent->v.angles, host_frametime, ent->v.avelocity, ent->v.angles);

	// move origin
	VectorAdd(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);
	VectorScale(ent->v.velocity, host_frametime, move);
	trace = SV_PushEntity(ent, move);
	VectorSubtract(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);

	if (trace.fraction == 1)
		return;
	if (ent->free)
		return;

	if (ent->v.movetype == MOVETYPE_BOUNCEMISSILE)
		backoff = 2;
	else if (ent->v.movetype == MOVETYPE_BOUNCE)
		backoff = 1.5f;
	else
		backoff = 1;

	ClipVelocity(ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff);

	// stop if on ground
	if (trace.plane.normal[2] > FLOOR_NORMAL_Z)
	{
		if (ent->v.velocity[2] < 60 || ent->v.movetype != MOVETYPE_BOUNCE)
		{
			ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(trace.ent);
			VectorClear(ent->v.velocity);
			VectorClear(ent->v.avelocity);
		}
	}

	// check for in water
	SV_CheckWaterTransition(ent);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
static void SV_Physics_Step(edict_t *ent)
{
	qboolean	hitsound = false;
	qboolean	inwater;
	qboolean	wasonground;
	float		speed, newspeed, control;
	float		friction;
	edict_t		*groundentity;

	// ride a conveyor
	groundentity = PROG_TO_EDICT(ent->v.groundentity);
	if (groundentity && ((int)groundentity->v.flags & FL_CONVEYOR))
		VectorScale(groundentity->v.movedir, groundentity->v.speed, ent->v.basevelocity);
	else
		VectorCopy(vec3_origin, ent->v.basevelocity);

	pr_global_struct->self = EDICT_TO_PROG(ent);
	pr_global_struct->time = sv.time;

	SV_LinkEdict(ent, false);

	// freefall if not onground
	wasonground = ((int)ent->v.flags & FL_ONGROUND) != 0;
	inwater = SV_CheckWater(ent);

	if (!wasonground)
	{
		if (!((int)ent->v.flags & (FL_FLY | FL_SWIM)))
		{
			if (ent->v.velocity[2] < sv_gravity.value * -0.1f)
				hitsound = true;
			if (!inwater)
				SV_AddGravity(ent);
		}
	}

	if (!VectorCompare(ent->v.velocity, vec3_origin) || !VectorCompare(ent->v.basevelocity, vec3_origin))
	{
		ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;

		// apply friction
		if (wasonground && (ent->v.waterlevel > WATERLEVEL_DRY || SV_CheckBottom(ent)))
		{
			speed = sqrt(ent->v.velocity[0] * ent->v.velocity[0] + ent->v.velocity[1] * ent->v.velocity[1]);
			if (speed > 0)
			{
				friction = sv_friction.value;

				control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
				newspeed = speed - host_frametime * control * friction;

				if (newspeed < 0)
					newspeed = 0;
				newspeed /= speed;

				ent->v.velocity[0] *= newspeed;
				ent->v.velocity[1] *= newspeed;
			}
		}

		VectorAdd(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);
		SV_FlyMove(ent, host_frametime, NULL);
		VectorSubtract(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);

		// stop on the ground, unless it is a conveyor
		if (((int)ent->v.flags & FL_ONGROUND) && groundentity && !((int)groundentity->v.flags & FL_CONVEYOR))
		{
			ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
			ent->v.groundentity = EDICT_TO_PROG(groundentity);
			VectorCopy(vec3_origin, ent->v.velocity);
			VectorCopy(vec3_origin, ent->v.avelocity);
		}
	}

	// determine if it's on solid ground at all
	SV_LinkEdict(ent, true);

	if (hitsound && ((int)ent->v.flags & FL_ONGROUND) && !wasonground)
		SV_StartSound(ent, CHAN_AUTO, "common/thump.wav", 255, ATTN_NORM);

	// regular thinking
	SV_RunThink(ent);
	SV_CheckWaterTransition(ent);
}

/*
=============
SV_CompareEntityState

Not called; compares against a NULL pointer.
=============
*/
int SV_CompareEntityState(int length)
{
	return memcmp(&sv.models[1], NULL, length);
}
