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
// BaseMonster - CBaseMonster implementation
//=========================================================

#include <math.h>
#include <new>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "basemonster.h"
#include "enginefuncs.h"
#include "utils.h"

//=========================================================
// entvars byte offsets used by the shared damage handler that are
// not yet named in utils.h. Values match the shared TakeDamage / Killed
// handlers exactly; naming follows the standard Half-Life entvars_t fields.
//=========================================================
enum
{
	PEV_ABSMIN			= 4,	// absolute bounding-box mins (vec3)
	PEV_ABSMAX			= 16,	// absolute bounding-box maxs (vec3)
	PEV_DMG_TAKE		= 400,	// m_flDmgTake accumulator (this frame)
	PEV_DMG_SAVE		= 404,	// m_flDmgSave / armor pool
	PEV_DMG_FIELD		= 444,	// pev->dmg_take running total
	PEV_DMG_FIELD2		= 448,	// pev->dmg_save running total
	PEV_DMG_INFLICTOR	= 452,	// pev->dmg_inflictor (attacker entindex)
	PEV_DMGTEAM			= 388,	// team id used to skip friendly-fire
	// render / movement fields the gib entity touches.
	// NB: pev->effects is at offset 140 (see player.cpp PEV_EFFECTS / FireGlock),
	// NOT 128. Offset 128 is pev->model (utils.h PEV_MODEL): the
	// Killed gib path zeroes pev->model @128 to hide the body.
	PEV_RENDERMODE_B	= 240,	// pev->rendermode (0xF0)
	PEV_RENDERAMT		= 244,	// pev->renderamt  (0xF4) - gib bounce-life counter
	PEV_RENDERFX		= 260,	// pev->renderfx   (0x104)
	PEV_AVELOCITY_Y		= 92,	// pev->avelocity[1] (0x5C) - gib spin
};

//=========================================================
// Temp-entity message constants (verified against combat.cpp and the
// blood-spray byte stream in the binary: SVC_TEMPENTITY=23, the blood
// decal message=0x68=104, TE_BLOODSTREAM=0x67=103).
//=========================================================
enum
{
	GIB_SVC_TEMPENTITY	= 23,
	GIB_TE_BLOODDECAL	= 104,	// 0x68: blood decal splat at the trace hit
	GIB_TE_BLOODSTREAM	= 103,	// 0x67: directional blood spray at the hit
};

//=========================================================
// Gib models the human-gib shower spawns (the binary spawns the skull
// always, then one of legbone / lung / b_gib / b_bone). Strings verified
// against the binary's data section; the engine precaches all five at
// level start.
//=========================================================
static const char* const kGibSkull		= "models/gib_skull.mdl";
static const char* const kGibLegbone	= "models/gib_legbone.mdl";
static const char* const kGibLung		= "models/gib_lung.mdl";
static const char* const kGibBGib		= "models/gib_b_gib.mdl";
static const char* const kGibBBone		= "models/gib_b_bone.mdl";

// Splat sound played when the body bursts into gibs. Verified against
// the even-roll branch of the shared Killed handler.
static const char* const kSndBodySplat	= "common/bodysplat.wav";

// globalvars_t kill-tally field (globals+0xAC). Both death paths in
// the binary bump it by 1.0 and emit a one-byte death event (WriteByte
// MSG_ALL, 27) when a monster (flag 0x20) dies.
enum { GLOBALS_MONSTERS_KILLED = 172 };

static void BookMonsterKill(void* globals)
{
	if (globals)
		*(float*)((unsigned char*)globals + GLOBALS_MONSTERS_KILLED) += 1.0f;

	EngineWriteByte(2, 27);
}

//=========================================================
// MonsterCenter - entity bounding-box center used as the
// damage source when the inflictor is the victim itself.
//=========================================================
static void MonsterCenter(float* out, entvars_t* pevVictim)
{
	out[0] = PevVector(pevVictim, PEV_SIZE).x * 0.5f + PevVector(pevVictim, PEV_ABSMIN).x;
	out[1] = PevVector(pevVictim, PEV_SIZE).y * 0.5f + PevVector(pevVictim, PEV_ABSMIN).y;
	out[2] = PevVector(pevVictim, PEV_SIZE).z * 0.5f + PevVector(pevVictim, PEV_ABSMIN).z;
}

//=========================================================
// EntIndexOf / VarsOf helpers (engine table slots used by the handler:
// IndexOfEdict, GetPrivateData, and PEntityOfEntIndex via the attacker
// edict pointer).
//=========================================================
static int EntIndexFromEdictPtr(edict_t* edict)
{
	return edict ? EngineIndexOfEdict(edict) : 0;
}

