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
// HGrunt - Human grunt monster
//=========================================================

#include <new>
#include <stdlib.h>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "ggrenade.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// Monster-specific constants
//=========================================================

#define HGRUNT_THINK_INTERVAL 0.1f
#define HGRUNT_MELEE_DIST 64.0f
#define HGRUNT_RANGE_DIST 1024.0f

#define HGRUNT_GRENADE_CHECK_DELAY 5.0f
#define HGRUNT_GRENADE_MIN_VEL 400.0f

#define HGRUNT_DMG_KICK 5.0f
#define HGRUNT_DMG_HEAVY_PAIN 10.0f

#define HGRUNT_VOL 1.0f
#define HGRUNT_ATTN_IDLE 2.0f
#define HGRUNT_ATTN_COMBAT 0.8f

static const char kHGruntModel[] = "models/hgrunt.mdl";
static const char kHAssaultModel[] = "models/hassault.mdl";
static const char kHumanAssaultClassname[] = "monster_human_assault";
static const char kHootSound[] = "player/hoot1.wav";

//=========================================================
// Sound Table
//=========================================================

static const char* pPainSounds[] =
{
	"hgrunt/gr_pain1.wav",
	"hgrunt/gr_pain2.wav",
	"hgrunt/gr_pain3.wav",
	"hgrunt/gr_pain4.wav",
	"hgrunt/gr_pain5.wav",
};

static const char* pIdleSounds[] =
{
	"hgrunt/gr_idle1.wav",
	"hgrunt/gr_idle2.wav",
	"hgrunt/gr_idle3.wav",
};

static const char* pAlertSounds[] =
{
	"hgrunt/gr_alert1.wav",
};

static const char* pMgunSounds[] =
{
	"hgrunt/gr_mgun1.wav",
	"hgrunt/gr_mgun2.wav",
	"hgrunt/gr_mgun3.wav",
};

static const char* pDieSounds[] =
{
	"hgrunt/gr_die1.wav",
	"hgrunt/gr_die2.wav",
	"hgrunt/gr_die3.wav",
};

static const char* pReloadSounds[] =
{
	"hgrunt/gr_reload1.wav",
};

static const char* pLoadTalkSounds[] =
{
	"hgrunt/gr_loadtalk.wav",
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

static const char* HGruntModelForClassname(entvars_t* pevSelf)
{
	const char* classname = EngineStringFromIndex(PevInt(pevSelf, PEV_CLASSNAME));
	if (classname && strcmp(classname, kHumanAssaultClassname) == 0)
		return kHAssaultModel;

	return kHGruntModel;
}

//=========================================================
// CHGrunt
//=========================================================

class CHGrunt : public CBaseMonster
{
public:
	CHGrunt();

	void Spawn();
	int Classify();

private:
	void SetActivity(int activity);
	void IdleSound();
	void AlertSound();
	int CheckAttacks(entvars_t* pevEnemy, float flDist);

	void ShootThink(CBaseEntity* pOther);
	void MeleeAttackThink(CBaseEntity* pOther);
	void ReloadThink(CBaseEntity* pOther);
	void HeavyPainThink(CBaseEntity* pOther);

	void ThrowGrenade(entvars_t* pevEnemy);
	void Pain(float flDamage);		// vtable slot 13
	void Death(int gibType);		// vtable slot 14
	// SetDeathActivity is the shared CBaseMonster::SetDeathActivity;
	// Death tail-calls it just like the binary, so no grunt-specific override exists.

private:
	float m_flNextGrenadeCheck;
	float m_flNextPainSound;
};

HL_COMPILE_TIME_ASSERT(sizeof(CHGrunt) <= 336, CHGrunt_private_data_size);

CHGrunt::CHGrunt()
{
	m_flNextGrenadeCheck = 0.0f;
	m_flNextPainSound = 0.0f;
}

//=========================================================
// Spawn
//=========================================================
void CHGrunt::Spawn()
{
	int i;
	const char* pszModel = HGruntModelForClassname(pev);

	EnginePrecacheModel(pszModel);

	EnginePrecacheSound(kHootSound);
	EnginePrecacheSound(pReloadSounds[0]);
	EnginePrecacheSound(pLoadTalkSounds[0]);

	for (i = 0; i < (int)ARRAYSIZE(pPainSounds); ++i)
		EnginePrecacheSound(pPainSounds[i]);

	EnginePrecacheSound(pAlertSounds[0]);

	for (i = 0; i < (int)ARRAYSIZE(pIdleSounds); ++i)
		EnginePrecacheSound(pIdleSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pMgunSounds); ++i)
		EnginePrecacheSound(pMgunSounds[i]);

	for (i = 0; i < (int)ARRAYSIZE(pDieSounds); ++i)
		EnginePrecacheSound(pDieSounds[i]);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, pszModel);

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
	PevFloat(pev, PEV_HEALTH) = RandomFloat(0.0f, 25.0f) + 50.0f;
	PevFloat(pev, PEV_YAWSPEED) = 14.0f;
	PevInt(pev, PEV_SEQUENCE) = 23;

	m_afEnemyFlags = 0;
	m_iRouteGoal = 0;
	m_iRouteIndex = 0;
	m_iSquadSize = 1;
	m_pSquadLeader = NULL;
	m_pSquadNext = NULL;

	m_iAmmo = 45;
	m_flDistTooFar = 512.0f;
	m_bloodColor = 70;

	PevInt(pev, PEV_WEAPON) = 4;

	m_flNextGrenadeCheck = 0.0f;
	m_flNextPainSound = 0.0f;

	SetThink(&CBaseMonster::WalkMonsterStart);
	// The binary adds to the existing nextthink, it does not reset it from time.
	PevFloat(pev, PEV_NEXTTHINK) = RandomFloat(0.0f, 0.5f) + PevFloat(pev, PEV_NEXTTHINK) + 0.5f;
}

