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
#include "eiface.h"

#define MOVETYPE_NONE			0
#define MOVETYPE_WALK			3
#define MOVETYPE_STEP			4
#define MOVETYPE_FLY			5
#define MOVETYPE_TOSS			6
#define MOVETYPE_PUSH			7
#define MOVETYPE_NOCLIP			8
#define MOVETYPE_FLYMISSILE		9
#define MOVETYPE_BOUNCE			10
#define MOVETYPE_BOUNCEMISSILE	11
#define MOVETYPE_FOLLOW			12
#define MOVETYPE_PUSHSTEP		13

#define STOP_EPSILON			0.1f
#define MOVE_EPSILON			0.01f

extern double		host_frametime;
extern edict_t		*sv_edicts;
extern int			*sv_num_edicts;

extern globalvars_t	*pr_global_struct;

#define gGlobalVariables (*pr_global_struct)
#define sv_maxclients (svs.maxclients)

extern cvar_t		sv_gravity;

extern edict_t		*SV_TestEntityPosition(edict_t *ent);
extern void			PR_ExecuteProgram(int fnum);
extern void			*ED_CallDispatch(void *ent, int callback_type, void *param);

void SV_Physics_Pusher(edict_t *ent);
void SV_Physics_Client(edict_t *ent, int num);
void SV_Physics_None(edict_t *ent);
void SV_Physics_Follow(edict_t *ent);
void SV_Physics_Noclip(edict_t *ent);
void SV_Physics_Toss(edict_t *ent);
void SV_Physics_Step(edict_t *ent);

void SV_Impact(edict_t *e1, edict_t *e2)
{
	int old_self;
	int old_other;

	gGlobalVariables.time = (float)sv.time;

	old_self = gGlobalVariables.self;
	old_other = gGlobalVariables.other;

	if (e1->v.solid != SOLID_NOT)
	{
		gGlobalVariables.self = EDICT_TO_PROG(e1);
		gGlobalVariables.other = EDICT_TO_PROG(e2);
		(void)ED_CallDispatch(e1, 2, 0);
	}

	if (e2->v.solid != SOLID_NOT)
	{
		gGlobalVariables.self = EDICT_TO_PROG(e2);
		gGlobalVariables.other = EDICT_TO_PROG(e1);
		(void)ED_CallDispatch(e2, 2, 0);
	}

	gGlobalVariables.self = old_self;
	gGlobalVariables.other = old_other;
}

int ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	int blocked;
	float backoff;
	float change;
	int i;
	double temp;

	blocked = 0;

	if (normal[2] > 0.0f)
		blocked |= 1;

	if (normal[2] == 0.0f)
		blocked |= 2;

	backoff = DotProduct(in, normal) * overbounce;

	for (i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;

		temp = out[i];
		if (temp > -0.1 && temp < 0.1)
			out[i] = 0.0f;
	}

	return blocked;
}

