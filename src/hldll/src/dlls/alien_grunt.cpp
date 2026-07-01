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
// Agrunt - Dominant, warlike alien grunt monster
//=========================================================

#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// Monster-specific constants
//=========================================================

static const char kAGruntModel[] = "models/agrunt.mdl";
static const char kHornetModel[] = "models/hornet.mdl";

#define AGRUNT_THINK_INTERVAL		0.1f

#define AGRUNT_MELEE_DIST			64.0f
#define AGRUNT_RANGE_DIST			1024.0f

#define AGRUNT_VOL					1.0f
#define AGRUNT_ATTN_IDLE			2.0f	// 0x40000000
#define AGRUNT_ATTN_COMBAT			0.8f	// 0x3F4CCCCD

#define HORNET_DAMAGE				10.0f	// 0x41200000
#define HORNET_SPEED				300.0f
#define HORNET_VOL					0.6f	// 0x3F19999A

//=========================================================
// Sound Table
//=========================================================

static const char* pHornetBuzzSounds[] =
{
	"hornet/ag_buzz1.wav",
	"hornet/ag_buzz2.wav",
	"hornet/ag_buzz3.wav",
};

static const char* pAlertSounds[] =
{
	"agrunt/ag_alert1.wav",
	"agrunt/ag_alert2.wav",
	"agrunt/ag_alert3.wav",
};

static const char* pFireSounds[] =
{
	"agrunt/ag_fire1.wav",
	"agrunt/ag_fire2.wav",
	"agrunt/ag_fire3.wav",
};

static const char* pDieSounds[] =
{
	"agrunt/ag_die1.wav",
	"agrunt/ag_die2.wav",
	"agrunt/ag_die3.wav",
};

static const char* pIdleSounds[] =
{
	"agrunt/ag_idle1.wav",
	"agrunt/ag_idle2.wav",
	"agrunt/ag_idle3.wav",
	"agrunt/ag_idle4.wav",
	"agrunt/ag_idle5.wav",
	"agrunt/ag_idle6.wav",
	"agrunt/ag_idle7.wav",
};

static const char* pPainSounds[] =
{
	"agrunt/ag_pain1.wav",
	"agrunt/ag_pain2.wav",
	"agrunt/ag_pain3.wav",
	"agrunt/ag_pain4.wav",
	"agrunt/ag_pain5.wav",
};

//=========================================================
// CHornet - alien grunt hornet projectile
//=========================================================

class CHornet : public CBaseMonster
{
public:
	void Init(entvars_t* pevOwner);
	void Touch(CBaseEntity* pOther);	// vtable slot 6

private:
	void HornetThink(CBaseEntity* pOther);	// m_pfnThink
	void Death(int gibType);				// vtable slot 14
};

HL_COMPILE_TIME_ASSERT(sizeof(CHornet) <= 344, CHornet_private_data_size);

//=========================================================
// Init
//=========================================================
void CHornet::Init(entvars_t* pevOwner)
{
	if (!pev || !pevOwner)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (!edict)
		return;

	EngineMakeVectors((const float*)&PevVector(pevOwner, PEV_ANGLES));

	PevFloat(pev, PEV_MOVETYPE) = 9.0f;			// 0x41100000
	PevFloat(pev, PEV_SOLID) = 2.0f;			// 0x40000000
	PevFloat(pev, PEV_HEALTH) = 1.0f;			// 0x3F800000
	PevFloat(pev, PEV_TAKEDAMAGE) = 2.0f;		// 0x40000000

	EngineSetModel(edict, kHornetModel);

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
	const float* right = GlobalsRight(globals);

	const float* ownerOrigin = (const float*)&PevVector(pevOwner, PEV_ORIGIN);

	// The original writes pev->origin directly (not via SetOrigin).
	float* origin = (float*)&PevVector(pev, PEV_ORIGIN);
	origin[0] = forward[0] * 64.0f + ownerOrigin[0] + right[0] * 24.0f;
	origin[1] = forward[1] * 64.0f + ownerOrigin[1] + right[1] * 24.0f;
	origin[2] = ownerOrigin[2] + 48.0f + forward[2] * 64.0f + right[2] * 24.0f;

	float spread = RandomFloat(-0.5f, 0.5f);	// -0.5 (0xBF000000) .. 0.5 (0x3F000000)
	float* velocity = (float*)&PevVector(pev, PEV_VELOCITY);
	velocity[0] = (right[0] * spread + forward[0]) * 150.0f;
	velocity[1] = (right[1] * spread + forward[1]) * 150.0f;
	velocity[2] = (spread * right[2] + forward[2]) * 150.0f;

	{
		float angles[3];
		EngineVecToAngles(velocity, angles);
		VecCopy((float*)&PevVector(pev, PEV_ANGLES), angles);
	}

	PevInt(pev, PEV_ENEMY) = PevInt(pevOwner, PEV_ENEMY);

	// Hornet blood color (single byte at this+296 in the 344-byte private data).
	*((unsigned char*)this + 296) = 54;

	// Lifetime tracking fields the original stamps at this+336/this+340.
	*(int*)((unsigned char*)this + 336) = 0;
	*(float*)((unsigned char*)this + 340) = GlobalTime();

	SetTouch(&CHornet::Touch);
	SetThink(&CHornet::HornetThink);

	{
		const char* sample;
		int n = rand() % 3;
		if (n == 1)
			sample = pFireSounds[1];
		else if (n == 2)
			sample = pFireSounds[2];
		else
			sample = pFireSounds[0];

		EngineEmitSound(edict, 2, sample, AGRUNT_VOL, AGRUNT_ATTN_COMBAT);
	}

	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.6f;
}

