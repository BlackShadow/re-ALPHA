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
#include "winquake.h"

#define CAM_DIST_DELTA		1.0f
#define CAM_ANGLE_DELTA		2.5f
#define CAM_ANGLE_SPEED		2.5f
#define CAM_MIN_DIST		30.0f
#define CAM_ANGLE_MOVE		0.5f

cvar_t	cam_command = {"cam_command", "0"};
cvar_t	cam_snapto = {"cam_snapto", "0"};
cvar_t	cam_idealyaw = {"cam_idealyaw", "90"};
cvar_t	cam_idealpitch = {"cam_idealpitch", "0"};
cvar_t	cam_idealdist = {"cam_idealdist", "64"};
cvar_t	cam_contain = {"cam_contain", "0"};

cvar_t	c_maxpitch = {"c_maxpitch", "90"};
cvar_t	c_minpitch = {"c_minpitch", "0"};
cvar_t	c_maxyaw = {"c_maxyaw", "135"};
cvar_t	c_minyaw = {"c_minyaw", "-135"};
cvar_t	c_maxdistance = {"c_maxdistance", "200"};
cvar_t	c_mindistance = {"c_mindistance", "30"};

extern cvar_t	sensitivity;

vec3_t	cam_ofs = {0, 0, 0};
int		cam_thirdperson = 0;
int		cam_mousemove = 0;
int		cam_distancemove = 0;
int		cam_old_mouse_x = 0;
int		cam_old_mouse_y = 0;
POINT	cam_mouse = {0, 0};

kbutton_t	cam_pitchup;
kbutton_t	cam_pitchdown;
kbutton_t	cam_yawleft;
kbutton_t	cam_yawright;
kbutton_t	cam_in;
kbutton_t	cam_out;

extern int		window_center_x;
extern int		window_center_y;

extern void		KeyDown(kbutton_t *b);
extern void		KeyUp(kbutton_t *b);
extern float	CL_KeyState(kbutton_t *key);
extern void		AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);

void CAM_ToThirdPerson(void);
void CAM_ToFirstPerson(void);

