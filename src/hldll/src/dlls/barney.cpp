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
// Barney - Security guard monster
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

#define BARNEY_THINK_INTERVAL	0.1f
#define BARNEY_MELEE_DIST		64.0f
#define BARNEY_RANGE_DIST		1024.0f

#define BARNEY_BULLET_SPREAD	0.05f

#define BARNEY_VOL				1.0f
#define BARNEY_ATTN_IDLE		2.0f
#define BARNEY_ATTN_COMBAT		0.8f

static const char kBarneyModel[] = "models/barney.mdl";

//=========================================================
// Sound Table
//=========================================================

static const char* pAttackSounds[] =
{
	"barney/ba_attack1.wav",
	"barney/ba_attack2.wav",
};

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
// CBarney
//=========================================================

class CBarney : public CBaseMonster
{
public:
	CBarney();

	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);
	int CheckAttacks(entvars_t* pevEnemy, float flDist);

	void AlertSound();
	void Pain(float flDamage);		// vtable slot 13
	void Death(int gibType);		// vtable slot 14

	void ShootThink(CBaseEntity* pOther);

private:
	float m_flFollowDist;
};

HL_COMPILE_TIME_ASSERT(sizeof(CBarney) <= 336, CBarney_private_data_size);

CBarney::CBarney()
{
	m_flFollowDist = 0.0f;
}

//=========================================================
// Spawn
//=========================================================
void CBarney::Spawn()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pAttackSounds); ++i)
		EnginePrecacheSound(pAttackSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); ++i)
		EnginePrecacheSound(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); ++i)
		EnginePrecacheSound(pPainSounds[i]);

	EnginePrecacheModel(kBarneyModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kBarneyModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 18.0f, 18.0f, 72.0f);
		VecSet(mins, -18.0f, -18.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 7.0f;
	PevFloat(pev, PEV_YAWSPEED) = 8.0f;
	PevInt(pev, PEV_SEQUENCE) = 17;

	m_iSquadSize = 1;
	m_bloodColor = 70;
	m_flDistTooFar = 384.0f;
	m_flFollowDist = 128.0f;

	PevFloat(pev, PEV_NEXTTHINK) = RandomFloat(0.0f, 0.5f) + PevFloat(pev, PEV_NEXTTHINK) + 0.5f;

	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// SetActivity
//=========================================================
void CBarney::SetActivity(int activity)
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
	case 7:
		sequence = 9;
		break;

	case 8:
	case 9:
	case 29:
		sequence = 4;
		break;

	case 10:
		{
			Vector vecDelta;
			vecDelta.x = PevVector(pev, PEV_ORIGIN).x - PevVector(m_pMoveTarget, PEV_ORIGIN).x;
			vecDelta.y = PevVector(pev, PEV_ORIGIN).y - PevVector(m_pMoveTarget, PEV_ORIGIN).y;
			vecDelta.z = PevVector(pev, PEV_ORIGIN).z - PevVector(m_pMoveTarget, PEV_ORIGIN).z;

			float flDist = (float)sqrt(vecDelta.x * vecDelta.x + vecDelta.y * vecDelta.y + vecDelta.z * vecDelta.z);

			if (m_flFollowDist * 2.0f >= flDist)
			{
				if (m_flFollowDist < flDist)
					sequence = 3;
				else
					sequence = 0;
			}
			else
			{
				sequence = 4;
			}
		}
		break;

	case 30:
		sequence = 10;
		break;

	case 35:
	case 38:
		sequence = 14;
		break;

	case 36:
		sequence = 15;
		break;

	case 37:
		sequence = 13;
		break;

	default:
		EngineAlertMessage(1, "Barney's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;

	if (sequence != 4 && sequence != 3)
		PevFloat(pev, PEV_FRAME) = 0.0f;

	ResetSequenceInfo(BARNEY_THINK_INTERVAL);

	switch (sequence)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 9:
	case 10:
	case 12:
	case 13:
	case 14:
	case 15:
		break;

	default:
		EngineAlertMessage(1, "Bogus Barney anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// Classify (returns 5)
//=========================================================
int CBarney::Classify()
{
	return 5;
}

//=========================================================
// CheckAttacks
//=========================================================
int CBarney::CheckAttacks(entvars_t* pevEnemy, float flDist)
{
	if (flDist <= BARNEY_MELEE_DIST && CheckMeleeAttack(pevEnemy))
	{
		m_IdealActivity = 7;
		SetThink(&CBarney::ShootThink);
		return 1;
	}

	if (CheckRangeAttack(pevEnemy) && flDist <= BARNEY_RANGE_DIST)
	{
		m_IdealActivity = 7;
		SetThink(&CBarney::ShootThink);
		return 1;
	}

	return 0;
}

//=========================================================
// AlertSound
//=========================================================
void CBarney::AlertSound()
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, pAttackSounds[0], BARNEY_VOL, BARNEY_ATTN_COMBAT);

	m_Activity = 6;
	m_flNextAttack = GlobalTime() + 1.0f;
}

//=========================================================
// Pain - vtable slot 13
//=========================================================
void CBarney::Pain(float flDamage)
{
	HL_UNUSED(flDamage);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, pPainSounds[0], BARNEY_VOL, BARNEY_ATTN_COMBAT);

	// The original tail-calls the alert/signal virtual (slot 12).
	AlertSound();
}

