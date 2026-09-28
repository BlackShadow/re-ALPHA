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
// snd_mix.c -- portable code to mix sounds for snd_dma.c, and the room effects

#include <limits.h>

#include "quakedef.h"
#include "winquake.h"

#define	PAINTBUFFER_SIZE	512

// one scale table per 8 steps of the 0-255 channel volume
#define SCALETABLE_LEVELS	32
#define SCALETABLE_SHIFT	3

// clip a mixed sample to the 16 bit range
#define CLIP16(x)	{ if ((x) > SHRT_MAX) (x) = SHRT_MAX; else if ((x) < SHRT_MIN) (x) = SHRT_MIN; }

static portable_samplepair_t	paintbuffer[PAINTBUFFER_SIZE];

static int		snd_scaletable[SCALETABLE_LEVELS][256];
static int		*snd_p, snd_linear_count, snd_vol;
static short	*snd_out;

void SX_RoomProcess(int count);

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

/*
================
SND_PaintChannelFrom8

8 bit samples are mixed through the volume scale tables.
================
*/
void SND_PaintChannelFrom8(channel_t *ch, sfxcache_t *sc, int count)
{
	unsigned int			leftvol, rightvol;
	unsigned char			*sfx;
	int						*lscale, *rscale;
	portable_samplepair_t	*pb;
	int						data;
	int						i;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	if (leftvol > 255)
		leftvol = 255;
	if (rightvol > 255)
		rightvol = 255;

	sfx = sc->data + ch->pos;
	ch->pos += count;

	lscale = snd_scaletable[leftvol >> SCALETABLE_SHIFT];
	rscale = snd_scaletable[rightvol >> SCALETABLE_SHIFT];

	pb = paintbuffer;
	for (i = 0; i < count; i++, pb++)
	{
		data = *sfx++;
		pb->left += lscale[data];
		pb->right += rscale[data];
	}
}

/*
================
Snd_WriteLinearBlastStereo16

Clips snd_linear_count paintbuffer values from snd_p into snd_out.
================
*/
void Snd_WriteLinearBlastStereo16(void)
{
	int		i;
	int		val;

	i = snd_linear_count;
	do
	{
		val = (snd_vol * snd_p[i - 2]) >> 8;
		CLIP16(val);
		snd_out[i - 2] = val;

		val = (snd_vol * snd_p[i - 1]) >> 8;
		CLIP16(val);
		snd_out[i - 1] = val;

		i -= 2;
	}
	while (i > 0);
}

/*
================
SND_InitScaletable
================
*/
void SND_InitScaletable(void)
{
	int		i, j;

	for (i = 0; i < SCALETABLE_LEVELS; i++)
		for (j = 0; j < 256; j++)
			snd_scaletable[i][j] = ((signed char)j) * (i << SCALETABLE_SHIFT);
}

/*
================
SND_PaintChannelFrom16
================
*/
void SND_PaintChannelFrom16(channel_t *ch, sfxcache_t *sc, int count)
{
	int						data;
	int						leftvol, rightvol;
	short					*sfx;
	portable_samplepair_t	*pb;
	int						i;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	sfx = (short *)sc->data + ch->pos;

	if (count > 0)
	{
		pb = paintbuffer;
		i = count;
		do
		{
			data = *sfx++;
			pb->left += (data * leftvol) >> 8;
			pb->right += (data * rightvol) >> 8;
			pb++;
		}
		while (--i);
	}

	ch->pos += count;
}

/*
================
S_TransferStereo16

Writes the paintbuffer to a 16 bit stereo dma buffer.
================
*/
void S_TransferStereo16(int endtime)
{
	int		lpos;
	int		lpaintedtime;
	DWORD	*pbuf;
	int		reps;
	DWORD	dwSize, dwSize2;
	DWORD	*pbuf2;
	HRESULT	hresult;

	lpaintedtime = paintedtime;

	snd_vol = (int)(volume.value * 256.0f);

	snd_p = (int *)paintbuffer;

	if (pDSBuf)
	{
		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, (void **)&pbuf, &dwSize, (void **)&pbuf2, &dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return;
			}

			if (++reps > DS_LOCK_RETRIES)
			{
				Con_Printf("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return;
			}
		}
	}
	else
	{
		pbuf = (DWORD *)shm->buffer;
	}

	while (lpaintedtime < endtime)
	{
	// handle recirculating buffer issues
		lpos = lpaintedtime & ((shm->samples >> 1) - 1);

		snd_out = (short *)pbuf + (lpos << 1);

		snd_linear_count = (shm->samples >> 1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

	// write a linear blast of samples
		Snd_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count >> 1);
	}

	if (pDSBuf)
		pDSBuf->lpVtbl->Unlock(pDSBuf, pbuf, dwSize, NULL, 0);
}

