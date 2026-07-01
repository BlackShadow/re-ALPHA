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
// Headcrab - Headcrab monster
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

#define HC_AE_LEAP 2

#define HEADCRAB_THINK_INTERVAL	0.1f
#define HEADCRAB_ATTACK_DIST	1024.0f

#define HEADCRAB_SOUND_VOL		1.0f
#define HEADCRAB_ATTN_IDLE		2.0f
#define HEADCRAB_ATTN_COMBAT	0.8f

#define SVC_TEMPENTITY 23
#define TE_BLOODSPRITE 101
#define FL_ONGROUND 0x200

static const char kHeadcrabModel[] = "models/headcrab.mdl";

//=========================================================
// Sound Table
//=========================================================

static const char* pAttackSounds[] =
{
	"headcrab/hc_attack1.wav",
};

static const char* pAlertSounds[] =
{
	"headcrab/hc_alert1.wav",
};

static const char* pDieSounds[] =
{
	"headcrab/hc_die1.wav",
	"headcrab/hc_die2.wav",
};

static const char* pPainSounds[] =
{
	"headcrab/hc_pain1.wav",
	"headcrab/hc_pain2.wav",
	"headcrab/hc_pain3.wav",
};

static const char* pIdleSounds[] =
{
	"headcrab/hc_idle1.wav",
	"headcrab/hc_idle2.wav",
	"headcrab/hc_idle3.wav",
};

static const char* pBiteSounds[] =
{
	"headcrab/hc_headbite.wav",
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
// CHCHeadcrab
//=========================================================

class CHCHeadcrab : public CBaseMonster
{
public:
	CHCHeadcrab();

	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);
	void IdleSound();
	void AlertSound();
	int CheckAttacks(entvars_t* pevEnemy, float flDist);

	void Pain(float flDamage);		// vtable slot 13
	void Death(int gibType);		// vtable slot 14

	void LeapAttackThink(CBaseEntity* pOther);
	void LeapAttackTouch(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CHCHeadcrab) <= 336, CHCHeadcrab_private_data_size);

CHCHeadcrab::CHCHeadcrab()
{
}

//=========================================================
// Spawn
//=========================================================
void CHCHeadcrab::Spawn()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pAttackSounds); ++i)
		EnginePrecacheSound(pAttackSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pAlertSounds); ++i)
		EnginePrecacheSound(pAlertSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); ++i)
		EnginePrecacheSound(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); ++i)
		EnginePrecacheSound(pPainSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pIdleSounds); ++i)
		EnginePrecacheSound(pIdleSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pBiteSounds); ++i)
		EnginePrecacheSound(pBiteSounds[i]);

	EnginePrecacheModel(kHeadcrabModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kHeadcrabModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 12.0f, 12.0f, 24.0f);
		VecSet(mins, -12.0f, -12.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 12.0f;
	PevFloat(pev, PEV_YAWSPEED) = 10.0f;
	PevInt(pev, PEV_SEQUENCE) = 8;

	m_bloodColor = 54;
	m_flDistTooFar = 256.0f;

	// The binary adds the random delay onto the existing nextthink field
	// (pev->nextthink += RandomFloat(0,0.5) + 0.5), it does not reset from time.
	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_NEXTTHINK) + RandomFloat(0.0f, 0.5f) + 0.5f;
	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// Classify
//=========================================================
int CHCHeadcrab::Classify()
{
	return 8;
}

