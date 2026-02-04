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

#include <math.h>
#include "quakedef.h"

#define DI_NODIR    -1
#define M_PI        3.14159265358979323846

#include "edict.h"
#include "progdefs.h"

extern edict_t  *sv_edicts;
extern int      sv_edicts_base;
extern globalvars_t *pr_global_struct;

#define gGlobalVariables (*pr_global_struct)

extern float anglemod(float angle);
extern void SV_LinkEdict(edict_t *ent, qboolean touch_triggers);

extern double sin(double);
extern double cos(double);
extern double atan2(double, double);

void SV_ChangeYaw(edict_t *ent)
{
	float current_yaw;
	float ideal_yaw;
	float yaw_speed;
	float move;

	current_yaw = anglemod(ent->v.angles[1]);
	ideal_yaw = ent->v.ideal_yaw;
	yaw_speed = ent->v.yaw_speed;

	if (current_yaw == ideal_yaw)
		return;

	move = ideal_yaw - current_yaw;

	if (ideal_yaw > current_yaw)
	{
		if (move >= 180.0f)
			move -= 360.0f;
	}
	else
	{
		if (move <= -180.0f)
			move += 360.0f;
	}

	if (move > 0.0f)
	{
		if (move > yaw_speed)
			move = yaw_speed;
	}
	else
	{
		if (move < -yaw_speed)
			move = -yaw_speed;
	}

	ent->v.angles[1] = anglemod(current_yaw + move);
}

void SV_ChangePitch(edict_t *ent)
{
	float current_pitch;
	float ideal_pitch;
	float pitch_speed;
	float move;

	current_pitch = anglemod(ent->v.angles[0]);
	ideal_pitch = ent->v.idealpitch;
	pitch_speed = ent->v.pitch_speed;

	if (current_pitch == ideal_pitch)
		return;

	move = ideal_pitch - current_pitch;

	if (ideal_pitch > current_pitch)
	{
		if (move >= 180.0f)
			move -= 360.0f;
	}
	else
	{
		if (move <= -180.0f)
			move += 360.0f;
	}

	if (move > 0.0f)
	{
		if (move > pitch_speed)
			move = pitch_speed;
	}
	else
	{
		if (move < -pitch_speed)
			move = -pitch_speed;
	}

	ent->v.angles[0] = anglemod(current_pitch + move);
}

void PF_changeyaw(void)
{
	SV_ChangeYaw(PROG_TO_EDICT(pr_global_struct->self));
}

static int sv_checkbottom_traces;
static int sv_checkbottom_successes;

int SV_CheckBottom(edict_t *ent)
{
	float *ent_f;
	float max_x;
	float min_x;
	float min_y;
	float max_y;
	float bottom_z;
	vec3_t point;
	int x;
	int y;
	vec3_t start;
	vec3_t stop;
	vec3_t trace_mins;
	vec3_t trace_maxs;
	trace_t trace;
	float mid_z;
	float highest_z;

	ent_f = (float *)ent;

	max_x = ent_f[78] + ent_f[40];
	min_x = ent_f[75] + ent_f[40];
	min_y = ent_f[76] + ent_f[41];
	bottom_z = ent_f[42] + ent_f[77];
	max_y = ent_f[79] + ent_f[41];

	point[2] = bottom_z - 1.0f;
	for (x = 0; x <= 1; ++x)
	{
		point[0] = x ? max_x : min_x;
		for (y = 0; y <= 1; ++y)
		{
			point[1] = y ? max_y : min_y;
			if (SV_PointContents(point) != CONTENTS_SOLID)
				goto check_edges;
		}
	}

	++sv_checkbottom_successes;
	return 1;

check_edges:
	start[0] = (max_x + min_x) * 0.5f;
	start[1] = (max_y + min_y) * 0.5f;
	start[2] = bottom_z + 18.0f;

	stop[0] = start[0];
	stop[1] = start[1];
	stop[2] = start[2] - 36.0f;

	trace_mins[0] = 0.0f;
	trace_mins[1] = 0.0f;
	trace_mins[2] = 0.0f;
	trace_maxs[0] = 0.0f;
	trace_maxs[1] = 0.0f;
	trace_maxs[2] = 0.0f;

	++sv_checkbottom_traces;
	trace = SV_Move(start, trace_mins, trace_maxs, stop, 1, ent);
	if (trace.fraction == 1.0f)
		return 0;

	mid_z = trace.endpos[2];
	highest_z = trace.endpos[2];
	for (x = 0; x <= 1; ++x)
	{
		for (y = 0; y <= 1; ++y)
		{
			start[0] = x ? max_x : min_x;
			stop[0] = start[0];

			start[1] = y ? max_y : min_y;
			stop[1] = start[1];

			trace = SV_Move(start, trace_mins, trace_maxs, stop, 1, ent);
			if (trace.fraction != 1.0f && highest_z < trace.endpos[2])
				highest_z = trace.endpos[2];
			if (trace.fraction == 1.0f || mid_z - trace.endpos[2] > 18.0f)
				return 0;
		}
	}

	++sv_checkbottom_successes;
	return 1;
}