/*
================
S_TransferPaintBuffer

Writes the paintbuffer to the dma buffer in its sample format.
================
*/
void S_TransferPaintBuffer(int endtime)
{
	int		out_idx;
	int		count;
	int		out_mask;
	int		*p;
	int		step;
	int		val;
	int		vol;
	DWORD	*pbuf;
	int		reps;
	DWORD	dwSize, dwSize2;
	DWORD	*pbuf2;
	HRESULT	hresult;

	if (shm->samplebits == 16 && shm->channels == 2)
	{
		S_TransferStereo16(endtime);
		return;
	}

	p = (int *)paintbuffer;
	count = shm->channels * (endtime - paintedtime);
	out_idx = (shm->samples - 1) & (paintedtime * shm->channels);
	out_mask = shm->samples - 1;
	step = 3 - shm->channels;
	vol = (int)(volume.value * 256.0f);

	if (pDSBuf)
	{
		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, (void **)&pbuf, &dwSize, (void **)&pbuf2, &dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf("S_TransferPaintBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return;
			}

			if (++reps > DS_LOCK_RETRIES)
			{
				Con_Printf("S_TransferPaintBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return;
			}
		}
	}
	else
	{
		pbuf = (DWORD *)shm->buffer;
	}

	if (shm->samplebits == 16)
	{
		short *out = (short *)pbuf;

		while (count--)
		{
			val = (vol * *p) >> 8;
			p += step;
			CLIP16(val);
			out[out_idx] = val;
			out_idx = out_mask & (out_idx + 1);
		}
	}
	else if (shm->samplebits == 8)
	{
		unsigned char *out = (unsigned char *)pbuf;

		while (count--)
		{
			val = (vol * *p) >> 8;
			p += step;
			CLIP16(val);
			out[out_idx] = (val >> 8) + 128;
			out_idx = out_mask & (out_idx + 1);
		}
	}

	if (pDSBuf)
	{
		DWORD	dwNewpos, dwWrite;

		pDSBuf->lpVtbl->Unlock(pDSBuf, pbuf, dwSize, NULL, 0);
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &dwNewpos, &dwWrite);
	}
}

/*
================
S_PaintChannels

Mixes all playing channels up to endtime and sends them to the device.
================
*/
void S_PaintChannels(int endtime)
{
	int			i;
	int			end;
	channel_t	*ch;
	sfxcache_t	*sc;
	int			ltime, count;
	int			chend;

	while (paintedtime < endtime)
	{
	// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

	// clear the paint buffer
		Q_memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));

	// paint in the channels
		for (i = 0; i < total_channels; i++)
		{
			ch = &channels[i];
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			sc = S_LoadSound(ch->sfx);
			if (!sc)
				continue;

			ltime = paintedtime;

			while (ltime < end)
			{
			// paint up to end
				chend = ch->end;
				if (chend >= end)
					chend = end;

				count = chend - ltime;
				if (count > 0)
				{
					if (sc->width == 1)
						SND_PaintChannelFrom8(ch, sc, count);
					else
						SND_PaintChannelFrom16(ch, sc, count);

					ltime += count;
				}

			// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (sc->loopstart < 0)
					{
						// channel just stopped
						ch->sfx = NULL;
						break;
					}

					ch->pos = sc->loopstart;
					ch->end = ltime + sc->length - ch->pos;
				}
			}
		}

	// run the room effects over the mix
		SX_RoomProcess(end - paintedtime);

	// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

/*
===============================================================================

ROOM EFFECTS

The mixed paintbuffer runs through a lowpass and amplitude modulation, a
reverb made of two delay lines, a mono delay and a stereo (left) delay,
set up by the room_* cvars or a room_type preset.

===============================================================================
*/

#define CSXDLYMAX		4

#define ISXMONODLY		0		// mono delay line
#define ISXRVB			1		// first of the reverb delay lines
#define CSXRVBMAX		2
#define ISXSTEREODLY	3		// left channel delay line

#define SXDLY_MAX		0.4f	// max delay of the mono delay in seconds
#define SXRVB_MAX		0.1f	// max delay of the reverb lines in seconds
#define SXSTE_MAX		0.1f	// max delay of the stereo delay in seconds

// crossfade length, in samples, when a delay line moves its output
#define SXSTE_XFADEBITS	7
#define SXSTE_XFADE		(1 << SXSTE_XFADEBITS)
#define SXRVB_XFADEBITS	5
#define SXRVB_XFADE		(1 << SXRVB_XFADEBITS)

#define SX_BASE_SPEED	11025	// the modulation periods are in samples at this rate

