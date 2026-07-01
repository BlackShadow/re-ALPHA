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
// Turret - ceiling/floor mounted auto turret.  Deploys,
// scans for targets, tracks them with two bone controllers
// and fires a machine gun.  Heavy think-driven state machine.
//=========================================================

#include <new>
#include <stdlib.h>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// monster-specific constants
//=========================================================

#define TURRET_THINK_FAST		0.1f
#define TURRET_DEPLOY_DELAY		0.4f
#define TURRET_SPAWN_DELAY		0.3f
#define TURRET_SEARCH_DELAY		0.3f

#define TURRET_TURNRATE			360.0f
#define TURRET_TURN_STEP		4.5f
#define TURRET_RANGE			1024.0f
#define TURRET_MAX_SPIN			5.0f

#define TURRET_VOL				1.0f
#define TURRET_ATTN_NORM		0.8f
#define TURRET_ATTN_PING		2.0f

// pev byte offsets not yet named in utils.h -> recorded in shared_needs.
#define PEV_IDEAL_PITCH			364		// float
#define PEV_PITCH_SPEED			368		// float
#define PEV_CONTROLLER			172		// 4 packed controller bytes

static const char kTurretModel[] = "models/turret.mdl";

//=========================================================
// Sound Table
//=========================================================

static const char kSndFire[]		= "turret/tu_fire1.wav";
static const char kSndPing[]		= "turret/tu_ping.wav";
static const char kSndActive[]		= "turret/tu_active.wav";
static const char kSndDie[]			= "turret/tu_die.wav";
static const char kSndDie2[]		= "turret/tu_die2.wav";
static const char kSndDie3[]		= "turret/tu_die3.wav";
static const char kSndRetract[]		= "turret/tu_retract.wav";
static const char kSndDeploy[]		= "turret/tu_deploy.wav";
static const char kSndSpinUp[]		= "turret/tu_spinup.wav";
static const char kSndSpinDown[]	= "turret/tu_spindown.wav";
static const char kSndSearch[]		= "turret/tu_search.wav";
static const char kSndAlert[]		= "turret/tu_alert.wav";

//=========================================================
// SetBoneController
// Maps an angle value onto a packed bone-controller byte for
// the turret's studio model.  Mirrors the engine model helper.
//=========================================================
static void SetBoneController(entvars_t* pev, int controller, float value)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (!edict)
		return;

	unsigned char* pStudioHdr = (unsigned char*)EngineGetModelPtr(edict);
	if (!pStudioHdr)
		return;

	// studiohdr_t: numbonecontrollers at +84, bonecontrollerindex at +88.
	int numControllers = *(int*)(pStudioHdr + 84);
	if (controller >= numControllers)
		return;

	// each mstudiobonecontroller_t is 16 bytes: +4 type flags, +8 start, +12 end.
	unsigned char* pController = pStudioHdr + 16 * controller + *(int*)(pStudioHdr + 88);

	float flStart = *(float*)(pController + 8);
	float flEnd = *(float*)(pController + 12);

	if ((pController[4] & 0x38) != 0
		&& flStart + 359.0f >= flEnd
		&& (flStart + flEnd) * 0.5f + 180.0f < value)
	{
		value = value - 360.0f;
	}

	int setting = (int)((value - flStart) / (flEnd - flStart) * 255.0f);
	if (setting < 0)
		setting = 0;
	if (setting > 255)
		setting = 255;

	int* pControllerBytes = (int*)((unsigned char*)pev + PEV_CONTROLLER);
	*pControllerBytes = (*pControllerBytes & ~(255 << (8 * controller))) | (setting << (8 * controller));
}

//=========================================================
// Multi-damage / bullet-impact helpers
//
// The turret's FireThink does NOT call CBaseEntity::FireBullets
// ; it inlines a turret-specific two-bullet loop that calls the
// shared impact-effects routine and the multi-damage helpers
// . Those engine helpers are file-static
// inside combat.cpp, so they are reproduced here verbatim for the turret.
//=========================================================

enum
{
	TURRET_SVC_TEMPENTITY	= 23,
	TURRET_TE_GUNSHOT		= 2,
	TURRET_TE_TRACER		= 6,
	TURRET_TE_BLOODSTREAM	= 101,
	TURRET_TE_BLOOD			= 103,
	TURRET_TE_DECAL			= 104,
};

static int s_multiDamageTarget = 0;
static float s_multiDamageAmount = 0.0f;

static void ClearMultiDamage()
{
	s_multiDamageTarget = 0;
	s_multiDamageAmount = 0.0f;
}

