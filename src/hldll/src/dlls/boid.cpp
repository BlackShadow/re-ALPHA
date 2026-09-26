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
// Boid - flocking flyer. A lone boid idles until it sees
// the player close by, then forms a flock with the boids
// around it and leads them.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"
#include "monsters.h"

#define BOID_THINK_INTERVAL		0.1f
#define BOID_IDLE_INTERVAL		0.2f	// a lone boid looks for the player this often

#define AFLOCK_RECRUIT_RADIUS	128.0f	// the player must come this close to a lone boid
#define AFLOCK_GATHER_RADIUS	384.0f	// the new leader recruits the boids within this radius
#define AFLOCK_FLY_SPEED		125.0f	// the leader's cruising speed
#define AFLOCK_TURN_RATE		75.0f	// yaw speed while steering around an obstacle
#define AFLOCK_ACCELERATE		10.0f
#define AFLOCK_MAX_SPEED		250.0f
#define AFLOCK_CHECK_DIST		192.0f	// how far ahead the leader checks for obstacles
#define AFLOCK_PROBE_OFFSET		12.0f	// side offset of the two outer obstacle traces
#define AFLOCK_TOO_CLOSE		64.0f
#define AFLOCK_TOO_FAR			128.0f
#define AFLOCK_AVOID_RADIUS		64.0f	// boids closer than this are pushed apart

class CBoid : public CBaseMonster
{
public:
	void Spawn();
	void Death(int iDeathType);

	void SpawnCommonCode();

	void IdleThink(CBaseEntity *pOther);
	void FormFlock(CBaseEntity *pOther);
	void Start(CBaseEntity *pOther);
	void FlockLeaderThink(CBaseEntity *pOther);
	void FlockFollowerThink(CBaseEntity *pOther);

	void BoidAdvanceFrame();
	void SpreadFlock();
	void SpreadFlock2();
	BOOL FPathBlocked();

	void StartAnimating();

	BOOL		m_fInited;			// SpawnCommonCode has run
	BOOL		m_fActive;			// still free to join a flock
	BOOL		m_fLeader;
	BOOL		m_fTurning;			// the leader is steering around an obstacle
	BOOL		m_fPathBlocked;
	entvars_t	*m_pevLeader;		// flock leader of a follower
	float		m_flGoalSpeed;		// speed a follower accelerates to
};

