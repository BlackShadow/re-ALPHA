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
// Bullchicken - Bullsquid monster
//=========================================================

#include <stdlib.h>
#include <string.h>
#include <new>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// monster-specific DEFINE's
//=========================================================

#define BULLCHICKEN_THINK_INTERVAL	0.1f
#define BULLCHICKEN_MAX_ATTACK_DIST	1024.0f

#define BULLCHICKEN_SPIT_DAMAGE		15.0f

#define BULLCHICKEN_VOL				1.0f
#define BULLCHICKEN_ATTN_IDLE		2.0f
#define BULLCHICKEN_ATTN_COMBAT		0.8f

#define SVC_TEMPENTITY				23
#define TE_SPRITE_SPRAY				101

static const char kBullchickenModel[] = "models/bullchik.mdl";
static const char kSpitModel[] = "models/spit.mdl";

//=========================================================
// Sound Table
//=========================================================

static const char* pPainSounds[] =
{
	"bullchicken/bc_pain1.wav",
	"bullchicken/bc_pain2.wav",
	"bullchicken/bc_pain3.wav",
	"bullchicken/bc_pain4.wav",
};

static const char* pIdleSounds[] =
{
	"bullchicken/bc_idle1.wav",
	"bullchicken/bc_idle2.wav",
	"bullchicken/bc_idle3.wav",
	"bullchicken/bc_idle4.wav",
	"bullchicken/bc_idle5.wav",
};

static const char* pAttackSounds[] =
{
	"bullchicken/bc_attack1.wav",
	"bullchicken/bc_attack2.wav",
	"bullchicken/bc_attack3.wav",
};

static const char* pDieSounds[] =
{
	"bullchicken/bc_die1.wav",
	"bullchicken/bc_die2.wav",
	"bullchicken/bc_die3.wav",
};

//=========================================================
// Bullchicken's spit projectile
//=========================================================

class CSquidSpit : public CBaseMonster
{
public:
	void Init(entvars_t* pevOwner);
	void Touch(CBaseEntity* pOther);

private:
	void SpitThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CSquidSpit) <= 336, CSquidSpit_private_data_size);

//=========================================================
// Init
//=========================================================
void CSquidSpit::Init(entvars_t* pevOwner)
{
	if (!pev || !pevOwner)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (!edict)
		return;

	EngineMakeVectors(VecPtr(PevVector(pevOwner, PEV_ANGLES)));

	PevFloat(pev, PEV_MOVETYPE) = 6.0f;
	PevFloat(pev, PEV_SOLID) = 2.0f;
	EngineSetModel(edict, kSpitModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 6.0f, 6.0f, 6.0f);
		VecSet(mins, -6.0f, -6.0f, -6.0f);
		EngineSetSize(edict, mins, maxs);
	}

	PevInt(pev, PEV_OWNER_ENTINDEX) = EngineIndexOfEdict(EdictFromEntvars(pevOwner));

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	const float* forward = GlobalsForward(globals);
	const float* up = GlobalsUp(globals);

	const float* ownerOrigin = VecPtr(PevVector(pevOwner, PEV_ORIGIN));
	float spitOrigin[3];
	spitOrigin[0] = (forward ? forward[0] : 0.0f) * 64.0f + ownerOrigin[0];
	spitOrigin[1] = (forward ? forward[1] : 0.0f) * 64.0f + ownerOrigin[1];
	spitOrigin[2] = ownerOrigin[2] + 48.0f + (forward ? forward[2] : 0.0f) * 64.0f;
	VecCopy(VecPtr(PevVector(pev, PEV_ORIGIN)), spitOrigin);

	float* velocity = VecPtr(PevVector(pev, PEV_VELOCITY));
	velocity[0] = (forward ? forward[0] : 0.0f) * 600.0f + (up ? up[0] : 0.0f) * 200.0f;
	velocity[1] = (forward ? forward[1] : 0.0f) * 600.0f + (up ? up[1] : 0.0f) * 200.0f;
	velocity[2] = (up ? up[2] : 0.0f) * 200.0f + (forward ? forward[2] : 0.0f) * 600.0f;

	{
		float angles[3];
		EngineVecToAngles(velocity, angles);
		VecCopy(VecPtr(PevVector(pev, PEV_ANGLES)), angles);
	}

	SetThink(&CSquidSpit::SpitThink);
	PevInt(pev, PEV_SEQUENCE) = 0;
	ResetSequenceInfo(0.1f);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;

	SetTouch(&CSquidSpit::Touch);
}

