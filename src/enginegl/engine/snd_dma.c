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
// snd_dma.c -- main control for any streaming sound output device

#include <limits.h>

#include "quakedef.h"
#include "winquake.h"

#define SND_LOWMEM_HEAPSIZE	0x800000	// below this heap size all sounds load as 8 bit

// format of the -simsound fake dma buffer
#define FAKEDMA_SAMPLEBITS	16
#define FAKEDMA_SPEED		22050
#define FAKEDMA_CHANNELS	2
#define FAKEDMA_SAMPLES		32768

#define AMBIENT_MINVOL		8.0f		// quieter ambient levels are silent

void S_Play(void);
void S_PlayVol(void);
void S_SoundList(void);
void S_StopAllSoundsC(void);

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t		channels[MAX_CHANNELS];
int				total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;

static sfx_t	*ambient_sfx[NUM_AMBIENTS];

qboolean		snd_initialized = false;
qboolean		sound_started = false;
int				snd_blocked = 0;
qboolean		snd_ambient = false;

qboolean		fakedma = false;

// pointer should go away
volatile dma_t	*shm = NULL;
volatile dma_t	sn;

vec3_t			listener_origin;
vec3_t			listener_forward;
vec3_t			listener_right;
vec3_t			listener_up;
vec_t			sound_nominal_clip_dist = 1000.0f;

int				paintedtime;		// sample PAIRS
int				soundtime;			// sample PAIRS
int				soundtime_buffers;	// times the dma position wrapped
int				oldsamplepos;

sfx_t			*known_sfx = NULL;	// hunk allocated [MAX_SFX]
int				num_sfx = 0;

cvar_t			volume = {"volume", "0.7", true, false};
cvar_t			nosound = {"nosound", "0"};
cvar_t			precache = {"precache", "1"};
cvar_t			loadas8bit = {"loadas8bit", "0"};
cvar_t			bgmvolume = {"bgmvolume", "1", true, false};
cvar_t			bgmbuffer = {"bgmbuffer", "4096"};
cvar_t			ambient_level = {"ambient_level", "0.3"};
cvar_t			ambient_fade = {"ambient_fade", "100"};
cvar_t			snd_noextraupdate = {"snd_noextraupdate", "0"};
cvar_t			snd_show = {"snd_show", "0"};
cvar_t			snd_mixahead = {"_snd_mixahead", "0.1", true, false};

/*
================
S_SoundInfo_f
================
*/
void S_SoundInfo_f(void)
{
	if (!sound_started || !shm)
	{
		Con_Printf("sound system not started\n");
		return;
	}

	Con_Printf("%5d stereo\n", shm->channels - 1);
	Con_Printf("%5d samples\n", shm->samples);
	Con_Printf("%5d samplepos\n", shm->samplepos);
	Con_Printf("%5d samplebits\n", shm->samplebits);
	Con_Printf("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf("%5d speed\n", shm->speed);
	Con_Printf("0x%x dma buffer\n", (int)shm->buffer);
	Con_Printf("%5d total_channels\n", total_channels);
}

/*
================
S_Startup
================
*/
void S_Startup(void)
{
	if (!snd_initialized)
		return;

	sound_started = fakedma || SNDDMA_Init();
}

void S_AmbientOn(void)
{
	snd_ambient = true;
}

void S_AmbientOff_Null(void)
{
}

void S_AmbientOn_Null(void)
{
}

/*
================
S_Init
================
*/
void S_Init(void)
{
	Con_DPrintf("\nSound Initialization\n");

	if (COM_CheckParm("-nosound"))
		return;

	if (COM_CheckParm("-simsound"))
		fakedma = true;

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("playvol", S_PlayVol);
	Cmd_AddCommand("stopsound", S_StopAllSoundsC);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&precache);
	Cvar_RegisterVariable(&loadas8bit);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&bgmbuffer);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&snd_mixahead);

	if (host_parms.memsize < SND_LOWMEM_HEAPSIZE)
	{
		Cvar_Set("loadas8bit", "1");
		Con_DPrintf("loading all sounds as 8bit\n");
	}

	snd_initialized = true;

	S_Startup();

	SND_InitScaletable();

	known_sfx = (sfx_t *)Hunk_AllocName(MAX_SFX * sizeof(sfx_t), "sfx_t");
	num_sfx = 0;

// create a piece of DMA memory
	if (fakedma)
	{
		shm = (dma_t *)Hunk_AllocName(sizeof(*shm), "shm");
		shm->splitbuffer = false;
		shm->samplebits = FAKEDMA_SAMPLEBITS;
		shm->speed = FAKEDMA_SPEED;
		shm->channels = FAKEDMA_CHANNELS;
		shm->samples = FAKEDMA_SAMPLES;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;
		shm->submission_chunk = 1;
		shm->buffer = (unsigned char *)Hunk_AllocName(FAKEDMA_SAMPLES * FAKEDMA_SAMPLEBITS / 8, "shmbuf");
	}

	Con_DPrintf("Sound sampling rate: %i\n", shm->speed);

	S_StopAllSounds(true);

	SX_Init();
}

// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown(void)
{
	if (!sound_started)
		return;

	if (shm)
		shm->gamealive = false;

	shm = NULL;
	sound_started = false;

	if (!fakedma)
		SNDDMA_Shutdown();
}

// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName
==================
*/
sfx_t *S_FindName(const char *name)
{
	int		i;
	sfx_t	*sfx;

	if (!name)
		Sys_Error("S_FindName: NULL\n");

	if (strlen(name) >= MAX_QPATH)
		Sys_Error("Sound name too long: %s", name);

// see if already loaded
	for (i = 0; i < num_sfx; i++)
	{
		if (!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];
	}

	if (num_sfx == MAX_SFX)
		Sys_Error("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strcpy(sfx->name, name);

	num_sfx++;

	return sfx;
}

/*
==================
S_PrecacheSound
==================
*/
sfx_t *S_PrecacheSound(const char *name)
{
	sfx_t	*sfx;

	if (!sound_started || nosound.value != 0.0f)
		return NULL;

	sfx = S_FindName(name);

// cache it in
	if (precache.value != 0.0f)
		S_LoadSound(sfx);

	return sfx;
}

//=============================================================================

/*
=================
SND_PickChannel
=================
*/
channel_t *SND_PickChannel(int entnum, int entchannel)
{
	int			ch_idx;
	int			first_to_die;
	int			life_left;
	channel_t	*ch;

// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = INT_MAX;

	for (ch_idx = NUM_AMBIENTS, ch = &channels[NUM_AMBIENTS]; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, ch++)
	{
		if (entchannel != 0		// channel 0 never overrides
			&& ch->entnum == entnum
			&& (ch->entchannel == entchannel || entchannel == -1))
		{
			// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (ch->entnum != cl_viewentity || cl_viewentity == entnum || !ch->sfx)
		{
			if (life_left > ch->end - paintedtime)
			{
				life_left = ch->end - paintedtime;
				first_to_die = ch_idx;
			}
		}
	}

	if (first_to_die == -1)
		return NULL;

	ch = &channels[first_to_die];
	if (ch->sfx)
		ch->sfx = NULL;

	return ch;
}

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize(channel_t *ch)
{
	vec_t	dot;
	vec_t	dist;
	vec_t	lscale, rscale, scale;
	vec3_t	source_vec;

// anything coming from the view entity will always be full volume
	if (ch->entnum == cl_viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
	}
	else
	{
	// calculate stereo separation and distance attenuation
		VectorSubtract(ch->origin, listener_origin, source_vec);

		dist = VectorNormalize(source_vec) * ch->dist_mult;

		if (shm->channels == 1)
		{
			rscale = 1.0f;
			lscale = 1.0f;
		}
		else
		{
			dot = DotProduct(listener_right, source_vec);

			rscale = 1.0f + dot;
			lscale = 1.0f - dot;
		}

	// add in distance effect
		scale = (1.0f - dist) * (float)ch->master_vol;
		ch->rightvol = (int)(rscale * scale);
		if (ch->rightvol < 0)
			ch->rightvol = 0;

		ch->leftvol = (int)(lscale * scale);
		if (ch->leftvol < 0)
			ch->leftvol = 0;
	}
}

// =======================================================================
// Start a sound effect
// =======================================================================

/*
=================
S_StartSound
=================
*/
void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t	*target_chan, *check;
	sfxcache_t	*sc;
	int			vol;
	int			ch_idx;
	int			skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (nosound.value != 0.0f)
		return;

	vol = (int)(fvol * 255.0f);

// pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

// spatialize
	memset(target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;		// not audible at all

// new channel
	sc = S_LoadSound(sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0;
	target_chan->end = paintedtime + sc->length;

// if an identical sound has also been started this frame, offset the pos
// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = rand() % (int)(shm->speed * 0.1f);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
}

/*
=================
S_StaticSound
=================
*/
void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t	*ss;
	sfxcache_t	*sc;

	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Con_Printf("total_channels == MAX_CHANNELS\n");
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	sc = S_LoadSound(sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Con_Printf("Sound %s not looped\n", sfx->name);
		return;
	}

	ss->sfx = sfx;
	ss->end = paintedtime + sc->length;
	ss->master_vol = (int)vol;
	ss->dist_mult = attenuation / 64.0f / sound_nominal_clip_dist;
	VectorCopy(origin, ss->origin);

	SND_Spatialize(ss);
}

/*
=================
S_StopSound
=================
*/
void S_StopSound(int entnum, int entchannel)
{
	int			i;
	channel_t	*ch;

	for (i = 0, ch = channels; i < total_channels; i++, ch++)
	{
		if (ch->entnum == entnum && ch->entchannel == entchannel)
		{
			ch->end = 0;
			ch->sfx = NULL;
			return;
		}
	}
}

/*
=================
S_StopAllSounds
=================
*/
void S_StopAllSounds(qboolean clear)
{
	int			i;
	channel_t	*ch;

	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i = 0, ch = channels; i < MAX_CHANNELS; i++, ch++)
	{
		if (ch->sfx)
			ch->sfx = NULL;
	}

	memset(channels, 0, sizeof(channels));

	if (clear)
		S_ClearBuffer();
}

void S_StopAllSoundsC(void)
{
	S_StopAllSounds(true);
}

/*
=================
S_ClearBuffer
=================
*/
void S_ClearBuffer(void)
{
	int			clear;
	void		*pData;
	DWORD		dwSize;
	int			reps;
	HRESULT		hresult;

	if (!sound_started || !shm)
		return;

	if (!shm->buffer && !pDSBuf)
		return;

	if (shm->samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	if (pDSBuf)
	{
		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pData, &dwSize, NULL, NULL, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf("S_ClearBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				return;
			}

			if (++reps > DS_LOCK_RETRIES)
			{
				Con_Printf("S_ClearBuffer: DS: couldn't restore buffer\n");
				S_Shutdown();
				return;
			}
		}

		memset(pData, clear, shm->samples * shm->samplebits / 8);

		pDSBuf->lpVtbl->Unlock(pDSBuf, pData, dwSize, NULL, 0);
	}
	else
	{
		memset(shm->buffer, clear, shm->samples * shm->samplebits / 8);
	}
}

/*
=================
S_GetSoundtime
=================
*/
void S_GetSoundtime(void)
{
	int		samplepos;
	int		fullsamples;

	fullsamples = shm->samples / shm->channels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update. Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		soundtime_buffers++;		// buffer wrapped

		if (paintedtime > 0x40000000)
		{
			// time to chop things off to avoid 32 bit limits
			paintedtime = fullsamples;
			soundtime_buffers = 0;
			S_StopAllSounds(true);
		}
	}

	oldsamplepos = samplepos;

	soundtime = soundtime_buffers * fullsamples + samplepos / shm->channels;
}

