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
// Combat - damage, death, gibs and bullets
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"
#include "weapons.h"
#include "decals.h"

// pev->items armor bits
#define IT_ARMOR1			(1<<13)
#define IT_ARMOR2			(1<<14)
#define IT_ARMOR3			(1<<15)

#define BULLET_DAMAGE		2.0f
#define CORPSE_MIN_HEALTH	-99.0f

Vector g_vecAttackDir;

// damage of all the bullets that hit the same entity, applied at once
typedef struct
{
	EOFFSET	eoffsetEntity;
	float	amount;
} MULTIDAMAGE;

static MULTIDAMAGE gMultiDamage;

// every fourth monster bullet draws a tracer
static int g_iTracerCount;

//=========================================================
// BloodDecalTrace - blood splat where the trace ended
//=========================================================
static void BloodDecalTrace(TraceResult *ptr, int bloodColor)
{
	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_DECAL);
	WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.x);
	WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.y);
	WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.z);
	WRITE_SHORT(MSG_BROADCAST, ENTINDEX(ptr->pHit));

	if (bloodColor == BLOOD_COLOR_RED)
		WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_BLOOD1, DECAL_BLOOD6));
	else
		WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_YBLOOD1, DECAL_YBLOOD6));
}

//=========================================================
// Gibs - body parts of a monster that was blown apart
//=========================================================
class CGib : public CBaseEntity
{
public:
	void Spawn(entvars_t *pevVictim, const char *szGibModel, float flHealth, int bloodColor);
	void Touch(CBaseEntity *pOther);
	void StartFadeOut(CBaseEntity *pOther);
	void FadeOut(CBaseEntity *pOther);

	static void SpawnRandomGibs(entvars_t *pevVictim, float flHealth, int bloodColor, BOOL fHuman);

	int		m_bloodColor;
};

void CGib::Spawn(entvars_t *pevVictim, const char *szGibModel, float flHealth, int bloodColor)
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->renderamt = 255;
	pev->rendermode = kRenderNormal;
	pev->renderfx = 0;
	pev->solid = SOLID_SLIDEBOX;

	SET_MODEL(ENT(pev), szGibModel);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	// somewhere inside the victim
	pev->origin.x = RANDOM_FLOAT(0.0f, 1.0f) * pevVictim->size.x + pevVictim->absmin.x;
	pev->origin.y = RANDOM_FLOAT(0.0f, 1.0f) * pevVictim->size.y + pevVictim->absmin.y;
	pev->origin.z = RANDOM_FLOAT(0.0f, 1.0f) * pevVictim->size.z + pevVictim->absmin.z;

	Vector vecVelocity;
	vecVelocity.z = RANDOM_FLOAT(200.0f, 300.0f);
	vecVelocity.y = RANDOM_FLOAT(-100.0f, 100.0f);
	vecVelocity.x = RANDOM_FLOAT(-100.0f, 100.0f);

	// the more damage, the faster the gibs fly
	float flScale;
	if (flHealth > -50.0f)
		flScale = 0.7f;
	else if (flHealth > -200.0f)
		flScale = 2.0f;
	else
		flScale = 10.0f;

	pev->velocity = vecVelocity * flScale;
	pev->avelocity.y = RANDOM_FLOAT(100.0f, 300.0f);

	m_bloodColor = bloodColor;

	pev->nextthink = gpGlobals->time + 10.0f;
	SetThink(&CGib::StartFadeOut);
}

//=========================================================
// Touch - bleeds where it lands and stops spinning once
// it lies still
//=========================================================
void CGib::Touch(CBaseEntity *pOther)
{
	TraceResult tr;

	Vector vecStart = pev->origin;
	vecStart.z += 8.0f;
	Vector vecEnd = vecStart;
	vecEnd.z -= 24.0f;

	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev), &tr);
	BloodDecalTrace(&tr, m_bloodColor);

	if (pev->velocity == g_vecZero)
	{
		pev->avelocity = g_vecZero;
		pev->solid = SOLID_NOT;
	}
}

void CGib::StartFadeOut(CBaseEntity *pOther)
{
	pev->rendermode = kRenderTransTexture;
	pev->solid = SOLID_NOT;
	pev->avelocity = g_vecZero;

	pev->nextthink = gpGlobals->time + 0.1f;
	SetThink(&CGib::FadeOut);
}

void CGib::FadeOut(CBaseEntity *pOther)
{
	if (pev->renderamt != 0)
	{
		pev->renderamt -= 5.0f;
		pev->nextthink = gpGlobals->time + 0.1f;
	}
	else
	{
		pev->nextthink = gpGlobals->time + 5.0f;
		SetThink(&CBaseEntity::SUB_Remove);
	}
}

