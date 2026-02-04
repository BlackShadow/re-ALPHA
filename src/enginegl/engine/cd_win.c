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
#include <windows.h>
#include <mmsystem.h>

static qboolean	cdValid = false;
static qboolean	cdPlaying = false;
static qboolean	cdWasPlaying = false;
static qboolean	cdInitialized = false;
static qboolean	cdEnabled = false;
static qboolean	cdPlayLooping = false;
static byte		cdPlayTrack;
static byte		cdMaxTrack;
static float	cdVolume;

static UINT		wDeviceID;
static byte		remap[100];

extern HWND		mainwindow;
extern cvar_t	bgmvolume;

static int CDAudio_GetAudioDiskInfo(void)
{
	MCIERROR	mciError;
	MCI_STATUS_PARMS	mciStatusParms;

	cdValid = false;

	mciStatusParms.dwItem = MCI_STATUS_READY;
	mciError = mciSendCommandA(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)&mciStatusParms);
	if (mciError)
	{
		Con_DPrintf("CDAudio: drive not ready\n");
		return -1;
	}

	if (!mciStatusParms.dwReturn)
	{
		Con_DPrintf("CDAudio: drive not ready\n");
		return -1;
	}

	mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	mciError = mciSendCommandA(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR)&mciStatusParms);
	if (mciError)
	{
		Con_DPrintf("CDAudio: get tracks - Loss\n");
		return -1;
	}

	if (mciStatusParms.dwReturn < 1)
	{
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	cdMaxTrack = (byte)mciStatusParms.dwReturn;

	return 0;
}

void CDAudio_Stop(void)
{
	MCIERROR mciError;

	if (!cdEnabled || !cdPlaying)
		return;

	mciError = mciSendCommandA(wDeviceID, MCI_STOP, 0, (DWORD_PTR)NULL);
	if (mciError)
		Con_DPrintf("MCI_STOP failed (%i)", mciError);

	cdWasPlaying = false;
	cdPlaying = false;
}

void CDAudio_Pause(void)
{
	MCIERROR			mciError;
	MCI_GENERIC_PARMS	mciGenericParms;

	if (!cdEnabled || !cdPlaying)
		return;

	mciGenericParms.dwCallback = (DWORD_PTR)mainwindow;
	mciError = mciSendCommandA(wDeviceID, MCI_PAUSE, 0, (DWORD_PTR)&mciGenericParms);
	if (mciError)
		Con_DPrintf("MCI_PAUSE failed (%i)", mciError);

	cdWasPlaying = cdPlaying;
	cdPlaying = false;
}

void CDAudio_Resume(void)
{
	MCIERROR		mciError;
	MCI_PLAY_PARMS	mciPlayParms;

	if (!cdEnabled || !cdValid || !cdWasPlaying)
		return;

	mciPlayParms.dwFrom = MCI_MAKE_TMSF(cdPlayTrack, 0, 0, 0);
	mciPlayParms.dwTo = MCI_MAKE_TMSF(cdPlayTrack + 1, 0, 0, 0);
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;

	mciError = mciSendCommandA(wDeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD_PTR)&mciPlayParms);
	if (mciError)
		Con_DPrintf("CDAudio: MCI_PLAY failed (%i)\n", mciError);
	else
		cdPlaying = true;
}

void CDAudio_Play(byte track, qboolean looping)
{
	MCIERROR			mciError;
	MCI_PLAY_PARMS		mciPlayParms;
	MCI_STATUS_PARMS	mciStatusParms;
	byte				realTrack;

	if (!cdEnabled)
		return;

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	realTrack = remap[track];

	if (realTrack < 1 || realTrack > cdMaxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", realTrack);
		return;
	}

	mciStatusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	mciStatusParms.dwTrack = realTrack;
	mciError = mciSendCommandA(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR)&mciStatusParms);
	if (mciError)
	{
		Con_DPrintf("MCI_STATUS failed (%i)\n", mciError);
		return;
	}

	if (mciStatusParms.dwReturn != MCI_CDA_TRACK_AUDIO)
	{
		Con_Printf("CDAudio: track %i is not audio\n", realTrack);
		return;
	}

	mciStatusParms.dwItem = MCI_STATUS_LENGTH;
	mciStatusParms.dwTrack = realTrack;
	mciError = mciSendCommandA(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR)&mciStatusParms);
	if (mciError)
	{
		Con_DPrintf("MCI_STATUS failed (%i)\n", mciError);
		return;
	}

	if (cdPlaying)
	{
		if (realTrack == cdPlayTrack)
			return;
		CDAudio_Stop();
	}

	mciPlayParms.dwFrom = MCI_MAKE_TMSF(realTrack, 0, 0, 0);
	mciPlayParms.dwTo = MCI_MAKE_TMSF(realTrack, mciStatusParms.dwReturn, 0, 0);
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;

	mciError = mciSendCommandA(wDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD_PTR)&mciPlayParms);
	if (mciError)
	{
		Con_DPrintf("CDAudio: MCI_PLAY failed (%i)\n", mciError);
		return;
	}

	cdPlayTrack = realTrack;
	cdPlaying = true;
	cdPlayLooping = looping;

	if (cdVolume == 0.0f)
		CDAudio_Pause();
}

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (lParam != wDeviceID)
		return 1;

	switch (wParam)
	{
	case MCI_NOTIFY_SUCCESSFUL:
		if (cdPlaying)
		{
			cdPlaying = false;
			if (cdPlayLooping)
				CDAudio_Play(cdPlayTrack, true);
		}
		break;

	case MCI_NOTIFY_ABORTED:
	case MCI_NOTIFY_SUPERSEDED:
		break;

	case MCI_NOTIFY_FAILURE:
		Con_DPrintf("MCI_NOTIFY_FAILURE\n");
		CDAudio_Stop();
		cdValid = false;
		break;

	default:
		Con_DPrintf("Unexpected MM_MCINOTIFY type (%i)\n", wParam);
		return 1;
	}

	return 0;
}

