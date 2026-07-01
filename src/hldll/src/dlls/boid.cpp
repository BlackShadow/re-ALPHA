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
// Boid - Flying boid monster (flock member)
//=========================================================

#include <new>
#include <math.h>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"

//=========================================================
// monster-specific constants
//=========================================================

#define BOID_PRIVATE_DATA_SIZE	392

#define BOID_THINK_INTERVAL		0.1f
#define BOID_DELTA_TIME			0.2f

#define BOID_VOL				1.0f
#define BOID_ATTN_NORM			0.8f

#define AFLOCK_RECRUIT_RADIUS	128.0f
#define AFLOCK_GATHER_RADIUS	384.0f
#define AFLOCK_FLY_SPEED		125.0f
#define AFLOCK_TURN_RATE		75.0f
#define AFLOCK_ACCELERATE		10.0f
#define AFLOCK_MAX_SPEED		250.0f
#define AFLOCK_CHECK_DIST		192.0f
#define AFLOCK_PROBE_OFFSET		12.0f
#define AFLOCK_TOO_CLOSE		64.0f
#define AFLOCK_TOO_FAR			128.0f
#define AFLOCK_AVOID_RADIUS		64.0f

#define FL_ONGROUND				0x200

// pev byte offsets not yet named in utils.h
#define PEV_AVELOCITY_Y			92		// avelocity[1] (PEV_AVELOCITY + 4)
#define PEV_FLOCK_SPEED			496		// scratch: per-frame flight speed magnitude
#define PEV_FLOCK_PREV_SPEED	404		// scratch: previous frame's flight speed magnitude

static const char kBoidModel[] = "models/boid.mdl";
static const char kBoidClassname[] = "monster_boid";

//=========================================================
// Sound Table
//=========================================================

static const char* pSonarSounds[] =
{
	"boid/sonar.wav",
};

//=========================================================
// CBoid
//=========================================================

class CBoid : public CBaseMonster
{
public:
	void Spawn();
	void Death(int gibType);		// vtable slot 14
	void RemoveThink(CBaseEntity* pOther);

	void SetupBoid();

	void AdvanceFrameThink(CBaseEntity* pOther);
	void BecomeLeaderThink(CBaseEntity* pOther);
	void StartFlightThink(CBaseEntity* pOther);
	void FlockLeaderThink(CBaseEntity* pOther);
	void FlockFollowerThink(CBaseEntity* pOther);

	void AdvanceFrame();
	void SpreadFlock();
	void SpreadFlock2();
	BOOL FPathBlocked();

	void StartAnimating();

	// Boid-private scratch fields. In the binary these live at byte 336+ (after
	// the 336-byte CBaseMonster); the C++ CBaseMonster has a different internal
	// size, so these are accessed only through C++ member names and need not sit
	// at those byte offsets. They map 1:1 to the binary fields:
	//   336 m_fInited  340 m_fActive  344 m_fLeader  348 m_fTurning
	//   356 m_fPathBlocked  360 m_pSquadLeader  388 m_flGoalSpeed
	int m_fInited;			// boid has been var-initialised
	int m_fActive;			// boid is alive / available to be recruited
	int m_fLeader;			// this boid is the flock leader
	int m_fTurning;			// leader is currently steering around an obstacle
	int m_fPathBlocked;		// leader's path is blocked this frame
	entvars_t* m_pSquadLeader;	// follower's leader entvars
	float m_flGoalSpeed;	// follower's desired flight speed
};

HL_COMPILE_TIME_ASSERT(sizeof(CBoid) <= BOID_PRIVATE_DATA_SIZE, CBoid_private_data_size);

//=========================================================
// vector helpers (shared engine math routines)
//=========================================================