//=========================================================
// SpawnRandomGibs - a skull for humans, plus one random gib
//=========================================================
void CGib::SpawnRandomGibs(entvars_t *pevVictim, float flHealth, int bloodColor, BOOL fHuman)
{
	if (fHuman)
	{
		CGib *pSkull = GetClassPtr((CGib *)NULL);
		pSkull->Spawn(pevVictim, "models/gib_skull.mdl", flHealth, bloodColor);
	}
	else
	{
		// no skull, but the random number is used up all the same
		rand();
	}

	CGib *pGib = GetClassPtr((CGib *)NULL);
	const char *szGibModel;

	if (rand() % 15 == 0)
	{
		szGibModel = "models/gib_legbone.mdl";
	}
	else
	{
		int iGib = rand() % 3;

		if (iGib == 0)
			szGibModel = "models/gib_lung.mdl";
		else if (iGib == 1)
			szGibModel = "models/gib_b_gib.mdl";
		else
			szGibModel = "models/gib_b_bone.mdl";
	}

	pGib->Spawn(pevVictim, szGibModel, flHealth, bloodColor);
}

//=========================================================
// SpawnBloodSpray - blood splashes on the walls behind a
// monster that was blown apart
//=========================================================
static void SpawnBloodSpray(entvars_t *pev, int bloodColor)
{
	UTIL_MakeVectors(g_vecAttackDir);

	for (int i = 0; i < 6; i++)
	{
		float flRight = RANDOM_FLOAT(-1.0f, 1.0f) * 0.35f;
		float flUp = RANDOM_FLOAT(-1.0f, 1.0f) * 0.35f;
		Vector vecDir = -g_vecAttackDir + gpGlobals->v_right * flRight + gpGlobals->v_up * flUp;

		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin + vecDir * 256.0f, ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0f)
		{
			BloodDecalTrace(&tr, bloodColor);

			WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(MSG_BROADCAST, TE_BLOOD);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.x);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.y);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.z);
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(0.0f, 1.0f));
			WRITE_BYTE(MSG_BROADCAST, (unsigned char)bloodColor);
			WRITE_BYTE(MSG_BROADCAST, 15);		// speed
		}
	}
}

static void CountMonsterKill()
{
	gpGlobals->killed_monsters += 1;
	WRITE_BYTE(MSG_ALL, SVC_KILLEDMONSTER);
}

//=========================================================
// TakeDamage - armor absorbs part of the damage, the rest
// is taken from health. Monsters turn on their attacker.
//=========================================================
int CBaseMonster::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage)
{
	if (pev->takedamage == DAMAGE_NO)
		return 0;

	Vector vecCenter = pev->size * 0.5f + pev->absmin;
	g_vecAttackDir = (pevInflictor->origin - vecCenter).Normalize();

	float flSave = (float)ceil(pev->armortype * flDamage);
	if (pev->armorvalue <= flSave)
	{
		// armor is used up
		flSave = pev->armorvalue;
		pev->armortype = 0;
		pev->items &= ~(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3);
	}

	pev->armorvalue -= flSave;
	float flTake = (float)ceil(flDamage - flSave);

	int flags = (int)pev->flags;

	// the client shows the damage it took
	if (flags & FL_CLIENT)
	{
		pev->dmg_take += flTake;
		pev->dmg_save += flSave;
		pev->dmg_inflictor = OFFSET(pevInflictor);
	}

	// push players away from the inflictor
	if (!FNullEnt(pevInflictor) && pev->movetype == MOVETYPE_WALK && pevAttacker && pevAttacker->solid != SOLID_TRIGGER)
	{
		Vector vecDir = (pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5f).Normalize();
		pev->velocity += vecDir * flDamage * 8.0f;
	}

	if (FClassnameIs(pev, "player"))
	{
		if (flags & FL_GODMODE)
			return 1;

		// no damage from teammates
		if (gpGlobals->teamplay == 1 && pev->team > 0 && pevAttacker && pevAttacker->team == pev->team)
			return 1;
	}

	pev->health -= flTake;

	if (pev->health <= 0)
	{
		Killed(pevAttacker ? OFFSET(pevAttacker) : 0);
		return 1;
	}

	// go after a player or a monster of another class that hurt us
	if ((flags & FL_MONSTER) && !FNullEnt(pevAttacker) && ((int)pevAttacker->flags & (FL_CLIENT | FL_MONSTER)))
	{
		CBaseEntity *pAttacker = Instance(pevAttacker);

		if (!pAttacker || Classify() != pAttacker->Classify())
		{
			pev->goalentity = OFFSET(pevAttacker);
			pev->enemy = pev->goalentity;
			m_pevAttacker = pevAttacker;

			// guess where the attacker is
			m_vecAttackerLKP = g_vecAttackDir * 64.0f + pev->origin;
			pev->ideal_yaw = UTIL_VecToYaw(m_vecAttackerLKP - pev->origin);
		}
	}

	if (gpGlobals->time > pev->pain_finished)
		Pain(flDamage);

	return 1;
}

