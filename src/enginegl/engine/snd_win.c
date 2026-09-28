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
// snd_win.c -- DirectSound output, with waveOut as the fallback

#include "quakedef.h"
#include "winquake.h"

// 64K is > 1 second at 16-bit, 11025 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

#define WAV_BLOCKS_AHEAD		4		// waveOut blocks kept queued

// format the output device is opened with
#define SND_CHANNELS			2
#define SND_SAMPLEBITS			16
#define SND_SPEED				11025

typedef enum {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

static LPDIRECTSOUND		pDS;
LPDIRECTSOUNDBUFFER			pDSBuf;
static LPDIRECTSOUNDBUFFER	pDSPBuf;

static HWAVEOUT				hWaveOut;
static HGLOBAL				hWaveHdr;
static LPWAVEHDR			lpWaveHdr;
static HGLOBAL				hData;
static LPSTR				lpData;

int							gSndBufSize;
static DWORD				mmstarttime;

static HINSTANCE			hInstDS;
static HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);

static qboolean				dsound_init;
static qboolean				wav_init;
static qboolean				snd_firsttime = true;
static qboolean				snd_isdirect;
static qboolean				snd_iswave;
static qboolean				primary_format_set;
static int					snd_acquired;	// DirectSound is shut down while this is 0
static int					sample16;
static int					snd_sent, snd_completed;
static qboolean				wavonly;

/*
==================
S_BlockSound
==================
*/
void S_BlockSound(void)
{
// DirectSound takes care of blocking itself
	if (snd_iswave)
	{
		if (++snd_blocked == 1)
			waveOutReset(hWaveOut);
	}
}

/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound(void)
{
// DirectSound takes care of blocking itself
	if (snd_iswave)
		--snd_blocked;
}

