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
// in_win.c -- windows 95 mouse and joystick code

#include "quakedef.h"
#include "winquake.h"

#define MAX_PITCH	80.0f
#define MIN_PITCH	-70.0f

// mouse variables
int		mouseshowtoggle = 1;
int		mouseactivatetoggle;
int		mouseinitialized;
int		mouseparmsvalid;
int		restore_spi;
int		mouseactive;
int		originalmouseparms[3];
int		newmouseparms[3] = {0, 0, 1};
int		mouse_buttons;
int		mouse_oldbuttonstate;
int		console_visible;
POINT	current_pos;
int		mx_accum;
int		my_accum;
int		old_mouse_x;
int		old_mouse_y;
int		mouse_y;
int		mouse_x;

cvar_t	m_pitch = {"m_pitch", "0.022", true, false};
cvar_t	m_yaw = {"m_yaw", "0.022"};
cvar_t	m_forward = {"m_forward", "1"};
cvar_t	m_side = {"m_side", "0.8"};
cvar_t	sensitivity = {"sensitivity", "3", true, false};
cvar_t	lookspring = {"lookspring", "0", true, false};
cvar_t	lookstrafe = {"lookstrafe", "0", true, false};
cvar_t	freelook = {"freelook", "1", true, false};
cvar_t	m_filter = {"m_filter", "0"};

// joystick defines and variables
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define JOY_AXIS_MAP		0x0000000f		// low bits of joyadvaxis* pick the control
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

#define JOY_AXIS_CENTER		32768.0f		// raw axis values are 0..65535

#define JOY_MAX_BUTTONS		4				// buttons past these are K_AUX keys
#define JOY_POV_DIRS		4

enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn
};

int				in_joystick;
int				joy_avail;
JOYINFOEX		ji;
UINT			joy_id;
int				joy_numbuttons;
int				joy_haspov;
int				joy_oldbuttonstate;
int				joy_oldpovstate;
int				joy_advancedinit;
unsigned int	dwAxisMap[JOY_MAX_AXES];
unsigned int	dwControlMap[JOY_MAX_AXES];
unsigned int	*pdwRawValue[JOY_MAX_AXES];

cvar_t	joy_name = {"joyname", "joystick"};
cvar_t	joy_advanced = {"joyadvanced", "0"};
cvar_t	joy_advaxisx = {"joyadvaxisx", "0"};
cvar_t	joy_advaxisy = {"joyadvaxisy", "0"};
cvar_t	joy_advaxisz = {"joyadvaxisz", "0"};
cvar_t	joy_advaxisr = {"joyadvaxisr", "0"};
cvar_t	joy_advaxisu = {"joyadvaxisu", "0"};
cvar_t	joy_advaxisv = {"joyadvaxisv", "0"};

int dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

unsigned int	joy_flags;

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

// joystick turn rates, in degrees per second
float	joy_pitchspeed = 150.0f;
float	joy_yawspeed = 140.0f;

void IN_StartupJoystick(void);
void Joy_AdvancedUpdate_f(void);
void IN_JoyMove(usercmd_t *cmd);
void Force_CenterView_f(void);

/*
===========
IN_ShowMouse
===========
*/
int IN_ShowMouse(void)
{
	int		result;

	result = 0;
	if (!mouseshowtoggle)
	{
		result = ShowCursor(TRUE);
		mouseshowtoggle = 1;
	}
	return result;
}

/*
===========
IN_HideMouse
===========
*/
int IN_HideMouse(void)
{
	int		result;

	result = 0;
	if (mouseshowtoggle)
	{
		result = ShowCursor(FALSE);
		mouseshowtoggle = 0;
	}
	return result;
}

/*
===========
IN_ActivateMouse
===========
*/
BOOL IN_ActivateMouse(void)
{
	mouseactivatetoggle = 1;

	if (!mouseinitialized)
		return FALSE;

	if (mouseparmsvalid)
		restore_spi = SystemParametersInfoA(SPI_SETMOUSE, 0, newmouseparms, 0);

	SetCursorPos(window_center_x, window_center_y);
	mouseactive = 1;
	SetCapture(mainwindow);
	return ClipCursor(&window_rect);
}