int SV_movestep(edict_t *ent, float *move, int relink)
{
	float *ent_f;
	int *ent_i;
	int old_x;
	int old_y;
	int old_z;
	float new_x;
	float new_y;
	float new_z;
	int flags;
	trace_t trace;

	ent_f = (float *)ent;
	ent_i = (int *)ent;

	old_x = ent_i[40];
	old_y = ent_i[41];
	old_z = ent_i[42];

	new_x = move[0] + ent_f[40];
	new_y = move[1] + ent_f[41];
	new_z = move[2] + ent_f[42];

	flags = (int)ent_f[125];

	if ((flags & 3) != 0)
	{
		int tries;
		vec3_t end;
		edict_t *move_base;

		tries = 0;
		while (1)
		{
			end[0] = move[0] + ent_f[40];
			end[1] = move[1] + ent_f[41];
			end[2] = move[2] + ent_f[42];

			move_base = (edict_t *)((byte *)sv_edicts + ent_i[124]);
			if (tries == 0 && move_base != (edict_t *)sv_edicts)
			{
				float *move_base_f;
				float height_diff;

				move_base_f = (float *)move_base;
				height_diff = ent_f[42] - move_base_f[42];
				if (height_diff > 40.0f)
					end[2] -= 8.0f;
				if (height_diff < 30.0f)
					end[2] += 8.0f;
			}

			trace = SV_Move(&ent_f[40], &ent_f[75], &ent_f[78], end, 0, ent);
			if (trace.fraction == 1.0f)
				break;

			if (move_base != (edict_t *)sv_edicts && ++tries < 2)
				continue;
			return 0;
		}

		if ((flags & 2) != 0 && SV_PointContents(trace.endpos) == CONTENTS_EMPTY)
			return 0;

		ent_f[40] = trace.endpos[0];
		ent_f[41] = trace.endpos[1];
		ent_f[42] = trace.endpos[2];

		if (!relink)
			return 1;

		SV_LinkEdict(ent, 1);
		return 1;
	}
	else
	{
		vec3_t start;
		vec3_t end;

		start[0] = new_x;
		start[1] = new_y;
		start[2] = new_z + 18.0f;

		end[0] = start[0];
		end[1] = start[1];
		end[2] = start[2] - 36.0f;

		trace = SV_Move(start, &ent_f[75], &ent_f[78], end, 0, ent);
		if (trace.allsolid)
			return 0;

		if (trace.startsolid)
		{
			start[2] -= 18.0f;
			trace = SV_Move(start, &ent_f[75], &ent_f[78], end, 0, ent);
			if (trace.allsolid || trace.startsolid)
				return 0;
		}

		if (trace.fraction == 1.0f)
		{
			if ((flags & 0x400) == 0)
				return 0;

			ent_f[40] = new_x;
			ent_f[41] = new_y;
			ent_f[42] = new_z;

			if (relink)
				SV_LinkEdict(ent, 1);

			flags &= ~0x200;
			ent_f[125] = (float)flags;
			return 1;
		}

		ent_f[40] = trace.endpos[0];
		ent_f[41] = trace.endpos[1];
		ent_f[42] = trace.endpos[2];

		if (SV_CheckBottom(ent))
		{
			if ((flags & 0x400) != 0)
			{
				flags &= ~0x400;
				ent_f[125] = (float)flags;
			}

			ent_i[89] = (int)((byte *)trace.ent - (byte *)sv_edicts);

			if (!relink)
				return 1;

			SV_LinkEdict(ent, 1);
			return 1;
		}

		if ((flags & 0x400) == 0)
		{
			ent_i[40] = old_x;
			ent_i[41] = old_y;
			ent_i[42] = old_z;
			return 0;
		}

		if (relink)
			SV_LinkEdict(ent, 1);
		return 1;
	}
}

int SV_StepDirection(edict_t *ent, float yaw, float dist)
{
	vec3_t move;
	double radians;
	float delta;
	vec3_t oldOrigin;

	ent->v.ideal_yaw = yaw;

	SV_ChangeYaw(ent);

	radians = yaw * M_PI * 2.0 / 360.0;
	move[0] = cos(radians) * dist;
	move[1] = sin(radians) * dist;
	move[2] = 0.0f;

	VectorCopy(ent->v.origin, oldOrigin);

	if (SV_movestep(ent, move, 0))
	{

		delta = ent->v.angles[1] - ent->v.ideal_yaw;
		if (delta > 30.0f && delta < 330.0f)
		{

			VectorCopy(oldOrigin, ent->v.origin);
		}
		SV_LinkEdict(ent, 1);
		return 1;
	}
	else
	{
		SV_LinkEdict(ent, 1);
		return 0;
	}
}