//=========================================================
// CGib (vtable) - the bouncing gib model the human-gib shower
// spawns. The binary builds it as a raw 32-byte private-data object whose
// Spawn randomizes an origin inside the dead monster's
// bounding box, flings it as a MOVETYPE_BOUNCE prop, copies the monster's
// blood color (this+0x1c), and arms a 10-second think.
//
// Think chain (m_pfnThink, dispatched via the default Think slot):
//   FirstThink fires once after 10s: it settles the gib
//     (rendermode 2, solid 0, avelocity = g_vecZero), arms +0.1 and hands
//     off to BounceThink.
//   BounceThink: each tick drains 5 from pev->renderamt; when
//     it hits 0 it arms +5 and hands off to RemoveThink.
//   RemoveThink: RemoveEntity.
//
// Touch (entity vtable slot 6): on each world bounce it traces
// 24 units straight down and splats a blood decal; once the gib has come to
// rest (velocity == g_vecZero) it zeroes the avelocity and goes non-solid.
//=========================================================
class CGib : public CBaseAnimating
{
public:
	// The binary: spawn the gib for a dying monster (pevSource) using the
	// given model and the monster's raw health (drives the velocity scale).
	void SpawnFromSource(entvars_t* pevSource, const char* model, float flHealth, int bloodColor);

	virtual void Touch(CBaseEntity* pOther);	// blood-on-bounce

	void FirstThink(CBaseEntity* pOther);							// settle
	void BounceThink(CBaseEntity* pOther);							// renderamt drain
	void RemoveThink(CBaseEntity* pOther);							// engine RemoveEntity

	int m_bloodColor;	// copied from the source monster (this+0x1c in the binary)
};

HL_COMPILE_TIME_ASSERT(sizeof(CGib) <= 336, CGib_private_data_size);

// The engine "zero/stop" vector global. The gib's settle think
// stamps it into pev->avelocity, and the touch handler compares pev->velocity
// against it to detect that the gib has come to rest.
static const float g_vecGibStop[3] = {0.0f, 0.0f, 0.0f};

static CGib* CreateGib()
{
	edict_t* pEdict = EngineCreateEntity();
	entvars_t* pev = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
	if (!pev)
		return NULL;

	void* privateData = EngineAllocPrivateData(pEdict, 336);
	if (!privateData)
		return NULL;

	memset(privateData, 0, 336);

	CGib* pGib = new (privateData) CGib();
	pGib->pev = pev;
	pGib->m_pGlobals = GlobalsFromEntvars(pev);
	return pGib;
}

void CGib::SpawnFromSource(entvars_t* pevSource, const char* model, float flHealth, int bloodColor)
{
	if (!pev || !pevSource || !model)
		return;

	edict_t* edict = EdictFromEntvars(pev);

	// MOVETYPE_BOUNCE prop, full opaque render, SOLID_SLIDEBOX.
	PevFloat(pev, PEV_MOVETYPE) = 10.0f;
	PevFloat(pev, PEV_RENDERAMT) = 255.0f;
	PevInt(pev, PEV_RENDERMODE_B) = 0;
	PevInt(pev, PEV_RENDERFX) = 0;
	PevFloat(pev, PEV_SOLID) = 3.0f;

	if (edict)
		EngineSetModel(edict, model);

	// Zero the bounding box right after SetModel, matching the binary.
	{
		float vecZero[3] = {0.0f, 0.0f, 0.0f};
		if (edict)
			EngineSetSize(edict, vecZero, vecZero);
	}

	// Origin: a random point inside the dead monster's bounding box
	// (absmin + rand[0,1] * size), exactly as the binary.
	{
		Vector& org = PevVector(pev, PEV_ORIGIN);
		org.x = RandomFloat(0.0f, 1.0f) * PevVector(pevSource, PEV_SIZE).x + PevVector(pevSource, PEV_ABSMIN).x;
		org.y = RandomFloat(0.0f, 1.0f) * PevVector(pevSource, PEV_SIZE).y + PevVector(pevSource, PEV_ABSMIN).y;
		org.z = RandomFloat(0.0f, 1.0f) * PevVector(pevSource, PEV_SIZE).z + PevVector(pevSource, PEV_ABSMIN).z;
	}

	// Launch velocity: random fling, then scaled by how far below zero the
	// monster's health was driven (the harder the over-kill, the faster).
	// Draw order matches the binary: z (200..300), then y, then x.
	{
		float vz = RandomFloat(200.0f, 300.0f);
		float vy = RandomFloat(-100.0f, 100.0f);
		float vx = RandomFloat(-100.0f, 100.0f);

		float scale;
		if (flHealth > -50.0f)
			scale = 0.7f;
		else if (flHealth > -200.0f)
			scale = 2.0f;
		else
			scale = 10.0f;

		Vector& vel = PevVector(pev, PEV_VELOCITY);
		vel.x = vx * scale;
		vel.y = vy * scale;
		vel.z = vz * scale;
	}

	PevFloat(pev, PEV_AVELOCITY_Y) = RandomFloat(100.0f, 300.0f);

	m_bloodColor = bloodColor;	// gib remembers blood color (this+0x1c in the binary)

	// The binary arms a 10-second settle think and a blood-on-bounce touch.
	SetNextThink(10.0f);
	SetThink(&CGib::FirstThink);
}