/*
===========
IN_DeactivateMouse
===========
*/
BOOL IN_DeactivateMouse(void)
{
	mouseactivatetoggle = 0;

	if (!mouseinitialized)
		return FALSE;

	if (restore_spi)
		SystemParametersInfoA(SPI_SETMOUSE, 0, originalmouseparms, 0);

	mouseactive = 0;
	ClipCursor(NULL);
	return ReleaseCapture();
}

void IN_MouseActivate(void)
{
	IN_ActivateMouse();
}

void IN_MouseDeactivate(void)
{
	IN_DeactivateMouse();
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

/*
===========
IN_Accumulate
===========
*/
void IN_Accumulate(void)
{
	if (!cam_mousemove && mouseactive)
	{
		GetCursorPos(&current_pos);

		mx_accum += current_pos.x - window_center_x;
		my_accum += current_pos.y - window_center_y;

	// force the mouse to the center, so there's room to move
		SetCursorPos(window_center_x, window_center_y);
	}
}

void IN_ClearStates(void)
{
}

/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse(void)
{
	if (COM_CheckParm("nomouse"))
		return;

	mouseinitialized = 1;
	mouseparmsvalid = SystemParametersInfoA(SPI_GETMOUSE, 0, originalmouseparms, 0);

	if (mouseparmsvalid)
	{
		if (COM_CheckParm("noforcemspd"))
			newmouseparms[2] = originalmouseparms[2];

		if (COM_CheckParm("noforcemaccel"))
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
		}

		if (COM_CheckParm("noforcemparms"))
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
			newmouseparms[2] = originalmouseparms[2];
		}
	}

	mouse_buttons = 3;

// if a fullscreen video mode was set before the mouse was initialized,
// set the mouse state appropriately
	if (mouseactivatetoggle)
		IN_ActivateMouse();
}

/*
===========
IN_RegisterCvars
===========
*/
void IN_RegisterCvars(void)
{
	// mouse variables
	Cvar_RegisterVariable(&sensitivity);
	Cvar_RegisterVariable(&m_pitch);
	Cvar_RegisterVariable(&m_yaw);
	Cvar_RegisterVariable(&m_forward);
	Cvar_RegisterVariable(&m_side);
	Cvar_RegisterVariable(&lookspring);
	Cvar_RegisterVariable(&lookstrafe);
	Cvar_RegisterVariable(&freelook);
	Cvar_RegisterVariable(&m_filter);

	// joystick variables
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
	Cmd_AddCommand("joyadvancedupdate", Joy_AdvancedUpdate_f);

	IN_StartupMouse();
	IN_StartupJoystick();
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown(void)
{
	IN_DeactivateMouse();
	IN_ShowMouse();
}

/*
===========
IN_MouseEvent
===========
*/
int IN_MouseEvent(int mstate)
{
	int		i;

	if (!mouseactive)
		return 0;

// perform button actions
	for (i = 0; i < mouse_buttons; i++)
	{
		if ((mstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i)))
			Key_Event(K_MOUSE1 + i, true);

		if (!(mstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i)))
			Key_Event(K_MOUSE1 + i, false);
	}

	mouse_oldbuttonstate = mstate;
	return mstate;
}