//=========================================================
// SetDeathActivity - plays one of the death animations
//=========================================================
void CBaseMonster::SetDeathActivity(int iDeathType)
{
	if (iDeathType >= 0 && iDeathType < NUM_DEATH_TYPES)
		m_MonsterState = MONSTERSTATE_DIE1 + iDeathType;
	else
		ALERT(at_console, "Unknown death type!\n");

	pev->ideal_yaw = pev->angles.y;
	SetActivity(m_MonsterState);
	SetThink(&CBaseMonster::MonsterThink);
	pev->nextthink = gpGlobals->time + 0.1f;
}

//=========================================================
// Killed
//=========================================================
void CBaseMonster::Killed(EOFFSET eoffsetAttacker)
{
	// leave the squad
	if (m_iSquadSize > 1)
	{
		entvars_t *pevNewLeader = m_pSquadLeader;
		unsigned int i;

		if (m_fSquadLeader)
		{
			CBaseMonster *pNext = (CBaseMonster *)Instance(m_pSquadNext);
			if (pNext)
				pNext->m_fSquadLeader = TRUE;

			pevNewLeader = m_pSquadNext;
			ALERT(at_console, "*\n");
		}

		// unlink from the ring
		entvars_t *pevMember = pev;
		for (i = 0; i < m_iSquadSize && pevMember; i++)
		{
			CBaseMonster *pMember = (CBaseMonster *)Instance(pevMember);
			if (!pMember)
				break;

			if (pMember->m_pSquadNext == pev)
				pMember->m_pSquadNext = m_pSquadNext;

			pevMember = pMember->m_pSquadNext;
		}

		for (i = 0; i < m_iSquadSize && pevMember; i++)
		{
			CBaseMonster *pMember = (CBaseMonster *)Instance(pevMember);
			if (!pMember)
				break;

			ALERT(at_console, "-");
			pMember->m_pSquadLeader = pevNewLeader;
			pevMember = pMember->m_pSquadNext;
		}
	}

	// stop the weapon sound
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/null.wav", VOL_NORM, ATTN_NORM);
	pev->solid = SOLID_NOT;

	int flags = (int)pev->flags;

	if (pev->health < GIB_HEALTH && (flags & FL_MONSTER))
	{
		SpawnBloodSpray(pev, BloodColor());

		if (rand() & 1)
		{
			// thrown back in one piece
			CountMonsterKill();

			pev->takedamage = DAMAGE_NO;
			SetTouch(NULL);
			pev->origin.z += 1.0f;

			if ((int)pev->flags & FL_ONGROUND)
			{
				pev->movetype = MOVETYPE_TOSS;
				pev->flags -= FL_ONGROUND;
				pev->velocity = g_vecAttackDir * -400.0f;
			}

			Death(DEATH_NORMAL);
			return;
		}

		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/bodysplat.wav", VOL_NORM, ATTN_NORM);
		pev->solid = SOLID_NOT;
		pev->model = 0;

		int iClass = Classify();
		BOOL fHuman = (iClass == CLASS_HUMAN_MILITARY || iClass == CLASS_PLAYER_ALLY || iClass == CLASS_PLAYER);
		CGib::SpawnRandomGibs(pev, pev->health, BloodColor(), fHuman);
	}

	if (pev->health < CORPSE_MIN_HEALTH)
		pev->health = CORPSE_MIN_HEALTH;

	// doors, triggers, etc
	if (pev->movetype == MOVETYPE_PUSH || pev->movetype == MOVETYPE_NONE)
	{
		Death(DEATH_NORMAL);
		return;
	}

	pev->enemy = eoffsetAttacker;

	if (flags & FL_MONSTER)
	{
		CountMonsterKill();

		// nothing left to animate
		if (pev->health <= GIB_HEALTH)
		{
			SetThink(&CBaseEntity::SUB_DoNothing);
			pev->nextthink = gpGlobals->time + 0.1f;
			return;
		}
	}

	pev->takedamage = DAMAGE_NO;
	SetTouch(NULL);
	Death(DEATH_NORMAL);
}

