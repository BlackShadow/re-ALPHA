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
// Houndeye - Houndeye monster
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// monster-specific constants
//=========================================================

#define HOUNDEYE_THINK_INTERVAL		0.1f
#define HOUNDEYE_MAX_ATTACK_DIST	1024.0f
#define HOUNDEYE_ATTACK_RADIUS		384.0f
#define HOUNDEYE_ATTACK_COOLDOWN	9.0f

#define HOUNDEYE_ATTACK_DMG_MIN		30
#define HOUNDEYE_ATTACK_DMG_MAX		50

#define HOUNDEYE_VOL				1.0f
#define HOUNDEYE_ATTN_IDLE			2.0f
#define HOUNDEYE_ATTN_COMBAT		0.8f

static const char kHoundeyeModel[] = "models/houndeye.mdl";

//=========================================================
// Sound Table
//=========================================================

static const char* pAlertSounds[] =
{
	"houndeye/he_alert1.wav",
};

static const char* pDieSounds[] =
{
	"houndeye/he_die1.wav",
	"houndeye/he_die2.wav",
	"houndeye/he_die3.wav",
};

static const char* pIdleSounds[] =
{
	"houndeye/he_idle1.wav",
	"houndeye/he_idle2.wav",
	"houndeye/he_idle3.wav",
	"houndeye/he_idle4.wav",
};

static const char* pAttackSounds[] =
{
	"houndeye/he_attack1.wav",
	"houndeye/he_attack2.wav",
	"houndeye/he_attack3.wav",
};

static const char* pPainSounds[] =
{
	"houndeye/he_pain1.wav",
	"houndeye/he_pain2.wav",
	"houndeye/he_pain4.wav",
	"houndeye/he_pain5.wav",
};

//=========================================================
// Helpers
//=========================================================

static entvars_t* EnemyVars(entvars_t* pevSelf)
{
	if (!pevSelf)
		return NULL;

	int enemyIndex = PevInt(pevSelf, PEV_ENEMY);
	if (!enemyIndex)
		return NULL;

	edict_t* pEdict = EnginePEntityOfEntIndex(enemyIndex);
	return pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
}

//=========================================================
// CHoundeye
//=========================================================

class CHoundeye : public CBaseMonster
{
public:
	CHoundeye();

	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);
	void IdleSound();
	void AlertSound();
	int CheckAttacks(entvars_t* pevEnemy, float flDist);

	void Pain(float flDamage);		// vtable slot 13
	void Death(int gibType);		// vtable slot 14
	void SonicAttackThink(CBaseEntity* pOther);
	void SonicFollowThink(CBaseEntity* pOther);

private:
	int m_fSonicActive;
};

HL_COMPILE_TIME_ASSERT(sizeof(CHoundeye) <= 336, CHoundeye_private_data_size);

CHoundeye::CHoundeye()
{
	m_fSonicActive = 0;
}