#define SXAMOD_PERIOD_LEFT	350		// amplitude modulation periods
#define SXAMOD_PERIOD_RIGHT	450
#define SXAMOD_MIN			32		// lowest random modulation target

#define SXRVB_MOD_PERIOD1	500		// reverb crossfade periods
#define SXRVB_MOD_PERIOD2	700
#define SXRVB_LINE2_SCALE	0.71f	// the second reverb line is shorter

#define SXSTE_MOD_PERIOD	5000	// stereo delay crossfade period

typedef struct dly_s
{
	int		cdelaysamplesmax;	// size of delay line in samples
	int		lp;					// lowpass flag 0 = off, 1 = on

	int		idelayinput;		// i/o indices into circular buffer
	int		idelayoutput;

	int		idelayoutputxf;		// crossfade output pointer
	int		xfade;				// crossfade value

	int		delaysamples;		// current delay setting
	int		delayfeed;			// current feedback setting

	int		lp0, lp1, lp2;		// lowpass filter buffer

	int		mod;				// sample modulation count
	int		modcur;

	HGLOBAL	hdelayline;			// handle to delay line buffer
	short	*lpdelayline;		// buffer
} dly_t;

static dly_t	rgsxdly[CSXDLYMAX];

// last cvar values the delay lines were set up for
static float	sxdly_delay_prev;
static float	sxrvb_size_prev;
static float	sxste_delay_prev;
static float	sxroom_type_prev;

static int		rgsxlp[10];			// lowpass history, left then right

static int		sxamod_left;		// amplitude modulation, 0-255
static int		sxamod_target_left;
static int		sxamod_right;
static int		sxamod_target_right;
static int		sxmod_period_left;
static int		sxmod_period_right;
static int		sxmod_count_left;
static int		sxmod_count_right;

/*
================
SX_Init
================
*/
void SX_Init(void)
{
	int		speed;

	Q_memset(rgsxdly, 0, sizeof(rgsxdly));
	Q_memset(rgsxlp, 0, sizeof(rgsxlp));

	sxdly_delay_prev = -1.0f;
	sxrvb_size_prev = -1.0f;
	sxroom_type_prev = -1.0f;
	sxste_delay_prev = -1.0f;

	sxamod_right = 255;
	sxamod_left = 255;
	sxamod_target_right = 255;
	sxamod_target_left = 255;

	speed = shm->speed;
	sxmod_period_left = SXAMOD_PERIOD_LEFT * (speed / SX_BASE_SPEED);
	sxmod_count_left = SXAMOD_PERIOD_LEFT * (speed / SX_BASE_SPEED);
	sxmod_period_right = SXAMOD_PERIOD_RIGHT * (speed / SX_BASE_SPEED);
	sxmod_count_right = SXAMOD_PERIOD_RIGHT * (speed / SX_BASE_SPEED);

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
}

/*
================
SXDLY_Init

Allocates delay line idelay for a delay of up to delay seconds.
================
*/
qboolean SXDLY_Init(int idelay, float delay)
{
	dly_t	*pdly;
	int		cbsamples;
	HGLOBAL	hmem;
	short	*lpbuf;

	pdly = &rgsxdly[idelay];

	if (delay > SXDLY_MAX)
		delay = SXDLY_MAX;

	if (pdly->lpdelayline)
	{
		GlobalUnlock(pdly->hdelayline);
		GlobalFree(pdly->hdelayline);
		pdly->hdelayline = NULL;
		pdly->lpdelayline = NULL;
	}

	if (delay == 0.0f)
		return true;

	pdly->cdelaysamplesmax = (int)((float)shm->speed * delay + 1.0f);
	cbsamples = pdly->cdelaysamplesmax * sizeof(short);

	hmem = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, cbsamples);
	if (!hmem)
	{
		Con_Printf("Sound FX: Out of memory.\n");
		return false;
	}

	lpbuf = (short *)GlobalLock(hmem);
	if (!lpbuf)
	{
		Con_Printf("Sound FX: Failed to lock delay buffer memory.\n");
		GlobalFree(hmem);
		return false;
	}

	memset(lpbuf, 0, cbsamples);

	pdly->hdelayline = hmem;
	pdly->lpdelayline = lpbuf;

	// init delay loop input and output counters
	pdly->idelayinput = 0;
	pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples;
	pdly->xfade = 0;
	pdly->mod = 0;
	pdly->lp = 1;
	pdly->modcur = 0;
	pdly->lp2 = 0;
	pdly->lp1 = 0;
	pdly->lp0 = 0;

	return true;
}

/*
================
SXDLY_Free
================
*/
void SXDLY_Free(int idelay)
{
	dly_t	*pdly;

	pdly = &rgsxdly[idelay];

	if (pdly->lpdelayline)
	{
		GlobalUnlock(pdly->hdelayline);
		GlobalFree(pdly->hdelayline);
		pdly->hdelayline = NULL;
		pdly->lpdelayline = NULL;
	}
}