void CBaseMonster::Pain(float flDamage)
{
}

void CBaseMonster::Death(int iDeathType)
{
	SetDeathActivity(DEATH_NORMAL);
}

//=========================================================
// Multi-damage
//=========================================================
static void ClearMultiDamage()
{
	gMultiDamage.eoffsetEntity = 0;
	gMultiDamage.amount = 0;
}

//=========================================================
// ApplyMultiDamage - inflicts the collected damage, the
// victim sometimes bleeds on the wall behind it
//=========================================================
static void ApplyMultiDamage(entvars_t *pevInflictor)
{
	if (FNullEnt(gMultiDamage.eoffsetEntity))
		return;

	edict_t *pentHit = ENT(gMultiDamage.eoffsetEntity);
	entvars_t *pevHit = VARS(pentHit);
	CBaseEntity *pHit = CBaseEntity::Instance(pentHit);

	if (pHit)
		pHit->TakeDamage(pevInflictor, pevInflictor, gMultiDamage.amount);

	if (FClassnameIs(pevHit, "cycler"))
		return;

	if (RANDOM_FLOAT(0.0f, 1.0f) >= 0.3f)
		return;

	UTIL_MakeVectors(pevInflictor->origin - pevHit->origin);

	Vector vecSrc = pevHit->origin;
	vecSrc.z += pevHit->size.z * 0.5f;

	float flRight = RANDOM_FLOAT(-1.0f, 1.0f) * 0.45f;
	float flUp = RANDOM_FLOAT(-1.0f, 1.0f) * 0.45f;
	Vector vecDir = -g_vecAttackDir + gpGlobals->v_right * flRight + gpGlobals->v_up * flUp;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 128.0f, ignore_monsters, pentHit, &tr);

	if (tr.flFraction != 1.0f)
		BloodDecalTrace(&tr, pHit ? pHit->BloodColor() : 0);
}

static void AddMultiDamage(entvars_t *pevInflictor, EOFFSET eoffsetEntity, float flDamage)
{
	if (FNullEnt(eoffsetEntity))
		return;

	if (eoffsetEntity == gMultiDamage.eoffsetEntity)
	{
		gMultiDamage.amount += flDamage;
		return;
	}

	ApplyMultiDamage(pevInflictor);
	gMultiDamage.eoffsetEntity = eoffsetEntity;
	gMultiDamage.amount = flDamage;
}

//=========================================================
// SpawnBlood - blood flying out towards the attacker
//=========================================================
static void SpawnBlood(const Vector &vecSpot, int bloodColor, float flDamage)
{
	if (flDamage > 255.0f)
		flDamage = 255.0f;

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_BLOOD);
	WRITE_COORD(MSG_BROADCAST, vecSpot.x);
	WRITE_COORD(MSG_BROADCAST, vecSpot.y);
	WRITE_COORD(MSG_BROADCAST, vecSpot.z);
	WRITE_COORD(MSG_BROADCAST, g_vecAttackDir.x);
	WRITE_COORD(MSG_BROADCAST, g_vecAttackDir.y);
	WRITE_COORD(MSG_BROADCAST, g_vecAttackDir.z);
	WRITE_BYTE(MSG_BROADCAST, (unsigned char)bloodColor);
	WRITE_BYTE(MSG_BROADCAST, (int)flDamage);
}