/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove(usercmd_t *cmd)
{
	int		mx, my;
	int		filtered_x;

	if (cam_mousemove || key_dest == key_menu || cl_paused)
		return;

	GetCursorPos(&current_pos);
	mx = current_pos.x + mx_accum - window_center_x;
	my = current_pos.y + my_accum - window_center_y;
	mx_accum = 0;
	my_accum = 0;

	if (m_filter.value == 0.0f)
	{
		filtered_x = mx;
		mouse_y = my;
	}
	else
	{
		filtered_x = (int)((double)(mx + old_mouse_x) * 0.5);
		mouse_y = (int)((double)(my + old_mouse_y) * 0.5);
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x = (int)((float)filtered_x * sensitivity.value);
	mouse_y = (int)((float)mouse_y * sensitivity.value);

// add mouse X/Y movement to cmd
	if ((in_strafe.state & KB_DOWN) || (lookstrafe.value != 0.0f && (in_mlook.state & KB_DOWN)))
		cmd->sidemove += (float)mouse_x * m_side.value;
	else
		cl_viewangles[YAW] -= (float)mouse_x * m_yaw.value;

	if (in_mlook.state & KB_DOWN)
		V_StopPitchDrift();

	if ((in_mlook.state & KB_DOWN) && !(in_strafe.state & KB_DOWN))
	{
		cl_viewangles[PITCH] += (float)mouse_y * m_pitch.value;
		if (cl_viewangles[PITCH] > MAX_PITCH)
			cl_viewangles[PITCH] = MAX_PITCH;
		if (cl_viewangles[PITCH] < MIN_PITCH)
			cl_viewangles[PITCH] = MIN_PITCH;
	}
	else
	{
		if ((in_strafe.state & KB_DOWN) && freelook.value)
			cmd->upmove -= (float)mouse_y * m_forward.value;
		else
			cmd->forwardmove -= (float)mouse_y * m_forward.value;
	}

// if the mouse has moved, force it to the center, so there's room to move
	if (mx || my)
		SetCursorPos(window_center_x, window_center_y);
}

/*
===========
IN_Move
===========
*/
void IN_Move(usercmd_t *cmd)
{
	if (!cam_mousemove && mouseactive)
		IN_MouseMove(cmd);

	if (app_active_flag)
		IN_JoyMove(cmd);
}

void IN_ClearMouseAccum(void)
{
	if (!cam_mousemove && mouseactive)
	{
		GetCursorPos(&current_pos);

		mx_accum += current_pos.x - window_center_x;
		my_accum += current_pos.y - window_center_y;

		SetCursorPos(window_center_x, window_center_y);
	}
}

/*
===========
IN_ClearMouseState
===========
*/
int IN_ClearMouseState(void)
{
	if (mouseactive)
	{
		mx_accum = 0;
		my_accum = 0;
		mouse_oldbuttonstate = 0;
	}
	return 0;
}

/*
===============
IN_StartupJoystick
===============
*/
void IN_StartupJoystick(void)
{
	int			numdevs;
	JOYCAPS		jc;
	MMRESULT	mmr;

	// assume no joystick
	joy_avail = 0;

	// abort startup if user requests no joystick
	if (COM_CheckParm("nojoy"))
		return;

	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs()) == 0)
	{
		Con_DPrintf("joystick not found -- driver not present\n\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	mmr = JOYERR_NOERROR;
	for (joy_id = 0; (int)joy_id < numdevs; joy_id++)
	{
		memset(&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx(joy_id, &ji)) == JOYERR_NOERROR)
			break;
	}

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Con_DPrintf("\njoystick not found -- no valid joysticks (%x)\n\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset(&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCapsA(joy_id, &jc, sizeof(jc))) != JOYERR_NOERROR)
	{
		Con_DPrintf("\njoystick not found -- invalid joystick capabilities (%x)\n\n", mmr);
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization
	Con_Printf("joystick found\n\n");
	joy_advancedinit = 0;
	joy_avail = 1;
}

/*
===========
RawValuePointer
===========
*/
unsigned int *RawValuePointer(int axis)
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return (unsigned int *)&ji.dwXpos;
	case JOY_AXIS_Y:
		return (unsigned int *)&ji.dwYpos;
	case JOY_AXIS_Z:
		return (unsigned int *)&ji.dwZpos;
	case JOY_AXIS_R:
		return (unsigned int *)&ji.dwRpos;
	case JOY_AXIS_U:
		return (unsigned int *)&ji.dwUpos;
	case JOY_AXIS_V:
		return (unsigned int *)&ji.dwVpos;
	}
	return (unsigned int *)&ji.dwXpos;
}