//=========================================================
// Death
//=========================================================
void CHornet::Death(int gibType)
{
	HL_UNUSED(gibType);

	if (!pev)
		return;

	SetRemoveThink();
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime();
}

//=========================================================
// Touch
//=========================================================
void CHornet::Touch(CBaseEntity* pOther)
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

	// Don't strike the agrunt that fired us.
	if (otherIndex == PevInt(pev, PEV_OWNER_ENTINDEX))
		return;

	// Don't strike another hornet (same model index).
	if (pevOther && PevInt(pevOther, PEV_MODELINDEX) == PevInt(pev, PEV_MODELINDEX))
		return;

	if (pevOther && (PevInt(pevOther, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
	{
		const char* sample;
		int n = rand() % 3;
		if (n == 1)
			sample = pHornetBuzzSounds[1];
		else if (n == 2)
			sample = pHornetBuzzSounds[2];
		else
			sample = pHornetBuzzSounds[0];

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 2, sample, HORNET_VOL, AGRUNT_ATTN_COMBAT);

		CBaseEntity* pHit = (CBaseEntity*)EngineGetPrivateData(otherEdict);
		if (pHit)
			pHit->TakeDamage(pev, pev, HORNET_DAMAGE);
	}

	PevInt(pev, PEV_MODELINDEX) = 0;
	PevFloat(pev, PEV_SOLID) = 0.0f;

	SetRemoveThink();
	SetNextThink(AGRUNT_THINK_INTERVAL);
}

//=========================================================
// HornetThink
//=========================================================
void CHornet::HornetThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	edict_t* enemyEdict = EnginePEntityOfEntIndex(PevInt(pev, PEV_ENEMY));
	entvars_t* enemyVars = enemyEdict ? EngineGetVarsOfEnt(enemyEdict) : NULL;

	if (!enemyVars || PevFloat(enemyVars, PEV_HEALTH) <= 0.0f)
	{
		SetRemoveThink();
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.1f;
		if (!enemyVars)
			return;
	}

	const float* origin = (const float*)&PevVector(pev, PEV_ORIGIN);
	const float* enemyOrigin = (const float*)&PevVector(enemyVars, PEV_ORIGIN);

	float desired[3];
	desired[0] = enemyOrigin[0] - origin[0];
	desired[1] = enemyOrigin[1] - origin[1];
	desired[2] = enemyOrigin[2] - origin[2];
	VecNormalize(desired);

	float* velocity = (float*)&PevVector(pev, PEV_VELOCITY);
	float velDir[3];
	VecCopy(velDir, velocity);
	VecNormalize(velDir);

	float dot = VecDot(desired, velDir);
	if (dot < 0.5f)	// flt double == 0.5
	{
		const char* sample;
		int n = rand() % 3;
		if (n == 1)
			sample = pHornetBuzzSounds[1];
		else if (n == 2)
			sample = pHornetBuzzSounds[2];
		else
			sample = pHornetBuzzSounds[0];

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 2, sample, HORNET_VOL, AGRUNT_ATTN_COMBAT);
	}

	if (dot <= 0.0f)
		dot = 0.25f;

	float newDir[3];
	newDir[0] = desired[0] + velDir[0];
	newDir[1] = desired[1] + velDir[1];
	newDir[2] = desired[2] + velDir[2];
	if (!VecNormalize(newDir))
		VecSet(newDir, 0.0f, 0.0f, 0.0f);

	velocity[0] = newDir[0];
	velocity[1] = newDir[1];
	velocity[2] = newDir[2];

	velocity[0] += RandomFloat(-0.1f, 0.1f);
	velocity[1] += RandomFloat(-0.1f, 0.1f);
	velocity[2] += RandomFloat(-0.1f, 0.1f);

	{
		float speed = dot * HORNET_SPEED;
		velocity[0] = velocity[0] * speed;
		velocity[1] = velocity[1] * speed;
		velocity[2] = velocity[2] * speed;
	}

	{
		float angles[3];
		EngineVecToAngles(velocity, angles);
		VecCopy((float*)&PevVector(pev, PEV_ANGLES), angles);
	}

	AdvanceAnimation(AGRUNT_THINK_INTERVAL);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.1f;
}