static void BoidVecScale(const float* in, float scale, float* out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

static void BoidVecAdd(const float* a, const float* b, float* out)
{
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

static void BoidVecSub(const float* a, const float* b, float* out)
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

static float BoidVecLength(const float* v)
{
	return (float)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static void BoidVecNormalize(const float* in, float* out)
{
	float length = (float)sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);

	if (length == 0.0f)
	{
		out[0] = 0.0f;
		out[1] = 0.0f;
		out[2] = 0.0f;
	}
	else
	{
		float inv = 1.0f / length;
		out[0] = in[0] * inv;
		out[1] = in[1] * inv;
		out[2] = in[2] * inv;
	}
}

//=========================================================
// helpers
//=========================================================

static const char* BoidClassname(entvars_t* pevEnt)
{
	return EngineStringFromIndex(PevInt(pevEnt, PEV_CLASSNAME));
}

//=========================================================
// SetupBoid
//=========================================================
void CBoid::SetupBoid()
{
	m_fInited = 1;

	PevInt(pev, PEV_CLASSNAME) = EngineAllocString(kBoidClassname);
	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 5.0f;
	PevFloat(pev, PEV_TAKEDAMAGE) = 2.0f;
	PevFloat(pev, PEV_HEALTH) = 20.0f;

	m_fPathBlocked = 0;
	m_fActive = 1;
	m_fLeader = 0;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kBoidModel);

	{
		float mins[3];
		float maxs[3];
		VecSet(mins, 0.0f, 0.0f, 0.0f);
		VecSet(maxs, 0.0f, 0.0f, 0.0f);
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}
}

//=========================================================
// Spawn (reconstructed from SetupBoid and the in-flock boid init in
// BoidFlockCreateMember)
//=========================================================
void CBoid::Spawn()
{
	EnginePrecacheSound(pSonarSounds[0]);
	EnginePrecacheModel(kBoidModel);

	SetupBoid();

	PevFloat(pev, PEV_FRAME) = 0.0f;

	SetThink(&CBoid::AdvanceFrameThink);
	SetNextThink(BOID_THINK_INTERVAL);
}

//=========================================================
// Death - vtable slot 14
// A dying boid drops out of the flock and removes itself
// next tick.
//=========================================================
void CBoid::Death(int gibType)
{
	HL_UNUSED(gibType);

	SetRemoveThink();
	SetNextThink(BOID_THINK_INTERVAL);
}

//=========================================================
// RemoveThink
//=========================================================
void CBoid::RemoveThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// AdvanceFrame
// Adjusts the playback frame rate towards the ground speed,
// then advances the animation.
//=========================================================
void CBoid::AdvanceFrame()
{
	float flGroundSpeed = (PevFloat(pev, PEV_FLOCK_SPEED) - PevFloat(pev, PEV_FLOCK_PREV_SPEED)) * 0.1f;
	PevFloat(pev, PEV_FLOCK_PREV_SPEED) = PevFloat(pev, PEV_FLOCK_SPEED);

	float flFrameRate = PevFloat(pev, PEV_FRAMERATE);

	if (flFrameRate + 0.2f < flGroundSpeed && flFrameRate < 2.0f)
	{
		PevFloat(pev, PEV_FRAMERATE) = flFrameRate + 0.1f;
	}
	else if (flFrameRate > flGroundSpeed && flFrameRate > 0.2f)
	{
		PevFloat(pev, PEV_FRAMERATE) = flFrameRate - 0.1f;
	}

	AdvanceAnimation(BOID_THINK_INTERVAL);
}

//=========================================================
// AdvanceFrameThink
// A lone boid keeps animating; if it is in the player's PVS
// and visible it promotes itself to flock leader.
//=========================================================
void CBoid::AdvanceFrameThink(CBaseEntity* pOther)
{
	if (m_fInited)
		AdvanceFrame();

	edict_t* pPlayer = EngineFindClientInPVS();
	if (pPlayer)
	{
		if (EngineIndexOfEdict(pPlayer))
		{
			entvars_t* pevPlayer = EngineGetVarsOfEnt(pPlayer);
			if (FVisible(pev, pevPlayer))
			{
				Vector vecSrc = PevVector(pev, PEV_ORIGIN);

				edict_t* pEntity = EngineFindEntityInSphere(VecPtr(vecSrc), AFLOCK_RECRUIT_RADIUS);
				while (pEntity)
				{
					if (!EngineIndexOfEdict(pEntity))
						break;

					if (pEntity == pPlayer && m_fActive)
					{
						m_fLeader = 1;
						SetThink(&CBoid::BecomeLeaderThink);
					}

					entvars_t* pevEntity = EngineGetVarsOfEnt(pEntity);
					int chainIndex = PevInt(pevEntity, PEV_CHAIN);
					pEntity = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
				}
			}
		}
	}

	SetNextThink(BOID_DELTA_TIME);
}

//=========================================================
// BecomeLeaderThink
// Gathers every nearby boid into this flock, schedules them
// to begin flight, then begins flight itself.
//=========================================================
void CBoid::BecomeLeaderThink(CBaseEntity* pOther)
{
	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	float flTime = GlobalsTime(globals);

	edict_t* selfEdict = EdictFromEntvars(pev);

	Vector vecSrc = PevVector(pev, PEV_ORIGIN);

	edict_t* pEntity = EngineFindEntityInSphere(VecPtr(vecSrc), AFLOCK_GATHER_RADIUS);
	while (pEntity)
	{
		if (!EngineIndexOfEdict(pEntity))
			break;

		entvars_t* pevEntity = EngineGetVarsOfEnt(pEntity);
		const char* pszClassname = BoidClassname(pevEntity);

		if (pszClassname && strcmp(pszClassname, kBoidClassname) == 0 && pEntity != selfEdict)
		{
			CBoid* pBoid = (CBoid*)EngineGetPrivateData(pEntity);
			if (pBoid && pBoid->m_fActive)
			{
				pBoid->m_pSquadLeader = pev;
				pBoid->m_fActive = 0;
				pBoid->SetThink(&CBoid::StartFlightThink);

				float flStagger = RandomFloat(0.0f, 1.0f);
				PevFloat(pBoid->pev, PEV_NEXTTHINK) = flTime + flStagger + 1.0f;
			}
		}

		int chainIndex = PevInt(pevEntity, PEV_CHAIN);
		pEntity = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
	}

	SetThink(&CBoid::StartFlightThink);
	PevFloat(pev, PEV_NEXTTHINK) = flTime;
}

//=========================================================
// StartFlightThink
// Initialises this boid's flight: gives it a starting velocity
// and a random body position, then dispatches to the leader
// or follower flight think.
//=========================================================
void CBoid::StartFlightThink(CBaseEntity* pOther)
{
	PevFloat(pev, PEV_MOVETYPE) = 5.0f;

	// nudge the origin up off the ground
	{
		float vecUp[3];
		float vecLift[3];
		VecSet(vecUp, 0.0f, 0.0f, 1.0f);
		BoidVecAdd(VecPtr(PevVector(pev, PEV_ORIGIN)), vecUp, vecLift);

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineSetOrigin(edict, vecLift);
	}

	// clear the on-ground flag
	PevFloat(pev, PEV_FLAGS) = PevFloat(pev, PEV_FLAGS) - 512.0f;

	// scatter the starting velocity (RNG draw order matches the binary:
	// the up component first, then the two horizontal components)
	{
		float flZ = RandomFloat(0.0f, 100.0f) + 50.0f;
		float flX = 20.0f - RandomFloat(0.0f, 40.0f);
		float flY = 20.0f - RandomFloat(0.0f, 40.0f);

		Vector& vecVel = PevVector(pev, PEV_VELOCITY);
		vecVel.x = flX;
		vecVel.y = flY;
		vecVel.z = flZ;
	}

	SetThink(&CBoid::FlockLeaderThink);
	if (!m_fLeader)
		SetThink(&CBoid::FlockFollowerThink);

	{
		void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 1.0f;
	}

	{
		Vector& vecVel = PevVector(pev, PEV_VELOCITY);
		PevFloat(pev, PEV_FLOCK_SPEED) =
			(float)sqrt(vecVel.x * vecVel.x + vecVel.y * vecVel.y + vecVel.z * vecVel.z);
	}

	PevInt(pev, PEV_SEQUENCE) = 0;
	ResetSequenceInfo(BOID_THINK_INTERVAL);

	AdvanceFrame();
}

//=========================================================
// FPathBlocked
// Traces straight ahead and slightly to each side; returns
// TRUE if the leader has to steer to stay clear of the world.
//=========================================================
BOOL CBoid::FPathBlocked()
{
	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);

	const float* forward = GlobalsForward(globals);
	const float* right = GlobalsRight(globals);

	BOOL fBlocked = 0;

	edict_t* edict = EdictFromEntvars(pev);
	Vector& vecOrigin = PevVector(pev, PEV_ORIGIN);

	float vecForwardProbe[3];
	BoidVecScale(forward, AFLOCK_CHECK_DIST, vecForwardProbe);

	float vecSideProbe[3];
	BoidVecScale(right, AFLOCK_PROBE_OFFSET, vecSideProbe);

	// straight ahead
	{
		float vecEnd[3];
		BoidVecAdd(VecPtr(vecOrigin), vecForwardProbe, vecEnd);

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine(VecPtr(vecOrigin), vecEnd, 0, edict, &tr);	// binary thunk arg 0 -> ignore monsters
		if (tr.flFraction != 1.0f)
			fBlocked = 1;
	}

	// ahead and to the right: binary traces from (origin + right*12) to (+ forward*192)
	{
		float vecStart[3];
		BoidVecAdd(VecPtr(vecOrigin), vecSideProbe, vecStart);
		float vecEnd[3];
		BoidVecAdd(vecStart, vecForwardProbe, vecEnd);

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine(vecStart, vecEnd, 0, edict, &tr);	// start offset to the side, ignore monsters
		if (tr.flFraction != 1.0f)
			fBlocked = 1;
	}

	// ahead and to the left: binary traces from (origin - right*12) to (+ forward*192)
	{
		float vecStart[3];
		BoidVecSub(VecPtr(vecOrigin), vecSideProbe, vecStart);
		float vecEnd[3];
		BoidVecAdd(vecStart, vecForwardProbe, vecEnd);

		TraceResult tr;
		memset(&tr, 0, sizeof(tr));
		EngineTraceLine(vecStart, vecEnd, 0, edict, &tr);	// start offset to the side, ignore monsters
		if (tr.flFraction != 1.0f)
			fBlocked = 1;
	}

	return fBlocked;
}