/*
===========
Joy_AdvancedUpdate_f

Called once by IN_JoyMove and by the user whenever an update is needed.
===========
*/
void Joy_AdvancedUpdate_f(void)
{
	int				i;
	unsigned int	dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if (joy_advanced.value == 0.0f)
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
	}
	else
	{
		if (Q_strcmp(joy_name.string, "joystick"))
		{
			// notify user of advanced controller
			Con_Printf("\n%s configured\n\n", joy_name.string);
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (int)joy_advaxisx.value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & JOY_AXIS_MAP;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;

		dwTemp = (int)joy_advaxisy.value;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & JOY_AXIS_MAP;

		dwTemp = (int)joy_advaxisz.value;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & JOY_AXIS_MAP;

		dwTemp = (int)joy_advaxisr.value;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwAxisMap[JOY_AXIS_R] = dwTemp & JOY_AXIS_MAP;

		dwTemp = (int)joy_advaxisu.value;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwAxisMap[JOY_AXIS_U] = dwTemp & JOY_AXIS_MAP;

		dwTemp = (int)joy_advaxisv.value;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
		dwAxisMap[JOY_AXIS_V] = dwTemp & JOY_AXIS_MAP;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		if (dwAxisMap[i] != AxisNada)
			joy_flags |= dwAxisFlags[i];
	}
}

/*
===========
IN_Commands

Turns joystick button and POV changes into key events.
===========
*/
void IN_Commands(void)
{
	int				i, key_index;
	unsigned int	buttonstate, povstate;

	if (!joy_avail)
		return;

	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = ji.dwButtons;
	for (i = 0; i < joy_numbuttons; i++)
	{
		if ((buttonstate & (1 << i)) && !(joy_oldbuttonstate & (1 << i)))
		{
			key_index = (i < JOY_MAX_BUTTONS) ? K_JOY1 : K_AUX1;
			Key_Event(key_index + i, true);
		}

		if (!(buttonstate & (1 << i)) && (joy_oldbuttonstate & (1 << i)))
		{
			key_index = (i < JOY_MAX_BUTTONS) ? K_JOY1 : K_AUX1;
			Key_Event(key_index + i, false);
		}
	}
	joy_oldbuttonstate = buttonstate;

	if (joy_haspov)
	{
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
		if (ji.dwPOV != JOY_POVCENTERED)
		{
			povstate = (ji.dwPOV == JOY_POVFORWARD);
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}

		// determine which bits have changed and key an auxillary event for each change
		for (i = 0; i < JOY_POV_DIRS; i++)
		{
			if ((povstate & (1 << i)) && !(joy_oldpovstate & (1 << i)))
				Key_Event(K_AUX29 + i, true);

			if (!(povstate & (1 << i)) && (joy_oldpovstate & (1 << i)))
				Key_Event(K_AUX29 + i, false);
		}
		joy_oldpovstate = povstate;
	}
}

/*
===============
IN_ReadJoystick
===============
*/
qboolean IN_ReadJoystick(void)
{
	memset(&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx(joy_id, &ji) != JOYERR_NOERROR)
		return false;

	// this is a hack -- there is a bug in the Logitech WingMan Warrior DirectInput Driver
	// rather than having 32768 be the zero point, they have the zero point at 32668
	// go figure -- anyway, now we get the full resolution out of the device
	if (joy_wwhack1.value != 0.0f)
		ji.dwUpos += 100;

	return true;
}