/*
================
SXDLY_CheckNewStereoDelayVal

Sets up the stereo delay when room_left changed.
================
*/
void SXDLY_CheckNewStereoDelayVal(void)
{
	dly_t	*pdly;
	int		delaysamples;
	int		outxf;
	float	delay;

	pdly = &rgsxdly[ISXSTEREODLY];

	if (sxste_delay_prev == room_left.value)
		return;

	delay = room_left.value;

	if (delay == 0.0f)
	{
		SXDLY_Free(ISXSTEREODLY);
		sxste_delay_prev = 0.0f;
		return;
	}

	if (delay >= SXSTE_MAX)
		delay = SXSTE_MAX;

	delaysamples = (int)((double)shm->speed * delay);

	// init delay line if not active
	if (!pdly->lpdelayline)
	{
		pdly->delaysamples = delaysamples;
		SXDLY_Init(ISXSTEREODLY, SXSTE_MAX);
	}

	// do crossfade to new delay if delay has changed
	if (pdly->delaysamples != delaysamples)
	{
		// set up crossfade from old pdly->delaysamples to new delaysamples
		outxf = pdly->idelayinput - delaysamples;
		if (outxf < 0)
			outxf += pdly->cdelaysamplesmax;
		pdly->idelayoutputxf = outxf;
		pdly->xfade = SXSTE_XFADE;
	}

	sxste_delay_prev = room_left.value;

	pdly->mod = SXSTE_MOD_PERIOD * (shm->speed / SX_BASE_SPEED);
	pdly->modcur = pdly->mod;

	// deactivate delay line if we're turning it off
	if (!pdly->delaysamples)
		SXDLY_Free(ISXSTEREODLY);
}

/*
================
SXDLY_DoStereoDelay

Delays the left channel, crossfading to a random new delay now and then.
================
*/
void SXDLY_DoStereoDelay(int count)
{
	dly_t					*pdly;
	portable_samplepair_t	*pbuf;
	int						left;
	short					sampledly;
	int						samplexf;
	int						countr;

	pdly = &rgsxdly[ISXSTEREODLY];

	if (!pdly->lpdelayline)
		return;

	pbuf = paintbuffer;
	countr = count;

	while (countr--)
	{
		if (--pdly->modcur < 0)
			pdly->modcur = pdly->mod;

		sampledly = pdly->lpdelayline[pdly->idelayoutput];
		left = pbuf->left;

		if (!pdly->xfade && !sampledly && !left)
		{
			// delay line and input are silent
			pdly->lpdelayline[pdly->idelayinput] = 0;
		}
		else
		{
			// pick a new random delay to crossfade to
			if (!pdly->xfade && !pdly->modcur)
			{
				pdly->idelayoutputxf = pdly->idelayinput + pdly->delaysamples * rand() / (-2 * RAND_MAX) - pdly->delaysamples;
				if (pdly->idelayoutputxf < 0)
					pdly->idelayoutputxf += pdly->cdelaysamplesmax;
				pdly->xfade = SXSTE_XFADE;
			}

			if (pdly->xfade)
			{
				samplexf = pdly->lpdelayline[pdly->idelayoutputxf++];
				sampledly = (short)(((SXSTE_XFADE - pdly->xfade) * samplexf) >> SXSTE_XFADEBITS)
					+ ((pdly->xfade * sampledly) >> SXSTE_XFADEBITS);

				if (pdly->idelayoutputxf >= pdly->cdelaysamplesmax)
					pdly->idelayoutputxf = 0;

				if (!--pdly->xfade)
					pdly->idelayoutput = pdly->idelayoutputxf;
			}

			CLIP16(left);
			pdly->lpdelayline[pdly->idelayinput] = left;
			pbuf->left = sampledly;
		}

		if (++pdly->idelayinput >= pdly->cdelaysamplesmax)
			pdly->idelayinput = 0;

		if (++pdly->idelayoutput >= pdly->cdelaysamplesmax)
			pdly->idelayoutput = 0;

		pbuf++;
	}
}

