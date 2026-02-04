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

#define MAX_FORWARD 6

void SV_SetIdealPitch(void)
{
	float angle_radians;
	float sin_yaw;
	float cos_yaw;
	trace_t trace;
	vec3_t top;
	vec3_t bottom;
	float z[MAX_FORWARD];
	int i;
	int j;
	int step;
	int dir;
	int steps;

	if (!((int)sv_player->v.flags & FL_ONGROUND))
		return;

	angle_radians = sv_player->v.angles[YAW] * (float)(6.283185307179586 / 360.0);
	sin_yaw = (float)sin(angle_radians);
	cos_yaw = (float)cos(angle_radians);

	for (i = 0; i < MAX_FORWARD; ++i)
	{
		top[0] = sv_player->v.origin[0] + cos_yaw * (float)(i + 3) * 12.0f;
		top[1] = sv_player->v.origin[1] + sin_yaw * (float)(i + 3) * 12.0f;
		top[2] = sv_player->v.origin[2] + sv_player->v.view_ofs[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160.0f;

		trace = SV_Move(top, vec3_origin, vec3_origin, bottom, MOVE_NOMONSTERS, sv_player);
		if (trace.allsolid)
			return;
		if (trace.fraction == 1.0f)
			return;

		z[i] = top[2] + trace.fraction * (bottom[2] - top[2]);
	}

	dir = 0;
	steps = 0;
	for (j = 1; j < i; ++j)
	{
		step = (int)(z[j] - z[j - 1]);
		if (step > -0.1f && step < 0.1f)
			continue;

		if (dir && ((step - dir) > 0.1f || (step - dir) < -0.1f))
			return;

		++steps;
		dir = step;
	}

	if (!dir)
	{
		sv_player->v.idealpitch = 0.0f;
		return;
	}

	if (steps < 2)
		return;

	sv_player->v.idealpitch = (float)(-dir) * sv_idealpitchscale.value;
}

extern edict_t		*g_pmove;
extern vec3_t		*g_velocity;
extern vec3_t		*g_origin;
extern int			g_onground;
extern float		g_forwardmove;
extern float		g_sidemove;
extern float		g_upmove;

extern void			PM_PreventMegaBunnyJumping(void);
extern int			PM_WalkMove(void);
extern void			PM_AirMove(void);
extern void PM_WaterMove(void);

extern float V_CalcRoll(vec3_t angles, vec3_t velocity);

static usercmd_t sv_client_cmd;

void SV_ClientThink(void)
{
	byte *pmove;
	int flags;
	float cmd_x;
	float cmd_y;

	g_pmove = sv_player;
	pmove = (byte *)g_pmove;

	if (*(float *)(pmove + 152) == 0.0f)
		return;

	g_velocity = (vec3_t *)(pmove + 184);
	g_origin = (vec3_t *)(pmove + 160);

	flags = (int)*(float *)(pmove + 500);
	g_onground = flags & FL_ONGROUND;

	PM_PreventMegaBunnyJumping();

	if (*(float *)(pmove + 384) <= 0.0f)
		return;

	memcpy(&sv_client_cmd, &host_client->cmd, sizeof(sv_client_cmd));
	g_forwardmove = sv_client_cmd.forwardmove;
	g_sidemove = sv_client_cmd.sidemove;
	g_upmove = sv_client_cmd.upmove;

	cmd_x = *(float *)(pmove + 472) + *(float *)(pmove + 232);
	cmd_y = *(float *)(pmove + 476) + *(float *)(pmove + 236);

	*(float *)(pmove + 204) = V_CalcRoll((float *)(pmove + 196), (float *)(pmove + 184)) * 4.0f;

	if (*(float *)(pmove + 468) == 0.0f)
	{
		float *p = (float *)(pmove + 196);
		p[1] = cmd_y;
		p[0] = cmd_x / -3.0f;
	}

	if (((int)*(float *)(pmove + 500) & 0x800) != 0)
	{
		PM_WalkMove();
		return;
	}

	if (*(float *)(pmove + 528) < 2.0f || *(float *)(pmove + 152) == 8.0f)
	{
		PM_AirMove();
		*(int *)(pmove + 268) = 0x3F800000;
		return;
	}

	PM_WaterMove();
}