int SV_movestep_simple(edict_t *ent, float yaw, float dist)
{
	vec3_t move;
	double radians;

	radians = yaw * M_PI * 2.0 / 360.0;
	move[0] = cos(radians) * dist;
	move[1] = sin(radians) * dist;
	move[2] = 0.0f;

	if (SV_movestep(ent, move, 0))
	{
		SV_LinkEdict(ent, 1);
		return 1;
	}
	else
	{
		SV_LinkEdict(ent, 1);
		return 0;
	}
}

int SV_FixCheckBottom(edict_t *ent)
{

	ent->v.flags = (float)((int)ent->v.flags | 0x400);
	return (int)ent->v.flags;
}

int SV_NewChaseDir(edict_t *actor, edict_t *enemy, float dist)
{
	float deltax, deltay;
	float d[3];
	float tdir, olddir, turnaround;
	int temp;

	olddir = (float)(int)(45 * (long long)(actor->v.ideal_yaw / 45.0f));
	anglemod(olddir);

	turnaround = olddir - 180.0f;
	anglemod(turnaround);

	deltax = enemy->v.origin[0] - actor->v.origin[0];
	deltay = enemy->v.origin[1] - actor->v.origin[1];

	if (deltax <= 10.0f)
	{
		d[1] = 180.0f;
		if (deltax >= -10.0f)
			d[1] = DI_NODIR;
	}
	else
	{
		d[1] = 0.0f;
	}

	if (deltay >= -10.0f)
	{
		d[2] = 90.0f;
		if (deltay <= 10.0f)
			d[2] = DI_NODIR;
	}
	else
	{
		d[2] = 270.0f;
	}

	if (d[1] == DI_NODIR || d[2] == DI_NODIR)
		goto try_other;

	if (d[1] == 0.0f)
	{
		tdir = (d[2] == 90.0f) ? 45.0f : 315.0f;
	}
	else
	{
		tdir = (d[2] == 90.0f) ? 135.0f : 215.0f;
	}

	if (turnaround != tdir && SV_StepDirection(actor, tdir, dist))
		return 1;

try_other:

	if ((rand() & 1) || (int)abs((int)deltay) > (int)abs((int)deltax))
	{

		temp = *(int *)&d[1];
		d[1] = d[2];
		d[2] = *(float *)&temp;
	}

	if (d[1] != DI_NODIR && turnaround != d[1])
	{
		if (SV_StepDirection(actor, d[1], dist))
			return 1;
	}

	if (d[2] != DI_NODIR && turnaround != d[2])
	{
		if (SV_StepDirection(actor, d[2], dist))
			return 1;
	}

	if (olddir != DI_NODIR)
	{
		if (SV_StepDirection(actor, olddir, dist))
			return 1;
	}

	if (rand() & 1)
	{
		for (tdir = 0.0f; tdir <= 315.0f; tdir += 45.0f)
		{
			if (turnaround != tdir)
			{
				if (SV_StepDirection(actor, tdir, dist))
					return 1;
			}
		}
	}
	else
	{
		for (tdir = 315.0f; tdir >= 0.0f; tdir -= 45.0f)
		{
			if (turnaround != tdir)
			{
				if (SV_StepDirection(actor, tdir, dist))
					return 1;
			}
		}
	}

	if (turnaround != DI_NODIR)
	{
		if (SV_StepDirection(actor, turnaround, dist))
			return 1;
	}

	actor->v.ideal_yaw = olddir;

	if (!SV_CheckBottom(actor))
		return SV_FixCheckBottom(actor);

	return 0;
}

int SV_CloseEnough(edict_t *ent, edict_t *goal, float dist)
{
	int i;

	for (i = 0; i < 3; i++)
	{

		if (ent->v.absmax[i] + dist < goal->v.absmin[i])
			return 0;

		if (ent->v.absmin[i] - dist > goal->v.absmax[i])
			return 0;
	}

	return 1;
}

int SV_CloseEnough_point(edict_t *ent, float *point, float dist)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		if (ent->v.absmax[i] + dist < point[i])
			return 0;
		if (ent->v.absmin[i] - dist > point[i])
			return 0;
	}

	return 1;
}

int SV_MoveToGoal_step(edict_t *ent, float dist)
{
	int goalent_num;
	edict_t *goalent;
	int enemy_num;
	edict_t *enemy;

	if (((int)ent->v.flags & 0x203) != 0)
	{

		enemy_num = ent->v.enemy;
		enemy = EDICT_NUM(enemy_num);

		if (enemy_num == 0 || !SV_CloseEnough(ent, enemy, dist))
		{

			if (!SV_StepDirection(ent, ent->v.ideal_yaw, dist))
			{
				goalent_num = ent->v.goalentity;
				goalent = EDICT_NUM(goalent_num);
				return SV_NewChaseDir(ent, goalent, dist);
			}
			return 1;
		}
		return 1;
	}

	return 0;
}