/*
================
SXDLY_CheckNewDelayVal

Sets up the mono delay when room_delay changed.
================
*/
void SXDLY_CheckNewDelayVal(void)
{
	dly_t	*pdly;
	float	delay;

	pdly = &rgsxdly[ISXMONODLY];

	if (room_delay.value != sxdly_delay_prev)
	{
		delay = room_delay.value;

		if (delay == 0.0f)
		{
			SXDLY_Free(ISXMONODLY);
			sxdly_delay_prev = room_delay.value;
		}
		else
		{
			// init delay line if not active
			if (!pdly->lpdelayline)
				SXDLY_Init(ISXMONODLY, SXDLY_MAX);

			if (delay >= SXDLY_MAX)
				delay = SXDLY_MAX;

			pdly->delaysamples = (int)((double)shm->speed * delay);

			// flush the delay line and filter
			if (pdly->lpdelayline)
			{
				Q_memset(pdly->lpdelayline, 0, pdly->cdelaysamplesmax * sizeof(short));
				pdly->lp0 = 0;
				pdly->lp1 = 0;
				pdly->lp2 = 0;
			}

			pdly->idelayinput = 0;
			pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples;
			sxdly_delay_prev = room_delay.value;

			// deactivate delay line if we're turning it off
			if (!pdly->delaysamples)
				SXDLY_Free(ISXMONODLY);
		}
	}

	pdly->lp = (int)room_dlylp.value;
	pdly->delayfeed = (int)(room_feedback.value * 255.0f);
}

/*
================
SXDLY_DoDelay

Mono delay with feedback, mixed into both channels.
================
*/
void SXDLY_DoDelay(int count)
{
	dly_t					*pdly;
	portable_samplepair_t	*pbuf;
	short					sampledly;
	int						val;
	int						valt;
	int						left, right;
	int						countr;

	pdly = &rgsxdly[ISXMONODLY];

	if (!pdly->lpdelayline)
		return;

	pbuf = paintbuffer;
	countr = count;

	while (countr--)
	{
		sampledly = pdly->lpdelayline[pdly->idelayoutput];
		left = pbuf->left;
		right = pbuf->right;

		if (!sampledly && !left && !right)
		{
			// delay line and input are silent
			pdly->lp1 = 0;
			pdly->lp0 = 0;
			pdly->lpdelayline[pdly->idelayinput] = 0;
		}
		else
		{
			// get delay line sample, mix in feedback and mono input
			val = ((pdly->delayfeed * sampledly) >> 8) + ((left + right) >> 1);
			CLIP16(val);

			if (pdly->lp)
			{
				// 3 tap lowpass
				valt = (pdly->lp0 + pdly->lp1 + val) / 3;
				pdly->lp0 = pdly->lp1;
				pdly->lp1 = val;
			}
			else
			{
				valt = val;
			}

			pdly->lpdelayline[pdly->idelayinput] = valt;

			// mix a quarter of the delay into the output
			left = (valt >> 2) + left;
			right = (valt >> 2) + right;
			CLIP16(left);
			CLIP16(right);
			pbuf->left = left;
			pbuf->right = right;
		}

		if (++pdly->idelayinput >= pdly->cdelaysamplesmax)
			pdly->idelayinput = 0;

		if (++pdly->idelayoutput >= pdly->cdelaysamplesmax)
			pdly->idelayoutput = 0;

		pbuf++;
	}
}

/*
================
SXRVB_CheckNewReverbVal

Sets up the reverb delay lines when room_size changed.
================
*/
void SXRVB_CheckNewReverbVal(void)
{
	dly_t	*pdly;
	int		delaysamples;
	int		prevsamples;
	int		i;
	int		speed;
	float	delay;

	if (sxrvb_size_prev != room_size.value)
	{
		sxrvb_size_prev = room_size.value;

		if (room_size.value == 0.0f)
		{
			// deactivate all delay lines
			SXDLY_Free(ISXRVB);
			SXDLY_Free(ISXRVB + 1);
		}
		else
		{
			for (i = ISXRVB, pdly = &rgsxdly[ISXRVB]; pdly < &rgsxdly[ISXRVB + CSXRVBMAX]; i++, pdly++)
			{
				speed = shm->speed;

				// init delay array
				switch (i)
				{
				case ISXRVB:
					delay = room_size.value;
					if (delay >= SXRVB_MAX)
						delay = SXRVB_MAX;
					delaysamples = (int)(delay * (double)speed);
					pdly->mod = SXRVB_MOD_PERIOD1 * (speed / SX_BASE_SPEED);
					break;

				case ISXRVB + 1:
					delay = room_size.value * SXRVB_LINE2_SCALE;
					if (delay >= SXRVB_MAX)
						delay = SXRVB_MAX;
					delaysamples = (int)(delay * (double)speed);
					pdly->mod = SXRVB_MOD_PERIOD2 * (speed / SX_BASE_SPEED);
					break;
				}

				pdly->modcur = pdly->mod;

				// init delay line if not active
				if (!pdly->lpdelayline)
				{
					pdly->delaysamples = delaysamples;
					SXDLY_Init(i, SXRVB_MAX);
				}

				// do crossfade to new delay if delay has changed
				prevsamples = pdly->delaysamples;
				if (delaysamples != prevsamples)
				{
					pdly->idelayoutputxf = pdly->idelayinput - delaysamples;
					if (pdly->idelayoutputxf < 0)
						pdly->idelayoutputxf += pdly->cdelaysamplesmax;
					pdly->xfade = SXRVB_XFADE;
				}

				// deactivate delay line if we're turning it off
				if (!prevsamples)
					SXDLY_Free(i);
			}
		}
	}

	rgsxdly[ISXRVB].delayfeed = (int)(room_refl.value * 255.0f);
	rgsxdly[ISXRVB].lp = (int)room_rvblp.value;
	rgsxdly[ISXRVB + 1].delayfeed = rgsxdly[ISXRVB].delayfeed;
	rgsxdly[ISXRVB + 1].lp = (int)room_rvblp.value;
}