int SV_FlyMove(edict_t *ent, float time, trace_t *steptrace)
{
	int blocked;
	int bumpcount;
	int numbumps;
	int numplanes;
	vec3_t dir;
	float d;
	vec3_t primal_velocity;
	vec3_t original_velocity;
	vec3_t new_velocity;
	float planes[5][3];
	vec3_t end;
	float time_left;
	int i, j;
	trace_t trace;
	edict_t *hit_ent;

	blocked = 0;
	numbumps = 4;

	VectorCopy(ent->v.velocity, primal_velocity);
	VectorCopy(ent->v.velocity, original_velocity);

	numplanes = 0;
	time_left = time;

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (ent->v.velocity[0] == 0.0f && ent->v.velocity[1] == 0.0f && ent->v.velocity[2] == 0.0f)
			break;

		VectorMA(ent->v.origin, time_left, ent->v.velocity, end);

		trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);

		if (trace.allsolid)
		{
			VectorCopy(vec3_origin, ent->v.velocity);
			return 4;
		}

		if (trace.fraction > 0.0f)
		{
			VectorCopy(trace.endpos, ent->v.origin);
			VectorCopy(ent->v.velocity, primal_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1.0f)
			break;

		hit_ent = trace.ent;
		if (!hit_ent)
			Sys_Error("SV_FlyMove: trace.ent == NULL");

		if (trace.plane.normal[2] > 0.7f)
		{
			blocked |= 1;

			if (hit_ent->v.solid == SOLID_BSP || hit_ent->v.movetype == MOVETYPE_PUSHSTEP)
			{
				ent->v.flags = (float)((int)ent->v.flags | FL_ONGROUND);
				ent->v.groundentity = EDICT_TO_PROG(hit_ent);
			}
		}

		if (trace.plane.normal[2] == 0.0f)
		{
			blocked |= 2;
			if (steptrace)
				*steptrace = trace;
		}

		SV_Impact(ent, hit_ent);

		if (ent->free)
			break;

		time_left -= time_left * trace.fraction;

		if (numplanes >= 5)
		{
			VectorCopy(vec3_origin, ent->v.velocity);
			return blocked;
		}

		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;

		for (i = 0; i < numplanes; i++)
		{
			ClipVelocity(primal_velocity, planes[i], new_velocity, 1.0f);

			for (j = 0; j < numplanes; j++)
			{
				if (j != i)
				{
					if (DotProduct(planes[j], new_velocity) < 0.0f)
						break;
				}
			}

			if (j == numplanes)
				break;
		}

		if (i != numplanes)
		{

			VectorCopy(new_velocity, ent->v.velocity);
		}
		else
		{

			if (numplanes != 2)
				return blocked;

			CrossProduct(planes[0], planes[1], dir);
			d = DotProduct(ent->v.velocity, dir);
			VectorScale(dir, d, ent->v.velocity);
		}

		if (DotProduct(ent->v.velocity, original_velocity) <= 0.0f)
		{
			VectorCopy(vec3_origin, ent->v.velocity);
			return blocked;
		}
	}

	return blocked;
}

void SV_AddGravity(edict_t *ent)
{
	float ent_gravity;
	float gravity_mult;

	if (ent->v.armortype == 0.0f)
		ent_gravity = 1.0f;
	else
		ent_gravity = ent->v.armortype;

	gravity_mult = ent_gravity * sv_gravity.value * (float)host_frametime;
	ent->v.velocity[2] -= gravity_mult;
}

void SV_CheckVelocity(edict_t *ent)
{
	int i;

	for (i = 0; i < 3; i++)
	{

		if (IS_NAN(ent->v.velocity[i]))
		{
			Con_Printf("Got a NaN velocity\n");
			ent->v.velocity[i] = 0.0f;
		}

		if (IS_NAN(ent->v.origin[i]))
		{
			Con_Printf("Got a NaN origin\n");
			ent->v.origin[i] = 0.0f;
		}

		if (ent->v.velocity[i] > sv_maxvelocity.value)
			ent->v.velocity[i] = sv_maxvelocity.value;
		else if (ent->v.velocity[i] < -sv_maxvelocity.value)
			ent->v.velocity[i] = -sv_maxvelocity.value;
	}
}

qboolean SV_RunThink(edict_t *ent)
{
	float thinktime;

	thinktime = ent->v.nextthink;

	if (thinktime <= 0)
		return true;

	if (thinktime > sv.time + host_frametime)
		return true;

	if (thinktime < sv.time)
		thinktime = sv.time;

	ent->v.nextthink = 0;
	gGlobalVariables.time = thinktime;
	gGlobalVariables.self = EDICT_TO_PROG(ent);
	gGlobalVariables.other = 0;

	(void)ED_CallDispatch(ent, 1, 0);

	return !ent->free;
}

trace_t SV_PushEntity(edict_t *ent, vec3_t push)
{
	trace_t trace;
	vec3_t end;
	int type;

	VectorAdd(ent->v.origin, push, end);

	if (ent->v.movetype == MOVETYPE_FLYMISSILE)
		type = MOVE_MISSILE;
	else if (ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT)
		type = MOVE_NOMONSTERS;
	else
		type = MOVE_NORMAL;

	trace = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, type, ent);

	VectorCopy(trace.endpos, ent->v.origin);
	SV_LinkEdict(ent, true);

	if (trace.ent)
		SV_Impact(ent, trace.ent);

	return trace;
}