static void ApplyMultiDamage(entvars_t* pevInflictor)
{
	if (!pevInflictor || !s_multiDamageTarget)
		return;

	void* globals = GlobalsFromEntvars(pevInflictor);

	edict_t* pHitEdict = EnginePEntityOfEntIndex(s_multiDamageTarget);
	if (!pHitEdict)
		return;

	entvars_t* pevHit = EngineGetVarsOfEnt(pHitEdict);
	CBaseEntity* pHit = (CBaseEntity*)EngineGetPrivateData(pHitEdict);
	if (pHit)
		pHit->TakeDamage(pevInflictor, pevInflictor, s_multiDamageAmount);

	if (!pevHit)
		return;

	const char* pszClassname = EngineStringFromIndex(PevInt(pevHit, PEV_CLASSNAME));
	if (pszClassname && strcmp(pszClassname, "cycler") == 0)
		return;

	if (RandomFloat(0.0f, 1.0f) >= 0.3f)
		return;

	float delta[3];
	delta[0] = PevVector(pevInflictor, PEV_ORIGIN).x - PevVector(pevHit, PEV_ORIGIN).x;
	delta[1] = PevVector(pevInflictor, PEV_ORIGIN).y - PevVector(pevHit, PEV_ORIGIN).y;
	delta[2] = PevVector(pevInflictor, PEV_ORIGIN).z - PevVector(pevHit, PEV_ORIGIN).z;

	EngineMakeVectors(delta);

	float vecSrc[3];
	vecSrc[0] = PevVector(pevHit, PEV_ORIGIN).x;
	vecSrc[1] = PevVector(pevHit, PEV_ORIGIN).y;
	vecSrc[2] = PevVector(pevHit, PEV_ORIGIN).z + PevVector(pevHit, PEV_SIZE).z * 0.5f;

	float vecDir[3];
	{
		const float* right = GlobalsRight(globals);
		const float* up = GlobalsUp(globals);
		float randRight = RandomFloat(-1.0f, 1.0f) * 0.45f;	// FIRST draw -> v_right
		float randUp = RandomFloat(-1.0f, 1.0f) * 0.45f;	// SECOND draw -> v_up

		vecDir[0] = -g_vecAttackDir[0] + (up ? up[0] : 0.0f) * randUp + (right ? right[0] : 0.0f) * randRight;
		vecDir[1] = -g_vecAttackDir[1] + (up ? up[1] : 0.0f) * randUp + (right ? right[1] : 0.0f) * randRight;
		vecDir[2] = -g_vecAttackDir[2] + (up ? up[2] : 0.0f) * randUp + (right ? right[2] : 0.0f) * randRight;
	}

	float vecEnd[3];
	vecEnd[0] = vecSrc[0] + vecDir[0] * 128.0f;
	vecEnd[1] = vecSrc[1] + vecDir[1] * 128.0f;
	vecEnd[2] = vecSrc[2] + vecDir[2] * 128.0f;

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	// Reproduction of the binary's blood-decal trace: fNoMonsters arg = 0
	// (ignore monsters; decal lands on the wall behind the victim).
	EngineTraceLine(vecSrc, vecEnd, 0, pHitEdict, &tr);

	if (tr.flFraction != 1.0f)
	{
		int bloodColor = pHit ? pHit->BloodColor() : 0;

		EngineWriteByte(0, TURRET_SVC_TEMPENTITY);
		EngineWriteByte(0, TURRET_TE_DECAL);
		EngineWriteCoord(0, tr.vecEndPos[0]);
		EngineWriteCoord(0, tr.vecEndPos[1]);
		EngineWriteCoord(0, tr.vecEndPos[2]);
		EngineWriteShort(0, (short)EngineModelIndex(TraceHitIndex(&tr)));

		if (bloodColor == 70)
			EngineWriteByte(0, RandomLong(14, 19));
		else
			EngineWriteByte(0, RandomLong(20, 25));
	}
}

static void AddMultiDamage(entvars_t* pevInflictor, int hitEntIndex, float flDamage)
{
	if (!hitEntIndex)
		return;

	if (hitEntIndex == s_multiDamageTarget)
	{
		s_multiDamageAmount += flDamage;
		return;
	}

	ApplyMultiDamage(pevInflictor);
	s_multiDamageTarget = hitEntIndex;
	s_multiDamageAmount = flDamage;
}

static void BloodEffect(const float* vecOrigin, int bloodColor, float bloodAmount)
{
	if (bloodAmount > 255.0f)
		bloodAmount = 255.0f;

	EngineWriteByte(0, TURRET_SVC_TEMPENTITY);
	EngineWriteByte(0, TURRET_TE_BLOOD);
	EngineWriteCoord(0, vecOrigin[0]);
	EngineWriteCoord(0, vecOrigin[1]);
	EngineWriteCoord(0, vecOrigin[2]);
	EngineWriteCoord(0, g_vecAttackDir[0]);
	EngineWriteCoord(0, g_vecAttackDir[1]);
	EngineWriteCoord(0, g_vecAttackDir[2]);
	EngineWriteByte(0, (unsigned char)bloodColor);
	EngineWriteByte(0, (unsigned char)(int)bloodAmount);
}