//=========================================================
// SpreadFlock
// Pushes each flock-mate's flight direction away from this
// boid to avoid clumping (leader variant).
//=========================================================
void CBoid::SpreadFlock()
{
	Vector vecSrc = PevVector(pev, PEV_ORIGIN);

	edict_t* pEntity = EngineFindEntityInSphere(VecPtr(vecSrc), AFLOCK_AVOID_RADIUS);
	while (pEntity)
	{
		if (!EngineIndexOfEdict(pEntity))
			break;

		entvars_t* pevEntity = EngineGetVarsOfEnt(pEntity);
		const char* pszClassname = BoidClassname(pevEntity);

		if (pszClassname && strcmp(pszClassname, kBoidClassname) == 0)
		{
			float vecDir[3];
			vecDir[0] = PevVector(pevEntity, PEV_ORIGIN).x - PevVector(pev, PEV_ORIGIN).x;
			vecDir[1] = PevVector(pevEntity, PEV_ORIGIN).y - PevVector(pev, PEV_ORIGIN).y;
			vecDir[2] = PevVector(pevEntity, PEV_ORIGIN).z - PevVector(pev, PEV_ORIGIN).z;

			float vecToHelp[3];
			BoidVecNormalize(vecDir, vecToHelp);

			Vector& vecVel = PevVector(pevEntity, PEV_VELOCITY);
			float flSpeed = BoidVecLength(VecPtr(vecVel));

			float vecFlight[3];
			BoidVecNormalize(VecPtr(vecVel), vecFlight);

			vecFlight[0] = (vecFlight[0] + vecToHelp[0]) * 0.5f;
			vecFlight[1] = (vecFlight[1] + vecToHelp[1]) * 0.5f;
			vecFlight[2] = (vecFlight[2] + vecToHelp[2]) * 0.5f;

			vecVel.x = vecFlight[0] * flSpeed;
			vecVel.y = vecFlight[1] * flSpeed;
			vecVel.z = vecFlight[2] * flSpeed;
		}

		int chainIndex = PevInt(pevEntity, PEV_CHAIN);
		pEntity = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
	}
}