//=========================================================
// CAGrunt
//=========================================================

class CAGrunt : public CBaseMonster
{
public:
	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);	// slot 10
	void IdleSound();				// slot 15
	void AlertSound();				// slot 12
	int CheckAttacks(entvars_t* pevEnemy, float flDist);	// slot 16

	void Pain(float flDamage);		// slot 13
	void Death(int gibType);		// slot 14

	void MeleeAttack(CBaseEntity* pOther);				// m_pfnThink
	void HornetAttack(CBaseEntity* pOther);			// m_pfnThink

	CHornet* CreateHornet();

	int m_iHornetCount;
};

HL_COMPILE_TIME_ASSERT(sizeof(CAGrunt) <= 336, CAGrunt_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CAGrunt::Spawn()
{
	int i;

	EnginePrecacheModel(kAGruntModel);
	EnginePrecacheModel(kHornetModel);

	for (i = 0; i < (int)ARRAYSIZE(pHornetBuzzSounds); ++i)
		EnginePrecacheSound(pHornetBuzzSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pAlertSounds); ++i)
		EnginePrecacheSound(pAlertSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pFireSounds); ++i)
		EnginePrecacheSound(pFireSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); ++i)
		EnginePrecacheSound(pDieSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pIdleSounds); ++i)
		EnginePrecacheSound(pIdleSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); ++i)
		EnginePrecacheSound(pPainSounds[i]);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kAGruntModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 32.0f, 32.0f, 64.0f);
		VecSet(mins, -32.0f, -32.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevFloat(pev, PEV_SOLID) = 3.0f;			// 0x40400000
	PevFloat(pev, PEV_MOVETYPE) = 4.0f;			// 0x40800000
	PevFloat(pev, PEV_STUCK) = 0.0f;
	PevFloat(pev, PEV_HEALTH) = RandomFloat(0.0f, 30.0f) + 70.0f;	// 30.0 = 0x41F00000
	PevFloat(pev, PEV_YAWSPEED) = 8.0f;			// 0x41000000
	PevInt(pev, PEV_SEQUENCE) = 15;

	// The binary writes 512.0 to object offset 256 = m_flDistTooFar, the
	// chase/run distance threshold the SHARED CBaseMonster::MonsterThink reads
	// (monsters.cpp). Storing it elsewhere left m_flDistTooFar at 0, so the grunt
	// always treated the enemy as "too far" and picked the wrong activity.
	m_flDistTooFar = 512.0f;					// 0x44000000  (this+256)
	m_bloodColor = -61;

	SetThink(&CBaseMonster::WalkMonsterStart);
	PevFloat(pev, PEV_NEXTTHINK) = RandomFloat(0.0f, 0.5f) + PevFloat(pev, PEV_NEXTTHINK) + 0.5f;
}

//=========================================================
// Classify
//=========================================================
int CAGrunt::Classify()
{
	return 2;
}

