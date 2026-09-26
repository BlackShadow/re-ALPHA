/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Monsters - the shared monster AI
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

#define COVER_MAX_SCORE			2048.0f		// worst possible CoverScore

#define FRIENDLY_FIRE_TRACES	4			// shots tried by CheckFriendlyFire
#define FRIENDLY_FIRE_SPREAD	0.025f

//=========================================================
// DrawDebugLine
//=========================================================
void DrawDebugLine(const Vector &vecStart, const Vector &vecEnd)
{
	if (!g_fDrawLines)
		return;

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_SHOWLINE);
	WRITE_COORD(MSG_BROADCAST, vecStart.x);
	WRITE_COORD(MSG_BROADCAST, vecStart.y);
	WRITE_COORD(MSG_BROADCAST, vecStart.z);
	WRITE_COORD(MSG_BROADCAST, vecEnd.x);
	WRITE_COORD(MSG_BROADCAST, vecEnd.y);
	WRITE_COORD(MSG_BROADCAST, vecEnd.z);
}

//=========================================================
// FVisible - TRUE if the looker's eyes can see the
// target's eyes
//=========================================================
BOOL FVisible(entvars_t *pevLooker, entvars_t *pevTarget)
{
	if (!pevLooker || !pevTarget)
		return FALSE;

	TraceResult tr;
	Vector vecLookerOrigin = pevLooker->origin + pevLooker->view_ofs;
	Vector vecTargetOrigin = pevTarget->origin + pevTarget->view_ofs;

	UTIL_TraceLine(vecLookerOrigin, vecTargetOrigin, ignore_monsters, ENT(pevLooker), &tr);

	return tr.flFraction == 1.0f;
}

//=========================================================
// FInViewCone - TRUE if the target is within flDot of the
// looker's facing
//=========================================================
BOOL FInViewCone(entvars_t *pevLooker, entvars_t *pevTarget, float flDot)
{
	if (!pevLooker || !pevTarget)
		return FALSE;

	UTIL_MakeVectors(pevLooker->angles);

	Vector vecLOS = pevTarget->origin - pevLooker->origin;
	float flDist = vecLOS.Length();
	if (flDist <= 0.0f)
		return FALSE;

	vecLOS = vecLOS * (1.0f / flDist);

	return DotProduct(gpGlobals->v_forward, vecLOS) > flDot;
}

//=========================================================
// CheckFriendlyFire - TRUE if a shot along the direction
// might hit a monster of the shooter's own class
//=========================================================
BOOL CheckFriendlyFire(entvars_t *pevShooter, const Vector &vecDir, float flDistance)
{
	int iClass = CBaseEntity::Instance(pevShooter)->Classify();

	Vector vecSrc = pevShooter->origin + pevShooter->view_ofs;

	UTIL_MakeVectors(pevShooter->angles);

	for (int i = 0; i < FRIENDLY_FIRE_TRACES; i++)
	{
		float flRight = RANDOM_FLOAT(-1.0f, 1.0f) * FRIENDLY_FIRE_SPREAD;
		float flUp = RANDOM_FLOAT(-1.0f, 1.0f) * FRIENDLY_FIRE_SPREAD;
		Vector vecSpread = gpGlobals->v_right * flRight + gpGlobals->v_up * flUp;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSrc + (vecDir + vecSpread) * flDistance, dont_ignore_monsters, ENT(pevShooter), &tr);

		if (tr.flFraction != 1.0f && !FNullEnt(tr.pHit))
		{
			edict_t *pentHit = ENT(tr.pHit);

			if ((int)VARS(pentHit)->flags & FL_MONSTER)
			{
				CBaseEntity *pHit = CBaseEntity::Instance(pentHit);
				if (pHit && pHit->Classify() == iClass)
					return TRUE;
			}
		}
	}

	return FALSE;
}