//=========================================================
// SpreadFlock2
// Follower variant: accumulates avoidance into each
// flock-mate's flight direction.
//=========================================================
void CBoid::SpreadFlock2()
{
	Vector vecSrc = PevVector(pev, PEV_ORIGIN);

	edict_t* pEntity = EngineFindEntityInSphere(VecPtr(vecSrc), AFLOCK_AVOID_RADIUS);
	while (pEntity)
	{
		if (!EngineIndexOfEdict(pEntity))
			break;

		entvars_t* pevEntity = EngineGetVarsOfEnt(pEntity);
		const char* pszClassname = BoidClassname(pevEntity);

		if (pszClassname && strcmp(pszClassname, kBoidClassname) == 0)
		{
			float vecDir[3];
			vecDir[0] = PevVector(pev, PEV_ORIGIN).x - PevVector(pevEntity, PEV_ORIGIN).x;
			vecDir[1] = PevVector(pev, PEV_ORIGIN).y - PevVector(pevEntity, PEV_ORIGIN).y;
			vecDir[2] = PevVector(pev, PEV_ORIGIN).z - PevVector(pevEntity, PEV_ORIGIN).z;

			float vecToHelp[3];
			BoidVecNormalize(vecDir, vecToHelp);

			Vector& vecVel = PevVector(pev, PEV_VELOCITY);	// SELF velocity, not neighbor (binary: iVar3 = pev, +0x40)
			vecVel.x = vecVel.x + vecToHelp[0];
			vecVel.y = vecVel.y + vecToHelp[1];
			vecVel.z = vecVel.z + vecToHelp[2];
		}

		int chainIndex = PevInt(pevEntity, PEV_CHAIN);
		pEntity = chainIndex ? EnginePEntityOfEntIndex(chainIndex) : NULL;
	}
}

