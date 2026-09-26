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
// Turret - ceiling or floor mounted gun. It deploys when
// it spots an enemy, aims with two bone controllers and
// fires until the enemy is dead or out of sight.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"
#include "monsters.h"
#include "decals.h"
#include "studio.h"

// angle indices
#define PITCH	0
#define YAW		1
#define ROLL	2

#define TURRET_THINK_INTERVAL	0.1f
#define TURRET_DEPLOY_DELAY		0.4f
#define TURRET_SPAWN_DELAY		0.3f
#define TURRET_SEARCH_DELAY		0.3f
#define TURRET_SPINUP_DELAY		1.4f

#define TURRET_TURN_STEP		4.5f		// degrees per think for both controllers
#define TURRET_RANGE			1024.0f
#define TURRET_BULLET_DAMAGE	2.0f

#define TURRET_ORIENT_FLOOR		0
#define TURRET_ORIENT_CEILING	1

// bone controllers
#define TURRET_CONTROLLER_YAW	0
#define TURRET_CONTROLLER_PITCH	1

// turret.mdl sequences
enum
{
	TURRET_SEQ_IDLE = 0,	// retracted
	TURRET_SEQ_ACTIVE,
	TURRET_SEQ_DEPLOY,
	TURRET_SEQ_RETIRE,		// played backwards
};

//=========================================================
// SetBoneController - sets a controller of the model to
// the given angle
//=========================================================
static void SetBoneController(entvars_t *pev, int iController, float flValue)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)GET_MODEL_PTR(ENT(pev));
	if (!pstudiohdr)
		return;

	if (iController >= pstudiohdr->numbonecontrollers)
		return;

	mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *)((unsigned char *)pstudiohdr + pstudiohdr->bonecontrollerindex) + iController;

	float flStart = pbonecontroller->start;
	float flEnd = pbonecontroller->end;

	// wrap the angle of a rotational controller that doesn't go all the way around
	if ((pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
		&& flStart + 359.0f >= flEnd
		&& (flStart + flEnd) * 0.5f + 180.0f < flValue)
	{
		flValue = flValue - 360.0f;
	}

	int iSetting = (int)((flValue - flStart) / (flEnd - flStart) * 255.0f);
	if (iSetting < 0)
		iSetting = 0;
	if (iSetting > 255)
		iSetting = 255;

	pev->controller[iController] = iSetting;
}

//=========================================================
// Multi-damage: the turret's own copy of the bullet damage
// helpers. The damage of all bullets that hit the same
// entity in a row is added up and applied at once.
//=========================================================

static EOFFSET	g_eoffsetMultiDamage;
static float	g_flMultiDamage;

static void ClearMultiDamage()
{
	g_eoffsetMultiDamage = 0;
	g_flMultiDamage = 0.0f;
}

//=========================================================
// ApplyMultiDamage - deals the added up damage, and now and
// then splats blood on the wall behind the victim
//=========================================================
static void ApplyMultiDamage(entvars_t *pevInflictor)
{
	if (FNullEnt(g_eoffsetMultiDamage))
		return;

	edict_t *pentHit = ENT(g_eoffsetMultiDamage);
	entvars_t *pevHit = VARS(pentHit);
	CBaseEntity *pHit = CBaseEntity::Instance(pentHit);

	if (pHit)
		pHit->TakeDamage(pevInflictor, pevInflictor, g_flMultiDamage);

	if (FClassnameIs(pevHit, "cycler"))
		return;

	if (RANDOM_FLOAT(0.0f, 1.0f) >= 0.3f)
		return;

	UTIL_MakeVectors(pevInflictor->origin - pevHit->origin);

	Vector vecSrc = pevHit->origin;
	vecSrc.z += pevHit->size.z * 0.5f;

	float flRight = RANDOM_FLOAT(-1.0f, 1.0f) * 0.45f;
	float flUp = RANDOM_FLOAT(-1.0f, 1.0f) * 0.45f;
	Vector vecDir = -g_vecAttackDir + gpGlobals->v_up * flUp + gpGlobals->v_right * flRight;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 128.0f, ignore_monsters, pentHit, &tr);

	if (tr.flFraction != 1.0f)
	{
		int iBloodColor = pHit ? pHit->BloodColor() : 0;

		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_DECAL);
		WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.x);
		WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.y);
		WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.z);
		WRITE_SHORT(MSG_BROADCAST, ENTINDEX(tr.pHit));

		if (iBloodColor == BLOOD_COLOR_RED)
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_BLOOD1, DECAL_BLOOD6));
		else
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_YBLOOD1, DECAL_YBLOOD6));
	}
}