//=========================================================
// Spawn
//=========================================================
void CHoundeye::Spawn()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pAlertSounds); ++i)
		EnginePrecacheSound(pAlertSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); ++i)
		EnginePrecacheSound(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pIdleSounds); ++i)
		EnginePrecacheSound(pIdleSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pAttackSounds); ++i)
		EnginePrecacheSound(pAttackSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); ++i)
		EnginePrecacheSound(pPainSounds[i]);

	EnginePrecacheModel(kHoundeyeModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kHoundeyeModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 18.0f, 18.0f, 36.0f);
		VecSet(mins, -18.0f, -18.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 15.0f;
	PevFloat(pev, PEV_YAWSPEED) = 10.0f;
	PevInt(pev, PEV_SEQUENCE) = 6;

	m_flDistTooFar = 128.0f;
	m_bloodColor = 195;
	m_iSquadSize = 1;

	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + 1.0f;
	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// Classify
//=========================================================
int CHoundeye::Classify()
{
	return 3;
}

//=========================================================
// SetActivity
//=========================================================
void CHoundeye::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 1:
		sequence = 1;
		break;
	case 2:
		sequence = 0;
		break;
	case 3:
		sequence = 2;
		break;
	case 4:
	case 8:
	case 9:
		sequence = 4;
		break;
	case 6:
	case 7:
		sequence = 1;
		break;
	case 29:
	case 30:
		sequence = 3;
		break;
	case 35:
		sequence = 5;
		break;
	default:
		EngineAlertMessage(1, "Houndeye's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(HOUNDEYE_THINK_INTERVAL);

	volatile int validatedSequence = sequence;
	if (validatedSequence < 0 || validatedSequence > 5)
	{
		EngineAlertMessage(1, "Bogus Houndeye anim: %d", validatedSequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
	}
}

//=========================================================
// CheckAttacks
//=========================================================
int CHoundeye::CheckAttacks(entvars_t* pevEnemy, float flDist)
{
	if (!CheckRangeAttack(pevEnemy) || flDist > HOUNDEYE_MAX_ATTACK_DIST)
		return 0;

	m_IdealActivity = 7;
	SetThink(&CHoundeye::SonicAttackThink);
	return 1;
}

//=========================================================
// AlertSound
//=========================================================
void CHoundeye::AlertSound()
{
	if (m_iSquadSize > 1)
		return;

	int recruits = SquadRecruit(512);
	if (!recruits)
	{
		EngineAlertMessage(1, "No Squad\n");
		m_Activity = 7;
		return;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, pAlertSounds[0], HOUNDEYE_VOL, HOUNDEYE_ATTN_COMBAT);

	m_iSquadSize += recruits;

	entvars_t* pevMember = m_pSquadNext;
	while (pevMember && pevMember != pev)
	{
		edict_t* pMemberEdict = EdictFromEntvars(pevMember);
		CBaseMonster* pMember = pMemberEdict ? (CBaseMonster*)EngineGetPrivateData(pMemberEdict) : NULL;
		if (!pMember)
			break;

		pMember->SetMonsterActivity(8);
		pMember->SetSquadSize(m_iSquadSize);

		pevMember = pMember->SquadNext();
	}

	EngineAlertMessage(1, "group of: %d\n", m_iSquadSize);

	m_Activity = 8;
	m_fSonicActive = 1;
	m_iSquadSize = recruits;
}

//=========================================================
// IdleSound
//=========================================================
void CHoundeye::IdleSound()
{
	float rnd = RandomFloat(0.0f, 1.0f);
	const char* sample;

	if (rnd <= 0.25f)
		sample = pIdleSounds[0];
	else if (rnd <= 0.5f)
		sample = pIdleSounds[1];
	else if (rnd <= 0.75f)
		sample = pIdleSounds[2];
	else
		sample = pIdleSounds[3];

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, sample, HOUNDEYE_VOL, HOUNDEYE_ATTN_IDLE);

	m_flNextSoundTime = (float)RandomLong(0, 3) + GlobalTime() + 1.0f;
}

//=========================================================
// Pain - vtable slot 13
//=========================================================
void CHoundeye::Pain(float flDamage)
{
	HL_UNUSED(flDamage);

	float rnd = RandomFloat(0.0f, 1.0f);
	const char* sample;

	if (rnd <= 0.25f)
		sample = pPainSounds[0];
	else if (rnd <= 0.5f)
		sample = pPainSounds[1];
	else if (rnd <= 0.75f)
		sample = pPainSounds[2];
	else
		sample = pPainSounds[3];

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, sample, HOUNDEYE_VOL, HOUNDEYE_ATTN_COMBAT);

	if (m_Activity == 1 || m_Activity == 4)
		AlertSound();
}

//=========================================================
// Death - vtable slot 14
//=========================================================
void CHoundeye::Death(int gibType)
{
	HL_UNUSED(gibType);

	float rnd = RandomFloat(0.0f, 1.0f);

	if (PevFloat(pev, PEV_HEALTH) > -30.0f)
	{
		edict_t* edict = EdictFromEntvars(pev);

		if (rnd <= 0.33f)
		{
			if (edict)
				EngineEmitSound(edict, 2, pDieSounds[0], HOUNDEYE_VOL, HOUNDEYE_ATTN_COMBAT);
		}

		const char* sample;
		if (rnd <= 0.66f)
			sample = pDieSounds[1];
		else
			sample = pDieSounds[2];

		if (edict)
			EngineEmitSound(edict, 2, sample, HOUNDEYE_VOL, HOUNDEYE_ATTN_COMBAT);
	}

	// Binary tail-calls the binary(this, 0): the shared SetDeathActivity
	// helper (selects the death animation, faces the corpse along its yaw and
	// hands off to MonsterThink). Death type 0 == DEAD activity (35).
	SetDeathActivity(0);
}

//=========================================================
// TakeDamage routes through the shared CBaseMonster::TakeDamage
// (vtable slot 17), which raises Pain / Death.
//=========================================================

