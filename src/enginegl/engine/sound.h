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
// sound.h -- client sound i/o functions

#ifndef SOUND_H
#define SOUND_H

#define	MAX_CHANNELS			128
#define	MAX_DYNAMIC_CHANNELS	8
#define	MAX_SFX					512

// channels[0] to channels[NUM_AMBIENTS-1] are the ambient sounds,
// the next MAX_DYNAMIC_CHANNELS are entity sounds and the rest up to
// total_channels are static sounds

typedef struct
{
	int			left;
	int			right;
} portable_samplepair_t;

typedef struct sfx_s
{
	char		name[MAX_QPATH];
	cache_user_t	cache;
} sfx_t;

typedef struct sfxcache_s
{
	int			length;
	int			loopstart;
	int			speed;
	int			width;
	int			stereo;
	byte		data[1];		// variable sized
} sfxcache_t;

typedef struct dma_s
{
	qboolean	gamealive;
	qboolean	soundalive;
	qboolean	splitbuffer;
	int			channels;
	int			samples;			// mono samples in buffer
	int			submission_chunk;	// don't mix less than this #
	int			samplepos;			// in mono samples
	int			samplebits;
	int			speed;
	unsigned char	*buffer;
} dma_t;

typedef struct
{
	sfx_t		*sfx;			// sfx number
	int			leftvol;		// 0-255 volume
	int			rightvol;		// 0-255 volume
	int			end;			// end time in global paintsamples
	int			pos;			// sample position in sfx
	int			looping;
	int			entnum;			// to allow overriding a specific sound
	int			entchannel;
	vec3_t		origin;			// origin of sound effect
	vec_t		dist_mult;		// distance multiplier (attenuation/clipK)
	int			master_vol;		// 0-255 master volume
} channel_t;

typedef struct
{
	int			rate;
	int			width;
	int			channels;
	int			loopstart;
	int			samples;
	int			dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

// snd_dma.c
void S_Init(void);
void S_Startup(void);
void S_Shutdown(void);
void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation);
void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_StopSound(int entnum, int entchannel);
void S_StopAllSounds(qboolean clear);
void S_ClearBuffer(void);
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up);
void S_ExtraUpdate(void);
void S_AmbientOff(void);
void S_AmbientOn(void);
void S_LocalSound(const char *sound);
void S_InsertText(const char *text);
sfx_t *S_PrecacheSound(const char *sample);

// picks a channel based on priorities, empty slots, number of channels
channel_t *SND_PickChannel(int entnum, int entchannel);

// spatializes a channel
void SND_Spatialize(channel_t *ch);

// snd_mem.c
sfxcache_t *S_LoadSound(sfx_t *s);
wavinfo_t GetWavinfo(char *name, byte *wav, int wavlength);

// snd_mix.c
void S_PaintChannels(int endtime);
void SND_InitScaletable(void);
void SX_Init(void);

// snd_dsp.c
extern cvar_t room_delay;
extern cvar_t room_feedback;
extern cvar_t room_dlylp;
extern cvar_t room_size;
extern cvar_t room_refl;
extern cvar_t room_rvblp;
extern cvar_t room_left;
extern cvar_t room_lp;
extern cvar_t room_mod;
extern cvar_t room_type;
extern cvar_t room_off;

// snd_win.c
void S_BlockSound(void);
void S_UnblockSound(void);
void S_BeginPrecaching(void);
void S_EndPrecaching(void);

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int SNDDMA_GetDMAPos(void);

// sends the mixed samples to the device (waveOut only)
void SNDDMA_Submit(void);

// shutdown the DMA xfer
void SNDDMA_Shutdown(void);

// cd_win.c
int CDAudio_Init(void);
void CDAudio_Play(byte track, qboolean looping);
void CDAudio_Stop(void);
void CDAudio_Pause(void);
void CDAudio_Resume(void);
void CDAudio_Shutdown(void);
void CDAudio_Update(void);

extern channel_t	channels[MAX_CHANNELS];
extern int			total_channels;

extern qboolean		fakedma;
extern int			paintedtime;
extern int			snd_blocked;

extern volatile dma_t	*shm;
extern volatile dma_t	sn;

extern cvar_t		loadas8bit;
extern cvar_t		bgmvolume;
extern cvar_t		volume;

#endif // SOUND_H