//=========================================================
// FirstThink: fires once 10 seconds after spawn. Settles the
// gib - half-translucent rendermode, non-solid, avelocity latched to the
// engine zero vector - then hands off to the per-tick BounceThink.
//=========================================================
void CGib::FirstThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	PevFloat(pev, PEV_RENDERMODE_B) = 2.0f;		// kRenderTransTexture
	PevInt(pev, PEV_SOLID) = 0;

	PevVector(pev, PEV_AVELOCITY).x = g_vecGibStop[0];
	PevVector(pev, PEV_AVELOCITY).y = g_vecGibStop[1];
	PevVector(pev, PEV_AVELOCITY).z = g_vecGibStop[2];

	SetNextThink(0.1f);
	SetThink(&CGib::BounceThink);
}

//=========================================================
// BounceThink: each tick drains 5 from the gib's renderamt
// life counter; when it runs out the gib arms a 5-second fuse and hands off
// to RemoveThink so the engine reclaims it.
//=========================================================
void CGib::BounceThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	if ((PevInt(pev, PEV_RENDERAMT) & 0x7FFFFFFF) != 0)
	{
		PevFloat(pev, PEV_RENDERAMT) -= 5.0f;
		SetNextThink(0.1f);
	}
	else
	{
		SetNextThink(5.0f);
		SetRemoveThink();
	}
}

//=========================================================
// RemoveThink: free the gib edict.
//=========================================================
void CGib::RemoveThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// Touch: each world bounce traces 24 units straight down and
// splats a blood decal where it lands. Once the gib has come to rest (its
// velocity matches the engine zero vector) it zeroes its avelocity and goes
// non-solid so it stops jittering.
//=========================================================
void CGib::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);

	float vecSrc[3];
	vecSrc[0] = PevVector(pev, PEV_ORIGIN).x;
	vecSrc[1] = PevVector(pev, PEV_ORIGIN).y;
	vecSrc[2] = PevVector(pev, PEV_ORIGIN).z + 8.0f;

	float vecEnd[3];
	vecEnd[0] = vecSrc[0];
	vecEnd[1] = vecSrc[1];
	vecEnd[2] = vecSrc[2] - 24.0f;

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(vecSrc, vecEnd, 0, edict, &tr);

	// blood decal splat at the trace hit
	EngineWriteByte(0, GIB_SVC_TEMPENTITY);
	EngineWriteByte(0, GIB_TE_BLOODDECAL);
	EngineWriteCoord(0, tr.vecEndPos[0]);
	EngineWriteCoord(0, tr.vecEndPos[1]);
	EngineWriteCoord(0, tr.vecEndPos[2]);
	EngineWriteShort(0, (short)EngineModelIndex(TraceHitIndex(&tr)));

	if (m_bloodColor == 70)
		EngineWriteByte(0, RandomLong(14, 19));
	else
		EngineWriteByte(0, RandomLong(20, 25));

	// gib at rest: stop the spin and go non-solid.
	if (PevVector(pev, PEV_VELOCITY).x == g_vecGibStop[0]
		&& PevVector(pev, PEV_VELOCITY).y == g_vecGibStop[1]
		&& PevVector(pev, PEV_VELOCITY).z == g_vecGibStop[2])
	{
		PevVector(pev, PEV_AVELOCITY).x = g_vecGibStop[0];
		PevVector(pev, PEV_AVELOCITY).y = g_vecGibStop[1];
		PevVector(pev, PEV_AVELOCITY).z = g_vecGibStop[2];
		PevInt(pev, PEV_SOLID) = 0;
	}
}

