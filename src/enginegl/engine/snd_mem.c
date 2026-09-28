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
// snd_mem.c: sound caching

#include "quakedef.h"

/*
===============================================================================

WAV loading

===============================================================================
*/

static byte		*data_p;
static byte		*iff_end;
static byte		*last_chunk;
static byte		*iff_data;
static int		iff_chunk_len;

short GetLittleShort(void)
{
	short	val;

	val = (short)(data_p[0] + (data_p[1] << 8));
	data_p += 2;
	return val;
}

int GetLittleLong(void)
{
	int		val;

	val = data_p[0] + (data_p[1] << 8) + (data_p[2] << 16) + (data_p[3] << 24);
	data_p += 4;
	return val;
}

void FindNextChunk(char *name)
{
	while (1)
	{
		if (last_chunk >= iff_end)
			break;		// didn't find the chunk

		data_p = last_chunk + 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
			break;

		data_p -= 8;
		last_chunk = data_p + 8 + ((iff_chunk_len + 1) & ~1);
		if (!strncmp((char *)data_p, name, 4))
			return;
	}

	data_p = NULL;
}

void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk(name);
}

void DumpChunks(void)
{
	char	str[5];

	str[4] = 0;
	data_p = iff_data;
	do
	{
		memcpy(str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf("0x%x : %s (%d)\n", (int)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	}
	while (data_p < iff_end);
}

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo(char *name, byte *wav, int wavlength)
{
	wavinfo_t	info;
	int			format;
	int			samples;

	memset(&info, 0, sizeof(info));

	if (!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!data_p || strncmp((char *)data_p + 8, "WAVE", 4))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		Con_Printf("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != WAVE_FORMAT_PCM)
	{
		Con_Printf("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4 + 2;	// skip the byte rate and block align
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk("LIST");
		if (data_p && !strncmp((char *)data_p + 28, "mark", 4))
		{
			// this is not a proper parse, but it works with cooledit...
			data_p += 24;
			info.samples = GetLittleLong() + info.loopstart;	// samples in loop
		}
	}
	else
	{
		info.loopstart = -1;
	}

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Con_Printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong() / info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Sys_Error("Sound %s has a bad loop length", name);
	}
	else
	{
		info.samples = samples;
	}

	info.dataofs = data_p - wav;

	return info;
}

//=============================================================================

/*
================
ResampleSfx
================
*/
void ResampleSfx(sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	int			outcount;
	int			srcsample;
	float		stepscale;
	int			i;
	int			sample, samplefrac, fracstep;
	sfxcache_t	*sc;

	sc = (sfxcache_t *)Cache_Check(&sfx->cache);
	if (!sc)
		return;

	stepscale = (float)inrate / (float)shm->speed;	// this is usually 0.5, 1, or 2

	outcount = (int)((float)sc->length / stepscale);
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = (int)((float)sc->loopstart / stepscale);

	sc->speed = shm->speed;
	if (loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

// resample / decimate to the current source rate
	if (stepscale == 1.0f && inwidth == 1 && sc->width == 1)
	{
	// fast special case
		for (i = 0; i < outcount; i++)
			((signed char *)sc->data)[i] = (int)((unsigned char)(data[i])) - 128;
	}
	else
	{
	// general case
		samplefrac = 0;
		fracstep = (int)(stepscale * 256.0f);
		for (i = 0; i < outcount; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort(((short *)data)[srcsample]);
			else
				sample = (int)((unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound(sfx_t *s)
{
	char		namebuffer[256];
	byte		*data;
	wavinfo_t	info;
	int			len;
	float		stepscale;
	sfxcache_t	*sc;
	byte		stackbuf[1*1024];		// avoid dirtying the cache heap

// see if still in memory
	sc = (sfxcache_t *)Cache_Check(&s->cache);
	if (sc)
		return sc;

// load it in
	strcpy(namebuffer, "sound/");
	strcat(namebuffer, s->name);

	data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));

	if (!data)
	{
		Con_Printf("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = GetWavinfo(s->name, data, com_filesize);
	if (info.channels != 1)
	{
		Con_Printf("%s is a stereo sample\n", s->name);
		return NULL;
	}

	stepscale = (float)info.rate / (float)shm->speed;
	len = (int)((float)info.samples / stepscale);

	len = len * info.width;

	sc = (sfxcache_t *)Cache_Alloc(&s->cache, len + sizeof(sfxcache_t), s->name);
	if (!sc)
		return NULL;

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	ResampleSfx(s, info.rate, info.width, data + info.dataofs);

	return sc;
}