// ImpactEffects: apply bullet damage + spawn impact visuals.
static void ImpactEffects(entvars_t* pevInflictor, float flDamage, int fSkipEffects, const float* vecDir, TraceResult* ptr)
{
	if (!pevInflictor || !vecDir || !ptr)
		return;

	int hitEntIndex = TraceHitIndex(ptr);
	// World hit -> tr.pHit == 0 (prog offset). Resolve through PROG_TO_EDICT like the
	// binary (no null-guard) so wall hits keep a valid pevHit and emit the decal.
	edict_t* pHitEdict = EnginePEntityOfEntIndex(hitEntIndex);
	entvars_t* pevHit = pHitEdict ? EngineGetVarsOfEnt(pHitEdict) : NULL;

	float hitPos[3];
	hitPos[0] = ptr->vecEndPos[0] - vecDir[0] * 4.0f;
	hitPos[1] = ptr->vecEndPos[1] - vecDir[1] * 4.0f;
	hitPos[2] = ptr->vecEndPos[2] - vecDir[2] * 4.0f;

	if (pevHit && (PevInt(pevHit, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
	{
		CBaseEntity* pEntity = (CBaseEntity*)EngineGetPrivateData(pHitEdict);
		int bloodColor = pEntity ? pEntity->BloodColor() : 0;

		AddMultiDamage(pevInflictor, hitEntIndex, flDamage);

		const char* pszClassname = EngineStringFromIndex(PevInt(pevHit, PEV_CLASSNAME));
		if (pszClassname)
		{
			if (strcmp(pszClassname, "func_glass") == 0)
				return;
			if (strcmp(pszClassname, "func_breakable") == 0)
				return;
		}

		if (PevFloat(pevHit, PEV_HEALTH) > 1000.0f)
			return;

		BloodEffect(hitPos, bloodColor, flDamage);

		if (PevFloat(pevHit, PEV_HEALTH) <= s_multiDamageAmount)
		{
			float org[3];
			org[0] = ptr->vecEndPos[0] + vecDir[0] * 8.0f;
			org[1] = ptr->vecEndPos[1] + vecDir[1] * 8.0f;
			org[2] = ptr->vecEndPos[2] + vecDir[2] * 8.0f;

			EngineWriteByte(0, TURRET_SVC_TEMPENTITY);
			EngineWriteByte(0, TURRET_TE_BLOODSTREAM);
			EngineWriteCoord(0, org[0]);
			EngineWriteCoord(0, org[1]);
			EngineWriteCoord(0, org[2]);
			EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
			EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
			EngineWriteCoord(0, RandomFloat(0.0f, 1.0f));
			EngineWriteByte(0, (unsigned char)bloodColor);
			EngineWriteByte(0, (unsigned char)RandomLong(80, 150));
		}
	}

	if (!fSkipEffects && pevHit && PevFloat(pevHit, PEV_SOLID) == 4.0f)
	{
		EngineWriteByte(0, TURRET_SVC_TEMPENTITY);
		EngineWriteByte(0, TURRET_TE_GUNSHOT);
		EngineWriteCoord(0, hitPos[0]);
		EngineWriteCoord(0, hitPos[1]);
		EngineWriteCoord(0, hitPos[2]);

		EngineWriteByte(0, TURRET_SVC_TEMPENTITY);
		EngineWriteByte(0, TURRET_TE_DECAL);
		EngineWriteCoord(0, ptr->vecEndPos[0]);
		EngineWriteCoord(0, ptr->vecEndPos[1]);
		EngineWriteCoord(0, ptr->vecEndPos[2]);
		EngineWriteShort(0, (short)EngineModelIndex(TraceHitIndex(ptr)));	// binary writes ModelIndex(tr.pHit), like ApplyMultiDamage

		if (hitEntIndex && (PevInt(pevHit, PEV_RENDERMODE) & 0x7FFFFFFF) != 0)
			EngineWriteByte(0, RandomLong(26, 28));
		else
			EngineWriteByte(0, RandomLong(0, 4));
	}
}

//=========================================================
// CTurret
//=========================================================

class CTurret : public CBaseMonster
{
public:
	CTurret();

	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	int Classify();
	void Use(CBaseEntity* pOther);
	int TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage);	// custom

private:
	void Death(int gibType);		// vtable slot 14

	// think states
	void InitialThink(CBaseEntity* pOther);
	void AutoSearchThink(CBaseEntity* pOther);
	void DeployThink(CBaseEntity* pOther);
	void RetractThink(CBaseEntity* pOther);
	void ActiveThink(CBaseEntity* pOther);
	void SpinUpThink(CBaseEntity* pOther);
	void FireThink(CBaseEntity* pOther);

	// helpers
	void SetTurretAnim(int sequence);
	void MoveTurret();
	edict_t* FindEnemy();

private:
	int m_iOn;					// member+344
	int m_iAutoStart;			// member+352
	int m_iSpin;				// member+348 (always-fire flag)
	int m_iOrientation;			// member+340 (0 = floor, 1 = ceiling)
	int m_iTurnRate;			// member+336
	float m_flMaxSpin;			// member+372 (maxsleep)
	float m_flSearchSpeed;		// member+376 (searchspeed)
	float m_flLastSight;		// member+368
	float m_flStartSpin;		// member+412 (ping/start-spin timer)
	int m_iMuzzleFlash;			// member+308 (tracer cycle 0..3)
	float m_flBaseYaw;			// member+416 (designer-set base yaw, angles.y)

	float m_vecGoalAngles[2];	// member+396 [0], member+400 [1]
	float m_vecCurAngles[3];	// member+384 [0], member+388 [1], member+392 [2]
	float m_vecLastSight[3];	// member+356..364
};

HL_COMPILE_TIME_ASSERT(sizeof(CTurret) <= 420, CTurret_private_data_size);

CTurret::CTurret()
{
	m_iOn = 0;
	m_iAutoStart = 0;
	m_iSpin = 0;
	m_iOrientation = 0;
	m_iTurnRate = 10;
	m_flMaxSpin = 5.0f;
	m_flSearchSpeed = 0.0f;
	m_flLastSight = 0.0f;
	m_flStartSpin = 0.0f;
	m_iMuzzleFlash = 0;
	m_flBaseYaw = 0.0f;

	m_vecGoalAngles[0] = 0.0f;
	m_vecGoalAngles[1] = 0.0f;
	m_vecCurAngles[0] = 0.0f;
	m_vecCurAngles[1] = 0.0f;
	m_vecCurAngles[2] = 0.0f;
	m_vecLastSight[0] = 0.0f;
	m_vecLastSight[1] = 0.0f;
	m_vecLastSight[2] = 0.0f;
}

//=========================================================
// Spawn
//=========================================================
void CTurret::Spawn()
{
	EnginePrecacheSound(kSndFire);
	EnginePrecacheSound(kSndPing);
	EnginePrecacheSound(kSndActive);
	EnginePrecacheSound(kSndDie);
	EnginePrecacheSound(kSndDie2);
	EnginePrecacheSound(kSndDie3);
	EnginePrecacheSound(kSndRetract);
	EnginePrecacheSound(kSndDeploy);
	EnginePrecacheSound(kSndSpinUp);
	EnginePrecacheSound(kSndSpinDown);
	EnginePrecacheSound(kSndSearch);
	EnginePrecacheSound(kSndAlert);

	EnginePrecacheModel(kTurretModel);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kTurretModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 16.0f, 16.0f, 16.0f);
		VecSet(mins, -16.0f, -16.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	m_Activity = 1;

	// pev+28 is an unnamed field in the alpha entvars (see SHARED_NEEDS);
	// this nextthink is immediately overwritten below by GlobalTime()+0.3.
	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, 28) + 1.0f;
	PevFloat(pev, PEV_MOVETYPE) = 5.0f;
	PevInt(pev, PEV_SEQUENCE) = 0;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_HEALTH) = 100.0f;
	PevFloat(pev, PEV_TAKEDAMAGE) = 2.0f;

	// flags |= FL_MONSTER (0x20)
	{
		float& flags = PevFloat(pev, PEV_FLAGS);
		flags = (float)((int)flags | 0x20);
	}

	m_iOn = 0;
	m_iSpin = 0;
	m_iTurnRate = 10;
	m_flMaxSpin = 5.0f;

	SetUse(&CTurret::Use);

	// stash the designer-set base yaw (angles.y, pev+80), then zero it.
	m_flBaseYaw = PevVector(pev, PEV_ANGLES).y;
	PevVector(pev, PEV_ANGLES).y = 0.0f;

	SetThink(&CTurret::InitialThink);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_SPAWN_DELAY;
}

