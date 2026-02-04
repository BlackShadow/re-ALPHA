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

int mouseshowtoggle = 1;
int mouseactivatetoggle;
int mouseinitialized;
int mouseparmsvalid;
int restore_spi;
int mouseactive;
extern HWND mainwindow;
int originalmouseparms[3];
int newmouseparms[3] = { 0, 0, 1 };
int mouse_buttons;
int mouse_oldbuttonstate;
int console_visible;
POINT current_pos;
int mx_accum;
int my_accum;
int old_mouse_x;
int old_mouse_y;
int mouse_y;
int mouse_x;

cvar_t	m_pitch = {"m_pitch", "0.022", true, false};
cvar_t	m_yaw = {"m_yaw", "0.022"};
cvar_t	m_forward = {"m_forward", "1"};
cvar_t	m_side = {"m_side", "0.8"};
cvar_t	sensitivity = {"sensitivity", "3", true, false};
cvar_t	lookspring = {"lookspring", "0", true, false};
cvar_t	lookstrafe = {"lookstrafe", "0", true, false};
cvar_t	freelook = {"freelook", "1", true, false};
cvar_t  m_filter = {"m_filter", "0"};
int in_joystick;
int joy_avail;
JOYINFOEX ji;
UINT joy_id;
int joy_numbuttons;
int joy_haspov;
int joy_oldbuttonstate;
int joy_oldpovstate;
int joy_advancedinit;
unsigned int dwAxisMap[6];
unsigned int dwControlMap[6];
unsigned int *pdwRawValue[6];

cvar_t	joy_name = {"joyname", "joystick"};
cvar_t	joy_advanced = {"joyadvanced", "0"};
cvar_t	joy_advaxisx = {"joyadvaxisx", "0"};
cvar_t	joy_advaxisy = {"joyadvaxisy", "0"};
cvar_t	joy_advaxisz = {"joyadvaxisz", "0"};
cvar_t	joy_advaxisr = {"joyadvaxisr", "0"};
cvar_t	joy_advaxisu = {"joyadvaxisu", "0"};
cvar_t	joy_advaxisv = {"joyadvaxisv", "0"};
int dwAxisFlags[6] = {
	JOY_RETURNX,
	JOY_RETURNY,
	JOY_RETURNZ,
	JOY_RETURNR,
	JOY_RETURNU,
	JOY_RETURNV
};
unsigned int joy_flags;

cvar_t	in_jlook = {"in_jlook", "0", true, false};
cvar_t	joy_wwhack1 = {"joywwhack1", "0"};
cvar_t	joy_wwhack2 = {"joywwhack2", "0"};
cvar_t	joy_forwardthreshold = {"joyforwardthreshold", "0.15"};
cvar_t	joy_forwardsensitivity = {"joyforwardsensitivity", "-1.0"};
cvar_t	joy_pitchthreshold = {"joypitchthreshold", "0.15"};
cvar_t	joy_pitchsensitivity = {"joypitchsensitivity", "1.0"};
cvar_t	joy_sidethreshold = {"joysidethreshold", "0.15"};
cvar_t	joy_sidesensitivity = {"joysidesensitivity", "-1.0"};
cvar_t	joy_yawthreshold = {"joyyawthreshold", "0.15"};
cvar_t	joy_yawsensitivity = {"joyyawsensitivity", "-1.0"};

extern double		host_frametime;
extern cvar_t		scr_centertime;
extern cvar_t		cl_forwardspeed;
extern cvar_t		cl_sidespeed;

float	cl_pitchspeed_var = 150.0f;
float	cl_yawspeed_var = 140.0f;
extern int			window_center_x;
extern int			window_center_y;
extern RECT			window_rect;
extern kbutton_t	in_mlook;
extern kbutton_t	in_strafe;
extern int			cl_intermission;
extern int			cam_mousemove;
extern int			app_active_flag;

extern void			V_StopPitchDrift(void);