/*
================
SXRVB_DoReverb

Two delay lines with feedback, mixed into both channels.
================
*/
void SXRVB_DoReverb(int count)
{
	dly_t					*pdly;
	portable_samplepair_t	*pbuf;
	int						left, right;
	int						vlr;
	short					sampledly;
	int						samplexf;
	int						val;
	int						valt;
	int						voutm;
	int						countr;

	if (!rgsxdly[ISXRVB].lpdelayline)
		return;

	pbuf = paintbuffer;
	countr = count;

	while (countr--)
	{
		left = pbuf->left;
		right = pbuf->right;
		vlr = (right + left) >> 1;

		// first delay line
		pdly = &rgsxdly[ISXRVB];

		if (--pdly->modcur < 0)
			pdly->modcur = pdly->mod;

		sampledly = pdly->lpdelayline[pdly->idelayoutput];

		if (!pdly->xfade && !sampledly && !left && !right)
		{
			// delay line and input are silent
			pdly->lp0 = 0;
			pdly->lpdelayline[pdly->idelayinput] = 0;
			valt = 0;
		}
		else
		{
			// pick a new random delay to crossfade to
			if (!pdly->xfade && !pdly->mod)
			{
				pdly->idelayoutputxf = pdly->idelayinput + pdly->delaysamples * rand() / (-2 * RAND_MAX) - pdly->delaysamples;
				if (pdly->idelayoutputxf < 0)
					pdly->idelayoutputxf += pdly->cdelaysamplesmax;
				pdly->xfade = SXRVB_XFADE;
			}

			if (pdly->xfade)
			{
				samplexf = pdly->lpdelayline[pdly->idelayoutputxf++];
				sampledly = (short)((((SXRVB_XFADE - pdly->xfade) * samplexf) >> SXRVB_XFADEBITS)
					+ ((pdly->xfade * sampledly) >> SXRVB_XFADEBITS));

				if (pdly->idelayoutputxf >= pdly->cdelaysamplesmax)
					pdly->idelayoutputxf = 0;

				if (!--pdly->xfade)
					pdly->idelayoutput = pdly->idelayoutputxf;
			}

			// mix in feedback
			if (sampledly)
			{
				val = vlr + ((pdly->delayfeed * sampledly) >> 8);
				CLIP16(val);
			}
			else
			{
				val = vlr;
			}

			if (pdly->lp)
			{
				// 2 tap lowpass
				valt = (val + pdly->lp0) >> 1;
				pdly->lp0 = val;
			}
			else
			{
				valt = val;
			}

			pdly->lpdelayline[pdly->idelayinput] = valt;
		}

		if (++pdly->idelayinput >= pdly->cdelaysamplesmax)
			pdly->idelayinput = 0;

		if (++pdly->idelayoutput >= pdly->cdelaysamplesmax)
			pdly->idelayoutput = 0;

		voutm = valt;

		// second delay line
		pdly = &rgsxdly[ISXRVB + 1];

		if (--pdly->modcur < 0)
			pdly->modcur = pdly->mod;

		if (pdly->lpdelayline)
		{
			sampledly = pdly->lpdelayline[pdly->idelayoutput];

			if (!pdly->xfade && !sampledly && !left && !right)
			{
				// delay line and input are silent
				pdly->lp0 = 0;
				pdly->lpdelayline[pdly->idelayinput] = 0;
				valt = 0;
			}
			else
			{
				// pick a new random delay to crossfade to
				if (!pdly->xfade && !pdly->mod)
				{
					pdly->idelayoutputxf = pdly->idelayinput + pdly->delaysamples * rand() / (-2 * RAND_MAX) - pdly->delaysamples;
					if (pdly->idelayoutputxf < 0)
						pdly->idelayoutputxf += pdly->cdelaysamplesmax;
					pdly->xfade = SXRVB_XFADE;
				}

				if (pdly->xfade)
				{
					samplexf = pdly->lpdelayline[pdly->idelayoutputxf++];
					sampledly = (short)((((SXRVB_XFADE - pdly->xfade) * samplexf) >> SXRVB_XFADEBITS)
						+ ((pdly->xfade * sampledly) >> SXRVB_XFADEBITS));

					if (pdly->idelayoutputxf >= pdly->cdelaysamplesmax)
						pdly->idelayoutputxf = 0;

					if (!--pdly->xfade)
						pdly->idelayoutput = pdly->idelayoutputxf;
				}

				// mix in feedback
				if (sampledly)
				{
					val = vlr + ((pdly->delayfeed * sampledly) >> 8);
					CLIP16(val);
				}
				else
				{
					val = vlr;
				}

				if (pdly->lp)
				{
					// 2 tap lowpass
					valt = (val + pdly->lp0) >> 1;
					pdly->lp0 = val;
				}
				else
				{
					valt = val;
				}

				pdly->lpdelayline[pdly->idelayinput] = valt;
			}

			if (++pdly->idelayinput >= pdly->cdelaysamplesmax)
				pdly->idelayinput = 0;

			if (++pdly->idelayoutput >= pdly->cdelaysamplesmax)
				pdly->idelayoutput = 0;

			voutm += valt;
		}

		// mix the reverb into the output
		left = voutm / 6 + left;
		right = voutm / 6 + right;
		CLIP16(left);
		CLIP16(right);
		pbuf->left = left;
		pbuf->right = right;

		pbuf++;
	}
}

