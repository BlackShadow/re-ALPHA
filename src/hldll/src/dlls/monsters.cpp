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
// Monsters - Shared monster AI helpers
//=========================================================

#include <math.h>
#include <string.h>
#include "basemonster.h"
#include "monsters.h"
#include "enginefuncs.h"
#include "utils.h"

//=========================================================
// Helpers
//=========================================================

#define MONSTER_THINK_INTERVAL 0.1f
#define MSG_BROADCAST 0
#define SVC_TEMPENTITY 23
#define TE_SHOWLINE 102

void DrawDebugLine(const Vector &start, const Vector &end)
{
	if (!g_fDrawLines)
		return;

	EngineWriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
	EngineWriteByte(MSG_BROADCAST, TE_SHOWLINE);
	EngineWriteCoord(MSG_BROADCAST, start.x);
	EngineWriteCoord(MSG_BROADCAST, start.y);
	EngineWriteCoord(MSG_BROADCAST, start.z);
	EngineWriteCoord(MSG_BROADCAST, end.x);
	EngineWriteCoord(MSG_BROADCAST, end.y);
	EngineWriteCoord(MSG_BROADCAST, end.z);
}

static void VectorAdd(const Vector &a, const Vector &b, Vector &out)
{
	out.x = a.x + b.x;
	out.y = a.y + b.y;
	out.z = a.z + b.z;
}

static void VectorSubtract(const Vector &a, const Vector &b, Vector &out)
{
	out.x = a.x - b.x;
	out.y = a.y - b.y;
	out.z = a.z - b.z;
}