//=========================================================
// GibBloodSpray (the 6-iteration spray loop in): for a violently
// killed monster, trace six short rays out and away from the damage direction
// (spread through the engine right/up vectors) and, wherever they hit world,
// splat a blood decal plus a directional blood stream. Constants verified from
// the raw bytes: 6 iterations, 0.35 spread, 256-unit trace, decal index
// 14..19 (red blood) or 20..25, bloodstream amount 15.
//=========================================================
static void GibBloodSpray(entvars_t* pev, void* globals, int bloodColor)
{
	int i;

	if (!pev)
		return;

	// Engine right/up come from the damage-direction frame. The binary feeds
	// the raw damage direction straight to MakeVectors (the binary
	// EngineMakeVectors), matching combat.cpp's blood-trace convention.
	EngineMakeVectors(g_vecAttackDir);

	{
		const float* right = GlobalsRight(globals);
		const float* up = GlobalsUp(globals);
		edict_t* selfEdict = EdictFromEntvars(pev);

		for (i = 0; i < 6; i++)
		{
			float spreadRight = RandomFloat(-1.0f, 1.0f) * 0.35f;
			float spreadUp = RandomFloat(-1.0f, 1.0f) * 0.35f;

			float dir[3];
			dir[0] = -g_vecAttackDir[0]
				+ (right ? right[0] : 0.0f) * spreadRight
				+ (up ? up[0] : 0.0f) * spreadUp;
			dir[1] = -g_vecAttackDir[1]
				+ (right ? right[1] : 0.0f) * spreadRight
				+ (up ? up[1] : 0.0f) * spreadUp;
			dir[2] = -g_vecAttackDir[2]
				+ (right ? right[2] : 0.0f) * spreadRight
				+ (up ? up[2] : 0.0f) * spreadUp;

			float vecSrc[3];
			vecSrc[0] = PevVector(pev, PEV_ORIGIN).x;
			vecSrc[1] = PevVector(pev, PEV_ORIGIN).y;
			vecSrc[2] = PevVector(pev, PEV_ORIGIN).z;

			float vecEnd[3];
			vecEnd[0] = vecSrc[0] + dir[0] * 256.0f;
			vecEnd[1] = vecSrc[1] + dir[1] * 256.0f;
			vecEnd[2] = vecSrc[2] + dir[2] * 256.0f;

			TraceResult tr;
			memset(&tr, 0, sizeof(tr));
			EngineTraceLine(vecSrc, vecEnd, 0, selfEdict, &tr);

			if (tr.flFraction != 1.0f)
			{
				// blood decal splat at the hit
				EngineWriteByte(0, GIB_SVC_TEMPENTITY);
				EngineWriteByte(0, GIB_TE_BLOODDECAL);
				EngineWriteCoord(0, tr.vecEndPos[0]);
				EngineWriteCoord(0, tr.vecEndPos[1]);
				EngineWriteCoord(0, tr.vecEndPos[2]);
				EngineWriteShort(0, (short)EngineModelIndex(TraceHitIndex(&tr)));

				if (bloodColor == 70)
					EngineWriteByte(0, RandomLong(14, 19));
				else
					EngineWriteByte(0, RandomLong(20, 25));

				// directional blood stream at the hit
				EngineWriteByte(0, GIB_SVC_TEMPENTITY);
				EngineWriteByte(0, GIB_TE_BLOODSTREAM);
				EngineWriteCoord(0, tr.vecEndPos[0]);
				EngineWriteCoord(0, tr.vecEndPos[1]);
				EngineWriteCoord(0, tr.vecEndPos[2]);
				EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
				EngineWriteCoord(0, RandomFloat(-1.0f, 1.0f));
				EngineWriteCoord(0, RandomFloat(0.0f, 1.0f));
				EngineWriteByte(0, (unsigned char)bloodColor);
				EngineWriteByte(0, 15);
			}
		}
	}
}

//=========================================================
// SpawnHumanGibs (the gib-shower tail of): a flesh monster
// (spawnSkull) flings a skull gib first; a non-flesh monster instead just
// burns one RNG roll (matching the bare rand() the binary takes in that
// branch). Either way one more random gib is flung unconditionally - 1-in-15
// legbone, otherwise an even split of lung / b_gib / b_bone. Health drives
// each gib's launch velocity scale.
//=========================================================
static void SpawnHumanGibs(entvars_t* pevSource, float flHealth, int bloodColor, BOOL spawnSkull)
{
	if (spawnSkull)
	{
		CGib* pSkull = CreateGib();
		if (pSkull)
			pSkull->SpawnFromSource(pevSource, kGibSkull, flHealth, bloodColor);
	}
	else
	{
		// non-flesh monster: the binary still advances the RNG here.
		rand();
	}

	{
		CGib* pGib = CreateGib();
		if (!pGib)
			return;

		const char* model;
		if (rand() % 15 == 0)
		{
			model = kGibLegbone;
		}
		else
		{
			int pick = rand() % 3;
			if (pick == 0)
				model = kGibLung;
			else if (pick == 1)
				model = kGibBGib;
			else
				model = kGibBBone;
		}

		pGib->SpawnFromSource(pevSource, model, flHealth, bloodColor);
	}
}