//=========================================================
// BulletImpact - damage, blood and impact effects of a
// bullet that hit something
//=========================================================
static void BulletImpact(entvars_t *pevInflictor, float flDamage, BOOL fNoEffects, const Vector &vecDir, TraceResult *ptr)
{
	edict_t *pentHit = ENT(ptr->pHit);
	entvars_t *pevHit = VARS(pentHit);
	Vector vecSpot = ptr->vecEndPos - vecDir * 4.0f;

	if (pevHit->takedamage != DAMAGE_NO)
	{
		CBaseEntity *pHit = CBaseEntity::Instance(pentHit);
		int bloodColor = pHit ? pHit->BloodColor() : 0;

		AddMultiDamage(pevInflictor, ptr->pHit, flDamage);

		if (FClassnameIs(pevHit, "func_glass") || FClassnameIs(pevHit, "func_breakable"))
			return;

		// too tough to bleed
		if (pevHit->health > 1000.0f)
			return;

		SpawnBlood(vecSpot, bloodColor, flDamage);

		// dying, blood gushes out
		if (pevHit->health <= gMultiDamage.amount)
		{
			Vector vecOrigin = ptr->vecEndPos + vecDir * 8.0f;

			WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(MSG_BROADCAST, TE_BLOODSTREAM);
			WRITE_COORD(MSG_BROADCAST, vecOrigin.x);
			WRITE_COORD(MSG_BROADCAST, vecOrigin.y);
			WRITE_COORD(MSG_BROADCAST, vecOrigin.z);
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(-1.0f, 1.0f));
			WRITE_COORD(MSG_BROADCAST, RANDOM_FLOAT(0.0f, 1.0f));
			WRITE_BYTE(MSG_BROADCAST, (unsigned char)bloodColor);
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(80, 150));	// speed
		}
	}

	if (!fNoEffects && pevHit->solid == SOLID_BSP)
	{
		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_GUNSHOT);
		WRITE_COORD(MSG_BROADCAST, vecSpot.x);
		WRITE_COORD(MSG_BROADCAST, vecSpot.y);
		WRITE_COORD(MSG_BROADCAST, vecSpot.z);

		WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(MSG_BROADCAST, TE_DECAL);
		WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.x);
		WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.y);
		WRITE_COORD(MSG_BROADCAST, ptr->vecEndPos.z);
		WRITE_SHORT(MSG_BROADCAST, ENTINDEX(ptr->pHit));

		// see-through brush entities crack
		if (!FNullEnt(ptr->pHit) && pevHit->rendermode != kRenderNormal)
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_BREAK1, DECAL_BREAK3));
		else
			WRITE_BYTE(MSG_BROADCAST, RANDOM_LONG(DECAL_SHOT1, DECAL_SHOT5));
	}
}

//=========================================================
// FireBullets - shoots cShots bullets from the eyes along
// vecDirShooting, randomly spread by flSpreadRight and
// flSpreadUp
//=========================================================
void CBaseEntity::FireBullets(int cShots, const Vector &vecDirShooting, float flSpreadRight, float flSpreadUp, int iBulletType, float flDistance)
{
	TraceResult tr;

	UTIL_MakeVectors(pev->v_angle);

	Vector vecSrc;
	vecSrc.x = pev->origin.x + gpGlobals->v_forward.x * 10.0f;
	vecSrc.y = pev->origin.y + gpGlobals->v_forward.y * 10.0f;
	vecSrc.z = pev->origin.z + pev->view_ofs.z - 4.0f;

	// players see all their tracers
	BOOL fPlayer = FClassnameIs(pev, "player");
	int iTracerFreq = fPlayer ? 1 : 4;

	ClearMultiDamage();

	BOOL fNoEffects = FALSE;

	for (int iShot = 0; iShot < cShots; iShot++)
	{
		float flRight = RANDOM_FLOAT(-1.0f, 1.0f) * flSpreadRight;
		float flUp = RANDOM_FLOAT(-1.0f, 1.0f) * flSpreadUp;
		Vector vecDir = vecDirShooting + gpGlobals->v_right * flRight + gpGlobals->v_up * flUp;

		UTIL_TraceLine(vecSrc, vecSrc + vecDir * flDistance, dont_ignore_monsters, ENT(pev), &tr);

		g_iTracerCount = (g_iTracerCount + 1) % iTracerFreq;

		if (g_iTracerCount == iTracerFreq - 1 && pev->weapon == WEAPON_MP5)
		{
			Vector vecTracerSrc = vecSrc;

			if (fPlayer)
			{
				// from the gun
				vecTracerSrc = pev->origin + gpGlobals->v_forward * 16.0f + gpGlobals->v_right * 2.0f;

				if ((int)pev->flags & FL_DUCKING)
					vecTracerSrc.z += 6.0f;
				else
					vecTracerSrc.z += 24.0f;
			}
			else
			{
				// no more impact effects for the rest of the burst
				fNoEffects = TRUE;
			}

			WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(MSG_BROADCAST, TE_TRACER);
			WRITE_COORD(MSG_BROADCAST, vecTracerSrc.x);
			WRITE_COORD(MSG_BROADCAST, vecTracerSrc.y);
			WRITE_COORD(MSG_BROADCAST, vecTracerSrc.z);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.x);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.y);
			WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.z);
		}

		if (tr.flFraction != 1.0f)
			BulletImpact(pev, BULLET_DAMAGE, fNoEffects, vecDir, &tr);
	}

	ApplyMultiDamage(pev);
}
