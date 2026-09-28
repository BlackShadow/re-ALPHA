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
// cam.c -- third person camera

#include "quakedef.h"
#include "winquake.h"

#define CAM_DIST_DELTA		1.0f
#define CAM_ANGLE_DELTA		2.5f
#define CAM_ANGLE_SPEED		2.5f
#define CAM_MIN_DIST		30.0f
#define CAM_ANGLE_MOVE		0.5f

cvar_t	cam_command = { "cam_command", "0" };	// 1 = go third person, 2 = go first person
cvar_t	cam_snapto = { "cam_snapto", "0" };
cvar_t	cam_idealyaw = { "cam_idealyaw", "90" };
cvar_t	cam_idealpitch = { "cam_idealpitch", "0" };
cvar_t	cam_idealdist = { "cam_idealdist", "64" };
cvar_t	cam_contain = { "cam_contain", "0" };

cvar_t	c_maxpitch = { "c_maxpitch", "90" };
cvar_t	c_minpitch = { "c_minpitch", "0" };
cvar_t	c_maxyaw = { "c_maxyaw", "135" };
cvar_t	c_minyaw = { "c_minyaw", "-135" };
cvar_t	c_maxdistance = { "c_maxdistance", "200" };
cvar_t	c_mindistance = { "c_mindistance", "30" };

vec3_t	cam_ofs = { 0, 0, 0 };		// pitch, yaw, distance
int		cam_thirdperson = 0;
int		cam_mousemove = 0;
int		cam_distancemove = 0;
int		cam_old_mouse_x = 0;
int		cam_old_mouse_y = 0;
POINT	cam_mouse = { 0, 0 };

kbutton_t	cam_pitchup;
kbutton_t	cam_pitchdown;
kbutton_t	cam_yawleft;
kbutton_t	cam_yawright;
kbutton_t	cam_in;
kbutton_t	cam_out;

/*
==============
MoveToward

Moves an angle a quarter of the way to goal, taking the shorter way around
==============
*/
float MoveToward(float cur, float goal, float maxspeed)
{
	if (cur != goal)
	{
		if (abs((int)(cur - goal)) > 180.0f)
		{
			if (cur <= goal)
				cur += 360.0f;
			else
				cur -= 360.0f;
		}

		if (cur < goal)
		{
			if (cur < goal - 1.0f)
				cur = (goal - cur) / 4.0f + cur;
			else
				cur = goal;
		}
		else
		{
			if (cur > goal + 1.0f)
				cur = cur - (cur - goal) / 4.0f;
			else
				cur = goal;
		}
	}

	// bring back into range
	if (cur < 0)
		return cur + 360.0f;
	else if (cur >= 360.0f)
		return cur - 360.0f;

	return cur;
}