//=========================================================
// SpitThink
//=========================================================
void CSquidSpit::SpitThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;
	AdvanceAnimation(0.1f);

	float angles[3];
	EngineVecToAngles(VecPtr(PevVector(pev, PEV_VELOCITY)), angles);
	VecCopy(VecPtr(PevVector(pev, PEV_ANGLES)), angles);
}

//=========================================================
// Touch
//=========================================================
void CSquidSpit::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	if (!pev)
		return;

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	if (!globals)
		return;

	int otherIndex = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);
	edict_t* otherEdict = EnginePEntityOfEntIndex(otherIndex);
	entvars_t* pevOther = otherEdict ? EngineGetVarsOfEnt(otherEdict) : NULL;

	if (otherIndex == PevInt(pev, PEV_OWNER_ENTINDEX))
		return;

	if (pevOther && (PevInt(pevOther, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
	{
		CBaseEntity* pHit = (CBaseEntity*)EngineGetPrivateData(otherEdict);
		if (pHit)
			pHit->TakeDamage(pev, pev, BULLCHICKEN_SPIT_DAMAGE);
	}

	PevInt(pev, PEV_MODELINDEX) = 0;
	SetRemoveThink();

	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;
}

//=========================================================
// CBullchicken
//=========================================================

class CBullchicken : public CBaseMonster
{
public:
	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);
	void AlertSound();
	void IdleSound();
	int CheckAttacks(entvars_t* pevEnemy, float flDist);

	void Pain(float flDamage);		// vtable slot 13
	void BigFlinchThink(CBaseEntity* pOther);
	void Death(int gibType);		// vtable slot 14

	void SpitAttackThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CBullchicken) <= 336, CBullchicken_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CBullchicken::Spawn()
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); ++i)
		EnginePrecacheSound(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pAttackSounds); ++i)
		EnginePrecacheSound(pAttackSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pIdleSounds); ++i)
		EnginePrecacheSound(pIdleSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); ++i)
		EnginePrecacheSound(pPainSounds[i]);

	EnginePrecacheModel(kBullchickenModel);
	EnginePrecacheModel(kSpitModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kBullchickenModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 32.0f, 32.0f, 64.0f);
		VecSet(mins, -32.0f, -32.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = 50.0f;
	PevFloat(pev, PEV_YAWSPEED) = 5.0f;
	PevInt(pev, PEV_SEQUENCE) = 7;

	m_flDistTooFar = 512.0f;
	m_bloodColor = 22;

	// The binary: pev->nextthink = RandomFloat(0,0.5) + pev->nextthink + 0.5
	// (adds the EXISTING nextthink value, not gpGlobals->time).
	PevFloat(pev, PEV_NEXTTHINK) = RandomFloat(0.0f, 0.5f) + PevFloat(pev, PEV_NEXTTHINK) + 0.5f;
	SetThink(&CBaseMonster::WalkMonsterStart);
}

//=========================================================
// Classify
//=========================================================
int CBullchicken::Classify()
{
	return 7;
}