//=========================================================
// CBaseMonster
//=========================================================
CBaseMonster::CBaseMonster()
{
	m_MonsterState = MONSTERSTATE_NONE;
	m_IdealMonsterState = MONSTERSTATE_NONE;
	m_flNextAttack = 0;
	m_bloodColor = 0;

	m_vecMoveGoal = g_vecZero;
	m_vecEnemyLKP = g_vecZero;
	for (int i = 0; i < MONSTER_ROUTE_SIZE; i++)
		m_vecRoute[i] = g_vecZero;

	m_pMoveTarget = NULL;
	m_pSquadLeader = NULL;
	m_pSquadNext = NULL;
	m_iSquadSize = 1;

	m_flGoalRadius = 0;
	m_flDistTooFar = 0;
	m_flNextSoundTime = 0;
	m_flLastEnemySightTime = 0;

	m_iAmmo = 0;
	m_afEnemyFlags = 0;
	m_iRouteIndex = 0;
	m_iRouteGoal = 0;

	m_pevAttacker = NULL;
	m_vecAttackerLKP = g_vecZero;
}

void CBaseMonster::SetActivity(int activity)
{
}

void CBaseMonster::IdleSound()
{
}

void CBaseMonster::AlertSound()
{
}

int CBaseMonster::CheckAttacks(entvars_t *pevEnemy, float flDist)
{
	return FALSE;
}

int CBaseMonster::BloodColor()
{
	return m_bloodColor;
}

//=========================================================
// TogglePlayerUse - the player's use key makes a monster
// follow the player, or stop following
//=========================================================
void CBaseMonster::TogglePlayerUse(entvars_t *pevPlayer)
{
	if (m_pMoveTarget == pevPlayer)
	{
		m_pMoveTarget = pev;
		m_MonsterState = MONSTERSTATE_IDLE;
	}
	else
	{
		m_pMoveTarget = pevPlayer;
		m_MonsterState = MONSTERSTATE_FOLLOW;
	}
}

//=========================================================
// CheckMeleeAttack
//=========================================================
BOOL CBaseMonster::CheckMeleeAttack(entvars_t *pevEnemy)
{
	if (!pevEnemy)
		return FALSE;

	if (gpGlobals->time < m_flNextAttack)
		return FALSE;

	if (!FInViewCone(pev, pevEnemy, 0.1f))
		return FALSE;

	if (!FVisible(pev, pevEnemy))
		return FALSE;

	// about the same height
	return fabs(pevEnemy->origin.z - pev->origin.z) <= 48.0f;
}

//=========================================================
// CheckRangeAttack
//=========================================================
BOOL CBaseMonster::CheckRangeAttack(entvars_t *pevEnemy)
{
	if (!pevEnemy)
		return FALSE;

	if (gpGlobals->time < m_flNextAttack)
		return FALSE;

	if (!FInViewCone(pev, pevEnemy, 0.9f))
		return FALSE;

	return FVisible(pev, pevEnemy);
}

//=========================================================
// SquadRecruit - builds a squad ring of all the living
// monsters of our class nearby, led by us. Returns the
// number of recruits.
//=========================================================
int CBaseMonster::SquadRecruit(int searchRadius)
{
	int iRecruits = 0;
	int iMyClass = Classify();
	CBaseMonster *pLast = this;

	m_pSquadLeader = pev;

	edict_t *pentEnt = FIND_ENTITY_IN_SPHERE(pev->origin, (float)searchRadius);
	while (!FNullEnt(pentEnt))
	{
		entvars_t *pevEnt = VARS(pentEnt);

		if ((int)pevEnt->flags & FL_MONSTER)
		{
			CBaseMonster *pRecruit = (CBaseMonster *)CBaseEntity::Instance(pentEnt);

			if (pRecruit && pRecruit->Classify() == iMyClass && pevEnt != pev && pevEnt->health > 0)
			{
				iRecruits++;
				pevEnt->enemy = pev->enemy;
				pRecruit->m_pSquadLeader = pev;

				pLast->m_pSquadNext = pevEnt;
				pLast = pRecruit;
			}
		}

		pentEnt = ENT(pevEnt->chain);
	}

	// close the ring
	pLast->m_pSquadNext = pev;

	return iRecruits;
}

