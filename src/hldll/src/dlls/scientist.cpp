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
// Scientist - Scientist monster
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// Monster-specific constants
//=========================================================

#define SCIENTIST_THINK_INTERVAL	0.1f

#define SCIENTIST_VOL				1.0f
#define SCIENTIST_ATTN_COMBAT		0.8f

// pev->body lives at byte offset 136 in the alpha entvars_t and is not yet
// named in utils.h. A value of -1.0 means "pick a random scientist body".
#define PEV_BODY					136
#define FL_ONGROUND					0x200

static const char kScientistModel[] = "models/scientist.mdl";

//=========================================================
// Sound Table
//=========================================================

static const char* pDieSounds[] =
{
	"barney/ba_die1.wav",
	"barney/ba_die2.wav",
	"barney/ba_die3.wav",
	"barney/ba_die4.wav",
};

static const char* pPainSounds[] =
{
	"barney/ba_pain1.wav",
};

//=========================================================
// CScientist
//=========================================================

class CScientist : public CBaseMonster
{
public:
	CScientist();

	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);

	void Pain(float flDamage);		// vtable slot 13
	void Death(int gibType);		// vtable slot 14
	void SetDeathActivity(int type);

private:
	float m_flFollowDist;
};

HL_COMPILE_TIME_ASSERT(sizeof(CScientist) <= 336, CScientist_private_data_size);

CScientist::CScientist()
{
	m_flFollowDist = 0.0f;
}

//=========================================================
// Spawn
//=========================================================
void CScientist::Spawn()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); ++i)
		EnginePrecacheSound(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); ++i)
		EnginePrecacheSound(pPainSounds[i]);

	EnginePrecacheModel(kScientistModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kScientistModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 16.0f, 16.0f, 64.0f);
		VecSet(mins, -16.0f, -16.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 10.0f;
	PevFloat(pev, PEV_YAWSPEED) = 10.0f;
	PevInt(pev, PEV_SEQUENCE) = 11;

	m_flDistTooFar = 999999.0f;
	m_flFollowDist = 128.0f;
	m_bloodColor = 70;

	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + 1.0f;

	// pev->body == -1 selects a random scientist body (0..2).
	if (PevFloat(pev, PEV_BODY) == -1.0f)
		PevFloat(pev, PEV_BODY) = (float)(rand() % 3);

	SetThink(&CBaseMonster::WalkMonsterStart);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.5f;
}

//=========================================================
// Classify
//=========================================================
int CScientist::Classify()
{
	return 5;
}

//=========================================================
// SetActivity
//=========================================================
void CScientist::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 1:
		sequence = 0;
		break;

	case 2:
		sequence = 1;
		break;

	case 3:
		sequence = 2;
		break;

	case 4:
		sequence = 3;
		break;

	case 6:
		sequence = 0;
		break;

	case 8:
	case 9:
		sequence = 5;
		break;

	case 10:
		{
			Vector vecDelta;
			vecDelta.x = PevVector(pev, PEV_ORIGIN).x - PevVector(m_pMoveTarget, PEV_ORIGIN).x;
			vecDelta.y = PevVector(pev, PEV_ORIGIN).y - PevVector(m_pMoveTarget, PEV_ORIGIN).y;
			vecDelta.z = PevVector(pev, PEV_ORIGIN).z - PevVector(m_pMoveTarget, PEV_ORIGIN).z;

			float flDist = (float)sqrt(vecDelta.x * vecDelta.x + vecDelta.y * vecDelta.y + vecDelta.z * vecDelta.z);

			if (m_flFollowDist * 2.0f > flDist)
			{
				if (m_flFollowDist < flDist)
					sequence = 3;
				else
					sequence = 0;
			}
			else
			{
				sequence = 5;
			}
		}
		break;

	case 29:
		sequence = 7;
		break;

	case 35:
	case 38:
		sequence = 10;
		break;

	case 36:
		sequence = 8;
		break;

	case 37:
		sequence = 9;
		break;

	default:
		EngineAlertMessage(3, "Scientist's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	{
		int oldSequence = PevInt(pev, PEV_SEQUENCE);

		if ((sequence == 5 || sequence == 3) && (oldSequence == 5 || oldSequence == 3))
			PevFloat(pev, PEV_FRAME) = PevFloat(pev, PEV_FRAME) + 1.0f;
		else
			PevFloat(pev, PEV_FRAME) = 0.0f;
	}

	PevInt(pev, PEV_SEQUENCE) = sequence;
	ResetSequenceInfo(SCIENTIST_THINK_INTERVAL);

	// The original validates the resolved sequence after ResetSequenceInfo.
	// The switch above never produces the bad values (4 or 6), but keep the
	// error arm for binary-behavior parity.
	switch (sequence)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 5:
	case 7:
	case 8:
	case 9:
	case 10:
		break;

	default:
		EngineAlertMessage(3, "Bogus scientist anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// Pain - vtable slot 13
//=========================================================
void CScientist::Pain(float flDamage)
{
	HL_UNUSED(flDamage);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, pPainSounds[0], SCIENTIST_VOL, SCIENTIST_ATTN_COMBAT);
}

