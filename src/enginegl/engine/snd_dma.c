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
#include <dsound.h>

extern int		SNDDMA_Init(void);
extern void		SNDDMA_Shutdown(void);
extern int		SNDDMA_GetDMAPos(void);
extern void		SNDDMA_Submit(void);
extern int		S_PaintChannels(int endtime);
extern void		SND_InitScaletable(void);
extern int		SX_Init(void);
extern sfxcache_t	*S_LoadSound(sfx_t *s);
extern BOOL		IN_Accumulate(void);
extern LPDIRECTSOUNDBUFFER pDSBuf;
extern int		gSndBufSize;
extern model_t		*cl_worldmodel;
extern double		host_frametime;

extern void		S_StopAllSoundsC(void);

void			S_Play(void);
void			S_PlayVol(void);
void			S_SoundList(void);
void			S_ClearBuffer(void);

channel_t		g_Channels[MAX_CHANNELS];
int				g_TotalChannels = 12;

static sfx_t	*g_AmbientSfx[NUM_AMBIENTS];

qboolean		g_SndInitialized = false;
qboolean		g_SoundStarted = false;
int				g_SndBlocked = 0;
int				g_SndAmbient = 0;

qboolean		g_FakeDMA = false;

volatile dma_t	*shm = NULL;
volatile dma_t	sn;

vec3_t			g_ListenerOrigin;
vec3_t			g_ListenerForward;
vec3_t			g_ListenerRight;
vec3_t			g_ListenerUp;
float			g_SoundNominalClipDist = 1000.0f;

int				g_SoundTime = 0;
int				g_PaintedTime = 0;
int				g_SoundTimeWraps = 0;
int				g_LastDMAPos = 0;

sfx_t			*g_KnownSfx = NULL;
int				g_NumSfx = 0;

int				g_FrameCount = 0;

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

void S_SoundInfo_f(void)
{
	if (!g_SoundStarted || !shm)
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
	Con_Printf("%5d total_channels\n", g_TotalChannels);
}

void S_Startup(void)
{
	if (g_SndInitialized)
		g_SoundStarted = g_FakeDMA || SNDDMA_Init();
}

void S_AmbientOn(void)
{
	g_SndAmbient = 1;
}

//=========================================================
// S_AmbientOff_Null - empty stub
//=========================================================
void S_AmbientOff_Null(void)
{
}

//=========================================================
// S_AmbientOn_Null - empty stub
//=========================================================
void S_AmbientOn_Null(void)
{
}

int S_Init(void)
{
	dma_t *shm_temp;
	int nosound_parm;

	Con_DPrintf("\nSound Initialization\n");

	nosound_parm = COM_CheckParm("-nosound");
	if (nosound_parm)
		return nosound_parm;

	if (COM_CheckParm("-simsound"))
		g_FakeDMA = true;

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

	if (host_parms.memsize < 0x800000)
	{
		Cvar_Set("loadas8bit", "1");
		Con_DPrintf("loading all sounds as 8bit\n");
	}

	g_SndInitialized = true;

	S_Startup();

	SND_InitScaletable();

	g_KnownSfx = (sfx_t *)Hunk_AllocName(MAX_SFX * sizeof(sfx_t), "sfx_t");
	g_NumSfx = 0;

	if (g_FakeDMA)
	{
		shm_temp = (dma_t *)Hunk_AllocName(sizeof(*shm_temp), "shm");
		shm = shm_temp;
		shm->splitbuffer = 0;
		shm->samplebits = 16;
		shm->speed = 22050;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = 1;
		shm->gamealive = 1;
		shm->submission_chunk = 1;
		shm->buffer = (unsigned char *)Hunk_AllocName(0x10000, "shmbuf");
	}

	Con_DPrintf("Sound sampling rate: %i\n", shm->speed);

	S_StopAllSounds(true);

	return SX_Init();
}

void S_Shutdown(void)
{
	if (!g_SoundStarted)
		return;

	if (shm)
		shm->gamealive = 0;

	shm = NULL;
	g_SoundStarted = false;

	if (!g_FakeDMA)
		SNDDMA_Shutdown();
}