//=========================================================
// SonicAttackThink
//=========================================================
void CHoundeye::SonicAttackThink(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + HOUNDEYE_THINK_INTERVAL;

	if (!m_fSonicActive)
	{
		m_flNextAttack = GlobalTime() + 99999.0f;
		return;
	}

	if (m_Activity != 30)
	{
		int soundIndex = rand() % 3;
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 2, pAttackSounds[soundIndex], HOUNDEYE_VOL, HOUNDEYE_ATTN_COMBAT);

		entvars_t* pevMember = m_pSquadNext;
		while (pevMember && pevMember != pev)
		{
			edict_t* pMemberEdict = EdictFromEntvars(pevMember);
			CBaseMonster* pMember = pMemberEdict ? (CBaseMonster*)EngineGetPrivateData(pMemberEdict) : NULL;
			if (!pMember)
				break;

			if (PevFloat(pevMember, PEV_HEALTH) > 0.0f)
			{
				CHoundeye* pHoundeye = (CHoundeye*)pMember;
				pHoundeye->SetThink(&CHoundeye::SonicFollowThink);
				pHoundeye->SetMonsterIdealActivity(7);
			}

			pevMember = pMember->SquadNext();
		}

		m_Activity = 30;
		SetActivity(30);
	}

	{
		entvars_t* pevEnemy = EnemyVars(pev);
		if (pevEnemy)
			UpdateEnemyInfo(pevEnemy);
	}

	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineChangeYaw(edict);
	}

	GetAnimationEventFlags(HOUNDEYE_THINK_INTERVAL);
	AdvanceAnimation(HOUNDEYE_THINK_INTERVAL);

	if (!m_fSequenceFinished)
		return;

	{
		int attackScale = (int)m_iSquadSize;

		edict_t* edict = EdictFromEntvars(pev);
		Vector vecSrc = PevVector(pev, PEV_ORIGIN);
		edict_t* pEdict = EngineFindEntityInSphere(VecPtr(vecSrc), HOUNDEYE_ATTACK_RADIUS);

		while (pEdict)
		{
			int hitIndex = EngineIndexOfEdict(pEdict);
			if (!hitIndex)
				break;

			entvars_t* pevHit = EngineGetVarsOfEnt(pEdict);
			if (!pevHit)
				break;

			if ((PevInt(pevHit, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
			{
				CBaseEntity* pEntity = (CBaseEntity*)EngineGetPrivateData(pEdict);
				if (!pEntity || pEntity->Classify() != 3)
				{
					float vecEnd[3];
					vecEnd[0] = PevVector(pevHit, PEV_ORIGIN).x;
					vecEnd[1] = PevVector(pevHit, PEV_ORIGIN).y;
					vecEnd[2] = PevVector(pevHit, PEV_ORIGIN).z + PevVector(pevHit, PEV_SIZE).z * 0.5f;

					TraceResult tr;
					memset(&tr, 0, sizeof(tr));
					EngineTraceLine(VecPtr(vecSrc), vecEnd, 0, edict, &tr);

					if (tr.flFraction == 1.0f)
					{
						float vecDelta[3];
						vecDelta[0] = PevVector(pev, PEV_ORIGIN).x - PevVector(pevHit, PEV_ORIGIN).x;
						vecDelta[1] = PevVector(pev, PEV_ORIGIN).y - PevVector(pevHit, PEV_ORIGIN).y;
						vecDelta[2] = PevVector(pev, PEV_ORIGIN).z - PevVector(pevHit, PEV_ORIGIN).z;

						float flDamage = (float)(HOUNDEYE_ATTACK_DMG_MIN * attackScale) - VecLength(vecDelta) * 0.2f;
						if (flDamage < 0.0f)
							flDamage = 0.0f;

						EngineAlertMessage(1, "%f/%f\n", flDamage, (float)(HOUNDEYE_ATTACK_DMG_MAX * attackScale));

						if (pEntity)
							pEntity->TakeDamage(pev, pev, flDamage);
					}
				}
			}

			{
				int nextChain = PevInt(pevHit, PEV_CHAIN);
				pEdict = nextChain ? EnginePEntityOfEntIndex(nextChain) : NULL;
			}
		}
	}

	m_Activity = m_IdealActivity;
	SetThink(&CBaseMonster::MonsterThink);
	m_flNextAttack = GlobalTime() + HOUNDEYE_ATTACK_COOLDOWN;
}

//=========================================================
// SonicFollowThink
//=========================================================
void CHoundeye::SonicFollowThink(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + HOUNDEYE_THINK_INTERVAL;

	if (m_Activity != 30)
	{
		m_Activity = 30;
		SetActivity(30);
	}

	GetAnimationEventFlags(HOUNDEYE_THINK_INTERVAL);
	AdvanceAnimation(HOUNDEYE_THINK_INTERVAL);

	{
		entvars_t* pevEnemy = EnemyVars(pev);
		if (pevEnemy)
			UpdateEnemyInfo(pevEnemy);
	}

	if (m_pSquadLeader)
	{
		float vecLeaderDelta[3];
		vecLeaderDelta[0] = PevVector(m_pSquadLeader, PEV_ORIGIN).x - PevVector(pev, PEV_ORIGIN).x;
		vecLeaderDelta[1] = PevVector(m_pSquadLeader, PEV_ORIGIN).y - PevVector(pev, PEV_ORIGIN).y;
		vecLeaderDelta[2] = PevVector(m_pSquadLeader, PEV_ORIGIN).z - PevVector(pev, PEV_ORIGIN).z;
		PevFloat(pev, PEV_IDEAL_YAW) = EngineVecToYaw(vecLeaderDelta);
	}

	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineChangeYaw(edict);
	}

	if (!m_fSequenceFinished)
		return;

	m_Activity = m_IdealActivity;
	SetThink(&CBaseMonster::MonsterThink);
	m_flNextAttack = GlobalTime() + HOUNDEYE_ATTACK_COOLDOWN;
}

//=========================================================
// monster_houndeye
//=========================================================
DLLEXPORT void monster_houndeye(entvars_t* pev)
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

		CHoundeye* monster = new (privateData) CHoundeye();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