//=========================================================
// SetDeathActivity
//=========================================================
void CScientist::SetDeathActivity(int type)
{
	switch (type)
	{
	case 0:
		m_Activity = 35;
		break;
	case 1:
		m_Activity = 36;
		break;
	case 2:
		m_Activity = 37;
		break;
	case 3:
		m_Activity = 38;
		break;
	case 4:
		m_Activity = 39;
		break;
	default:
		// The binary default arm the binary alerts and leaves m_Activity at its
		// prior value (no write); it does NOT force activity 35.
		EngineAlertMessage(1, "Unknown death type!\n");
		break;
	}

	PevFloat(pev, PEV_IDEAL_YAW) = PevVector(pev, PEV_ANGLES).y;
	SetActivity(m_Activity);
	SetThink(&CBaseMonster::MonsterThink);
	SetNextThink(SCIENTIST_THINK_INTERVAL);
}

//=========================================================
// Death - vtable slot 14
//=========================================================
void CScientist::Death(int gibType)
{
	HL_UNUSED(gibType);

	switch (rand() % 4)
	{
	case 0:
		{
			edict_t* edict = EdictFromEntvars(pev);
			if (edict)
				EngineEmitSound(edict, 2, pDieSounds[0], SCIENTIST_VOL, SCIENTIST_ATTN_COMBAT);
		}
		break;
	case 1:
		{
			edict_t* edict = EdictFromEntvars(pev);
			if (edict)
				EngineEmitSound(edict, 2, pDieSounds[1], SCIENTIST_VOL, SCIENTIST_ATTN_COMBAT);
		}
		break;
	case 2:
		{
			edict_t* edict = EdictFromEntvars(pev);
			if (edict)
				EngineEmitSound(edict, 2, pDieSounds[2], SCIENTIST_VOL, SCIENTIST_ATTN_COMBAT);
		}
		break;
	case 3:
		{
			edict_t* edict = EdictFromEntvars(pev);
			if (edict)
				EngineEmitSound(edict, 2, pDieSounds[3], SCIENTIST_VOL, SCIENTIST_ATTN_COMBAT);
		}
		break;
	}

	// The original compares the raw HEALTH bit pattern as an unsigned dword
	// against -30.0f, which for negative health is equivalent to HEALTH > -30.0.
	// A scientist that is only lightly dead drops into the standard death
	// animation; a more heavily gibbed one (HEALTH <= -30) hops first.
	if (PevFloat(pev, PEV_HEALTH) > -30.0f)
	{
		SetDeathActivity(0);
		return;
	}

	PevVector(pev, PEV_ORIGIN).z = PevVector(pev, PEV_ORIGIN).z + 1.0f;

	// pev->flags is stored as a float but tested/cleared as an integer bit
	// mask, matching the original alpha behavior.
	if (((int)PevFloat(pev, PEV_FLAGS) & FL_ONGROUND) != 0)
	{
		PevFloat(pev, PEV_FLAGS) = PevFloat(pev, PEV_FLAGS) - (float)FL_ONGROUND;

		PevVector(pev, PEV_VELOCITY).x = 0.0f;
		PevVector(pev, PEV_VELOCITY).y = 0.0f;
		PevVector(pev, PEV_VELOCITY).z = 200.0f;
	}

	SetDeathActivity(1);
}

//=========================================================
// TakeDamage routes through the shared CBaseMonster::TakeDamage
// (vtable slot 17), which raises Pain
// Death.
//=========================================================

//=========================================================
// monster_scientist
//=========================================================
DLLEXPORT void monster_scientist(entvars_t* pev)
{
	entvars_t* entvars = pev;
	if (!entvars)
	{
		edict_t* created = EngineCreateEntity();
		entvars = created ? EngineGetVarsOfEnt(created) : NULL;
	}

	edict_t* edict = EdictFromEntvars(entvars);
	if (!edict)
		return;

	void* privateData = EngineGetPrivateData(edict);
	if (!privateData)
	{
		privateData = EngineAllocPrivateData(edict, 336);
		if (!privateData)
			return;

		memset(privateData, 0, 336);

		CScientist* monster = new (privateData) CScientist();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
