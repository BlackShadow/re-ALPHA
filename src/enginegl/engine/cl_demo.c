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

void CL_FinishTimeDemo(void);

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

void CL_WriteDemoMessage(void)
{
    int i;
    int len;
    float angle;

    len = LittleLong(net_message.cursize);
    fwrite(&len, 4, 1, cls.demofile);

    for (i = 0; i < 3; i++)
    {
        angle = LittleFloat(cl_viewangles[i]);
        fwrite(&angle, 4, 1, cls.demofile);
    }

    fwrite(net_message.data, net_message.cursize, 1, cls.demofile);
    fflush(cls.demofile);
}

int CL_GetMessage(void)
{
    int i;
    float angle;
    int r;

    if (cls.demoplayback)
    {

		if (cls.signon == SIGNONS)
		{
			if (cls.timedemo)
			{
				if (host_framecount == cls.td_lastframe)
					return 0;
				cls.td_lastframe = host_framecount;

				if (host_framecount - cls.td_startframe == 1)
					cls.td_starttime = realtime;
			}
			else if (cl_time <= cl_mtime[0])
			{
				return 0;
			}
		}

        if (fread(&net_message.cursize, 4, 1, cls.demofile) != 1)
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
            if (fread(&angle, 4, 1, cls.demofile) != 1)
            {
                CL_StopPlayback();
                return 0;
            }
			cl.mviewangles[0][i] = LittleFloat(angle);
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

void CL_Stop_f(void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf("Not recording a demo.\n");
		return;
	}

	SZ_Clear(&net_message);
	MSG_WriteByte(&net_message, svc_disconnect);
	CL_WriteDemoMessage();

	fclose(cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Printf("Completed demo\n");
}

void CL_Record_f(void)
{
	int argc;
	const char *demoname;
	char filename[MAX_OSPATH];
	int track;

	if (cmd_source != src_command)
		return;

	argc = Cmd_Argc();
	if (argc != 2 && argc != 3 && argc != 4)
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

	if (argc == 2 && cls.state == ca_connected)
	{
		Con_Printf("Can not record - already connected to server\n"
		           "Client demo recording must be started before connecting\n");
		return;
	}

	if (argc == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf("Forcing CD track to %i\n", track);
	}
	else
	{
		track = -1;
	}

	sprintf(filename, "%s/%s", com_gamedir, demoname);

	if (argc > 2)
		Cmd_ExecuteString(va("map %s", Cmd_Argv(2)), src_command);

	COM_DefaultExtension(filename, ".dem");
	Con_Printf("recording to %s.\n", filename);

	cls.demofile = fopen(filename, "wb");
	if (!cls.demofile)
	{
		Con_Printf("ERROR: couldn't open.\n");
		return;
	}

	cls.forcetrack = track;
	fprintf(cls.demofile, "%i\n", track);
	cls.demorecording = true;
}

void CL_PlayDemo_f(void)
{
	char demoname[MAX_OSPATH];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("play <demoname> : plays a demo\n");
		return;
	}

	CL_Disconnect();

	strcpy(demoname, Cmd_Argv(1));
	COM_DefaultExtension(demoname, ".dem");

	Con_Printf("Playing demo from %s.\n", demoname);
	COM_FOpenFile(demoname, &cls.demofile);
	if (!cls.demofile)
	{
		Con_Printf("ERROR: couldn't open.\n");
		cls.demonum = -1;
		return;
	}

	cls.demoplayback = true;
	cls.state = ca_connected;
	fscanf(cls.demofile, "%i\n", &cls.forcetrack);

	cl_time = -99999.0;
	cl_oldtime = -99999.0;
	cl_mtime[0] = 0.0;
	cl_mtime[1] = 0.0;

	key_dest = key_game;
	Con_ClearNotify();
}

void CL_FinishTimeDemo(void)
{
    float totaltime;
    float frames;

    totaltime = realtime - cls.td_starttime;
    cls.timedemo = false;

    if (totaltime == 0.0)
        totaltime = 1.0;

    frames = (float)(host_framecount - cls.td_startframe - 1);
    Con_Printf("%i frames %5.1f seconds %5.1f fps\n", (int)frames, totaltime, frames / totaltime);
}

void CL_TimeDemo_f(void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f();
	cls.timedemo = true;
	cls.td_lastframe = -1;
	cls.td_startframe = host_framecount;
}

void CL_StartMovie_f(void)
{
    const char *filename;

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
