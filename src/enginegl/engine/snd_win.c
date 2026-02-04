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
#include <dsound.h>

static LPDIRECTSOUND			pDS = NULL;
LPDIRECTSOUNDBUFFER				pDSBuf = NULL;
static LPDIRECTSOUNDBUFFER		pDSPBuf = NULL;

static HWAVEOUT					hWaveOut = NULL;
static HGLOBAL					hWaveHdr = NULL;
static LPWAVEHDR				lpWaveHdr = NULL;
static HGLOBAL					hData = NULL;
static LPSTR					lpData = NULL;

int								gSndBufSize = 0;
static DWORD					mmStartTime = 0;

static HINSTANCE				hInstDS = NULL;
typedef HRESULT (WINAPI *pDirectSoundCreate_t)(GUID FAR *, LPDIRECTSOUND FAR *, IUnknown FAR *);
static pDirectSoundCreate_t		pDirectSoundCreate = NULL;

static int						dsound_init = 0;
static int						wav_init = 0;
static int						snd_firsttime = 1;
static int						snd_isdirect = 0;
static int						snd_iswave = 0;
static int						primary_format_set = 0;
static int						snd_blocked = 0;
static int						sample16 = 0;
static int						snd_sent = 0;
static int						snd_completed = 0;
static int						wavonly = 0;

extern HWND						mainwindow;

extern int g_SndBlocked;

#define WAV_BUFFERS				64
#define WAV_MASK				0x3F
#define WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

void S_BlockSound(void)
{
	if (snd_iswave)
	{
		if (++g_SndBlocked == 1)
			waveOutReset(hWaveOut);
	}
}

void S_UnblockSound(void)
{
	if (snd_iswave)
		--g_SndBlocked;
}