static void AddMultiDamage(entvars_t *pevInflictor, EOFFSET eoffsetHit, float flDamage)
{
	if (FNullEnt(eoffsetHit))
		return;

	if (eoffsetHit == g_eoffsetMultiDamage)
	{
		g_flMultiDamage += flDamage;
		return;
	}

	ApplyMultiDamage(pevInflictor);
	g_eoffsetMultiDamage = eoffsetHit;
	g_flMultiDamage = flDamage;
}

static void SpawnBlood(const Vector &vecSpot, int iBloodColor, float flAmount)
{
	if (flAmount > 255.0f)
		flAmount = 255.0f;

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_BLOOD);
	WRITE_COORD(MSG_BROADCAST, vecSpot.x);
	WRITE_COORD(MSG_BROADCAST, vecSpot.y);
	WRITE_COORD(MSG_BROADCAST, vecSpot.z);
	WRITE_COORD(MSG_BROADCAST, g_vecAttackDir.x);
	WRITE_COORD(MSG_BROADCAST, g_vecAttackDir.y);
	WRITE_COORD(MSG_BROADCAST, g_vecAttackDir.z);
	WRITE_BYTE(MSG_BROADCAST, (unsigned char)iBloodColor);
	WRITE_BYTE(MSG_BROADCAST, (int)flAmount);
}

//=========================================================
// ImpactEffects - damage, blood and the bullet hole of one
// bullet
//=========================================================
static void ImpactEffects(entvars_t *pevInflictor, float flDamage, BOOL fSkipEffects, const Vector &vecDir, TraceResult *ptr)
{
	EOFFSET eoffsetHit = ptr->pHit;
	edict_t *pentHit = ENT(eoffsetHit);
	entvars_t *pevHit = VARS(pentHit);

	Vector vecHit = ptr->vecEndPos - vecDir * 4.0f;

	if (pevHit->takedamage != DAMAGE_NO)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(pentHit);
		int iBloodColor = pEntity ? pEntity->BloodColor() : 0;

		AddMultiDamage(pevInflictor, eoffsetHit, flDamage);

		if (FClassnameIs(pevHit, "func_glass") || FClassnameIs(pevHit, "func_breakable"))
			return;

		// very tough things don't bleed
		if (pevHit->health > 1000.0f)
			return;

		SpawnBlood(vecHit, iBloodColor, flDamage);

		// a stream of blood for a killing shot
		if (pevHit->health <= g_flMultiDamage)
		{
			Vector vecSpot = ptr->vecEndPos + vecDir * 8.0f;

			WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(MSG_BROADCAST, TE_BLOODSTREAM);
			WRITE_COORD(MSG_BROADCAST, vecSpot.x);
			WRITE_COORD(MSG_BROADCAST, vecSpot.y);
			WRITE_COORD(MSG_BROADCAST, vecSpot.z);
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(0.0f, 1.0f));
			WRITE_BYTE(MSG_BROADCAST, (unsigned char)iBloodColor);
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(80, 150));	// speed
		}
	}

	if (!fSkipEffects && pevHit->solid == SOLID_BSP)
	{
		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_GUNSHOT);
		WRITE_COORD(MSG_BROADCAST, vecHit.x);
		WRITE_COORD(MSG_BROADCAST, vecHit.y);
		WRITE_COORD(MSG_BROADCAST, vecHit.z);

		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_DECAL);
		WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.x);
		WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.y);
		WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.z);
		WRITE_SHORT(MSG_BROADCAST, ENTINDEX(ptr->pHit));

		// glass cracks on see-through brushes
		if (!FNullEnt(eoffsetHit) && pevHit->rendermode != kRenderNormal)
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_BREAK1, DECAL_BREAK3));
		else
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_SHOT1, DECAL_SHOT5));
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
	void KeyValue(KeyValueData *pkvd);
	int Classify();
	void Use(CBaseEntity *pOther);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage);
	void Death(int iDeathType);

	void InitialThink(CBaseEntity *pOther);
	void AutoSearchThink(CBaseEntity *pOther);
	void DeployThink(CBaseEntity *pOther);
	void RetractThink(CBaseEntity *pOther);
	void ActiveThink(CBaseEntity *pOther);
	void SpinUpThink(CBaseEntity *pOther);
	void FireThink(CBaseEntity *pOther);

	void SetTurretAnim(int iSequence);
	void MoveTurret();
	edict_t *FindEnemy();

	int		m_iOn;
	int		m_iAutoStart;
	int		m_fBerserk;				// badly damaged, fires wildly
	int		m_iOrientation;			// TURRET_ORIENT_*
	int		m_iTurnRate;
	float	m_flMaxSpin;			// how long the turret keeps firing at an enemy out of sight
	float	m_flSearchSpeed;
	float	m_flLastSight;			// time to give up on an enemy out of sight
	float	m_flPingTime;			// time of the next ping
	int		m_iTracerCount;
	float	m_flBaseYaw;			// yaw the turret was placed with

	float	m_vecGoalAngles[2];		// pitch and yaw to turn to
	float	m_vecCurAngles[3];
	Vector	m_vecLastSight;
};