//=========================================================
// KeyValue
//=========================================================
void CTurret::KeyValue(KeyValueData* pkvd)
{
	if (!pkvd)
		return;

	if (strcmp(pkvd->szKeyName, "maxsleep") == 0)
	{
		m_flMaxSpin = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "orientation") == 0)
	{
		m_iOrientation = atoi(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "searchspeed") == 0)
	{
		m_flSearchSpeed = (float)atoi(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "autostart") == 0)
	{
		m_iAutoStart = atoi(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "turnrate") == 0)
	{
		m_iTurnRate = atoi(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "style") == 0
		|| strcmp(pkvd->szKeyName, "height") == 0
		|| strcmp(pkvd->szKeyName, "killtarget") == 0
		|| strcmp(pkvd->szKeyName, "value1") == 0
		|| strcmp(pkvd->szKeyName, "value2") == 0
		|| strcmp(pkvd->szKeyName, "value3") == 0)
	{
		pkvd->fHandled = 1;
	}
}

//=========================================================
// Classify
//=========================================================
int CTurret::Classify()
{
	return 6;
}

//=========================================================
// SetTurretAnim
// Sets sequence + frame/framerate for deploy/retract/active.
//=========================================================
void CTurret::SetTurretAnim(int sequence)
{
	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	ResetSequenceInfo(TURRET_THINK_FAST);

	if (sequence == 3)
	{
		// retract: play backwards from the last frame
		PevFloat(pev, PEV_FRAME) = 255.0f;
		PevFloat(pev, PEV_FRAMERATE) = -1.0f;
	}
	else
	{
		PevFloat(pev, PEV_FRAME) = 0.0f;
		PevFloat(pev, PEV_FRAMERATE) = 1.0f;
	}
}

//=========================================================
// InitialThink
// One-shot startup: aim straight, decide between an
// auto-search loop and a dormant idle.
//=========================================================
void CTurret::InitialThink(CBaseEntity* pOther)
{
	// binary gates the pitch setup behind m_iOrientation==1 (ceiling turret); floor
	// turrets (orientation 0) never get the ChangePitch / ideal_pitch+pitch_speed overwrite.
	if (m_iOrientation == 1)
	{
		PevFloat(pev, PEV_IDEAL_PITCH) = 180.0f;
		PevFloat(pev, PEV_PITCH_SPEED) = 360.0f;
		EngineChangePitch(EdictFromEntvars(pev));
		PevFloat(pev, PEV_IDEAL_YAW) = -m_flBaseYaw;
	}
	else
		PevFloat(pev, PEV_IDEAL_YAW) = m_flBaseYaw;

	PevFloat(pev, PEV_YAWSPEED) = 360.0f;
	EngineChangeYaw(EdictFromEntvars(pev));

	m_vecGoalAngles[0] = 360.0f;

	if (m_iAutoStart)
	{
		m_iOn = 1;
		SetThink(&CTurret::AutoSearchThink);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_THINK_FAST;
	}
	else
	{
		SetDoNothingThink();
	}
}

//=========================================================
// AutoSearchThink
// Hunt for an enemy; on success, deploy.
//=========================================================
void CTurret::AutoSearchThink(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_SEARCH_DELAY;

	// drop the enemy if it died (health gone negative)
	int enemyIndex = PevInt(pev, PEV_ENEMY);
	if (enemyIndex)
	{
		edict_t* pEnemyEdict = EnginePEntityOfEntIndex(enemyIndex);
		entvars_t* pevEnemy = pEnemyEdict ? EngineGetVarsOfEnt(pEnemyEdict) : NULL;
		if (pevEnemy && *(unsigned int*)&PevFloat(pevEnemy, PEV_HEALTH) > 0x80000000)
			PevInt(pev, PEV_ENEMY) = 0;
	}

	if (PevInt(pev, PEV_ENEMY))
		return;

	edict_t* pFound = FindEnemy();
	if (!pFound || !EngineIndexOfEdict(pFound))
		return;

	SetThink(&CTurret::DeployThink);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_DEPLOY_DELAY;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 3, kSndAlert, TURRET_VOL, TURRET_ATTN_NORM);
}