CBaseMonster::CBaseMonster()
{
	int i;

	m_Activity = 0;
	m_IdealActivity = 0;
	m_flNextAttack = 0.0f;
	m_bloodColor = 0;

	m_vecMoveGoal.x = 0.0f;
	m_vecMoveGoal.y = 0.0f;
	m_vecMoveGoal.z = 0.0f;

	m_vecEnemyLKP.x = 0.0f;
	m_vecEnemyLKP.y = 0.0f;
	m_vecEnemyLKP.z = 0.0f;

	for (i = 0; i < 5; i++)
	{
		m_vecRoute[i].x = 0.0f;
		m_vecRoute[i].y = 0.0f;
		m_vecRoute[i].z = 0.0f;
	}

	m_pMoveTarget = NULL;
	m_pSquadLeader = NULL;
	m_pSquadNext = NULL;
	m_iSquadSize = 1;

	m_flGoalRadius = 0.0f;
	m_flDistTooFar = 0.0f;
	m_flNextSoundTime = 0.0f;
	m_flLastEnemySightTime = 0.0f;

	m_iAmmo = 0;
	m_afEnemyFlags = 0;
	m_iRouteIndex = 0;
	m_iRouteGoal = 0;

	m_hActivator = NULL;
	m_vecDeathGoal.x = 0.0f;
	m_vecDeathGoal.y = 0.0f;
	m_vecDeathGoal.z = 0.0f;
}

//=========================================================
// TogglePlayerUse
//=========================================================
void CBaseMonster::TogglePlayerUse(entvars_t* pevPlayer)
{
	if (m_pMoveTarget == pevPlayer)
	{
		m_pMoveTarget = pev;
		m_Activity = 1;
	}
	else
	{
		m_pMoveTarget = pevPlayer;
		m_Activity = 10;
	}
}

