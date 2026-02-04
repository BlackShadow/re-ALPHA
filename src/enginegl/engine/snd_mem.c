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

static byte		*data_p;
static byte		*iff_end;
static byte		*last_chunk;
static byte		*iff_data;
static int		iff_chunk_len;

extern int		com_filesize;
extern short (*LittleShort)(short l);
extern cvar_t	loadas8bit;

typedef struct wavinfo_s
{
	int rate;
	int width;
	int channels;
	int loopstart;
	int samples;
	int dataofs;
} wavinfo_t;

short GetLittleShort(void)
{
	short val;

	val = (short)(data_p[0] + (data_p[1] << 8));
	data_p += 2;

	return val;
}

int GetLittleLong(void)
{
	int val;

	val = data_p[0] + (data_p[1] << 8) + (data_p[2] << 16) + (data_p[3] << 24);
	data_p += 4;

	return val;
}

void FindNextChunk(char *name)
{
	while (1)
	{
		if (last_chunk >= iff_end)
			break;

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

	FindChunk("RIFF");
	if (!data_p || strncmp((char *)data_p + 8, "WAVE", 4))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		Con_Printf("Missing fmt chunk\n");
		return info;
	}

	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Con_Printf("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 6;
	info.width = GetLittleShort() / 8;

	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();

		FindNextChunk("LIST");
		if (data_p && !strncmp((char *)data_p + 28, "mark", 4))
		{

			data_p += 24;
			info.samples = GetLittleLong() + info.loopstart;
		}
	}
	else
	{
		info.loopstart = -1;
	}

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

	stepscale = (float)inrate / (float)shm->speed;
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

	if (stepscale == 1.0f && inwidth == 1 && sc->width == 1)
	{

		for (i = 0; i < outcount; i++)
			((signed char *)sc->data)[i] = (int)((unsigned char)(data[i])) - 128;
	}
	else
	{

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

sfxcache_t *S_LoadSound(sfx_t *s)
{
	char		namebuffer[256];
	byte		*data;
	wavinfo_t	info;
	int			len;
	float		stepscale;
	float		sample_count_f;
	sfxcache_t	*sc;
	byte		stackbuf[1024];

	sc = (sfxcache_t *)Cache_Check(&s->cache);
	if (sc)
		return sc;

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
	sample_count_f = (float)info.samples;
	len = info.width * (int)(sample_count_f / stepscale) + 24;

	sc = (sfxcache_t *)Cache_Alloc(&s->cache, len, s->name);
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