//=========================================================
// SetActivity
//=========================================================
void CAGrunt::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 1:
	case 2:
	case 7:
		sequence = 0;
		break;
	case 3:
		sequence = 1;
		break;
	case 4:
		sequence = 3;
		break;
	case 8:
		sequence = 4;
		break;
	case 29:
		sequence = 10;
		break;
	case 30:
		sequence = 14;
		break;
	case 35:
		sequence = 12;
		break;
	case 37:
		sequence = 13;
		break;
	default:
		EngineAlertMessage(1, "AGrunt's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(AGRUNT_THINK_INTERVAL);

	// The binary validates the resolved sequence after rebuilding sequence
	// info. The activity switch above does not produce the rejected values, but
	// keep the alert path for parity with the binary.
	switch (sequence)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 7:
	case 10:
	case 12:
	case 13:
	case 14:
		break;

	default:
		EngineAlertMessage(1, "Bogus AGrunt anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// IdleSound
//=========================================================
void CAGrunt::IdleSound()
{
	if (!pev)
		return;

	const char* sample;
	switch (rand() % 7)
	{
	case 1:
		sample = pIdleSounds[1];
		break;
	case 2:
		sample = pIdleSounds[2];
		break;
	case 3:
		sample = pIdleSounds[3];
		break;
	case 4:
		sample = pIdleSounds[4];
		break;
	case 5:
		sample = pIdleSounds[5];
		break;
	case 6:
		sample = pIdleSounds[6];
		break;
	default:
		sample = pIdleSounds[0];
		break;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, sample, AGRUNT_VOL, AGRUNT_ATTN_IDLE);

	if ((rand() % 7) >= 4)
		m_flNextSoundTime = GlobalTime() + 5.0f;
	else
		m_flNextSoundTime = GlobalTime() + 1.0f;
}

//=========================================================
// AlertSound
//=========================================================
void CAGrunt::AlertSound()
{
	if (!pev)
		return;

	m_Activity = 8;

	const char* sample;
	int n = rand() % 3;
	if (n == 1)
		sample = pAlertSounds[1];
	else if (n == 2)
		sample = pAlertSounds[2];
	else
		sample = pAlertSounds[0];

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, sample, AGRUNT_VOL, AGRUNT_ATTN_COMBAT);

	m_flNextAttack = GlobalTime() + 1.0f;
}

//=========================================================
// Pain - vtable slot 13
//=========================================================
void CAGrunt::Pain(float flDamage)
{
	HL_UNUSED(flDamage);

	if (!pev)
		return;

	const char* sample;
	switch (rand() % 5)
	{
	case 1:
		sample = pPainSounds[1];
		break;
	case 2:
		sample = pPainSounds[2];
		break;
	case 3:
		sample = pPainSounds[3];
		break;
	case 4:
		sample = pPainSounds[4];
		break;
	default:
		sample = pPainSounds[0];
		break;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, sample, AGRUNT_VOL, AGRUNT_ATTN_COMBAT);

	PevFloat(pev, PEV_PAIN_FINISHED) = GlobalTime() + 1.0f;

	// Original calls vtable slot 12 (AlertSound) when idling/walking.
	if (m_Activity == 1 || m_Activity == 4)
		AlertSound();
}

//=========================================================
// Death - vtable slot 14
//=========================================================
void CAGrunt::Death(int gibType)
{
	HL_UNUSED(gibType);

	if (!pev)
		return;

	const char* sample;
	int n = rand() % 3;
	if (n == 1)
		sample = pDieSounds[1];
	else if (n == 2)
		sample = pDieSounds[2];
	else
		sample = pDieSounds[0];

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, sample, AGRUNT_VOL, AGRUNT_ATTN_COMBAT);

	// SetDeathState(0) - die forwards.
	m_Activity = 35;
	PevFloat(pev, PEV_IDEAL_YAW) = PevVector(pev, PEV_ANGLES).y;
	SetActivity(m_Activity);
	SetThink(&CBaseMonster::MonsterThink);
	SetNextThink(AGRUNT_THINK_INTERVAL);
}

//=========================================================
// TakeDamage routes through the shared CBaseMonster::TakeDamage
// (vtable slot 17), which raises Pain / Death.
//=========================================================