sfx_t *S_FindName(const char *name)
{
	int		i;
	sfx_t	*sfx;

	if (!name)
		Sys_Error("S_FindName: NULL\n");

	if (strlen(name) >= MAX_QPATH)
		Sys_Error("Sound name too long: %s", name);

	for (i = 0; i < g_NumSfx; i++)
	{
		if (!strcmp(g_KnownSfx[i].name, name))
			return &g_KnownSfx[i];
	}

	if (g_NumSfx == MAX_SFX)
		Sys_Error("S_FindName: out of sfx_t");

	sfx = &g_KnownSfx[i];
	strcpy(sfx->name, name);

	g_NumSfx++;

	return sfx;
}

sfx_t *S_PrecacheSound(const char *name)
{
	sfx_t	*sfx;

	if (!g_SoundStarted || nosound.value != 0.0f)
		return NULL;

	sfx = S_FindName(name);

	if (precache.value != 0.0f)
		S_LoadSound(sfx);

	return sfx;
}

channel_t *SND_PickChannel(int entnum, int entchannel)
{
	int			i;
	int			first_to_die;
	int			life_left;
	channel_t	*ch;

	first_to_die = -1;
	life_left = 0x7FFFFFFF;

	for (i = 4, ch = &g_Channels[4]; i < 12; i++, ch++)
	{

		if (entchannel != 0 && ch->entnum == entnum &&
			(ch->entchannel == entchannel || entchannel == -1))
		{

			first_to_die = i;
			break;
		}

		if (ch->entnum != cl_viewentity || cl_viewentity == entnum || !ch->sfx)
		{
			if (life_left > ch->end - g_SoundTime)
			{
				life_left = ch->end - g_SoundTime;
				first_to_die = i;
			}
		}
	}

	if (first_to_die == -1)
		return NULL;

	ch = &g_Channels[first_to_die];
	if (ch->sfx)
		ch->sfx = NULL;

	return ch;
}

void SND_Spatialize(channel_t *ch)
{
	vec_t	dot;
	vec_t	dist;
	vec_t	lscale, rscale, scale;
	vec3_t	source_vec;

	if (ch->entnum == cl_viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
	}
	else
	{

		VectorSubtract(ch->origin, g_ListenerOrigin, source_vec);

		dist = VectorNormalize(source_vec) * ch->dist_mult;

		if (shm->channels == 1)
		{
			rscale = 1.0f;
			lscale = 1.0f;
		}
		else
		{

			dot = DotProduct(g_ListenerRight, source_vec);

			rscale = 1.0f + dot;
			lscale = 1.0f - dot;
		}

		scale = (1.0f - dist) * (float)ch->master_vol;
		ch->rightvol = (int)(rscale * scale);
		if (ch->rightvol < 0)
			ch->rightvol = 0;

		ch->leftvol = (int)(lscale * scale);
		if (ch->leftvol < 0)
			ch->leftvol = 0;
	}
}

