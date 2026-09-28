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

// cl_input.c  -- builds an intended movement command to send to the server

#include "quakedef.h"

#define PLAYER_DUCKING_MULTIPLIER	0.333f	// movement speed while ducked

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

===============================================================================
*/

static kbutton_t in_attack2;
static kbutton_t in_lookdown;
static kbutton_t in_speed;
static kbutton_t in_attack;
static kbutton_t in_up;
static kbutton_t in_use;
static kbutton_t in_down;
static kbutton_t in_duck;
kbutton_t in_mlook;
static kbutton_t in_forward;
static kbutton_t in_left;
static kbutton_t in_right;
static kbutton_t in_moveleft;
static kbutton_t in_lookup;
static kbutton_t in_jump;
kbutton_t in_strafe;
static kbutton_t in_moveright;
static kbutton_t in_back;
static kbutton_t in_klook;

static int in_cancel;
static int in_impulse;

cvar_t cl_upspeed = { "cl_upspeed", "200" };
cvar_t cl_forwardspeed = { "cl_forwardspeed", "200" };
cvar_t cl_backspeed = { "cl_backspeed", "200" };
cvar_t cl_sidespeed = { "cl_sidespeed", "350" };
cvar_t cl_movespeedkey = { "cl_movespeedkey", "2.0" };
cvar_t cl_yawspeed = { "cl_yawspeed", "140" };
cvar_t cl_pitchspeed = { "cl_pitchspeed", "150" };
cvar_t cl_anglespeedkey = { "cl_anglespeedkey", "1.5" };

void KeyDown(kbutton_t *b)
{
	int			k;
	const char	*c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (b->down[0] == k || b->down[1] == k)
		return;		// repeating key

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		Con_Printf("Three keys down for a button!\n");
		return;
	}

	if (b->state & KB_DOWN)
		return;		// still down
	b->state |= KB_DOWN | KB_IMPULSEDOWN;
}

void KeyUp(kbutton_t *b)
{
	int			k;
	const char	*c;

	c = Cmd_Argv(1);
	if (c[0])
	{
		k = atoi(c);
	}
	else
	{	// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = 0;
		b->down[1] = 0;
		b->state = KB_IMPULSEUP;
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without corresponding down (menu pass through)

	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & KB_DOWN))
		return;		// still up (this should not happen)

	b->state &= ~KB_DOWN;		// now up
	b->state |= KB_IMPULSEUP;	// impulse up
}

void IN_AttackDown(void)
{
	KeyDown(&in_attack);
}

void IN_AttackUp(void)
{
	KeyUp(&in_attack);
}

void IN_Attack2Down(void)
{
	KeyDown(&in_attack2);
}

void IN_Attack2Up(void)
{
	KeyUp(&in_attack2);
}

void IN_UseDown(void)
{
	KeyDown(&in_use);
}

void IN_UseUp(void)
{
	KeyUp(&in_use);
}

void IN_JumpDown(void)
{
	KeyDown(&in_jump);
}

void IN_JumpUp(void)
{
	KeyUp(&in_jump);
}

void IN_DuckDown(void)
{
	KeyDown(&in_duck);
}

void IN_DuckUp(void)
{
	KeyUp(&in_duck);
}

void IN_ForwardDown(void)
{
	KeyDown(&in_forward);
}

void IN_ForwardUp(void)
{
	KeyUp(&in_forward);
}

void IN_BackDown(void)
{
	KeyDown(&in_back);
}

void IN_BackUp(void)
{
	KeyUp(&in_back);
}

void IN_MoveLeftDown(void)
{
	KeyDown(&in_moveleft);
}

void IN_MoveLeftUp(void)
{
	KeyUp(&in_moveleft);
}

void IN_MoveRightDown(void)
{
	KeyDown(&in_moveright);
}

void IN_MoveRightUp(void)
{
	KeyUp(&in_moveright);
}

void IN_LeftDown(void)
{
	KeyDown(&in_left);
}

void IN_LeftUp(void)
{
	KeyUp(&in_left);
}

void IN_RightDown(void)
{
	KeyDown(&in_right);
}

void IN_RightUp(void)
{
	KeyUp(&in_right);
}

void IN_LookupDown(void)
{
	KeyDown(&in_lookup);
}

void IN_LookupUp(void)
{
	KeyUp(&in_lookup);
}

void IN_LookdownDown(void)
{
	KeyDown(&in_lookdown);
}

void IN_LookdownUp(void)
{
	KeyUp(&in_lookdown);
}