CTurret::CTurret()
{
	m_iOn = 0;
	m_iAutoStart = 0;
	m_fBerserk = 0;
	m_iOrientation = TURRET_ORIENT_FLOOR;
	m_iTurnRate = 10;
	m_flMaxSpin = 5.0f;
	m_flSearchSpeed = 0.0f;
	m_flLastSight = 0.0f;
	m_flPingTime = 0.0f;
	m_iTracerCount = 0;
	m_flBaseYaw = 0.0f;

	m_vecGoalAngles[PITCH] = 0.0f;
	m_vecGoalAngles[YAW] = 0.0f;
	m_vecCurAngles[PITCH] = 0.0f;
	m_vecCurAngles[YAW] = 0.0f;
	m_vecCurAngles[ROLL] = 0.0f;
	m_vecLastSight = g_vecZero;
}

void CTurret::Spawn()
{
	PRECACHE_SOUND("turret/tu_fire1.wav");
	PRECACHE_SOUND("turret/tu_ping.wav");
	PRECACHE_SOUND("turret/tu_active.wav");
	PRECACHE_SOUND("turret/tu_die.wav");
	PRECACHE_SOUND("turret/tu_die2.wav");
	PRECACHE_SOUND("turret/tu_die3.wav");
	PRECACHE_SOUND("turret/tu_retract.wav");
	PRECACHE_SOUND("turret/tu_deploy.wav");
	PRECACHE_SOUND("turret/tu_spinup.wav");
	PRECACHE_SOUND("turret/tu_spindown.wav");
	PRECACHE_SOUND("turret/tu_search.wav");
	PRECACHE_SOUND("turret/tu_alert.wav");

	PRECACHE_MODEL("models/turret.mdl");

	SET_MODEL(ENT(pev), "models/turret.mdl");
	UTIL_SetSize(pev, Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 16.0f));

	m_MonsterState = MONSTERSTATE_IDLE;

	pev->nextthink = pev->ltime + 1.0f;
	pev->movetype = MOVETYPE_FLY;
	pev->sequence = TURRET_SEQ_IDLE;
	pev->frame = 0.0f;
	pev->solid = SOLID_SLIDEBOX;
	pev->health = 100.0f;
	pev->takedamage = DAMAGE_AIM;
	pev->flags = (float)((int)pev->flags | FL_MONSTER);

	m_iOn = 0;
	m_fBerserk = 0;
	m_iTurnRate = 10;
	m_flMaxSpin = 5.0f;

	SetUse(&CTurret::Use);

	// the controllers do the turning, the model keeps yaw 0
	m_flBaseYaw = pev->angles.y;
	pev->angles.y = 0.0f;

	SetThink(&CTurret::InitialThink);
	pev->nextthink = gpGlobals->time + TURRET_SPAWN_DELAY;
}