void SV_PushMove(edict_t *pusher, float movetime)
{
	int i, e;
	edict_t *check;
	vec3_t mins, maxs, move;
	vec3_t entorig, pushorig;
	int num_moved;
	edict_t *moved_edict[MAX_EDICTS];
	vec3_t moved_from[MAX_EDICTS];

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

	VectorAdd(pusher->v.origin, move, pusher->v.origin);
	pusher->v.ltime += movetime;
	SV_LinkEdict(pusher, false);

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

		if (((int)check->v.flags & FL_ONGROUND) == 0 || PROG_TO_EDICT(check->v.groundentity) != pusher)
		{

			if (check->v.absmin[0] >= maxs[0]
			 || check->v.absmin[1] >= maxs[1]
			 || check->v.absmin[2] >= maxs[2]
			 || check->v.absmax[0] <= mins[0]
			 || check->v.absmax[1] <= mins[1]
			 || check->v.absmax[2] <= mins[2])
				continue;

			if (!SV_TestEntityPosition(check))
				continue;
		}

		if (check->v.movetype != MOVETYPE_WALK)
			check->v.flags = (float)((int)check->v.flags & ~FL_ONGROUND);

		VectorCopy(check->v.origin, entorig);
		VectorCopy(check->v.origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		pusher->v.solid = SOLID_NOT;
		SV_PushEntity(check, move);
		pusher->v.solid = SOLID_BSP;

		if (SV_TestEntityPosition(check))
		{
			if (check->v.mins[0] != check->v.maxs[0])
			{
				if (check->v.solid != SOLID_NOT && check->v.solid != SOLID_TRIGGER)
				{

					VectorCopy(entorig, check->v.origin);
					SV_LinkEdict(check, true);

					VectorCopy(pushorig, pusher->v.origin);
					pusher->v.ltime -= movetime;
					SV_LinkEdict(pusher, false);

					gGlobalVariables.self = EDICT_TO_PROG(pusher);
					gGlobalVariables.other = EDICT_TO_PROG(check);
					(void)ED_CallDispatch(pusher, 4, 0);

					for (i = 0; i < num_moved; i++)
					{
						VectorCopy(moved_from[i], moved_edict[i]->v.origin);
						SV_LinkEdict(moved_edict[i], false);
					}
					return;
				}

				{
					const float minsZ = check->v.mins[2];
					check->v.mins[0] = 0.0f;
					check->v.maxs[0] = 0.0f;
					check->v.maxs[2] = minsZ;
					check->v.mins[1] = 0.0f;
					check->v.maxs[1] = 0.0f;
				}
			}
		}
	}
}

void SV_PushRotate(edict_t *pusher, float movetime)
{
	int i, e;
	edict_t *check;
	vec3_t move, amove;
	vec3_t org, org2;
	vec3_t forward, right, up;
	vec3_t entorig, pushorig, pushang;
	int num_moved;
	edict_t *moved_edict[MAX_EDICTS];
	vec3_t moved_from[MAX_EDICTS];

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

	VectorAdd(pusher->v.angles, amove, pusher->v.angles);
	pusher->v.ltime += movetime;
	SV_LinkEdict(pusher, false);

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

		if (((int)check->v.flags & FL_ONGROUND) == 0 || PROG_TO_EDICT(check->v.groundentity) != pusher)
		{

			if (check->v.absmin[0] >= pusher->v.absmax[0]
			 || check->v.absmin[1] >= pusher->v.absmax[1]
			 || check->v.absmin[2] >= pusher->v.absmax[2]
			 || check->v.absmax[0] <= pusher->v.absmin[0]
			 || check->v.absmax[1] <= pusher->v.absmin[1]
			 || check->v.absmax[2] <= pusher->v.absmin[2])
				continue;

			if (!SV_TestEntityPosition(check))
				continue;
		}

		if (check->v.movetype != MOVETYPE_WALK)
			check->v.flags = (float)((int)check->v.flags & ~FL_ONGROUND);

		VectorCopy(check->v.origin, entorig);
		VectorCopy(check->v.origin, moved_from[num_moved]);
		moved_edict[num_moved] = check;
		num_moved++;

		VectorSubtract(check->v.origin, pusher->v.origin, org);
		org2[0] = DotProduct(org, forward);
		org2[1] = -DotProduct(org, right);
		org2[2] = DotProduct(org, up);
		VectorSubtract(org2, org, move);

		pusher->v.solid = SOLID_NOT;
		SV_PushEntity(check, move);
		pusher->v.solid = SOLID_BSP;

		if (SV_TestEntityPosition(check))
		{
			if (check->v.mins[0] != check->v.maxs[0])
			{
				if (check->v.solid != SOLID_NOT && check->v.solid != SOLID_TRIGGER)
				{

					VectorCopy(entorig, check->v.origin);
					SV_LinkEdict(check, true);

					VectorCopy(pushang, pusher->v.angles);
					VectorCopy(pushorig, pusher->v.origin);
					pusher->v.ltime -= movetime;
					SV_LinkEdict(pusher, false);

					gGlobalVariables.self = EDICT_TO_PROG(pusher);
					gGlobalVariables.other = EDICT_TO_PROG(check);
					(void)ED_CallDispatch(pusher, 4, 0);

					for (i = 0; i < num_moved; i++)
					{
						VectorCopy(moved_from[i], moved_edict[i]->v.origin);
						VectorSubtract(moved_edict[i]->v.angles, amove, moved_edict[i]->v.angles);
						SV_LinkEdict(moved_edict[i], false);
					}
					return;
				}

				{
					const float minsZ = check->v.mins[2];
					check->v.mins[0] = 0.0f;
					check->v.maxs[0] = 0.0f;
					check->v.maxs[2] = minsZ;
					check->v.mins[1] = 0.0f;
					check->v.maxs[1] = 0.0f;
				}
			}
		}
		else
		{
			VectorAdd(check->v.angles, amove, check->v.angles);
		}
	}
}