//=========================================================
// Classify
//=========================================================
int CHGrunt::Classify()
{
	return 1;
}

//=========================================================
// SetActivity
//=========================================================
void CHGrunt::SetActivity(int activity)
{
	int sequence;

	switch (activity)
	{
	case 1:
	case 2:
	case 3:
	case 5:
	case 6:
		sequence = 11;
		break;

	case 4:
		sequence = 0;
		break;

	case 7:
		sequence = 13;
		break;

	case 8:
	case 9:
	case 23:
		sequence = 2;
		break;

	case 11:
		sequence = 21;
		break;

	case 12:
		sequence = 22;
		break;

	case 18:
	case 33:
		sequence = 9;
		break;

	case 29:
		sequence = 16;
		break;

	case 30:
		sequence = 15;
		break;

	case 32:
		sequence = 14;
		break;

	case 35:
		sequence = 3;
		break;

	case 37:
		sequence = 4;
		break;

	case 20:
	case 25:
	case 26:
	case 27:
	case 31:
		return;

	default:
		EngineAlertMessage(1, "HGrunt's monster state is bogus: %d", activity);
		return;
	}

	if (PevInt(pev, PEV_SEQUENCE) == sequence)
		return;

	PevInt(pev, PEV_SEQUENCE) = sequence;
	PevFloat(pev, PEV_FRAME) = 0.0f;
	ResetSequenceInfo(HGRUNT_THINK_INTERVAL);

	switch (sequence)
	{
	case 0:
	case 2:
	case 3:
	case 4:
	case 9:
	case 14:
	case 15:
	case 16:
	case 21:
	case 22:
		break;

	case 11:
	case 12:
	case 13:
		PevFloat(pev, PEV_FRAME) = RandomFloat(0.0f, 1.0f) * 256.0f;
		break;

	default:
		EngineAlertMessage(1, "Bogus HGrunt anim: %d", sequence);
		m_flFrameRate = 0.0f;
		m_flGroundSpeed = 0.0f;
		break;
	}
}

//=========================================================
// IdleSound
//=========================================================
void CHGrunt::IdleSound()
{
	float rnd = RandomFloat(0.0f, 1.0f);
	float vol = RandomFloat(0.5f, 1.0f);
	const char* sound;

	if (rnd <= 0.33f)
		sound = pIdleSounds[0];
	else if (rnd <= 0.66f)
		sound = pIdleSounds[1];
	else
		sound = pIdleSounds[2];

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, sound, vol, HGRUNT_ATTN_IDLE);

	m_flNextSoundTime = GlobalTime() + RandomFloat(0.0f, 5.0f) + 3.0f;
}