/*
================
SX_RoomFX

Lowpass and amplitude modulation over the whole mix.
================
*/
void SX_RoomFX(int count)
{
	portable_samplepair_t	*pbuf;
	int						left, right;
	int						vl, vr;
	int						vrsum;
	qboolean				flp, fmod;
	int						countr;

	if (room_lp.value == 0.0f && room_mod.value == 0.0f)
		return;

	pbuf = paintbuffer;
	flp = (room_lp.value != 0.0f);
	fmod = (room_mod.value != 0.0f);
	countr = count;

	while (countr--)
	{
		left = pbuf->left;
		right = pbuf->right;
		vl = left;
		vr = right;

		if (flp)
		{
			// lowpass over the sample history
			vl = (vl + rgsxlp[0] + rgsxlp[4] + rgsxlp[3] + rgsxlp[2] + rgsxlp[1]) / 4;

			vrsum = rgsxlp[8] + rgsxlp[7] + rgsxlp[6] + rgsxlp[5] + rgsxlp[9];
			rgsxlp[9] = right;
			vr = (vrsum + right) / 4;

			rgsxlp[0] = rgsxlp[1];
			rgsxlp[1] = rgsxlp[2];
			rgsxlp[2] = rgsxlp[3];
			rgsxlp[3] = left;
			rgsxlp[4] = rgsxlp[5];
			rgsxlp[5] = rgsxlp[6];
			rgsxlp[6] = rgsxlp[7];
			rgsxlp[7] = rgsxlp[8];
			rgsxlp[8] = right;
		}

		if (fmod)
		{
			// pick new random modulation targets; min() calls rand() twice
			if (--sxmod_count_left < 0)
				sxmod_count_left = sxmod_period_left;

			if (!sxmod_period_left)
				sxamod_target_left = min(255, 255 * rand() / RAND_MAX + SXAMOD_MIN);

			if (--sxmod_count_right < 0)
				sxmod_count_right = sxmod_period_right;

			if (!sxmod_period_right)
				sxamod_target_right = min(255, 255 * rand() / RAND_MAX + SXAMOD_MIN);

			vl = (sxamod_left * vl) >> 8;
			vr = (sxamod_right * vr) >> 8;

			// move the modulation toward its target
			if (sxamod_target_left > sxamod_left)
				sxamod_left++;
			else if (sxamod_target_left < sxamod_left)
				sxamod_left--;

			if (sxamod_target_right > sxamod_right)
				sxamod_right++;
			else if (sxamod_target_right < sxamod_right)
				sxamod_right--;
		}

		CLIP16(vl);
		CLIP16(vr);
		pbuf->left = vl;
		pbuf->right = vr;
		pbuf++;
	}
}

// room_type presets
typedef struct sx_preset_s
{
	float	room_lp;
	float	room_mod;
	float	room_size;
	float	room_refl;
	float	room_rvblp;
	float	room_delay;
	float	room_feedback;
	float	room_dlylp;
	float	room_left;
} sx_preset_t;

#define CSXROOM		29