/*
===================
S_UpdateAmbientSounds
===================
*/
static void S_UpdateAmbientSounds(void)
{
	int			ambient_channel;
	mleaf_t		*l;
	channel_t	*chan;
	float		vol;

	if (snd_ambient && cl_worldmodel)
	{
	// calc ambient sound levels
		l = Mod_PointInLeaf(listener_origin, cl_worldmodel);
		if (l && ambient_level.value != 0.0f)
		{
			chan = channels;
			for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++, chan++)
			{
				chan->sfx = ambient_sfx[ambient_channel];

				vol = (float)l->ambient_sound_level[ambient_channel] * ambient_level.value;
				if (vol < AMBIENT_MINVOL)
					vol = 0.0f;

			// don't adjust volume too fast
				if (vol > (float)chan->master_vol)
				{
					chan->master_vol = (int)((float)chan->master_vol + host_frametime * ambient_fade.value);
					if ((float)chan->master_vol > vol)
						chan->master_vol = (int)vol;
				}
				else if (vol < (float)chan->master_vol)
				{
					chan->master_vol = (int)((float)chan->master_vol - host_frametime * ambient_fade.value);
					if ((float)chan->master_vol < vol)
						chan->master_vol = (int)vol;
				}

				chan->leftvol = chan->master_vol;
				chan->rightvol = chan->master_vol;
			}
			return;
		}

		chan = channels;
		for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++, chan++)
			chan->sfx = NULL;
	}
}

static void S_Update_(void);

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i, j;
	int			total;
	channel_t	*ch;
	channel_t	*combine;

	if (!sound_started || snd_blocked > 0)
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

// update general area ambient sound sources
	S_UpdateAmbientSounds();

	combine = NULL;