//=========================================================
// TakeDamage
//
// The shared monster damage handler (binary vtable slot 17). It:
//  - gates on pev->takedamage,
//  - records the (normalized) damage direction in globals,
//  - splits the incoming damage across the armor pool (pev->dmg_save) and
//    flesh (pev->health), rounding each up via ceil,
//  - books the totals into pev->dmg_take/dmg_save/dmg_inflictor,
//  - applies a directional knockback to pev->velocity,
//  - subtracts the flesh damage from pev->health and, when the monster is
//    dead, dispatches Killed; otherwise it acquires the attacker as the new
//    enemy and raises the Pain virtual once pain_finished has elapsed.
//=========================================================
int CBaseMonster::TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage)
{
	float center[3];
	float dir[3];
	float length;
	float dmgArmor;
	float dmgFlesh;
	int flags;

	if (!pev)
		return 0;

	if ((PevInt(pev, PEV_TAKEDAMAGE) & 0x7FFFFFFF) == 0)
		return 0;

	// --- damage direction (normalized inflictor.origin - self center) ---
	MonsterCenter(center, pev);
	dir[0] = PevVector(inflictor, PEV_ORIGIN).x - center[0];
	dir[1] = PevVector(inflictor, PEV_ORIGIN).y - center[1];
	dir[2] = PevVector(inflictor, PEV_ORIGIN).z - center[2];

	length = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
	if (length == 0.0f)
	{
		dir[0] = 0.0f;
		dir[1] = 0.0f;
		dir[2] = 0.0f;
	}
	else
	{
		float inv = 1.0f / length;
		dir[0] = dir[0] * inv;
		dir[1] = dir[1] * inv;
		dir[2] = dir[2] * inv;
	}

	g_vecAttackDir[0] = dir[0];
	g_vecAttackDir[1] = dir[1];
	g_vecAttackDir[2] = dir[2];

	EntIndexFromEdictPtr(attacker ? EdictFromEntvars(attacker) : NULL);

	// --- armor / flesh split ---
	dmgArmor = (float)ceil((double)(PevFloat(pev, PEV_DMG_TAKE) * damage));
	if (PevFloat(pev, PEV_DMG_SAVE) <= dmgArmor)
	{
		dmgArmor = PevFloat(pev, PEV_DMG_SAVE);
		PevFloat(pev, PEV_DMG_TAKE) = 0.0f;
		PevInt(pev, PEV_ITEMS_HIGH) &= 0xFFFF1FFF;
	}
	PevFloat(pev, PEV_DMG_SAVE) -= dmgArmor;
	dmgFlesh = (float)ceil((double)(damage - dmgArmor));

	// --- damage bookkeeping (pev->dmg_take/dmg_save/dmg_inflictor) ---
	flags = (int)PevFloat(pev, PEV_FLAGS);
	if ((flags & 8) != 0)
	{
		PevFloat(pev, PEV_DMG_FIELD) += dmgFlesh;
		PevFloat(pev, PEV_DMG_FIELD2) += dmgArmor;
		PevInt(pev, PEV_DMG_INFLICTOR) = EntIndexFromEdictPtr(inflictor ? EdictFromEntvars(inflictor) : NULL);
	}

	// --- directional knockback on pev->velocity ---
	if (inflictor && EntIndexFromEdictPtr(EdictFromEntvars(inflictor))
		&& PevFloat(pev, PEV_MOVETYPE) == 3.0f
		&& attacker && PevFloat(attacker, PEV_SOLID) != 1.0f)
	{
		float push[3];
		float pushLen;

		push[2] = PevVector(pev, PEV_ORIGIN).z
			- (PevVector(inflictor, PEV_ABSMAX).z + PevVector(inflictor, PEV_ABSMIN).z) * 0.5f;
		push[1] = PevVector(pev, PEV_ORIGIN).y
			- (PevVector(inflictor, PEV_ABSMAX).y + PevVector(inflictor, PEV_ABSMIN).y) * 0.5f;
		push[0] = PevVector(pev, PEV_ORIGIN).x
			- (PevVector(inflictor, PEV_ABSMIN).x + PevVector(inflictor, PEV_ABSMAX).x) * 0.5f;

		pushLen = sqrtf(push[0] * push[0] + push[1] * push[1] + push[2] * push[2]);
		if (pushLen == 0.0f)
		{
			push[0] = 0.0f;
			push[1] = 0.0f;
			push[2] = 0.0f;
		}
		else
		{
			float inv = 1.0f / pushLen;
			push[0] = push[0] * inv;
			push[1] = push[1] * inv;
			push[2] = push[2] * inv;
		}

		PevVector(pev, PEV_VELOCITY).x += push[0] * damage * 8.0f;
		PevVector(pev, PEV_VELOCITY).y += push[1] * damage * 8.0f;
		PevVector(pev, PEV_VELOCITY).z += push[2] * damage * 8.0f;
	}

	// --- friendly-fire / godmode gate (always true for non-player monsters) ---
	{
		const char* classname = EngineStringFromIndex(PevInt(pev, PEV_CLASSNAME));
		BOOL notPlayer = !(classname && strcmp(classname, "player") == 0);
		void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
		float friendlyFire = globals ? *(float*)((unsigned char*)globals + 152) : 0.0f;
		BOOL applyDamage;

		// Original: apply when (not player) OR (not godmode AND friendly-fire
		// conditions fail). For non-player monsters this is always true.
		applyDamage = notPlayer
			|| ((flags & 0x40) == 0
				&& (friendlyFire != 1.0f
					|| PevInt(pev, PEV_DMGTEAM) <= 0
					|| !attacker
					|| PevFloat(attacker, PEV_DMGTEAM) != PevFloat(pev, PEV_DMGTEAM)));

		if (!applyDamage)
			return 1;

		PevFloat(pev, PEV_HEALTH) -= dmgFlesh;
		if (PevInt(pev, PEV_HEALTH) <= 0)
		{
			Killed(EntIndexFromEdictPtr(attacker ? EdictFromEntvars(attacker) : NULL));
			return 1;
		}

		// --- acquire attacker as the new enemy ---
		if ((flags & 0x20) != 0
			&& attacker
			&& EntIndexFromEdictPtr(EdictFromEntvars(attacker))
			&& ((int)PevFloat(attacker, PEV_FLAGS) & 0x28) != 0)
		{
			edict_t* attackerEdict = EdictFromEntvars(attacker);
			CBaseEntity* pAttacker = attackerEdict ? (CBaseEntity*)EngineGetPrivateData(attackerEdict) : NULL;

			if (!pAttacker || Classify() != pAttacker->Classify())
			{
				int attackerIndex = EntIndexFromEdictPtr(attackerEdict);
				PevInt(pev, PEV_GOALENTINDEX) = attackerIndex;
				PevInt(pev, PEV_ENEMY) = PevInt(pev, PEV_GOALENTINDEX);
				m_hActivator = attacker;

				m_vecDeathGoal.x = g_vecAttackDir[0] * 64.0f + PevVector(pev, PEV_ORIGIN).x;
				m_vecDeathGoal.y = g_vecAttackDir[1] * 64.0f + PevVector(pev, PEV_ORIGIN).y;
				m_vecDeathGoal.z = g_vecAttackDir[2] * 64.0f + PevVector(pev, PEV_ORIGIN).z;

				{
					float toGoal[3];
					toGoal[0] = m_vecDeathGoal.x - PevVector(pev, PEV_ORIGIN).x;
					toGoal[1] = m_vecDeathGoal.y - PevVector(pev, PEV_ORIGIN).y;
					toGoal[2] = m_vecDeathGoal.z - PevVector(pev, PEV_ORIGIN).z;
					PevFloat(pev, PEV_IDEAL_YAW) = EngineVecToYaw(toGoal);
				}
			}
		}

		// --- pain reaction once pain_finished has elapsed ---
		// (the original reads time from pev->pContainingEntity globals, +524/+124)
		if (GlobalsTime(GlobalsFromEntvars(pev)) > PevFloat(pev, PEV_PAIN_FINISHED))
			Pain(damage);
	}

	return 1;
}