static const sx_preset_t rgsxpre[CSXROOM] =
{
//	  lp    mod   size    refl    rvblp delay   feedback dlylp left
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.0f,   0.0f,   2.0f, 0.0f },	// 0: off
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.08f,  0.8f,   2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.02f,  0.75f,  0.0f, 0.001f },
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.03f,  0.78f,  0.0f, 0.002f },
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.06f,  0.77f,  0.0f, 0.003f },
	{ 0.0f, 0.0f, 0.05f,  0.85f,  1.0f, 0.008f, 0.96f,  2.0f, 0.01f },
	{ 0.0f, 0.0f, 0.05f,  0.88f,  1.0f, 0.01f,  0.98f,  2.0f, 0.02f },
	{ 0.0f, 0.0f, 0.05f,  0.92f,  1.0f, 0.015f, 0.995f, 2.0f, 0.04f },
	{ 0.0f, 0.0f, 0.05f,  0.84f,  1.0f, 0.0f,   0.0f,   2.0f, 0.003f },
	{ 0.0f, 0.0f, 0.05f,  0.9f,   1.0f, 0.0f,   0.0f,   2.0f, 0.002f },
	{ 0.0f, 0.0f, 0.05f,  0.95f,  1.0f, 0.0f,   0.0f,   2.0f, 0.001f },
	{ 0.0f, 0.0f, 0.05f,  0.7f,   0.0f, 0.0f,   0.0f,   2.0f, 0.003f },
	{ 0.0f, 0.0f, 0.055f, 0.78f,  0.0f, 0.0f,   0.0f,   2.0f, 0.002f },
	{ 0.0f, 0.0f, 0.05f,  0.86f,  0.0f, 0.0f,   0.0f,   2.0f, 0.001f },
	{ 1.0f, 1.0f, 0.0f,   0.0f,   1.0f, 0.0f,   0.0f,   2.0f, 0.01f },
	{ 1.0f, 1.0f, 0.0f,   0.0f,   1.0f, 0.06f,  0.85f,  2.0f, 0.02f },
	{ 1.0f, 1.0f, 0.0f,   0.0f,   1.0f, 0.2f,   0.6f,   2.0f, 0.05f },
	{ 0.0f, 0.0f, 0.05f,  0.8f,   1.0f, 0.15f,  0.48f,  2.0f, 0.008f },
	{ 0.0f, 0.0f, 0.06f,  0.9f,   1.0f, 0.22f,  0.52f,  2.0f, 0.005f },
	{ 0.0f, 0.0f, 0.07f,  0.94f,  1.0f, 0.3f,   0.6f,   2.0f, 0.001f },
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.3f,   0.42f,  2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.35f,  0.48f,  2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.38f,  0.6f,   2.0f, 0.0f },
	{ 0.0f, 0.0f, 0.05f,  0.9f,   1.0f, 0.2f,   0.28f,  0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.07f,  0.9f,   1.0f, 0.3f,   0.4f,   0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.09f,  0.9f,   1.0f, 0.35f,  0.5f,   0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.01f,  0.9f,   0.0f, 0.0f,   0.0f,   2.0f, 0.05f },
	{ 0.0f, 0.0f, 0.0f,   0.0f,   1.0f, 0.009f, 0.999f, 2.0f, 0.04f },
	{ 0.0f, 0.0f, 0.001f, 0.999f, 0.0f, 0.2f,   0.8f,   2.0f, 0.05f },
};

/*
================
SX_RoomProcess

Main routine for the room effects, called once per paintbuffer.
================
*/
void SX_RoomProcess(int count)
{
	qboolean			fchanged;
	int					i;
	const sx_preset_t	*ppre;

	if (room_off.value == 0.0f)
	{
		fchanged = false;

		// set up the room_* cvars when room_type changed
		if (sxroom_type_prev != room_type.value)
		{
			sxroom_type_prev = room_type.value;

			i = (int)room_type.value;
			if ((unsigned int)i < CSXROOM)
			{
				ppre = &rgsxpre[i];
				Cvar_SetValue("room_lp", ppre->room_lp);
				Cvar_SetValue("room_mod", ppre->room_mod);
				Cvar_SetValue("room_size", ppre->room_size);
				Cvar_SetValue("room_refl", ppre->room_refl);
				Cvar_SetValue("room_rvblp", ppre->room_rvblp);
				Cvar_SetValue("room_delay", ppre->room_delay);
				Cvar_SetValue("room_feedback", ppre->room_feedback);
				Cvar_SetValue("room_dlylp", ppre->room_dlylp);
				Cvar_SetValue("room_left", ppre->room_left);
			}

			SXRVB_CheckNewReverbVal();
			SXDLY_CheckNewDelayVal();
			SXDLY_CheckNewStereoDelayVal();

			fchanged = true;
		}

		if (fchanged || room_type.value != 0.0f)
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