void SV_Physics(void)
{
	int i;
	edict_t *ent;
	float time;

	pr_global_struct->self = 0;
	pr_global_struct->other = 0;
	time = (float)sv_time;
	pr_global_struct->time = time;

	DispatchEntityCallback(3);

	ent = sv_edicts;

	for (i = 0; i < *sv_num_edicts; i++)
	{
		if (!ent->free)
		{

			if (pr_global_struct->force_retouch != 0.0f)
				SV_LinkEdict(ent, 1);

			if (i > 0 && i <= sv_maxclients)
			{
				SV_Physics_Client(ent, i);
			}
			else
			{
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
		}

		ent = (edict_t *)((char *)ent + pr_edict_size);
	}

	if (pr_global_struct->force_retouch != 0.0f)
		pr_global_struct->force_retouch -= 1.0f;

	sv_time += host_frametime;
}

trace_t *SV_Trace_Toss(trace_t *result, edict_t *ent_start, edict_t *passent)
{
	double old_frametime;
	edict_t ent_copy;
	trace_t trace;
	vec3_t move;
	vec3_t end;
	float frametime;

	old_frametime = host_frametime;
	host_frametime = 0.05;

	memcpy(&ent_copy, ent_start, sizeof(edict_t));

	do
	{
		do
		{
			SV_CheckVelocity(&ent_copy);
			SV_AddGravity(&ent_copy);

			frametime = (float)host_frametime;
			VectorMA(ent_copy.v.angles, frametime, ent_copy.v.avelocity, ent_copy.v.angles);

			VectorScale(ent_copy.v.velocity, frametime, move);

			VectorAdd(move, ent_copy.v.origin, end);

			trace = SV_Move(ent_copy.v.origin, ent_copy.v.mins, ent_copy.v.maxs, end, MOVE_NORMAL, &ent_copy);

			VectorCopy(trace.endpos, ent_copy.v.origin);
		}
		while (!trace.ent);
	}
	while (trace.ent == passent);

	host_frametime = old_frametime;

	if (result)
		*result = trace;

	return result;
}

void SV_Physics_Pusher(edict_t *ent)
{
	float old_ltime;
	float thinktime;
	float movetime;

	old_ltime = ent->v.ltime;
	thinktime = ent->v.nextthink;

	if (ent->v.nextthink < ent->v.ltime + host_frametime)
	{
		movetime = ent->v.nextthink - ent->v.ltime;
		if (movetime < 0)
			movetime = 0;
	}
	else
		movetime = host_frametime;

	if (movetime != 0.0f)
	{

		if (ent->v.avelocity[0] || ent->v.avelocity[1] || ent->v.avelocity[2])
		{
			SV_PushRotate(ent, movetime);
		}
		else
		{
			SV_PushMove(ent, movetime);
		}
	}

	if (ent->v.nextthink > old_ltime && ent->v.nextthink <= ent->v.ltime)
	{
		ent->v.nextthink = 0;
		gGlobalVariables.time = sv.time;
		gGlobalVariables.self = EDICT_TO_PROG(ent);
		gGlobalVariables.other = 0;
		(void)ED_CallDispatch(ent, 1, 0);
	}
}

void SV_CheckStuck(edict_t *ent)
{
	vec3_t saved_origin;
	int z, x, y;

	if (SV_TestEntityPosition(ent) == 0)
	{

		VectorCopy(ent->v.origin, ent->v.oldorigin);
		return;
	}

	VectorCopy(ent->v.origin, saved_origin);
	VectorCopy(ent->v.oldorigin, ent->v.origin);

	if (SV_TestEntityPosition(ent) == 0)
	{
		Con_Printf("Unstuck.\n");
		SV_LinkEdict(ent, 1);
		return;
	}

	for (z = 0; z < 18; z++)
	{
		for (x = -1; x <= 1; x++)
		{
			for (y = -1; y <= 1; y++)
			{
				ent->v.origin[0] = saved_origin[0] + (float)x;
				ent->v.origin[1] = saved_origin[1] + (float)y;
				ent->v.origin[2] = saved_origin[2] + (float)z;

				if (SV_TestEntityPosition(ent) == 0)
				{
					Con_Printf("Unstuck.\n");
					SV_LinkEdict(ent, 1);
					return;
				}
			}
		}
	}

	VectorCopy(saved_origin, ent->v.origin);
	Con_Printf("player is stuck.\n");
}

int SV_CheckWater(edict_t *ent)
{
	static const vec3_t current_table[] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, -1.0f },
	};

	vec3_t test_point;
	int point_contents;
	int true_contents;

	ent->v.waterlevel = 0.0f;
	ent->v.watertype = CONTENTS_EMPTY;

	test_point[0] = ent->v.origin[0];
	test_point[1] = ent->v.origin[1];
	test_point[2] = ent->v.origin[2] + ent->v.mins[2] + 1.0f;

	point_contents = SV_PointContents(test_point);
	if (point_contents <= CONTENTS_WATER && point_contents > CONTENTS_TRANSLUCENT)
	{
		true_contents = SV_TruePointContents(test_point);

		ent->v.waterlevel = 1.0f;
		ent->v.watertype = (float)point_contents;

		test_point[2] = ent->v.origin[2] + (ent->v.maxs[2] + ent->v.mins[2]) * 0.5f;
		point_contents = SV_PointContents(test_point);

		if (point_contents <= CONTENTS_WATER && point_contents > CONTENTS_TRANSLUCENT)
		{
			ent->v.waterlevel = 2.0f;

			test_point[2] = ent->v.origin[2] + ent->v.view_ofs[2];
			point_contents = SV_PointContents(test_point);

			if (point_contents <= CONTENTS_WATER && point_contents > CONTENTS_TRANSLUCENT)
				ent->v.waterlevel = 3.0f;
		}

		if (true_contents <= CONTENTS_CURRENT_0 && true_contents >= CONTENTS_CURRENT_DOWN)
		{
			const int current_index = CONTENTS_CURRENT_0 - true_contents;
			const float current_scale = ent->v.waterlevel * 150.0f / 3.0f;
			VectorMA(ent->v.basevelocity, current_scale, current_table[current_index], ent->v.basevelocity);
		}
	}

	return ent->v.waterlevel > 1.0f;
}