static float VectorLength(const Vector &v)
{
	return (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float VectorNormalize(Vector &v)
{
	float length = VectorLength(v);
	if (length <= 0.0f)
	{
		v.x = 0.0f;
		v.y = 0.0f;
		v.z = 0.0f;
		return 0.0f;
	}

	float inv = 1.0f / length;
	v.x = v.x * inv;
	v.y = v.y * inv;
	v.z = v.z * inv;
	return length;
}

static float DotProduct(const Vector &a, const Vector &b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

//=========================================================
// FVisible
//=========================================================
BOOL FVisible(entvars_t *pevLooker, entvars_t *pevTarget)
{
	if (!pevLooker || !pevTarget)
		return 0;

	Vector start;
	Vector end;
	VectorAdd(PevVector(pevLooker, PEV_ORIGIN), PevVector(pevLooker, PEV_VIEWOFS), start);
	VectorAdd(PevVector(pevTarget, PEV_ORIGIN), PevVector(pevTarget, PEV_VIEWOFS), end);

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine((const float *)&start, (const float *)&end, 0, EdictFromEntvars(pevLooker), &tr);
	return tr.flFraction == 1.0f;
}

//=========================================================
// FInViewCone
//=========================================================
BOOL FInViewCone(entvars_t *pevLooker, entvars_t *pevTarget, float flDot)
{
	if (!pevLooker || !pevTarget)
		return 0;

	Vector angles = PevVector(pevLooker, PEV_ANGLES);
	EngineMakeVectors((const float *)&angles);

	void *globals = GlobalsFromEntvars(pevLooker);
	const float *forward = GlobalsForward(globals);
	if (!forward)
		return 0;

	Vector dir;
	VectorSubtract(PevVector(pevTarget, PEV_ORIGIN), PevVector(pevLooker, PEV_ORIGIN), dir);
	if (VectorNormalize(dir) <= 0.0f)
		return 0;

	Vector fwd;
	fwd.x = forward[0];
	fwd.y = forward[1];
	fwd.z = forward[2];
	return DotProduct(fwd, dir) > flDot;
}

//=========================================================
// CheckFriendlyFire
//=========================================================
BOOL CheckFriendlyFire(entvars_t *pevShooter, float dirX, float dirY, float dirZ, float flDistance)
{
	if (!pevShooter)
		return 0;

	edict_t *pShooterEdict = EdictFromEntvars(pevShooter);
	if (!pShooterEdict)
		return 0;

	CBaseEntity *pShooter = (CBaseEntity *)EngineGetPrivateData(pShooterEdict);
	if (!pShooter)
		return 0;

	int shooterClass = pShooter->Classify();

	void *globals = GlobalsFromEntvars(pevShooter);
	if (!globals)
		return 0;

	float start[3];
	start[0] = PevVector(pevShooter, PEV_ORIGIN).x + PevVector(pevShooter, PEV_VIEWOFS).x;
	start[1] = PevVector(pevShooter, PEV_ORIGIN).y + PevVector(pevShooter, PEV_VIEWOFS).y;
	start[2] = PevVector(pevShooter, PEV_ORIGIN).z + PevVector(pevShooter, PEV_VIEWOFS).z;

	EngineMakeVectors((const float *)&PevVector(pevShooter, PEV_ANGLES));

	const float *right = GlobalsRight(globals);
	const float *up = GlobalsUp(globals);

	int i;
	for (i = 0; i < 4; ++i)
	{
		float jitter[3] = {0.0f, 0.0f, 0.0f};

		if (right)
		{
			float s = RandomFloat(-1.0f, 1.0f) * 0.025f;	// binary scales only by 0.025 (flDistance applied once at the endpoint)
			jitter[0] += right[0] * s;
			jitter[1] += right[1] * s;
			jitter[2] += right[2] * s;
		}

		if (up)
		{
			float s = RandomFloat(-1.0f, 1.0f) * 0.025f;	// binary scales only by 0.025 (flDistance applied once at the endpoint)
			jitter[0] += up[0] * s;
			jitter[1] += up[1] * s;
			jitter[2] += up[2] * s;
		}

		float end[3];
		end[0] = start[0] + (dirX + jitter[0]) * flDistance;
		end[1] = start[1] + (dirY + jitter[1]) * flDistance;
		end[2] = start[2] + (dirZ + jitter[2]) * flDistance;

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine(start, end, 1, pShooterEdict, &tr);

		if (tr.flFraction != 1.0f && TraceHitIndex(&tr))
		{
			edict_t *pHitEdict = TraceHitEdict(&tr);
			entvars_t *pevHit = pHitEdict ? EngineGetVarsOfEnt(pHitEdict) : NULL;
			if (pevHit && ((((int)PevFloat(pevHit, PEV_FLAGS)) & 0x20) != 0))
			{
				CBaseEntity *pHit = (CBaseEntity *)EngineGetPrivateData(pHitEdict);
				if (pHit && pHit->Classify() == shooterClass)
					return 1;
			}
		}
	}

	return 0;
}

//=========================================================
// CBaseMonster default virtuals
//=========================================================
void CBaseMonster::SetActivity(int activity)
{
	HL_UNUSED(activity);
}

void CBaseMonster::IdleSound()
{
}

void CBaseMonster::AlertSound()
{
}

int CBaseMonster::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	HL_UNUSED(pevEnemy);
	HL_UNUSED(flDist);
	return 0;
}

int CBaseMonster::BloodColor()
{
	return m_bloodColor;
}

//=========================================================
// CheckMeleeAttack
//=========================================================
BOOL CBaseMonster::CheckMeleeAttack(entvars_t *pevEnemy)
{
	if (!pev || !pevEnemy)
		return 0;

	if (GlobalTime() < m_flNextAttack)
		return 0;

	if (!FInViewCone(pev, pevEnemy, 0.1f))
		return 0;

	if (!FVisible(pev, pevEnemy))
		return 0;

	return (float)fabs(PevVector(pevEnemy, PEV_ORIGIN).z - PevVector(pev, PEV_ORIGIN).z) <= 48.0f;
}

//=========================================================
// CheckRangeAttack
//=========================================================
BOOL CBaseMonster::CheckRangeAttack(entvars_t *pevEnemy)
{
	if (!pev || !pevEnemy)
		return 0;

	if (GlobalTime() < m_flNextAttack)
		return 0;

	if (!FInViewCone(pev, pevEnemy, 0.9f))
		return 0;

	return FVisible(pev, pevEnemy);
}

//=========================================================
// SquadRecruit
//=========================================================
int CBaseMonster::SquadRecruit(int searchRadius)
{
	if (!pev)
		return 0;

	int count = 0;
	int myClass = Classify();

	m_pSquadLeader = pev;

	Vector origin = PevVector(pev, PEV_ORIGIN);
	edict_t *pEdict = EngineFindEntityInSphere((const float *)&origin, (float)searchRadius);

	entvars_t *pevLast = pev;

	while (pEdict)
	{
		if (!EngineIndexOfEdict(pEdict))
			break;

		entvars_t *pevEnt = EngineGetVarsOfEnt(pEdict);
		if (!pevEnt)
			break;

		if ((((int)PevFloat(pevEnt, PEV_FLAGS)) & 0x20) != 0)
		{
			CBaseEntity *pEntity = (CBaseEntity *)EngineGetPrivateData(pEdict);
			if (pEntity && pEntity->Classify() == myClass && pevEnt != pev && PevFloat(pevEnt, PEV_HEALTH) > 0.0f)
			{
				++count;
				PevInt(pevEnt, PEV_ENEMY) = PevInt(pev, PEV_ENEMY);

				CBaseMonster *pMonster = (CBaseMonster *)pEntity;
				if (pMonster)
					pMonster->m_pSquadLeader = pev;

				edict_t *pLastEdict = EdictFromEntvars(pevLast);
				CBaseMonster *pLast = pLastEdict ? (CBaseMonster *)EngineGetPrivateData(pLastEdict) : NULL;
				if (pLast)
					pLast->m_pSquadNext = pevEnt;

				pevLast = pevEnt;
			}
		}

		int chainIndex = PevInt(pevEnt, PEV_CHAIN);
		pEdict = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
	}

	{
		edict_t *pLastEdict = EdictFromEntvars(pevLast);
		CBaseMonster *pLast = pLastEdict ? (CBaseMonster *)EngineGetPrivateData(pLastEdict) : NULL;
		if (pLast)
			pLast->m_pSquadNext = pev;
	}

	return count;
}

//=========================================================
// Cover helpers
//=========================================================

static Vector TraceEndPosToVector(const TraceResult &tr)
{
	Vector out;
	out.x = tr.vecEndPos[0];
	out.y = tr.vecEndPos[1];
	out.z = tr.vecEndPos[2];
	return out;
}

static Vector TraceToFloor(entvars_t *pevOwner, const Vector &start, float distance)
{
	Vector end = start;
	end.z = end.z - distance;

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine((const float *)&start, (const float *)&end, 0, EdictFromEntvars(pevOwner), &tr);
	return TraceEndPosToVector(tr);
}

static float CoverScore(entvars_t *pevOwner, const Vector &vecCover, const Vector &vecThreat)
{
	if (!pevOwner)
		return 0.0f;

	Vector dir;
	VectorSubtract(vecThreat, vecCover, dir);

	float angles[3];
	EngineVecToAngles((const float *)&dir, angles);
	EngineMakeVectors(angles);

	void *globals = GlobalsFromEntvars(pevOwner);
	const float *forward = GlobalsForward(globals);
	const float *right = GlobalsRight(globals);
	if (!forward || !right)
		return 0.0f;

	Vector vForward;
	vForward.x = forward[0];
	vForward.y = forward[1];
	vForward.z = forward[2];

	Vector vRight;
	vRight.x = right[0];
	vRight.y = right[1];
	vRight.z = right[2];

	float hullWidth = PevVector(pevOwner, PEV_MAXS).x;

	Vector startA;
	startA.x = vecCover.x + vRight.x * hullWidth;
	startA.y = vecCover.y + vRight.y * hullWidth;
	startA.z = vecCover.z + vRight.z * hullWidth;

	Vector startB;
	startB.x = vecCover.x - vRight.x * hullWidth;
	startB.y = vecCover.y - vRight.y * hullWidth;
	startB.z = vecCover.z - vRight.z * hullWidth;

	Vector endA;
	endA.x = startA.x + vForward.x * 1024.0f;
	endA.y = startA.y + vForward.y * 1024.0f;
	endA.z = startA.z + vForward.z * 1024.0f;

	Vector endB;
	endB.x = startB.x + vForward.x * 1024.0f;
	endB.y = startB.y + vForward.y * 1024.0f;
	endB.z = startB.z + vForward.z * 1024.0f;

	TraceResult trA;
	memset(&trA, 0, sizeof(trA));
	EngineTraceLine((const float *)&startA, (const float *)&endA, 0, EdictFromEntvars(pevOwner), &trA);

	TraceResult trB;
	memset(&trB, 0, sizeof(trB));
	EngineTraceLine((const float *)&startB, (const float *)&endB, 0, EdictFromEntvars(pevOwner), &trB);

	Vector endPosA = TraceEndPosToVector(trA);
	Vector endPosB = TraceEndPosToVector(trB);

	Vector deltaA;
	VectorSubtract(endPosA, vecCover, deltaA);

	Vector deltaB;
	VectorSubtract(endPosB, vecCover, deltaB);

	return VectorLength(deltaA) + VectorLength(deltaB);
}

Vector CBaseMonster::FindCover(entvars_t *pevEnemy)
{
	Vector bestRight = PevVector(pev, PEV_ORIGIN);
	Vector bestLeft = bestRight;
	float bestRightScore = 2048.0f;
	float bestLeftScore = 2048.0f;

	if (!pev || !pevEnemy)
		return bestRight;

	Vector enemyEye;
	VectorAdd(PevVector(pevEnemy, PEV_ORIGIN), PevVector(pevEnemy, PEV_VIEWOFS), enemyEye);

	Vector angles = PevVector(pev, PEV_ANGLES);
	EngineMakeVectors((const float *)&angles);

	void *globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	const float *right = GlobalsRight(globals);
	if (!right)
		return bestRight;

	Vector rightDir;
	rightDir.x = right[0];
	rightDir.y = right[1];
	rightDir.z = right[2];

	Vector start = PevVector(pev, PEV_ORIGIN);
	start.z = start.z + 16.0f;

	const float sizeY = PevVector(pev, PEV_SIZE).y;

	int dist = 32;
	for (int i = 0; i < 13; i++)
	{
		Vector candidate;
		candidate.x = start.x + rightDir.x * (float)dist;
		candidate.y = start.y + rightDir.y * (float)dist;
		candidate.z = start.z + rightDir.z * (float)dist;

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine((const float *)&enemyEye, (const float *)&candidate, 0, EdictFromEntvars(pev), &tr);
		if (tr.flFraction != 1.0f)
		{
			TraceResult tr2;
			memset(&tr2, 0, sizeof(tr2));
			EngineTraceLine((const float *)&start, (const float *)&candidate, 1, EdictFromEntvars(pev), &tr2);
			if (tr2.flFraction == 1.0f)
			{
				Vector offset;
				offset.x = rightDir.x * sizeY;
				offset.y = rightDir.y * sizeY;
				offset.z = rightDir.z * sizeY;

				Vector candidate2;
				VectorAdd(candidate, offset, candidate2);

				TraceResult tr3;
				memset(&tr3, 0, sizeof(tr3));
				EngineTraceLine((const float *)&candidate, (const float *)&candidate2, 1, EdictFromEntvars(pev), &tr3);
				if (tr3.flFraction == 1.0f)
				{
					float score = CoverScore(pev, candidate, enemyEye);
					if (score < bestRightScore)
					{
						bestRight = candidate;
						bestRightScore = score;
					}
				}
			}
		}

		candidate.x = start.x - rightDir.x * (float)dist;
		candidate.y = start.y - rightDir.y * (float)dist;
		candidate.z = start.z - rightDir.z * (float)dist;

		memset(&tr, 0, sizeof(tr));
		EngineTraceLine((const float *)&enemyEye, (const float *)&candidate, 0, EdictFromEntvars(pev), &tr);
		if (tr.flFraction != 1.0f)
		{
			TraceResult tr2;
			memset(&tr2, 0, sizeof(tr2));
			EngineTraceLine((const float *)&start, (const float *)&candidate, 1, EdictFromEntvars(pev), &tr2);
			if (tr2.flFraction == 1.0f)
			{
				Vector offset;
				offset.x = rightDir.x * sizeY;
				offset.y = rightDir.y * sizeY;
				offset.z = rightDir.z * sizeY;

				Vector candidate2;
				VectorSubtract(candidate, offset, candidate2);

				TraceResult tr3;
				memset(&tr3, 0, sizeof(tr3));
				EngineTraceLine((const float *)&candidate, (const float *)&candidate2, 1, EdictFromEntvars(pev), &tr3);
				if (tr3.flFraction == 1.0f)
				{
					float score = CoverScore(pev, candidate, enemyEye);
					if (score < bestLeftScore)
					{
						bestLeft = candidate;
						bestLeftScore = score;
					}
				}
			}
		}

		dist += 32;
	}

	Vector origin = PevVector(pev, PEV_ORIGIN);

	if (bestLeft.x != origin.x || bestLeft.y != origin.y || bestLeft.z != origin.z)
		bestLeft = TraceToFloor(pev, bestLeft, 128.0f);

	if (bestRight.x != origin.x || bestRight.y != origin.y || bestRight.z != origin.z)
		bestRight = TraceToFloor(pev, bestRight, 128.0f);

	if (g_fDrawLines)
	{
		Vector enemyOrigin = PevVector(pevEnemy, PEV_ORIGIN);
		DrawDebugLine(origin, bestRight);
		DrawDebugLine(bestRight, enemyOrigin);
		DrawDebugLine(origin, bestLeft);
		DrawDebugLine(bestLeft, enemyOrigin);
	}

	// Binary forces the cover Z back to the monster's origin.z, discarding the
	// floor-traced z (FindRetreat does the same; returns origin.z).
	bestLeft.z = origin.z;
	bestRight.z = origin.z;

	if (bestRight.x == origin.x && bestRight.y == origin.y && bestRight.z == origin.z
		&& bestLeft.x == origin.x && bestLeft.y == origin.y && bestLeft.z == origin.z)
	{
		return origin;
	}

	if (bestRight.x == origin.x && bestRight.y == origin.y && bestRight.z == origin.z)
	{
		m_Activity = 11;
		return bestLeft;
	}

	if (bestLeft.x == origin.x && bestLeft.y == origin.y && bestLeft.z == origin.z)
	{
		m_Activity = 12;
		return bestRight;
	}

	Vector deltaRight;
	VectorSubtract(bestRight, origin, deltaRight);

	Vector deltaLeft;
	VectorSubtract(bestLeft, origin, deltaLeft);

	if (VectorLength(deltaLeft) > VectorLength(deltaRight))
	{
		m_Activity = 12;
		return bestRight;
	}

	m_Activity = 11;
	return bestLeft;
}

Vector CBaseMonster::FindRetreat(entvars_t *pevEnemy)
{
	Vector best = PevVector(pev, PEV_ORIGIN);
	Vector bestDebug = best;
	float bestScore = 2048.0f;

	if (!pev || !pevEnemy)
		return best;

	Vector enemyEye;
	VectorAdd(PevVector(pevEnemy, PEV_ORIGIN), PevVector(pevEnemy, PEV_VIEWOFS), enemyEye);

	Vector delta;
	VectorSubtract(PevVector(pevEnemy, PEV_ORIGIN), PevVector(pev, PEV_ORIGIN), delta);
	float distToEnemy = VectorLength(delta);

	int step = (int)distToEnemy / 8;
	if (step <= 0)
		step = 1;

	Vector angles = PevVector(pev, PEV_ANGLES);
	EngineMakeVectors((const float *)&angles);

	void *globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	const float *forward = GlobalsForward(globals);
	const float *right = GlobalsRight(globals);
	if (!forward || !right)
		return best;

	Vector forwardDir;
	forwardDir.x = forward[0];
	forwardDir.y = forward[1];
	forwardDir.z = forward[2];

	Vector rightDir;
	rightDir.x = right[0];
	rightDir.y = right[1];
	rightDir.z = right[2];

	Vector start = PevVector(pev, PEV_ORIGIN);
	start.z = start.z + 16.0f;

	Vector viewOfs = PevVector(pevEnemy, PEV_VIEWOFS);
	Vector enemyEye2;
	VectorAdd(enemyEye, viewOfs, enemyEye2);

	for (int offset = -(int)distToEnemy; offset <= (int)distToEnemy; offset += step)
	{
		Vector candidate;
		candidate.x = start.x + forwardDir.x * distToEnemy + rightDir.x * (float)offset;
		candidate.y = start.y + forwardDir.y * distToEnemy + rightDir.y * (float)offset;
		candidate.z = start.z + forwardDir.z * distToEnemy + rightDir.z * (float)offset;

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine((const float *)&start, (const float *)&candidate, 0, EdictFromEntvars(pev), &tr);
		if (tr.flFraction != 1.0f)
		{
			float back = -(PevVector(pev, PEV_SIZE).y * 0.8f);
			Vector hit = TraceEndPosToVector(tr);
			candidate.x = hit.x + forwardDir.x * back;
			candidate.y = hit.y + forwardDir.y * back;
			candidate.z = hit.z + forwardDir.z * back;
		}

		Vector candidate2;
		VectorAdd(candidate, viewOfs, candidate2);

		TraceResult tr2;
		memset(&tr2, 0, sizeof(tr2));
		EngineTraceLine((const float *)&candidate2, (const float *)&enemyEye2, 0, EdictFromEntvars(pev), &tr2);
		if (tr2.flFraction != 1.0f)
		{
			float score = CoverScore(pev, candidate, enemyEye);
			if (score < bestScore)
			{
				bestScore = score;
				bestDebug = candidate;
				best.x = candidate.x;
				best.y = candidate.y;
				best.z = PevVector(pev, PEV_ORIGIN).z;
			}
		}
	}

	DrawDebugLine(PevVector(pev, PEV_ORIGIN), bestDebug);
	return best;
}

Vector CBaseMonster::FindShootPosition(entvars_t *pevEnemy)
{
	Vector origin = PevVector(pev, PEV_ORIGIN);

	if (!pev || !pevEnemy)
		return origin;

	Vector enemyEye;
	VectorAdd(PevVector(pevEnemy, PEV_ORIGIN), PevVector(pevEnemy, PEV_VIEWOFS), enemyEye);

	Vector angles = PevVector(pev, PEV_ANGLES);
	EngineMakeVectors((const float *)&angles);

	void *globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	const float *forward = GlobalsForward(globals);
	const float *right = GlobalsRight(globals);
	if (!forward || !right)
		return origin;

	Vector forwardDir;
	forwardDir.x = forward[0];
	forwardDir.y = forward[1];
	forwardDir.z = forward[2];

	Vector rightDir;
	rightDir.x = right[0];
	rightDir.y = right[1];
	rightDir.z = right[2];

	float halfWidth = PevVector(pev, PEV_SIZE).x * 0.5f;

	Vector start;
	start.x = origin.x - forwardDir.x * halfWidth;
	start.y = origin.y - forwardDir.y * halfWidth;
	start.z = origin.z - forwardDir.z * halfWidth + 16.0f;

	int enemyIndex = EngineIndexOfEdict(EdictFromEntvars(pevEnemy));
	int dist = 16;
	Vector leftCandidate = origin;
	int foundLeft = 0;
	for (int i = 0; i < 6; i++)
	{
		Vector candidate;
		candidate.x = start.x + rightDir.x * (float)dist;
		candidate.y = start.y + rightDir.y * (float)dist;
		candidate.z = start.z + rightDir.z * (float)dist;

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine((const float *)&candidate, (const float *)&enemyEye, 1, EdictFromEntvars(pev), &tr);
		if (TraceHitIndex(&tr) == enemyIndex)
		{
			TraceResult tr2;
			memset(&tr2, 0, sizeof(tr2));
			EngineTraceLine((const float *)&candidate, (const float *)&start, 1, EdictFromEntvars(pev), &tr2);
			if (tr2.flFraction == 1.0f)
			{
				m_Activity = 12;
				m_IdealActivity = 7;

				float shift = PevVector(pev, PEV_SIZE).x * 0.75f;
				Vector shifted;
				shifted.x = candidate.x + rightDir.x * shift;
				shifted.y = candidate.y + rightDir.y * shift;
				shifted.z = candidate.z + rightDir.z * shift;
				return TraceToFloor(pev, shifted, 128.0f);
			}
		}

		candidate.x = start.x - rightDir.x * (float)dist;
		candidate.y = start.y - rightDir.y * (float)dist;
		candidate.z = start.z - rightDir.z * (float)dist;

		memset(&tr, 0, sizeof(tr));
		EngineTraceLine((const float *)&candidate, (const float *)&enemyEye, 1, EdictFromEntvars(pev), &tr);
		if (TraceHitIndex(&tr) == enemyIndex)
		{
			TraceResult tr2;
			memset(&tr2, 0, sizeof(tr2));
			EngineTraceLine((const float *)&candidate, (const float *)&start, 1, EdictFromEntvars(pev), &tr2);
			if (tr2.flFraction == 1.0f)
			{
				leftCandidate = candidate;
				foundLeft = 1;
				break;
			}
		}

		dist += 16;
	}

	if (!foundLeft)
	{
		m_Activity = m_IdealActivity;
		return origin;
	}

	m_Activity = 11;
	m_IdealActivity = 7;

	float shift = PevVector(pev, PEV_SIZE).x * 0.75f;
	Vector shifted;
	shifted.x = leftCandidate.x - rightDir.x * shift;
	shifted.y = leftCandidate.y - rightDir.y * shift;
	shifted.z = leftCandidate.z - rightDir.z * shift;

	return TraceToFloor(pev, shifted, 128.0f);
}

//=========================================================
// UpdateEnemyInfo
//=========================================================
float CBaseMonster::UpdateEnemyInfo(entvars_t *pevEnemy)
{
	if (!pev || !pevEnemy)
		return 0.0f;

	m_afEnemyFlags = (unsigned char)(m_afEnemyFlags & ~3);

	if (FVisible(pev, pevEnemy))
		m_afEnemyFlags |= 2;

	if (FInViewCone(pev, pevEnemy, 0.1f))
		m_afEnemyFlags |= 1;

	Vector delta;
	VectorSubtract(PevVector(pevEnemy, PEV_ORIGIN), PevVector(pev, PEV_ORIGIN), delta);
	float dist = VectorLength(delta);

	if (((m_afEnemyFlags & 2) && (m_afEnemyFlags & 1)) || ((m_afEnemyFlags & 2) && dist < 256.0f))
	{
		m_iRouteGoal = 0;
		m_iRouteIndex = 0;
		m_afEnemyFlags |= 4;
		m_vecEnemyLKP = PevVector(pevEnemy, PEV_ORIGIN);

		if (m_iSquadSize > 1 && m_pSquadLeader)
		{
			CBaseMonster *pLeader = (CBaseMonster *)EngineGetPrivateData(EdictFromEntvars(m_pSquadLeader));
			if (pLeader)
				pLeader->m_vecEnemyLKP = m_vecEnemyLKP;
		}
	}
	else if (m_iSquadSize > 1 && m_pSquadLeader)
	{
		CBaseMonster *pLeader = (CBaseMonster *)EngineGetPrivateData(EdictFromEntvars(m_pSquadLeader));
		if (pLeader)
			m_vecEnemyLKP = pLeader->m_vecEnemyLKP;
	}

	VectorSubtract(m_vecEnemyLKP, PevVector(pev, PEV_ORIGIN), delta);
	PevFloat(pev, PEV_IDEAL_YAW) = EngineVecToYaw((const float *)&delta);

	return dist;
}

//=========================================================
// WalkMonsterStart
//=========================================================
void CBaseMonster::WalkMonsterStart(CBaseEntity* pOther)
{
	if (!pev)
		return;

	void *globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	if (globals)
	{
		if ((*(int *)((unsigned char *)globals + 144) & 0x7FFFFFFF) != 0)
		{
			EngineRemoveEntity(EdictFromEntvars(pev));
			return;
		}
	}

	PevVector(pev, PEV_ORIGIN).z = PevVector(pev, PEV_ORIGIN).z + 1.0f;
	EngineDropToFloor(EdictFromEntvars(pev));

	if (EngineWalkMove(EdictFromEntvars(pev), 0.0f, 0.0f) == 0.0f)
	{
		const char *pszClassname = EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME));
		EngineAlertMessage(3, "Monster %s stuck in wall--level design error", pszClassname);
		PevFloat(pev, PEV_STUCK) = 1.0f;
	}

	PevFloat(pev, PEV_TAKEDAMAGE) = 2.0f;
	PevFloat(pev, PEV_IDEAL_YAW) = PevVector(pev, PEV_ANGLES).y;
	PevVector(pev, PEV_VIEWOFS).x = 0.0f;
	PevVector(pev, PEV_VIEWOFS).y = 0.0f;
	PevVector(pev, PEV_VIEWOFS).z = 64.0f;
	// pev->flags (offset 380 / 0x17C) is a FLOAT-encoded int. Binary WalkMonsterStart
	// does flags = (float)((int)flags | 0x20):
	//   fld [pev+0x17C]; call __ftol; or eax,0x20; fild; fstp [pev+0x17C].
	// Writing via raw PevInt OR-s 0x20 into the float BIT PATTERN and stores it back
	// as an int, so FL_MONSTER (0x20) is never actually set in the decoded flags --
	// breaking engine monster movement/physics. The monster could then only idle.
	PevFloat(pev, PEV_FLAGS) = (float)(((int)PevFloat(pev, PEV_FLAGS)) | 0x20);

	m_iRouteGoal = 0;
	m_iRouteIndex = 0;
	SetThink(&CBaseMonster::MonsterThink);
	m_Activity = 1;

	int targetNameIndex = PevInt(pev, PEV_TARGET);
	if (targetNameIndex)
	{
		const char *targetName = EngineStringFromIndex(targetNameIndex);
		if (targetName)
		{
			edict_t *pTarget = EngineFindEntityByString(NULL, "targetname", targetName);
			PevInt(pev, PEV_GOALENTINDEX) = pTarget ? EngineIndexOfEdict(pTarget) : 0;
			if (PevInt(pev, PEV_GOALENTINDEX))
			{
				entvars_t *pevTarget = EngineGetVarsOfEnt(pTarget);
				if (pevTarget)
				{
					Vector delta;
					VectorSubtract(PevVector(pevTarget, PEV_ORIGIN), PevVector(pev, PEV_ORIGIN), delta);
					PevFloat(pev, PEV_IDEAL_YAW) = EngineVecToYaw((const float *)&delta);

					const char *pszClassname = EngineStringFromIndex(PevInt(pevTarget, PEV_CLASSNAME));
					if (!pszClassname || strcmp(pszClassname, "path_corner") != 0)
					{
						EngineAlertMessage(2, "WalkMonsterStart--monster's initial goal '%s' is not a path_corner", targetName);
					}

					m_Activity = 4;
				}
			}
			else
			{
				const char *pszClassname = EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME));
				EngineAlertMessage(3, "WalkMonsterStart--%s couldn't find target %s", pszClassname, targetName);
			}
		}
	}

	PevFloat(pev, PEV_NEXTTHINK) = RandomFloat(0.0f, 0.5f) + PevFloat(pev, PEV_NEXTTHINK);
}