//=========================================================
// AlertSound
//=========================================================
void CHGrunt::AlertSound()
{
	if (m_iSquadSize > 1)
		return;

	int recruits = SquadRecruit(512);
	if (recruits == 0)
	{
		EngineAlertMessage(1, "No Squad");
		m_Activity = 8;
		return;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, pAlertSounds[0], HGRUNT_VOL, HGRUNT_ATTN_COMBAT);

	// During the walk the leader advertises the full count (recruits + own
	// previous size); each member receives that value.  After the walk the
	// leader's own size is reset to the recruit count alone.
	unsigned int flGroupSize = (unsigned int)recruits + m_iSquadSize;
	m_iSquadSize = flGroupSize;

	if (flGroupSize)
	{
		unsigned int i = 0;
		entvars_t* pevMember = pev;
		do
		{
			edict_t* pMemberEdict = EdictFromEntvars(pevMember);
			CBaseMonster* pMember = pMemberEdict ? (CBaseMonster*)EngineGetPrivateData(pMemberEdict) : NULL;
			if (!pMember)
				break;

			const char* classname = EngineStringFromIndex(PevInt(pevMember, PEV_CLASSNAME));
			if (classname)
			{
				if (strcmp(classname, "monster_human_grunt") == 0)
				{
					pMember->SetMonsterActivity(25);
					pMember->SetMonsterIdealActivity(5);
				}
				else if (strcmp(classname, "monster_human_assault") == 0)
				{
					pMember->SetMonsterActivity(20);
				}
			}

			++i;
			pMember->SetSquadSize(flGroupSize);
			EngineAlertMessage(1, "%d\n", pMember->SquadSize());

			pevMember = pMember->SquadNext();
		}
		while (flGroupSize > i);
	}

	EngineAlertMessage(1, "group of: %d\n", m_iSquadSize);

	m_iSquadSize = (unsigned int)recruits;
	m_Activity = 20;
	m_IdealActivity = 26;
	m_fSquadLeader = 1;	// binary sets *(this+0x138)=1 (this monster now leads the squad)
}

//=========================================================
// CheckAttacks
//=========================================================
int CHGrunt::CheckAttacks(entvars_t* pevEnemy, float flDist)
{
	if (!pevEnemy)
		return 0;

	if (m_iAmmo > 0)
	{
		if (flDist <= HGRUNT_MELEE_DIST && CheckMeleeAttack(pevEnemy))
		{
			m_IdealActivity = 7;
			SetThink(&CHGrunt::MeleeAttackThink);
			return 1;
		}

		if (CheckRangeAttack(pevEnemy) && flDist <= HGRUNT_RANGE_DIST)
		{
			m_IdealActivity = 7;
			SetThink(&CHGrunt::ShootThink);
			return 1;
		}

		if (GlobalTime() <= m_flNextGrenadeCheck)
			return 0;

		if (FVisible(pev, pevEnemy))
			return 0;

		ThrowGrenade(pevEnemy);
		m_flNextGrenadeCheck = GlobalTime() + HGRUNT_GRENADE_CHECK_DELAY;
		return 1;
	}

	m_IdealActivity = FVisible(pev, pevEnemy) ? 31 : 27;
	SetThink(&CHGrunt::ReloadThink);
	return 1;
}