void SV_WallFriction(edict_t *ent, trace_t *trace)
{
	vec3_t forward, right, up;
	float d;
	float normal_speed;
	vec3_t normal_vel;
	float sideways_x;
	float sideways_y;

	AngleVectors(ent->v.v_angle, forward, right, up);
	(void)right;
	(void)up;

	d = trace->plane.normal[0] * forward[0] + trace->plane.normal[1] * forward[1] + trace->plane.normal[2] * forward[2];
	d += 0.5f;

	if (d < 0.0f)
	{
		normal_speed = ent->v.velocity[0] * trace->plane.normal[0] +
		               ent->v.velocity[1] * trace->plane.normal[1] +
		               ent->v.velocity[2] * trace->plane.normal[2];

		VectorScale(trace->plane.normal, normal_speed, normal_vel);

		sideways_x = ent->v.velocity[0] - normal_vel[0];
		sideways_y = ent->v.velocity[1] - normal_vel[1];

		d += 1.0f;

		ent->v.velocity[0] = sideways_x * d;
		ent->v.velocity[1] = sideways_y * d;

		SV_CheckVelocity(ent);
	}
}

int SV_TryUnstick(edict_t *ent, vec3_t oldvel)
{
	trace_t trace;
	vec3_t base_origin;
	vec3_t move;
	int i;
	int blocked;

	VectorCopy(ent->v.origin, base_origin);
	VectorClear(move);

	for (i = 0; i < 8; i++)
	{
		switch (i)
		{
			case 0:
				move[0] = 2.0f;
				move[1] = 0.0f;
				break;
			case 1:
				move[0] = 0.0f;
				move[1] = 2.0f;
				break;
			case 2:
				move[0] = -2.0f;
				move[1] = 0.0f;
				break;
			case 3:
				move[0] = 0.0f;
				move[1] = -2.0f;
				break;
			case 4:
				move[0] = 2.0f;
				move[1] = 2.0f;
				break;
			case 5:
				move[0] = -2.0f;
				move[1] = 2.0f;
				break;
			case 6:
				move[0] = 2.0f;
				move[1] = -2.0f;
				break;
			case 7:
				move[0] = -2.0f;
				move[1] = -2.0f;
				break;
			default:
				break;
		}

		(void)SV_PushEntity(ent, move);

		ent->v.velocity[0] = oldvel[0];
		ent->v.velocity[1] = oldvel[1];
		ent->v.velocity[2] = 0.0f;

		SV_CheckVelocity(ent);
		blocked = SV_FlyMove(ent, 0.1f, &trace);
		SV_CheckVelocity(ent);

		if (fabs(base_origin[1] - ent->v.origin[1]) > 4.0f || fabs(base_origin[0] - ent->v.origin[0]) > 4.0f)
			return blocked;

		VectorCopy(base_origin, ent->v.origin);
	}

	VectorClear(ent->v.velocity);
	return 7;
}

