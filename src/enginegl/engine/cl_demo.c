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

// cl_demo.c

#include "quakedef.h"

void CL_FinishTimeDemo(void);

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback(void)
{
	if (!cls.demoplayback)
		return;

	fclose(cls.demofile);
	cls.demoplayback = false;
	cls.demofile = NULL;
	cls.state = ca_disconnected;

	if (cls.timedemo)
		CL_FinishTimeDemo();
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage(void)
{
	int		len;
	int		i;
	float	f;

	len = LittleLong(net_message.cursize);
	fwrite(&len, sizeof(len), 1, cls.demofile);

	for (i = 0; i < 3; i++)
	{
		f = LittleFloat(cl_viewangles[i]);
		fwrite(&f, sizeof(f), 1, cls.demofile);
	}

	fwrite(net_message.data, net_message.cursize, 1, cls.demofile);
	fflush(cls.demofile);
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage(void)
{
	int		r, i;
	float	f;

	if (cls.demoplayback)
	{
	// decide if it is time to grab the next message
		if (cls.signon == SIGNONS)	// always grab until fully connected
		{
			if (cls.timedemo)
			{
				if (host_framecount == cls.td_lastframe)
					return 0;		// already read this frame's message
				cls.td_lastframe = host_framecount;

			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
				if (host_framecount - cls.td_startframe == 1)
					cls.td_starttime = realtime;
			}
			else if (cl_time <= cl_mtime[0])
			{
				return 0;		// don't need another message yet
			}
		}

	// get the next message
		if (fread(&net_message.cursize, sizeof(net_message.cursize), 1, cls.demofile) != 1)
		{
			CL_StopPlayback();
			return 0;
		}

		net_message.cursize = LittleLong(net_message.cursize);
		if (net_message.cursize > MAX_MSGLEN)
			Sys_Error("Demo message > MAX_MSGLEN");

		VectorCopy(cl.mviewangles[0], cl.mviewangles[1]);

		for (i = 0; i < 3; i++)
		{
			if (fread(&f, sizeof(f), 1, cls.demofile) != 1)
			{
				CL_StopPlayback();
				return 0;
			}
			cl.mviewangles[0][i] = LittleFloat(f);
		}

		if (fread(net_message.data, net_message.cursize, 1, cls.demofile) == 1)
		{
			return 1;
		}
		else
		{
			CL_StopPlayback();
			return 0;
		}
	}
	else
	{
		while (1)
		{
			r = NET_GetMessage(cls.netcon);
			if (r != 1 && r != 2)
				break;

		// discard nop keepalive message
			if (net_message.cursize != 1 || net_message.data[0] != svc_nop)
			{
				if (cls.demorecording)
					CL_WriteDemoMessage();
				return r;
			}
			Con_Printf("<-- server to client keepalive\n");
		}
		return r;
	}
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f(void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf("Not recording a demo.\n");
		return;
	}

// write a disconnect message to the demo file
	SZ_Clear(&net_message);
	MSG_WriteByte(&net_message, svc_disconnect);
	CL_WriteDemoMessage();

// finish up
	fclose(cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Printf("Completed demo\n");
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void CL_Record_f(void)
{
	int			c;
	const char	*demoname;
	char		name[MAX_OSPATH];
	int			track;

	if (cmd_source != src_command)
		return;

	c = Cmd_Argc();
	if (c != 2 && c != 3 && c != 4)
	{
		Con_Printf("record <demoname> [<map> [cd track]]\n");
		return;
	}

	demoname = Cmd_Argv(1);
	if (strstr(demoname, ".."))
	{
		Con_Printf("Relative pathnames are not allowed.\n");
		return;
	}

	if (c == 2 && cls.state == ca_connected)
	{
		Con_Printf("Can not record - already connected to server\n"
				   "Client demo recording must be started before connecting\n");
		return;
	}

// get the track
	if (c == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf("Forcing CD track to %i\n", track);
	}
	else
	{
		track = -1;
	}

	sprintf(name, "%s/%s", com_gamedir, demoname);

//
// start the map up
//
	if (c > 2)
		Cmd_ExecuteString(va("map %s", Cmd_Argv(2)), src_command);

//
// open the demo file
//
	COM_DefaultExtension(name, ".dem");

	Con_Printf("recording to %s.\n", name);
	cls.demofile = fopen(name, "wb");
	if (!cls.demofile)
	{
		Con_Printf("ERROR: couldn't open.\n");
		return;
	}

	cls.forcetrack = track;
	fprintf(cls.demofile, "%i\n", track);
	cls.demorecording = true;
}

/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDemo_f(void)
{
	char	name[MAX_OSPATH];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("play <demoname> : plays a demo\n");
		return;
	}

//
// disconnect from server
//
	CL_Disconnect();

//
// open the demo file
//
	strcpy(name, Cmd_Argv(1));
	COM_DefaultExtension(name, ".dem");

	Con_Printf("Playing demo from %s.\n", name);
	COM_FOpenFile(name, &cls.demofile);
	if (!cls.demofile)
	{
		Con_Printf("ERROR: couldn't open.\n");
		cls.demonum = -1;
		return;
	}

	cls.demoplayback = true;
	cls.state = ca_connected;
	fscanf(cls.demofile, "%i\n", &cls.forcetrack);

	cl_time = -99999;	// get a new message this frame
	cl_oldtime = -99999;
	cl_mtime[0] = 0;
	cl_mtime[1] = 0;

	key_dest = key_game;
	Con_ClearNotify();
}

/*
====================
CL_FinishTimeDemo
====================
*/
void CL_FinishTimeDemo(void)
{
	float	time;
	float	frames;

	time = realtime - cls.td_starttime;
	cls.timedemo = false;
	if (time == 0)
		time = 1;

// the first frame didn't count
	frames = host_framecount - cls.td_startframe - 1;
	Con_Printf("%i frames %5.1f seconds %5.1f fps\n", (int)frames, time, frames / time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f(void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("timedemo <demoname> : gets demo speeds\n");
		return;
	}

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted

	CL_PlayDemo_f();

	cls.timedemo = true;
	cls.td_lastframe = -1;		// get a new message this frame
	cls.td_startframe = host_framecount;
}

/*
====================
CL_StartMovie_f

startmovie <filename>
====================
*/
void CL_StartMovie_f(void)
{
	const char	*filename;

	if (Cmd_Argc() == 2)
	{
		cl.movie_recording = true;
		filename = Cmd_Argv(1);
		VID_WriteBuffer(filename);
		Con_Printf("Started recording movie...\n");
	}
	else
	{
		Con_Printf("startmovie <filename>\n");
	}
}

/*
====================
CL_StopDemoAndEnableCom1
====================
*/
int CL_StopDemoAndEnableCom1(void)
{
	Cmd_ExecuteString("stopdemo", src_command);

	if (cls.state <= ca_connected)
	{
		Cmd_ExecuteString("com1 enable", src_command);
	}

	if (cls.state == ca_active)
	{
		return 1;
	}

	return 0;
}
