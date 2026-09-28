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
// sv_move.c -- monster movement

#include "quakedef.h"

#define STEPSIZE	18

#define DI_NODIR	-1

/*
==============
SV_ChangeYaw

Turns toward ideal_yaw at yaw_speed
==============
*/
void SV_ChangeYaw(edict_t *ent)
{
	float	ideal, current, move, speed;

	current = anglemod(ent->v.angles[YAW]);
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;

	if (current == ideal)
		return;

	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}

	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v.angles[YAW] = anglemod(current + move);
}

/*
==============
SV_ChangePitch

Turns toward idealpitch at pitch_speed
==============
*/
void SV_ChangePitch(edict_t *ent)
{
	float	ideal, current, move, speed;

	current = anglemod(ent->v.angles[PITCH]);
	ideal = ent->v.idealpitch;
	speed = ent->v.pitch_speed;

	if (current == ideal)
		return;

	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}

	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v.angles[PITCH] = anglemod(current + move);
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw(void)
{
	SV_ChangeYaw(PROG_TO_EDICT(pr_global_struct->self));
}

/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.
=============
*/
static int c_no;
static int c_yes;

qboolean SV_CheckBottom(edict_t *ent)
{
	vec3_t	mins, maxs, point, start, stop;
	vec3_t	nullmins, nullmaxs;
	trace_t	trace;
	int		x, y;
	float	mid, bottom;

	mins[0] = ent->v.mins[0] + ent->v.origin[0];
	mins[1] = ent->v.mins[1] + ent->v.origin[1];
	mins[2] = ent->v.origin[2] + ent->v.mins[2];
	maxs[0] = ent->v.maxs[0] + ent->v.origin[0];
	maxs[1] = ent->v.maxs[1] + ent->v.origin[1];

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	point[2] = mins[2] - 1;
	for (x = 0; x <= 1; x++)
	{
		point[0] = x ? maxs[0] : mins[0];
		for (y = 0; y <= 1; y++)
		{
			point[1] = y ? maxs[1] : mins[1];
			if (SV_PointContents(point) != CONTENTS_SOLID)
				goto realcheck;
		}
	}

	c_yes++;
	return true;		// we got out easy

realcheck:
	// check it for real...
	// the midpoint must be within a step of the bottom
	start[0] = (maxs[0] + mins[0]) * 0.5f;
	start[1] = (maxs[1] + mins[1]) * 0.5f;
	start[2] = mins[2] + STEPSIZE;

	stop[0] = start[0];
	stop[1] = start[1];
	stop[2] = start[2] - 2 * STEPSIZE;

	VectorClear(nullmins);
	VectorClear(nullmaxs);

	c_no++;
	trace = SV_Move(start, nullmins, nullmaxs, stop, MOVE_NOMONSTERS, ent);
	if (trace.fraction == 1)
		return false;

	mid = bottom = trace.endpos[2];

	// the corners must be within a step of the midpoint
	for (x = 0; x <= 1; x++)
	{
		for (y = 0; y <= 1; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			stop[0] = start[0];
			start[1] = y ? maxs[1] : mins[1];
			stop[1] = start[1];

			trace = SV_Move(start, nullmins, nullmaxs, stop, MOVE_NOMONSTERS, ent);
			if (trace.fraction != 1 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1 || mid - trace.endpos[2] > STEPSIZE)
				return false;
		}
	}

	c_yes++;
	return true;
}