//=========================================================
// FlockLeaderThink
// Steers the flock leader: makes the sonar call, avoids the
// world and keeps the followers spread out.
//=========================================================
void CBoid::FlockLeaderThink(CBaseEntity* pOther)
{
	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);

	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + 0.1f;

	// roughly once every 100 ticks the leader pings its sonar
	if (RandomFloat(0.0f, 100.0f) <= 1.0f)
	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 2, pSonarSounds[0], BOID_VOL, BOID_ATTN_NORM);
	}

	if (!m_fLeader)
		return;

	EngineMakeVectors(VecPtr(PevVector(pev, PEV_ANGLES)));

	if (FPathBlocked())
	{
		m_fPathBlocked = 1;

		if (!m_fTurning)
		{
			// trace to each side and turn toward whichever has more
			// open space ahead
			const float* right = GlobalsRight(globals);
			Vector& vecOrigin = PevVector(pev, PEV_ORIGIN);
			edict_t* edict = EdictFromEntvars(pev);

			float vecRightProbe[3];
			BoidVecScale(right, AFLOCK_CHECK_DIST, vecRightProbe);

			// clearance to the right
			float vecRightEnd[3];
			BoidVecAdd(VecPtr(vecOrigin), vecRightProbe, vecRightEnd);

			TraceResult trRight;
			memset(&trRight, 0, sizeof(trRight));
			EngineTraceLine(VecPtr(vecOrigin), vecRightEnd, 0, edict, &trRight);	// binary thunk arg 0 -> ignore monsters

			float vecRightClear[3];
			BoidVecSub(trRight.vecEndPos, VecPtr(vecOrigin), vecRightClear);
			float flRight = BoidVecLength(vecRightClear);

			// clearance to the left
			float vecLeftEnd[3];
			BoidVecSub(VecPtr(vecOrigin), vecRightProbe, vecLeftEnd);

			TraceResult trLeft;
			memset(&trLeft, 0, sizeof(trLeft));
			EngineTraceLine(VecPtr(vecOrigin), vecLeftEnd, 0, edict, &trLeft);	// binary thunk arg 0 -> ignore monsters

			float vecLeftClear[3];
			BoidVecSub(trLeft.vecEndPos, VecPtr(vecOrigin), vecLeftClear);
			float flLeft = BoidVecLength(vecLeftClear);

			if (flLeft >= flRight)
				PevFloat(pev, PEV_AVELOCITY_Y) = AFLOCK_TURN_RATE;
			else
				PevFloat(pev, PEV_AVELOCITY_Y) = -AFLOCK_TURN_RATE;

			m_fTurning = 1;
		}

		SpreadFlock();

		// fly forward at the current flock speed
		{
			const float* forward = GlobalsForward(globals);
			float flSpeed = PevFloat(pev, PEV_FLOCK_SPEED);

			Vector& vecVel = PevVector(pev, PEV_VELOCITY);
			vecVel.x = forward[0] * flSpeed;
			vecVel.y = forward[1] * flSpeed;
			vecVel.z = forward[2] * flSpeed;
		}

		// trace 16 units straight down; if there's floor close below and we are
		// descending, stop the descent so we don't dive into the ground
		{
			const float* up = GlobalsUp(globals);
			Vector& vecOrigin = PevVector(pev, PEV_ORIGIN);

			float vecDownProbe[3];
			BoidVecScale(up, 16.0f, vecDownProbe);

			float vecEnd[3];
			BoidVecSub(VecPtr(vecOrigin), vecDownProbe, vecEnd);

			edict_t* edict = EdictFromEntvars(pev);

			TraceResult tr;
			memset(&tr, 0, sizeof(tr));
			EngineTraceLine(VecPtr(vecOrigin), vecEnd, 0, edict, &tr);	// binary thunk arg 0 -> ignore monsters

			if (tr.flFraction != 1.0f && PevVector(pev, PEV_VELOCITY).z < 0.0f)
				PevVector(pev, PEV_VELOCITY).z = 0.0f;
		}

		// if we've hit the floor, hop up and level off (flags is stored as a
		// float in this build, so convert to int before masking)
		if (((int)PevFloat(pev, PEV_FLAGS) & FL_ONGROUND) != 0)
		{
			float vecUp[3];
			float vecLift[3];
			VecSet(vecUp, 0.0f, 0.0f, 1.0f);
			BoidVecAdd(VecPtr(PevVector(pev, PEV_ORIGIN)), vecUp, vecLift);

			edict_t* edict = EdictFromEntvars(pev);
			if (edict)
				EngineSetOrigin(edict, vecLift);

			PevVector(pev, PEV_VELOCITY).z = 0.0f;
		}

		AdvanceFrame();
	}
	else
	{
		if (m_fTurning)
		{
			m_fTurning = 0;
			PevFloat(pev, PEV_AVELOCITY_Y) = 0.0f;
		}

		m_fPathBlocked = 0;

		if (PevFloat(pev, PEV_FLOCK_SPEED) <= AFLOCK_FLY_SPEED)
			PevFloat(pev, PEV_FLOCK_SPEED) = PevFloat(pev, PEV_FLOCK_SPEED) + 5.0f;

		{
			const float* forward = GlobalsForward(globals);
			float flSpeed = PevFloat(pev, PEV_FLOCK_SPEED);

			Vector& vecVel = PevVector(pev, PEV_VELOCITY);
			vecVel.x = forward[0] * flSpeed;
			vecVel.y = forward[1] * flSpeed;
			vecVel.z = forward[2] * flSpeed;
		}

		AdvanceFrame();
	}
}