//=========================================================
// TraceToFloor - where vecStart ends up when moved down
// by flDist
//=========================================================
static Vector TraceToFloor(entvars_t *pev, const Vector &vecStart, float flDist)
{
	TraceResult tr;
	Vector vecEnd = vecStart;
	vecEnd.z -= flDist;

	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev), &tr);

	return tr.vecEndPos;
}

//=========================================================
// CoverScore - how open vecCover is towards vecThreat: the
// free distance on both sides of the monster. Smaller is
// better cover.
//=========================================================
static float CoverScore(entvars_t *pev, const Vector &vecCover, const Vector &vecThreat)
{
	UTIL_MakeVectors(UTIL_VecToAngles(vecThreat - vecCover));

	float flWidth = pev->maxs.x;
	Vector vecRight = vecCover + gpGlobals->v_right * flWidth;
	Vector vecLeft = vecCover - gpGlobals->v_right * flWidth;

	TraceResult trRight;
	TraceResult trLeft;
	UTIL_TraceLine(vecRight, vecRight + gpGlobals->v_forward * 1024.0f, ignore_monsters, ENT(pev), &trRight);
	UTIL_TraceLine(vecLeft, vecLeft + gpGlobals->v_forward * 1024.0f, ignore_monsters, ENT(pev), &trLeft);

	return (trRight.vecEndPos - vecCover).Length() + (trLeft.vecEndPos - vecCover).Length();
}

//=========================================================
// FValidCover - the enemy can't see vecSpot, the monster
// can get there from vecStart, and there is room to stand
//=========================================================
static BOOL FValidCover(entvars_t *pev, const Vector &vecStart, const Vector &vecSpot, const Vector &vecRoom, const Vector &vecEnemyEye)
{
	TraceResult tr;

	UTIL_TraceLine(vecEnemyEye, vecSpot, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction == 1.0f)
		return FALSE;

	UTIL_TraceLine(vecStart, vecSpot, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0f)
		return FALSE;

	UTIL_TraceLine(vecSpot, vecSpot + vecRoom, dont_ignore_monsters, ENT(pev), &tr);

	return tr.flFraction == 1.0f;
}

//=========================================================
// FindCover - looks for cover from the enemy to the right
// and to the left, and sets the move state to go there.
// Returns pev->origin when there is none.
//=========================================================
Vector CBaseMonster::FindCover(entvars_t *pevEnemy)
{
	Vector vecBestRight = pev->origin;
	Vector vecBestLeft = vecBestRight;
	float flBestRightScore = COVER_MAX_SCORE;
	float flBestLeftScore = COVER_MAX_SCORE;

	if (!pevEnemy)
		return vecBestRight;

	Vector vecEnemyEye = pevEnemy->origin + pevEnemy->view_ofs;

	UTIL_MakeVectors(pev->angles);
	Vector vecRight = gpGlobals->v_right;

	Vector vecStart = pev->origin;
	vecStart.z += 16.0f;

	float flWidth = pev->size.y;

	for (int i = 1; i <= 13; i++)
	{
		Vector vecOffset = vecRight * (float)(i * 32);
		Vector vecSpot;
		float flScore;

		vecSpot = vecStart + vecOffset;
		if (FValidCover(pev, vecStart, vecSpot, vecRight * flWidth, vecEnemyEye))
		{
			flScore = CoverScore(pev, vecSpot, vecEnemyEye);
			if (flScore < flBestRightScore)
			{
				vecBestRight = vecSpot;
				flBestRightScore = flScore;
			}
		}

		vecSpot = vecStart - vecOffset;
		if (FValidCover(pev, vecStart, vecSpot, -(vecRight * flWidth), vecEnemyEye))
		{
			flScore = CoverScore(pev, vecSpot, vecEnemyEye);
			if (flScore < flBestLeftScore)
			{
				vecBestLeft = vecSpot;
				flBestLeftScore = flScore;
			}
		}
	}

	if (vecBestLeft != pev->origin)
		vecBestLeft = TraceToFloor(pev, vecBestLeft, 128.0f);

	if (vecBestRight != pev->origin)
		vecBestRight = TraceToFloor(pev, vecBestRight, 128.0f);

	DrawDebugLine(pev->origin, vecBestRight);
	DrawDebugLine(vecBestRight, pevEnemy->origin);
	DrawDebugLine(pev->origin, vecBestLeft);
	DrawDebugLine(vecBestLeft, pevEnemy->origin);

	vecBestLeft.z = pev->origin.z;
	vecBestRight.z = pev->origin.z;

	if (vecBestRight == pev->origin && vecBestLeft == pev->origin)
		return pev->origin;

	if (vecBestRight == pev->origin)
	{
		m_MonsterState = MONSTERSTATE_MOVE_LEFT;
		return vecBestLeft;
	}

	if (vecBestLeft == pev->origin)
	{
		m_MonsterState = MONSTERSTATE_MOVE_RIGHT;
		return vecBestRight;
	}

	// take the closer one
	if ((vecBestLeft - pev->origin).Length() > (vecBestRight - pev->origin).Length())
	{
		m_MonsterState = MONSTERSTATE_MOVE_RIGHT;
		return vecBestRight;
	}

	m_MonsterState = MONSTERSTATE_MOVE_LEFT;
	return vecBestLeft;
}