//=========================================================
// DeployThink
// Raise the turret out of its housing.
//=========================================================
void CTurret::DeployThink(CBaseEntity* pOther)
{
	SetTurretAnim(2);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_THINK_FAST;
	AdvanceAnimation(TURRET_THINK_FAST);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 3, kSndDeploy, TURRET_VOL, TURRET_ATTN_NORM);

	if (m_fSequenceFinished)
	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 34.0f, 34.0f, 34.0f);
		VecSet(mins, -16.0f, -16.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);

		m_vecCurAngles[1] = m_flBaseYaw;
		m_vecCurAngles[0] = 360.0f;

		SetTurretAnim(1);
		SetThink(&CTurret::ActiveThink);
	}
}

//=========================================================
// RetractThink
// Tuck the turret back into its housing.
//=========================================================
void CTurret::RetractThink(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_THINK_FAST;
	AdvanceAnimation(TURRET_THINK_FAST);

	if (m_vecGoalAngles[0] != m_vecCurAngles[0])
	{
		MoveTurret();
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) != 3)
	{
		SetTurretAnim(3);
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 3, kSndRetract, TURRET_VOL, TURRET_ATTN_NORM);
	}

	if (m_fSequenceFinished)
	{
		SetTurretAnim(0);

		float mins[3];
		float maxs[3];
		VecSet(maxs, 16.0f, 16.0f, 16.0f);
		VecSet(mins, -16.0f, -16.0f, 0.0f);
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineSetSize(edict, mins, maxs);

		SetDoNothingThink();
	}
}

//=========================================================
// MoveTurret
// Step the two bone controllers toward the goal angles.
//=========================================================
void CTurret::MoveTurret()
{
	// pitch controller (controller 1): step the current pitch toward the goal
	if (m_vecGoalAngles[0] != m_vecCurAngles[0])
	{
		float flDir = (m_vecGoalAngles[0] > m_vecCurAngles[0]) ? 1.0f : -1.0f;
		m_vecCurAngles[0] = m_vecCurAngles[0] + TURRET_TURN_STEP * flDir;

		if (flDir == 1.0f)
		{
			if (m_vecCurAngles[0] > m_vecGoalAngles[0])
				m_vecCurAngles[0] = m_vecGoalAngles[0];
		}
		else
		{
			if (m_vecCurAngles[0] < m_vecGoalAngles[0])
				m_vecCurAngles[0] = m_vecGoalAngles[0];
		}

		SetBoneController(pev, 1, m_vecCurAngles[0]);
	}

	// yaw controller (controller 0): step the current yaw toward the goal
	if (m_vecGoalAngles[1] == m_vecCurAngles[1])
		return;

	float flDir = (m_vecGoalAngles[1] > m_vecCurAngles[1]) ? 1.0f : -1.0f;
	if (fabsf(m_vecGoalAngles[1] - m_vecCurAngles[1]) > 180.0f)
		flDir = -flDir;

	m_vecCurAngles[1] = m_vecCurAngles[1] + flDir * TURRET_TURN_STEP;

	if (*(unsigned int*)&m_vecCurAngles[1] > 0x80000000)
		m_vecCurAngles[1] = m_vecCurAngles[1] + 360.0f;
	else if (*(int*)&m_vecCurAngles[1] > 0x43B40000)	// > 360.0
		m_vecCurAngles[1] = m_vecCurAngles[1] - 360.0f;

	if (fabsf(m_vecCurAngles[1] - m_vecGoalAngles[1]) < 2.25f)
		m_vecCurAngles[1] = m_vecGoalAngles[1];

	float flValue = m_vecCurAngles[1] - m_flBaseYaw + 360.0f;
	if (flValue > 360.0f)
		flValue = flValue - 360.0f;

	SetBoneController(pev, 0, flValue);
}