//=========================================================
// SpawnCommonCode - shared by map placed boids and the
// boids of a monster_boid_flock
//=========================================================
void CBoid::SpawnCommonCode()
{
	m_fInited = TRUE;

	pev->classname = ALLOC_STRING("monster_boid");
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_AIM;
	pev->health = 20.0f;

	m_fPathBlocked = FALSE;
	m_fActive = TRUE;
	m_fLeader = FALSE;

	SET_MODEL(ENT(pev), "models/boid.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}

void CBoid::Spawn()
{
	PRECACHE_SOUND("boid/sonar.wav");
	PRECACHE_MODEL("models/boid.mdl");

	SpawnCommonCode();

	pev->frame = 0.0f;

	SetThink(&CBoid::IdleThink);
	pev->nextthink = gpGlobals->time + BOID_THINK_INTERVAL;
}

void CBoid::Death(int iDeathType)
{
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + BOID_THINK_INTERVAL;
}

//=========================================================
// BoidAdvanceFrame - flaps faster while speeding up and
// slower while slowing down
//=========================================================
void CBoid::BoidAdvanceFrame()
{
	float flFlapSpeed = (pev->speed - pev->armorvalue) * 0.1f;
	pev->armorvalue = pev->speed;	// speed of the last frame

	float flFrameRate = pev->framerate;

	if (flFrameRate + 0.2f < flFlapSpeed && flFrameRate < 2.0f)
	{
		pev->framerate = flFrameRate + 0.1f;
	}
	else if (flFrameRate > flFlapSpeed && flFrameRate > 0.2f)
	{
		pev->framerate = flFrameRate - 0.1f;
	}

	AdvanceAnimation(BOID_THINK_INTERVAL);
}

//=========================================================
// IdleThink - a lone boid waits for the player to come
// close, then starts a flock with itself as the leader
//=========================================================
void CBoid::IdleThink(CBaseEntity *pOther)
{
	if (m_fInited)
		BoidAdvanceFrame();

	edict_t *pentPlayer = FIND_CLIENT_IN_PVS();
	if (!FNullEnt(pentPlayer))
	{
		entvars_t *pevPlayer = VARS(pentPlayer);
		if (FVisible(pev, pevPlayer))
		{
			edict_t *pentEntity = FIND_ENTITY_IN_SPHERE(pev->origin, AFLOCK_RECRUIT_RADIUS);
			while (!FNullEnt(pentEntity))
			{
				if (pentEntity == pentPlayer && m_fActive)
				{
					m_fLeader = TRUE;
					SetThink(&CBoid::FormFlock);
				}

				entvars_t *pevEntity = VARS(pentEntity);
				pentEntity = pevEntity->chain ? ENT(pevEntity->chain) : NULL;
			}
		}
	}

	pev->nextthink = gpGlobals->time + BOID_IDLE_INTERVAL;
}

//=========================================================
// FormFlock - the leader recruits the free boids around it
// and makes them take off shortly after itself
//=========================================================
void CBoid::FormFlock(CBaseEntity *pOther)
{
	float flTime = gpGlobals->time;

	edict_t *pentSelf = ENT(pev);

	edict_t *pentEntity = FIND_ENTITY_IN_SPHERE(pev->origin, AFLOCK_GATHER_RADIUS);
	while (!FNullEnt(pentEntity))
	{
		entvars_t *pevEntity = VARS(pentEntity);

		if (FClassnameIs(pevEntity, "monster_boid") && pentEntity != pentSelf)
		{
			CBoid *pBoid = (CBoid *)GET_PRIVATE(pentEntity);
			if (pBoid && pBoid->m_fActive)
			{
				pBoid->m_pevLeader = pev;
				pBoid->m_fActive = FALSE;
				pBoid->SetThink(&CBoid::Start);

				float flDelay = RANDOM_FLOAT(0.0f, 1.0f);
				pBoid->pev->nextthink = flTime + flDelay + 1.0f;
			}
		}

		pentEntity = pevEntity->chain ? ENT(pevEntity->chain) : NULL;
	}

	SetThink(&CBoid::Start);
	pev->nextthink = flTime;
}

//=========================================================
// Start - takes off with a random velocity and flies with
// the flock from now on
//=========================================================
void CBoid::Start(CBaseEntity *pOther)
{
	pev->movetype = MOVETYPE_FLY;

	// lift off the ground
	UTIL_SetOrigin(pev, pev->origin + Vector(0.0f, 0.0f, 1.0f));

	pev->flags -= FL_ONGROUND;	// subtracted, the flag is expected to be set

	// z is drawn first
	float flZ = RANDOM_FLOAT(0.0f, 100.0f) + 50.0f;
	float flX = 20.0f - RANDOM_FLOAT(0.0f, 40.0f);
	float flY = 20.0f - RANDOM_FLOAT(0.0f, 40.0f);
	pev->velocity = Vector(flX, flY, flZ);

	if (m_fLeader)
		SetThink(&CBoid::FlockLeaderThink);
	else
		SetThink(&CBoid::FlockFollowerThink);

	pev->nextthink = gpGlobals->time + 1.0f;

	pev->speed = pev->velocity.Length();

	pev->sequence = 0;
	ResetSequenceInfo(BOID_THINK_INTERVAL);

	BoidAdvanceFrame();
}

//=========================================================
// FPathBlocked - checks straight ahead and just to either
// side for anything in the way
//=========================================================
BOOL CBoid::FPathBlocked()
{
	BOOL fBlocked = FALSE;

	const Vector &vecOrigin = pev->origin;
	Vector vecAhead = gpGlobals->v_forward * AFLOCK_CHECK_DIST;
	Vector vecSide = gpGlobals->v_right * AFLOCK_PROBE_OFFSET;

	TraceResult tr;

	// straight ahead
	memset(&tr, 0, sizeof(tr));
	UTIL_TraceLine(vecOrigin, vecOrigin + vecAhead, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0f)
		fBlocked = TRUE;

	// ahead, on the right
	Vector vecStart = vecOrigin + vecSide;
	memset(&tr, 0, sizeof(tr));
	UTIL_TraceLine(vecStart, vecStart + vecAhead, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0f)
		fBlocked = TRUE;

	// ahead, on the left
	vecStart = vecOrigin - vecSide;
	memset(&tr, 0, sizeof(tr));
	UTIL_TraceLine(vecStart, vecStart + vecAhead, ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction != 1.0f)
		fBlocked = TRUE;

	return fBlocked;
}

//=========================================================
// SpreadFlock - turns the boids close to the leader away
// from it, keeping their speed
//=========================================================
void CBoid::SpreadFlock()
{
	edict_t *pentEntity = FIND_ENTITY_IN_SPHERE(pev->origin, AFLOCK_AVOID_RADIUS);
	while (!FNullEnt(pentEntity))
	{
		entvars_t *pevEntity = VARS(pentEntity);

		if (FClassnameIs(pevEntity, "monster_boid"))
		{
			Vector vecAway = (pevEntity->origin - pev->origin).Normalize();

			// average the course with the push, keeping the speed
			float flSpeed = pevEntity->velocity.Length();
			Vector vecFlight = (pevEntity->velocity.Normalize() + vecAway) * 0.5f;
			pevEntity->velocity = vecFlight * flSpeed;
		}

		pentEntity = pevEntity->chain ? ENT(pevEntity->chain) : NULL;
	}
}

//=========================================================
// SpreadFlock2 - turns a follower away from the boids close
// to it
//=========================================================
void CBoid::SpreadFlock2()
{
	edict_t *pentEntity = FIND_ENTITY_IN_SPHERE(pev->origin, AFLOCK_AVOID_RADIUS);
	while (!FNullEnt(pentEntity))
	{
		entvars_t *pevEntity = VARS(pentEntity);

		if (FClassnameIs(pevEntity, "monster_boid"))
		{
			Vector vecAway = (pev->origin - pevEntity->origin).Normalize();
			pev->velocity += vecAway;
		}

		pentEntity = pevEntity->chain ? ENT(pevEntity->chain) : NULL;
	}
}

//=========================================================
// FlockLeaderThink - the leader pings its sonar now and
// then, steers around obstacles and keeps the flock apart
//=========================================================
void CBoid::FlockLeaderThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + BOID_THINK_INTERVAL;

	// about once in a hundred thinks
	if (RANDOM_FLOAT(0.0f, 100.0f) <= 1.0f)
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "boid/sonar.wav", VOL_NORM, ATTN_NORM);

	if (!m_fLeader)
		return;

	UTIL_MakeVectors(pev->angles);

	if (FPathBlocked())
	{
		m_fPathBlocked = TRUE;

		if (!m_fTurning)
		{
			// turn to the side with more room
			const Vector &vecOrigin = pev->origin;
			Vector vecRight = gpGlobals->v_right * AFLOCK_CHECK_DIST;

			TraceResult trRight;
			memset(&trRight, 0, sizeof(trRight));
			UTIL_TraceLine(vecOrigin, vecOrigin + vecRight, ignore_monsters, ENT(pev), &trRight);
			float flRight = (trRight.vecEndPos - vecOrigin).Length();

			TraceResult trLeft;
			memset(&trLeft, 0, sizeof(trLeft));
			UTIL_TraceLine(vecOrigin, vecOrigin - vecRight, ignore_monsters, ENT(pev), &trLeft);
			float flLeft = (trLeft.vecEndPos - vecOrigin).Length();

			if (flLeft >= flRight)
				pev->avelocity.y = AFLOCK_TURN_RATE;
			else
				pev->avelocity.y = -AFLOCK_TURN_RATE;

			m_fTurning = TRUE;
		}

		SpreadFlock();

		pev->velocity = gpGlobals->v_forward * pev->speed;

		// don't dive into a floor right below
		const Vector &vecOrigin = pev->origin;
		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		UTIL_TraceLine(vecOrigin, vecOrigin - gpGlobals->v_up * 16.0f, ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0f && pev->velocity.z < 0.0f)
			pev->velocity.z = 0.0f;

		// on the ground: hop up and level off
		if ((int)pev->flags & FL_ONGROUND)
		{
			UTIL_SetOrigin(pev, pev->origin + Vector(0.0f, 0.0f, 1.0f));
			pev->velocity.z = 0.0f;
		}

		BoidAdvanceFrame();
	}
	else
	{
		if (m_fTurning)
		{
			m_fTurning = FALSE;
			pev->avelocity.y = 0.0f;
		}

		m_fPathBlocked = FALSE;

		// below cruising speed, accelerate
		if (pev->speed <= AFLOCK_FLY_SPEED)
			pev->speed += 5.0f;

		pev->velocity = gpGlobals->v_forward * pev->speed;

		BoidAdvanceFrame();
	}
}