//=========================================================
// Death - vtable slot 14
//=========================================================
void CBarney::Death(int gibType)
{
	HL_UNUSED(gibType);

	if (PevFloat(pev, PEV_HEALTH) <= -30.0f)
	{
		SetDeathActivity(1);
		return;
	}

	int index = rand() % 4;
	edict_t* edict = EdictFromEntvars(pev);

	switch (index)
	{
	case 0:
		if (edict)
			EngineEmitSound(edict, 2, pDieSounds[0], BARNEY_VOL, BARNEY_ATTN_COMBAT);
		break;
	case 1:
		if (edict)
			EngineEmitSound(edict, 2, pDieSounds[1], BARNEY_VOL, BARNEY_ATTN_COMBAT);
		break;
	case 2:
		if (edict)
			EngineEmitSound(edict, 2, pDieSounds[2], BARNEY_VOL, BARNEY_ATTN_COMBAT);
		break;
	case 3:
		if (edict)
			EngineEmitSound(edict, 2, pDieSounds[3], BARNEY_VOL, BARNEY_ATTN_COMBAT);
		break;
	}

	SetDeathActivity(0);
}

//=========================================================
// TakeDamage routes through the shared CBaseMonster::TakeDamage
// (vtable slot 17), which raises Pain
// Death.
//=========================================================

//=========================================================
// ShootThink
//=========================================================
void CBarney::ShootThink(CBaseEntity* pOther)
{
	SetNextThink(BARNEY_THINK_INTERVAL);

	if (m_Activity != 30)
	{
		m_Activity = 30;
		SetActivity(30);
		m_flNextAttack = RandomFloat(0.5f, 1.5f) + GlobalTime();
	}

	int events = GetAnimationEventFlags(BARNEY_THINK_INTERVAL);
	AdvanceAnimation(BARNEY_THINK_INTERVAL);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineChangeYaw(edict);

	if ((events & 2) != 0)
	{
		if (edict)
			EngineEmitSound(edict, 2, pAttackSounds[1], BARNEY_VOL, BARNEY_ATTN_COMBAT);

		EngineMakeVectors((const float*)&PevVector(pev, PEV_ANGLES));

		void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
		const float* forward = GlobalsForward(globals);

		float dir[3];
		dir[0] = forward ? forward[0] : 0.0f;
		dir[1] = forward ? forward[1] : 0.0f;
		dir[2] = forward ? forward[2] : 0.0f;

		FireBullets(1, dir, BARNEY_BULLET_SPREAD, BARNEY_BULLET_SPREAD, 0, BARNEY_RANGE_DIST);

		// pev->effects |= EF_MUZZLEFLASH (the field at +140 is stored as
		// a float but OR'd as an integer, matching the original).
		PevFloat(pev, PEV_STUCK) = (float)(((int)PevFloat(pev, PEV_STUCK)) | 2);
	}

	if (m_fSequenceFinished)
	{
		SetThink(&CBaseMonster::MonsterThink);
		m_Activity = m_IdealActivity;
	}
}

//=========================================================
// monster_barney
//=========================================================
DLLEXPORT void monster_barney(entvars_t* pev)
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

		CBarney* monster = new (privateData) CBarney();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