int IN_ShowMouse(void);
int IN_HideMouse(void);
BOOL IN_ActivateMouse(void);
BOOL IN_DeactivateMouse(void);
int IN_MouseInit(void);
int IN_RegisterCvars(void);
int IN_Shutdown(void);
int IN_MouseEvent(int buttons);
void IN_MouseMove(usercmd_t *cmd);
void IN_Move(usercmd_t *cmd);
BOOL IN_ClearMouseAccum(void);
int IN_ClearMouseState(void);
int IN_StartupJoystick(void);
unsigned int *IN_GetJoyAxisValuePointer(int axis);
int Joy_AdvancedUpdate_f(void);
void IN_JoyMove(void);
int IN_ReadJoystickState(void);
void IN_JoyInput(usercmd_t *cmd);
void Force_CenterView_f(void);
void Joy_AdvancedUpdate_f_Wrapper(void);

int IN_ShowMouse(void)
{
	int result;

	result = 0;
	if (!mouseshowtoggle)
	{
		result = ShowCursor(1);
		mouseshowtoggle = 1;
	}
	return result;
}

int IN_HideMouse(void)
{
	int result;

	result = 0;
	if (mouseshowtoggle)
	{
		result = ShowCursor(0);
		mouseshowtoggle = 0;
	}
	return result;
}

BOOL IN_ActivateMouse(void)
{
	BOOL result;

	mouseactivatetoggle = 1;
	result = FALSE;
	if (mouseinitialized)
	{
		if (mouseparmsvalid)
			restore_spi = SystemParametersInfoA(SPI_SETMOUSE, 0, newmouseparms, 0);
		SetCursorPos(window_center_x, window_center_y);
		mouseactive = 1;
		SetCapture(mainwindow);
		return ClipCursor(&window_rect);
	}
	return result;
}

BOOL IN_DeactivateMouse(void)
{
	BOOL result;

	result = FALSE;
	mouseactivatetoggle = 0;
	if (mouseinitialized)
	{
		if (restore_spi)
			SystemParametersInfoA(SPI_SETMOUSE, 0, originalmouseparms, 0);
		mouseactive = 0;
		ClipCursor(0);
		return ReleaseCapture();
	}
	return result;
}

void IN_MouseActivate(void)
{
	(void)IN_ActivateMouse();
}

void IN_MouseDeactivate(void)
{
	(void)IN_DeactivateMouse();
}

void IN_MouseCenter(void)
{
	SetCursorPos(window_center_x, window_center_y);
}

void IN_MouseRestore(void)
{
	if (mouseinitialized)
	{
		if (restore_spi)
			SystemParametersInfoA(SPI_SETMOUSE, 0, originalmouseparms, 0);
		mouseactive = 0;
		ClipCursor(NULL);
		ReleaseCapture();
	}
}

BOOL IN_Accumulate(void)
{
	if (!cam_mousemove && mouseactive)
	{
		GetCursorPos(&current_pos);
		mx_accum += current_pos.x - window_center_x;
		my_accum += current_pos.y - window_center_y;
		return SetCursorPos(window_center_x, window_center_y);
	}

	return FALSE;
}

//=========================================================
// IN_ClearStates - empty input stub
//=========================================================
void IN_ClearStates(void)
{
}

int IN_MouseInit(void)
{
	int result;

	result = COM_CheckParm("nomouse");
	if (!result)
	{
		mouseinitialized = 1;
		result = SystemParametersInfoA(SPI_GETMOUSE, 0, originalmouseparms, 0);
		mouseparmsvalid = result;
		if (result)
		{
			if (COM_CheckParm("noforcemspd"))
				newmouseparms[2] = originalmouseparms[2];
			if (COM_CheckParm("noforcemaccel"))
			{
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
			}
			result = COM_CheckParm("noforcemparms");
			if (result)
			{
				result = originalmouseparms[0];
				newmouseparms[0] = originalmouseparms[0];
				newmouseparms[1] = originalmouseparms[1];
				newmouseparms[2] = originalmouseparms[2];
			}
		}
		mouse_buttons = 3;
		if (mouseactivatetoggle)
			return IN_ActivateMouse();
	}
	return result;
}