void CTurret::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "maxsleep"))
	{
		m_flMaxSpin = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "orientation"))
	{
		m_iOrientation = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "searchspeed"))
	{
		m_flSearchSpeed = (float)atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "autostart"))
	{
		m_iAutoStart = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "turnrate"))
	{
		m_iTurnRate = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "style")
		|| FStrEq(pkvd->szKeyName, "height")
		|| FStrEq(pkvd->szKeyName, "killtarget")
		|| FStrEq(pkvd->szKeyName, "value1")
		|| FStrEq(pkvd->szKeyName, "value2")
		|| FStrEq(pkvd->szKeyName, "value3"))
	{
		pkvd->fHandled = TRUE;
	}
}

int CTurret::Classify()
{
	return CLASS_MACHINE;
}

void CTurret::SetTurretAnim(int iSequence)
{
	if (pev->sequence == iSequence)
		return;

	pev->sequence = iSequence;
	ResetSequenceInfo(TURRET_THINK_INTERVAL);

	if (iSequence == TURRET_SEQ_RETIRE)
	{
		pev->frame = 255.0f;
		pev->framerate = -1.0f;
	}
	else
	{
		pev->frame = 0.0f;
		pev->framerate = 1.0f;
	}
}

//=========================================================
// InitialThink - turns the model to its placement angles
// and waits, or starts searching when set to autostart
//=========================================================
void CTurret::InitialThink(CBaseEntity *pOther)
{
	if (m_iOrientation == TURRET_ORIENT_CEILING)
	{
		// upside down
		pev->idealpitch = 180.0f;
		pev->pitch_speed = 360.0f;
		CHANGE_PITCH(ENT(pev));
		pev->ideal_yaw = -m_flBaseYaw;
	}
	else
	{
		pev->ideal_yaw = m_flBaseYaw;
	}

	pev->yaw_speed = 360.0f;
	CHANGE_YAW(ENT(pev));

	m_vecGoalAngles[PITCH] = 360.0f;

	if (m_iAutoStart)
	{
		m_iOn = 1;
		SetThink(&CTurret::AutoSearchThink);
		pev->nextthink = gpGlobals->time + TURRET_THINK_INTERVAL;
	}
	else
	{
		SetThink(&CBaseEntity::SUB_DoNothing);
	}
}

//=========================================================
// AutoSearchThink - looks for an enemy, deploys when it
// finds one
//=========================================================
void CTurret::AutoSearchThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + TURRET_SEARCH_DELAY;

	// forget a dead enemy
	if (!FNullEnt(pev->enemy) && VARS(pev->enemy)->health < 0.0f)
		pev->enemy = 0;

	if (pev->enemy)
		return;

	edict_t *pentFound = FindEnemy();
	if (FNullEnt(pentFound))
		return;

	SetThink(&CTurret::DeployThink);
	pev->nextthink = gpGlobals->time + TURRET_DEPLOY_DELAY;

	EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_alert.wav", VOL_NORM, ATTN_NORM);
}