/*
=============
SV_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done and false is returned
=============
*/
qboolean SV_movestep(edict_t *ent, vec3_t move, qboolean relink)
{
	float		dz;
	vec3_t		oldorg, neworg, start, end;
	trace_t		trace;
	int			i;
	int			flags;
	edict_t		*enemy;

	// try the move
	VectorCopy(ent->v.origin, oldorg);

	neworg[0] = move[0] + ent->v.origin[0];
	neworg[1] = move[1] + ent->v.origin[1];
	neworg[2] = move[2] + ent->v.origin[2];

	flags = (int)ent->v.flags;

	// flying monsters don't step up
	if (flags & (FL_SWIM | FL_FLY))
	{
		// try one move with vertical motion, then one without
		i = 0;
		while (1)
		{
			end[0] = move[0] + ent->v.origin[0];
			end[1] = move[1] + ent->v.origin[1];
			end[2] = move[2] + ent->v.origin[2];

			enemy = (edict_t *)((byte *)sv_edicts + ent->v.enemy);
			if (i == 0 && enemy != sv_edicts)
			{
				dz = ent->v.origin[2] - enemy->v.origin[2];
				if (dz > 40)
					end[2] -= 8;
				if (dz < 30)
					end[2] += 8;
			}

			trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);
			if (trace.fraction == 1)
				break;

			if (enemy != sv_edicts && ++i < 2)
				continue;

			return false;
		}

		if ((flags & FL_SWIM) && SV_PointContents(trace.endpos) == CONTENTS_EMPTY)
			return false;	// swim monster left water

		VectorCopy(trace.endpos, ent->v.origin);
		if (relink)
			SV_LinkEdict(ent, true);
		return true;
	}

	// push down from a step height above the wished position
	start[0] = neworg[0];
	start[1] = neworg[1];
	start[2] = neworg[2] + STEPSIZE;

	end[0] = start[0];
	end[1] = start[1];
	end[2] = start[2] - STEPSIZE * 2;

	trace = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);
	if (trace.allsolid)
		return false;

	if (trace.startsolid)
	{
		start[2] -= STEPSIZE;
		trace = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);
		if (trace.allsolid || trace.startsolid)
			return false;
	}

	if (trace.fraction == 1)
	{
		// if monster had the ground pulled out, go ahead and fall
		if (!(flags & FL_PARTIALGROUND))
			return false;	// walked off an edge

		VectorCopy(neworg, ent->v.origin);
		if (relink)
			SV_LinkEdict(ent, true);
		ent->v.flags = flags & ~FL_ONGROUND;
		return true;
	}

	// check point traces down for dangling corners
	VectorCopy(trace.endpos, ent->v.origin);

	if (SV_CheckBottom(ent))
	{
		// the move is ok
		if (flags & FL_PARTIALGROUND)
			ent->v.flags = flags & ~FL_PARTIALGROUND;

		ent->v.groundentity = (byte *)trace.ent - (byte *)sv_edicts;

		if (relink)
			SV_LinkEdict(ent, true);
		return true;
	}

	if (!(flags & FL_PARTIALGROUND))
	{
		VectorCopy(oldorg, ent->v.origin);
		return false;
	}

	// entity had floor mostly pulled out from underneath it
	// and is trying to correct
	if (relink)
		SV_LinkEdict(ent, true);
	return true;
}

/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.
======================
*/
static qboolean SV_StepDirection(edict_t *ent, float yaw, float dist)
{
	vec3_t	move, oldorigin;
	double	radians;
	float	delta;

	ent->v.ideal_yaw = yaw;
	SV_ChangeYaw(ent);

	radians = yaw * M_PI * 2 / 360;
	move[0] = cos(radians) * dist;
	move[1] = sin(radians) * dist;
	move[2] = 0;

	VectorCopy(ent->v.origin, oldorigin);
	if (SV_movestep(ent, move, false))
	{
		delta = ent->v.angles[YAW] - ent->v.ideal_yaw;
		if (delta > 30 && delta < 330)
		{
			// not turned far enough, so don't take the step
			VectorCopy(oldorigin, ent->v.origin);
		}
		SV_LinkEdict(ent, true);
		return true;
	}

	SV_LinkEdict(ent, true);
	return false;
}

/*
======================
SV_WalkDirection

Walks the current distance along yaw without turning
======================
*/
static qboolean SV_WalkDirection(edict_t *ent, float yaw, float dist)
{
	vec3_t	move;
	double	radians;

	radians = yaw * M_PI * 2 / 360;
	move[0] = cos(radians) * dist;
	move[1] = sin(radians) * dist;
	move[2] = 0;

	if (SV_movestep(ent, move, false))
	{
		SV_LinkEdict(ent, true);
		return true;
	}

	SV_LinkEdict(ent, true);
	return false;
}

/*
======================
SV_FixCheckBottom

Returns the new flags
======================
*/
static int SV_FixCheckBottom(edict_t *ent)
{
	ent->v.flags = (int)ent->v.flags | FL_PARTIALGROUND;
	return (int)ent->v.flags;
}