//=========================================================
// FindRetreat - looks for cover from the enemy ahead of
// the monster, as far away as the enemy is. Returns
// pev->origin when there is none.
//=========================================================
Vector CBaseMonster::FindRetreat(entvars_t *pevEnemy)
{
	Vector vecBest = pev->origin;
	Vector vecBestDebug = vecBest;
	float flBestScore = COVER_MAX_SCORE;

	if (!pevEnemy)
		return vecBest;

	Vector vecEnemyEye = pevEnemy->origin + pevEnemy->view_ofs;
	float flDist = (pevEnemy->origin - pev->origin).Length();

	int iDist = (int)flDist;
	int iStep = iDist / 8;
	if (iStep <= 0)
		iStep = 1;

	UTIL_MakeVectors(pev->angles);
	Vector vecForward = gpGlobals->v_forward;
	Vector vecRight = gpGlobals->v_right;

	Vector vecStart = pev->origin;
	vecStart.z += 16.0f;

	Vector vecEnemyTarget = vecEnemyEye + pevEnemy->view_ofs;

	for (int iOffset = -iDist; iOffset <= iDist; iOffset += iStep)
	{
		Vector vecSpot = vecStart + vecForward * flDist + vecRight * (float)iOffset;

		TraceResult tr;
		UTIL_TraceLine(vecStart, vecSpot, ignore_monsters, ENT(pev), &tr);

		// blocked, stay a bit away from the wall
		if (tr.flFraction != 1.0f)
			vecSpot = tr.vecEndPos - vecForward * (pev->size.y * 0.8f);

		UTIL_TraceLine(vecSpot + pevEnemy->view_ofs, vecEnemyTarget, ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0f)
		{
			float flScore = CoverScore(pev, vecSpot, vecEnemyEye);
			if (flScore < flBestScore)
			{
				flBestScore = flScore;
				vecBestDebug = vecSpot;
				vecBest = Vector(vecSpot.x, vecSpot.y, pev->origin.z);
			}
		}
	}

	DrawDebugLine(pev->origin, vecBestDebug);

	return vecBest;
}

//=========================================================
// FShootSpot - the enemy can be hit from vecSpot, and the
// monster can get there from vecStart
//=========================================================
static BOOL FShootSpot(entvars_t *pev, const Vector &vecSpot, const Vector &vecStart, EOFFSET eoffsetEnemy, const Vector &vecEnemyEye)
{
	TraceResult tr;

	UTIL_TraceLine(vecSpot, vecEnemyEye, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.pHit != eoffsetEnemy)
		return FALSE;

	UTIL_TraceLine(vecSpot, vecStart, dont_ignore_monsters, ENT(pev), &tr);

	return tr.flFraction == 1.0f;
}