int FreeSound(void)
{
	int i;

	if (pDSBuf)
	{
		pDSBuf->lpVtbl->Stop(pDSBuf);
		pDSBuf->lpVtbl->Release(pDSBuf);
	}

	if (pDSPBuf && pDSBuf != pDSPBuf)
	{
		pDSPBuf->lpVtbl->Release(pDSPBuf);
	}

	if (pDS)
	{
		pDS->lpVtbl->SetCooperativeLevel(pDS, mainwindow, DSSCL_NORMAL);
		pDS->lpVtbl->Release(pDS);
	}

	i = 0;
	if (hWaveOut)
	{
		waveOutReset(hWaveOut);

		if (lpWaveHdr)
		{
			do
			{
				waveOutUnprepareHeader(hWaveOut, (LPWAVEHDR)((char*)lpWaveHdr + i), sizeof(WAVEHDR));
				i += sizeof(WAVEHDR);
			}
			while (i < 2048);
		}

		waveOutClose(hWaveOut);

		if (hWaveHdr)
		{
			GlobalUnlock(hWaveHdr);
			GlobalFree(hWaveHdr);
		}

		if (hData)
		{
			GlobalUnlock(hData);
			GlobalFree(hData);
		}
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = NULL;
	hData = NULL;
	hWaveHdr = NULL;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = 0;
	wav_init = 0;

	return 0;
}

void S_EndPrecaching(void)
{
	if (snd_isdirect)
	{
		if (!--snd_blocked)
		{
			S_ClearBuffer();
			S_Shutdown();
			return;
		}
	}
}

void S_BeginPrecaching(void)
{
	if (snd_isdirect)
	{
		if (++snd_blocked == 1)
		{
			S_Startup();
			return;
		}
	}
}

int SNDDMA_InitDirect(void)
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DWORD			dwSize;
	DWORD			dwWrite;
	DSCAPS			dscaps;
	WAVEFORMATEX	format;
	WAVEFORMATEX	pformat;
	HRESULT			hresult;
	int				reps;
	int*			shmPtr;

	memset((void *)&sn, 0, sizeof(sn));
	shm = &sn;

	*((int*)shm + 3) = 2;
	*((int*)shm + 7) = 16;
	*((int*)shm + 8) = 11025;

	memset(&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.wBitsPerSample = 16;
	format.nSamplesPerSec = 11025;
	format.nBlockAlign = 4;
	format.cbSize = 0;
	format.nAvgBytesPerSec = 44100;

	if (!hInstDS)
	{
		hInstDS = LoadLibraryA("dsound.dll");
		if (!hInstDS)
		{
			Con_SafePrintf("Couldn't load dsound.dll\n");
			return 1;
		}

		pDirectSoundCreate = (pDirectSoundCreate_t)GetProcAddress(hInstDS, "DirectSoundCreate");
		if (!pDirectSoundCreate)
		{
			Con_SafePrintf("Couldn't get DS proc addr\n");
			return 1;
		}
	}

	while (1)
	{
		hresult = pDirectSoundCreate(NULL, &pDS, NULL);
		if (hresult == DS_OK)
			break;

		if (hresult != DSERR_ALLOCATED)
		{
			Con_Printf("DirectSound create failed\n");
			return 1;
		}

		if (MessageBoxA(NULL,
			"The sound hardware is in use by another app.\n\n"
			"Select Retry to try to start sound again or Cancel to run Quake with no sound.",
			"Sound not available",
			MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_SafePrintf("DirectSoundCreate failure\n  hardware already in use\n");
			return 2;
		}
	}

	dscaps.dwSize = sizeof(dscaps);
	if (pDS->lpVtbl->GetCaps(pDS, &dscaps))
	{
		Con_SafePrintf("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_SafePrintf("No DirectSound driver installed\n");
		FreeSound();
		return 1;
	}

	if (pDS->lpVtbl->SetCooperativeLevel(pDS, mainwindow, DSSCL_EXCLUSIVE))
	{
		Con_SafePrintf("Set coop level failed\n");
		FreeSound();
		return 1;
	}

	memset(&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = 0;

	if (!COM_CheckParm("-snoforceformat"))
	{
		if (pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL) == DS_OK)
		{
			pformat = format;
			if (pDSPBuf->lpVtbl->SetFormat(pDSPBuf, &pformat))
			{
				if (snd_firsttime)
					Con_DPrintf("Set primary sound buffer format: no\n");
			}
			else
			{
				if (snd_firsttime)
					Con_DPrintf("Set primary sound buffer format: yes\n");
				primary_format_set = 1;
			}
		}
	}

	if (primary_format_set && COM_CheckParm("-primarysound"))
	{

		if (pDS->lpVtbl->SetCooperativeLevel(pDS, mainwindow, DSSCL_WRITEPRIMARY))
		{
			Con_SafePrintf("Set coop level failed\n");
			FreeSound();
			return 1;
		}

		if (pDSPBuf->lpVtbl->GetCaps(pDSPBuf, &dsbcaps))
		{
			Con_Printf("DS:GetCaps failed\n");
			return 1;
		}

		pDSBuf = pDSPBuf;
		Con_SafePrintf("Using primary sound buffer\n");
	}
	else
	{

		memset(&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		if (pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
		{
			Con_SafePrintf("DS:CreateSoundBuffer Failed");
			FreeSound();
			return 1;
		}

		shmPtr = (int*)shm;
		shmPtr[3] = format.nChannels;
		shmPtr[7] = format.wBitsPerSample;
		shmPtr[8] = format.nSamplesPerSec;

		if (pDSBuf->lpVtbl->GetCaps(pDSBuf, &dsbcaps))
		{
			Con_SafePrintf("DS:GetCaps failed\n");
			FreeSound();
			return 1;
		}

		if (snd_firsttime)
			Con_SafePrintf("Using secondary sound buffer\n");
	}

	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
	{
		Con_DPrintf("   %d channel(s)\n   %d bits/sample\n   %d bytes/sec\n",
			*((int*)shm + 3),
			*((int*)shm + 7),
			*((int*)shm + 8));
	}

	gSndBufSize = dsbcaps.dwBufferBytes;

	reps = 0;
	while (1)
	{
		hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, (void**)&lpData, &dwSize, NULL, NULL, 0);
		if (hresult == DS_OK)
			break;

		if (hresult != DSERR_BUFFERLOST)
		{
			Con_SafePrintf("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound();
			return 1;
		}

		if (++reps > 10000)
		{
			Con_SafePrintf("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound();
			return 1;
		}
	}

	memset(lpData, 0, dwSize);
	pDSBuf->lpVtbl->Unlock(pDSBuf, lpData, dwSize, NULL, 0);

	lpData = NULL;

	pDSBuf->lpVtbl->Stop(pDSBuf);
	pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmStartTime, &dwWrite);
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	shmPtr = (int*)shm;
	shmPtr[1] = 1;
	shmPtr[2] = 0;
	shmPtr[4] = gSndBufSize / (shmPtr[7] / 8);
	shmPtr[6] = 0;
	shmPtr[5] = 1;
	shmPtr[9] = (int)lpData;

	dsound_init = 1;
	sample16 = (shmPtr[7] / 8) - 1;

	return 0;
}

int SNDDMA_InitWav(void)
{
	WAVEFORMATEX	format;
	int				i;
	MMRESULT		hr;
	int*			shmPtr;

	shm = &sn;

	*((int*)shm + 3) = 2;
	*((int*)shm + 7) = 16;
	*((int*)shm + 8) = 11025;

	snd_sent = 0;
	snd_completed = 0;

	memset(&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 2;
	format.wBitsPerSample = 16;
	format.nSamplesPerSec = 11025;
	format.nBlockAlign = 4;
	format.cbSize = 0;
	format.nAvgBytesPerSec = 44100;

	while (1)
	{
		hr = waveOutOpen(&hWaveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
		if (hr == MMSYSERR_NOERROR)
			break;

		if (hr != MMSYSERR_ALLOCATED)
		{
			Con_Printf("waveOutOpen failed\n");
			return 0;
		}

		if (MessageBoxA(NULL,
			"The sound hardware is in use by another app.\n\n"
			"Select Retry to try to start sound again or Cancel to run Quake with no sound.",
			"Sound not available",
			MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_SafePrintf("waveOutOpen failure;\n  hardware already in use\n");
			return 0;
		}
	}

	gSndBufSize = 0x10000;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData)
	{
		Con_SafePrintf("Sound: Out of memory.\n");
		FreeSound();
		return 0;
	}

	lpData = GlobalLock(hData);
	if (!lpData)
	{
		Con_SafePrintf("Sound: Failed to lock.\n");
		FreeSound();
		return 0;
	}

	memset(lpData, 0, gSndBufSize);

	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, 0x800);
	if (!hWaveHdr)
	{
		Con_SafePrintf("Sound: Failed to Alloc header.\n");
		FreeSound();
		return 0;
	}

	lpWaveHdr = (LPWAVEHDR)GlobalLock(hWaveHdr);
	if (!lpWaveHdr)
	{
		Con_SafePrintf("Sound: Failed to lock header.\n");
		FreeSound();
		return 0;
	}

	memset(lpWaveHdr, 0, 0x800);

	i = 0;
	while (1)
	{
		lpWaveHdr[i / sizeof(WAVEHDR)].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i / sizeof(WAVEHDR)].lpData = lpData + (i / sizeof(WAVEHDR)) * WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, &lpWaveHdr[i / sizeof(WAVEHDR)], sizeof(WAVEHDR)))
		{
			Con_SafePrintf("Sound: failed to prepare wave headers\n");
			FreeSound();
			return 0;
		}

		i += sizeof(WAVEHDR);
		if (i >= 2048)
			break;
	}

	shmPtr = (int*)shm;
	shmPtr[1] = 1;
	shmPtr[2] = 0;
	shmPtr[4] = gSndBufSize / (shmPtr[7] / 8);
	shmPtr[6] = 0;
	shmPtr[5] = 1;
	shmPtr[9] = (int)lpData;

	wav_init = 1;
	sample16 = (shmPtr[7] / 8) - 1;

	return 1;
}

int SNDDMA_Init(void)
{
	int stat;

	if (COM_CheckParm("-wavonly"))
		wavonly = 1;

	wav_init = 0;
	dsound_init = 0;
	stat = 1;

	if (!wavonly)
	{
		if (snd_firsttime || snd_isdirect)
		{
			stat = SNDDMA_InitDirect();

			if (stat == 0)
			{
				snd_isdirect = 1;
				if (snd_firsttime)
					Con_DPrintf("DirectSound initialized\n");
			}
			else
			{
				snd_isdirect = 0;
				Con_DPrintf("DirectSound failed to init\n");
			}
		}
	}

	if (!dsound_init && stat != 2)
	{
		if (snd_firsttime || snd_iswave)
		{
			snd_iswave = SNDDMA_InitWav();

			if (snd_iswave)
			{
				if (snd_firsttime)
					Con_DPrintf("Wave sound initialized\n");
			}
			else
			{
				Con_DPrintf("Wave sound failed to init\n");
			}
		}
	}

	snd_blocked = 1;
	snd_firsttime = 0;

	if (dsound_init || wav_init)
		return 1;

	return 0;
}

int SNDDMA_GetDMAPos(void)
{
	DWORD	dwWrite;
	int		s;
	DWORD	mmtime;

	if (dsound_init)
	{
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime, &dwWrite);
		s = mmtime - mmStartTime;
	}
	else if (wav_init)
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}
	else
	{
		s = 0;
	}

	s >>= sample16;
	s &= (*((int*)shm + 4) - 1);

	return s;
}

void SNDDMA_Submit(void)
{
	LPWAVEHDR	h;
	int			wResult;
	int			idx;

	if (!wav_init)
		return;

	while (1)
	{
		if (snd_completed == snd_sent)
		{
			Con_Printf("Sound overrun\n");
			break;
		}

		if (!(lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE))
			break;

		snd_completed++;
	}

	while (((snd_sent - snd_completed) >> sample16) < 4)
	{
		idx = snd_sent & WAV_MASK;
		h = &lpWaveHdr[idx];
		snd_sent++;

		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));
		if (wResult != MMSYSERR_NOERROR)
		{
			Con_SafePrintf("Failed to write block to device\n");
			FreeSound();
			return;
		}
	}
}

void SNDDMA_Shutdown(void)
{
	FreeSound();
}
