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
// Grenade - timed grenades and radius damage
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "decals.h"
#include "ggrenade.h"

#define GRENADE_DAMAGE		100.0f
#define GRENADE_FUSE		2.0f	// monster grenades explode after this long

extern short g_sModelIndexShrapnel;

//=========================================================
// RadiusDamage - damages everything within reach that can
// be seen from the explosion, less the further away it is
//=========================================================
static void RadiusDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore)
{
	// in case the grenade is lying on the ground
	pevInflictor->origin.z += 1.0f;

	Vector vecSrc = pevInflictor->origin;
	float flRadius = flDamage * 2.0f;

	edict_t *pentHit = FIND_ENTITY_IN_SPHERE(vecSrc, flRadius);
	while (!FNullEnt(pentHit))
	{
		entvars_t *pevHit = VARS(pentHit);

		if (pevHit->takedamage != DAMAGE_NO)
		{
			CBaseEntity *pHit = CBaseEntity::Instance(pentHit);

			if (pHit && pHit->Classify() != iClassIgnore)
			{
				Vector vecSpot = pevHit->origin;
				vecSpot.z += pevHit->size.z * 0.5f;

				TraceResult tr;
				UTIL_TraceLine(vecSrc, vecSpot, ignore_monsters, ENT(pevInflictor), &tr);

				if (tr.flFraction == 1.0f)
				{
					float flAdjustedDamage = flDamage - (vecSrc - pevHit->origin).Length() * 0.4f;
					if (flAdjustedDamage < 0)
						flAdjustedDamage = 0;

					pHit->TakeDamage(pevInflictor, pevAttacker, flAdjustedDamage);
				}
			}
		}

		pentHit = ENT(pevHit->chain);
	}
}

//=========================================================
// GetTossTarget
//=========================================================
Vector GetTossTarget(entvars_t *pevOwner, const Vector &vecStart, const Vector &vecTarget)
{
	UTIL_MakeVectors(pevOwner->angles);

	// miss by up to 24 units
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);
	float flMiss = (flRand * 16.0f - 8.0f) + (flRand * 32.0f - 16.0f);

	Vector vecAim = vecTarget + gpGlobals->v_right * flMiss + gpGlobals->v_forward * flMiss;

	// the top of the arc is under the ceiling halfway there
	Vector vecMidPoint = (vecAim - vecStart) * 0.5f + vecStart;

	TraceResult tr;
	UTIL_TraceLine(vecMidPoint, vecMidPoint + gpGlobals->v_up * 1024.0f, ignore_monsters, ENT(pevOwner), &tr);
	Vector vecApex = tr.vecEndPos;

	// both halves of the arc must be clear
	UTIL_TraceLine(vecStart, vecApex, dont_ignore_monsters, ENT(pevOwner), &tr);
	if (tr.flFraction != 1.0f)
		vecApex = g_vecZero;

	UTIL_TraceLine(vecAim, vecApex, dont_ignore_monsters, ENT(pevOwner), &tr);
	if (tr.flFraction != 1.0f)
		vecApex = g_vecZero;

	if (vecApex != g_vecZero)
	{
		DrawDebugLine(vecAim, vecStart);
		DrawDebugLine(vecStart, vecApex);
		DrawDebugLine(vecAim, vecApex);
	}

	return vecApex;
}

//=========================================================
// CGrenade
//=========================================================
class CGrenade : public CBaseMonster
{
public:
	void Init(entvars_t *pevOwner, const Vector &vecStart, const Vector &vecVelocity);
	void Touch(CBaseEntity *pOther);
	void ExplodeThink(CBaseEntity *pOther);
};

void CGrenade::Init(entvars_t *pevOwner, const Vector &vecStart, const Vector &vecVelocity)
{
	BOOL fContact = pevOwner && FClassnameIs(pevOwner, "player");

	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = ALLOC_STRING("grenade");

	pev->renderamt = 0;
	pev->rendermode = kRenderNormal;
	pev->renderfx = 0;

	if (fContact)
		pev->gravity = 0.4f;

	pev->solid = SOLID_BBOX;

	if (pevOwner)
		pev->owner = OFFSET(pevOwner);

	SET_MODEL(ENT(pev), "models/grenade.mdl");
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	pev->origin = vecStart;
	pev->velocity = vecVelocity;
	pev->angles = UTIL_VecToAngles(vecVelocity);
	pev->dmg = GRENADE_DAMAGE;

	if (fContact)
	{
		SetThink(&CBaseEntity::SUB_DoNothing);
		pev->avelocity.x = RANDOM_FLOAT(-100.0f, -500.0f);
	}
	else
	{
		SetThink(&CGrenade::ExplodeThink);
		pev->nextthink = gpGlobals->time + GRENADE_FUSE;
		pev->avelocity.x = -400.0f;
	}
}