//=========================================================
// CheckAttacks
//=========================================================
int CAGrunt::CheckAttacks(entvars_t* pevEnemy, float flDist)
{
	if (flDist <= AGRUNT_MELEE_DIST && CheckMeleeAttack(pevEnemy))
	{
		m_IdealActivity = 7;
		SetThink(&CAGrunt::MeleeAttack);
		return 1;
	}

	if (CheckRangeAttack(pevEnemy) && flDist <= AGRUNT_RANGE_DIST)
	{
		m_IdealActivity = 7;
		m_iHornetCount = RandomLong(4, 6);
		SetThink(&CAGrunt::HornetAttack);
		return 1;
	}

	return 0;
}

//=========================================================
// MeleeAttack
//=========================================================
void CAGrunt::MeleeAttack(CBaseEntity* pOther)
{
	if (!pev)
		return;

	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.1f;

	if (m_Activity != 29)
	{
		m_Activity = 29;
		SetActivity(29);
		m_flNextAttack = GlobalTime() + 2.0f;
	}

	AdvanceAnimation(AGRUNT_THINK_INTERVAL);

	{
		entvars_t* pevEnemy = NULL;
		edict_t* pEnemyEdict = EnginePEntityOfEntIndex(PevInt(pev, PEV_ENEMY));
		pevEnemy = pEnemyEdict ? EngineGetVarsOfEnt(pEnemyEdict) : NULL;
		if (pevEnemy)
			UpdateEnemyInfo(pevEnemy);
	}

	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineChangeYaw(edict);
	}

	if (PevFloat(pev, PEV_FRAME) >= 255.0f)	// 0x437F0000
	{
		SetThink(&CBaseMonster::MonsterThink);
		m_Activity = m_IdealActivity;
		m_flNextAttack = GlobalTime() + 1.0f;
	}
}

//=========================================================
// CreateHornet (inline allocation)
//=========================================================
CHornet* CAGrunt::CreateHornet()
{
	edict_t* created = EngineCreateEntity();
	entvars_t* hornetVars = created ? EngineGetVarsOfEnt(created) : NULL;
	if (!created || !hornetVars)
		return NULL;

	void* privateData = EngineGetPrivateData(created);
	if (!privateData)
	{
		privateData = EngineAllocPrivateData(created, 344);
		if (!privateData)
			return NULL;

		memset(privateData, 0, 344);

		CHornet* hornet = new (privateData) CHornet();
		hornet->pev = hornetVars;
		hornet->m_pGlobals = GlobalsFromEntvars(hornetVars);
		return hornet;
	}

	CHornet* hornet = (CHornet*)privateData;
	hornet->pev = hornetVars;
	hornet->m_pGlobals = GlobalsFromEntvars(hornetVars);
	return hornet;
}

//=========================================================
// HornetAttack
//=========================================================
void CAGrunt::HornetAttack(CBaseEntity* pOther)
{
	if (!pev)
		return;

	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.1f;

	if (m_Activity != 30)
	{
		float next = GlobalTime() + 2.0f;
		m_Activity = 30;
		m_flNextAttack = next;
		SetActivity(30);
	}

	{
		entvars_t* pevEnemy = NULL;
		edict_t* pEnemyEdict = EnginePEntityOfEntIndex(PevInt(pev, PEV_ENEMY));
		pevEnemy = pEnemyEdict ? EngineGetVarsOfEnt(pEnemyEdict) : NULL;
		if (pevEnemy)
			UpdateEnemyInfo(pevEnemy);
	}

	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineChangeYaw(edict);
	}

	int events = GetAnimationEventFlags(AGRUNT_THINK_INTERVAL);
	AdvanceAnimation(AGRUNT_THINK_INTERVAL);

	if ((events & 0x1E) != 0)
	{
		PevFloat(pev, PEV_STUCK) = 2.0f;	// 0x40000000 (pev+140)

		CHornet* hornet = CreateHornet();
		if (hornet)
			hornet->Init(pev);
	}

	if (m_fSequenceFinished)
	{
		m_Activity = m_IdealActivity;
		m_flNextAttack = RandomFloat(0.0f, 2.0f) + GlobalTime() + 1.0f;
		SetThink(&CBaseMonster::MonsterThink);
	}
}

//=========================================================
// monster_alien_grunt (export)
//=========================================================
DLLEXPORT void monster_alien_grunt(entvars_t* pev)
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

		CAGrunt* monster = new (privateData) CAGrunt();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