int PF_MoveToGoal_Classic(void)
{
	int self_num = gGlobalVariables.self;
	edict_t *self = EDICT_NUM(self_num);

	return SV_MoveToGoal_step(self, gGlobalVariables.arg0[0]);
}

int SV_NewChaseDir_goal(edict_t *actor, float *goalPos, float dist)
{
	float deltax, deltay;
	float d[3];
	float tdir, olddir, turnaround;
	int temp;

	olddir = (float)(int)(45 * (long long)(actor->v.ideal_yaw / 45.0f));
	anglemod(olddir);

	turnaround = olddir - 180.0f;
	anglemod(turnaround);

	deltax = goalPos[0] - actor->v.origin[0];
	deltay = goalPos[1] - actor->v.origin[1];

	if (deltax <= 10.0f)
	{
		d[1] = 180.0f;
		if (deltax >= -10.0f)
			d[1] = DI_NODIR;
	}
	else
	{
		d[1] = 0.0f;
	}

	if (deltay >= -10.0f)
	{
		d[2] = 90.0f;
		if (deltay <= 10.0f)
			d[2] = DI_NODIR;
	}
	else
	{
		d[2] = 270.0f;
	}

	if (d[1] == DI_NODIR || d[2] == DI_NODIR)
		goto try_other;

	if (d[1] == 0.0f)
	{
		tdir = (d[2] == 90.0f) ? 45.0f : 315.0f;
	}
	else
	{
		tdir = (d[2] == 90.0f) ? 135.0f : 215.0f;
	}

	if (turnaround != tdir && SV_StepDirection(actor, tdir, dist))
		return 1;

try_other:

	if ((rand() & 1) || (int)abs((int)deltay) > (int)abs((int)deltax))
	{

		temp = *(int *)&d[1];
		d[1] = d[2];
		d[2] = *(float *)&temp;
	}

	if (d[1] != DI_NODIR && turnaround != d[1])
	{
		if (SV_StepDirection(actor, d[1], dist))
			return 1;
	}

	if (d[2] != DI_NODIR && turnaround != d[2])
	{
		if (SV_StepDirection(actor, d[2], dist))
			return 1;
	}

	if (olddir != DI_NODIR)
	{
		if (SV_StepDirection(actor, olddir, dist))
			return 1;
	}

	if (rand() & 1)
	{
		for (tdir = 0.0f; tdir <= 315.0f; tdir += 45.0f)
		{
			if (turnaround != tdir)
			{
				if (SV_StepDirection(actor, tdir, dist))
					return 1;
			}
		}
	}
	else
	{
		for (tdir = 315.0f; tdir >= 0.0f; tdir -= 45.0f)
		{
			if (turnaround != tdir)
			{
				if (SV_StepDirection(actor, tdir, dist))
					return 1;
			}
		}
	}

	if (turnaround != DI_NODIR)
	{
		if (SV_StepDirection(actor, turnaround, dist))
			return 1;
	}

	actor->v.ideal_yaw = olddir;

	if (!SV_CheckBottom(actor))
		return SV_FixCheckBottom(actor);

	return 0;
}

float SV_MoveToGoal_internal(edict_t *self, float *goalPos, float dist, int chase)
{
	float flags;
	float result;
	float deltax, deltay;
	float yaw;

	flags = self->v.flags;
	result = 0.0f;

	if (((int)flags & 0x203) == 0)
		gGlobalVariables.v_return[0] = 0;

	if (chase)
	{

		float diff = self->v.angles[1] - self->v.ideal_yaw;
		if (diff > 30.0f && diff < 330.0f)
			result = diff;

		if ((rand() & 3) == 1 || !SV_StepDirection(self, self->v.ideal_yaw, dist))
			SV_NewChaseDir_goal(self, goalPos, dist);
	}
	else
	{

		deltax = goalPos[0] - self->v.origin[0];
		deltay = goalPos[1] - self->v.origin[1];

		yaw = (float)(int)(atan2(deltay, deltax) * 180.0 / M_PI);
		if (yaw < 0.0f)
			yaw += 360.0f;

		SV_movestep_simple(self, yaw, dist);
	}

	return result;
}

int PF_MoveToGoal(void)
{
	float result;
	int self_num;
	edict_t *self;

	self_num = gGlobalVariables.self;
	self = EDICT_NUM(self_num);

	result = SV_MoveToGoal_internal(
		self,
		gGlobalVariables.arg1,
		gGlobalVariables.arg0[0],
		(int)gGlobalVariables.arg2[0]
	);

	gGlobalVariables.v_return[0] = result;

	return 1;

}