int IN_RegisterCvars(void)
{
	Cvar_RegisterVariable(&sensitivity);
	Cvar_RegisterVariable(&m_pitch);
	Cvar_RegisterVariable(&m_yaw);
	Cvar_RegisterVariable(&m_forward);
	Cvar_RegisterVariable(&m_side);
	Cvar_RegisterVariable(&lookspring);
	Cvar_RegisterVariable(&lookstrafe);
	Cvar_RegisterVariable(&freelook);
	Cvar_RegisterVariable(&m_filter);
	Cvar_RegisterVariable(&joy_name);
	Cvar_RegisterVariable(&joy_advanced);
	Cvar_RegisterVariable(&joy_advaxisx);
	Cvar_RegisterVariable(&joy_advaxisy);
	Cvar_RegisterVariable(&joy_advaxisz);
	Cvar_RegisterVariable(&joy_advaxisr);
	Cvar_RegisterVariable(&joy_advaxisu);
	Cvar_RegisterVariable(&joy_advaxisv);
	Cvar_RegisterVariable(&in_jlook);
	Cvar_RegisterVariable(&joy_wwhack1);
	Cvar_RegisterVariable(&joy_wwhack2);
	Cvar_RegisterVariable(&joy_forwardthreshold);
	Cvar_RegisterVariable(&joy_forwardsensitivity);
	Cvar_RegisterVariable(&joy_pitchthreshold);
	Cvar_RegisterVariable(&joy_pitchsensitivity);
	Cvar_RegisterVariable(&joy_sidethreshold);
	Cvar_RegisterVariable(&joy_sidesensitivity);
	Cvar_RegisterVariable(&joy_yawthreshold);
	Cvar_RegisterVariable(&joy_yawsensitivity);
	Cmd_AddCommand("force_centerview", Force_CenterView_f);
	Cmd_AddCommand("joyadvancedupdate", Joy_AdvancedUpdate_f_Wrapper);
	IN_MouseInit();
	return IN_StartupJoystick();
}

int IN_Shutdown(void)
{
	IN_DeactivateMouse();
	return IN_ShowMouse();
}

int IN_MouseEvent(int buttons)
{
	int i;
	int mask;

	i = 0;
	if (mouseactive)
	{
		if (mouse_buttons > 0)
		{
			do
			{
				mask = 1 << i;
				if ((buttons & (1 << i)) != 0 && (mask & mouse_oldbuttonstate) == 0)
					Key_Event(i + 200, 1);
				if ((buttons & (1 << i)) == 0 && (mask & mouse_oldbuttonstate) != 0)
					Key_Event(i + 200, 0);
				++i;
			}
			while (i < mouse_buttons);
		}
		mouse_oldbuttonstate = buttons;
		return buttons;
	}
	return 0;
}

void IN_MouseMove(usercmd_t *cmd)
{
	int mouseX;
	int mouseY;
	int deltaX;

	if (cam_mousemove || key_dest == key_menu || cl_paused)
		return;

	GetCursorPos(&current_pos);
	mouseX = current_pos.x + mx_accum - window_center_x;
	mouseY = current_pos.y + my_accum - window_center_y;
	mx_accum = 0;
	my_accum = 0;

	if (m_filter.value == 0.0f)
	{
		deltaX = mouseX;
		mouse_y = mouseY;
	}
	else
	{
		deltaX = (int)((double)(mouseX + old_mouse_x) * 0.5);
		mouse_y = (int)((double)(mouseY + old_mouse_y) * 0.5);
	}
	old_mouse_x = mouseX;
	old_mouse_y = mouseY;

	mouse_x = (int)((float)deltaX * sensitivity.value);
	mouse_y = (int)((float)mouse_y * sensitivity.value);

	if ((in_strafe.state & 1) != 0 || (lookstrafe.value != 0.0f && (in_mlook.state & 1) != 0))
	{
		cmd->sidemove += (float)mouse_x * m_side.value;
	}
	else
	{
		cl_viewangles[YAW] -= (float)mouse_x * m_yaw.value;
	}

	if ((in_mlook.state & 1) != 0 && (V_StopPitchDrift(), (in_mlook.state & 1) != 0))
	{
		if ((in_strafe.state & 1) == 0)
		{
			cl_viewangles[PITCH] += (float)mouse_y * m_pitch.value;
			if (cl_viewangles[PITCH] > 80.0f)
				cl_viewangles[PITCH] = 80.0f;
			if (cl_viewangles[PITCH] < -70.0f)
				cl_viewangles[PITCH] = -70.0f;
			goto done;
		}
	}
	else if ((in_strafe.state & 1) == 0)
	{
		cmd->forwardmove -= (float)mouse_y * m_forward.value;
		goto done;
	}

	if (!freelook.value)
	{
		cmd->forwardmove -= (float)mouse_y * m_forward.value;
	}
	else
	{
		cmd->upmove -= (float)mouse_y * m_forward.value;
	}

done:
	if (mouseX || mouseY)
		SetCursorPos(window_center_x, window_center_y);
}