//=========================================================
// Touch - the player's grenades explode on what they hit,
// the others bounce
//=========================================================
void CGrenade::Touch(CBaseEntity *pOther)
{
	entvars_t *pevOwner = FNullEnt(pev->owner) ? NULL : VARS(pev->owner);

	if (pevOwner && FClassnameIs(pevOwner, "player"))
	{
		pev->enemy = gpGlobals->other;

		SetThink(&CGrenade::ExplodeThink);
		pev->nextthink = gpGlobals->time;

		// breakables take the full damage
		if (!FNullEnt(gpGlobals->other))
		{
			edict_t *pentOther = ENT(gpGlobals->other);
			entvars_t *pevOther = VARS(pentOther);

			if (FClassnameIs(pevOther, "func_breakable") || FClassnameIs(pevOther, "func_glass"))
			{
				CBaseEntity *pHit = CBaseEntity::Instance(pentOther);
				if (pHit)
					pHit->TakeDamage(pev, pevOwner, pev->dmg);
			}
		}
		return;
	}

	pev->movetype = MOVETYPE_BOUNCE;
	pev->avelocity = Vector(300.0f, 300.0f, 300.0f);
	pev->gravity = 1.0f;

	if (gpGlobals->other != pev->owner)
	{
		if (RANDOM_FLOAT(0.0f, 1.0f) < 0.7f)
		{
			float flRand = RANDOM_FLOAT(0.0f, 1.0f);
			const char *pszSound;

			if (flRand <= 0.33f)
				pszSound = "weapons/g_bounce1.wav";
			else if (flRand <= 0.66f)
				pszSound = "weapons/g_bounce2.wav";
			else
				pszSound = "weapons/g_bounce3.wav";

			EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_NORM);
		}

		pev->velocity = pev->velocity * 0.8f;
	}
}

//=========================================================
// ExplodeThink
//=========================================================
void CGrenade::ExplodeThink(CBaseEntity *pOther)
{
	pev->model = 0;
	pev->solid = SOLID_NOT;

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_EXPLOSION);
	WRITE_COORD(MSG_BROADCAST, pev->origin.x);
	WRITE_COORD(MSG_BROADCAST, pev->origin.y);
	WRITE_COORD(MSG_BROADCAST, pev->origin.z);

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_SPRITE_SPRAY);
	WRITE_COORD(MSG_BROADCAST, pev->origin.x);
	WRITE_COORD(MSG_BROADCAST, pev->origin.y);
	WRITE_COORD(MSG_BROADCAST, pev->origin.z);
	WRITE_COORD(MSG_BROADCAST, 400.0f);		// speed
	WRITE_SHORT(MSG_BROADCAST, g_sModelIndexShrapnel);
	WRITE_SHORT(MSG_BROADCAST, 30);			// count
	WRITE_BYTE(MSG_BROADCAST, 15);			// life

	entvars_t *pevOwner = FNullEnt(pev->owner) ? NULL : VARS(pev->owner);
	RadiusDamage(pev, pevOwner, pev->dmg, CLASS_NONE);

	// scorch mark where the grenade was going
	Vector vecStart;
	Vector vecEnd;

	if (pev->velocity != g_vecZero)
	{
		Vector vecDir = pev->velocity.Normalize();
		vecStart = pev->origin - vecDir * 32.0f;
		vecEnd = pev->origin;
	}
	else
	{
		vecStart = pev->origin;
		vecStart.z += 8.0f;
		vecEnd = vecStart;
		vecEnd.z -= 24.0f;
	}

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev), &tr);

	WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(MSG_BROADCAST, TE_DECAL);
	WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.x);
	WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.y);
	WRITE_COORD(MSG_BROADCAST, tr.vecEndPos.z);
	WRITE_SHORT(MSG_BROADCAST, ENTINDEX(tr.pHit));

	if (RANDOM_FLOAT(0.0f, 1.0f) < 0.5f)
		WRITE_BYTE(MSG_BROADCAST, DECAL_SCORCH1);
	else
		WRITE_BYTE(MSG_BROADCAST, DECAL_SCORCH2);

	float flRndSound = RANDOM_FLOAT(0.0f, 1.0f);	// not used

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris1.wav", VOL_NORM, ATTN_NORM);
		break;

	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris2.wav", VOL_NORM, ATTN_NORM);
		break;

	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris3.wav", VOL_NORM, ATTN_NORM);
		break;
	}

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 2.0f;
}

//=========================================================
// ShootTimedGrenade
//=========================================================
void ShootTimedGrenade(entvars_t *pevOwner, const Vector &vecStart, const Vector &vecVelocity)
{
	CGrenade *pGrenade = GetClassPtr((CGrenade *)NULL);
	pGrenade->Init(pevOwner, vecStart, vecVelocity);
}