// update spatialization for static and dynamic sounds
	for (i = NUM_AMBIENTS; i < total_channels; i++)
	{
		ch = &channels[i];
		if (!ch->sfx)
			continue;

		SND_Spatialize(ch);		// respatialize channel

		if (!ch->leftvol && !ch->rightvol)
			continue;

		if (i < MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
			continue;

	// try to combine static sounds with a previous channel of the same
	// sound effect so we don't mix five torches every frame

		// see if it can just use the last one
		if (combine && combine->sfx == ch->sfx)
		{
			combine->leftvol += ch->leftvol;
			combine->rightvol += ch->rightvol;
			ch->leftvol = 0;
			ch->rightvol = 0;
			continue;
		}

		// search for one
		combine = &channels[MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS];
		for (j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; j < i; j++, combine++)
		{
			if (combine->sfx == ch->sfx)
				break;
		}

		if (j == i)
		{
			combine = NULL;
			continue;
		}

		if (combine != ch)
		{
			combine->leftvol += ch->leftvol;
			combine->rightvol += ch->rightvol;
			ch->leftvol = 0;
			ch->rightvol = 0;
		}
	}

//
// debugging output
//
	if (snd_show.value)
	{
		total = 0;
		for (i = 0, ch = channels; i < total_channels; i++, ch++)
		{
			if (ch->sfx && (ch->leftvol || ch->rightvol))
				total++;
		}

		Con_Printf("----(%i)----\n", total);
	}

// mix some sound
	S_Update_();
}

static void S_Update_(void)
{
	int		endtime;
	int		samps;
	DWORD	dwStatus;

	if (!sound_started || snd_blocked > 0)
		return;

// updates DMA time
	S_GetSoundtime();

// check to make sure that we haven't overshot
	if (soundtime > paintedtime)
		paintedtime = soundtime;

// mix ahead of current position
	endtime = (int)((float)shm->speed * snd_mixahead.value + (float)soundtime);
	samps = (unsigned int)shm->samples >> (shm->channels - 1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

// if the buffer was lost or stopped, restore it and/or restart it
	if (pDSBuf)
	{
		if (pDSBuf->lpVtbl->GetStatus(pDSBuf, &dwStatus) != DS_OK)
			Con_Printf("Couldn't get sound buffer status\n");

		if (dwStatus & DSBSTATUS_BUFFERLOST)
			pDSBuf->lpVtbl->Restore(pDSBuf);

		if (!(dwStatus & DSBSTATUS_PLAYING))
			pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
	}

	S_PaintChannels(endtime);

	SNDDMA_Submit();
}

void S_ExtraUpdate(void)
{
	IN_Accumulate();

	if (snd_noextraupdate.value)
		return;		// don't pollute timings

	S_Update_();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_LocalSound(const char *sound)
{
	sfx_t	*sfx;

	if (nosound.value != 0.0f)
		return;
	if (!sound_started)
		return;

	sfx = S_PrecacheSound(sound);
	if (!sfx)
	{
		Con_Printf("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound(cl_viewentity, -1, sfx, listener_origin, 1.0f, 1.0f);
}

void S_Play(void)
{
	static int	hash = 345;
	int			i;
	char		name[256];
	sfx_t		*sfx;

	i = 1;
	while (i < Cmd_Argc())
	{
		if (Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strcpy(name, Cmd_Argv(i));
		}
		else
		{
			Q_strcpy(name, Cmd_Argv(i));
			Q_strcat(name, ".wav");
		}
		i++;
		sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0f, 1.0f);
	}
}

void S_PlayVol(void)
{
	static int	hash = 543;
	int			i;
	float		vol;
	char		name[256];
	sfx_t		*sfx;

	i = 1;
	while (i < Cmd_Argc())
	{
		if (Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strcpy(name, Cmd_Argv(i));
		}
		else
		{
			Q_strcpy(name, Cmd_Argv(i));
			Q_strcat(name, ".wav");
		}
		sfx = S_PrecacheSound(name);
		vol = Q_atof(Cmd_Argv(i + 1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 1.0f);
		i += 2;
	}
}

void S_SoundList(void)
{
	int			i;
	sfx_t		*sfx;
	sfxcache_t	*sc;
	int			size, total;

	total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++)
	{
		sc = (sfxcache_t *)Cache_Check(&sfx->cache);
		if (!sc)
			continue;
		size = sc->length * sc->width * (sc->stereo + 1);
		total += size;
		if (sc->loopstart < 0)
			Con_Printf(" ");
		else
			Con_Printf("L");
		Con_Printf("(%2db) %6i : %s\n", sc->width * 8, size, sfx->name);
	}
	Con_Printf("Total resident: %i\n", total);
}

void Sound_CacheReport(void)
{
	float	cachesize;

	cachesize = (float)(g_HunkSize - g_HunkLowUsed - g_HunkHighUsed);
	Con_Printf("%4.1f megabyte data cache\n", cachesize / (1024.0f * 1024.0f));
}

void S_AmbientOff(void)
{
	snd_ambient = false;
}

/*
==================
S_InsertText

Touches a precached sound so it stays in the cache.
==================
*/
void S_InsertText(const char *text)
{
	sfx_t	*sfx;

	if (!sound_started)
		return;

	sfx = S_FindName(text);
	Cache_Check(&sfx->cache);
}