//=========================================================
// FlockFollowerThink - follows the leader's heading and
// keeps up with it
//=========================================================
void CBoid::FlockFollowerThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + BOID_THINK_INTERVAL;

	entvars_t *pevLeader = m_pevLeader;
	if (!pevLeader)
		return;

	Vector vecDirToLeader = pevLeader->origin - pev->origin;

	// match heading with the leader
	pev->angles = pevLeader->angles;

	float flDistToLeader = vecDirToLeader.Length();
	float flLeaderSpeed = pevLeader->velocity.Length();

	if (!FInViewCone(pev, pevLeader, 0.1f))
	{
		// the leader isn't out in front, slow down to let it pass
		m_flGoalSpeed = flLeaderSpeed * 0.5f;
	}
	else if (flDistToLeader > AFLOCK_TOO_FAR)
	{
		m_flGoalSpeed = flLeaderSpeed * 1.5f;
	}
	else if (flDistToLeader < AFLOCK_TOO_CLOSE)
	{
		m_flGoalSpeed = flLeaderSpeed * 0.5f;
	}

	SpreadFlock2();

	// keep the speed, turn the velocity into the heading
	Vector &vecVelocity = pev->velocity;
	pev->speed = vecVelocity.Length();
	vecVelocity = vecVelocity.Normalize();

	// too far from the leader, average in a course towards it
	if (flDistToLeader > AFLOCK_TOO_FAR)
		vecVelocity = (vecVelocity + vecDirToLeader.Normalize()) * 0.5f;

	if (m_flGoalSpeed > AFLOCK_MAX_SPEED)
		m_flGoalSpeed = AFLOCK_MAX_SPEED;

	if (pev->speed < m_flGoalSpeed)
		pev->speed += AFLOCK_ACCELERATE;
	else if (pev->speed > m_flGoalSpeed)
		pev->speed -= AFLOCK_ACCELERATE;

	pev->velocity = pev->velocity * pev->speed;

	BoidAdvanceFrame();
}

void CBoid::StartAnimating()
{
	pev->nextthink = gpGlobals->time + BOID_THINK_INTERVAL;

	SetThink(&CBoid::IdleThink);
}

//=========================================================
// BoidFlockCreateMember - creates one boid of a
// monster_boid_flock
//=========================================================
edict_t *BoidFlockCreateMember(const Vector &vecOrigin, const Vector &vecAngles)
{
	CBoid *pBoid = GetClassPtr((CBoid *)NULL);

	pBoid->SpawnCommonCode();

	pBoid->pev->movetype = MOVETYPE_TOSS;
	pBoid->pev->angles = vecAngles;
	UTIL_SetOrigin(pBoid->pev, vecOrigin);
	pBoid->pev->frame = 0.0f;

	pBoid->StartAnimating();

	return ENT(pBoid->pev);
}

LINK_ENTITY_TO_CLASS(monster_boid, CBoid);