/*
================
SV_NewChaseDir

Picks a new direction toward enemy
================
*/
static int SV_NewChaseDir(edict_t *actor, edict_t *enemy, float dist)
{
	float	deltax, deltay;
	float	d[3];
	float	tdir, olddir, turnaround;

	olddir = (int)(actor->v.ideal_yaw / 45) * 45;
	anglemod(olddir);
	turnaround = olddir - 180;
	anglemod(turnaround);

	deltax = enemy->v.origin[0] - actor->v.origin[0];
	deltay = enemy->v.origin[1] - actor->v.origin[1];

	if (deltax <= 10)
	{
		d[1] = 180;
		if (deltax >= -10)
			d[1] = DI_NODIR;
	}
	else
		d[1] = 0;

	if (deltay >= -10)
	{
		d[2] = 90;
		if (deltay <= 10)
			d[2] = DI_NODIR;
	}
	else
		d[2] = 270;

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		if (d[1] == 0)
			tdir = d[2] == 90 ? 45 : 315;
		else
			tdir = d[2] == 90 ? 135 : 215;

		if (turnaround != tdir && SV_StepDirection(actor, tdir, dist))
			return true;
	}

	// try other directions
	if ((rand() & 1) || abs((int)deltay) > abs((int)deltax))
	{
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if (d[1] != DI_NODIR && turnaround != d[1] && SV_StepDirection(actor, d[1], dist))
		return true;

	if (d[2] != DI_NODIR && turnaround != d[2] && SV_StepDirection(actor, d[2], dist))
		return true;

	// there is no direct path to the player, so pick another direction
	if (olddir != DI_NODIR && SV_StepDirection(actor, olddir, dist))
		return true;

	// randomly determine direction of search
	if (rand() & 1)
	{
		for (tdir = 0; tdir <= 315; tdir += 45)
		{
			if (turnaround != tdir && SV_StepDirection(actor, tdir, dist))
				return true;
		}
	}
	else
	{
		for (tdir = 315; tdir >= 0; tdir -= 45)
		{
			if (turnaround != tdir && SV_StepDirection(actor, tdir, dist))
				return true;
		}
	}

	if (turnaround != DI_NODIR && SV_StepDirection(actor, turnaround, dist))
		return true;

	actor->v.ideal_yaw = olddir;		// can't move

	// if a bridge was pulled out from underneath a monster, it may not have
	// a valid standing position at all
	if (!SV_CheckBottom(actor))
		return SV_FixCheckBottom(actor);

	return false;
}

/*
======================
SV_CloseEnough
======================
*/
static qboolean SV_CloseEnough(edict_t *ent, edict_t *goal, float dist)
{
	int		i;

	for (i = 0; i < 3; i++)
	{
		if (ent->v.absmax[i] + dist < goal->v.absmin[i])
			return false;
		if (ent->v.absmin[i] - dist > goal->v.absmax[i])
			return false;
	}
	return true;
}

/*
======================
SV_CloseEnoughPoint
======================
*/
qboolean SV_CloseEnoughPoint(edict_t *ent, vec3_t point, float dist)
{
	int		i;

	for (i = 0; i < 3; i++)
	{
		if (ent->v.absmax[i] + dist < point[i])
			return false;
		if (ent->v.absmin[i] - dist > point[i])
			return false;
	}
	return true;
}