/*
===========
IN_JoyMove
===========
*/
void IN_JoyMove(usercmd_t *cmd)
{
	int		i;
	double	fTemp;
	double	fPitch;
	double	fSign;
	float	speed, aspeed;
	float	fAxisValue;
	float	fAxisMove;

	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if (joy_advancedinit != 1)
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = 1;
	}

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || in_jlook.value == 0.0f)
		return;

	// collect the joystick data, if possible
	if (IN_ReadJoystick() != true)
		return;

	if (cl_intermission & 1)
		speed = scr_centertime.value;
	else
		speed = 1.0f;
	aspeed = host_frametime * speed;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float)*pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= JOY_AXIS_CENTER;

		if (joy_wwhack2.value != 0.0f && dwAxisMap[i] == AxisTurn)
		{
			// this is a special formula for the Logitech WingMan Warrior
			// y=ax^b; where a = 300 and b = 1.3
			// also x values are in increments of 800 (so this is factored out)
			// then bounds check result to level out excessively high spin rates
			fSign = fAxisValue;
			fAxisValue = pow((double)abs((int)fAxisValue) / 800.0, 1.3) * 300.0;
			if (fAxisValue > 14000.0f)
				fAxisValue = 14000.0f;
			// restore direction information
			if (fSign <= 0.0)
				fAxisValue = -fAxisValue;
		}

		// convert range from -32768..32767 to -1..1
		fAxisMove = fAxisValue / JOY_AXIS_CENTER;

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if (joy_advanced.value != 0.0f || !(in_mlook.state & KB_DOWN))
			{
				// user wants forward control to be forward control
				if (fabs(fAxisMove) > joy_forwardthreshold.value)
					cmd->forwardmove += fAxisMove * joy_forwardsensitivity.value * cl_forwardspeed.value * speed;
			}
			else if (fabs(fAxisMove) > joy_pitchthreshold.value)
			{
				// user wants forward control to become look control
				// if mouse invert is on, invert the joystick pitch value
				// only absolute control support here (joy_advanced is false)
				fTemp = joy_pitchsensitivity.value * fAxisMove * joy_pitchspeed * aspeed;
				if (m_pitch.value >= 0.0f)
					fPitch = fTemp + cl_viewangles[PITCH];
				else
					fPitch = cl_viewangles[PITCH] - fTemp;
				cl_viewangles[PITCH] = (float)fPitch;
				V_StopPitchDrift();
			}
			else if (lookspring.value == 0.0f)
			{
				// no pitch movement
				// disable pitch return-to-center unless requested by user
				V_StopPitchDrift();
			}
			break;

		case AxisLook:
			if (in_mlook.state & KB_DOWN)
			{
				if (fabs(fAxisMove) > joy_pitchthreshold.value)
				{
					// pitch movement detected and pitch movement desired by user
					fTemp = joy_pitchsensitivity.value * fAxisMove;
					if (dwControlMap[i] != JOY_ABSOLUTE_AXIS)
						cl_viewangles[PITCH] = (float)(fTemp * speed * 180.0f + cl_viewangles[PITCH]);
					else
						cl_viewangles[PITCH] = fTemp * joy_pitchspeed * aspeed + cl_viewangles[PITCH];
					V_StopPitchDrift();
				}
				else if (lookspring.value == 0.0f)
				{
					// no pitch movement
					// disable pitch return-to-center unless requested by user
					V_StopPitchDrift();
				}
			}
			break;

		case AxisSide:
			if (fabs(fAxisMove) > joy_sidethreshold.value)
				cmd->sidemove += joy_sidesensitivity.value * fAxisMove * cl_sidespeed.value * speed;
			break;

		case AxisTurn:
			if (!(in_strafe.state & KB_DOWN) && (lookstrafe.value == 0.0f || !(in_mlook.state & KB_DOWN)))
			{
				// user wants turn control to be turn control
				if (fabs(fAxisMove) > joy_yawthreshold.value)
				{
					fTemp = joy_yawsensitivity.value * fAxisMove;
					if (dwControlMap[i] != JOY_ABSOLUTE_AXIS)
						cl_viewangles[YAW] = (float)(fTemp * speed * 180.0f + cl_viewangles[YAW]);
					else
						cl_viewangles[YAW] = fTemp * joy_yawspeed * aspeed + cl_viewangles[YAW];
				}
			}
			else
			{
				// user wants turn control to become side control
				if (fabs(fAxisMove) > joy_sidethreshold.value)
					cmd->sidemove -= joy_sidesensitivity.value * fAxisMove * cl_sidespeed.value * speed;
			}
			break;
		}
	}

	// bounds check pitch
	if (cl_viewangles[PITCH] > MAX_PITCH)
		cl_viewangles[PITCH] = MAX_PITCH;
	if (cl_viewangles[PITCH] < MIN_PITCH)
		cl_viewangles[PITCH] = MIN_PITCH;
}

void Force_CenterView_f(void)
{
	cl_viewangles[PITCH] = 0;
}