//=========================================================
// FlockFollowerThink
// Steers a follower so that it tracks the leader's heading
// and matches its speed by distance.
//=========================================================
void CBoid::FlockFollowerThink(CBaseEntity* pOther)
{
	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + BOID_THINK_INTERVAL;

	entvars_t* pevLeader = m_pSquadLeader;
	if (!pevLeader)
		return;

	float vecDirToLeader[3];
	vecDirToLeader[0] = PevVector(pevLeader, PEV_ORIGIN).x - PevVector(pev, PEV_ORIGIN).x;
	vecDirToLeader[1] = PevVector(pevLeader, PEV_ORIGIN).y - PevVector(pev, PEV_ORIGIN).y;
	vecDirToLeader[2] = PevVector(pevLeader, PEV_ORIGIN).z - PevVector(pev, PEV_ORIGIN).z;

	// adopt the leader's heading
	PevVector(pev, PEV_ANGLES) = PevVector(pevLeader, PEV_ANGLES);

	float flDistToLeader = BoidVecLength(vecDirToLeader);

	float flLeaderSpeed = BoidVecLength(VecPtr(PevVector(pevLeader, PEV_VELOCITY)));

	if (!FInViewCone(pev, pevLeader, 0.1f))
	{
		// can't see the leader ahead: ease off
		m_flGoalSpeed = flLeaderSpeed * 0.5f;
	}
	else if (flDistToLeader > AFLOCK_TOO_FAR)
	{
		m_flGoalSpeed = flLeaderSpeed * 1.5f;	// too far, speed up
	}
	else if (flDistToLeader < AFLOCK_TOO_CLOSE)
	{
		m_flGoalSpeed = flLeaderSpeed * 0.5f;	// too close, slow down
	}
	// else nicely spaced: keep last frame's goal speed unchanged

	SpreadFlock2();

	// turn the current velocity into a unit heading
	{
		Vector& vecVel = PevVector(pev, PEV_VELOCITY);
		PevFloat(pev, PEV_FLOCK_SPEED) = BoidVecLength(VecPtr(vecVel));

		float vecHeading[3];
		BoidVecNormalize(VecPtr(vecVel), vecHeading);
		vecVel.x = vecHeading[0];
		vecVel.y = vecHeading[1];
		vecVel.z = vecHeading[2];

		// if the leader is far away, steer our heading toward it
		if (flDistToLeader > AFLOCK_TOO_FAR)
		{
			float vecDir[3];
			BoidVecNormalize(vecDirToLeader, vecDir);
			vecVel.x = (vecVel.x + vecDir[0]) * 0.5f;
			vecVel.y = (vecVel.y + vecDir[1]) * 0.5f;
			vecVel.z = (vecVel.z + vecDir[2]) * 0.5f;
		}
	}

	// clamp the goal speed
	if (m_flGoalSpeed > AFLOCK_MAX_SPEED)
		m_flGoalSpeed = AFLOCK_MAX_SPEED;

	// accelerate / decelerate toward the goal speed
	{
		float flSpeed = PevFloat(pev, PEV_FLOCK_SPEED);
		if (flSpeed < m_flGoalSpeed)
			PevFloat(pev, PEV_FLOCK_SPEED) = flSpeed + AFLOCK_ACCELERATE;
		else if (flSpeed > m_flGoalSpeed)
			PevFloat(pev, PEV_FLOCK_SPEED) = flSpeed - AFLOCK_ACCELERATE;
	}

	// apply the final velocity
	{
		Vector& vecVel = PevVector(pev, PEV_VELOCITY);
		float flSpeed = PevFloat(pev, PEV_FLOCK_SPEED);
		vecVel.x = vecVel.x * flSpeed;
		vecVel.y = vecVel.y * flSpeed;
		vecVel.z = vecVel.z * flSpeed;
	}

	AdvanceFrame();
}