void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t	*target_chan, *check;
	sfxcache_t	*sc;
	int			vol;
	int			ch_idx;
	int			skip;

	if (!g_SoundStarted)
		return;

	if (!sfx)
		return;

	if (nosound.value != 0.0f)
		return;

	vol = (int)(fvol * 255.0f);

	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

	memset(target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / g_SoundNominalClipDist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;

	sc = (sfxcache_t *)S_LoadSound(sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return;
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0;
	target_chan->end = g_SoundTime + sc->length;

	check = &g_Channels[4];
	for (ch_idx = 4; ch_idx < 12; ch_idx++, check++)
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

void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t *ch;
	sfxcache_t *sc;

	if (!sfx)
		return;

	if (g_TotalChannels == MAX_CHANNELS)
	{
		Con_Printf("total_channels == MAX_CHANNELS\n");
		return;
	}

	ch = &g_Channels[g_TotalChannels++];

	sc = (sfxcache_t *)S_LoadSound(sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Con_Printf("Sound %s not looped\n", sfx->name);
		return;
	}

	ch->sfx = sfx;
	ch->end = g_SoundTime + sc->length;
	ch->master_vol = (int)vol;
	ch->dist_mult = attenuation / 64.0f / g_SoundNominalClipDist;
	VectorCopy(origin, ch->origin);

	SND_Spatialize(ch);
}

void S_StopSound(int entnum, int entchannel)
{
	int		i;
	channel_t *ch;

	for (i = 0, ch = g_Channels; i < g_TotalChannels; i++, ch++)
	{
		if (ch->entnum == entnum && ch->entchannel == entchannel)
		{
			ch->end = 0;
			ch->sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds(qboolean clear)
{
	int		i;
	channel_t *ch;

	if (!g_SoundStarted)
		return;

	g_TotalChannels = 12;

	for (i = 0, ch = g_Channels; i < MAX_CHANNELS; i++, ch++)
	{
		if (ch->sfx)
			ch->sfx = NULL;
	}

	memset(g_Channels, 0, sizeof(g_Channels));

	if (clear)
		S_ClearBuffer();
}

void S_StopAllSoundsC(void)
{
	S_StopAllSounds(true);
}

void S_ClearBuffer(void)
{
	int			clear;
	void		*pData;
	DWORD		dwSize;
	int			retry_count;
	HRESULT		hr;

	if (!g_SoundStarted || !shm)
		return;

	if (shm->buffer || pDSBuf)
	{
		if (shm->samplebits == 8)
			clear = 0x80;
		else
			clear = 0;

		if (pDSBuf)
		{
			retry_count = 0;
			while (1)
			{
				hr = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pData, &dwSize, NULL, NULL, 0);
				if (hr == DS_OK)
					break;

				if (hr != DSERR_BUFFERLOST)
				{
					Con_Printf("S_ClearBuffer: DS::Lock Sound Buffer Failed\n");
					S_Shutdown();
					return;
				}

				if (++retry_count > 10000)
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
}

void S_GetSoundtime(void)
{
	int		samplepos;
	int		fullsamples;

	fullsamples = shm->samples / shm->channels;

	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < g_LastDMAPos)
	{
		g_SoundTimeWraps++;

		if (g_SoundTime > 0x40000000)
		{

			g_SoundTime = fullsamples;
			g_SoundTimeWraps = 0;
			S_StopAllSounds(true);
		}
	}

	g_LastDMAPos = samplepos;

	g_PaintedTime = g_SoundTimeWraps * fullsamples + samplepos / shm->channels;
}

static void S_UpdateAmbientSounds(void)
{
	int i;
	mleaf_t *leaf;
	channel_t *ch;
	float target_vol;

	if (g_SndAmbient && cl_worldmodel)
	{
		leaf = Mod_PointInLeaf(g_ListenerOrigin, cl_worldmodel);
		if (leaf && ambient_level.value != 0.0f)
		{
			ch = g_Channels;
			for (i = 0; i < NUM_AMBIENTS; ++i, ++ch)
			{
				ch->sfx = g_AmbientSfx[i];

				target_vol = (float)leaf->ambient_sound_level[i] * ambient_level.value;
				if (target_vol < 8.0f)
					target_vol = 0.0f;

				if (target_vol > (float)ch->master_vol)
				{
					ch->master_vol = (int)((float)ch->master_vol + host_frametime * ambient_fade.value);
					if ((float)ch->master_vol > target_vol)
						ch->master_vol = (int)target_vol;
				}
				else if (target_vol < (float)ch->master_vol)
				{
					ch->master_vol = (int)((float)ch->master_vol - host_frametime * ambient_fade.value);
					if ((float)ch->master_vol < target_vol)
						ch->master_vol = (int)target_vol;
				}

				ch->leftvol = ch->master_vol;
				ch->rightvol = ch->master_vol;
			}

			return;
		}

		ch = g_Channels;
		for (i = 0; i < NUM_AMBIENTS; ++i, ++ch)
			ch->sfx = NULL;
	}
}

static void S_Update_(void);

void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int i;
	int j;
	int total;
	channel_t *ch;
	channel_t *combine;

	if (!g_SoundStarted || g_SndBlocked > 0)
		return;

	VectorCopy(origin, g_ListenerOrigin);
	VectorCopy(forward, g_ListenerForward);
	VectorCopy(right, g_ListenerRight);
	VectorCopy(up, g_ListenerUp);

	S_UpdateAmbientSounds();

	combine = NULL;
	for (i = 4; i < g_TotalChannels; ++i)
	{
		ch = &g_Channels[i];
		if (!ch->sfx)
			continue;

		SND_Spatialize(ch);

		if (!ch->leftvol && !ch->rightvol)
			continue;

		if (i < 12)
			continue;

		if (combine && combine->sfx == ch->sfx)
		{
			combine->leftvol += ch->leftvol;
			combine->rightvol += ch->rightvol;
			ch->leftvol = 0;
			ch->rightvol = 0;
			continue;
		}

		combine = &g_Channels[12];
		for (j = 12; j < i; ++j, ++combine)
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

	if (snd_show.value)
	{
		total = 0;
		ch = g_Channels;
		for (i = 0; i < g_TotalChannels; ++i, ++ch)
		{
			if (ch->sfx && (ch->leftvol || ch->rightvol))
				total++;
		}

		Con_Printf("----(%i)----\n", total);
	}

	S_Update_();
}

static void S_Update_(void)
{
	int endtime;
	unsigned int fullsamples;
	DWORD status;

	if (!g_SoundStarted || g_SndBlocked > 0)
		return;

	S_GetSoundtime();

	if (g_PaintedTime > g_SoundTime)
		g_SoundTime = g_PaintedTime;

	endtime = (int)((float)shm->speed * snd_mixahead.value + (float)g_PaintedTime);

	fullsamples = (unsigned int)shm->samples >> (shm->channels - 1);
	if (endtime - g_PaintedTime > (int)fullsamples)
		endtime = g_PaintedTime + (int)fullsamples;

	if (pDSBuf)
	{
		if (pDSBuf->lpVtbl->GetStatus(pDSBuf, &status))
			Con_Printf("Couldn't get sound buffer status\n");

		if ((status & DSBSTATUS_BUFFERLOST) != 0)
			pDSBuf->lpVtbl->Restore(pDSBuf);

		if ((status & DSBSTATUS_PLAYING) == 0)
			pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
	}

	S_PaintChannels(endtime);
	SNDDMA_Submit();
}

void S_ExtraUpdate(void)
{
	IN_Accumulate();

	if (snd_noextraupdate.value)
		return;

	S_Update_();
}

void S_LocalSound(const char *sound)
{
	sfx_t	*sfx;

	if (nosound.value != 0.0f || !g_SoundStarted)
		return;

	sfx = S_PrecacheSound(sound);
	if (!sfx)
	{
		Con_Printf("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound(cl_viewentity, -1, sfx, g_ListenerOrigin, 1.0f, 1.0f);
}

void S_Play(void)
{
	static int hash = 345;
	int i;
	char name[256];
	sfx_t *sfx;

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
		++i;
		sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, g_ListenerOrigin, 1.0f, 1.0f);
	}
}

void S_PlayVol(void)
{
	static int hash = 543;
	int i;
	char name[256];
	sfx_t *sfx;
	float vol;

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
		S_StartSound(hash++, 0, sfx, g_ListenerOrigin, vol, 1.0f);
		i += 2;
	}
}

void S_SoundList(void)
{
	int i;
	sfx_t *sfx;
	sfxcache_t *sc;
	int size;
	int total;

	total = 0;
	sfx = g_KnownSfx;
	for (i = 0; i < g_NumSfx; i++, sfx++)
	{
		sc = (sfxcache_t *)Cache_Check(&sfx->cache);
		if (sc)
		{
			size = sc->length * sc->width * (sc->stereo + 1);
			total += size;
			if ((int)sc->loopstart < 0)
				Con_Printf(" ");
			else
				Con_Printf("L");
			Con_Printf("(%2db) %6i : %s\n", 8 * sc->width, size, sfx->name);
		}
	}
	Con_Printf("Total resident: %i\n", total);
}

void Sound_CacheReport(void)
{
	float cacheSize;

	cacheSize = (float)(g_HunkSize - g_HunkLowUsed - g_HunkHighUsed);
	Con_Printf("%4.1f megabyte data cache\n", cacheSize / 1048576.0f);
}

void S_AmbientOff(void)
{
	g_SndAmbient = 0;
}

void S_InsertText(const char *text)
{
	sfx_t *sfx;

	if (g_SoundStarted)
	{
		sfx = S_FindName(text);
		Cache_Check(&sfx->cache);
	}
}