//=========================================================
// FindShootPosition - looks to the right and to the left
// for a spot to shoot the enemy from, and sets the move
// state to go there. Returns pev->origin when there is none.
//=========================================================
Vector CBaseMonster::FindShootPosition(entvars_t *pevEnemy)
{
	if (!pevEnemy)
		return pev->origin;

	Vector vecEnemyEye = pevEnemy->origin + pevEnemy->view_ofs;

	UTIL_MakeVectors(pev->angles);

	// from the back of the monster
	Vector vecStart = pev->origin - gpGlobals->v_forward * (pev->size.x * 0.5f);
	vecStart.z += 16.0f;

	EOFFSET eoffsetEnemy = OFFSET(pevEnemy);

	for (int i = 1; i <= 6; i++)
	{
		Vector vecOffset = gpGlobals->v_right * (float)(i * 16);
		Vector vecSpot;

		vecSpot = vecStart + vecOffset;
		if (FShootSpot(pev, vecSpot, vecStart, eoffsetEnemy, vecEnemyEye))
		{
			m_MonsterState = MONSTERSTATE_MOVE_RIGHT;
			m_IdealMonsterState = MONSTERSTATE_COMBAT;
			return TraceToFloor(pev, vecSpot + gpGlobals->v_right * (pev->size.x * 0.75f), 128.0f);
		}

		vecSpot = vecStart - vecOffset;
		if (FShootSpot(pev, vecSpot, vecStart, eoffsetEnemy, vecEnemyEye))
		{
			m_MonsterState = MONSTERSTATE_MOVE_LEFT;
			m_IdealMonsterState = MONSTERSTATE_COMBAT;
			return TraceToFloor(pev, vecSpot - gpGlobals->v_right * (pev->size.x * 0.75f), 128.0f);
		}
	}

	m_MonsterState = m_IdealMonsterState;
	return pev->origin;
}

//=========================================================
// UpdateEnemyInfo - checks whether the enemy can be seen,
// updates its last known position and turns towards it.
// Returns the distance to the enemy.
//=========================================================
float CBaseMonster::UpdateEnemyInfo(entvars_t *pevEnemy)
{
	if (!pevEnemy)
		return 0;

	m_afEnemyFlags &= ~(ENEMY_IN_VIEWCONE | ENEMY_VISIBLE);

	if (FVisible(pev, pevEnemy))
		m_afEnemyFlags |= ENEMY_VISIBLE;

	if (FInViewCone(pev, pevEnemy, 0.1f))
		m_afEnemyFlags |= ENEMY_IN_VIEWCONE;

	float flDist = (pevEnemy->origin - pev->origin).Length();

	if ((m_afEnemyFlags & ENEMY_VISIBLE) && ((m_afEnemyFlags & ENEMY_IN_VIEWCONE) || flDist < 256.0f))
	{
		// seen, tell the squad leader
		m_iRouteGoal = 0;
		m_iRouteIndex = 0;
		m_afEnemyFlags |= ENEMY_SEEN;
		m_vecEnemyLKP = pevEnemy->origin;

		if (m_iSquadSize > 1 && m_pSquadLeader)
		{
			CBaseMonster *pLeader = (CBaseMonster *)Instance(m_pSquadLeader);
			if (pLeader)
				pLeader->m_vecEnemyLKP = m_vecEnemyLKP;
		}
	}
	else if (m_iSquadSize > 1 && m_pSquadLeader)
	{
		// ask the squad leader
		CBaseMonster *pLeader = (CBaseMonster *)Instance(m_pSquadLeader);
		if (pLeader)
			m_vecEnemyLKP = pLeader->m_vecEnemyLKP;
	}

	pev->ideal_yaw = UTIL_VecToYaw(m_vecEnemyLKP - pev->origin);

	return flDist;
}