/*
==============
CAM_Think
==============
*/
void CAM_Think(void)
{
	vec3_t		camAngles;
	vec3_t		camForward, camRight, camUp;
	vec3_t		pnt;
	vec3_t		ext;
	float		dist;
	int			i;
	cl_entity_t	*ent;
	trace_t		trace;

	if ((int)cam_command.value == 1)
		CAM_ToThirdPerson();
	else if ((int)cam_command.value == 2)
		CAM_ToFirstPerson();

	if (!cam_thirdperson)
		return;

	if (cam_contain.value == 0)
	{
		ent = NULL;
	}
	else
	{
		ext[0] = ext[1] = ext[2] = 0;
		ent = &cl_entities[cl_viewentity];
	}

	camAngles[PITCH] = cam_idealpitch.value;
	camAngles[YAW] = cam_idealyaw.value;
	dist = cam_idealdist.value;

	//
	// movement of the camera with the mouse
	//
	if (cam_mousemove)
	{
		// get the current mouse position
		GetCursorPos(&cam_mouse);

		if (!cam_distancemove)
		{
			// check for X delta values and adjust accordingly
			if (window_center_x < cam_mouse.x)
			{
				if (c_maxyaw.value > camAngles[YAW])
					camAngles[YAW] = (cam_mouse.x - window_center_x) / 2 * CAM_ANGLE_MOVE + camAngles[YAW];
				if (c_maxyaw.value < camAngles[YAW])
					camAngles[YAW] = c_maxyaw.value;
			}
			else if (window_center_x > cam_mouse.x)
			{
				if (c_minyaw.value < camAngles[YAW])
					camAngles[YAW] = (cam_mouse.x - window_center_x) / 2 * CAM_ANGLE_MOVE + camAngles[YAW];
				if (c_minyaw.value > camAngles[YAW])
					camAngles[YAW] = c_minyaw.value;
			}

			// check for Y delta values and adjust accordingly
			if (window_center_y < cam_mouse.y)
			{
				if (camAngles[PITCH] > c_maxpitch.value)
					camAngles[PITCH] = (cam_mouse.y - window_center_y) / 2 * CAM_ANGLE_MOVE + camAngles[PITCH];
				if (camAngles[PITCH] < c_maxpitch.value)
					camAngles[PITCH] = c_maxpitch.value;
			}
			else if (window_center_y > cam_mouse.y)
			{
				if (camAngles[PITCH] < c_minpitch.value)
					camAngles[PITCH] = (cam_mouse.y - window_center_y) / 2 * CAM_ANGLE_MOVE + camAngles[PITCH];
				if (camAngles[PITCH] > c_minpitch.value)
					camAngles[PITCH] = c_minpitch.value;
			}

			// set old mouse coordinates to current mouse coordinates
			// since we are done with the mouse
			cam_old_mouse_x = cam_mouse.x * sensitivity.value;
			cam_old_mouse_y = cam_mouse.y * sensitivity.value;
			SetCursorPos(window_center_x, window_center_y);
		}
	}

	//
	// orbit the camera with the keys
	//
	if (CL_KeyState(&cam_pitchup))
		camAngles[PITCH] += CAM_ANGLE_DELTA;
	else if (CL_KeyState(&cam_pitchdown))
		camAngles[PITCH] -= CAM_ANGLE_DELTA;

	if (CL_KeyState(&cam_yawleft))
		camAngles[YAW] -= CAM_ANGLE_DELTA;
	else if (CL_KeyState(&cam_yawright))
		camAngles[YAW] += CAM_ANGLE_DELTA;

	if (CL_KeyState(&cam_in))
	{
		dist -= CAM_DIST_DELTA;
		if (dist < CAM_MIN_DIST)
		{
			// if we go back into first person, reset the angle
			dist = CAM_MIN_DIST;
			camAngles[PITCH] = 0;
			camAngles[YAW] = 0;
		}
	}
	else if (CL_KeyState(&cam_out))
	{
		dist += CAM_DIST_DELTA;
	}

	//
	// zoom the camera with the mouse
	//
	if (cam_distancemove)
	{
		if (window_center_y < cam_mouse.y)
		{
			if (c_maxdistance.value > dist)
				dist = (cam_mouse.y - window_center_y) / 2 + dist;
			if (c_maxdistance.value < dist)
				dist = c_maxdistance.value;
		}
		else if (window_center_y > cam_mouse.y)
		{
			if (dist > c_mindistance.value)
				dist = (cam_mouse.y - window_center_y) / 2 + dist;
			if (dist < c_mindistance.value)
				dist = c_mindistance.value;
		}

		// set old mouse coordinates to current mouse coordinates
		// since we are done with the mouse
		cam_old_mouse_x = cam_mouse.x * sensitivity.value;
		cam_old_mouse_y = cam_mouse.y * sensitivity.value;
		SetCursorPos(window_center_x, window_center_y);
	}

	if (cam_contain.value)
	{
		// check new ideal
		VectorCopy(ent->origin, pnt);
		AngleVectors(camAngles, camForward, camRight, camUp);
		for (i = 0; i < 3; i++)
			pnt[i] = pnt[i] - camForward[i] * dist;

		// check line of sight
		trace = SV_ClipMoveToEntity(sv.edicts, r_refdef.vieworg, ext, ext, pnt);
		if (trace.fraction == 1.0f)
		{
			// update ideal
			cam_idealpitch.value = camAngles[PITCH];
			cam_idealyaw.value = camAngles[YAW];
			cam_idealdist.value = dist;
		}
	}
	else
	{
		// update ideal
		cam_idealpitch.value = camAngles[PITCH];
		cam_idealyaw.value = camAngles[YAW];
		cam_idealdist.value = dist;
	}

	// move towards ideal
	camAngles[PITCH] = cam_ofs[PITCH];
	camAngles[YAW] = cam_ofs[YAW];
	camAngles[ROLL] = cam_ofs[2];

	if (cam_snapto.value)
	{
		camAngles[YAW] = cl_viewangles[YAW] + cam_idealyaw.value;
		camAngles[PITCH] = cl_viewangles[PITCH] + cam_idealpitch.value;
		camAngles[ROLL] = cam_idealdist.value;
	}
	else
	{
		if (camAngles[YAW] - cl_viewangles[YAW] != cam_idealyaw.value)
			camAngles[YAW] = MoveToward(camAngles[YAW], cl_viewangles[YAW] + cam_idealyaw.value, CAM_ANGLE_SPEED);

		if (camAngles[PITCH] - cl_viewangles[PITCH] != cam_idealpitch.value)
			camAngles[PITCH] = MoveToward(camAngles[PITCH], cl_viewangles[PITCH] + cam_idealpitch.value, CAM_ANGLE_SPEED);

		if (abs((int)(camAngles[ROLL] - cam_idealdist.value)) >= 2.0f)
			camAngles[ROLL] = camAngles[ROLL] + (cam_idealdist.value - camAngles[ROLL]) / 4.0f;
		else
			camAngles[ROLL] = cam_idealdist.value;
	}

	if (cam_contain.value)
	{
		// check new position
		dist = camAngles[ROLL];
		camAngles[ROLL] = 0;

		VectorCopy(ent->origin, pnt);
		AngleVectors(camAngles, camForward, camRight, camUp);
		for (i = 0; i < 3; i++)
			pnt[i] = pnt[i] - camForward[i] * dist;

		// check line of sight
		ext[0] = ext[1] = ext[2] = 0;
		trace = SV_ClipMoveToEntity(sv.edicts, r_refdef.vieworg, ext, ext, pnt);
		if (trace.fraction != 1.0f)
			return;
	}

	cam_ofs[0] = camAngles[0];
	cam_ofs[1] = camAngles[1];
	cam_ofs[2] = dist;
}