void SV_WalkMove(edict_t *ent)
{
	trace_t steptrace;
	trace_t downtrace;
	int old_flags;
	vec3_t original_origin;
	vec3_t original_velocity;
	int blocked;

	old_flags = (int)ent->v.flags;
	ent->v.flags = (float)(old_flags & ~FL_ONGROUND);

	VectorCopy(ent->v.origin, original_origin);
	VectorCopy(ent->v.velocity, original_velocity);

	blocked = SV_FlyMove(ent, (float)host_frametime, &steptrace);

	if (blocked & 2)
	{
		/* Con_DPrintf("clip %d\n", blocked); */
		SV_CheckVelocity(ent);

		if (((old_flags & FL_ONGROUND) != 0 || ent->v.waterlevel != 0.0f) &&
		    ent->v.movetype == MOVETYPE_WALK &&
		    sv_nostep.value == 0.0f &&
		    (((int)sv_player->v.flags & FL_WATERJUMP) == 0))
		{
			vec3_t moved_origin;
			vec3_t moved_velocity;
			vec3_t up;
			vec3_t down;
			int step_blocked;

			VectorCopy(ent->v.origin, moved_origin);
			VectorCopy(ent->v.velocity, moved_velocity);

			VectorCopy(original_origin, ent->v.origin);

			up[0] = 0.0f;
			up[1] = 0.0f;
			up[2] = 18.0f;

			down[0] = 0.0f;
			down[1] = 0.0f;
			down[2] = (float)(original_velocity[2] * host_frametime - 18.0);

			(void)SV_PushEntity(ent, up);

			ent->v.velocity[0] = original_velocity[0];
			ent->v.velocity[1] = original_velocity[1];
			ent->v.velocity[2] = 0.0f;

			SV_CheckVelocity(ent);
			step_blocked = SV_FlyMove(ent, (float)host_frametime, &steptrace);
			SV_CheckVelocity(ent);

			if (step_blocked &&
			    fabs(original_origin[1] - ent->v.origin[1]) < 0.03125f &&
			    fabs(original_origin[0] - ent->v.origin[0]) < 0.03125f)
			{
				step_blocked = (step_blocked & ~0xFF) | (SV_TryUnstick(ent, original_velocity) & 0xFF);
			}

			if (step_blocked & 2)
				SV_WallFriction(ent, &steptrace);

			downtrace = SV_PushEntity(ent, down);

			if (downtrace.plane.normal[2] <= 0.7f)
			{
				VectorCopy(moved_origin, ent->v.origin);
				VectorCopy(moved_velocity, ent->v.velocity);
				SV_CheckVelocity(ent);
			}
			else if (ent->v.solid == SOLID_BSP || ent->v.movetype == MOVETYPE_PUSHSTEP)
			{
				ent->v.flags = (float)((int)ent->v.flags | FL_ONGROUND);
				ent->v.groundentity = EDICT_TO_PROG(downtrace.ent);
			}
		}
	}
}