/*
======================
SV_MoveToGoal_step

Steps toward the enemy, or picks a new direction to the goal entity
======================
*/
int SV_MoveToGoal_step(edict_t *ent, float dist)
{
	int			enemynum;
	edict_t		*enemy, *goal;

	if (!((int)ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
		return false;

	// if the next step hits the enemy, return immediately
	enemynum = ent->v.enemy;
	enemy = EDICT_NUM(enemynum);
	if (enemynum && SV_CloseEnough(ent, enemy, dist))
		return true;

	// bump around...
	if (!SV_StepDirection(ent, ent->v.ideal_yaw, dist))
	{
		goal = EDICT_NUM(ent->v.goalentity);
		return SV_NewChaseDir(ent, goal, dist);
	}

	return true;
}

/*
======================
PF_MoveToGoal_Classic

float(float dist) movetogoal
======================
*/
int PF_MoveToGoal_Classic(void)
{
	edict_t		*ent;

	ent = EDICT_NUM(pr_global_struct->self);
	return SV_MoveToGoal_step(ent, G_FLOAT(OFS_PARM0));
}

/*
================
SV_NewChaseDirToPoint

SV_NewChaseDir toward a point instead of an entity
================
*/
static int SV_NewChaseDirToPoint(edict_t *actor, vec3_t goal, float dist)
{
	float	deltax, deltay;
	float	d[3];
	float	tdir, olddir, turnaround;

	olddir = (int)(actor->v.ideal_yaw / 45) * 45;
	anglemod(olddir);
	turnaround = olddir - 180;
	anglemod(turnaround);

	deltax = goal[0] - actor->v.origin[0];
	deltay = goal[1] - actor->v.origin[1];

	if (deltax <= 10)
	{
		d[1] = 180;
		if (deltax >= -10)
			d[1] = DI_NODIR;
	}
	else
		d[1] = 0;

	if (deltay >= -10)
	{
		d[2] = 90;
		if (deltay <= 10)
			d[2] = DI_NODIR;
	}
	else
		d[2] = 270;

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		if (d[1] == 0)
			tdir = d[2] == 90 ? 45 : 315;
		else
			tdir = d[2] == 90 ? 135 : 215;

		if (turnaround != tdir && SV_StepDirection(actor, tdir, dist))
			return true;
	}

	// try other directions
	if ((rand() & 1) || abs((int)deltay) > abs((int)deltax))
	{
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if (d[1] != DI_NODIR && turnaround != d[1] && SV_StepDirection(actor, d[1], dist))
		return true;

	if (d[2] != DI_NODIR && turnaround != d[2] && SV_StepDirection(actor, d[2], dist))
		return true;

	// there is no direct path to the goal, so pick another direction
	if (olddir != DI_NODIR && SV_StepDirection(actor, olddir, dist))
		return true;

	// randomly determine direction of search
	if (rand() & 1)
	{
		for (tdir = 0; tdir <= 315; tdir += 45)
		{
			if (turnaround != tdir && SV_StepDirection(actor, tdir, dist))
				return true;
		}
	}
	else
	{
		for (tdir = 315; tdir >= 0; tdir -= 45)
		{
			if (turnaround != tdir && SV_StepDirection(actor, tdir, dist))
				return true;
		}
	}

	if (turnaround != DI_NODIR && SV_StepDirection(actor, turnaround, dist))
		return true;

	actor->v.ideal_yaw = olddir;		// can't move

	// if a bridge was pulled out from underneath a monster, it may not have
	// a valid standing position at all
	if (!SV_CheckBottom(actor))
		return SV_FixCheckBottom(actor);

	return false;
}

/*
======================
SV_MoveToGoal_internal

Moves self toward goal, chasing (chase != 0) or walking straight at it.
Returns how far the yaw is from ideal_yaw when that is more than 30 degrees.
======================
*/
float SV_MoveToGoal_internal(edict_t *self, float *goal, float dist, int chase)
{
	float	flags;
	float	result;
	float	deltax, deltay;
	float	yaw;
	float	delta;

	flags = self->v.flags;
	result = 0;

	if (!((int)flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
		G_FLOAT(OFS_RETURN) = 0;

	if (chase)
	{
		delta = self->v.angles[YAW] - self->v.ideal_yaw;
		if (delta > 30 && delta < 330)
			result = delta;

		// bump around...
		if ((rand() & 3) == 1 || !SV_StepDirection(self, self->v.ideal_yaw, dist))
			SV_NewChaseDirToPoint(self, goal, dist);
	}
	else
	{
		deltax = goal[0] - self->v.origin[0];
		deltay = goal[1] - self->v.origin[1];

		yaw = (int)(atan2(deltay, deltax) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		SV_WalkDirection(self, yaw, dist);
	}

	return result;
}

/*
======================
PF_MoveToGoal

float(vector goal, float dist, float chase) movetogoal
======================
*/
int PF_MoveToGoal(void)
{
	edict_t		*ent;
	float		result;

	ent = EDICT_NUM(pr_global_struct->self);
	result = SV_MoveToGoal_internal(ent, G_VECTOR(OFS_PARM1), G_FLOAT(OFS_PARM0), (int)G_FLOAT(OFS_PARM2));
	G_FLOAT(OFS_RETURN) = result;

	return 1;
}