/*
==================
FreeSound
==================
*/
void FreeSound(void)
{
	int		i;

	if (pDSBuf)
	{
		pDSBuf->lpVtbl->Stop(pDSBuf);
		pDSBuf->lpVtbl->Release(pDSBuf);
	}

// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && pDSBuf != pDSPBuf)
	{
		pDSPBuf->lpVtbl->Release(pDSPBuf);
	}

	if (pDS)
	{
		pDS->lpVtbl->SetCooperativeLevel(pDS, mainwindow, DSSCL_NORMAL);
		pDS->lpVtbl->Release(pDS);
	}

	if (hWaveOut)
	{
		waveOutReset(hWaveOut);

		if (lpWaveHdr)
		{
			for (i = 0; i < WAV_BUFFERS; i++)
				waveOutUnprepareHeader(hWaveOut, lpWaveHdr + i, sizeof(WAVEHDR));
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
	dsound_init = false;
	wav_init = false;
}

/*
==================
S_EndPrecaching

Releases DirectSound (around a video mode change).
==================
*/
void S_EndPrecaching(void)
{
	if (snd_isdirect)
	{
		if (!--snd_acquired)
		{
			S_ClearBuffer();
			S_Shutdown();
		}
	}
}

/*
==================
S_BeginPrecaching

Restarts DirectSound after S_EndPrecaching.
==================
*/
void S_BeginPrecaching(void)
{
	if (snd_isdirect)
	{
		if (++snd_acquired == 1)
			S_Startup();
	}
}

/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
sndinitstat SNDDMA_InitDirect(void)
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DWORD			dwSize, dwWrite;
	DSCAPS			dscaps;
	WAVEFORMATEX	format, pformat;
	HRESULT			hresult;
	int				reps;

	memset((void *)&sn, 0, sizeof(sn));

	shm = &sn;

	shm->channels = SND_CHANNELS;
	shm->samplebits = SND_SAMPLEBITS;
	shm->speed = SND_SPEED;

	memset(&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = SND_CHANNELS;
	format.wBitsPerSample = SND_SAMPLEBITS;
	format.nSamplesPerSec = SND_SPEED;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	if (!hInstDS)
	{
		hInstDS = LoadLibraryA("dsound.dll");
		if (!hInstDS)
		{
			Con_SafePrintf("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate = (void *)GetProcAddress(hInstDS, "DirectSoundCreate");
		if (!pDirectSoundCreate)
		{
			Con_SafePrintf("Couldn't get DS proc addr\n");
			return SIS_FAILURE;
		}
	}

	while ((hresult = pDirectSoundCreate(NULL, &pDS, NULL)) != DS_OK)
	{
		if (hresult != DSERR_ALLOCATED)
		{
			Con_Printf("DirectSound create failed\n");
			return SIS_FAILURE;
		}

		if (MessageBoxA(NULL,
						"The sound hardware is in use by another app.\n\n"
						"Select Retry to try to start sound again or Cancel to run Quake with no sound.",
						"Sound not available",
						MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_SafePrintf("DirectSoundCreate failure\n"
						   "  hardware already in use\n");
			return SIS_NOTAVAIL;
		}
	}

	dscaps.dwSize = sizeof(dscaps);
	if (DS_OK != pDS->lpVtbl->GetCaps(pDS, &dscaps))
	{
		Con_SafePrintf("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_SafePrintf("No DirectSound driver installed\n");
		FreeSound();
		return SIS_FAILURE;
	}

	if (DS_OK != pDS->lpVtbl->SetCooperativeLevel(pDS, mainwindow, DSSCL_EXCLUSIVE))
	{
		Con_SafePrintf("Set coop level failed\n");
		FreeSound();
		return SIS_FAILURE;
	}

// get access to the primary buffer, if possible, so we can set the
// sound hardware format
	memset(&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

	if (!COM_CheckParm("-snoforceformat"))
	{
		if (DS_OK == pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL))
		{
			pformat = format;

			if (DS_OK != pDSPBuf->lpVtbl->SetFormat(pDSPBuf, &pformat))
			{
				if (snd_firsttime)
					Con_DPrintf("Set primary sound buffer format: no\n");
			}
			else
			{
				if (snd_firsttime)
					Con_DPrintf("Set primary sound buffer format: yes\n");

				primary_format_set = true;
			}
		}
	}

	if (primary_format_set && COM_CheckParm("-primarysound"))
	{
		if (DS_OK != pDS->lpVtbl->SetCooperativeLevel(pDS, mainwindow, DSSCL_WRITEPRIMARY))
		{
			Con_SafePrintf("Set coop level failed\n");
			FreeSound();
			return SIS_FAILURE;
		}

		if (DS_OK != pDSPBuf->lpVtbl->GetCaps(pDSPBuf, &dsbcaps))
		{
			Con_Printf("DS:GetCaps failed\n");
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
		Con_SafePrintf("Using primary sound buffer\n");
	}
	else
	{
	// create the secondary buffer we'll actually work with
		memset(&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
		{
			Con_SafePrintf("DS:CreateSoundBuffer Failed");
			FreeSound();
			return SIS_FAILURE;
		}

		shm->channels = format.nChannels;
		shm->samplebits = format.wBitsPerSample;
		shm->speed = format.nSamplesPerSec;

		if (DS_OK != pDSBuf->lpVtbl->GetCaps(pDSBuf, &dsbcaps))
		{
			Con_SafePrintf("DS:GetCaps failed\n");
			FreeSound();
			return SIS_FAILURE;
		}

		if (snd_firsttime)
			Con_SafePrintf("Using secondary sound buffer\n");
	}

// make sure mixer is active
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		Con_DPrintf("   %d channel(s)\n"
					"   %d bits/sample\n"
					"   %d bytes/sec\n",
					shm->channels, shm->samplebits, shm->speed);

	gSndBufSize = dsbcaps.dwBufferBytes;

// initialize the buffer
	reps = 0;

	while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, (void **)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_SafePrintf("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound();
			return SIS_FAILURE;
		}

		if (++reps > DS_LOCK_RETRIES)
		{
			Con_SafePrintf("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound();
			return SIS_FAILURE;
		}
	}

	memset(lpData, 0, dwSize);

	pDSBuf->lpVtbl->Unlock(pDSBuf, lpData, dwSize, NULL, 0);

	// we don't want anyone to access the buffer directly w/o locking it first
	lpData = NULL;

	pDSBuf->lpVtbl->Stop(pDSBuf);
	pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmstarttime, &dwWrite);
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *)lpData;

	dsound_init = true;
	sample16 = (shm->samplebits / 8) - 1;

	return SIS_SUCCESS;
}

/*
==================
SNDDMA_InitWav

Crappy windows multimedia base
==================
*/
qboolean SNDDMA_InitWav(void)
{
	WAVEFORMATEX	format;
	int				i;
	MMRESULT		hr;

	shm = &sn;

	shm->channels = SND_CHANNELS;
	shm->samplebits = SND_SAMPLEBITS;
	shm->speed = SND_SPEED;

	snd_sent = 0;
	snd_completed = 0;

	memset(&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = SND_CHANNELS;
	format.wBitsPerSample = SND_SAMPLEBITS;
	format.nSamplesPerSec = SND_SPEED;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	// open a waveform device for output
	while ((hr = waveOutOpen(&hWaveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL)) != MMSYSERR_NOERROR)
	{
		if (hr != MMSYSERR_ALLOCATED)
		{
			Con_Printf("waveOutOpen failed\n");
			return false;
		}

		if (MessageBoxA(NULL,
						"The sound hardware is in use by another app.\n\n"
						"Select Retry to try to start sound again or Cancel to run Quake with no sound.",
						"Sound not available",
						MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_SafePrintf("waveOutOpen failure;\n"
						   "  hardware already in use\n");
			return false;
		}
	}

	// allocate and lock memory for the waveform data, it must be
	// globally allocated with GMEM_MOVEABLE and GMEM_SHARE flags
	gSndBufSize = WAV_BUFFERS * WAV_BUFFER_SIZE;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData)
	{
		Con_SafePrintf("Sound: Out of memory.\n");
		FreeSound();
		return false;
	}

	lpData = GlobalLock(hData);
	if (!lpData)
	{
		Con_SafePrintf("Sound: Failed to lock.\n");
		FreeSound();
		return false;
	}
	memset(lpData, 0, gSndBufSize);

	// allocate and lock memory for the headers, same flags
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(WAVEHDR) * WAV_BUFFERS);
	if (!hWaveHdr)
	{
		Con_SafePrintf("Sound: Failed to Alloc header.\n");
		FreeSound();
		return false;
	}

	lpWaveHdr = (LPWAVEHDR)GlobalLock(hWaveHdr);
	if (!lpWaveHdr)
	{
		Con_SafePrintf("Sound: Failed to lock header.\n");
		FreeSound();
		return false;
	}

	memset(lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	// after allocation, set up and prepare headers
	for (i = 0; i < WAV_BUFFERS; i++)
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i * WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr + i, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			Con_SafePrintf("Sound: failed to prepare wave headers\n");
			FreeSound();
			return false;
		}
	}

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *)lpData;

	wav_init = true;
	sample16 = (shm->samplebits / 8) - 1;

	return true;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
qboolean SNDDMA_Init(void)
{
	sndinitstat	stat;

	if (COM_CheckParm("-wavonly"))
		wavonly = true;

	dsound_init = wav_init = false;

	stat = SIS_FAILURE;		// assume DirectSound won't initialize

	// init DirectSound
	if (!wavonly)
	{
		if (snd_firsttime || snd_isdirect)
		{
			stat = SNDDMA_InitDirect();

			if (stat == SIS_SUCCESS)
			{
				snd_isdirect = true;

				if (snd_firsttime)
					Con_DPrintf("DirectSound initialized\n");
			}
			else
			{
				snd_isdirect = false;
				Con_DPrintf("DirectSound failed to init\n");
			}
		}
	}

// if DirectSound didn't succeed in initializing, try to initialize
// waveOut sound, unless DirectSound failed because the hardware is
// already allocated (in which case the user has already chosen not
// to have sound)
	if (!dsound_init && stat != SIS_NOTAVAIL)
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

	snd_acquired = 1;
	snd_firsttime = false;

	if (dsound_init || wav_init)
		return true;

	return false;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	DWORD	mmtime;
	int		s;
	DWORD	dwWrite;

	if (dsound_init)
	{
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime, &dwWrite);
		s = mmtime - mmstarttime;
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

	s &= (shm->samples - 1);

	return s;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
	LPWAVEHDR	h;
	int			wResult;

	if (!wav_init)
		return;

	//
	// find which sound blocks have completed
	//
	while (1)
	{
		if (snd_completed == snd_sent)
		{
			Con_Printf("Sound overrun\n");
			break;
		}

		if (!(lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE))
			break;

		snd_completed++;	// this buffer has been played
	}

	//
	// submit new sound blocks
	//
	while (((snd_sent - snd_completed) >> sample16) < WAV_BLOCKS_AHEAD)
	{
		h = lpWaveHdr + (snd_sent & WAV_MASK);

		snd_sent++;

		// the data block is sent to the output device in the background
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));
		if (wResult != MMSYSERR_NOERROR)
		{
			Con_SafePrintf("Failed to write block to device\n");
			FreeSound();
			return;
		}
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	FreeSound();
}