//=========================================================
// MonsterThink
//=========================================================
void CBaseMonster::MonsterThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	SetNextThink(MONSTER_THINK_INTERVAL);

	Vector oldOrigin = PevVector(pev, PEV_ORIGIN);

	SetActivity(m_Activity);
	AdvanceAnimation(MONSTER_THINK_INTERVAL);

	entvars_t *pevEnemy = NULL;
	float flDist = 0.0f;

	int enemyIndex = PevInt(pev, PEV_ENEMY);
	if (enemyIndex && PevFloat(pev, PEV_HEALTH) > 0.0f)
	{
		edict_t *pEnemyEdict = EnginePEntityOfEntIndex(enemyIndex);
		pevEnemy = pEnemyEdict ? EngineGetVarsOfEnt(pEnemyEdict) : NULL;
		if (!pevEnemy || PevFloat(pevEnemy, PEV_HEALTH) <= 0.0f)
		{
			PevInt(pev, PEV_ENEMY) = 0;
			m_Activity = 1;
			return;
		}

		flDist = UpdateEnemyInfo(pevEnemy);
	}

	if (PevInt(pev, PEV_ENEMY) == 0 && PevFloat(pev, PEV_HEALTH) > 0.0f)
	{
		edict_t *pClient = EngineFindClientInPVS();
		if (pClient && EngineIndexOfEdict(pClient))
		{
			entvars_t *pevClient = EngineGetVarsOfEnt(pClient);
			if (pevClient
				&& FInViewCone(pev, pevClient, 0.1f)
				&& FVisible(pev, pevClient)
				&& (((int)PevFloat(pevClient, PEV_FLAGS)) & 0x80) == 0)
			{
				if (Classify() != 5)
				{
					if ((((int)PevFloat(pev, PEV_SPAWNFLAGS)) & 1) == 0 || FInViewCone(pevClient, pev, 0.7f))
					{
						int clientIndex = EngineIndexOfEdict(pClient);
						PevInt(pev, PEV_ENEMY) = clientIndex;
						PevInt(pev, PEV_GOALENTINDEX) = clientIndex;
						m_vecEnemyLKP = PevVector(pevClient, PEV_ORIGIN);
						m_flLastEnemySightTime = GlobalTime();
						AlertSound();
					}
				}
				return;
			}
		}
	}

	switch (m_Activity)
	{
	case 1:
	case 2:
	case 3:
		if (GlobalTime() > m_flNextSoundTime && (((int)PevFloat(pev, PEV_FLAGS)) & 2) == 0)
			IdleSound();

		if (m_fSequenceFinished)
		{
			int n = rand() % 10;
			if (n == 0)
				m_Activity = 2;
			else if (n == 1)
				m_Activity = 3;
			else
				m_Activity = 1;
		}
		break;

	case 4:
		{
			int goalIndex = PevInt(pev, PEV_GOALENTINDEX);
			entvars_t *pevGoal = NULL;
			if (goalIndex)
			{
				edict_t *pGoal = EnginePEntityOfEntIndex(goalIndex);
				pevGoal = pGoal ? EngineGetVarsOfEnt(pGoal) : NULL;
			}

			if (pevGoal)
			{
				Vector goal = PevVector(pevGoal, PEV_ORIGIN);
				EngineMoveToOrigin(EdictFromEntvars(pev), (const float *)&goal, m_flGroundSpeed, 1);
			}

			if (GlobalTime() > m_flNextSoundTime && (((int)PevFloat(pev, PEV_FLAGS)) & 2) == 0)
				IdleSound();
		}
		break;

	case 5:
		EngineChangeYaw(EdictFromEntvars(pev));
		CheckAttacks(pevEnemy, flDist);
		break;

	case 6:
		EngineChangeYaw(EdictFromEntvars(pev));
		if (!CheckAttacks(pevEnemy, flDist))
		{
			if ((m_afEnemyFlags & 3) == 3)
				m_Activity = 7;
		}
		break;

	case 7:
		EngineChangeYaw(EdictFromEntvars(pev));
		if (!CheckAttacks(pevEnemy, flDist)
			&& m_flDistTooFar < flDist
			&& (m_afEnemyFlags & 2) != 0
			&& EngineWalkMove(EdictFromEntvars(pev), PevFloat(pev, PEV_IDEAL_YAW), 15.0f) != 0.0f)
		{
			m_Activity = 8;
		}
		break;

	case 8:
		{
			Vector delta;
			VectorSubtract(m_vecEnemyLKP, PevVector(pev, PEV_ORIGIN), delta);
			PevFloat(pev, PEV_IDEAL_YAW) = EngineVecToYaw((const float *)&delta);

			Vector goal = m_vecEnemyLKP;
			EngineMoveToOrigin(EdictFromEntvars(pev), (const float *)&goal, m_flGroundSpeed, 1);

			Vector newOrigin = PevVector(pev, PEV_ORIGIN);
			if (newOrigin.x == oldOrigin.x && newOrigin.y == oldOrigin.y && newOrigin.z == oldOrigin.z)
			{
				m_Activity = 7;
			}
			else if (m_flDistTooFar > flDist && (m_afEnemyFlags & 2) != 0)
			{
				m_Activity = 7;
			}
		}
		break;

	case 9:
		{
			Vector dir = {0.0f, 0.0f, 0.0f};
			EngineParticleEffect((const float *)&m_vecEnemyLKP, (const float *)&dir, 255.0f, 20.0f);

			Vector delta;
			VectorSubtract(PevVector(pev, PEV_ORIGIN), m_vecEnemyLKP, delta);
			float dist = VectorLength(delta);

			Vector yawDelta;
			VectorSubtract(m_vecEnemyLKP, PevVector(pev, PEV_ORIGIN), yawDelta);
			PevFloat(pev, PEV_IDEAL_YAW) = EngineVecToYaw((const float *)&yawDelta);

			if (m_flGroundSpeed > dist)
			{
				EngineMoveToOrigin(EdictFromEntvars(pev), (const float *)&m_vecEnemyLKP, dist, 1);
				EngineAlertMessage(1, "there!\n");
				if ((m_afEnemyFlags & 2) != 0)
					m_Activity = 8;
				else
					m_Activity = 1;
			}
			else
			{
				EngineMoveToOrigin(EdictFromEntvars(pev), (const float *)&m_vecRoute[m_iRouteIndex], m_flGroundSpeed, 1);
			}
		}
		break;

	case 10:
		if (m_pMoveTarget)
		{
			UpdateEnemyInfo(m_pMoveTarget);

			Vector delta;
			VectorSubtract(PevVector(m_pMoveTarget, PEV_ORIGIN), PevVector(pev, PEV_ORIGIN), delta);
			if (VectorLength(delta) <= m_flGoalRadius)
			{
				EngineChangeYaw(EdictFromEntvars(pev));
			}
			else
			{
				Vector goal = PevVector(m_pMoveTarget, PEV_ORIGIN);
				EngineMoveToOrigin(EdictFromEntvars(pev), (const float *)&goal, m_flGroundSpeed, 1);
			}
		}
		break;

	case 11:
	case 12:
		{
			Vector delta;
			VectorSubtract(PevVector(pev, PEV_ORIGIN), m_vecMoveGoal, delta);
			float dist = VectorLength(delta);

			float moveDist = m_flGroundSpeed;
			if (moveDist > dist)
			{
				moveDist = dist;
				EngineMoveToOrigin(EdictFromEntvars(pev), (const float *)&m_vecMoveGoal, moveDist, 0);
				m_Activity = m_IdealActivity;
			}
			else
			{
				EngineMoveToOrigin(EdictFromEntvars(pev), (const float *)&m_vecMoveGoal, moveDist, 0);
			}

			Vector newOrigin = PevVector(pev, PEV_ORIGIN);
			if (newOrigin.x == oldOrigin.x && newOrigin.y == oldOrigin.y && newOrigin.z == oldOrigin.z)
			{
				EngineAlertMessage(1, "Inhibited!\n");
				m_Activity = m_IdealActivity;
			}
		}
		break;

	case 20:
		m_Activity = 31;
		EngineChangeYaw(EdictFromEntvars(pev));
		break;

	case 22:
		return;

	case 23:
		{
			Vector delta;
			VectorSubtract(m_vecMoveGoal, PevVector(pev, PEV_ORIGIN), delta);
			float dist = VectorLength(delta);
			if (dist > 20.0f)
			{
				PevFloat(pev, PEV_IDEAL_YAW) = EngineVecToYaw((const float *)&delta);
				EngineMoveToOrigin(EdictFromEntvars(pev), (const float *)&m_vecMoveGoal, m_flGroundSpeed, 1);
			}
			else
			{
				m_Activity = 5;
			}
		}
		break;

	case 25:
		{
			Vector goal = FindCover(pevEnemy);
			m_vecMoveGoal = goal;
			if (goal.x == PevVector(pev, PEV_ORIGIN).x
				&& goal.y == PevVector(pev, PEV_ORIGIN).y
				&& goal.z == PevVector(pev, PEV_ORIGIN).z)
			{
				m_Activity = m_IdealActivity;
			}
		}
		break;

	case 26:
		{
			Vector goal = FindRetreat(pevEnemy);
			m_vecMoveGoal = goal;
			if (goal.x != PevVector(pev, PEV_ORIGIN).x
				|| goal.y != PevVector(pev, PEV_ORIGIN).y
				|| goal.z != PevVector(pev, PEV_ORIGIN).z)
			{
				m_Activity = 23;
			}
			else
			{
				m_Activity = 7;
			}
		}
		break;

	case 27:
		{
			Vector goal = FindShootPosition(pevEnemy);
			m_vecMoveGoal = goal;
			if (goal.x == PevVector(pev, PEV_ORIGIN).x
				&& goal.y == PevVector(pev, PEV_ORIGIN).y
				&& goal.z == PevVector(pev, PEV_ORIGIN).z)
			{
				m_Activity = 6;
			}
		}
		break;

	case 29:
	case 30:
		EngineChangeYaw(EdictFromEntvars(pev));
		break;

	case 31:
		if (!CheckAttacks(pevEnemy, flDist))
			m_Activity = 6;
		break;

	case 35:
	case 36:
	case 37:
	case 38:
		EngineChangeYaw(EdictFromEntvars(pev));
		if (m_fSequenceFinished)
		{
			PevFloat(pev, PEV_FRAMERATE) = 0.0f;
			PevFloat(pev, PEV_SOLID) = 0.0f;
			m_Activity = 42;
			SetDoNothingThink();
		}
		break;

	case 42:
		SetDoNothingThink();
		break;

	default:
		EngineAlertMessage(3, "Monster's state is bogus: %d", m_Activity);
		break;
	}

}