void IN_Move(usercmd_t *cmd)
{
	if (!cam_mousemove && mouseactive)
		IN_MouseMove(cmd);
	if (app_active_flag)
		IN_JoyInput(cmd);
}

BOOL IN_ClearMouseAccum(void)
{
	BOOL result;

	result = FALSE;
	if (!cam_mousemove)
	{
		if (mouseactive)
		{
			GetCursorPos(&current_pos);
			mx_accum += current_pos.x - window_center_x;
			my_accum += current_pos.y - window_center_y;
			return SetCursorPos(window_center_x, window_center_y);
		}
	}
	return result;
}

int IN_ClearMouseState(void)
{
	int result;

	result = 0;
	if (mouseactive)
	{
		mx_accum = 0;
		my_accum = 0;
		mouse_oldbuttonstate = 0;
	}
	return result;
}

int IN_StartupJoystick(void)
{
	signed int numDevs;
	MMRESULT pos;
	MMRESULT devCaps;
	JOYCAPS jc = { 0 };

	joy_avail = 0;
	if (!COM_CheckParm("nojoy"))
	{
		numDevs = joyGetNumDevs();
		if (!numDevs)
		{
			Con_DPrintf("joystick not found -- driver not present\n\n");
			return 0;
		}

		joy_id = 0;
		if (numDevs <= 0)
		{
			pos = jc.wPid;
			if (pos)
			{
				Con_DPrintf("\njoystick not found -- no valid joysticks (%x)\n\n", pos);
				return 0;
			}
		}
		else
		{
			while (1)
			{
				memset(&ji, 0, sizeof(ji));
				ji.dwSize = 52;
				ji.dwFlags = 1024;
				pos = joyGetPosEx(joy_id, &ji);
				if (!pos)
					break;
				if (numDevs <= (int)++joy_id)
				{
					if (pos)
					{
						Con_DPrintf("\njoystick not found -- no valid joysticks (%x)\n\n", pos);
						return 0;
					}
					break;
				}
			}
		}

		memset(&jc, 0, sizeof(jc));
		devCaps = joyGetDevCapsA(joy_id, &jc, sizeof(jc));
		if (devCaps)
		{
			Con_DPrintf("\njoystick not found -- invalid joystick capabilities (%x)\n\n", devCaps);
		}
		else
		{
			joy_numbuttons = jc.wNumButtons;
			joy_oldpovstate = 0;
			joy_haspov = jc.wCaps & 0x10;
			joy_oldbuttonstate = 0;
			Con_Printf("joystick found\n\n", 0);
			joy_advancedinit = 0;
			joy_avail = 1;
		}
	}

	return 0;
}

unsigned int *IN_GetJoyAxisValuePointer(int axis)
{
	switch (axis)
	{
	case 0:
		return (unsigned int *)&ji.dwXpos;
	case 1:
		return (unsigned int *)&ji.dwYpos;
	case 2:
		return (unsigned int *)&ji.dwZpos;
	case 3:
		return (unsigned int *)&ji.dwRpos;
	case 4:
		return (unsigned int *)&ji.dwUpos;
	case 5:
		return (unsigned int *)&ji.dwVpos;
	}
	return (unsigned int *)&ji.dwXpos;
}