//=========================================================
// SetActivity
//=========================================================
void CBullchicken::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 1:
		sequence = 3;
		break;
	case 2:
		sequence = 0;
		break;
	case 3:
		sequence = 0;
		break;
	case 4:
		sequence = 0;
		break;
	case 6:
		sequence = 3;
		break;
	case 7:
		sequence = 3;
		break;
	case 8:
		sequence = 1;
		break;
	case 29:
		sequence = 0;
		break;
	case 30:
		sequence = 4;
		break;
	case 34:
		sequence = 2;
		break;
	case 35:
		sequence = 6;
		break;
	default:
		EngineAlertMessage(1, "BullChicken's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(0.1f);

	volatile int validatedSequence = sequence;
	if (validatedSequence < 0 || validatedSequence > 6)
	{
		EngineAlertMessage(1, "Bogus BullChicken anim: %d", validatedSequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
	}
}

//=========================================================
// AlertSound
//=========================================================
void CBullchicken::AlertSound()
{
	m_Activity = 6;
}

//=========================================================
// IdleSound
//=========================================================
void CBullchicken::IdleSound()
{
	const char* sound;

	switch (rand() % 5)
	{
	case 0:
		sound = pIdleSounds[0];
		break;
	case 1:
		sound = pIdleSounds[1];
		break;
	case 2:
		sound = pIdleSounds[2];
		break;
	case 3:
		sound = pIdleSounds[3];
		break;
	case 4:
		sound = pIdleSounds[4];
		break;
	default:
		sound = NULL;
		break;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (sound && edict)
		EngineEmitSound(edict, 2, sound, BULLCHICKEN_VOL, BULLCHICKEN_ATTN_IDLE);

	m_flNextSoundTime = RandomFloat(0.0f, 2.0f) + GlobalTime() + 3.0f;
}

//=========================================================
// Pain - vtable slot 13
// invoked from the shared TakeDamage
//=========================================================
void CBullchicken::Pain(float flDamage)
{
	HL_UNUSED(flDamage);

	if (!pev)
		return;

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;

	if (m_Activity == 1 || m_Activity == 4)
		AlertSound();

	if (m_Activity != 18)
	{
		const char* sound;
		switch (rand() % 4)
		{
		case 0:
			sound = pPainSounds[0];
			break;
		case 1:
			sound = pPainSounds[1];
			break;
		case 2:
			sound = pPainSounds[2];
			break;
		case 3:
			sound = pPainSounds[3];
			break;
		default:
			sound = NULL;
			break;
		}

		edict_t* edict = EdictFromEntvars(pev);
		if (sound && edict)
			EngineEmitSound(edict, 2, sound, BULLCHICKEN_VOL, BULLCHICKEN_ATTN_COMBAT);

		if ((abs(rand()) & 0xFF) % 2 == 1)
		{
			m_Activity = 7;
			SetThink(&CBaseMonster::MonsterThink);
			return;
		}

		SetThink(&CBullchicken::BigFlinchThink);
	}

	AdvanceAnimation(0.1f);

	if (m_fSequenceFinished)
		SetThink(&CBaseMonster::MonsterThink);
}

//=========================================================
// BigFlinchThink
//=========================================================
void CBullchicken::BigFlinchThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;

	if (m_Activity != 34)
	{
		const char* sound;
		switch (rand() % 4)
		{
		case 0:
			sound = pPainSounds[0];
			break;
		case 1:
			sound = pPainSounds[1];
			break;
		case 2:
			sound = pPainSounds[2];
			break;
		case 3:
			sound = pPainSounds[3];
			break;
		default:
			sound = NULL;
			break;
		}

		edict_t* edict = EdictFromEntvars(pev);
		if (sound && edict)
			EngineEmitSound(edict, 2, sound, BULLCHICKEN_VOL, BULLCHICKEN_ATTN_COMBAT);

		if ((abs(rand()) & 0xFF) % 2 == 1)
		{
			SetThink(&CBaseMonster::MonsterThink);
			return;
		}

		m_Activity = 34;
		SetActivity(34);
	}

	GetAnimationEventFlags(0.1f);
	AdvanceAnimation(0.1f);

	if (m_fSequenceFinished)
	{
		m_Activity = 7;
		SetActivity(7);
		SetThink(&CBaseMonster::MonsterThink);
	}
}

//=========================================================
// Death - vtable slot 14
//=========================================================
void CBullchicken::Death(int gibType)
{
	HL_UNUSED(gibType);

	const char* sound;

	switch (rand() % 3)
	{
	case 0:
		sound = pDieSounds[0];
		break;
	case 1:
		sound = pDieSounds[1];
		break;
	case 2:
		sound = pDieSounds[2];
		break;
	default:
		sound = NULL;
		break;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (sound && edict)
		EngineEmitSound(edict, 2, sound, BULLCHICKEN_VOL, BULLCHICKEN_ATTN_COMBAT);

	// The binary copies the 3-dword.data constant at the binary
	// (all bytes 0x00000000, i.e. g_vecZero) into pev->velocity, so the
	// corpse stops moving before the death animation plays.
	PevVector(pev, PEV_VELOCITY).x = 0.0f;
	PevVector(pev, PEV_VELOCITY).y = 0.0f;
	PevVector(pev, PEV_VELOCITY).z = 0.0f;

	// The binary(this, 0): set the death activity and fall back to MonsterThink.
	m_Activity = 35;
	PevFloat(pev, PEV_IDEAL_YAW) = PevVector(pev, PEV_ANGLES).y;
	SetActivity(m_Activity);
	SetThink(&CBaseMonster::MonsterThink);

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;
}

//=========================================================
// CheckAttacks / CheckRangeAttack1
//=========================================================
int CBullchicken::CheckAttacks(entvars_t* pevEnemy, float flDist)
{
	if (!CheckRangeAttack(pevEnemy) || flDist > BULLCHICKEN_MAX_ATTACK_DIST)
		return 0;

	m_IdealActivity = 7;
	SetThink(&CBullchicken::SpitAttackThink);
	return 1;
}

//=========================================================
// SpitAttackThink
//=========================================================
void CBullchicken::SpitAttackThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;

	if (m_Activity != 30)
	{
		m_Activity = 30;
		SetActivity(30);
	}

	int events = GetAnimationEventFlags(0.1f);
	AdvanceAnimation(0.1f);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineChangeYaw(edict);

	if ((events & (1 << 1)) != 0)
	{
		const char* sound;
		switch (rand() % 3)
		{
		case 0:
			sound = pAttackSounds[0];
			break;
		case 1:
			sound = pAttackSounds[1];
			break;
		case 2:
			sound = pAttackSounds[2];
			break;
		default:
			sound = NULL;
			break;
		}

		if (sound && edict)
			EngineEmitSound(edict, 2, sound, BULLCHICKEN_VOL, BULLCHICKEN_ATTN_COMBAT);

		{
			edict_t* created = EngineCreateEntity();
			entvars_t* spitVars = created ? EngineGetVarsOfEnt(created) : NULL;
			CSquidSpit* spit = NULL;

			if (created && spitVars)
			{
				void* privateData = EngineGetPrivateData(created);
				if (!privateData)
				{
					privateData = EngineAllocPrivateData(created, 336);
					if (privateData)
					{
						memset(privateData, 0, 336);
						spit = new (privateData) CSquidSpit();
					}
				}
				else
				{
					spit = (CSquidSpit*)privateData;
				}

				if (spit)
				{
					spit->pev = spitVars;
					spit->m_pGlobals = GlobalsFromEntvars(spitVars);
					spit->Init(pev);
				}
			}
		}

		EngineMakeVectors(VecPtr(PevVector(pev, PEV_ANGLES)));
		globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
		const float* forward = GlobalsForward(globals);
		const float* up = GlobalsUp(globals);

		float dir[3];
		dir[0] = (forward ? forward[0] : 0.0f) * 400.0f + (up ? up[0] : 0.0f) * 400.0f;
		dir[1] = (up ? up[1] : 0.0f) * 400.0f + (forward ? forward[1] : 0.0f) * 400.0f;
		dir[2] = (up ? up[2] : 0.0f) * 400.0f + (forward ? forward[2] : 0.0f) * 400.0f;
		VecNormalize(dir);

		const float* origin = VecPtr(PevVector(pev, PEV_ORIGIN));
		float start[3];
		start[0] = (forward ? forward[0] : 0.0f) * 64.0f + origin[0];
		start[1] = (forward ? forward[1] : 0.0f) * 64.0f + origin[1];
		start[2] = origin[2] + 48.0f + (forward ? forward[2] : 0.0f) * 64.0f;

		for (int i = 0; i < 4; ++i)
		{
			EngineWriteByte(0, SVC_TEMPENTITY);
			EngineWriteByte(0, TE_SPRITE_SPRAY);
			EngineWriteCoord(0, start[0]);
			EngineWriteCoord(0, start[1]);
			EngineWriteCoord(0, start[2]);
			EngineWriteCoord(0, RandomFloat(-0.6f, 0.6f) + dir[0]);
			EngineWriteCoord(0, RandomFloat(-0.6f, 0.6f) + dir[1]);
			EngineWriteCoord(0, RandomFloat(-0.6f, 0.6f) + dir[2]);
			EngineWriteByte(0, 22);
			EngineWriteByte(0, 400);
		}
	}

	if (m_fSequenceFinished)
	{
		m_Activity = m_IdealActivity;
		SetThink(&CBaseMonster::MonsterThink);
		m_flNextAttack = GlobalTime() + 4.0f;
	}
}

//=========================================================
// TakeDamage - bullchicken does NOT override the damage slot; the shared
// CBaseMonster::TakeDamage (vtable slot 17) handles it and
// on HP <= 0, routes through Killed to the Death virtual
// (slot 14). On survival it raises the Pain virtual (slot 13).
//=========================================================

//=========================================================
// monster_bullchicken (export)
//=========================================================
DLLEXPORT void monster_bullchicken(entvars_t* pev)
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

		CBullchicken* monster = new (privateData) CBullchicken();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
