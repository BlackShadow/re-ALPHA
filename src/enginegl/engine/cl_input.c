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
#include <stdlib.h>
#include <string.h>
#include "quakedef.h"

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

extern float cl_viewangles[3];

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
    int k;
    const char *c;

    c = Cmd_Argv(1);
    if (c[0])
        k = atoi(c);
    else
        k = -1;

    if (b->down[0] == k || b->down[1] == k)
        return;

    if (!b->down[0])
        b->down[0] = k;
    else if (!b->down[1])
        b->down[1] = k;
    else
    {
        Con_Printf("Three keys down for a button!\n");
        return;
    }

    if (b->state & 1)
        return;
    b->state |= 1 + 2;
}

void KeyUp(kbutton_t *b)
{
    int k;
    const char *c;

    c = Cmd_Argv(1);
    if (c[0])
        k = atoi(c);
    else
    {
        b->down[0] = 0;
        b->down[1] = 0;
        b->state = 4;
        return;
    }

    if (b->down[0] == k)
        b->down[0] = 0;
    else if (b->down[1] == k)
        b->down[1] = 0;
    else
        return;

    if (b->down[0] || b->down[1])
        return;

    if (!(b->state & 1))
        return;

    b->state &= ~1;
    b->state |= 4;
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
    if (!(in_mlook.state & 1))
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

float CL_KeyState(kbutton_t *key)
{
    float val;
    int impulsedown;
    int impulseup;
    int down;

    impulsedown = key->state & 2;
    impulseup = key->state & 4;
    down = key->state & 1;

    val = 0.0f;

    if (impulsedown && !impulseup)
    {
        if (down)
            val = 0.5f;
        else
            val = 0.0f;
    }
    else if (impulseup && !impulsedown)
    {
        if (down)
            val = 0.0f;
        else
            val = 0.0f;
    }
    else if (!impulsedown && !impulseup)
    {
        if (down)
            val = 1.0f;
        else
            val = 0.0f;
    }
    else
    {

        if (down)
            val = 0.75f;
        else
            val = 0.25f;
    }

    key->state = down;

    return val;
}

void CL_AdjustAngles(void)
{
    float speed;
    float up;
    float down;

    speed = (float)host_frametime;
    if (in_speed.state & 1)
        speed *= cl_anglespeedkey.value;

    if (!(in_strafe.state & 1))
    {
        cl_viewangles[1] -= CL_KeyState(&in_right) * cl_yawspeed.value * speed;
        cl_viewangles[1] += CL_KeyState(&in_left) * cl_yawspeed.value * speed;
        cl_viewangles[1] = anglemod(cl_viewangles[1]);
    }

    if (in_klook.state & 1)
    {
        V_StopPitchDrift();
        cl_viewangles[0] -= CL_KeyState(&in_forward) * cl_pitchspeed.value * speed;
        cl_viewangles[0] += CL_KeyState(&in_back) * cl_pitchspeed.value * speed;
    }

    up = CL_KeyState(&in_lookup);
    down = CL_KeyState(&in_lookdown);

    cl_viewangles[0] -= cl_pitchspeed.value * speed * up;
    cl_viewangles[0] += cl_pitchspeed.value * speed * down;

    if (up != 0.0f || down != 0.0f)
        V_StopPitchDrift();

    if (cl_viewangles[0] > 80.0f)
        cl_viewangles[0] = 80.0f;
    if (cl_viewangles[0] < -70.0f)
        cl_viewangles[0] = -70.0f;

    if (cl_viewangles[2] > 50.0f)
        cl_viewangles[2] = 50.0f;
    if (cl_viewangles[2] < -50.0f)
        cl_viewangles[2] = -50.0f;
}

void CL_BaseMove(usercmd_t *cmd)
{
	if (cls.signon != 4)
		return;

	CL_AdjustAngles();
	memset(cmd, 0, sizeof(*cmd));

	if (in_strafe.state & 1)
	{
		cmd->sidemove += cl_sidespeed.value * CL_KeyState(&in_right);
		cmd->sidemove -= cl_sidespeed.value * CL_KeyState(&in_left);
	}

	cmd->sidemove += cl_sidespeed.value * CL_KeyState(&in_moveright);
	cmd->sidemove -= cl_sidespeed.value * CL_KeyState(&in_moveleft);

	cmd->upmove += cl_upspeed.value * CL_KeyState(&in_up);
	cmd->upmove -= cl_upspeed.value * CL_KeyState(&in_down);

	if (!(in_klook.state & 1))
	{
		cmd->forwardmove += cl_forwardspeed.value * CL_KeyState(&in_forward);
		cmd->forwardmove -= cl_backspeed.value * CL_KeyState(&in_back);
	}

	if (in_speed.state & 1)
	{
		cmd->forwardmove *= cl_movespeedkey.value;
		cmd->sidemove *= cl_movespeedkey.value;
		cmd->upmove *= cl_movespeedkey.value;
	}

	if (in_duck.state & 1)
	{
		cmd->forwardmove *= 0.333f;
		cmd->sidemove *= 0.333f;
		cmd->upmove *= 0.333f;
	}

	cmd->lightlevel = (byte)cl_lightlevel;
}

int CL_SendMove(usercmd_t *cmd)
{
	sizebuf_t msg;
	byte msgdata[128];
	int buttons;
	int result;
	static usercmd_t lastcmd;
	static int movemessages;

	result = 0;
	memcpy(&lastcmd, cmd, sizeof(lastcmd));

	memset(&msg, 0, sizeof(msg));
	msg.data = msgdata;
	msg.maxsize = sizeof(msgdata);
	msg.cursize = 0;

	MSG_WriteByte(&msg, 3);
	MSG_WriteFloat(&msg, (float)cl_mtime[0]);

	MSG_WriteAngle(&msg, cl_viewangles[0]);
	MSG_WriteAngle(&msg, cl_viewangles[1]);
	MSG_WriteAngle(&msg, cl_viewangles[2]);

	MSG_WriteShort(&msg, (int)cmd->forwardmove);
	MSG_WriteShort(&msg, (int)cmd->sidemove);
	MSG_WriteShort(&msg, (int)cmd->upmove);

	buttons = 0;
	if ((in_attack.state & 3) != 0)
		buttons |= 0x0001;
	in_attack.state &= ~2;
	if ((in_duck.state & 3) != 0)
		buttons |= 0x0004;
	in_duck.state &= ~2;
	if ((in_jump.state & 3) != 0)
		buttons |= 0x0002;
	in_jump.state &= ~2;
	if ((in_forward.state & 3) != 0)
		buttons |= 0x0008;
	in_forward.state &= ~2;
	if ((in_back.state & 3) != 0)
		buttons |= 0x0010;
	in_back.state &= ~2;
	if ((in_use.state & 3) != 0)
		buttons |= 0x0020;
	in_use.state &= ~2;
	if (in_cancel)
		buttons |= 0x0040;
	if ((in_left.state & 3) != 0)
		buttons |= 0x0080;
	in_left.state &= ~2;
	if ((in_right.state & 3) != 0)
		buttons |= 0x0100;
	in_right.state &= ~2;
	if ((in_moveleft.state & 3) != 0)
		buttons |= 0x0200;
	in_moveleft.state &= ~2;
	if ((in_moveright.state & 3) != 0)
		buttons |= 0x0400;
	in_moveright.state &= ~2;
	if ((in_attack2.state & 3) != 0)
		buttons |= 0x0800;
	in_attack2.state &= ~2;

	MSG_WriteShort(&msg, buttons);

	MSG_WriteByte(&msg, in_impulse);
	in_impulse = 0;

	MSG_WriteByte(&msg, cmd->lightlevel);

	if (!cls.demoplayback && ++movemessages > 2)
	{
		result = NET_SendUnreliableMessage(cls.netcon, &msg);
		if (result == -1)
		{
			Con_Printf("CL_SendMove: lost server connection\n");
			return CL_Disconnect();
		}
	}

	return result;
}

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
	return -(cls.state == 0);
}
