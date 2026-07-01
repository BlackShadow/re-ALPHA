#pragma once

#include "cbase.h"

class CBaseAnimating : public CBaseEntity
{
public:
	CBaseAnimating();

	int GetAnimationEventFlags(float interval);
	void ResetSequenceInfo(float intervalScale);
	void AdvanceAnimation(float interval);

protected:
	float m_flFrameRate;
	float m_flGroundSpeed;
	int m_fSequenceFinished;
};