//=========================================================
// WalkMonsterStart - puts the monster on the floor and
// starts its AI. A monster with a target walks to that
// path_corner.
//=========================================================
void CBaseMonster::WalkMonsterStart(CBaseEntity *pOther)
{
	if (gpGlobals->deathmatch != 0)
	{
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	pev->origin.z += 1.0f;
	DROP_TO_FLOOR(ENT(pev));

	if (WALK_MOVE(ENT(pev), 0.0f, 0.0f) == 0.0f)
	{
		ALERT(at_error, "Monster %s stuck in wall--level design error", STRING(pev->classname));
		pev->effects = EF_BRIGHTFIELD;
	}

	pev->takedamage = DAMAGE_AIM;
	pev->ideal_yaw = pev->angles.y;
	pev->view_ofs = Vector(0.0f, 0.0f, 64.0f);
	pev->flags = (int)pev->flags | FL_MONSTER;

	m_iRouteGoal = 0;
	m_iRouteIndex = 0;
	SetThink(&CBaseMonster::MonsterThink);
	m_MonsterState = MONSTERSTATE_IDLE;

	if (!FStringNull(pev->target))
	{
		const char *pszTarget = STRING(pev->target);
		edict_t *pentTarget = FIND_ENTITY_BY_STRING(NULL, "targetname", pszTarget);

		pev->goalentity = pentTarget ? OFFSET(pentTarget) : 0;

		if (pev->goalentity)
		{
			entvars_t *pevTarget = VARS(pentTarget);

			pev->ideal_yaw = UTIL_VecToYaw(pevTarget->origin - pev->origin);

			if (!FClassnameIs(pevTarget, "path_corner"))
				ALERT(at_warning, "WalkMonsterStart--monster's initial goal '%s' is not a path_corner", pszTarget);

			m_MonsterState = MONSTERSTATE_WALK;
		}
		else
		{
			ALERT(at_error, "WalkMonsterStart--%s couldn't find target %s", STRING(pev->classname), pszTarget);
		}
	}

	// spread the monsters' think times
	pev->nextthink += RANDOM_FLOAT(0.0f, 0.5f);
}

//=========================================================
// MonsterThink - runs the AI for the current monster state
//=========================================================
void CBaseMonster::MonsterThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + MONSTER_THINK_INTERVAL;

	Vector vecOldOrigin = pev->origin;

	SetActivity(m_MonsterState);
	AdvanceAnimation(MONSTER_THINK_INTERVAL);

	entvars_t *pevEnemy = NULL;
	float flDist = 0;

	if (pev->enemy && pev->health > 0)
	{
		pevEnemy = VARS(pev->enemy);

		// the enemy is dead
		if (pevEnemy->health <= 0)
		{
			pev->enemy = 0;
			m_MonsterState = MONSTERSTATE_IDLE;
			return;
		}

		flDist = UpdateEnemyInfo(pevEnemy);
	}

	// look for the player
	if (!pev->enemy && pev->health > 0)
	{
		edict_t *pentClient = FIND_CLIENT_IN_PVS();

		if (!FNullEnt(pentClient))
		{
			entvars_t *pevClient = VARS(pentClient);

			if (FInViewCone(pev, pevClient, 0.1f) && FVisible(pev, pevClient) && !((int)pevClient->flags & FL_NOTARGET))
			{
				if (Classify() != CLASS_PLAYER_ALLY)
				{
					if (!((int)pev->spawnflags & SF_MONSTER_WAIT_TILL_SEEN) || FInViewCone(pevClient, pev, 0.7f))
					{
						pev->enemy = OFFSET(pentClient);
						pev->goalentity = pev->enemy;
						m_vecEnemyLKP = pevClient->origin;
						m_flLastEnemySightTime = gpGlobals->time;
						AlertSound();
					}
				}
				return;
			}
		}
	}

	switch (m_MonsterState)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_IDLE2:
	case MONSTERSTATE_IDLE3:
		if (gpGlobals->time > m_flNextSoundTime && !((int)pev->flags & FL_SWIM))
			IdleSound();

		// pick the next idle animation
		if (m_fSequenceFinished)
		{
			int iIdle = rand() % 10;

			if (iIdle == 0)
				m_MonsterState = MONSTERSTATE_IDLE2;
			else if (iIdle == 1)
				m_MonsterState = MONSTERSTATE_IDLE3;
			else
				m_MonsterState = MONSTERSTATE_IDLE;
		}
		break;

	case MONSTERSTATE_WALK:
		if (!FNullEnt(pev->goalentity))
		{
			Vector vecGoal = VARS(pev->goalentity)->origin;
			MOVE_TO_ORIGIN(ENT(pev), vecGoal, m_flGroundSpeed, MOVE_CHASE);
		}

		if (gpGlobals->time > m_flNextSoundTime && !((int)pev->flags & FL_SWIM))
			IdleSound();
		break;

	case MONSTERSTATE_COMBAT_FACE:
		CHANGE_YAW(ENT(pev));
		CheckAttacks(pevEnemy, flDist);
		break;

	case MONSTERSTATE_COMBAT_IDLE:
		CHANGE_YAW(ENT(pev));

		if (!CheckAttacks(pevEnemy, flDist)
			&& (m_afEnemyFlags & (ENEMY_IN_VIEWCONE | ENEMY_VISIBLE)) == (ENEMY_IN_VIEWCONE | ENEMY_VISIBLE))
		{
			m_MonsterState = MONSTERSTATE_COMBAT;
		}
		break;

	case MONSTERSTATE_COMBAT:
		CHANGE_YAW(ENT(pev));

		if (!CheckAttacks(pevEnemy, flDist)
			&& m_flDistTooFar < flDist
			&& (m_afEnemyFlags & ENEMY_VISIBLE)
			&& WALK_MOVE(ENT(pev), pev->ideal_yaw, 15.0f) != 0.0f)
		{
			m_MonsterState = MONSTERSTATE_CHASE;
		}
		break;

	case MONSTERSTATE_CHASE:
		pev->ideal_yaw = UTIL_VecToYaw(m_vecEnemyLKP - pev->origin);
		MOVE_TO_ORIGIN(ENT(pev), m_vecEnemyLKP, m_flGroundSpeed, MOVE_CHASE);

		// stuck, or close enough
		if (pev->origin == vecOldOrigin)
			m_MonsterState = MONSTERSTATE_COMBAT;
		else if (m_flDistTooFar > flDist && (m_afEnemyFlags & ENEMY_VISIBLE))
			m_MonsterState = MONSTERSTATE_COMBAT;
		break;

	case MONSTERSTATE_HUNT:
		{
			PARTICLE_EFFECT(m_vecEnemyLKP, g_vecZero, 255.0f, 20.0f);

			float flGoalDist = (pev->origin - m_vecEnemyLKP).Length();
			pev->ideal_yaw = UTIL_VecToYaw(m_vecEnemyLKP - pev->origin);

			if (m_flGroundSpeed > flGoalDist)
			{
				MOVE_TO_ORIGIN(ENT(pev), m_vecEnemyLKP, flGoalDist, MOVE_CHASE);
				ALERT(at_console, "there!\n");

				if (m_afEnemyFlags & ENEMY_VISIBLE)
					m_MonsterState = MONSTERSTATE_CHASE;
				else
					m_MonsterState = MONSTERSTATE_IDLE;
			}
			else
			{
				MOVE_TO_ORIGIN(ENT(pev), m_vecRoute[m_iRouteIndex], m_flGroundSpeed, MOVE_CHASE);
			}
		}
		break;

	case MONSTERSTATE_FOLLOW:
		if (m_pMoveTarget)
		{
			UpdateEnemyInfo(m_pMoveTarget);

			if ((m_pMoveTarget->origin - pev->origin).Length() <= m_flGoalRadius)
			{
				CHANGE_YAW(ENT(pev));
			}
			else
			{
				Vector vecGoal = m_pMoveTarget->origin;
				MOVE_TO_ORIGIN(ENT(pev), vecGoal, m_flGroundSpeed, MOVE_CHASE);
			}
		}
		break;

	case MONSTERSTATE_MOVE_LEFT:
	case MONSTERSTATE_MOVE_RIGHT:
		{
			float flGoalDist = (pev->origin - m_vecMoveGoal).Length();

			if (m_flGroundSpeed > flGoalDist)
			{
				MOVE_TO_ORIGIN(ENT(pev), m_vecMoveGoal, flGoalDist, MOVE_STRAIGHT);
				m_MonsterState = m_IdealMonsterState;
			}
			else
			{
				MOVE_TO_ORIGIN(ENT(pev), m_vecMoveGoal, m_flGroundSpeed, MOVE_STRAIGHT);
			}

			if (pev->origin == vecOldOrigin)
			{
				ALERT(at_console, "Inhibited!\n");
				m_MonsterState = m_IdealMonsterState;
			}
		}
		break;

	case MONSTERSTATE_ALERT:
		m_MonsterState = MONSTERSTATE_ATTACK;
		CHANGE_YAW(ENT(pev));
		break;

	case MONSTERSTATE_WAIT:
		return;

	case MONSTERSTATE_RETREAT:
		{
			Vector vecToGoal = m_vecMoveGoal - pev->origin;

			if (vecToGoal.Length() > 20.0f)
			{
				pev->ideal_yaw = UTIL_VecToYaw(vecToGoal);
				MOVE_TO_ORIGIN(ENT(pev), m_vecMoveGoal, m_flGroundSpeed, MOVE_CHASE);
			}
			else
			{
				m_MonsterState = MONSTERSTATE_COMBAT_FACE;
			}
		}
		break;

	case MONSTERSTATE_FIND_COVER:
		m_vecMoveGoal = FindCover(pevEnemy);
		if (m_vecMoveGoal == pev->origin)
			m_MonsterState = m_IdealMonsterState;
		break;

	case MONSTERSTATE_FIND_RETREAT:
		m_vecMoveGoal = FindRetreat(pevEnemy);
		if (m_vecMoveGoal != pev->origin)
			m_MonsterState = MONSTERSTATE_RETREAT;
		else
			m_MonsterState = MONSTERSTATE_COMBAT;
		break;

	case MONSTERSTATE_FIND_SHOOT_POSITION:
		m_vecMoveGoal = FindShootPosition(pevEnemy);
		if (m_vecMoveGoal == pev->origin)
			m_MonsterState = MONSTERSTATE_COMBAT_IDLE;
		break;

	case MONSTERSTATE_MELEE_ATTACK:
	case MONSTERSTATE_RANGE_ATTACK:
		CHANGE_YAW(ENT(pev));
		break;

	case MONSTERSTATE_ATTACK:
		if (!CheckAttacks(pevEnemy, flDist))
			m_MonsterState = MONSTERSTATE_COMBAT_IDLE;
		break;

	case MONSTERSTATE_DIE1:
	case MONSTERSTATE_DIE2:
	case MONSTERSTATE_DIE3:
	case MONSTERSTATE_DIE4:
		CHANGE_YAW(ENT(pev));

		if (m_fSequenceFinished)
		{
			pev->framerate = 0;
			pev->solid = SOLID_NOT;
			m_MonsterState = MONSTERSTATE_DEAD;
			SetThink(&CBaseEntity::SUB_DoNothing);
		}
		break;

	case MONSTERSTATE_DEAD:
		SetThink(&CBaseEntity::SUB_DoNothing);
		break;

	default:
		ALERT(at_error, "Monster's state is bogus: %d", m_MonsterState);
		break;
	}
}