int Joy_AdvancedUpdate_f(void)
{
	int axis;
	int i;
	int mapping;
	int result;
	int j;

	axis = 0;
	for (i = 0; i < 6; ++i)
	{
		dwAxisMap[i] = 0;
		dwControlMap[i] = 0;
		pdwRawValue[i] = IN_GetJoyAxisValuePointer(axis);
		++axis;
	}

	if (joy_advanced.value == 0.0f)
	{
		dwAxisMap[0] = 4;
		dwAxisMap[1] = 1;
	}
	else
	{
		if (Q_strcmp(joy_name.string, "joystick"))
			Con_Printf("\n%s configured\n\n", joy_name.string);

		mapping = (int)joy_advaxisx.value;
		dwAxisMap[0] = mapping & 0xF;
		dwControlMap[0] = mapping & 0x10;

		mapping = (int)joy_advaxisy.value;
		dwControlMap[1] = mapping & 0x10;
		dwAxisMap[1] = mapping & 0xF;

		mapping = (int)joy_advaxisz.value;
		dwControlMap[2] = mapping & 0x10;
		dwAxisMap[2] = mapping & 0xF;

		mapping = (int)joy_advaxisr.value;
		dwControlMap[3] = mapping & 0x10;
		dwAxisMap[3] = mapping & 0xF;

		mapping = (int)joy_advaxisu.value;
		dwControlMap[4] = mapping & 0x10;
		dwAxisMap[4] = mapping & 0xF;

		mapping = (int)joy_advaxisv.value;
		dwControlMap[5] = mapping & 0x10;
		dwAxisMap[5] = mapping & 0xF;
	}

	result = 1216;
	for (j = 0; j < 6; ++j)
	{
		if (dwAxisMap[j])
			result |= dwAxisFlags[j];
		joy_flags = result;
	}
	return result;
}

void IN_JoyMove(void)
{
	unsigned int buttons;
	int i;
	int mask;
	int keyIndex;
	int keyIndex2;
	int pov;
	int j;
	int povMask;

	if (joy_avail)
	{
		buttons = ji.dwButtons;
		for (i = 0; i < joy_numbuttons; ++i)
		{
			mask = 1 << i;
			if (((1 << i) & buttons) != 0 && (mask & joy_oldbuttonstate) == 0)
			{
				keyIndex = 207;
				if (i < 4)
					keyIndex = 203;
				Key_Event(i + keyIndex, 1);
			}
			if (((1 << i) & buttons) == 0 && (mask & joy_oldbuttonstate) != 0)
			{
				keyIndex2 = 207;
				if (i < 4)
					keyIndex2 = 203;
				Key_Event(i + keyIndex2, 0);
			}
		}

		pov = 0;
		joy_oldbuttonstate = buttons;
		if (joy_haspov)
		{
			if (ji.dwPOV != 0xFFFF)
			{
				pov = ji.dwPOV == 0;
				if (ji.dwPOV == 9000)
					pov |= 2u;
				if (ji.dwPOV == 18000)
					pov |= 4u;
				if (ji.dwPOV == 27000)
					pov |= 8u;
			}
			for (j = 0; j < 4; ++j)
			{
				povMask = 1 << j;
				if (((1 << j) & pov) != 0 && (povMask & joy_oldpovstate) == 0)
					Key_Event(j + 235, 1);
				if (((1 << j) & pov) == 0 && (povMask & joy_oldpovstate) != 0)
					Key_Event(j + 235, 0);
			}
			joy_oldpovstate = pov;
		}
	}
}

int IN_ReadJoystickState(void)
{
	memset(&ji, 0, sizeof(ji));
	ji.dwSize = 52;
	ji.dwFlags = joy_flags;
	if (joyGetPosEx(joy_id, &ji))
		return 0;
	if (joy_wwhack1.value != 0.0f)
		ji.dwUpos += 100;
	return 1;
}