//=========================================================
// ShootThink
//=========================================================
void CHGrunt::ShootThink(CBaseEntity* pOther)
{
	SetNextThink(HGRUNT_THINK_INTERVAL);

	entvars_t* pevEnemy = EnemyVars(pev);
	if (!pevEnemy)
		return;

	if (m_Activity != 30)
	{
		float start[3];
		float end[3];

		start[0] = PevVector(pev, PEV_ORIGIN).x + PevVector(pev, PEV_VIEWOFS).x;
		start[1] = PevVector(pev, PEV_ORIGIN).y + PevVector(pev, PEV_VIEWOFS).y;
		start[2] = PevVector(pev, PEV_ORIGIN).z + PevVector(pev, PEV_VIEWOFS).z;

		end[0] = PevVector(pevEnemy, PEV_ORIGIN).x + PevVector(pevEnemy, PEV_VIEWOFS).x;
		end[1] = PevVector(pevEnemy, PEV_ORIGIN).y + PevVector(pevEnemy, PEV_VIEWOFS).y;
		end[2] = PevVector(pevEnemy, PEV_ORIGIN).z + PevVector(pevEnemy, PEV_VIEWOFS).z;

		float dir[3];
		Vec3Sub(dir, end, start);
		if (!VecNormalize(dir))
			VecSet(dir, 0.0f, 0.0f, 0.0f);

		if (CheckFriendlyFire(pev, dir[0], dir[1], dir[2], HGRUNT_RANGE_DIST))
		{
			m_Activity = 27;
			m_IdealActivity = 31;
			SetThink(&CBaseMonster::MonsterThink);
			return;
		}

		m_Activity = 30;
		SetActivity(30);
	}

	GetAnimationEventFlags(HGRUNT_THINK_INTERVAL);
	AdvanceAnimation(HGRUNT_THINK_INTERVAL);

	float flDist = UpdateEnemyInfo(pevEnemy);
	if (flDist <= HGRUNT_MELEE_DIST)
	{
		SetThink(&CHGrunt::MeleeAttackThink);
		return;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineChangeYaw(edict);

	if ((m_afEnemyFlags & 2) == 0)
	{
		m_Activity = 6;
		SetThink(&CBaseMonster::MonsterThink);
		return;
	}

	{
		float start[3];
		float end[3];

		start[0] = PevVector(pev, PEV_ORIGIN).x + PevVector(pev, PEV_VIEWOFS).x;
		start[1] = PevVector(pev, PEV_ORIGIN).y + PevVector(pev, PEV_VIEWOFS).y;
		start[2] = PevVector(pev, PEV_ORIGIN).z + PevVector(pev, PEV_VIEWOFS).z;

		end[0] = PevVector(pevEnemy, PEV_ORIGIN).x + PevVector(pevEnemy, PEV_VIEWOFS).x;
		end[1] = PevVector(pevEnemy, PEV_ORIGIN).y + PevVector(pevEnemy, PEV_VIEWOFS).y;
		end[2] = PevVector(pevEnemy, PEV_ORIGIN).z + PevVector(pevEnemy, PEV_VIEWOFS).z;

		float dir[3];
		Vec3Sub(dir, end, start);
		if (!VecNormalize(dir))
			VecSet(dir, 0.0f, 0.0f, 0.0f);

		if (CheckFriendlyFire(pev, dir[0], dir[1], dir[2], HGRUNT_RANGE_DIST))
		{
			m_Activity = 27;
			m_IdealActivity = 31;
			SetThink(&CBaseMonster::MonsterThink);
			return;
		}

		if (edict)
			EngineEmitSound(edict, 1, pMgunSounds[rand() % ARRAYSIZE(pMgunSounds)], HGRUNT_VOL, HGRUNT_ATTN_COMBAT);

		FireBullets(1, dir, 0.035f, 0.035f, 0, HGRUNT_RANGE_DIST);
		--m_iAmmo;

		if (m_fSequenceFinished)
		{
			if (m_iAmmo > 0)
			{
				PevFloat(pev, PEV_FRAME) = 0.0f;
			}
			else
			{
				m_Activity = 25;
				m_IdealActivity = 31;
				SetThink(&CBaseMonster::MonsterThink);
				m_flNextAttack = GlobalTime() + 1.0f;
			}
		}
	}
}

//=========================================================
// MeleeAttackThink
//=========================================================
void CHGrunt::MeleeAttackThink(CBaseEntity* pOther)
{
	SetNextThink(HGRUNT_THINK_INTERVAL);

	if (m_Activity != 29)
	{
		m_Activity = 29;
		SetActivity(29);
		m_flNextAttack = GlobalTime() + 2.0f;
	}

	int events = GetAnimationEventFlags(HGRUNT_THINK_INTERVAL);
	AdvanceAnimation(HGRUNT_THINK_INTERVAL);

	entvars_t* pevEnemy = EnemyVars(pev);
	if (pevEnemy)
		UpdateEnemyInfo(pevEnemy);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineChangeYaw(edict);

	if ((events & 8) != 0)
	{
		if (edict)
			EngineEmitSound(edict, 2, kHootSound, HGRUNT_VOL, HGRUNT_ATTN_COMBAT);

		// The binary copies pev->angles and calls MakeVectors at the top of the
		// (events&8) block, so gpGlobals forward/up are recomputed from the grunt's
		// facing before the kick trace and the (250*forward + 200*up) knockback.
		EngineMakeVectors(VecPtr(PevVector(pev, PEV_ANGLES)));
		void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
		const float* forward = GlobalsForward(globals);
		const float* up = GlobalsUp(globals);

		float start[3];
		start[0] = PevVector(pev, PEV_ORIGIN).x;
		start[1] = PevVector(pev, PEV_ORIGIN).y;
		start[2] = PevVector(pev, PEV_ORIGIN).z + 50.0f;

		float end[3];
		end[0] = start[0] + (forward ? forward[0] : 0.0f) * 64.0f;
		end[1] = start[1] + (forward ? forward[1] : 0.0f) * 64.0f;
		end[2] = start[2] + (forward ? forward[2] : 0.0f) * 64.0f - 16.0f;

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine(start, end, 1, edict, &tr);

		// The binary resolves the traced entity unconditionally and gates only
		// on its takedamage flag (no flFraction/index guard).
		{
			edict_t* pHitEdict = TraceHitEdict(&tr);
			entvars_t* pevHit = pHitEdict ? EngineGetVarsOfEnt(pHitEdict) : NULL;

			if (pevHit && (PevInt(pevHit, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
			{
				CBaseEntity* pEntity = (CBaseEntity*)EngineGetPrivateData(pHitEdict);
				if (pEntity)
					pEntity->TakeDamage(pev, pev, HGRUNT_DMG_KICK);

				PevFloat(pevHit, PEV_V_ANGLE) = 15.0f;

				if (forward)
				{
					PevVector(pevHit, PEV_VELOCITY).x += forward[0] * 250.0f;
					PevVector(pevHit, PEV_VELOCITY).y += forward[1] * 250.0f;
					PevVector(pevHit, PEV_VELOCITY).z += forward[2] * 250.0f;
				}

				if (up)
				{
					PevVector(pevHit, PEV_VELOCITY).x += up[0] * 200.0f;
					PevVector(pevHit, PEV_VELOCITY).y += up[1] * 200.0f;
					PevVector(pevHit, PEV_VELOCITY).z += up[2] * 200.0f;
				}

				m_flNextAttack = GlobalTime();
				m_Activity = m_IdealActivity;
				SetThink(&CBaseMonster::MonsterThink);
				return;
			}
		}
	}

	if (m_fSequenceFinished)
	{
		SetThink(&CBaseMonster::MonsterThink);
		m_Activity = m_IdealActivity;
	}
}

//=========================================================
// ReloadThink
//=========================================================
void CHGrunt::ReloadThink(CBaseEntity* pOther)
{
	SetNextThink(HGRUNT_THINK_INTERVAL);

	if (m_Activity != 32)
	{
		m_Activity = 32;
		SetActivity(32);
	}

	int events = GetAnimationEventFlags(HGRUNT_THINK_INTERVAL);
	AdvanceAnimation(HGRUNT_THINK_INTERVAL);

	entvars_t* pevEnemy = EnemyVars(pev);
	if (pevEnemy)
		UpdateEnemyInfo(pevEnemy);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineChangeYaw(edict);

	if ((events & 2) != 0)
	{
		if (edict)
		{
			EngineEmitSound(edict, 1, pReloadSounds[0], HGRUNT_VOL, HGRUNT_ATTN_COMBAT);
			if (RandomFloat(0.0f, 1.0f) < 0.25f)
				EngineEmitSound(edict, 2, pLoadTalkSounds[0], HGRUNT_VOL, HGRUNT_ATTN_COMBAT);
		}
	}

	if (m_fSequenceFinished)
	{
		m_iAmmo = 45;
		SetThink(&CBaseMonster::MonsterThink);
		m_Activity = m_IdealActivity;
	}
}

//=========================================================
// HeavyPainThink
//=========================================================
void CHGrunt::HeavyPainThink(CBaseEntity* pOther)
{
	if (m_Activity != 33)
	{
		m_Activity = 33;
		SetActivity(33);
	}

	SetNextThink(HGRUNT_THINK_INTERVAL);
	AdvanceAnimation(HGRUNT_THINK_INTERVAL);

	entvars_t* pevEnemy = EnemyVars(pev);
	if (pevEnemy)
		UpdateEnemyInfo(pevEnemy);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineChangeYaw(edict);

	if (!m_fSequenceFinished)
		return;

	SetThink(&CBaseMonster::MonsterThink);

	if (pevEnemy && FInViewCone(pev, pevEnemy, 0.1f))
	{
		m_Activity = 25;
		m_IdealActivity = 5;
	}
	else
	{
		m_Activity = 6;
	}
}

//=========================================================
// ThrowGrenade
//=========================================================
void CHGrunt::ThrowGrenade(entvars_t* pevEnemy)
{
	if (!pevEnemy)
		return;

	float vecTarget[3];
	vecTarget[0] = PevVector(pevEnemy, PEV_ORIGIN).x;
	vecTarget[1] = PevVector(pevEnemy, PEV_ORIGIN).y;
	vecTarget[2] = PevVector(pevEnemy, PEV_ORIGIN).z + 32.0f;

	float vecStart[3];
	vecStart[0] = PevVector(pev, PEV_ORIGIN).x;
	vecStart[1] = PevVector(pev, PEV_ORIGIN).y;
	vecStart[2] = PevVector(pev, PEV_ORIGIN).z + 32.0f;

	float vecTossTarget[3];
	GetTossTarget(vecTossTarget, pev, vecStart, vecTarget);

	if (vecTossTarget[0] == 0.0f && vecTossTarget[1] == 0.0f && vecTossTarget[2] == 0.0f)
		return;

	{
		const char* myClass = EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME));
		Vector enemyOrigin = PevVector(pevEnemy, PEV_ORIGIN);

		edict_t* pEnt = EngineFindEntityInSphere((const float*)&enemyOrigin, 72.0f);
		while (pEnt)
		{
			if (!EngineIndexOfEdict(pEnt))
				break;

			entvars_t* pevEnt = EngineGetVarsOfEnt(pEnt);
			if (!pevEnt)
				break;

			const char* pszClass = EngineStringFromIndex(PevInt(pevEnt, PEV_CLASSNAME));
			if (pszClass && myClass && strcmp(pszClass, myClass) == 0)
				return;

			int chainIndex = PevInt(pevEnt, PEV_CHAIN);
			pEnt = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
		}
	}

	float dir[3];
	dir[0] = vecTossTarget[0] - PevVector(pev, PEV_ORIGIN).x;
	dir[1] = vecTossTarget[1] - PevVector(pev, PEV_ORIGIN).y;
	dir[2] = vecTossTarget[2] - PevVector(pev, PEV_ORIGIN).z;

	float len = VecLength(dir);
	if (len <= 0.0f)
		return;

	float inv = 1.0f / len;
	dir[0] *= inv;
	dir[1] *= inv;
	dir[2] *= inv;

	float dx = PevVector(pev, PEV_ORIGIN).x - vecTossTarget[0];
	float dy = PevVector(pev, PEV_ORIGIN).y - vecTossTarget[1];

	float flHoriz = sqrtf(dx * dx + dy * dy);
	float flAdjust = (flHoriz * 2.0f + vecTossTarget[2] - PevVector(pev, PEV_ORIGIN).z) * 0.5f;

	float vecVel[3];
	vecVel[0] = dir[0] * flAdjust * 2.0f;
	vecVel[1] = dir[1] * flAdjust * 2.0f;
	vecVel[2] = dir[2] * flAdjust * 2.0f;

	if (VecLength(vecVel) < HGRUNT_GRENADE_MIN_VEL)
		return;

	ShootTimedGrenade(pev, vecStart, vecVel);
}

//=========================================================
// Pain - vtable slot 13
//=========================================================
void CHGrunt::Pain(float flDamage)
{
	float rnd = RandomFloat(0.0f, 1.0f);
	PevFloat(pev, PEV_PAIN_FINISHED) = GlobalTime() + 1.0f;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
	{
		const char* sound = pPainSounds[2];

		if (GlobalTime() <= m_flNextPainSound)
		{
			if (rnd <= 0.33f)
				sound = pPainSounds[2];
			else if (rnd <= 0.66f)
				sound = pPainSounds[3];
			else
				sound = pPainSounds[4];
		}
		else
		{
			if (rnd <= 0.2f)
				sound = pPainSounds[0];
			else if (rnd <= 0.4f)
				sound = pPainSounds[1];
			else if (rnd <= 0.6f)
				sound = pPainSounds[2];
			else if (rnd <= 0.8f)
				sound = pPainSounds[3];
			else
				sound = pPainSounds[4];

			m_flNextPainSound = GlobalTime() + 15.0f;
		}

		EngineEmitSound(edict, 2, sound, HGRUNT_VOL, HGRUNT_ATTN_COMBAT);
	}

	if (m_Activity == 1 || m_Activity == 4)
	{
		// Being hurt while idle/walking rallies the squad (vtable slot 12).
		AlertSound();
		SetThink(&CBaseMonster::MonsterThink);
		return;
	}

	if (flDamage >= HGRUNT_DMG_HEAVY_PAIN)
	{
		SetThink(&CHGrunt::HeavyPainThink);
		SetNextThink(0.0f);
		return;
	}

	SetThink(&CBaseMonster::MonsterThink);

	entvars_t* pevEnemy = EnemyVars(pev);
	if (!pevEnemy)
		return;

	if (FVisible(pev, pevEnemy))
	{
		if (m_Activity != 11 && m_Activity != 12)
		{
			if (RandomFloat(0.0f, 1.0f) >= 0.75f)
			{
				m_Activity = 31;
			}
			else
			{
				m_Activity = 25;
				m_IdealActivity = 5;
			}
		}
	}
	else
	{
		m_Activity = 27;
		m_IdealActivity = 6;
	}
}

//=========================================================
// Death - vtable slot 14
//=========================================================
void CHGrunt::Death(int gibType)
{
	HL_UNUSED(gibType);

	float rnd = RandomFloat(0.0f, 1.0f);

	// The binary: the die scream plays on a normal kill and is
	// SILENT on heavy over-kill. The binary uses an unsigned compare of the raw
	// health bits against 0xC1F00000 (-30.0f): play when health > -30.
	if (PevFloat(pev, PEV_HEALTH) > -30.0f)
	{
		const char* sound;

		if (rnd <= 0.33f)
			sound = pDieSounds[0];
		else if (rnd <= 0.66f)
			sound = pDieSounds[1];
		else
			sound = pDieSounds[2];

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 2, sound, HGRUNT_VOL, HGRUNT_ATTN_COMBAT);
	}

	// The binary always tail-calls the binary, the shared
	// CBaseMonster::SetDeathActivity(0).
	SetDeathActivity(0);
}

//=========================================================
// TakeDamage routes through the shared CBaseMonster::TakeDamage
// (vtable slot 17), which raises Pain
// Death (which tail-calls SetDeathActivity).
//=========================================================

//=========================================================
// monster_human_grunt
//=========================================================
DLLEXPORT void monster_human_grunt(entvars_t* pev)
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

		CHGrunt* monster = new (privateData) CHGrunt();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

// NOTE: the original alpha hl.dll has NO monster_human_assault spawn export.
// Maps (c1a2a/c2a4a/c2a4b/c3a1) place that classname, but with no spawn
// function the engine removes those entities, so the original game never shows
// an "assault"/minigun grunt.  A prior recreation added a monster_human_assault
// export bound to CHGrunt with models/hassault.mdl; driven by the hgrunt
// sequence indices that model rendered as a broken flat-shaded blob ("the cube")
// that the original never produced.  Removed to match the original export table.