//=========================================================
// FindEnemy - picks a visible client in the PVS, or the
// first living enemy in range, as pev->enemy
//=========================================================
edict_t *CTurret::FindEnemy()
{
	edict_t *pentTarget = NULL;

	edict_t *pentClient = FIND_CLIENT_IN_PVS();
	if (!FNullEnt(pentClient) && FVisible(pev, VARS(pentClient)))
		pentTarget = pentClient;

	if (!pentTarget)
	{
		edict_t *pentEntity = FIND_ENTITY_IN_SPHERE(pev->origin, TURRET_RANGE);

		while (!FNullEnt(pentEntity))
		{
			entvars_t *pevEntity = VARS(pentEntity);

			if (pevEntity->takedamage != DAMAGE_NO)
			{
				CBaseEntity *pEntity = CBaseEntity::Instance(pentEntity);
				int iClass = pEntity ? pEntity->Classify() : CLASS_NONE;

				switch (iClass)
				{
				case CLASS_HUMAN_MILITARY:
				case CLASS_ALIEN_MILITARY:
				case CLASS_HOUNDEYE:
				case CLASS_PLAYER:
				case CLASS_PLAYER_ALLY:
				case CLASS_BULLCHICKEN:
				case CLASS_HEADCRAB:
				{
					// the result isn't used
					TraceResult tr;
					UTIL_TraceLine(pev->origin, pevEntity->origin, dont_ignore_monsters, ENT(pev), &tr);

					if (pevEntity->health > 0.0f && pevEntity->health < CYCLER_HEALTH)
					{
						pentTarget = pentEntity;
						pentEntity = NULL;
					}
					break;
				}
				default:
					break;
				}
			}

			if (pentEntity)
				pentEntity = ENT(pevEntity->chain);
		}
	}

	if (FNullEnt(pentTarget))
		return NULL;

	entvars_t *pevTarget = VARS(pentTarget);

	if (!FVisible(pev, pevTarget) || ((int)pevTarget->flags & FL_NOTARGET))
		return NULL;

	if ((int)pev->spawnflags & SF_MONSTER_WAIT_TILL_SEEN)
	{
		if (!FInViewCone(pevTarget, pev, 0.7f))
			return NULL;
	}

	EOFFSET eoffsetTarget = OFFSET(pentTarget);
	pev->enemy = eoffsetTarget;
	pev->goalentity = eoffsetTarget;

	m_vecEnemyLKP = pevTarget->origin;
	m_flLastEnemySightTime = gpGlobals->time;

	return pentTarget;
}

//=========================================================
// DeployThink - raises the gun out of its housing
//=========================================================
void CTurret::DeployThink(CBaseEntity *pOther)
{
	SetTurretAnim(TURRET_SEQ_DEPLOY);
	pev->nextthink = gpGlobals->time + TURRET_THINK_INTERVAL;
	AdvanceAnimation(TURRET_THINK_INTERVAL);

	EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_deploy.wav", VOL_NORM, ATTN_NORM);

	if (m_fSequenceFinished)
	{
		UTIL_SetSize(pev, Vector(-16.0f, -16.0f, 0.0f), Vector(34.0f, 34.0f, 34.0f));

		m_vecCurAngles[YAW] = m_flBaseYaw;
		m_vecCurAngles[PITCH] = 360.0f;

		SetTurretAnim(TURRET_SEQ_ACTIVE);
		SetThink(&CTurret::ActiveThink);
	}
}

//=========================================================
// RetractThink - levels the gun, then pulls it back into
// its housing
//=========================================================
void CTurret::RetractThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + TURRET_THINK_INTERVAL;
	AdvanceAnimation(TURRET_THINK_INTERVAL);

	if (m_vecGoalAngles[PITCH] != m_vecCurAngles[PITCH])
	{
		MoveTurret();
		return;
	}

	if (pev->sequence != TURRET_SEQ_RETIRE)
	{
		SetTurretAnim(TURRET_SEQ_RETIRE);
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_retract.wav", VOL_NORM, ATTN_NORM);
	}

	if (m_fSequenceFinished)
	{
		SetTurretAnim(TURRET_SEQ_IDLE);
		UTIL_SetSize(pev, Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 16.0f));

		SetThink(&CBaseEntity::SUB_DoNothing);
	}
}

