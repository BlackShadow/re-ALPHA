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
// Combat - Bullet firing and multi-damage helpers
//=========================================================

#include <string.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "utils.h"

// Tracer cadence counter. In the original this lives outside the 28-byte
// CBaseEntity; a shared counter preserves the 1-in-N tracer visual.
static int g_iTracerCount = 0;

enum
{
	SVC_TEMPENTITY = 23,
	TE_GUNSHOT = 2,
	TE_TRACER = 6,
	TE_BLOODSTREAM = 101,
	TE_BLOOD = 103,
	TE_DECAL = 104,
};

static int g_multiDamageTarget = 0;
static float g_multiDamageAmount = 0.0f;

float g_vecAttackDir[3] = {0.0f, 0.0f, 0.0f};

//=========================================================
// multi-damage
//=========================================================

static void ClearMultiDamage()
{
	g_multiDamageTarget = 0;
	g_multiDamageAmount = 0.0f;
}

static void ApplyMultiDamage(entvars_t *pevInflictor)
{
	void *globals;
	edict_t *pHitEdict;
	entvars_t *pevHit;
	CBaseEntity *pHit;
	const char *pszClassname;

	if (!pevInflictor || !g_multiDamageTarget)
		return;

	globals = GlobalsFromEntvars(pevInflictor);

	pHitEdict = EnginePEntityOfEntIndex(g_multiDamageTarget);
	if (!pHitEdict)
		return;

	pevHit = EngineGetVarsOfEnt(pHitEdict);
	pHit = (CBaseEntity *)EngineGetPrivateData(pHitEdict);
	if (pHit)
		pHit->TakeDamage(pevInflictor, pevInflictor, g_multiDamageAmount);

	if (!pevHit)
		return;

	pszClassname = EngineStringFromIndex(PevInt(pevHit, PEV_CLASSNAME));
	if (pszClassname && strcmp(pszClassname, "cycler") == 0)
		return;

	if (RandomFloat(0.0f, 1.0f) >= 0.3f)
		return;

	{
		float delta[3];
		float vecSrc[3];
		float vecDir[3];
		float vecEnd[3];
		TraceResult tr;

		delta[0] = PevVector(pevInflictor, PEV_ORIGIN).x - PevVector(pevHit, PEV_ORIGIN).x;
		delta[1] = PevVector(pevInflictor, PEV_ORIGIN).y - PevVector(pevHit, PEV_ORIGIN).y;
		delta[2] = PevVector(pevInflictor, PEV_ORIGIN).z - PevVector(pevHit, PEV_ORIGIN).z;

		EngineMakeVectors(delta);

		vecSrc[0] = PevVector(pevHit, PEV_ORIGIN).x;
		vecSrc[1] = PevVector(pevHit, PEV_ORIGIN).y;
		vecSrc[2] = PevVector(pevHit, PEV_ORIGIN).z + PevVector(pevHit, PEV_SIZE).z * 0.5f;

		{
			// Original draws the RIGHT-scaling random first, then the UP-scaling one.
			const float *right = GlobalsRight(globals);
			const float *up = GlobalsUp(globals);
			float randRight = RandomFloat(-1.0f, 1.0f) * 0.45f;
			float randUp = RandomFloat(-1.0f, 1.0f) * 0.45f;

			vecDir[0] = -g_vecAttackDir[0] + (right ? right[0] : 0.0f) * randRight + (up ? up[0] : 0.0f) * randUp;
			vecDir[1] = -g_vecAttackDir[1] + (right ? right[1] : 0.0f) * randRight + (up ? up[1] : 0.0f) * randUp;
			vecDir[2] = -g_vecAttackDir[2] + (right ? right[2] : 0.0f) * randRight + (up ? up[2] : 0.0f) * randUp;
		}

		vecEnd[0] = vecSrc[0] + vecDir[0] * 128.0f;
		vecEnd[1] = vecSrc[1] + vecDir[1] * 128.0f;
		vecEnd[2] = vecSrc[2] + vecDir[2] * 128.0f;

		memset(&tr, 0, sizeof(tr));
		// The binary passes fNoMonsters arg7 = 0 to the trace wrapper
		// (EngineTraceLine): the blood-decal trace IGNORES monsters so the
		// decal lands on the wall behind the victim.
		EngineTraceLine(vecSrc, vecEnd, 0, pHitEdict, &tr);

		if (tr.flFraction != 1.0f)
		{
			int bloodColor = pHit ? pHit->BloodColor() : 0;

			EngineWriteByte(0, SVC_TEMPENTITY);
			EngineWriteByte(0, TE_DECAL);
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
}

static void AddMultiDamage(entvars_t *pevInflictor, int hitEntIndex, float flDamage)
{
	if (!hitEntIndex)
		return;

	if (hitEntIndex == g_multiDamageTarget)
	{
		g_multiDamageAmount += flDamage;
		return;
	}

	ApplyMultiDamage(pevInflictor);
	g_multiDamageTarget = hitEntIndex;
	g_multiDamageAmount = flDamage;
}

//=========================================================
// FireBullets
//=========================================================

static void BloodEffect(const float *vecOrigin, int bloodColor, float bloodAmount)
{
	if (bloodAmount > 255.0f)
		bloodAmount = 255.0f;

	EngineWriteByte(0, SVC_TEMPENTITY);
	EngineWriteByte(0, TE_BLOOD);
	EngineWriteCoord(0, vecOrigin[0]);
	EngineWriteCoord(0, vecOrigin[1]);
	EngineWriteCoord(0, vecOrigin[2]);
	EngineWriteCoord(0, g_vecAttackDir[0]);
	EngineWriteCoord(0, g_vecAttackDir[1]);
	EngineWriteCoord(0, g_vecAttackDir[2]);
	EngineWriteByte(0, (unsigned char)bloodColor);
	EngineWriteByte(0, (unsigned char)(int)bloodAmount);
}

static void ImpactEffects(entvars_t *pevInflictor, float flDamage, int fSkipEffects, const float *vecDir, TraceResult *ptr)
{
	int hitEntIndex;
	edict_t *pHitEdict;
	entvars_t *pevHit;

	if (!pevInflictor || !vecDir || !ptr)
		return;

	hitEntIndex = TraceHitIndex(ptr);
	// A world hit gives tr.pHit == 0 (prog offset of the world edict). The binary
	// resolves it straight through PROG_TO_EDICT (no null-guard), so resolve it here
	// too -- otherwise pevHit is NULL on every wall hit and the gunshot/decal block
	// below is skipped (no bullet marks on walls).
	pHitEdict = EnginePEntityOfEntIndex(hitEntIndex);
	pevHit = pHitEdict ? EngineGetVarsOfEnt(pHitEdict) : NULL;

	{
		float hitPos[3];
		hitPos[0] = ptr->vecEndPos[0] - vecDir[0] * 4.0f;
		hitPos[1] = ptr->vecEndPos[1] - vecDir[1] * 4.0f;
		hitPos[2] = ptr->vecEndPos[2] - vecDir[2] * 4.0f;

		if (pevHit && (PevInt(pevHit, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
		{
			const char *pszClassname;
			CBaseEntity *pEntity = (CBaseEntity *)EngineGetPrivateData(pHitEdict);
			int bloodColor = pEntity ? pEntity->BloodColor() : 0;

			AddMultiDamage(pevInflictor, hitEntIndex, flDamage);

			pszClassname = EngineStringFromIndex(PevInt(pevHit, PEV_CLASSNAME));
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

			if (PevFloat(pevHit, PEV_HEALTH) <= g_multiDamageAmount)
			{
				float org[3];
				org[0] = ptr->vecEndPos[0] + vecDir[0] * 8.0f;
				org[1] = ptr->vecEndPos[1] + vecDir[1] * 8.0f;
				org[2] = ptr->vecEndPos[2] + vecDir[2] * 8.0f;

				EngineWriteByte(0, SVC_TEMPENTITY);
				EngineWriteByte(0, TE_BLOODSTREAM);
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
			EngineWriteByte(0, SVC_TEMPENTITY);
			EngineWriteByte(0, TE_GUNSHOT);
			EngineWriteCoord(0, hitPos[0]);
			EngineWriteCoord(0, hitPos[1]);
			EngineWriteCoord(0, hitPos[2]);

			EngineWriteByte(0, SVC_TEMPENTITY);
			EngineWriteByte(0, TE_DECAL);
			EngineWriteCoord(0, ptr->vecEndPos[0]);
			EngineWriteCoord(0, ptr->vecEndPos[1]);
			EngineWriteCoord(0, ptr->vecEndPos[2]);
			EngineWriteShort(0, (short)EngineModelIndex(hitEntIndex));

			if (hitEntIndex && (PevInt(pevHit, PEV_RENDERMODE) & 0x7FFFFFFF) != 0)
				EngineWriteByte(0, RandomLong(26, 28));
			else
				EngineWriteByte(0, RandomLong(0, 4));
		}
	}
}

void CBaseEntity::FireBullets(int cShots, const float *vecDirShooting, float flSpreadUp, float flSpreadRight, int iBulletType, float flDistance)
{
	void *globals;
	edict_t *pEdict;
	const char *classname;
	const float *forward;
	const float *right;
	const float *up;
	float vecSrc[3];
	float tracerZ;
	int tracerMod;
	int iShot;
	int i;

	HL_UNUSED(iBulletType);

	if (!pev || !vecDirShooting)
		return;

	pEdict = EdictFromEntvars(pev);
	globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);

	EngineMakeVectors((const float *)((uint8_t *)pev + 352));

	forward = GlobalsForward(globals);
	right = GlobalsRight(globals);
	up = GlobalsUp(globals);

	vecSrc[0] = PevVector(pev, PEV_ORIGIN).x + (forward ? forward[0] : 0.0f) * 10.0f;
	vecSrc[1] = PevVector(pev, PEV_ORIGIN).y + (forward ? forward[1] : 0.0f) * 10.0f;

	tracerZ = PevVector(pev, PEV_ORIGIN).z + PevVector(pev, PEV_VIEWOFS).z - 4.0f;

	// The binary builds the trace-source x/y from origin+forward*10, then OVERWRITES
	// the z with the eye-height value (origin.z + viewofs.z - 4) at the binary BEFORE the
	// per-shot trace, so the bullet ray originates from eye height (not origin.z+forward.z*10).
	vecSrc[2] = tracerZ;

	tracerMod = 4;
	classname = EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME));
	if (classname && strcmp(classname, "player") == 0)
		tracerMod = 1;

	ClearMultiDamage();

	iShot = 0;	// sticky latch: set ONCE before the loop (binary local_e0/v46), never reset per-shot

	for (i = 0; i < cShots; i++)
	{
		// The original scales RIGHT by the first spread arg (flSpreadUp) and UP by the
		// second (flSpreadRight), drawing the RIGHT random first, then the UP random.
		float spreadRight = RandomFloat(-1.0f, 1.0f) * flSpreadUp;
		float spreadUp = RandomFloat(-1.0f, 1.0f) * flSpreadRight;

		float dir[3];
		dir[0] = vecDirShooting[0]
			+ (right ? right[0] : 0.0f) * spreadRight
			+ (up ? up[0] : 0.0f) * spreadUp;
		dir[1] = vecDirShooting[1]
			+ (right ? right[1] : 0.0f) * spreadRight
			+ (up ? up[1] : 0.0f) * spreadUp;
		dir[2] = vecDirShooting[2]
			+ (right ? right[2] : 0.0f) * spreadRight
			+ (up ? up[2] : 0.0f) * spreadUp;

		float vecEnd[3];
		vecEnd[0] = vecSrc[0] + dir[0] * flDistance;
		vecEnd[1] = vecSrc[1] + dir[1] * flDistance;
		vecEnd[2] = vecSrc[2] + dir[2] * flDistance;

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		// Binary FireBullets passes helper arg 1; EngineTraceLine applies the
		// alpha thunk inversion before calling the engine trace slot.
		EngineTraceLine(vecSrc, vecEnd, 1, pEdict, &tr);

		g_iTracerCount = (g_iTracerCount + 1) % tracerMod;
		if (g_iTracerCount == (tracerMod - 1))
		{
			if (PevInt(pev, PEV_WEAPON) == 4)
			{
				float trSrc[3];
				trSrc[0] = vecSrc[0];
				trSrc[1] = vecSrc[1];
				trSrc[2] = tracerZ;

				if (classname && strcmp(classname, "player") == 0)
				{
					// The binary derives this offset from
					// (globals+240)+0x18 = globals+264 = the RIGHT vector, not up.
					const float *globalForward = GlobalsForward(globals);
					const float *globalRight = GlobalsRight(globals);

					if ((((int)PevFloat(pev, PEV_FLAGS)) & 0x4000) != 0)
					{
						trSrc[0] = PevVector(pev, PEV_ORIGIN).x
							+ (globalForward ? globalForward[0] : 0.0f) * 16.0f
							+ (globalRight ? globalRight[0] : 0.0f) * 2.0f;
						trSrc[1] = PevVector(pev, PEV_ORIGIN).y
							+ (globalForward ? globalForward[1] : 0.0f) * 16.0f
							+ (globalRight ? globalRight[1] : 0.0f) * 2.0f;
						trSrc[2] = PevVector(pev, PEV_ORIGIN).z
							+ (globalForward ? globalForward[2] : 0.0f) * 16.0f
							+ (globalRight ? globalRight[2] : 0.0f) * 2.0f
							+ 6.0f;
					}
					else
					{
						trSrc[0] = PevVector(pev, PEV_ORIGIN).x
							+ (globalForward ? globalForward[0] : 0.0f) * 16.0f
							+ (globalRight ? globalRight[0] : 0.0f) * 2.0f;
						trSrc[1] = PevVector(pev, PEV_ORIGIN).y
							+ (globalForward ? globalForward[1] : 0.0f) * 16.0f
							+ (globalRight ? globalRight[1] : 0.0f) * 2.0f;
						trSrc[2] = PevVector(pev, PEV_ORIGIN).z
							+ (globalForward ? globalForward[2] : 0.0f) * 16.0f
							+ (globalRight ? globalRight[2] : 0.0f) * 2.0f
							+ 24.0f;
					}
				}

				if (tracerMod != 1)
					iShot = 1;

				EngineWriteByte(0, SVC_TEMPENTITY);
				EngineWriteByte(0, TE_TRACER);
				EngineWriteCoord(0, trSrc[0]);
				EngineWriteCoord(0, trSrc[1]);
				EngineWriteCoord(0, trSrc[2]);
				EngineWriteCoord(0, tr.vecEndPos[0]);
				EngineWriteCoord(0, tr.vecEndPos[1]);
				EngineWriteCoord(0, tr.vecEndPos[2]);
			}
		}

		if (tr.flFraction != 1.0f)
		{
			ImpactEffects(pev, 2.0f, iShot, dir, &tr);
		}
	}

	ApplyMultiDamage(pev);
}