//=========================================================
// StartAnimating
// Schedules the lone-boid animate/recruit think one tick out.
//=========================================================
void CBoid::StartAnimating()
{
	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	PevFloat(pev, PEV_NEXTTHINK) = GlobalsTime(globals) + BOID_THINK_INTERVAL;

	SetThink(&CBoid::AdvanceFrameThink);
}

//=========================================================
// BoidFlockCreateMember
// Spawns and var-initialises a single flock boid at the given
// origin/angles, sets it animating, and returns its edict.
// The flock controller calls this once per member.
//=========================================================
edict_t* BoidFlockCreateMember(const float* origin, const float* angles)
{
	edict_t* created = EngineCreateEntity();
	entvars_t* entvars = created ? EngineGetVarsOfEnt(created) : NULL;

	edict_t* edict = EdictFromEntvars(entvars);
	if (!edict)
		return NULL;

	CBoid* pBoid = (CBoid*)EngineGetPrivateData(edict);
	if (!pBoid)
	{
		void* privateData = EngineAllocPrivateData(edict, BOID_PRIVATE_DATA_SIZE);
		if (!privateData)
			return NULL;

		memset(privateData, 0, BOID_PRIVATE_DATA_SIZE);

		pBoid = new (privateData) CBoid();
		pBoid->pev = entvars;
		pBoid->m_pGlobals = GlobalsFromEntvars(entvars);
	}

	pBoid->SetupBoid();

	PevFloat(pBoid->pev, PEV_MOVETYPE) = 6.0f;

	PevVector(pBoid->pev, PEV_ANGLES).x = angles[0];
	PevVector(pBoid->pev, PEV_ANGLES).y = angles[1];
	PevVector(pBoid->pev, PEV_ANGLES).z = angles[2];

	edict_t* boidEdict = EdictFromEntvars(pBoid->pev);
	if (boidEdict)
		EngineSetOrigin(boidEdict, origin);

	PevFloat(pBoid->pev, PEV_FRAME) = 0.0f;

	pBoid->StartAnimating();

	return boidEdict;
}

//=========================================================
// monster_boid
//=========================================================
DLLEXPORT void monster_boid(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, BOID_PRIVATE_DATA_SIZE);
		if (!privateData)
			return;

		memset(privateData, 0, BOID_PRIVATE_DATA_SIZE);

		CBoid* monster = new (privateData) CBoid();
		monster->pev = entvars;
		monster->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
