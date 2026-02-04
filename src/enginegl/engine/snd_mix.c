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

#include <windows.h>
#include <dsound.h>
#include <stdlib.h>
#include <string.h>
#include "quakedef.h"

extern int				g_SoundTime;
extern int				g_TotalChannels;
extern channel_t		g_Channels[];

extern void				*pDSBuf;
extern int				gSndBufSize;

extern cvar_t			volume;

extern sfxcache_t		*S_LoadSound(sfx_t *s);
extern void				SX_RoomProcess(int count);
extern void				SX_RoomFX(int count);

#define PAINTBUFFER_SIZE	512

static int		paintbuffer[PAINTBUFFER_SIZE * 2 + 4];

static int		snd_scaletable[32][256];

static int		*snd_p;
static int		snd_linear_count;
static int		snd_vol;
static short	*snd_out;

static int		g_rgsxdly[4][15];

static float	g_sxdly_delay_lpf;
static float	g_sxrvb_roomsize_lpf;
static float	g_sxdly_stereo_lpf;
static float	g_last_room_type;

static int		g_room_lp_state[10];

static int		g_room_mod_current_left;
static int		g_room_mod_target_left;
static int		g_room_mod_current_right;
static int		g_room_mod_target_right;
static int		g_room_mod_rate_left;
static int		g_room_mod_rate_right;
static int		g_room_mod_counter_left;
static int		g_room_mod_counter_right;

static int		g_sxdly_lowpass;
static int		g_sxdly_feedbacklevel;

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

void SND_PaintChannelFrom8(channel_t *ch, sfxcache_t *sc, int count)
{
	unsigned int leftvol;
	unsigned int rightvol;
	const unsigned char *sfx;
	int *lscale;
	int *rscale;
	int *pb;
	int i;
	int sample;

	leftvol = (unsigned int)ch->leftvol;
	rightvol = (unsigned int)ch->rightvol;
	if (leftvol > 0xFF)
		leftvol = 0xFF;
	if (rightvol > 0xFF)
		rightvol = 0xFF;

	sfx = (const unsigned char *)sc + ch->pos + 20;
	ch->pos += count;

	lscale = snd_scaletable[(leftvol & 0xF8) >> 3];
	rscale = snd_scaletable[(rightvol & 0xF8) >> 3];

	pb = (int *)((unsigned char *)paintbuffer + 16);
	for (i = 0; i < count; ++i)
	{
		sample = *sfx++;
		pb[0] += lscale[sample];
		pb[1] += rscale[sample];
		pb += 2;
	}
}

void Snd_WriteLinearBlastStereo16(void)
{
	int i;
	int val_left, val_right;

	i = snd_linear_count;

	do
	{

		val_left = (snd_vol * snd_p[i - 2]) >> 8;

		if (val_left > 0x7FFF)
			val_left = 0x7FFF;
		else if (val_left < -32768)
			val_left = -32768;

		val_right = (snd_vol * snd_p[i - 1]) >> 8;

		if (val_right > 0x7FFF)
			val_right = 0x7FFF;
		else if (val_right < -32768)
			val_right = -32768;

		*(int *)(snd_out + i - 2) = (unsigned short)val_left | (val_right << 16);

		i -= 2;
	}
	while (i > 0);
}

void SND_InitScaletable(void)
{
	int		i, j;
	int		scale;

	scale = 0;
	for (i = 0; i < 32; i++)
	{
		for (j = 0; j < 256; j++)
		{
			snd_scaletable[i][j] = ((signed char)j) * scale * 8;
		}
		scale++;
	}
}

void SND_PaintChannelFrom16(channel_t *ch, sfxcache_t *sc, int count)
{
	int		leftvol, rightvol;
	short	*sfx;
	int		*pb;
	int		i;
	int		data;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	sfx = (short *)((unsigned char *)sc + 2 * ch->pos + 20);

	if (count > 0)
	{
		pb = (int *)((unsigned char *)paintbuffer + 16);
		i = count;
		do
		{
			data = *sfx++;
			*pb += (data * leftvol) >> 8;
			pb[1] += (data * rightvol) >> 8;
			pb += 2;
			i--;
		}
		while (i);
	}

	ch->pos += count;
}

