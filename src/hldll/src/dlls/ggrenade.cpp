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
// Grenade - Timed grenades and radius damage
//=========================================================

#include <math.h>
#include <new>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "ggrenade.h"
#include "utils.h"

enum
{
	SVC_TEMPENTITY = 23,
	TE_EXPLOSION = 3,
	TE_DECAL = 104,
	TE_ALPHABREAKMODEL = 107,
};

static const char kGrenadeClassname[] = "grenade";
static const char kPlayerClassname[] = "player";

static const char kGrenadeModel[] = "models/grenade.mdl";

static const char kDebris1[] = "weapons/debris1.wav";
static const char kDebris2[] = "weapons/debris2.wav";
static const char kDebris3[] = "weapons/debris3.wav";
static const char kBounce1[] = "weapons/g_bounce1.wav";
static const char kBounce2[] = "weapons/g_bounce2.wav";
static const char kBounce3[] = "weapons/g_bounce3.wav";
static const char kFuncBreakable[] = "func_breakable";
static const char kFuncGlass[] = "func_glass";

extern short g_sModelIndexShrapnel;

static Vector VecFromFloats(const float* vec)
{
	Vector out;
	out.x = vec[0];
	out.y = vec[1];
	out.z = vec[2];
	return out;
}

//=========================================================
// RadiusDamage
//=========================================================
static void RadiusDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int iClassIgnore)
{
	if (!pevInflictor)
		return;

	PevVector(pevInflictor, PEV_ORIGIN).z = PevVector(pevInflictor, PEV_ORIGIN).z + 1.0f;

	Vector vecSrc = PevVector(pevInflictor, PEV_ORIGIN);
	float flRadius = flDamage * 2.0f;

	edict_t* pEdict = EngineFindEntityInSphere((const float*)&vecSrc, flRadius);
	while (pEdict)
	{
		if (!EngineIndexOfEdict(pEdict))
			return;

		entvars_t* pevHit = EngineGetVarsOfEnt(pEdict);
		if (pevHit && (PevInt(pevHit, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
		{
			CBaseEntity* pEntity = (CBaseEntity*)EngineGetPrivateData(pEdict);
			if (pEntity && pEntity->Classify() != iClassIgnore)
			{
				float vecSpot[3];
				vecSpot[0] = PevVector(pevHit, PEV_ORIGIN).x;
				vecSpot[1] = PevVector(pevHit, PEV_ORIGIN).y;
				vecSpot[2] = PevVector(pevHit, PEV_ORIGIN).z + PevVector(pevHit, PEV_SIZE).z * 0.5f;

				TraceResult tr;
				memset(&tr, 0, sizeof(tr));
				EngineTraceLine((const float*)&vecSrc, vecSpot, 0, EdictFromEntvars(pevInflictor), &tr);

				if (tr.flFraction == 1.0f)
				{
					float delta[3];
					delta[0] = vecSrc.x - PevVector(pevHit, PEV_ORIGIN).x;
					delta[1] = vecSrc.y - PevVector(pevHit, PEV_ORIGIN).y;
					delta[2] = vecSrc.z - PevVector(pevHit, PEV_ORIGIN).z;

					float flAdjustedDamage = flDamage - VecLength(delta) * 0.4f;
					if (flAdjustedDamage < 0.0f)
						flAdjustedDamage = 0.0f;

					pEntity->TakeDamage(pevInflictor, pevAttacker, flAdjustedDamage);
				}
			}
		}

		int chainIndex = pevHit ? PevInt(pevHit, PEV_CHAIN) : 0;
		pEdict = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
	}
}

//=========================================================
// GetTossTarget
//=========================================================
BOOL GetTossTarget(float* out, entvars_t* pevOwner, const float* start, const float* target)
{
	if (!out)
		return 0;

	out[0] = 0.0f;
	out[1] = 0.0f;
	out[2] = 0.0f;

	if (!pevOwner || !start || !target)
		return 0;

	void* globals = GlobalsFromEntvars(pevOwner);
	edict_t* pOwnerEdict = EdictFromEntvars(pevOwner);
	if (!globals || !pOwnerEdict)
		return 0;

	EngineMakeVectors((const float*)&PevVector(pevOwner, PEV_ANGLES));

	const float* forward = GlobalsForward(globals);
	const float* up = GlobalsUp(globals);
	const float* right = GlobalsRight(globals);
	if (!forward || !up || !right)
		return 0;

	// Binary reuses ONE random value in both terms (== r*48 - 24), not two independent
	// draws. (Faithful structure; binary actually reads a shared global.)
	float r = RandomFloat(0.0f, 1.0f);
	float flRand = (r * 16.0f - 8.0f) + (r * 32.0f - 16.0f);

	float vecTarget[3];
	vecTarget[0] = target[0] + right[0] * flRand + forward[0] * flRand;
	vecTarget[1] = target[1] + right[1] * flRand + forward[1] * flRand;
	vecTarget[2] = target[2] + right[2] * flRand + forward[2] * flRand;

	float mid[3];
	mid[0] = (vecTarget[0] - start[0]) * 0.5f + start[0];
	mid[1] = (vecTarget[1] - start[1]) * 0.5f + start[1];
	mid[2] = (vecTarget[2] - start[2]) * 0.5f + start[2];

	float upOffset[3];
	upOffset[0] = up[0] * 1024.0f;
	upOffset[1] = up[1] * 1024.0f;
	upOffset[2] = up[2] * 1024.0f;

	float endUp[3];
	Vec3Add(endUp, mid, upOffset);

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(mid, endUp, 0, pOwnerEdict, &tr);	// fNoMonsters=0

	float vecApex[3];
	vecApex[0] = tr.vecEndPos[0];
	vecApex[1] = tr.vecEndPos[1];
	vecApex[2] = tr.vecEndPos[2];

	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(start, vecApex, 1, pOwnerEdict, &tr);	// fNoMonsters=1
	if (tr.flFraction != 1.0f)
	{
		vecApex[0] = 0.0f;
		vecApex[1] = 0.0f;
		vecApex[2] = 0.0f;
	}

	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(vecTarget, vecApex, 1, pOwnerEdict, &tr);	// fNoMonsters=1
	if (tr.flFraction != 1.0f)
	{
		vecApex[0] = 0.0f;
		vecApex[1] = 0.0f;
		vecApex[2] = 0.0f;
	}

	if (vecApex[0] != 0.0f || vecApex[1] != 0.0f || vecApex[2] != 0.0f)
	{
		DrawDebugLine(VecFromFloats(vecTarget), VecFromFloats(start));
		DrawDebugLine(VecFromFloats(start), VecFromFloats(vecApex));
		DrawDebugLine(VecFromFloats(vecTarget), VecFromFloats(vecApex));
	}

	out[0] = vecApex[0];
	out[1] = vecApex[1];
	out[2] = vecApex[2];
	return 1;
}

//=========================================================
// CGrenade
//=========================================================

class CGrenade : public CBaseMonster
{
public:
	void Init(entvars_t* pevOwner, const float* start, const float* velocity);

	void Touch(CBaseEntity* pOther);	// vtable slot 6

private:
	void ExplodeThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CGrenade) <= 336, CGrenade_private_data_size);

static CGrenade* CreateGrenadeEntity()
{
	edict_t* pEdict = EngineCreateEntity();
	entvars_t* pev = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
	if (!pev)
		return NULL;

	void* privateData = EngineGetPrivateData(pEdict);
	if (!privateData)
	{
		privateData = EngineAllocPrivateData(pEdict, 336);
		if (!privateData)
			return NULL;

		memset(privateData, 0, 336);

		CGrenade* pGrenade = new (privateData) CGrenade();
		pGrenade->pev = pev;
		pGrenade->m_pGlobals = GlobalsFromEntvars(pev);
		return pGrenade;
	}

	return (CGrenade*)privateData;
}

//=========================================================
// Init
//=========================================================

void CGrenade::Init(entvars_t* pevOwner, const float* start, const float* velocity)
{
	if (!pev || !start || !velocity)
		return;

	edict_t* edict = EdictFromEntvars(pev);

	PevFloat(pev, PEV_MOVETYPE) = 10.0f;	// MOVETYPE_BOUNCE (0x41200000)
	PevInt(pev, PEV_CLASSNAME) = EngineAllocString(kGrenadeClassname);

	// The engine-owned pev is not zeroed by us; clear the render fields and the
	// field at +260 exactly as the original Init does (offsets 244/240/260).
	PevInt(pev, 244) = 0;
	PevInt(pev, PEV_RENDERMODE) = 0;	// +240
	PevInt(pev, 260) = 0;

	if (pevOwner)
	{
		const char* ownerClass = EngineStringFromIndex(PevInt(pevOwner, PEV_CLASSNAME));
		if (ownerClass && strcmp(ownerClass, kPlayerClassname) == 0)
		{
			PevFloat(pev, PEV_GRAVITY) = 0.4f;	// +144 (0x3ECCCCCD)
		}
	}

	PevFloat(pev, PEV_SOLID) = 2.0f;	// SOLID_BBOX (0x40000000)

	if (pevOwner)
	{
		edict_t* ownerEdict = EdictFromEntvars(pevOwner);
		PevInt(pev, PEV_OWNER_ENTINDEX) = ownerEdict ? EngineIndexOfEdict(ownerEdict) : 0;
	}

	if (edict)
		EngineSetModel(edict, kGrenadeModel);

	{
		float vecZero[3] = {0.0f, 0.0f, 0.0f};
		if (edict)
			EngineSetSize(edict, vecZero, vecZero);
	}

	{
		Vector& vecOrigin = PevVector(pev, PEV_ORIGIN);
		vecOrigin.x = start[0];
		vecOrigin.y = start[1];
		vecOrigin.z = start[2];
	}

	{
		Vector& vecVel = PevVector(pev, PEV_VELOCITY);
		vecVel.x = velocity[0];
		vecVel.y = velocity[1];
		vecVel.z = velocity[2];
	}

	{
		float ang[3];
		EngineVecToAngles(velocity, ang);
		PevVector(pev, PEV_ANGLES).x = ang[0];
		PevVector(pev, PEV_ANGLES).y = ang[1];
		PevVector(pev, PEV_ANGLES).z = ang[2];
	}

	PevFloat(pev, PEV_DMG) = 100.0f;

	const char* ownerClass = pevOwner ? EngineStringFromIndex(PevInt(pevOwner, PEV_CLASSNAME)) : NULL;
	if (ownerClass && strcmp(ownerClass, kPlayerClassname) == 0)
	{
		SetDoNothingThink();
		PevVector(pev, PEV_AVELOCITY).x = RandomFloat(-100.0f, -500.0f);
	}
	else
	{
		SetThink(&CGrenade::ExplodeThink);
		SetNextThink(2.0f);
		PevVector(pev, PEV_AVELOCITY).x = -400.0f;	// 0xC3C80000
	}
}

//=========================================================
// Touch
//
// Player contact grenade: explode on whatever it hits (and forward
// our blocking damage to breakable/glass).  Otherwise it is a world
// bounce: randomize a bounce sound, set spin, and damp the velocity.
//=========================================================
void CGrenade::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	if (!pev || !m_pGlobals)
		return;

	int otherIndex = *GlobalsInt(m_pGlobals, GLOBALS_OTHER_ENTINDEX);
	edict_t* pOtherEdict = otherIndex ? EnginePEntityOfEntIndex(otherIndex) : NULL;
	entvars_t* pevOther = pOtherEdict ? EngineGetVarsOfEnt(pOtherEdict) : NULL;

	int ownerIndex = PevInt(pev, PEV_OWNER_ENTINDEX);
	edict_t* pOwnerEdict = ownerIndex ? EnginePEntityOfEntIndex(ownerIndex) : NULL;
	entvars_t* pevOwner = pOwnerEdict ? EngineGetVarsOfEnt(pOwnerEdict) : NULL;

	const char* ownerClass = pevOwner ? EngineStringFromIndex(PevInt(pevOwner, PEV_CLASSNAME)) : NULL;

	if (ownerClass && strcmp(ownerClass, kPlayerClassname) == 0)
	{
		// Remember the entity we hit, then explode now.
		PevInt(pev, PEV_ENEMY) = pOtherEdict ? EngineIndexOfEdict(pOtherEdict) : 0;

		SetThink(&CGrenade::ExplodeThink);
		SetNextThink(0.0f);

		// Breakable / glass: forward our blocking damage straight to it.
		const char* otherClass = pevOther ? EngineStringFromIndex(PevInt(pevOther, PEV_CLASSNAME)) : NULL;
		if (otherClass && (strcmp(otherClass, kFuncBreakable) == 0 || strcmp(otherClass, kFuncGlass) == 0))
		{
			CBaseEntity* pHit = (CBaseEntity*)EngineGetPrivateData(pOtherEdict);
			if (pHit)
				pHit->TakeDamage(pev, pevOwner, PevFloat(pev, PEV_DMG));
		}
		return;
	}

	// Non-player grenade hitting the world: bounce.
	PevFloat(pev, PEV_MOVETYPE) = 10.0f;	// MOVETYPE_BOUNCE

	PevVector(pev, PEV_AVELOCITY).x = 300.0f;
	PevVector(pev, PEV_AVELOCITY).y = 300.0f;
	PevVector(pev, PEV_AVELOCITY).z = 300.0f;

	PevFloat(pev, PEV_GRAVITY) = 1.0f;

	// Only play a bounce sound when we did not hit our owner.
	if (otherIndex != PevInt(pev, PEV_OWNER_ENTINDEX))
	{
		edict_t* edict = EdictFromEntvars(pev);
		if (RandomFloat(0.0f, 1.0f) < 0.7f)
		{
			float flPick = RandomFloat(0.0f, 1.0f);
			const char* sound;
			if (flPick <= 0.33f)
				sound = kBounce1;
			else if (flPick <= 0.66f)
				sound = kBounce2;
			else
				sound = kBounce3;

			if (edict)
				EngineEmitSound(edict, 2, sound, 1.0f, 0.8f);
		}

		Vector& vecVel = PevVector(pev, PEV_VELOCITY);
		vecVel.x = vecVel.x * 0.8f;
		vecVel.y = vecVel.y * 0.8f;
		vecVel.z = vecVel.z * 0.8f;
	}
}

//=========================================================
// ExplodeThink
//=========================================================
void CGrenade::ExplodeThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);

	PevInt(pev, 128) = 0;
	PevFloat(pev, PEV_SOLID) = 0.0f;

	EngineWriteByte(0, SVC_TEMPENTITY);
	EngineWriteByte(0, TE_EXPLOSION);
	EngineWriteCoord(0, PevVector(pev, PEV_ORIGIN).x);
	EngineWriteCoord(0, PevVector(pev, PEV_ORIGIN).y);
	EngineWriteCoord(0, PevVector(pev, PEV_ORIGIN).z);

	EngineWriteByte(0, SVC_TEMPENTITY);
	EngineWriteByte(0, TE_ALPHABREAKMODEL);
	EngineWriteCoord(0, PevVector(pev, PEV_ORIGIN).x);
	EngineWriteCoord(0, PevVector(pev, PEV_ORIGIN).y);
	EngineWriteCoord(0, PevVector(pev, PEV_ORIGIN).z);
	EngineWriteCoord(0, 400.0f);
	EngineWriteShort(0, g_sModelIndexShrapnel);
	EngineWriteShort(0, 30);
	EngineWriteByte(0, 15);

	{
		int ownerIndex = PevInt(pev, PEV_OWNER_ENTINDEX);
		edict_t* pOwnerEdict = ownerIndex ? EnginePEntityOfEntIndex(ownerIndex) : NULL;
		entvars_t* pevOwner = pOwnerEdict ? EngineGetVarsOfEnt(pOwnerEdict) : NULL;
		RadiusDamage(pev, pevOwner, PevFloat(pev, PEV_DMG), 0);
	}

	{
		float start[3];
		float end[3];

		float vel[3];
		vel[0] = PevVector(pev, PEV_VELOCITY).x;
		vel[1] = PevVector(pev, PEV_VELOCITY).y;
		vel[2] = PevVector(pev, PEV_VELOCITY).z;

		if ((vel[0] != 0.0f) || (vel[1] != 0.0f) || (vel[2] != 0.0f))
		{
			float dir[3];
			VecCopy(dir, vel);
			if (!VecNormalize(dir))
				VecSet(dir, 0.0f, 0.0f, 0.0f);

			start[0] = PevVector(pev, PEV_ORIGIN).x - dir[0] * 32.0f;
			start[1] = PevVector(pev, PEV_ORIGIN).y - dir[1] * 32.0f;
			start[2] = PevVector(pev, PEV_ORIGIN).z - dir[2] * 32.0f;

			end[0] = PevVector(pev, PEV_ORIGIN).x;
			end[1] = PevVector(pev, PEV_ORIGIN).y;
			end[2] = PevVector(pev, PEV_ORIGIN).z;
		}
		else
		{
			start[0] = PevVector(pev, PEV_ORIGIN).x;
			start[1] = PevVector(pev, PEV_ORIGIN).y;
			start[2] = PevVector(pev, PEV_ORIGIN).z + 8.0f;

			end[0] = start[0];
			end[1] = start[1];
			end[2] = start[2] - 24.0f;
		}

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine(start, end, 0, edict, &tr);

		EngineWriteByte(0, SVC_TEMPENTITY);
		EngineWriteByte(0, TE_DECAL);
		EngineWriteCoord(0, tr.vecEndPos[0]);
		EngineWriteCoord(0, tr.vecEndPos[1]);
		EngineWriteCoord(0, tr.vecEndPos[2]);
		EngineWriteShort(0, (short)EngineModelIndex(TraceHitIndex(&tr)));
		EngineWriteByte(0, (RandomFloat(0.0f, 1.0f) >= 0.5f) ? 13 : 12);
	}

	RandomFloat(0.0f, 1.0f);

	{
		int which = RandomLong(0, 2);
		const char* sound = kDebris1;
		if (which == 1)
			sound = kDebris2;
		else if (which == 2)
			sound = kDebris3;

		if (edict)
			EngineEmitSound(edict, 2, sound, 1.0f, 0.8f);
	}

	SetRemoveThink();
	SetNextThink(2.0f);
}

//=========================================================
// ShootTimedGrenade
//=========================================================
void ShootTimedGrenade(entvars_t* pevOwner, const float* start, const float* velocity)
{
	CGrenade* pGrenade = CreateGrenadeEntity();
	if (!pGrenade)
		return;

	pGrenade->Init(pevOwner, start, velocity);
}