void SV_Physics_Client(edict_t *ent, int client_num)
{
	server_client_t *client;

	client = &svs.clients[client_num - 1];
	if (!client->active)
		return;

	pr_global_struct->time = (float)sv_time;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	DispatchEntityCallback(1);

	SV_CheckVelocity(ent);

	switch ((int)ent->v.movetype)
	{
		case MOVETYPE_NONE:
			if (!SV_RunThink(ent))
				return;
			break;

		case MOVETYPE_WALK:
			if (!SV_RunThink(ent))
				return;

			if (!SV_CheckWater(ent) && (((int)ent->v.flags & FL_WATERJUMP) == 0))
				SV_AddGravity(ent);

			SV_CheckStuck(ent);

			ent->v.velocity[0] += ent->v.basevelocity[0];
			ent->v.velocity[1] += ent->v.basevelocity[1];
			ent->v.velocity[2] += ent->v.basevelocity[2];

			SV_CheckVelocity(ent);
			SV_WalkMove(ent);
			SV_CheckVelocity(ent);

			ent->v.velocity[0] -= ent->v.basevelocity[0];
			ent->v.velocity[1] -= ent->v.basevelocity[1];
			ent->v.velocity[2] -= ent->v.basevelocity[2];

			SV_CheckVelocity(ent);
			break;

		case MOVETYPE_FLY:
			if (!SV_RunThink(ent))
				return;
			(void)SV_FlyMove(ent, (float)host_frametime, NULL);
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

	SV_LinkEdict(ent, true);

	pr_global_struct->time = (float)sv_time;
	pr_global_struct->self = EDICT_TO_PROG(ent);
	DispatchEntityCallback(2);
}

void SV_Physics_None(edict_t *ent)
{
	SV_RunThink(ent);
}

void SV_Physics_Follow(edict_t *ent)
{
	edict_t *aiment;

	SV_RunThink(ent);

	aiment = EDICT_NUM((int)ent->v.aiment);

	if (aiment == sv_edicts)
	{

		ent->v.movetype = MOVETYPE_NONE;
		return;
	}

	VectorAdd(aiment->v.origin, ent->v.view_ofs, ent->v.origin);

	VectorCopy(aiment->v.angles, ent->v.angles);

	SV_LinkEdict(ent, 1);
}

void SV_Physics_Noclip(edict_t *ent)
{
	float frametime = (float)host_frametime;

	if (SV_RunThink(ent))
	{

		VectorMA(ent->v.angles, frametime, ent->v.avelocity, ent->v.angles);

		VectorMA(ent->v.origin, frametime, ent->v.velocity, ent->v.origin);

		SV_LinkEdict(ent, 0);
	}
}

int SV_CheckWaterTransition(edict_t *ent)
{
	vec3_t test_point;
	int contents;

	test_point[0] = ent->v.origin[0];
	test_point[1] = ent->v.origin[1];
	test_point[2] = ent->v.origin[2] + ent->v.mins[2] + 1.0f;

	contents = SV_PointContents(test_point);

	if (ent->v.watertype == 0.0f)
	{
		ent->v.waterlevel = 1;
		ent->v.watertype = contents;
	}
	else if (contents > CONTENTS_WATER || contents <= CONTENTS_TRANSLUCENT)
	{

		if (ent->v.watertype != CONTENTS_EMPTY)
		{
			SV_StartSound(ent, 0, "common/h2ohit1.wav", 255, 1.0f);
		}
		ent->v.watertype = CONTENTS_EMPTY;
		ent->v.waterlevel = (float)contents;
	}
	else
	{

		if (ent->v.watertype == CONTENTS_EMPTY)
		{
			SV_StartSound(ent, 0, "common/h2ohit1.wav", 255, 1.0f);
		}
		ent->v.waterlevel = 1;
		ent->v.watertype = contents;
	}
	return contents;
}

void SV_Physics_Toss(edict_t *ent)
{
	edict_t *groundent;
	trace_t trace;
	vec3_t move;
	vec3_t end;
	float backoff;

	groundent = PROG_TO_EDICT(ent->v.groundentity);

	if (groundent && ((int)groundent->v.flags & FL_CONVEYOR))
	{
		VectorScale(groundent->v.velocity, 1.0f, ent->v.basevelocity);
	}
	else
	{
		VectorClear(ent->v.basevelocity);
	}

	SV_CheckWater(ent);

	if (SV_RunThink(ent))
	{

		if (ent->v.velocity[2] > 0.0f)
			ent->v.flags = (float)((int)ent->v.flags & ~FL_ONGROUND);

		if (((int)ent->v.flags & FL_ONGROUND) == 0 || !VectorCompare(ent->v.basevelocity, vec3_origin))
		{
			SV_LinkEdict(ent, 0);

			if (((int)ent->v.flags & FL_ONGROUND) == 0)
			{
				if (ent->v.movetype != MOVETYPE_FLY && ent->v.movetype != MOVETYPE_FLYMISSILE && ent->v.movetype != MOVETYPE_BOUNCEMISSILE)
					SV_AddGravity(ent);
			}

			VectorMA(ent->v.angles, host_frametime, ent->v.avelocity, ent->v.angles);

			VectorAdd(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);

			VectorScale(ent->v.velocity, host_frametime, move);
			VectorAdd(ent->v.origin, move, end);

			trace = SV_PushEntity(ent, move);

			VectorSubtract(ent->v.velocity, ent->v.basevelocity, ent->v.velocity);

			if (trace.fraction != 1.0f && !ent->free)
			{
				if (ent->v.movetype == MOVETYPE_BOUNCEMISSILE)
					backoff = 2.0f;
				else if (ent->v.movetype == MOVETYPE_BOUNCE)
					backoff = 1.5f;
				else
					backoff = 1.0f;

				ClipVelocity(ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff);

				if (trace.plane.normal[2] > 0.7f)
				{
					if (ent->v.velocity[2] < 60 || ent->v.movetype != MOVETYPE_BOUNCE)
					{
						ent->v.flags = (float)((int)ent->v.flags | FL_ONGROUND);
						ent->v.groundentity = EDICT_TO_PROG(trace.ent);
						VectorClear(ent->v.velocity);
						VectorClear(ent->v.avelocity);
					}
				}

				SV_CheckWaterTransition(ent);
			}
		}
	}
}

void SV_Physics_Step(edict_t *ent)
{
	qboolean hitsound = false;
	qboolean inwater;
	qboolean wasonground;
	float speed, newspeed, control;
	float friction;
	edict_t *groundentity;

	groundentity = PROG_TO_EDICT(ent->v.groundentity);
	if (groundentity && ((int)groundentity->v.flags & FL_CONVEYOR))
	{
		VectorScale(groundentity->v.movedir, groundentity->v.speed, ent->v.basevelocity);
	}
	else
	{
		VectorCopy(vec3_origin, ent->v.basevelocity);
	}

	gGlobalVariables.self = EDICT_TO_PROG(ent);
	gGlobalVariables.time = sv.time;

	SV_LinkEdict(ent, false);

	wasonground = ((int)ent->v.flags & FL_ONGROUND) != 0;
	inwater = SV_CheckWater(ent);

	if (!wasonground)
	{
		if (((int)ent->v.flags & (FL_FLY | FL_SWIM)) == 0)
		{
			if (sv_gravity.value * -0.1f > ent->v.velocity[2])
				hitsound = true;
			if (!inwater)
				SV_AddGravity(ent);
		}
	}

	if (!VectorCompare(ent->v.velocity, vec3_origin) || !VectorCompare(ent->v.basevelocity, vec3_origin))
	{
		ent->v.flags = (float)((int)ent->v.flags & ~FL_ONGROUND);

		if (wasonground && (ent->v.waterlevel > 0 || SV_CheckBottom(ent)))
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

		if (((int)ent->v.flags & FL_ONGROUND) && groundentity)
		{
			if ((int)groundentity->v.flags & FL_CONVEYOR)
			{

			}
			else
			{
				ent->v.flags = (float)((int)ent->v.flags | FL_ONGROUND);
				ent->v.groundentity = EDICT_TO_PROG(groundentity);
				VectorCopy(vec3_origin, ent->v.velocity);
				VectorCopy(vec3_origin, ent->v.avelocity);
			}
		}
	}

	SV_LinkEdict(ent, true);

	if (hitsound && ((int)ent->v.flags & FL_ONGROUND) && !wasonground)
		SV_StartSound(ent, 0, "common/thump.wav", 255, 1.0f);

	SV_RunThink(ent);
	SV_CheckWaterTransition(ent);
}

int SV_CompareEntityState(int length)
{

	return memcmp((void *)((int)&sv + 236), (void *)0, length);
}