//=========================================================
// FindEnemy
// Scan a 1024u sphere for a damageable, visible target of a
// hostile class; the player's own visibility/spawnflag gates.
//=========================================================
edict_t* CTurret::FindEnemy()
{
	edict_t* pTarget = NULL;

	// prefer a client already in PVS
	edict_t* pClient = EngineFindClientInPVS();
	if (pClient && EngineIndexOfEdict(pClient))
	{
		entvars_t* pevClient = EngineGetVarsOfEnt(pClient);
		if (pevClient && FVisible(pev, pevClient))
			pTarget = pClient;
	}

	if (!pTarget)
	{
		Vector vecOrigin = PevVector(pev, PEV_ORIGIN);
		edict_t* pEnt = EngineFindEntityInSphere(VecPtr(vecOrigin), TURRET_RANGE);

		while (pEnt && EngineIndexOfEdict(pEnt))
		{
			entvars_t* pevEnt = EngineGetVarsOfEnt(pEnt);
			if (!pevEnt)
				break;

			// take the first hostile, alive, in-range entity in the chain
			if ((PevInt(pevEnt, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
			{
				CBaseEntity* pEntity = (CBaseEntity*)EngineGetPrivateData(pEnt);
				int cls = pEntity ? pEntity->Classify() : 0;

				switch (cls)
				{
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 7:
				case 8:
				{
					TraceResult tr;
					memset(&tr, 0, sizeof(tr));
					EngineTraceLine(VecPtr(PevVector(pev, PEV_ORIGIN)), VecPtr(PevVector(pevEnt, PEV_ORIGIN)), 1, EdictFromEntvars(pev), &tr);

					// The binary `cmp [health], 479C4000h; jl accept`
					// reject health >= 80000.0 (invulnerable/cycler sentinel), not 73728.0.
					if (PevFloat(pevEnt, PEV_HEALTH) > 0.0f && PevFloat(pevEnt, PEV_HEALTH) < 80000.0f)
					{
						pTarget = pEnt;
						pEnt = NULL;	// accepted -> stop scanning
					}
					break;
				}
				default:
					break;
				}
			}

			if (pEnt)
			{
				int chainIndex = PevInt(pevEnt, PEV_CHAIN);
				pEnt = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
			}
		}
	}

	if (!pTarget || !EngineIndexOfEdict(pTarget))
		return NULL;

	entvars_t* pevTarget = EngineGetVarsOfEnt(pTarget);
	if (!pevTarget)
		return NULL;

	if (!FVisible(pev, pevTarget) || ((int)PevFloat(pevTarget, PEV_FLAGS) & 0x80) != 0)
		return NULL;

	// SF_MONSTER_TURRET_AUTOACTIVATE style: only activate when the candidate
	// can itself see the turret (looker = target, viewed = turret).
	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & 1) != 0)
	{
		if (!FInViewCone(pevTarget, pev, 0.7f))
			return NULL;
	}

	int enemyIndex = EngineIndexOfEdict(pTarget);
	PevInt(pev, PEV_ENEMY) = enemyIndex;
	PevInt(pev, PEV_GOALENTINDEX) = enemyIndex;

	m_vecEnemyLKP = PevVector(pevTarget, PEV_ORIGIN);
	m_flLastEnemySightTime = GlobalTime();

	return pTarget;
}

//=========================================================
// ActiveThink
// Deployed and scanning.  Pings periodically and, once an
// enemy is locked, spins up to fire.
//=========================================================
void CTurret::ActiveThink(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_THINK_FAST;

	float flTime = GlobalTime();
	if (((*(int*)&m_flStartSpin) & 0x7FFFFFFF) != 0)
	{
		if (flTime >= m_flStartSpin)
		{
			m_flStartSpin = 0.0f;
			edict_t* edict = EdictFromEntvars(pev);
			if (edict)
				EngineEmitSound(edict, 3, kSndPing, TURRET_VOL, TURRET_ATTN_PING);
		}
	}
	else
	{
		m_flStartSpin = flTime + 1.0f;
	}

	// validate / refresh the enemy (drop it once its health goes negative)
	int enemyIndex = PevInt(pev, PEV_ENEMY);
	if (enemyIndex)
	{
		edict_t* pEnemyEdict = EnginePEntityOfEntIndex(enemyIndex);
		entvars_t* pevEnemy = pEnemyEdict ? EngineGetVarsOfEnt(pEnemyEdict) : NULL;
		if (pevEnemy && *(unsigned int*)&PevFloat(pevEnemy, PEV_HEALTH) > 0x80000000)
			PevInt(pev, PEV_ENEMY) = 0;
	}

	if (!PevInt(pev, PEV_ENEMY))
		FindEnemy();

	if (PevInt(pev, PEV_ENEMY))
	{
		SetThink(&CTurret::SpinUpThink);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 1.4f;

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 3, kSndSpinUp, TURRET_VOL, TURRET_ATTN_NORM);
	}
}

//=========================================================
// SpinUpThink
// One-shot spin-up; hands off to the firing think.
//=========================================================
void CTurret::SpinUpThink(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_THINK_FAST;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 3, kSndActive, TURRET_VOL, TURRET_ATTN_NORM);

	AdvanceAnimation(TURRET_THINK_FAST);

	SetThink(&CTurret::FireThink);
}