void IN_UpDown(void)
{
	KeyDown(&in_up);
}

void IN_UpUp(void)
{
	KeyUp(&in_up);
}

void IN_DownDown(void)
{
	KeyDown(&in_down);
}

void IN_DownUp(void)
{
	KeyUp(&in_down);
}

void IN_SpeedDown(void)
{
	KeyDown(&in_speed);
}

void IN_SpeedUp(void)
{
	KeyUp(&in_speed);
}

void IN_StrafeDown(void)
{
	KeyDown(&in_strafe);
}

void IN_StrafeUp(void)
{
	KeyUp(&in_strafe);
}

void IN_MLookDown(void)
{
	KeyDown(&in_mlook);
}

void IN_MLookUp(void)
{
	KeyUp(&in_mlook);
	if (!(in_mlook.state & KB_DOWN))
		V_StartPitchDrift();
}

void IN_KLookDown(void)
{
	KeyDown(&in_klook);
}

void IN_KLookUp(void)
{
	KeyUp(&in_klook);
}

void IN_Cancel(void)
{
	in_cancel = 1;
}

void IN_Impulse(void)
{
	in_impulse = atoi(Cmd_Argv(1));
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState(kbutton_t *key)
{
	float	val;
	int		impulsedown, impulseup, down;

	impulsedown = key->state & KB_IMPULSEDOWN;
	impulseup = key->state & KB_IMPULSEUP;
	down = key->state & KB_DOWN;

	val = 0;

	if (impulsedown && !impulseup)
	{
		if (down)
			val = 0.5f;		// pressed and held this frame
		else
			val = 0;
	}
	else if (impulseup && !impulsedown)
	{
		if (down)
			val = 0;
		else
			val = 0;		// released this frame
	}
	else if (!impulsedown && !impulseup)
	{
		if (down)
			val = 1.0f;		// held the entire frame
		else
			val = 0;		// up the entire frame
	}
	else
	{
		if (down)
			val = 0.75f;	// released and re-pressed this frame
		else
			val = 0.25f;	// pressed and released this frame
	}

	key->state = down;		// clear impulses

	return val;
}

//==========================================================================

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles(void)
{
	float	speed;
	float	up, down;

	speed = host_frametime;
	if (in_speed.state & KB_DOWN)
		speed *= cl_anglespeedkey.value;

	if (!(in_strafe.state & KB_DOWN))
	{
		cl_viewangles[YAW] -= CL_KeyState(&in_right) * cl_yawspeed.value * speed;
		cl_viewangles[YAW] += CL_KeyState(&in_left) * cl_yawspeed.value * speed;
		cl_viewangles[YAW] = anglemod(cl_viewangles[YAW]);
	}

	if (in_klook.state & KB_DOWN)
	{
		V_StopPitchDrift();
		cl_viewangles[PITCH] -= CL_KeyState(&in_forward) * cl_pitchspeed.value * speed;
		cl_viewangles[PITCH] += CL_KeyState(&in_back) * cl_pitchspeed.value * speed;
	}

	up = CL_KeyState(&in_lookup);
	down = CL_KeyState(&in_lookdown);

	cl_viewangles[PITCH] -= cl_pitchspeed.value * speed * up;
	cl_viewangles[PITCH] += cl_pitchspeed.value * speed * down;

	if (up || down)
		V_StopPitchDrift();

	if (cl_viewangles[PITCH] > 80)
		cl_viewangles[PITCH] = 80;
	if (cl_viewangles[PITCH] < -70)
		cl_viewangles[PITCH] = -70;

	if (cl_viewangles[ROLL] > 50)
		cl_viewangles[ROLL] = 50;
	if (cl_viewangles[ROLL] < -50)
		cl_viewangles[ROLL] = -50;
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove(usercmd_t *cmd)
{
	if (cls.signon != SIGNONS)
		return;

	CL_AdjustAngles();

	memset(cmd, 0, sizeof(*cmd));

	if (in_strafe.state & KB_DOWN)
	{
		cmd->sidemove += cl_sidespeed.value * CL_KeyState(&in_right);
		cmd->sidemove -= cl_sidespeed.value * CL_KeyState(&in_left);
	}

	cmd->sidemove += cl_sidespeed.value * CL_KeyState(&in_moveright);
	cmd->sidemove -= cl_sidespeed.value * CL_KeyState(&in_moveleft);

	cmd->upmove += cl_upspeed.value * CL_KeyState(&in_up);
	cmd->upmove -= cl_upspeed.value * CL_KeyState(&in_down);

	if (!(in_klook.state & KB_DOWN))
	{
		cmd->forwardmove += cl_forwardspeed.value * CL_KeyState(&in_forward);
		cmd->forwardmove -= cl_backspeed.value * CL_KeyState(&in_back);
	}

//
// adjust for speed key
//
	if (in_speed.state & KB_DOWN)
	{
		cmd->forwardmove *= cl_movespeedkey.value;
		cmd->sidemove *= cl_movespeedkey.value;
		cmd->upmove *= cl_movespeedkey.value;
	}

	if (in_duck.state & KB_DOWN)
	{
		cmd->forwardmove *= PLAYER_DUCKING_MULTIPLIER;
		cmd->sidemove *= PLAYER_DUCKING_MULTIPLIER;
		cmd->upmove *= PLAYER_DUCKING_MULTIPLIER;
	}

	cmd->lightlevel = (byte)cl_lightlevel;
}

/*
==============
CL_SendMove
==============
*/
void CL_SendMove(usercmd_t *cmd)
{
	sizebuf_t			buf;
	byte				data[128];
	int					bits;
	static usercmd_t	lastcmd;
	static int			movemessages;

	memcpy(&lastcmd, cmd, sizeof(lastcmd));

	memset(&buf, 0, sizeof(buf));
	buf.data = data;
	buf.maxsize = sizeof(data);
	buf.cursize = 0;

//
// send the movement message
//
	MSG_WriteByte(&buf, clc_move);
	MSG_WriteFloat(&buf, cl_mtime[0]);	// so server can get ping times

	MSG_WriteAngle(&buf, cl_viewangles[0]);
	MSG_WriteAngle(&buf, cl_viewangles[1]);
	MSG_WriteAngle(&buf, cl_viewangles[2]);

	MSG_WriteShort(&buf, cmd->forwardmove);
	MSG_WriteShort(&buf, cmd->sidemove);
	MSG_WriteShort(&buf, cmd->upmove);

//
// send button bits
//
	bits = 0;

	if (in_attack.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_ATTACK;
	in_attack.state &= ~KB_IMPULSEDOWN;

	if (in_duck.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_DUCK;
	in_duck.state &= ~KB_IMPULSEDOWN;

	if (in_jump.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_JUMP;
	in_jump.state &= ~KB_IMPULSEDOWN;

	if (in_forward.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_FORWARD;
	in_forward.state &= ~KB_IMPULSEDOWN;

	if (in_back.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_BACK;
	in_back.state &= ~KB_IMPULSEDOWN;

	if (in_use.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_USE;
	in_use.state &= ~KB_IMPULSEDOWN;

	if (in_cancel)
		bits |= IN_CANCEL;

	if (in_left.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_LEFT;
	in_left.state &= ~KB_IMPULSEDOWN;

	if (in_right.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_RIGHT;
	in_right.state &= ~KB_IMPULSEDOWN;

	if (in_moveleft.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_MOVELEFT;
	in_moveleft.state &= ~KB_IMPULSEDOWN;

	if (in_moveright.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_MOVERIGHT;
	in_moveright.state &= ~KB_IMPULSEDOWN;

	if (in_attack2.state & (KB_DOWN | KB_IMPULSEDOWN))
		bits |= IN_ATTACK2;
	in_attack2.state &= ~KB_IMPULSEDOWN;

	MSG_WriteShort(&buf, bits);

	MSG_WriteByte(&buf, in_impulse);
	in_impulse = 0;

	MSG_WriteByte(&buf, cmd->lightlevel);

//
// deliver the message
//
	if (cls.demoplayback)
		return;

//
// always dump the first two messages, because they may contain leftover inputs
// from the last level
//
	if (++movemessages <= 2)
		return;

	if (NET_SendUnreliableMessage(cls.netcon, &buf) == -1)
	{
		Con_Printf("CL_SendMove: lost server connection\n");
		CL_Disconnect();
	}
}

/*
============
CL_InitInput
============
*/
void CL_InitInput(void)
{
	Cmd_AddCommand("+moveup", IN_UpDown);
	Cmd_AddCommand("-moveup", IN_UpUp);
	Cmd_AddCommand("+movedown", IN_DownDown);
	Cmd_AddCommand("-movedown", IN_DownUp);
	Cmd_AddCommand("+left", IN_LeftDown);
	Cmd_AddCommand("-left", IN_LeftUp);
	Cmd_AddCommand("+right", IN_RightDown);
	Cmd_AddCommand("-right", IN_RightUp);
	Cmd_AddCommand("+forward", IN_ForwardDown);
	Cmd_AddCommand("-forward", IN_ForwardUp);
	Cmd_AddCommand("+back", IN_BackDown);
	Cmd_AddCommand("-back", IN_BackUp);
	Cmd_AddCommand("+lookup", IN_LookupDown);
	Cmd_AddCommand("-lookup", IN_LookupUp);
	Cmd_AddCommand("+lookdown", IN_LookdownDown);
	Cmd_AddCommand("-lookdown", IN_LookdownUp);
	Cmd_AddCommand("+strafe", IN_StrafeDown);
	Cmd_AddCommand("-strafe", IN_StrafeUp);
	Cmd_AddCommand("+moveleft", IN_MoveLeftDown);
	Cmd_AddCommand("-moveleft", IN_MoveLeftUp);
	Cmd_AddCommand("+moveright", IN_MoveRightDown);
	Cmd_AddCommand("-moveright", IN_MoveRightUp);
	Cmd_AddCommand("+speed", IN_SpeedDown);
	Cmd_AddCommand("-speed", IN_SpeedUp);
	Cmd_AddCommand("+attack", IN_AttackDown);
	Cmd_AddCommand("-attack", IN_AttackUp);
	Cmd_AddCommand("+attack2", IN_Attack2Down);
	Cmd_AddCommand("-attack2", IN_Attack2Up);
	Cmd_AddCommand("+use", IN_UseDown);
	Cmd_AddCommand("-use", IN_UseUp);
	Cmd_AddCommand("+jump", IN_JumpDown);
	Cmd_AddCommand("-jump", IN_JumpUp);
	Cmd_AddCommand("impulse", IN_Impulse);
	Cmd_AddCommand("+klook", IN_KLookDown);
	Cmd_AddCommand("-klook", IN_KLookUp);
	Cmd_AddCommand("+mlook", IN_MLookDown);
	Cmd_AddCommand("-mlook", IN_MLookUp);
	Cmd_AddCommand("+duck", IN_DuckDown);
	Cmd_AddCommand("-duck", IN_DuckUp);

	Cmd_AddCommand("+campitchup", CAM_PitchUpDown);
	Cmd_AddCommand("-campitchup", CAM_PitchUpUp);
	Cmd_AddCommand("+campitchdown", CAM_PitchDownDown);
	Cmd_AddCommand("-campitchdown", CAM_PitchDownUp);
	Cmd_AddCommand("+camyawleft", CAM_YawLeftDown);
	Cmd_AddCommand("-camyawleft", CAM_YawLeftUp);
	Cmd_AddCommand("+camyawright", CAM_YawRightDown);
	Cmd_AddCommand("-camyawright", CAM_YawRightUp);
	Cmd_AddCommand("+camin", CAM_InDown);
	Cmd_AddCommand("-camin", CAM_InUp);
	Cmd_AddCommand("+camout", CAM_OutDown);
	Cmd_AddCommand("-camout", CAM_OutUp);
	Cmd_AddCommand("thirdperson", CAM_ToThirdPerson);
	Cmd_AddCommand("firstperson", CAM_ToFirstPerson);
	Cmd_AddCommand("+cammousemove", CAM_StartMouseMove);
	Cmd_AddCommand("-cammousemove", CAM_EndMouseMove);
	Cmd_AddCommand("+camdistance", CAM_StartDistance);
	Cmd_AddCommand("-camdistance", CAM_EndDistance);
	Cmd_AddCommand("snapto", CAM_ToggleSnapto);

	Cvar_RegisterVariable(&cam_command);
	Cvar_RegisterVariable(&cam_snapto);
	Cvar_RegisterVariable(&cam_idealyaw);
	Cvar_RegisterVariable(&cam_idealpitch);
	Cvar_RegisterVariable(&cam_idealdist);
	Cvar_RegisterVariable(&cam_contain);
	Cvar_RegisterVariable(&c_maxpitch);
	Cvar_RegisterVariable(&c_minpitch);
	Cvar_RegisterVariable(&c_maxyaw);
	Cvar_RegisterVariable(&c_minyaw);
	Cvar_RegisterVariable(&c_maxdistance);
	Cvar_RegisterVariable(&c_mindistance);
}

int CL_CheckConnectionState(void)
{
	return (cls.state == ca_dedicated) ? -1 : 0;
}