//=========================================================
// MoveTurret - turns the controllers a step towards the
// goal angles
//=========================================================
void CTurret::MoveTurret()
{
	if (m_vecGoalAngles[PITCH] != m_vecCurAngles[PITCH])
	{
		float flDir = (m_vecGoalAngles[PITCH] > m_vecCurAngles[PITCH]) ? 1.0f : -1.0f;
		m_vecCurAngles[PITCH] = m_vecCurAngles[PITCH] + TURRET_TURN_STEP * flDir;

		// don't overshoot
		if (flDir == 1.0f)
		{
			if (m_vecCurAngles[PITCH] > m_vecGoalAngles[PITCH])
				m_vecCurAngles[PITCH] = m_vecGoalAngles[PITCH];
		}
		else
		{
			if (m_vecCurAngles[PITCH] < m_vecGoalAngles[PITCH])
				m_vecCurAngles[PITCH] = m_vecGoalAngles[PITCH];
		}

		SetBoneController(pev, TURRET_CONTROLLER_PITCH, m_vecCurAngles[PITCH]);
	}

	if (m_vecGoalAngles[YAW] == m_vecCurAngles[YAW])
		return;

	// turn the short way around
	float flDir = (m_vecGoalAngles[YAW] > m_vecCurAngles[YAW]) ? 1.0f : -1.0f;
	if (fabsf(m_vecGoalAngles[YAW] - m_vecCurAngles[YAW]) > 180.0f)
		flDir = -flDir;

	m_vecCurAngles[YAW] = m_vecCurAngles[YAW] + flDir * TURRET_TURN_STEP;

	if (m_vecCurAngles[YAW] < 0.0f)
		m_vecCurAngles[YAW] = m_vecCurAngles[YAW] + 360.0f;
	else if (m_vecCurAngles[YAW] > 360.0f)
		m_vecCurAngles[YAW] = m_vecCurAngles[YAW] - 360.0f;

	// close enough, snap to the goal
	if (fabsf(m_vecCurAngles[YAW] - m_vecGoalAngles[YAW]) < TURRET_TURN_STEP / 2)
		m_vecCurAngles[YAW] = m_vecGoalAngles[YAW];

	float flValue = m_vecCurAngles[YAW] - m_flBaseYaw + 360.0f;
	if (flValue > 360.0f)
		flValue = flValue - 360.0f;

	SetBoneController(pev, TURRET_CONTROLLER_YAW, flValue);
}

//=========================================================
// ActiveThink - deployed and searching, pings once a
// second. Spins up when it has an enemy.
//=========================================================
void CTurret::ActiveThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + TURRET_THINK_INTERVAL;

	float flTime = gpGlobals->time;
	if (m_flPingTime != 0.0f)
	{
		if (flTime >= m_flPingTime)
		{
			m_flPingTime = 0.0f;
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_ping.wav", VOL_NORM, ATTN_IDLE);
		}
	}
	else
	{
		m_flPingTime = flTime + 1.0f;
	}

	// forget a dead enemy
	if (!FNullEnt(pev->enemy) && VARS(pev->enemy)->health < 0.0f)
		pev->enemy = 0;

	if (!pev->enemy)
		FindEnemy();

	if (pev->enemy)
	{
		SetThink(&CTurret::SpinUpThink);
		pev->nextthink = gpGlobals->time + TURRET_SPINUP_DELAY;

		EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_spinup.wav", VOL_NORM, ATTN_NORM);
	}
}

void CTurret::SpinUpThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + TURRET_THINK_INTERVAL;

	EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_active.wav", VOL_NORM, ATTN_NORM);

	AdvanceAnimation(TURRET_THINK_INTERVAL);

	SetThink(&CTurret::FireThink);
}