//=========================================================
// FireThink
// Deployed and tracking.  Aims the two bone controllers at the
// enemy and runs an inlined two-bullet burst (NOT FireBullets).
// Spins down when the enemy dies or stays out of sight too long.
//=========================================================
void CTurret::FireThink(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_THINK_FAST;
	AdvanceAnimation(TURRET_THINK_FAST);

	edict_t* edict = EdictFromEntvars(pev);
	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);

	// no longer on, or no enemy -> spin down
	if (!m_iOn || !PevInt(pev, PEV_ENEMY))
	{
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 1.0f;
		if (edict)
			EngineEmitSound(edict, 3, kSndSpinDown, TURRET_VOL, TURRET_ATTN_NORM);
		SetThink(&CTurret::ActiveThink);
		return;
	}

	edict_t* pEnemyEdict = EnginePEntityOfEntIndex(PevInt(pev, PEV_ENEMY));
	entvars_t* pevEnemy = pEnemyEdict ? EngineGetVarsOfEnt(pEnemyEdict) : NULL;

	Vector vecOrigin = PevVector(pev, PEV_ORIGIN);

	// dead (health <= 0) or sentinel health 80000.0 (0x479C4000) -> spin down.
	// The binary `cmp [pevEnemy+0x108], 479C4000h` (80000 == the
	// invulnerable/cycler health value), NOT 73728.0.
	if (!pevEnemy
		|| *(int*)&PevFloat(pevEnemy, PEV_HEALTH) <= 0
		|| *(int*)&PevFloat(pevEnemy, PEV_HEALTH) == 0x479C4000)
	{
		PevInt(pev, PEV_ENEMY) = 0;
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 1.0f;
		if (edict)
			EngineEmitSound(edict, 3, kSndSpinDown, TURRET_VOL, TURRET_ATTN_NORM);
		SetThink(&CTurret::ActiveThink);
		return;
	}

	Vector vecMid = PevVector(pevEnemy, PEV_ORIGIN);

	BOOL fVisible = FVisible(pev, pevEnemy);
	if (fVisible)
	{
		// reset the out-of-sight grace timer; stash the sight position
		m_flLastSight = 0.0f;
		m_vecLastSight[0] = PevVector(pevEnemy, PEV_ORIGIN).x;
		m_vecLastSight[1] = PevVector(pevEnemy, PEV_ORIGIN).y;
		m_vecLastSight[2] = PevVector(pevEnemy, PEV_ORIGIN).z;
	}
	else
	{
		float flTime = GlobalsTime(globals);
		if ((*(unsigned int*)&m_flLastSight & 0x7FFFFFFF) != 0)
		{
			if (flTime > m_flLastSight)
			{
				// lost sight for too long -> spin down
				m_flLastSight = 0.0f;
				PevInt(pev, PEV_ENEMY) = 0;
				PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 1.0f;
				if (edict)
					EngineEmitSound(edict, 3, kSndSpinDown, TURRET_VOL, TURRET_ATTN_NORM);
				SetThink(&CTurret::ActiveThink);
				return;
			}
		}
		else
		{
			m_flLastSight = flTime + m_flMaxSpin;
		}
	}

	// raise the muzzle to the model's mid height
	float midHeightAdj = PevVector(pev, PEV_MAXS).z - PevVector(pev, PEV_MINS).z;
	if (m_iOrientation == 1)
		vecOrigin.z = vecOrigin.z + midHeightAdj * -0.5f;
	else
		vecOrigin.z = vecOrigin.z + midHeightAdj * 0.5f;

	// aim at the enemy's center of mass (players are already eye-centered)
	const char* enemyClass = EngineStringFromIndex(PevInt(pevEnemy, PEV_CLASSNAME));
	if (enemyClass && strcmp(enemyClass, "player") != 0)
		vecMid.z = vecMid.z + (PevVector(pevEnemy, PEV_MAXS).z - PevVector(pevEnemy, PEV_MINS).z) * 0.5f;

	float vecDirToEnemy[3];
	vecDirToEnemy[0] = vecMid.x - vecOrigin.x;
	vecDirToEnemy[1] = vecMid.y - vecOrigin.y;
	vecDirToEnemy[2] = vecMid.z - vecOrigin.z;
	float flDist = VecLength(vecDirToEnemy);

	float vecAngToEnemy[3];
	EngineVecToAngles(vecDirToEnemy, vecAngToEnemy);
	float flAimPitch = vecAngToEnemy[0];
	float flAimYaw = vecAngToEnemy[1];

	float flCurPitch = m_vecCurAngles[0];
	float flCurYaw = m_vecCurAngles[1];
	float flCurRoll = m_vecCurAngles[2];

	BOOL fShoot = 0;
	if (fVisible && flDist < 1024.0f)
		fShoot = 1;

	// when firing (or always-on) build the engine vectors from the barrel angles
	if (fShoot || m_iSpin)
	{
		if (m_iOrientation == 1)
		{
			flCurYaw = 180.0f - flCurYaw;
			flCurPitch = -flCurPitch;
		}

		float vecBarrel[3];
		vecBarrel[0] = flCurPitch;
		vecBarrel[1] = flCurYaw;
		vecBarrel[2] = flCurRoll;
		EngineMakeVectors(vecBarrel);
	}

	// shoot gate: barrel must point within ~5.7 degrees of the enemy
	if (fShoot)
	{
		float dir[3];
		dir[0] = vecDirToEnemy[0];
		dir[1] = vecDirToEnemy[1];
		dir[2] = vecDirToEnemy[2];

		float len = VecLength(dir);
		if (len == 0.0f)
			VecSet(dir, 0.0f, 0.0f, 0.0f);
		else
			Vec3Scale(dir, dir, 1.0f / len);

		const float* forward = GlobalsForward(globals);
		if (forward)
		{
			float flDot = forward[0] * dir[0] + forward[1] * dir[1] + forward[2] * dir[2];
			if (flDot <= 0.995f)
				fShoot = 0;
		}
	}

	if (fShoot || m_iSpin)
	{
		const float* forward = GlobalsForward(globals);
		const float* right = GlobalsRight(globals);
		const float* up = GlobalsUp(globals);

		SetTurretAnim(1);

		// muzzle source: 10u in front of the barrel, view-offset adjusted z
		float vecSrc[3];
		vecSrc[0] = (forward ? forward[0] : 0.0f) * 10.0f + vecOrigin.x;
		vecSrc[1] = (forward ? forward[1] : 0.0f) * 10.0f + vecOrigin.y;
		vecSrc[2] = vecOrigin.z - (PevVector(pev, PEV_VIEWOFS).z - 8.0f);

		ClearMultiDamage();

		for (int iShotCount = 0; iShotCount < 2; iShotCount++)
		{
			float flSpreadRight = RandomFloat(-1.0f, 1.0f) * 0.035f;

			float dir[3];
			dir[0] = vecDirToEnemy[0];
			dir[1] = vecDirToEnemy[1];
			dir[2] = vecDirToEnemy[2];
			float len = VecLength(dir);
			if (len == 0.0f)
				VecSet(dir, 0.0f, 0.0f, 0.0f);
			else
				Vec3Scale(dir, dir, 1.0f / len);

			float flSpreadUp = RandomFloat(-1.0f, 1.0f) * 0.035f;

			float vecShot[3];
			vecShot[0] = dir[0] + (right ? right[0] : 0.0f) * flSpreadRight + (up ? up[0] : 0.0f) * flSpreadUp;
			vecShot[1] = dir[1] + (right ? right[1] : 0.0f) * flSpreadRight + (up ? up[1] : 0.0f) * flSpreadUp;
			vecShot[2] = dir[2] + (right ? right[2] : 0.0f) * flSpreadRight + (up ? up[2] : 0.0f) * flSpreadUp;

			float vecEnd[3];
			vecEnd[0] = vecSrc[0] + vecShot[0] * 1024.0f;
			vecEnd[1] = vecSrc[1] + vecShot[1] * 1024.0f;
			vecEnd[2] = vecSrc[2] + vecShot[2] * 1024.0f;

			TraceResult tr;
			memset(&tr, 0, sizeof(tr));
			EngineTraceLine(vecSrc, vecEnd, 1, edict, &tr);	// fNoMonsters=1

			int fSkipTracerEffects = 0;
			m_iMuzzleFlash = (m_iMuzzleFlash + 1) % 4;
			if (m_iMuzzleFlash == 3)
			{
				fSkipTracerEffects = 1;
				EngineWriteByte(0, TURRET_SVC_TEMPENTITY);
				EngineWriteByte(0, TURRET_TE_TRACER);
				EngineWriteCoord(0, vecSrc[0]);
				EngineWriteCoord(0, vecSrc[1]);
				EngineWriteCoord(0, vecSrc[2]);
				EngineWriteCoord(0, tr.vecEndPos[0]);
				EngineWriteCoord(0, tr.vecEndPos[1]);
				EngineWriteCoord(0, tr.vecEndPos[2]);
			}

			if (tr.flFraction != 1.0f)
				ImpactEffects(pev, 2.0f, fSkipTracerEffects, vecShot, &tr);
		}

		ApplyMultiDamage(pev);

		if (edict)
			EngineEmitSound(edict, 1, kSndFire, TURRET_VOL, TURRET_ATTN_NORM);

		// scatter the model slightly when in always-on mode, then skip aiming
		if (m_iSpin)
		{
			float flYaw = (float)(255 * rand() / 32767);
			SetBoneController(pev, 0, flYaw);
			float flPitch = (float)(85 * rand() / -32767);
			SetBoneController(pev, 1, flPitch);

			MoveTurret();
			return;
		}
	}

	if (fVisible)
	{
		// aim the controllers toward where the enemy is
		float flYaw = flAimYaw;
		float flPitch = flAimPitch;

		if (m_iOrientation == 1)
			flYaw = 180.0f - flYaw;
		if (*(int*)&flYaw > 0x43B40000)		// > 360.0
			flYaw = flYaw - 360.0f;
		if (*(unsigned int*)&flYaw > 0x80000000)
			flYaw = flYaw + 360.0f;

		if (m_iOrientation != 1)
			flPitch = -flPitch;
		if (*(unsigned int*)&flPitch > 0x80000000)
			flPitch = flPitch + 360.0f;
		if (*(int*)&flPitch > 0x43B40000)	// > 360.0
			flPitch = flPitch - 360.0f;

		if (*(int*)&flPitch >= 0x43070000)	// >= 135.0
		{
			if (*(int*)&flPitch < 0x43898000)	// < 275.0
				flPitch = 275.0f;
		}
		else
		{
			flPitch = 360.0f;
		}

		m_vecGoalAngles[1] = flYaw;
		m_vecGoalAngles[0] = flPitch;
	}

	MoveTurret();
}