//=========================================================
// SetDeathActivity
//
// Shared death-state setup: selects the death animation activity, faces the
// monster along its current yaw, runs the death animation through the monster
// think loop. Also the body of the default Death.
//=========================================================
void CBaseMonster::SetDeathActivity(int type)
{
	switch (type)
	{
	case 0:
		m_Activity = 35;
		break;
	case 1:
		m_Activity = 36;
		break;
	case 2:
		m_Activity = 37;
		break;
	case 3:
		m_Activity = 38;
		break;
	case 4:
		m_Activity = 39;
		break;
	default:
		EngineAlertMessage(1, "Unknown death type!\n");
		break;
	}

	PevFloat(pev, PEV_IDEAL_YAW) = PevVector(pev, PEV_ANGLES).y;
	SetActivity(m_Activity);
	SetThink(&CBaseMonster::MonsterThink);
	SetNextThink(0.1f);
}

//=========================================================
// Killed
//
// Shared death/gib dispatch. Stops the looping sound, goes non-solid, and --
// when the monster has been blasted below -30 with the gib flag (0x20) set --
// sprays blood and coin-flips: an odd roll books the kill, flings the head as
// a MOVETYPE_TOSS prop and plays the death animation; an even roll plays the
// bodysplat sound, hides the body and showers human gibs before falling
// through to the normal death dispatch.
//
// The normal death dispatch clamps the corpse health to -99, then: FOLLOW
// (movetype 7) and zero-movetype corpses jump straight to Death(0); monsters
// book the kill and -- if still gibbed-dead (health <= -30) -- just stop
// thinking (nullsub think); otherwise they clear takedamage/touch and raise
// the Death virtual (binary slot 14) with type 0.
//
// The leading squad-chain fixup promotes a new leader when the leader dies,
// splices this monster out of the circular squad list, and rewrites the
// survivors' leader pointer.
//=========================================================
void CBaseMonster::Killed(int attackerIndex)
{
	int flags;
	void* globals;

	if (!pev)
		return;

	globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);

	if (m_iSquadSize > 1)
	{
		entvars_t* pNewLeader = m_pSquadLeader;
		entvars_t* pCurrent;
		unsigned int i;

		// Binary tests the +0x138 squad-leader flag (NOT the leader pointer) and
		// promotes by setting the next member's flag = 1 (lines 49-60).
		if (m_fSquadLeader != 0)
		{
			CBaseMonster* pPromoted = (CBaseMonster*)EngineGetPrivateData(EdictFromEntvars(m_pSquadNext));
			if (pPromoted)
				pPromoted->m_fSquadLeader = 1;

			pNewLeader = m_pSquadNext;
			EngineAlertMessage(1, "*\n");
		}

		pCurrent = pev;
		for (i = 0; i < m_iSquadSize && pCurrent; i++)
		{
			CBaseMonster* pMember = (CBaseMonster*)EngineGetPrivateData(EdictFromEntvars(pCurrent));
			if (!pMember)
				break;

			if (pMember->m_pSquadNext == pev)
				pMember->m_pSquadNext = m_pSquadNext;

			pCurrent = pMember->m_pSquadNext;
		}

		for (i = 0; i < m_iSquadSize && pCurrent; i++)
		{
			CBaseMonster* pMember = (CBaseMonster*)EngineGetPrivateData(EdictFromEntvars(pCurrent));
			if (!pMember)
				break;

			EngineAlertMessage(1, "-");
			pMember->m_pSquadLeader = pNewLeader;
			pCurrent = pMember->m_pSquadNext;
		}
	}

	// stop the looping idle/move sound (common/null.wav at full volume)
	{
		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, 1, "common/null.wav", 1.0f, 0.8f);
	}
	PevInt(pev, PEV_SOLID) = 0;

	flags = (int)PevFloat(pev, PEV_FLAGS);

	// gibbed: health driven below -30 (0xC1F00000) with the gib flag (0x20).
	if (PevFloat(pev, PEV_HEALTH) < -30.0f && (flags & 0x20) != 0)
	{
		// six short blood rays out and away from the damage direction
		GibBloodSpray(pev, globals, BloodColor());

		int roll = abs(rand());
		if ((roll & 1) == 1)
		{
			// odd roll: book the kill, then fling the head as a MOVETYPE_TOSS
			// prop and play the death animation.
			if ((flags & 0x20) != 0)
			{
				BookMonsterKill(globals);
			}

			PevInt(pev, PEV_TAKEDAMAGE) = 0;
			SetTouch((EntityFunc)NULL);
			PevVector(pev, PEV_ORIGIN).z += 1.0f;

			if (((int)PevFloat(pev, PEV_FLAGS) & 0x200) != 0)
			{
				PevFloat(pev, PEV_MOVETYPE) = 6.0f;
				PevFloat(pev, PEV_FLAGS) -= 512.0f;
				PevVector(pev, PEV_VELOCITY).x = g_vecAttackDir[0] * -400.0f;
				PevVector(pev, PEV_VELOCITY).y = g_vecAttackDir[1] * -400.0f;
				PevVector(pev, PEV_VELOCITY).z = g_vecAttackDir[2] * -400.0f;
			}
			Death(0);
			return;
		}

		// even roll: play the splat sound, hide the body and shower human
		// gibs. The binary always flings one random gib; the skull is added
		// only for flesh monsters (Classify 1/4/5).
		{
			edict_t* edict = EdictFromEntvars(pev);
			if (edict)
				EngineEmitSound(edict, 1, kSndBodySplat, 1.0f, 0.8f);
		}
		PevInt(pev, PEV_SOLID) = 0;
		PevInt(pev, PEV_MODEL) = 0;	// hide the gibbed body (binary zeroes pev->model @128)

		{
			int cls = Classify();
			BOOL isFlesh = (cls == 1 || cls == 5 || cls == 4);
			SpawnHumanGibs(pev, PevFloat(pev, PEV_HEALTH), BloodColor(), isFlesh);
		}
		// fall through to the corpse clamp + death-animation dispatch
	}

	// clamp corpse health to -99 (0xC2C60000) so it can't keep falling forever
	if (PevFloat(pev, PEV_HEALTH) < -99.0f)
		PevFloat(pev, PEV_HEALTH) = -99.0f;

	// MOVETYPE 7 (FOLLOW) corpses and zero-movetype gibs skip the death anim
	if (PevFloat(pev, PEV_MOVETYPE) == 7.0f || (PevInt(pev, PEV_MOVETYPE) & 0x7FFFFFFF) == 0)
	{
		Death(0);
		return;
	}

	PevInt(pev, PEV_ENEMY) = attackerIndex;

	if ((flags & 0x20) != 0)
	{
		// monster: book the kill, then -- if the corpse is still gibbed-dead
		// (health <= -30) -- stop thinking instead of playing a death anim.
		BookMonsterKill(globals);

		if (PevFloat(pev, PEV_HEALTH) <= -30.0f)
		{
			SetDoNothingThink();	// m_pfnThink = an empty stub
			SetNextThink(0.1f);
			return;
		}
	}

	PevInt(pev, PEV_TAKEDAMAGE) = 0;
	SetTouch((EntityFunc)NULL);
	Death(0);
}

//=========================================================
// Pain (binary vtable slot 13) - default: no pain reaction.
//=========================================================
void CBaseMonster::Pain(float flDamage)
{
	HL_UNUSED(flDamage);
}

//=========================================================
// Death (binary vtable slot 14) - default reproduces the binary
// (the shared death stub == SetDeathActivity(0)).
//=========================================================
void CBaseMonster::Death(int gibType)
{
	HL_UNUSED(gibType);
	SetDeathActivity(0);
}

void CBaseMonster::MonsterSlot18()
{
}