int S_TransferStereo16(int endtime)
{
	int		lpaintedtime;
	int		*dsbuf_ptr;
	int		result;
	int		retry_count;
	int		buffer_offset;
	int		lock_size1, lock_size2;
	char	*lock_ptr1, *lock_ptr2;
	HRESULT hr;

	lpaintedtime = g_SoundTime;
	dsbuf_ptr = (int *)pDSBuf;

	snd_vol = (int)(volume.value * 256.0f);

	snd_p = (int *)((char *)paintbuffer + 16);

	if (dsbuf_ptr)
	{
		retry_count = 0;
		while (1)
		{

			hr = ((LPDIRECTSOUNDBUFFER)dsbuf_ptr)->lpVtbl->Lock(
					(LPDIRECTSOUNDBUFFER)dsbuf_ptr, 0, gSndBufSize,
					(void **)&lock_ptr1, (DWORD *)&lock_size1,
					(void **)&lock_ptr2, (DWORD *)&lock_size2, 0);

			dsbuf_ptr = (int *)pDSBuf;
			if (hr == 0)
				break;

			if (hr != DSERR_BUFFERLOST)
			{
				Con_Printf("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return 0;
			}

			if (++retry_count > 10000)
			{
				Con_Printf("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return 0;
			}
		}
	}
	else
	{
		result = (int)shm;
		lock_ptr1 = (char *)shm->buffer;
	}

	for ( ; endtime > lpaintedtime; dsbuf_ptr = (int *)pDSBuf)
	{

		buffer_offset = lpaintedtime & ((shm->samples >> 1) - 1);
		snd_out = (short *)(lock_ptr1 + 4 * buffer_offset);
		snd_linear_count = (shm->samples >> 1) - buffer_offset;

		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count *= 2;

		Snd_WriteLinearBlastStereo16();

		result = (int)snd_p + 4 * snd_linear_count;
		snd_p = (int *)result;
		lpaintedtime += snd_linear_count >> 1;
	}

	if (dsbuf_ptr)
	{
		return ((LPDIRECTSOUNDBUFFER)dsbuf_ptr)->lpVtbl->Unlock(
			(LPDIRECTSOUNDBUFFER)dsbuf_ptr, lock_ptr1, lock_size1, NULL, 0);
	}

	return result;
}

int S_TransferPaintBuffer(int endtime)
{
	int		result;
	int		num_samples;
	int		buffer_offset;
	int		buffer_mask;
	int		channel_shift;
	int		*paint_ptr;
	int		retry_count;
	int		*dsbuf_ptr;
	int		vol_scale;
	int		lock_size1, lock_size2;
	char	*lock_ptr1, *lock_ptr2;
	char	*write_ptr;
	int		val;
	HRESULT hr;

	if (shm->samplebits == 16 && shm->channels == 2)
		return S_TransferStereo16(endtime);

	paint_ptr = (int *)((char *)paintbuffer + 16);
	num_samples = shm->channels * (endtime - g_SoundTime);
	buffer_offset = (shm->samples - 1) & (g_SoundTime * shm->channels);
	buffer_mask = shm->samples - 1;
	channel_shift = 3 - shm->channels;
	dsbuf_ptr = (int *)pDSBuf;
	vol_scale = (int)(volume.value * 256.0f);

	if (dsbuf_ptr)
	{
		retry_count = 0;
		while (1)
		{

			hr = ((LPDIRECTSOUNDBUFFER)dsbuf_ptr)->lpVtbl->Lock(
					(LPDIRECTSOUNDBUFFER)dsbuf_ptr, 0, gSndBufSize,
					(void **)&lock_ptr1, (DWORD *)&lock_size1,
					(void **)&lock_ptr2, (DWORD *)&lock_size2, 0);

			dsbuf_ptr = (int *)pDSBuf;
			if (hr == 0)
				break;

			if (hr != DSERR_BUFFERLOST)
			{
				Con_Printf("S_TransferPaintBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return 0;
			}

			if (++retry_count > 10000)
			{
				Con_Printf("S_TransferPaintBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return 0;
			}
		}
	}
	else
	{
		result = (int)shm->buffer;
		lock_ptr1 = (char *)result;
	}

	if (shm->samplebits == 16)
	{

		write_ptr = lock_ptr1;
		result = num_samples--;
		if (result)
		{
			channel_shift *= 4;
			do
			{
				val = (vol_scale * *paint_ptr) >> 8;
				paint_ptr = (int *)((char *)paint_ptr + channel_shift);

				if (val > 0x7FFF)
					val = 0x7FFF;
				else if (val < -32768)
					val = -32768;

				*(short *)(write_ptr + 2 * buffer_offset) = (short)val;
				buffer_offset = buffer_mask & (buffer_offset + 1);
				result = num_samples--;
			}
			while (result);
		}
	}
	else if (shm->samplebits == 8)
	{

		write_ptr = lock_ptr1;
		result = num_samples--;
		if (result)
		{
			channel_shift *= 4;
			do
			{
				val = (vol_scale * *paint_ptr) >> 8;
				paint_ptr = (int *)((char *)paint_ptr + channel_shift);

				if (val > 0x7FFF)
					val = 0x7F00;
				else if (val < -32768)
					val = 0x8000;

				*(unsigned char *)(write_ptr + buffer_offset) = (val >> 8) + 0x80;
				buffer_offset = buffer_mask & (buffer_offset + 1);
				result = num_samples--;
			}
			while (result);
		}
	}

	if (dsbuf_ptr)
	{
		((LPDIRECTSOUNDBUFFER)dsbuf_ptr)->lpVtbl->Unlock(
			(LPDIRECTSOUNDBUFFER)dsbuf_ptr, lock_ptr1, lock_size1, NULL, 0);

		return ((LPDIRECTSOUNDBUFFER)pDSBuf)->lpVtbl->GetCurrentPosition(
			(LPDIRECTSOUNDBUFFER)pDSBuf, (LPDWORD)&paint_ptr, (LPDWORD)&num_samples);
	}

	return result;
}

int S_PaintChannels(int endtime)
{
	int result;
	int end_this_pass;
	int i;

	result = endtime;
	if (endtime <= g_SoundTime)
		return result;

	do
	{
		end_this_pass = endtime;
		if (endtime - g_SoundTime > PAINTBUFFER_SIZE)
			end_this_pass = g_SoundTime + PAINTBUFFER_SIZE;

		Q_memset((unsigned char *)paintbuffer + 16, 0, 8 * (end_this_pass - g_SoundTime));

		for (i = 0; i < g_TotalChannels; ++i)
		{
			channel_t *ch;
			sfxcache_t *sc;
			int end_pos;

			ch = &g_Channels[i];
			if (!ch->sfx || (!ch->leftvol && !ch->rightvol))
				continue;

			sc = S_LoadSound(ch->sfx);
			if (!sc)
				continue;

			end_pos = g_SoundTime;
			while (end_this_pass > end_pos)
			{
				int end;
				int count;

				end = ch->end;
				if (end_this_pass <= end)
					end = end_this_pass;

				count = end - end_pos;
				if (count > 0)
				{
					if (sc->width == 1)
						SND_PaintChannelFrom8(ch, sc, count);
					else
						SND_PaintChannelFrom16(ch, sc, count);

					end_pos += count;
				}

				if (ch->end <= end_pos)
				{
					int loop_start;

					loop_start = sc->loopstart;
					if (loop_start < 0)
					{
						ch->sfx = NULL;
						break;
					}

					ch->pos = loop_start;
					ch->end = end_pos + sc->length - loop_start;
				}
			}
		}

		SX_RoomProcess(end_this_pass - g_SoundTime);
		result = S_TransferPaintBuffer(end_this_pass);
		g_SoundTime = end_this_pass;
	}
	while (end_this_pass < endtime);

	return result;
}

int SX_Init(void)
{
	int		sample_rate;

	Q_memset(&g_rgsxdly, 0, 240);
	Q_memset(&g_room_lp_state, 0, 40);

	g_sxdly_delay_lpf = -1.0f;
	g_sxrvb_roomsize_lpf = -1.0f;
	g_last_room_type = -1.0f;
	g_sxdly_stereo_lpf = -1.0f;

	g_room_mod_current_right = 255;
	g_room_mod_current_left = 255;
	g_room_mod_target_right = 255;
	g_room_mod_target_left = 255;

	sample_rate = shm->speed;
	g_room_mod_rate_left = 350 * (sample_rate / 11025);
	g_room_mod_counter_left = 350 * (sample_rate / 11025);
	g_room_mod_rate_right = 450 * (sample_rate / 11025);
	g_room_mod_counter_right = 450 * (sample_rate / 11025);

	Con_DPrintf("FX Processor Init\n");

	Cvar_RegisterVariable(&room_delay);
	Cvar_RegisterVariable(&room_feedback);
	Cvar_RegisterVariable(&room_dlylp);
	Cvar_RegisterVariable(&room_size);
	Cvar_RegisterVariable(&room_refl);
	Cvar_RegisterVariable(&room_rvblp);
	Cvar_RegisterVariable(&room_left);
	Cvar_RegisterVariable(&room_lp);
	Cvar_RegisterVariable(&room_mod);
	Cvar_RegisterVariable(&room_type);
	Cvar_RegisterVariable(&room_off);
	return 1;
}

int SXDLY_Init(int delay_index, float delay_time)
{
	int		*dly;
	int		delay_samples;
	int		alloc_size;
	HGLOBAL	hmem;
	void	*locked_mem;

	dly = &g_rgsxdly[delay_index][0];

	if (delay_time > 0.4f)
		delay_time = 0.4f;

	if (dly[14])
	{
		GlobalUnlock((HGLOBAL)dly[13]);
		GlobalFree((HGLOBAL)dly[13]);
		dly[13] = 0;
		dly[14] = 0;
	}

	if (delay_time == 0.0f)
		return 1;

	delay_samples = (int)((float)shm->speed * delay_time + 1.0f);
	*dly = delay_samples;
	alloc_size = 2 * delay_samples;

	hmem = GlobalAlloc(0x2002, alloc_size);
	if (!hmem)
	{
		Con_Printf("Sound FX: Out of memory.\n");
		return 0;
	}

	locked_mem = GlobalLock(hmem);
	if (!locked_mem)
	{
		Con_Printf("Sound FX: Failed to lock delay buffer memory.\n");
		GlobalFree(hmem);
		return 0;
	}

	memset(locked_mem, 0, alloc_size);

	dly[13] = (int)hmem;
	dly[14] = (int)locked_mem;
	dly[2] = 0;
	dly[3] = *dly - dly[6];
	dly[5] = 0;
	dly[11] = 0;
	dly[1] = 1;
	dly[12] = 0;
	dly[10] = 0;
	dly[9] = 0;
	dly[8] = 0;

	return 1;
}

int SXDLY_Free(int delay_index)
{
	int		result;
	int		*dly;

	result = 60 * delay_index;
	dly = &g_rgsxdly[delay_index][0];

	if (dly[14])
	{
		GlobalUnlock((HGLOBAL)dly[13]);
		result = (int)GlobalFree((HGLOBAL)dly[13]);
		dly[13] = 0;
		dly[14] = 0;
	}

	return result;
}

void SXDLY_CheckNewStereoDelayVal(void)
{
	int		delay_samples;
	float	delay_time;
	float	clamped_delay;
	int		read_offset;

	if (g_sxdly_stereo_lpf != room_left.value)
	{
		delay_time = room_left.value;

		if (delay_time == 0.0f)
		{

			SXDLY_Free(3);
			g_sxdly_stereo_lpf = 0.0f;
		}
		else
		{

			clamped_delay = delay_time;
			if (delay_time >= 0.1f)
				clamped_delay = 0.1f;

			delay_samples = (int)((double)shm->speed * clamped_delay);

			if (!g_rgsxdly[3][14])
			{
				g_rgsxdly[3][6] = delay_samples;
				SXDLY_Init(3, 0.1f);
			}

			if (g_rgsxdly[3][6] != delay_samples)
			{
				read_offset = g_rgsxdly[3][2] - delay_samples;
				if (read_offset < 0)
					read_offset += g_rgsxdly[3][0];
				g_rgsxdly[3][4] = read_offset;
				g_rgsxdly[3][5] = 128;
			}

			g_sxdly_stereo_lpf = room_left.value;
			g_rgsxdly[3][11] = 5000 * (shm->speed / 11025);
			g_rgsxdly[3][12] = g_rgsxdly[3][11];

			if (!g_rgsxdly[3][6])
				SXDLY_Free(3);
		}
	}
}

void SXDLY_DoStereoDelay(int count)
{
	int		*paint_ptr;
	int		remaining;
	short	delayed_sample;
	int		direct_sample;
	int		crossfade_sample;

	if (!g_rgsxdly[3][14])
		return;

	paint_ptr = (int *)((char *)paintbuffer + 16);
	remaining = count - 1;

	if (count)
	{
		while (1)
		{

			if (--g_rgsxdly[3][12] < 0)
				g_rgsxdly[3][12] = g_rgsxdly[3][11];

			delayed_sample = *(short *)(g_rgsxdly[3][14] + 2 * g_rgsxdly[3][3]);
			direct_sample = *paint_ptr;

			if (g_rgsxdly[3][5])
				goto crossfade_blend;

			if (!delayed_sample && !direct_sample)
			{
				*(short *)(g_rgsxdly[3][14] + 2 * g_rgsxdly[3][2]) = 0;
				goto next_sample;
			}

			if (!g_rgsxdly[3][5] && !g_rgsxdly[3][12])
			{
				g_rgsxdly[3][4] = g_rgsxdly[3][2] + g_rgsxdly[3][6] * rand() / -65534 - g_rgsxdly[3][6];
				if (g_rgsxdly[3][4] < 0)
					g_rgsxdly[3][4] += g_rgsxdly[3][0];
				g_rgsxdly[3][5] = 128;
			}

crossfade_blend:

			if (g_rgsxdly[3][5])
			{
				crossfade_sample = *(short *)(g_rgsxdly[3][14] + 2 * g_rgsxdly[3][4]++);
				delayed_sample = (short)(((128 - g_rgsxdly[3][5]) * crossfade_sample) >> 7) +
										 ((g_rgsxdly[3][5] * delayed_sample) >> 7);

				if (g_rgsxdly[3][4] >= g_rgsxdly[3][0])
					g_rgsxdly[3][4] = 0;

				if (!--g_rgsxdly[3][5])
					g_rgsxdly[3][3] = g_rgsxdly[3][4];
			}

			if (direct_sample > 0x7FFF)
				direct_sample = 0x7FFF;
			else if (direct_sample < -32768)
				direct_sample = -32768;

			*(short *)(g_rgsxdly[3][14] + 2 * g_rgsxdly[3][2]) = (short)direct_sample;
			*paint_ptr = delayed_sample;

next_sample:

			if (++g_rgsxdly[3][2] >= g_rgsxdly[3][0])
				g_rgsxdly[3][2] = 0;

			if (++g_rgsxdly[3][3] >= g_rgsxdly[3][0])
				g_rgsxdly[3][3] = 0;

			paint_ptr += 2;
			if (!remaining--)
				return;
		}
	}
}

void SXDLY_CheckNewDelayVal(void)
{
	float	delay_time;
	float	clamped_delay;

	if (room_delay.value != g_sxdly_delay_lpf)
	{
		delay_time = room_delay.value;

		if (delay_time == 0.0f)
		{

			SXDLY_Free(0);
			g_sxdly_delay_lpf = room_delay.value;
		}
		else
		{

			if (!g_rgsxdly[0][14])
				SXDLY_Init(0, 0.4f);

			clamped_delay = delay_time;
			if (delay_time >= 0.4f)
				clamped_delay = 0.4f;

			g_rgsxdly[0][6] = (int)((double)*(int *)((char *)shm + 32) * clamped_delay);

			if (g_rgsxdly[0][14])
			{
				Q_memset((void *)g_rgsxdly[0][14], 0, 2 * g_rgsxdly[0][0]);
				g_rgsxdly[0][8] = 0;
				g_rgsxdly[0][9] = 0;
				g_rgsxdly[0][10] = 0;
			}

			g_rgsxdly[0][2] = 0;
			g_rgsxdly[0][3] = g_rgsxdly[0][0] - g_rgsxdly[0][6];
			g_sxdly_delay_lpf = room_delay.value;

			if (!g_rgsxdly[0][6])
				SXDLY_Free(0);
		}
	}

	g_rgsxdly[0][1] = (int)room_dlylp.value;
	g_rgsxdly[0][7] = (int)(room_feedback.value * 255.0f);
}

void SXDLY_DoDelay(int count)
{
	short	delayed_sample;
	int		mixed_sample;
	int		filtered_sample;
	int		remaining;
	int		*paint_ptr;
	int		left_out, right_out;
	int		left_in, right_in;

	if (!g_rgsxdly[0][14])
		return;

	paint_ptr = (int *)((char *)paintbuffer + 16);
	remaining = count - 1;

	if (count)
	{
		do
		{

			delayed_sample = *(short *)(g_rgsxdly[0][14] + 2 * g_rgsxdly[0][3]);
			left_in = *paint_ptr;
			right_in = paint_ptr[1];

			if (!delayed_sample && !left_in && !right_in)
			{
				g_rgsxdly[0][9] = 0;
				g_rgsxdly[0][8] = 0;
				*(short *)(g_rgsxdly[0][14] + 2 * g_rgsxdly[0][2]) = 0;
			}
			else
			{

				mixed_sample = ((g_rgsxdly[0][7] * delayed_sample) >> 8) + ((left_in + right_in) >> 1);

				if (mixed_sample > 0x7FFF)
					mixed_sample = 0x7FFF;
				else if (mixed_sample < -32768)
					mixed_sample = -32768;

				if (g_rgsxdly[0][1])
				{
					filtered_sample = (g_rgsxdly[0][8] + g_rgsxdly[0][9] + mixed_sample) / 3;
					g_rgsxdly[0][8] = g_rgsxdly[0][9];
					g_rgsxdly[0][9] = mixed_sample;
				}
				else
				{
					filtered_sample = mixed_sample;
				}

				*(short *)(g_rgsxdly[0][14] + 2 * g_rgsxdly[0][2]) = (short)filtered_sample;

				left_out = (filtered_sample >> 2) + left_in;
				right_out = (filtered_sample >> 2) + right_in;

				if (left_out > 0x7FFF)
					left_out = 0x7FFF;
				else if (left_out < -32768)
					left_out = -32768;

				if (right_out > 0x7FFF)
					right_out = 0x7FFF;
				else if (right_out < -32768)
					right_out = -32768;

				*paint_ptr = left_out;
				paint_ptr[1] = right_out;
			}

			if (++g_rgsxdly[0][2] >= g_rgsxdly[0][0])
				g_rgsxdly[0][2] = 0;

			if (++g_rgsxdly[0][3] >= g_rgsxdly[0][0])
				g_rgsxdly[0][3] = 0;

			paint_ptr += 2;
		}
		while (remaining--);
	}
}

void SXRVB_CheckNewReverbVal(void)
{
	int		reverb_index;
	int		*reverb;
	int		prev_delay_samples;
	float	reverb_time;
	int		delay_samples;
	int		decay_time;
	int		crossfade_offset;

	if (g_sxrvb_roomsize_lpf != room_size.value)
	{
		g_sxrvb_roomsize_lpf = room_size.value;

		if (room_size.value == 0.0f)
		{

			SXDLY_Free(1);
			SXDLY_Free(2);
		}
		else
		{
			reverb_index = 1;
			reverb = &g_rgsxdly[1][0];

			do
			{

				if (reverb_index == 1)
				{
					reverb_time = room_size.value;
					if (reverb_time >= 0.1f)
						reverb_time = 0.1f;
					delay_samples = (int)(reverb_time * (double)*(int *)((char *)shm + 32));
					decay_time = 500 * (*(int *)((char *)shm + 32) / 11025);
				}
				else if (reverb_index == 2)
				{
					reverb_time = room_size.value * 0.71f;
					if (reverb_time >= 0.1f)
						reverb_time = 0.1f;
					delay_samples = (int)(reverb_time * (double)*(int *)((char *)shm + 32));
					decay_time = 700 * (*(int *)((char *)shm + 32) / 11025);
				}
				else
				{
					goto skip_init;
				}

				reverb[11] = decay_time;

skip_init:
				reverb[12] = reverb[11];

				if (!reverb[14])
				{
					reverb[6] = delay_samples;
					SXDLY_Init(reverb_index, 0.1f);
				}

				prev_delay_samples = reverb[6];
				if (delay_samples != prev_delay_samples)
				{

					crossfade_offset = reverb[2] - delay_samples;
					reverb[4] = crossfade_offset;
					if (crossfade_offset < 0)
						reverb[4] = crossfade_offset + *reverb;
					reverb[5] = 32;
				}

				if (!prev_delay_samples)
					SXDLY_Free(reverb_index);

				reverb += 15;
				++reverb_index;
			}
			while (reverb < &g_rgsxdly[3][0]);
		}
	}

	g_rgsxdly[1][7] = (int)(room_refl.value * 255.0f);
	g_rgsxdly[1][1] = (int)room_rvblp.value;
	g_rgsxdly[2][7] = g_rgsxdly[1][7];
	g_rgsxdly[2][1] = (int)room_rvblp.value;
}

void SXRVB_DoReverb(int count)
{
	int remaining;
	int *paint_ptr;

	if (!g_rgsxdly[1][14])
		return;

	paint_ptr = (int *)((char *)paintbuffer + 16);
	remaining = count - 1;

	if (count)
	{
		do
		{
			int left_in;
			int right_in;
			int mono_in;
			int reverb_mix;
			short delayed1;
			int sample1;
			int filtered1;

			left_in = paint_ptr[0];
			right_in = paint_ptr[1];
			mono_in = (right_in + left_in) >> 1;

			if (--g_rgsxdly[1][12] < 0)
				g_rgsxdly[1][12] = g_rgsxdly[1][11];

			delayed1 = *(short *)(g_rgsxdly[1][14] + 2 * g_rgsxdly[1][3]);

			if (!g_rgsxdly[1][5] && !delayed1 && !left_in && !right_in)
			{
				g_rgsxdly[1][8] = 0;
				*(short *)(g_rgsxdly[1][14] + 2 * g_rgsxdly[1][2]) = 0;
				filtered1 = 0;
			}
			else
			{
				if (!g_rgsxdly[1][5] && !g_rgsxdly[1][11])
				{
					g_rgsxdly[1][4] = g_rgsxdly[1][2] + g_rgsxdly[1][6] * rand() / -65534 - g_rgsxdly[1][6];
					if (g_rgsxdly[1][4] < 0)
						g_rgsxdly[1][4] += g_rgsxdly[1][0];
					g_rgsxdly[1][5] = 32;
				}

				if (g_rgsxdly[1][5])
				{
					const int crossfade_sample = *(short *)(g_rgsxdly[1][14] + 2 * g_rgsxdly[1][4]++);

					delayed1 = (short)((((32 - g_rgsxdly[1][5]) * crossfade_sample) >> 5) +
									   ((g_rgsxdly[1][5] * delayed1) >> 5));

					if (g_rgsxdly[1][4] >= g_rgsxdly[1][0])
						g_rgsxdly[1][4] = 0;

					if (!--g_rgsxdly[1][5])
						g_rgsxdly[1][3] = g_rgsxdly[1][4];
				}

				if (delayed1)
				{
					sample1 = mono_in + ((g_rgsxdly[1][7] * delayed1) >> 8);
					if (sample1 > 0x7FFF)
						sample1 = 0x7FFF;
					else if (sample1 < -32768)
						sample1 = -32768;
				}
				else
				{
					sample1 = mono_in;
				}

				if (g_rgsxdly[1][1])
				{
					const int prev = g_rgsxdly[1][8];
					g_rgsxdly[1][8] = sample1;
					filtered1 = (sample1 + prev) >> 1;
				}
				else
				{
					filtered1 = sample1;
				}

				*(short *)(g_rgsxdly[1][14] + 2 * g_rgsxdly[1][2]) = (short)filtered1;
			}

			reverb_mix = filtered1;

			if (++g_rgsxdly[1][2] >= g_rgsxdly[1][0])
				g_rgsxdly[1][2] = 0;

			if (++g_rgsxdly[1][3] >= g_rgsxdly[1][0])
				g_rgsxdly[1][3] = 0;

			if (--g_rgsxdly[2][12] < 0)
				g_rgsxdly[2][12] = g_rgsxdly[2][11];

			if (g_rgsxdly[2][14])
			{
				short delayed2;
				int sample2;
				int filtered2;

				delayed2 = *(short *)(g_rgsxdly[2][14] + 2 * g_rgsxdly[2][3]);

				if (!g_rgsxdly[2][5] && !delayed2 && !left_in && !right_in)
				{
					g_rgsxdly[2][8] = 0;
					*(short *)(g_rgsxdly[2][14] + 2 * g_rgsxdly[2][2]) = 0;
					filtered2 = 0;
				}
				else
				{
					if (!g_rgsxdly[2][5] && !g_rgsxdly[2][11])
					{
						g_rgsxdly[2][4] = g_rgsxdly[2][2] + g_rgsxdly[2][6] * rand() / -65534 - g_rgsxdly[2][6];
						if (g_rgsxdly[2][4] < 0)
							g_rgsxdly[2][4] += g_rgsxdly[2][0];
						g_rgsxdly[2][5] = 32;
					}

					if (g_rgsxdly[2][5])
					{
						const int crossfade_sample = *(short *)(g_rgsxdly[2][14] + 2 * g_rgsxdly[2][4]++);

						delayed2 = (short)((((32 - g_rgsxdly[2][5]) * crossfade_sample) >> 5) +
										   ((g_rgsxdly[2][5] * delayed2) >> 5));

						if (g_rgsxdly[2][4] >= g_rgsxdly[2][0])
							g_rgsxdly[2][4] = 0;

						if (!--g_rgsxdly[2][5])
							g_rgsxdly[2][3] = g_rgsxdly[2][4];
					}

					if (delayed2)
					{
						sample2 = mono_in + ((g_rgsxdly[2][7] * delayed2) >> 8);
						if (sample2 > 0x7FFF)
							sample2 = 0x7FFF;
						else if (sample2 < -32768)
							sample2 = -32768;
					}
					else
					{
						sample2 = mono_in;
					}

					if (g_rgsxdly[2][1])
					{
						const int prev = g_rgsxdly[2][8];
						g_rgsxdly[2][8] = sample2;
						filtered2 = (sample2 + prev) >> 1;
					}
					else
					{
						filtered2 = sample2;
					}

					*(short *)(g_rgsxdly[2][14] + 2 * g_rgsxdly[2][2]) = (short)filtered2;
				}

				reverb_mix += filtered2;

				if (++g_rgsxdly[2][2] >= g_rgsxdly[2][0])
					g_rgsxdly[2][2] = 0;

				if (++g_rgsxdly[2][3] >= g_rgsxdly[2][0])
					g_rgsxdly[2][3] = 0;
			}

			{
				int left_out;
				int right_out;

				left_out = reverb_mix / 6 + left_in;
				right_out = reverb_mix / 6 + right_in;

				if (left_out > 0x7FFF)
					left_out = 0x7FFF;
				else if (left_out < -32768)
					left_out = -32768;

				if (right_out > 0x7FFF)
					right_out = 0x7FFF;
				else if (right_out < -32768)
					right_out = -32768;

				paint_ptr[0] = left_out;
				paint_ptr[1] = right_out;
			}

			paint_ptr += 2;
		}
		while (remaining--);
	}
}

void SX_RoomFX(int count)
{
	int left_in;
	int right_in;
	int left_sample;
	int right_sample;
	int *paint_ptr;
	int do_lowpass;
	int do_random_mod;
	int remaining;

	if (room_lp.value == 0.0f && room_mod.value == 0.0f)
		return;

	paint_ptr = (int *)((char *)paintbuffer + 16);
	do_lowpass = (room_lp.value != 0.0f);
	do_random_mod = (room_mod.value != 0.0f);
	remaining = count - 1;

	if (count)
	{
		do
		{
			left_in = *paint_ptr;
			right_in = paint_ptr[1];
			left_sample = left_in;
			right_sample = right_in;

			if (do_lowpass)
			{
				int saved_left;
				int right_sum;

				saved_left = left_in;
				left_sample = (left_sample + g_room_lp_state[0] + g_room_lp_state[4] +
							   g_room_lp_state[3] + g_room_lp_state[2] + g_room_lp_state[1]) / 4;

				right_sum = g_room_lp_state[8] + g_room_lp_state[7] + g_room_lp_state[6] +
							g_room_lp_state[5] + g_room_lp_state[9];
				g_room_lp_state[9] = right_in;
				right_sample = (right_sum + right_in) / 4;

				g_room_lp_state[0] = g_room_lp_state[1];
				g_room_lp_state[1] = g_room_lp_state[2];
				g_room_lp_state[2] = g_room_lp_state[3];
				g_room_lp_state[3] = saved_left;
				g_room_lp_state[4] = g_room_lp_state[5];
				g_room_lp_state[5] = g_room_lp_state[6];
				g_room_lp_state[6] = g_room_lp_state[7];
				g_room_lp_state[7] = g_room_lp_state[8];
				g_room_lp_state[8] = right_in;
			}

			if (do_random_mod)
			{
				if (--g_room_mod_counter_left < 0)
					g_room_mod_counter_left = g_room_mod_rate_left;

				if (!g_room_mod_rate_left)
				{
					if (255 * rand() / 0x7FFF + 32 > 255)
						g_room_mod_target_left = 255;
					else
						g_room_mod_target_left = 255 * rand() / 0x7FFF + 32;
				}

				if (--g_room_mod_counter_right < 0)
					g_room_mod_counter_right = g_room_mod_rate_right;

				if (!g_room_mod_rate_right)
				{
					if (255 * rand() / 0x7FFF + 32 > 255)
						g_room_mod_target_right = 255;
					else
						g_room_mod_target_right = 255 * rand() / 0x7FFF + 32;
				}

				left_sample = (g_room_mod_current_left * left_sample) >> 8;
				right_sample = (g_room_mod_current_right * right_sample) >> 8;

				if (g_room_mod_target_left <= g_room_mod_current_left)
				{
					if (g_room_mod_target_left < g_room_mod_current_left)
						--g_room_mod_current_left;
				}
				else
				{
					++g_room_mod_current_left;
				}

				if (g_room_mod_target_right <= g_room_mod_current_right)
				{
					if (g_room_mod_target_right < g_room_mod_current_right)
						--g_room_mod_current_right;
				}
				else
				{
					++g_room_mod_current_right;
				}
			}

			if (left_sample <= 0x7FFF)
			{
				if (left_sample < -32768)
					left_sample = -32768;
			}
			else
			{
				left_sample = 0x7FFF;
			}

			if (right_sample <= 0x7FFF)
			{
				if (right_sample < -32768)
					right_sample = -32768;
			}
			else
			{
				right_sample = 0x7FFF;
			}

			*paint_ptr = left_sample;
			paint_ptr[1] = right_sample;
			paint_ptr += 2;
		}
		while (remaining--);
	}
}

typedef struct sx_roomtype_preset_s
{
	float room_lp;
	float room_mod;
	float room_size;
	float room_refl;
	float room_rvblp;
	float room_delay;
	float room_feedback;
	float room_dlylp;
	float room_left;
} sx_roomtype_preset_t;

static const sx_roomtype_preset_t g_room_type_presets[29] = {
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.08f, 0.8f, 2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.02f, 0.75f, 0.0f, 0.001f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.03f, 0.78f, 0.0f, 0.002f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.06f, 0.77f, 0.0f, 0.003f },
	{ 0.0f, 0.0f, 0.05f, 0.85f, 1.0f, 0.008f, 0.96f, 2.0f, 0.01f },
	{ 0.0f, 0.0f, 0.05f, 0.88f, 1.0f, 0.01f, 0.98f, 2.0f, 0.02f },
	{ 0.0f, 0.0f, 0.05f, 0.92f, 1.0f, 0.015f, 0.995f, 2.0f, 0.04f },
	{ 0.0f, 0.0f, 0.05f, 0.84f, 1.0f, 0.0f, 0.0f, 2.0f, 0.003f },
	{ 0.0f, 0.0f, 0.05f, 0.9f, 1.0f, 0.0f, 0.0f, 2.0f, 0.002f },
	{ 0.0f, 0.0f, 0.05f, 0.95f, 1.0f, 0.0f, 0.0f, 2.0f, 0.001f },
	{ 0.0f, 0.0f, 0.05f, 0.7f, 0.0f, 0.0f, 0.0f, 2.0f, 0.003f },
	{ 0.0f, 0.0f, 0.055f, 0.78f, 0.0f, 0.0f, 0.0f, 2.0f, 0.002f },
	{ 0.0f, 0.0f, 0.05f, 0.86f, 0.0f, 0.0f, 0.0f, 2.0f, 0.001f },
	{ 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f, 0.01f },
	{ 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.06f, 0.85f, 2.0f, 0.02f },
	{ 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.6f, 2.0f, 0.05f },
	{ 0.0f, 0.0f, 0.05f, 0.8f, 1.0f, 0.15f, 0.48f, 2.0f, 0.008f },
	{ 0.0f, 0.0f, 0.06f, 0.9f, 1.0f, 0.22f, 0.52f, 2.0f, 0.005f },
	{ 0.0f, 0.0f, 0.07f, 0.94f, 1.0f, 0.3f, 0.6f, 2.0f, 0.001f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.42f, 2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.35f, 0.48f, 2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.38f, 0.6f, 2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.05f, 0.9f, 1.0f, 0.2f, 0.28f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.07f, 0.9f, 1.0f, 0.3f, 0.4f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.09f, 0.9f, 1.0f, 0.35f, 0.5f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.01f, 0.9f, 0.0f, 0.0f, 0.0f, 2.0f, 0.05f },
	{ 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.009f, 0.999f, 2.0f, 0.04f },
	{ 0.0f, 0.0f, 0.001f, 0.999f, 0.0f, 0.2f, 0.8f, 2.0f, 0.05f },
};

void SX_RoomProcess(int count)
{
	int did_preset_change;
	int room_type_index;

	if (room_off.value == 0.0f)
	{
		did_preset_change = 0;
		if (g_last_room_type != room_type.value)
		{
			g_last_room_type = room_type.value;
			room_type_index = (int)room_type.value;
			if ((unsigned int)room_type_index <= 0x1C)
			{
				const sx_roomtype_preset_t *preset;

				preset = &g_room_type_presets[room_type_index];
				Cvar_SetValue("room_lp", preset->room_lp);
				Cvar_SetValue("room_mod", preset->room_mod);
				Cvar_SetValue("room_size", preset->room_size);
				Cvar_SetValue("room_refl", preset->room_refl);
				Cvar_SetValue("room_rvblp", preset->room_rvblp);
				Cvar_SetValue("room_delay", preset->room_delay);
				Cvar_SetValue("room_feedback", preset->room_feedback);
				Cvar_SetValue("room_dlylp", preset->room_dlylp);
				Cvar_SetValue("room_left", preset->room_left);
			}

			SXRVB_CheckNewReverbVal();
			SXDLY_CheckNewDelayVal();
			SXDLY_CheckNewStereoDelayVal();
			did_preset_change = 1;
		}

		if (did_preset_change || room_type.value != 0.0f)
		{
			SXRVB_CheckNewReverbVal();
			SXDLY_CheckNewDelayVal();
			SXDLY_CheckNewStereoDelayVal();
			SX_RoomFX(count);
			SXRVB_DoReverb(count);
			SXDLY_DoDelay(count);
			SXDLY_DoStereoDelay(count);
		}
	}
}