void CDAudio_Update(void)
{
	if (!cdEnabled)
		return;

	if (bgmvolume.value != cdVolume)
	{
		if (cdVolume == 0.0f)
		{
			Cvar_SetValue("bgmvolume", 1.0f);
			cdVolume = bgmvolume.value;
			CDAudio_Resume();
		}
		else
		{
			Cvar_SetValue("bgmvolume", 0.0f);
			cdVolume = bgmvolume.value;
			CDAudio_Pause();
		}
	}
}

static void CDAudio_Eject(void)
{
	MCIERROR mciError;

	mciError = mciSendCommandA(wDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, (DWORD_PTR)NULL);
	if (mciError)
		Con_DPrintf("MCI_SET_DOOR_OPEN failed (%i)\n", mciError);
}

static void CDAudio_CloseDoor(void)
{
	MCIERROR mciError;

	mciError = mciSendCommandA(wDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, (DWORD_PTR)NULL);
	if (mciError)
		Con_DPrintf("MCI_SET_DOOR_CLOSED failed (%i)\n", mciError);
}

static void CD_f(void)
{
	char	*command;
	int		n;
	int		i;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv(1);

	if (!Q_strcasecmp(command, "on"))
	{
		cdEnabled = true;
		return;
	}

	if (!Q_strcasecmp(command, "off"))
	{
		if (cdPlaying)
			CDAudio_Stop();
		cdEnabled = false;
		return;
	}

	if (!Q_strcasecmp(command, "reset"))
	{
		cdEnabled = true;
		if (cdPlaying)
			CDAudio_Stop();
		for (i = 0; i < 100; i++)
			remap[i] = i;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (!Q_strcasecmp(command, "remap"))
	{
		n = Cmd_Argc() - 2;
		if (n > 0)
		{
			for (i = 1; i <= n; i++)
				remap[i] = Q_atoi(Cmd_Argv(i + 1));
		}
		else
		{
			for (i = 1; i < 100; i++)
			{
				if (remap[i] != i)
					Con_Printf("  %u -> %u\n", i, remap[i]);
			}
		}
		return;
	}

	if (!Q_strcasecmp(command, "close"))
	{
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}

	if (!Q_strcasecmp(command, "play"))
	{
		CDAudio_Play(Q_atoi(Cmd_Argv(2)), false);
		return;
	}

	if (!Q_strcasecmp(command, "loop"))
	{
		CDAudio_Play(Q_atoi(Cmd_Argv(2)), true);
		return;
	}

	if (!Q_strcasecmp(command, "stop"))
	{
		CDAudio_Stop();
		return;
	}

	if (!Q_strcasecmp(command, "pause"))
	{
		CDAudio_Pause();
		return;
	}

	if (!Q_strcasecmp(command, "resume"))
	{
		CDAudio_Resume();
		return;
	}

	if (!Q_strcasecmp(command, "eject"))
	{
		if (cdPlaying)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (!Q_strcasecmp(command, "info"))
	{
		Con_Printf("%u tracks\n", cdMaxTrack);
		if (cdPlaying)
			Con_Printf("Currently %s track %u\n", cdPlayLooping ? "looping" : "playing", cdPlayTrack);
		else if (cdWasPlaying)
			Con_Printf("Paused %s track %u\n", cdPlayLooping ? "looping" : "playing", cdPlayTrack);
		Con_Printf("Volume is %f\n", cdVolume);
		return;
	}
}

int CDAudio_Init(void)
{
	MCIERROR		mciError;
	MCI_OPEN_PARMSA	mciOpenParms;
	MCI_SET_PARMS	mciSetParms;
	int				i;

	if (cls.state == ca_dedicated)
		return -1;

	if (COM_CheckParm("-nocdaudio"))
		return -1;

	memset(&mciOpenParms, 0, sizeof(mciOpenParms));
	memset(&mciSetParms, 0, sizeof(mciSetParms));

	mciOpenParms.lpstrDeviceType = "cdaudio";
	mciError = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD_PTR)&mciOpenParms);
	if (mciError)
	{
		Con_Printf("CDAudio_Init: MCI_OPEN failed (%i)\n", mciError);
		return -1;
	}

	wDeviceID = mciOpenParms.wDeviceID;

	mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;
	mciError = mciSendCommandA(wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&mciSetParms);
	if (mciError)
	{
		Con_Printf("MCI_SET_TIME_FORMAT failed (%i)\n", mciError);
		mciSendCommandA(wDeviceID, MCI_CLOSE, 0, (DWORD_PTR)NULL);
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;

	cdInitialized = true;
	cdEnabled = true;

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_Printf("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	Cmd_AddCommand("cd", CD_f);

	Con_Printf("CD Audio Initialized\n");

	return 0;
}

void CDAudio_Shutdown(void)
{
	if (!cdInitialized)
		return;

	CDAudio_Stop();

	if (mciSendCommandA(wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)NULL))
		Con_DPrintf("CDAudio_Shutdown: MCI_CLOSE failed\n");
}