//=========================================================
// FireThink - aims at the enemy and fires two bullets a
// think. Spins down when the enemy is dead or was out of
// sight for too long.
//=========================================================
void CTurret::FireThink(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + TURRET_THINK_INTERVAL;
	AdvanceAnimation(TURRET_THINK_INTERVAL);

	if (!m_iOn || !pev->enemy)
	{
		pev->nextthink = gpGlobals->time + 1.0f;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_spindown.wav", VOL_NORM, ATTN_NORM);
		SetThink(&CTurret::ActiveThink);
		return;
	}

	entvars_t *pevEnemy = VARS(pev->enemy);
	Vector vecOrigin = pev->origin;

	if (pevEnemy->health <= 0.0f || pevEnemy->health == CYCLER_HEALTH)
	{
		pev->enemy = 0;
		pev->nextthink = gpGlobals->time + 1.0f;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_spindown.wav", VOL_NORM, ATTN_NORM);
		SetThink(&CTurret::ActiveThink);
		return;
	}

	Vector vecMid = pevEnemy->origin;

	BOOL fVisible = FVisible(pev, pevEnemy);
	if (fVisible)
	{
		m_flLastSight = 0.0f;
		m_vecLastSight = pevEnemy->origin;
	}
	else
	{
		float flTime = gpGlobals->time;
		if (m_flLastSight != 0.0f)
		{
			if (flTime > m_flLastSight)
			{
				// out of sight for too long
				m_flLastSight = 0.0f;
				pev->enemy = 0;
				pev->nextthink = gpGlobals->time + 1.0f;
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_spindown.wav", VOL_NORM, ATTN_NORM);
				SetThink(&CTurret::ActiveThink);
				return;
			}
		}
		else
		{
			m_flLastSight = flTime + m_flMaxSpin;
		}
	}

	// shoot from half the model height
	float flHeight = pev->maxs.z - pev->mins.z;
	if (m_iOrientation == TURRET_ORIENT_CEILING)
		vecOrigin.z = vecOrigin.z + flHeight * -0.5f;
	else
		vecOrigin.z = vecOrigin.z + flHeight * 0.5f;

	// aim at the middle of the enemy, at the origin of players
	if (!FClassnameIs(pevEnemy, "player"))
		vecMid.z = vecMid.z + (pevEnemy->maxs.z - pevEnemy->mins.z) * 0.5f;

	Vector vecDirToEnemy = vecMid - vecOrigin;
	float flDist = vecDirToEnemy.Length();

	Vector vecAngToEnemy = UTIL_VecToAngles(vecDirToEnemy);
	float flAimPitch = vecAngToEnemy.x;
	float flAimYaw = vecAngToEnemy.y;

	float flCurPitch = m_vecCurAngles[PITCH];
	float flCurYaw = m_vecCurAngles[YAW];
	float flCurRoll = m_vecCurAngles[ROLL];

	BOOL fShoot = FALSE;
	if (fVisible && flDist < TURRET_RANGE)
		fShoot = TRUE;

	// the bullets go where the barrel points
	if (fShoot || m_fBerserk)
	{
		if (m_iOrientation == TURRET_ORIENT_CEILING)
		{
			flCurYaw = 180.0f - flCurYaw;
			flCurPitch = -flCurPitch;
		}

		UTIL_MakeVectors(Vector(flCurPitch, flCurYaw, flCurRoll));
	}

	// only shoot when the barrel points at the enemy
	if (fShoot && DotProduct(gpGlobals->v_forward, vecDirToEnemy.Normalize()) <= 0.995f)
		fShoot = FALSE;

	if (fShoot || m_fBerserk)
	{
		SetTurretAnim(TURRET_SEQ_ACTIVE);

		Vector vecSrc = gpGlobals->v_forward * 10.0f + vecOrigin;
		vecSrc.z = vecOrigin.z - (pev->view_ofs.z - 8.0f);

		ClearMultiDamage();

		for (int iShot = 0; iShot < 2; iShot++)
		{
			float flSpreadRight = RANDOM_FLOAT(-1.0f, 1.0f) * 0.035f;
			Vector vecDir = vecDirToEnemy.Normalize();
			float flSpreadUp = RANDOM_FLOAT(-1.0f, 1.0f) * 0.035f;

			Vector vecShot = vecDir + gpGlobals->v_right * flSpreadRight + gpGlobals->v_up * flSpreadUp;

			TraceResult tr;
			UTIL_TraceLine(vecSrc, vecSrc + vecShot * TURRET_RANGE, dont_ignore_monsters, ENT(pev), &tr);

			// every fourth bullet draws a tracer and leaves no hole
			BOOL fTracer = FALSE;
			m_iTracerCount = (m_iTracerCount + 1) % 4;
			if (m_iTracerCount == 3)
			{
				fTracer = TRUE;

				WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
				WRITE_BYTE(MSG_BROADCAST, TE_TRACER);
				WRITE_COORD(MSG_BROADCAST, vecSrc.x);
				WRITE_COORD(MSG_BROADCAST, vecSrc.y);
				WRITE_COORD(MSG_BROADCAST, vecSrc.z);
				WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.x);
				WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.y);
				WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.z);
			}

			if (tr.flFraction != 1.0f)
				ImpactEffects(pev, TURRET_BULLET_DAMAGE, fTracer, vecShot, &tr);
		}

		ApplyMultiDamage(pev);

		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "turret/tu_fire1.wav", VOL_NORM, ATTN_NORM);

		// berserk: shake the gun around instead of aiming
		if (m_fBerserk)
		{
			float flYaw = (float)(255 * rand() / RAND_MAX);
			SetBoneController(pev, TURRET_CONTROLLER_YAW, flYaw);
			float flPitch = (float)(85 * rand() / -RAND_MAX);
			SetBoneController(pev, TURRET_CONTROLLER_PITCH, flPitch);

			MoveTurret();
			return;
		}
	}

	if (fVisible)
	{
		// aim at the enemy
		float flYaw = flAimYaw;
		float flPitch = flAimPitch;

		if (m_iOrientation == TURRET_ORIENT_CEILING)
			flYaw = 180.0f - flYaw;
		if (flYaw > 360.0f)
			flYaw = flYaw - 360.0f;
		if (flYaw < 0.0f)
			flYaw = flYaw + 360.0f;

		if (m_iOrientation != TURRET_ORIENT_CEILING)
			flPitch = -flPitch;
		if (flPitch < 0.0f)
			flPitch = flPitch + 360.0f;
		if (flPitch > 360.0f)
			flPitch = flPitch - 360.0f;

		// the gun only pitches between 275 and 360
		if (flPitch >= 135.0f)
		{
			if (flPitch < 275.0f)
				flPitch = 275.0f;
		}
		else
		{
			flPitch = 360.0f;
		}

		m_vecGoalAngles[YAW] = flYaw;
		m_vecGoalAngles[PITCH] = flPitch;
	}

	MoveTurret();
}