//=========================================================
// SetActivity
//=========================================================
void CHCHeadcrab::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 1:
	case 2:
	case 3:
	case 6:
	case 7:
		sequence = 0;
		break;

	case 4:
		sequence = 1;
		break;

	case 8:
	case 9:
		sequence = 2;
		break;

	case 18:
	case 33:
		sequence = 5;
		break;

	case 30:
		sequence = 4;
		break;

	case 31:
		return;

	case 35:
		sequence = 6;
		break;

	case 41:
		sequence = 7;
		break;

	default:
		EngineAlertMessage(1, "Headcrab's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(HEADCRAB_THINK_INTERVAL);

	volatile int validatedSequence = sequence;
	if (validatedSequence < 0 || validatedSequence > 7)
	{
		EngineAlertMessage(1, "Bogus headcrab anim: %d", validatedSequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
	}
}

//=========================================================
// Pain - vtable slot 13
//=========================================================
void CHCHeadcrab::Pain(float flDamage)
{
	edict_t* edict = EdictFromEntvars(pev);
	int soundIndex = RandomLong(0, 2);

	if (edict)
		EngineEmitSound(edict, 2, pPainSounds[soundIndex], HEADCRAB_SOUND_VOL, HEADCRAB_ATTN_COMBAT);

	if (flDamage < 5.0f)
	{
		SetThink(&CBaseMonster::MonsterThink);
		if (PevInt(pev, PEV_ENEMY))
			m_Activity = 6;
	}
}

//=========================================================
// Death - vtable slot 14
//=========================================================
void CHCHeadcrab::Death(int gibType)
{
	HL_UNUSED(gibType);

	// Binary compares the raw health bits unsigned against 0xC1F00000 (-30.0):
	// the death gurgle only plays when the corpse has NOT been gibbed below -30.
	if (PevFloat(pev, PEV_HEALTH) > -30.0f)
	{
		int soundIndex = RandomLong(0, 1);
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 2, pDieSounds[soundIndex], HEADCRAB_SOUND_VOL, HEADCRAB_ATTN_COMBAT);
	}

	// The binary tail-calls the shared CBaseMonster::SetDeathActivity.
	SetDeathActivity(0);
}

//=========================================================
// AlertSound
//=========================================================
void CHCHeadcrab::AlertSound()
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, pAlertSounds[0], HEADCRAB_SOUND_VOL, HEADCRAB_ATTN_COMBAT);

	m_Activity = 31;
	m_flNextAttack = GlobalTime() + 1.0f;
}

//=========================================================
// IdleSound
//=========================================================
void CHCHeadcrab::IdleSound()
{
	float rnd = RandomFloat(0.0f, 1.0f);
	const char* sound;

	if (rnd <= 0.33f)
	{
		sound = pIdleSounds[0];
	}
	else if (rnd <= 0.66f)
	{
		sound = pIdleSounds[1];
	}
	else
	{
		sound = pIdleSounds[2];
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, sound, HEADCRAB_SOUND_VOL, HEADCRAB_ATTN_IDLE);

	m_flNextSoundTime = GlobalTime() + RandomFloat(0.0f, 2.0f) + 3.0f;
}

//=========================================================
// CheckAttacks
//=========================================================
int CHCHeadcrab::CheckAttacks(entvars_t* pevEnemy, float flDist)
{
	if (!pevEnemy)
		return 0;

	if (!CheckRangeAttack(pevEnemy) || flDist > HEADCRAB_ATTACK_DIST)
		return 0;

	m_IdealActivity = 7;
	SetThink(&CHCHeadcrab::LeapAttackThink);
	return 1;
}

//=========================================================
// LeapAttackTouch
//
// Installed via SetTouch by LeapAttackThink while the headcrab is
// airborne.  The base Touch dispatcher (CBaseEntity::Touch) forwards
// the entity that was hit to this callback.
//=========================================================
void CHCHeadcrab::LeapAttackTouch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	if (!globals)
		return;

	int otherIndex = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);
	edict_t* otherEdict = otherIndex ? EnginePEntityOfEntIndex(otherIndex) : NULL;
	entvars_t* pevOther = otherEdict ? EngineGetVarsOfEnt(otherEdict) : NULL;
	if (!pevOther)
		return;

	if ((PevInt(pevOther, PEV_TAKEDAMAGE) & 0x7FFFFFFF) == 0)
		return;

	CBaseEntity* pHit = (CBaseEntity*)EngineGetPrivateData(otherEdict);
	if (!pHit)
		return;

	if (pHit->Classify() == Classify())
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, pBiteSounds[0], HEADCRAB_SOUND_VOL, HEADCRAB_ATTN_COMBAT);

	pHit->TakeDamage(pev, pev, PevFloat(pevOther, PEV_HEALTH));

	{
		const Vector& vecOrigin = PevVector(pev, PEV_ORIGIN);

		for (int i = 0; i < 4; ++i)
		{
			EngineWriteByte(0, SVC_TEMPENTITY);
			EngineWriteByte(0, TE_BLOODSPRITE);
			EngineWriteCoord(0, vecOrigin.x);
			EngineWriteCoord(0, vecOrigin.y);
			EngineWriteCoord(0, vecOrigin.z);
			EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
			EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
			EngineWriteCoord(0, RandomFloat(0.0f, 1.0f));
			EngineWriteByte(0, pHit->BloodColor());
			EngineWriteByte(0, RandomLong(80, 150));
		}
	}
}