void CAM_PitchUpDown(void)
{
	KeyDown(&cam_pitchup);
}

void CAM_PitchUpUp(void)
{
	KeyUp(&cam_pitchup);
}

void CAM_PitchDownDown(void)
{
	KeyDown(&cam_pitchdown);
}

void CAM_PitchDownUp(void)
{
	KeyUp(&cam_pitchdown);
}

void CAM_YawLeftDown(void)
{
	KeyDown(&cam_yawleft);
}

void CAM_YawLeftUp(void)
{
	KeyUp(&cam_yawleft);
}

void CAM_YawRightDown(void)
{
	KeyDown(&cam_yawright);
}

void CAM_YawRightUp(void)
{
	KeyUp(&cam_yawright);
}

void CAM_InDown(void)
{
	KeyDown(&cam_in);
}

void CAM_InUp(void)
{
	KeyUp(&cam_in);
}

void CAM_OutDown(void)
{
	KeyDown(&cam_out);
}

void CAM_OutUp(void)
{
	KeyUp(&cam_out);
}

/*
==============
CAM_ToThirdPerson
==============
*/
void CAM_ToThirdPerson(void)
{
	if (!cam_thirdperson)
	{
		cam_ofs[YAW] = cl_viewangles[YAW];
		cam_ofs[PITCH] = cl_viewangles[PITCH];
		cam_thirdperson = 1;
		cam_ofs[2] = CAM_MIN_DIST;
	}

	Cvar_SetValue("cam_command", 0);
}

void CAM_ToFirstPerson(void)
{
	cam_thirdperson = 0;

	Cvar_SetValue("cam_command", 0);
}

void CAM_ToggleSnapto(void)
{
	cam_snapto.value = !cam_snapto.value;
}

/*
==============
CAM_StartMouseMove
==============
*/
void CAM_StartMouseMove(void)
{
	if (cam_thirdperson)
	{
		if (!cam_mousemove)
		{
			cam_mousemove = 1;
			GetCursorPos(&cam_mouse);
			cam_old_mouse_x = cam_mouse.x * sensitivity.value;
			cam_old_mouse_y = cam_mouse.y * sensitivity.value;
		}
	}
	else
	{
		cam_mousemove = 0;
	}
}

void CAM_EndMouseMove(void)
{
	cam_mousemove = 0;
}

/*
==============
CAM_StartDistance

the same as CAM_StartMouseMove, but the mouse zooms the camera
==============
*/
void CAM_StartDistance(void)
{
	if (cam_thirdperson)
	{
		if (!cam_distancemove)
		{
			cam_distancemove = 1;
			cam_mousemove = 1;
			GetCursorPos(&cam_mouse);
			cam_old_mouse_x = cam_mouse.x * sensitivity.value;
			cam_old_mouse_y = cam_mouse.y * sensitivity.value;
		}
	}
	else
	{
		cam_distancemove = 0;
		cam_mousemove = 0;
	}
}

void CAM_EndDistance(void)
{
	cam_distancemove = 0;
	cam_mousemove = 0;
}
