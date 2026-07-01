/***
*
*Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*This product contains software technology licensed from Id
*Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

//=========================================================
// Animating - CBaseAnimating implementation
//=========================================================

#include <math.h>
#include "animating.h"
#include "enginefuncs.h"
#include "utils.h"

typedef struct StudioHeader
{
	unsigned char	pad0[92];
	int		numseq;
	int		seqindex;
} StudioHeader;

static unsigned char *GetSequenceDesc(const StudioHeader *hdr, int sequence)
{
	unsigned char *base;

	if (!hdr)
		return NULL;

	if (sequence < 0 || sequence >= hdr->numseq)
		return NULL;

	base = (unsigned char *)hdr;
	return base + hdr->seqindex + 104 * sequence;
}

static void GetSequenceInfo(void *pModel, entvars_t *pev, float *pFrameRate, float *pGroundSpeed)
{
	StudioHeader *hdr;
	unsigned char *seq;
	int sequence;
	int numFrames;
	float fps;
	float moveX;
	float moveY;
	float moveZ;
	float moveLength;

	if (!pFrameRate || !pGroundSpeed)
		return;

	if (!pModel || !pev)
		return;

	hdr = (StudioHeader *)pModel;
	sequence = PevInt(pev, PEV_SEQUENCE);
	seq = GetSequenceDesc(hdr, sequence);
	if (!seq)
	{
		*pFrameRate = 0.0f;
		*pGroundSpeed = 0.0f;
		return;
	}

	numFrames = *(int *)(seq + 48);
	if (!numFrames)
	{
		*pFrameRate = 256.0f;
		*pGroundSpeed = 0.0f;
		return;
	}

	fps = *(float *)(seq + 32);
	*pFrameRate = fps / (float)numFrames * 256.0f;

	moveX = *(float *)(seq + 76);
	moveY = *(float *)(seq + 80);
	moveZ = *(float *)(seq + 84);
	moveLength = (float)sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
	*pGroundSpeed = moveLength / (float)numFrames * fps;
}

static int GetAnimationEventFlags(void *pModel, entvars_t *pev, float interval)
{
	StudioHeader *hdr;
	unsigned char *seq;
	int sequence;
	int numEvents;
	int eventIndex;
	int numFrames;
	float startFrame;
	float endFrame;
	short *events;
	int flags;
	int i;

	if (!pModel || !pev)
		return 0;

	hdr = (StudioHeader *)pModel;
	sequence = PevInt(pev, PEV_SEQUENCE);
	seq = GetSequenceDesc(hdr, sequence);
	if (!seq)
		return 0;

	numEvents = *(int *)(seq + 40);
	if (!numEvents)
		return 0;

	eventIndex = *(int *)(seq + 44);
	events = (short *)((unsigned char *)hdr + eventIndex);

	numFrames = *(int *)(seq + 48);
	startFrame = (float)numFrames * 0.00390625f * PevFloat(pev, PEV_FRAME);

	flags = 0;
	for (i = 0; i < numEvents; i++)
	{
		float frame = (float)events[0];
		if (frame >= startFrame)
		{
			endFrame = *(float *)(seq + 32) * interval + startFrame;
			if (endFrame > frame)
			{
				int eventType = *((unsigned char *)events + 2);
				flags |= (1 << (eventType & 31));
			}
		}

		events += 2;
	}

	return flags;
}

CBaseAnimating::CBaseAnimating()
{
	m_flFrameRate = 0.0f;
	m_flGroundSpeed = 0.0f;
	m_fSequenceFinished = 0;
}

int CBaseAnimating::GetAnimationEventFlags(float interval)
{
	void *pModel;

	if (!pev)
		return 0;

	pModel = EngineGetModelPtr(EdictFromEntvars(pev));
	return ::GetAnimationEventFlags(pModel, pev, interval);
}

void CBaseAnimating::ResetSequenceInfo(float intervalScale)
{
	void *pModel;

	if (!pev)
		return;

	pModel = EngineGetModelPtr(EdictFromEntvars(pev));
	GetSequenceInfo(pModel, pev, &m_flFrameRate, &m_flGroundSpeed);

	PevFloat(pev, PEV_ANIMTIME) = GlobalTime();
	PevFloat(pev, PEV_FRAMERATE) = 1.0f;

	m_fSequenceFinished = 0;
	m_flGroundSpeed = m_flGroundSpeed * intervalScale;
}

void CBaseAnimating::AdvanceAnimation(float interval)
{
	float framerate;
	float time;
	float predictedFrame;

	if (!pev)
		return;

	float &animTime = PevFloat(pev, PEV_ANIMTIME);
	float &frame = PevFloat(pev, PEV_FRAME);

	framerate = PevFloat(pev, PEV_FRAMERATE);
	time = GlobalTime();

	if ((*(int *)&animTime & 0x7FFFFFFF) != 0)
		frame = (time - animTime) * framerate * m_flFrameRate + frame;

	animTime = time;

	if (*(unsigned int *)&frame > 0x80000000U)
		frame = (float)(int)(frame * 0.00390625f) * -256.0f + frame;

	if (*(int *)&frame >= 1132462080)
		frame = (float)(int)(frame * 0.00390625f) * -256.0f + frame;

	m_fSequenceFinished = 0;

	predictedFrame = framerate * m_flFrameRate * interval + frame;
	if (m_flFrameRate <= 0.0f || predictedFrame <= 256.0f)
	{
		if (predictedFrame <= 0.0f)
			m_fSequenceFinished = 1;
	}
	else
	{
		m_fSequenceFinished = 1;
	}
}