//=========================================================
// LeapAttackThink
//=========================================================
void CHCHeadcrab::LeapAttackThink(CBaseEntity* pOther)
{
	SetNextThink(HEADCRAB_THINK_INTERVAL);

	if (m_Activity != 30)
	{
		m_Activity = 30;
		SetActivity(30);

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 1, pAttackSounds[0], HEADCRAB_SOUND_VOL, HEADCRAB_ATTN_COMBAT);

		EngineMakeVectors((const float*)&PevVector(pev, PEV_ANGLES));

		// FL_ONGROUND lives in the float-typed flags field; clear it by
		// subtracting the bit value (matches the original).
		PevFloat(pev, PEV_FLAGS) = PevFloat(pev, PEV_FLAGS) - 512.0f;

		if (edict)
		{
			Vector vecOrigin = PevVector(pev, PEV_ORIGIN);
			vecOrigin.z = vecOrigin.z + 1.0f;
			EngineSetOrigin(edict, (const float*)&vecOrigin);
		}

		Vector vecJumpDir;
		vecJumpDir.x = 0.0f;
		vecJumpDir.y = 0.0f;
		vecJumpDir.z = 0.0f;

		entvars_t* pevEnemy = EnemyVars(pev);
		if (pevEnemy)
		{
			vecJumpDir.x = PevVector(pevEnemy, PEV_ORIGIN).x - PevVector(pev, PEV_ORIGIN).x;
			vecJumpDir.y = PevVector(pevEnemy, PEV_ORIGIN).y - PevVector(pev, PEV_ORIGIN).y;
			vecJumpDir.z = PevVector(pevEnemy, PEV_ORIGIN).z - PevVector(pev, PEV_ORIGIN).z;
			VecNormalize((float*)&vecJumpDir);
		}

		void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
		const float* up = GlobalsUp(globals);

		PevVector(pev, PEV_VELOCITY).x = ((up ? up[0] : 0.0f) + vecJumpDir.x) * 250.0f;
		PevVector(pev, PEV_VELOCITY).y = ((up ? up[1] : 0.0f) + vecJumpDir.y) * 250.0f;
		PevVector(pev, PEV_VELOCITY).z = ((up ? up[2] : 0.0f) + vecJumpDir.z) * 250.0f;

		SetTouch(&CHCHeadcrab::LeapAttackTouch);
	}

	AdvanceAnimation(HEADCRAB_THINK_INTERVAL);

	{
		entvars_t* pevEnemy = EnemyVars(pev);
		if (pevEnemy)
			UpdateEnemyInfo(pevEnemy);
	}

	if ((((int)PevFloat(pev, PEV_FLAGS)) & FL_ONGROUND) != 0)
	{
		SetDoNothingTouch();
		SetThink(&CBaseMonster::MonsterThink);
		m_flNextAttack = GlobalTime() + 4.0f;
		m_Activity = m_IdealActivity;
	}
}

//=========================================================
// Death tail-calls the shared CBaseMonster::SetDeathActivity
// which is implemented in basemonster.cpp; there is no
// headcrab-specific copy in the binary, so we reuse the inherited helper.
//=========================================================

//=========================================================
// TakeDamage routes through the shared CBaseMonster::TakeDamage
// (vtable slot 17), which raises Pain / Death.
//=========================================================

//=========================================================
// monster_headcrab
//=========================================================
DLLEXPORT void monster_headcrab(entvars_t* pev)
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

		CHCHeadcrab* monster = new (privateData) CHCHeadcrab();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