void IN_JoyInput(usercmd_t *cmd)
{
	int axisIndex;
	int axisAction;
	double absValue;
	double adjustedValue;
	double pitchAdjust;
	double pitchAdjust2;
	double yawAdjust;
	float speed;
	float frameSpeed;
	float frameSpeedCopy;
	float axisValue;
	float centered;
	float centered2;
	float centered3;
	float centered6;

	if (joy_advancedinit != 1)
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = 1;
	}

	if (joy_avail && in_jlook.value != 0.0f && IN_ReadJoystickState() == 1)
	{
		if ((cl_intermission & 1) != 0)
			speed = scr_centertime.value;
		else
			speed = 1.0f;

		axisIndex = 0;
		frameSpeed = host_frametime * speed;
		frameSpeedCopy = frameSpeed;

		while (1)
		{
			axisValue = (float)*(unsigned int *)pdwRawValue[axisIndex];
			centered = axisValue - 32768.0f;

			if (joy_wwhack2.value != 0.0f && dwAxisMap[axisIndex] == 4)
			{
				absValue = centered;
				centered = pow((double)abs((int)centered) / 800.0, 1.3) * 300.0;
				if (centered > 14000.0f)
					centered = 14000.0f;
				if (absValue <= 0.0)
					centered = -centered;
			}

			centered2 = centered / 32768.0f;
			axisAction = dwAxisMap[axisIndex];

			switch (axisAction)
			{
			case 1:
				if (joy_advanced.value != 0.0f || (in_mlook.state & 1) == 0)
				{
					if (fabs(centered2) > joy_forwardthreshold.value)
						cmd->forwardmove += centered2 * joy_forwardsensitivity.value * cl_forwardspeed.value * speed;
					break;
				}
				if (fabs(centered2) > joy_pitchthreshold.value)
				{
					pitchAdjust = joy_pitchsensitivity.value * centered2 * cl_pitchspeed_var * frameSpeedCopy;
					if (m_pitch.value >= 0.0f)
						adjustedValue = pitchAdjust + cl_viewangles[PITCH];
					else
						adjustedValue = cl_viewangles[PITCH] - pitchAdjust;
					cl_viewangles[PITCH] = (float)adjustedValue;
					V_StopPitchDrift();
					break;
				}
				if (lookspring.value == 0.0f)
					goto clamp_pitch;
				break;

			case 2:
				if ((in_mlook.state & 1) != 0)
				{
					if (fabs(centered2) > joy_pitchthreshold.value)
					{
						pitchAdjust2 = joy_pitchsensitivity.value * centered2;
						if (dwControlMap[axisIndex])
						{
							centered3 = pitchAdjust2 * speed * 180.0f + cl_viewangles[PITCH];
							cl_viewangles[PITCH] = centered3;
						}
						else
						{
							cl_viewangles[PITCH] = pitchAdjust2 * cl_pitchspeed_var * frameSpeedCopy + cl_viewangles[PITCH];
						}
clamp_pitch:
						V_StopPitchDrift();
						break;
					}
					if (lookspring.value == 0.0f)
						goto clamp_pitch;
				}
				break;

			case 3:
				if (fabs(centered2) > joy_sidethreshold.value)
					cmd->sidemove += joy_sidesensitivity.value * centered2 * cl_sidespeed.value * speed;
				break;

			case 4:
				if ((in_strafe.state & 1) == 0 && (lookstrafe.value == 0.0f || (in_mlook.state & 1) == 0))
				{
					if (fabs(centered2) > joy_yawthreshold.value)
					{
						yawAdjust = joy_yawsensitivity.value * centered2;
						if (dwControlMap[axisIndex])
						{
							centered6 = yawAdjust * speed * 180.0f + cl_viewangles[YAW];
							cl_viewangles[YAW] = centered6;
						}
						else
						{
							cl_viewangles[YAW] = yawAdjust * cl_yawspeed_var * frameSpeedCopy + cl_viewangles[YAW];
						}
					}
					break;
				}
				if (fabs(centered2) > joy_sidethreshold.value)
					cmd->sidemove -= joy_sidesensitivity.value * centered2 * cl_sidespeed.value * speed;
				break;
			}

			if (++axisIndex >= 6)
			{
				if (cl_viewangles[PITCH] > 80.0f)
					cl_viewangles[PITCH] = 80.0f;
				if (cl_viewangles[PITCH] < -70.0f)
					cl_viewangles[PITCH] = -70.0f;
				return;
			}
		}
	}
}

void Force_CenterView_f(void)
{
	cl_viewangles[PITCH] = 0;
}

void Joy_AdvancedUpdate_f_Wrapper(void)
{
	Joy_AdvancedUpdate_f();
}