//=========================================================
// Use
// Toggle the turret between deployed (active) and retracted.
//=========================================================
void CTurret::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_THINK_FAST;

	if (m_iOn)
	{
		// turning off -> retract
		m_iOn = 0;
		m_flLastSight = 0.0f;
		PevInt(pev, PEV_ENEMY) = 0;

		m_vecCurAngles[1] = m_vecGoalAngles[1];
		m_vecGoalAngles[0] = 360.0f;

		SetThink(&CTurret::RetractThink);
	}
	else
	{
		// turning on -> deploy
		m_iOn = 1;

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 3, kSndAlert, TURRET_VOL, TURRET_ATTN_NORM);

		SetThink(&CTurret::DeployThink);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + TURRET_DEPLOY_DELAY;
	}
}

//=========================================================
// Death - vtable slot 14
// The turret death is handled in TakeDamage; the virtual death
// hook simply stops thinking.
//=========================================================
void CTurret::Death(int gibType)
{
	HL_UNUSED(gibType);

	SetDoNothingThink();
}

//=========================================================
// TakeDamage
//=========================================================
int CTurret::TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage)
{
	if (!pev)
		return 0;

	PevFloat(pev, PEV_HEALTH) = PevFloat(pev, PEV_HEALTH) - damage;

	if (PevFloat(pev, PEV_HEALTH) > 0.0f)
	{
		// low on health: chance to kick into always-on fire mode
		if (PevFloat(pev, PEV_HEALTH) <= 10.0f)
		{
			if (rand() > 800)
			{
				m_iSpin = 1;
				SetTurretAnim(1);
				SetThink(&CTurret::ActiveThink);
			}
		}

		HL_UNUSED(inflictor);
		return 1;
	}

	// dead -> play a random death sound, clear FL_MONSTER, run the
	// shared Killed/gib path.
	float rnd = RandomFloat(0.0f, 1.0f);
	edict_t* edict = EdictFromEntvars(pev);

	if (edict)
	{
		const char* sound;
		if (rnd <= 0.33f)
			sound = kSndDie;
		else if (rnd <= 0.66f)
			sound = kSndDie2;
		else
			sound = kSndDie3;

		EngineEmitSound(edict, 3, sound, TURRET_VOL, TURRET_ATTN_NORM);
	}

	SetTurretAnim(0);

	// flags &= ~FL_MONSTER (0x20)
	{
		float& flags = PevFloat(pev, PEV_FLAGS);
		flags = (float)((int)flags & ~0x20);
	}

	// The original tail-calls the shared monster Killed/gib routine
	// with the attacker's entity index.
	{
		edict_t* attackerEdict = EdictFromEntvars(attacker);
		Killed(EngineIndexOfEdict(attackerEdict));
	}

	HL_UNUSED(inflictor);
	return 1;
}

//=========================================================
// monster_turret (export)
//=========================================================
DLLEXPORT void monster_turret(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 420);
		if (!privateData)
			return;

		memset(privateData, 0, 420);

		CTurret* monster = new (privateData) CTurret();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
