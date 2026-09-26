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
//=========================================================
// CBaseAnimating - studio model animation
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "studio.h"

static mstudioseqdesc_t *GetSequenceDesc(studiohdr_t *pstudiohdr, int sequence)
{
	if (!pstudiohdr)
		return NULL;

	if (sequence < 0 || sequence >= pstudiohdr->numseq)
		return NULL;

	return (mstudioseqdesc_t *)((unsigned char *)pstudiohdr + pstudiohdr->seqindex) + sequence;
}

void GetSequenceInfo(void *pmodel, entvars_t *pev, float *pflFrameRate, float *pflGroundSpeed)
{
	if (!pflFrameRate || !pflGroundSpeed)
		return;

	if (!pmodel || !pev)
		return;

	mstudioseqdesc_t *pseqdesc = GetSequenceDesc((studiohdr_t *)pmodel, pev->sequence);
	if (!pseqdesc)
	{
		*pflFrameRate = 0.0f;
		*pflGroundSpeed = 0.0f;
		return;
	}

	int numframes = pseqdesc->numframes;
	if (!numframes)
	{
		*pflFrameRate = 256.0f;
		*pflGroundSpeed = 0.0f;
		return;
	}

	float fps = pseqdesc->fps;
	*pflFrameRate = fps / (float)numframes * 256.0f;
	*pflGroundSpeed = pseqdesc->linearmovement.Length() / (float)numframes * fps;
}

// Returns a mask with bit (event type) set for every event reached during the next flInterval seconds.
static int GetAnimationEventFlags(void *pmodel, entvars_t *pev, float flInterval)
{
	if (!pmodel || !pev)
		return 0;

	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;
	mstudioseqdesc_t *pseqdesc = GetSequenceDesc(pstudiohdr, pev->sequence);
	if (!pseqdesc)
		return 0;

	int numevents = pseqdesc->numevents;
	if (!numevents)
		return 0;

	mstudioevent_t *pevent = (mstudioevent_t *)((unsigned char *)pstudiohdr + pseqdesc->eventindex);
	float flStart = (float)pseqdesc->numframes / 256.0f * pev->frame;

	int flags = 0;
	for (int i = 0; i < numevents; i++)
	{
		float flFrame = (float)pevent[i].frame;
		if (flFrame >= flStart)
		{
			float flEnd = pseqdesc->fps * flInterval + flStart;
			if (flEnd > flFrame)
				flags |= 1 << (pevent[i].type & 31);
		}
	}

	return flags;
}

CBaseAnimating::CBaseAnimating()
{
	m_flFrameRate = 0.0f;
	m_flGroundSpeed = 0.0f;
	m_fSequenceFinished = FALSE;
}

int CBaseAnimating::GetAnimationEventFlags(float flInterval)
{
	if (!pev)
		return 0;

	return ::GetAnimationEventFlags(GET_MODEL_PTR(ENT(pev)), pev, flInterval);
}

//=========================================================
// ResetSequenceInfo - picks up the frame rate and ground
// speed (per flInterval) of the current sequence
//=========================================================
void CBaseAnimating::ResetSequenceInfo(float flInterval)
{
	if (!pev)
		return;

	GetSequenceInfo(GET_MODEL_PTR(ENT(pev)), pev, &m_flFrameRate, &m_flGroundSpeed);

	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0f;

	m_fSequenceFinished = FALSE;
	m_flGroundSpeed = m_flGroundSpeed * flInterval;
}

//=========================================================
// AdvanceAnimation - advances the frame and flags the end
// of the sequence when it will be reached within flInterval
//=========================================================
void CBaseAnimating::AdvanceAnimation(float flInterval)
{
	if (!pev)
		return;

	float flFrameRate = pev->framerate;
	float flTime = gpGlobals->time;

	if (pev->animtime != 0.0f)
		pev->frame = (flTime - pev->animtime) * flFrameRate * m_flFrameRate + pev->frame;

	pev->animtime = flTime;

	// keep the frame within 0..256
	if (pev->frame < 0.0f)
		pev->frame -= (int)(pev->frame / 256.0f) * 256.0f;

	if (pev->frame >= 256.0f)
		pev->frame -= (int)(pev->frame / 256.0f) * 256.0f;

	m_fSequenceFinished = FALSE;

	float flNextFrame = flFrameRate * m_flFrameRate * flInterval + pev->frame;
	if (m_flFrameRate <= 0.0f || flNextFrame <= 256.0f)
	{
		if (flNextFrame <= 0.0f)
			m_fSequenceFinished = TRUE;
	}
	else
	{
		m_fSequenceFinished = TRUE;
	}
}