//=========================================================
// Use - switches the turret on (deploy) or off (retract)
//=========================================================
void CTurret::Use(CBaseEntity *pOther)
{
	pev->nextthink = gpGlobals->time + TURRET_THINK_INTERVAL;

	if (m_iOn)
	{
		m_iOn = 0;
		m_flLastSight = 0.0f;
		pev->enemy = 0;

		m_vecCurAngles[YAW] = m_vecGoalAngles[YAW];
		m_vecGoalAngles[PITCH] = 360.0f;

		SetThink(&CTurret::RetractThink);
	}
	else
	{
		m_iOn = 1;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_alert.wav", VOL_NORM, ATTN_NORM);

		SetThink(&CTurret::DeployThink);
		pev->nextthink = gpGlobals->time + TURRET_DEPLOY_DELAY;
	}
}

void CTurret::Death(int iDeathType)
{
	SetThink(&CBaseEntity::SUB_DoNothing);
}

//=========================================================
// TakeDamage - a badly damaged turret may go berserk
//=========================================================
int CTurret::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage)
{
	pev->health = pev->health - flDamage;

	if (pev->health > 0.0f)
	{
		if (pev->health <= 10.0f)
		{
			if (rand() > 800)
			{
				m_fBerserk = 1;
				SetTurretAnim(TURRET_SEQ_ACTIVE);
				SetThink(&CTurret::ActiveThink);
			}
		}

		return 1;
	}

	float flRandom = RANDOM_FLOAT(0.0f, 1.0f);
	const char *pszSound;

	if (flRandom <= 0.33f)
		pszSound = "turret/tu_die.wav";
	else if (flRandom <= 0.66f)
		pszSound = "turret/tu_die2.wav";
	else
		pszSound = "turret/tu_die3.wav";

	EMIT_SOUND(ENT(pev), CHAN_ITEM, pszSound, VOL_NORM, ATTN_NORM);

	SetTurretAnim(TURRET_SEQ_IDLE);

	pev->flags = (float)((int)pev->flags & ~FL_MONSTER);

	edict_t *pentAttacker = pevAttacker ? ENT(pevAttacker) : NULL;
	Killed(OFFSET(pentAttacker));

	return 1;
}

LINK_ENTITY_TO_CLASS(monster_turret, CTurret);