float MoveToward(float cur, float goal, float maxspeed)
{
	if (cur != goal)
	{
		if ((float)abs((int)(cur - goal)) > 180.0f)
		{
			if (cur <= goal)
				cur = cur + 360.0f;
			else
				cur = cur - 360.0f;
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

	if (cur < 0.0f)
		return cur + 360.0f;
	else if (cur >= 360.0f)
		return cur - 360.0f;

	return cur;
}

void CAM_Think(void)
{
	vec3_t		camAngles;
	vec3_t		camForward, camRight, camUp;
	vec3_t		pnt;
	vec3_t		ext;
	float		dist;
	int			i;
	cl_entity_t	*ent;
	moveclip_t	clip;

	if ((int)cam_command.value == 1)
	{
		CAM_ToThirdPerson();
	}
	else if ((int)cam_command.value == 2)
	{
		CAM_ToFirstPerson();
	}

	if (!cam_thirdperson)
		return;

	if (cam_contain.value == 0.0f)
	{
		ent = NULL;
	}
	else
	{
		ext[0] = ext[1] = ext[2] = 0.0f;
		ent = &cl_entities[cl_viewentity];
	}

	camAngles[PITCH] = cam_idealpitch.value;
	camAngles[YAW] = cam_idealyaw.value;
	dist = cam_idealdist.value;

	if (!cam_mousemove || (GetCursorPos(&cam_mouse), cam_distancemove))
	{
		goto skip_mouse_yaw_pitch;
	}

	if (window_center_x >= cam_mouse.x)
	{
		if (window_center_x > cam_mouse.x)
		{
			if (c_minyaw.value < camAngles[YAW])
			{
				camAngles[YAW] = (float)((cam_mouse.x - window_center_x) / 2) * CAM_ANGLE_MOVE + camAngles[YAW];
			}
			if (c_minyaw.value > camAngles[YAW])
			{
				camAngles[YAW] = c_minyaw.value;
			}
		}
	}
	else
	{
		if (c_maxyaw.value > camAngles[YAW])
		{
			camAngles[YAW] = (float)((cam_mouse.x - window_center_x) / 2) * CAM_ANGLE_MOVE + camAngles[YAW];
		}
		if (c_maxyaw.value < camAngles[YAW])
		{
			camAngles[YAW] = c_maxyaw.value;
		}
	}

	if (window_center_y >= cam_mouse.y)
	{
		if (window_center_y > cam_mouse.y)
		{
			if (camAngles[PITCH] < c_minpitch.value)
			{
				camAngles[PITCH] = (float)((cam_mouse.y - window_center_y) / 2) * CAM_ANGLE_MOVE + camAngles[PITCH];
			}
			if (camAngles[PITCH] <= c_minpitch.value)
				goto done_mouse_pitch;
			camAngles[PITCH] = c_minpitch.value;
		}
	}
	else
	{
		if (camAngles[PITCH] > c_maxpitch.value)
		{
			camAngles[PITCH] = (float)((cam_mouse.y - window_center_y) / 2) * CAM_ANGLE_MOVE + camAngles[PITCH];
		}
		if (camAngles[PITCH] >= c_maxpitch.value)
			goto done_mouse_pitch;
		camAngles[PITCH] = c_maxpitch.value;
	}

	done_mouse_pitch:
	cam_old_mouse_x = (int)((float)cam_mouse.x * sensitivity.value);
	cam_old_mouse_y = (int)((float)cam_mouse.y * sensitivity.value);
	SetCursorPos(window_center_x, window_center_y);

	skip_mouse_yaw_pitch:
	if (CL_KeyState(&cam_pitchup) != 0.0f)
	{
		camAngles[PITCH] = camAngles[PITCH] + CAM_ANGLE_DELTA;
	}
	else if (CL_KeyState(&cam_pitchdown) != 0.0f)
	{
		camAngles[PITCH] = camAngles[PITCH] - CAM_ANGLE_DELTA;
	}

	if (CL_KeyState(&cam_yawleft) != 0.0f)
	{
		camAngles[YAW] = camAngles[YAW] - CAM_ANGLE_DELTA;
	}
	else if (CL_KeyState(&cam_yawright) != 0.0f)
	{
		camAngles[YAW] = camAngles[YAW] + CAM_ANGLE_DELTA;
	}

	if (CL_KeyState(&cam_in) != 0.0f)
	{
		dist = dist - CAM_DIST_DELTA;
		if (dist < CAM_MIN_DIST)
		{
			dist = CAM_MIN_DIST;
			camAngles[PITCH] = 0.0f;
			camAngles[YAW] = 0.0f;
		}
	}
	else if (CL_KeyState(&cam_out) != 0.0f)
	{
		dist = dist + CAM_DIST_DELTA;
	}

	if (!cam_distancemove)
		goto skip_distance_adjust;

	if (window_center_y >= cam_mouse.y)
	{
		if (window_center_y <= cam_mouse.y)
			goto done_distance_adjust;
		if (dist > c_mindistance.value)
			dist = (float)((cam_mouse.y - window_center_y) / 2) + dist;
		if (dist >= c_mindistance.value)
			goto done_distance_adjust;
		dist = c_mindistance.value;
	}
	else
	{
		if (c_maxdistance.value > dist)
			dist = (float)((cam_mouse.y - window_center_y) / 2) + dist;
		if (c_maxdistance.value >= dist)
			goto done_distance_adjust;
		dist = c_maxdistance.value;
	}

	done_distance_adjust:
	cam_old_mouse_x = (int)((float)cam_mouse.x * sensitivity.value);
	cam_old_mouse_y = (int)((float)cam_mouse.y * sensitivity.value);
	SetCursorPos(window_center_x, window_center_y);

	skip_distance_adjust:
	if (cam_contain.value != 0.0f)
	{
		VectorCopy(ent->origin, pnt);
		AngleVectors(camAngles, camForward, camRight, camUp);
		for (i = 0; i < 3; i++)
			pnt[i] = pnt[i] - camForward[i] * dist;

		memset(&clip, 0, sizeof(moveclip_t));
		clip.trace = SV_ClipMoveToEntity(sv.edicts, r_refdef.vieworg, ext, ext, pnt);

		if (clip.trace.fraction == 1.0f)
		{
			cam_idealpitch.value = camAngles[PITCH];
			cam_idealyaw.value = camAngles[YAW];
			cam_idealdist.value = dist;
		}
	}
	else
	{
		cam_idealpitch.value = camAngles[PITCH];
		cam_idealyaw.value = camAngles[YAW];
		cam_idealdist.value = dist;
	}

	camAngles[PITCH] = cam_ofs[PITCH];
	camAngles[YAW] = cam_ofs[YAW];
	camAngles[2] = cam_ofs[2];

	if (cam_snapto.value != 0.0f)
	{
		camAngles[YAW] = cl_viewangles[YAW] + cam_idealyaw.value;
		camAngles[PITCH] = cl_viewangles[PITCH] + cam_idealpitch.value;
		camAngles[2] = cam_idealdist.value;
	}
	else
	{
		if (camAngles[YAW] - cl_viewangles[YAW] != cam_idealyaw.value)
		{
			camAngles[YAW] = MoveToward(camAngles[YAW], cl_viewangles[YAW] + cam_idealyaw.value, CAM_ANGLE_SPEED);
		}
		if (camAngles[PITCH] - cl_viewangles[PITCH] != cam_idealpitch.value)
		{
			camAngles[PITCH] = MoveToward(camAngles[PITCH], cl_viewangles[PITCH] + cam_idealpitch.value, CAM_ANGLE_SPEED);
		}
		if ((float)abs((int)(camAngles[2] - cam_idealdist.value)) >= 2.0f)
		{
			camAngles[2] = camAngles[2] + (cam_idealdist.value - camAngles[2]) / 4.0f;
		}
		else
		{
			camAngles[2] = cam_idealdist.value;
		}
	}

	if (cam_contain.value != 0.0f)
	{
		dist = camAngles[ROLL];
		camAngles[ROLL] = 0.0f;

		VectorCopy(ent->origin, pnt);
		AngleVectors(camAngles, camForward, camRight, camUp);
		for (i = 0; i < 3; i++)
			pnt[i] = pnt[i] - camForward[i] * dist;

		memset(&clip, 0, sizeof(moveclip_t));
		ext[0] = ext[1] = ext[2] = 0.0f;
		clip.trace = SV_ClipMoveToEntity(sv.edicts, r_refdef.vieworg, ext, ext, pnt);

		if (clip.trace.fraction != 1.0f)
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

void CAM_ToThirdPerson(void)
{
	if (!cam_thirdperson)
	{
		cam_ofs[YAW] = cl_viewangles[YAW];
		cam_ofs[PITCH] = cl_viewangles[PITCH];
		cam_thirdperson = 1;
		cam_ofs[2] = CAM_MIN_DIST;
	}

	Cvar_SetValue("cam_command", 0.0f);
}

void CAM_ToFirstPerson(void)
{
	cam_thirdperson = 0;

	Cvar_SetValue("cam_command", 0.0f);
}

void CAM_ToggleSnapto(void)
{
	cam_snapto.value = (float)(cam_snapto.value == 0.0f);
}

void CAM_StartMouseMove(void)
{
	if (cam_thirdperson)
	{
		if (!cam_mousemove)
		{
			cam_mousemove = 1;
			GetCursorPos(&cam_mouse);
			cam_old_mouse_x = (int)((float)cam_mouse.x * sensitivity.value);
			cam_old_mouse_y = (int)((float)cam_mouse.y * sensitivity.value);
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

void CAM_StartDistance(void)
{
	if (cam_thirdperson)
	{
		if (!cam_distancemove)
		{
			cam_distancemove = 1;
			cam_mousemove = 1;
			GetCursorPos(&cam_mouse);
			cam_old_mouse_x = (int)((float)cam_mouse.x * sensitivity.value);
			cam_old_mouse_y = (int)((float)cam_mouse.y * sensitivity.value);
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
