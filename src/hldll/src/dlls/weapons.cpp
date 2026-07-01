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
// Weapons - Player-spawned weapon / projectile entities
//
// These classes are never spawned as map entities. The player
// builds the edict, installs the class vtable via placement-new,
// and immediately runs the matching constructor (the Deploy*/Throw*
// helpers at the bottom). Reversed from:
//   The binary (336) contact grenade
//   The binary (32) repeating sprayer projectile
//   The binary (28) single-shot sprayer projectile
//   The binary (32) deployed satchel ("Item")
//=========================================================

#include <math.h>
#include <new>
#include <stdlib.h>
#include <string.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "utils.h"
#include "weapons.h"
#include "ggrenade.h"

enum
{
	SVC_TEMPENTITY		= 23,
	TE_EXPLOSION		= 3,
	TE_DECAL			= 104,
	TE_ALPHABREAKMODEL	= 107,
};

enum
{
	CONTENTS_SOLID		= -2,
	MOVETYPE_TOSS		= 6,
	MOVETYPE_BOUNCE		= 10,
	SOLID_NOT			= 0,
	SOLID_BBOX			= 2,
};

// entvars_t byte offsets used here that have no named PEV_ constant yet.
// PEV_AIMANGLES is the player aim/punch vector MakeVectors is fed from
// (combat.cpp::FireBullets reads the same field).  PEV_GRAVITY (144) is in utils.h.
enum
{
	PEV_AIMANGLES		= 352,	// player aim angles (fed to MakeVectors)
};

static const char kGrenadeClassname[] = "grenade";
static const char kItemClassname[] = "Item";
static const char kPlayerClassname[] = "player";
static const char kFuncBreakable[] = "func_breakable";
static const char kFuncGlass[] = "func_glass";
static const char kKeyType[] = "type";

static const char kShellModel[] = "models/shell.mdl";

static const char kSprayerSound[] = "player/sprayer.wav";

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
// CSprayer - "sprayer" decal projectile
//
// Two distinct sizes, identical body except for the Think:
//   repeating variant (32 bytes): Init + RepeatThink
//   single variant (28 bytes): Init + SingleThink
// Both default-vtable everything else (plain CBaseEntity slots).
//=========================================================
// Repeating variant carries an 8-bit spray counter at +28 (32 bytes).
class CSprayerRepeat : public CBaseEntity
{
public:
	void Init(entvars_t* pevOwner);

private:
	void RepeatThink(CBaseEntity* pOther);
	void RemoveThink(CBaseEntity* pOther);

	unsigned char m_iSprayCount;
};

HL_COMPILE_TIME_ASSERT(sizeof(CSprayerRepeat) <= 32, CSprayerRepeat_private_data_size);

// Single-shot variant has no counter (28 bytes - bare CBaseEntity).
class CSprayerSingle : public CBaseEntity
{
public:
	void Init(entvars_t* pevOwner);

private:
	void SingleThink(CBaseEntity* pOther);
	void RemoveThink(CBaseEntity* pOther);
};

HL_COMPILE_TIME_ASSERT(sizeof(CSprayerSingle) <= 28, CSprayerSingle_private_data_size);

// Shared spawn body: origin = owner.origin raised 32, owner aim copied into
// pev->angles, owner index stored (prologue).
static void SprayerSpawnCommon(entvars_t* pev, entvars_t* pevOwner)
{
	PevVector(pev, PEV_ORIGIN).x = PevVector(pevOwner, PEV_ORIGIN).x;
	PevVector(pev, PEV_ORIGIN).y = PevVector(pevOwner, PEV_ORIGIN).y;
	PevVector(pev, PEV_ORIGIN).z = PevVector(pevOwner, PEV_ORIGIN).z + 32.0f;

	// Copy the owner's aim vector (owner+352) into our pev->angles (76);
	// the Think feeds these to MakeVectors to spray along the aim.
	PevVector(pev, PEV_ANGLES).x = PevFloat(pevOwner, PEV_AIMANGLES + 0);
	PevVector(pev, PEV_ANGLES).y = PevFloat(pevOwner, PEV_AIMANGLES + 4);
	PevVector(pev, PEV_ANGLES).z = PevFloat(pevOwner, PEV_AIMANGLES + 8);

	edict_t* ownerEdict = EdictFromEntvars(pevOwner);
	PevInt(pev, PEV_OWNER_ENTINDEX) = ownerEdict ? EngineIndexOfEdict(ownerEdict) : 0;
}

// Spray one decal forward from the projectile along the stored aim
// (shared by both Think variants).
static void SprayerPaintDecal(entvars_t* pev, void* globals, int decalIndex)
{
	if (!pev)
		return;

	// MakeVectors from the stored aim angles (pev->angles) -> globals forward.
	EngineMakeVectors((const float*)&PevVector(pev, PEV_ANGLES));
	const float* forward = GlobalsForward(globals);

	float vecEnd[3];
	vecEnd[0] = PevVector(pev, PEV_ORIGIN).x + (forward ? forward[0] : 0.0f) * 128.0f;
	vecEnd[1] = PevVector(pev, PEV_ORIGIN).y + (forward ? forward[1] : 0.0f) * 128.0f;
	vecEnd[2] = PevVector(pev, PEV_ORIGIN).z + (forward ? forward[2] : 0.0f) * 128.0f;

	float vecStart[3];
	vecStart[0] = PevVector(pev, PEV_ORIGIN).x;
	vecStart[1] = PevVector(pev, PEV_ORIGIN).y;
	vecStart[2] = PevVector(pev, PEV_ORIGIN).z;

	// Skip the owner; do not ignore monsters (fNoMonsters = 0).
	int ownerIndex = PevInt(pev, PEV_OWNER_ENTINDEX);
	edict_t* pOwnerEdict = ownerIndex ? EnginePEntityOfEntIndex(ownerIndex) : NULL;

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(vecStart, vecEnd, 0, pOwnerEdict, &tr);

	EngineWriteByte(0, SVC_TEMPENTITY);
	EngineWriteByte(0, TE_DECAL);
	EngineWriteCoord(0, tr.vecEndPos[0]);
	EngineWriteCoord(0, tr.vecEndPos[1]);
	EngineWriteCoord(0, tr.vecEndPos[2]);
	EngineWriteShort(0, (short)EngineModelIndex(TraceHitIndex(&tr)));
	EngineWriteByte(0, decalIndex);
}

static void SprayerRemove(entvars_t* pev)
{
	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

// Allocate the edict + private data for a sprayer of the given size.
// The caller installs the vtable with placement-new and fills pev.
static void* CreateSprayerPriv(int size, entvars_t** pevOut)
{
	edict_t* pEdict = EngineCreateEntity();
	entvars_t* pev = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
	if (!pev)
		return NULL;

	void* privateData = EngineAllocPrivateData(pEdict, size);
	if (!privateData)
		return NULL;

	memset(privateData, 0, size);
	*pevOut = pev;
	return privateData;
}

//=========================================================
// CSprayerRepeat::Init
//=========================================================
void CSprayerRepeat::Init(entvars_t* pevOwner)
{
	if (!pev || !pevOwner)
		return;

	SprayerSpawnCommon(pev, pevOwner);

	m_iSprayCount = 0;
	SetThink(&CSprayerRepeat::RepeatThink);
	SetNextThink(0.1f);

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineEmitSound(edict, 2, kSprayerSound, 1.0f, 0.8f);
}

//=========================================================
// CSprayerRepeat::RepeatThink
//=========================================================
void CSprayerRepeat::RepeatThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	SprayerPaintDecal(pev, m_pGlobals, m_iSprayCount + 6);

	unsigned char prev = m_iSprayCount;
	m_iSprayCount = (unsigned char)(prev + 1);
	if (prev >= 5)
		SetRemoveThink();

	SetNextThink(0.1f);
}

void CSprayerRepeat::RemoveThink(CBaseEntity* pOther)
{
	SprayerRemove(pev);
}

//=========================================================
// CSprayerSingle::Init
//=========================================================
void CSprayerSingle::Init(entvars_t* pevOwner)
{
	if (!pev || !pevOwner)
		return;

	SprayerSpawnCommon(pev, pevOwner);

	SetThink(&CSprayerSingle::SingleThink);
	SetNextThink(0.1f);
}

//=========================================================
// CSprayerSingle::SingleThink
//=========================================================
void CSprayerSingle::SingleThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	SprayerPaintDecal(pev, m_pGlobals, rand() % 6 + 14);

	SetRemoveThink();
	SetNextThink(0.1f);
}

void CSprayerSingle::RemoveThink(CBaseEntity* pOther)
{
	SprayerRemove(pev);
}

//=========================================================
// CSatchel - deployed satchel pickup ("Item")
//
// 9-slot CBaseEntity vtable. Unique overrides:
//   slot 1 KeyValue the binary ("type" -> item id)
//   slot 6 Touch the binary (grant item back to a player)
// Configured after construction by the binary (shell model + size).
//=========================================================
class CSatchel : public CBaseEntity
{
public:
	void Deploy(int itemId, const float* origin);

	virtual void KeyValue(KeyValueData* pkvd);
	virtual void Touch(CBaseEntity* pOther);

private:
	void RemoveThink(CBaseEntity* pOther);

	// item id this satchel grants back (this+28 in the 32-byte layout).
	int m_iItem;
};

HL_COMPILE_TIME_ASSERT(sizeof(CSatchel) <= 32, CSatchel_private_data_size);

static CSatchel* CreateSatchel()
{
	edict_t* pEdict = EngineCreateEntity();
	entvars_t* pev = pEdict ? EngineGetVarsOfEnt(pEdict) : NULL;
	if (!pev)
		return NULL;

	void* privateData = EngineAllocPrivateData(pEdict, 32);
	if (!privateData)
		return NULL;

	memset(privateData, 0, 32);

	CSatchel* pSatchel = new (privateData) CSatchel();
	pSatchel->pev = pev;
	pSatchel->m_pGlobals = GlobalsFromEntvars(pev);
	return pSatchel;
}

//=========================================================
// CSatchel::Deploy (the alloc/itemId part of)
//=========================================================
void CSatchel::Deploy(int itemId, const float* origin)
{
	if (!pev || !origin)
		return;

	m_iItem = itemId;

	PevFloat(pev, PEV_MOVETYPE) = (float)MOVETYPE_TOSS;
	PevInt(pev, PEV_CLASSNAME) = EngineAllocString(kItemClassname);
	PevFloat(pev, PEV_SOLID) = (float)SOLID_BBOX;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, kShellModel);

	{
		float mins[3] = {-16.0f, -16.0f, 0.0f};
		float maxs[3] = {16.0f, 16.0f, 16.0f};
		if (edict)
			EngineSetSize(edict, mins, maxs);
	}

	PevVector(pev, PEV_ORIGIN).x = origin[0];
	PevVector(pev, PEV_ORIGIN).y = origin[1];
	PevVector(pev, PEV_ORIGIN).z = origin[2];

	// Touch is the class virtual (vtable slot 6); the engine
	// dispatches it directly, so no SetTouch callback is needed.
}

//=========================================================
// CSatchel::KeyValue
//=========================================================
void CSatchel::KeyValue(KeyValueData* pkvd)
{
	if (!pkvd || !pkvd->szKeyName || !pkvd->szValue)
		return;

	if (strcmp(pkvd->szKeyName, kKeyType) == 0)
	{
		m_iItem = (int)atol(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CSatchel::Touch
//
// When a player walks over it, hand the stored item bit back and
// remove the satchel.
//=========================================================
void CSatchel::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	if (!pev || !m_pGlobals)
		return;

	int otherIndex = *GlobalsInt(m_pGlobals, GLOBALS_OTHER_ENTINDEX);
	edict_t* pOtherEdict = otherIndex ? EnginePEntityOfEntIndex(otherIndex) : NULL;
	entvars_t* pevOther = pOtherEdict ? EngineGetVarsOfEnt(pOtherEdict) : NULL;
	if (!pevOther)
		return;

	const char* otherClass = EngineStringFromIndex(PevInt(pevOther, PEV_CLASSNAME));
	if (!otherClass || strcmp(otherClass, kPlayerClassname) != 0)
		return;

	if (m_iItem >= 32)
		PevInt(pevOther, PEV_ITEMS_HIGH) |= (1 << (m_iItem - 32));
	else
		PevInt(pevOther, PEV_ITEMS_LOW) |= (1 << m_iItem);

	SetRemoveThink();
	SetNextThink(0.1f);
}

//=========================================================
// CSatchel::RemoveThink
//=========================================================
void CSatchel::RemoveThink(CBaseEntity* pOther)
{
	if (!pev)
		return;

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// Public spawn API (called by player code)
//=========================================================

void ThrowGrenade(entvars_t* pevOwner, const float* start, const float* velocity)
{
	// The grenade entity lives in ggrenade.cpp; its Init already branches on a
	// player owner to produce the contact-thrown grenade.
	ShootTimedGrenade(pevOwner, start, velocity);
}

void DeploySprayerRepeat(entvars_t* pevOwner)
{
	if (!pevOwner)
		return;

	entvars_t* pev = NULL;
	void* priv = CreateSprayerPriv(32, &pev);
	if (!priv)
		return;

	CSprayerRepeat* pSprayer = new (priv) CSprayerRepeat();
	pSprayer->pev = pev;
	pSprayer->m_pGlobals = GlobalsFromEntvars(pev);
	pSprayer->Init(pevOwner);
}

void DeploySprayerSingle(entvars_t* pevOwner)
{
	if (!pevOwner)
		return;

	entvars_t* pev = NULL;
	void* priv = CreateSprayerPriv(28, &pev);
	if (!priv)
		return;

	CSprayerSingle* pSprayer = new (priv) CSprayerSingle();
	pSprayer->pev = pev;
	pSprayer->m_pGlobals = GlobalsFromEntvars(pev);
	pSprayer->Init(pevOwner);
}

void* DeploySatchel(entvars_t* pevOwner, int itemId)
{
	if (!pevOwner)
		return NULL;

	CSatchel* pSatchel = CreateSatchel();
	if (!pSatchel)
		return NULL;

	// Drop point: 64 units along the owner's aim from the eye position.
	void* globals = GlobalsFromEntvars(pevOwner);
	EngineMakeVectors((const float*)((unsigned char*)pevOwner + PEV_AIMANGLES));
	const float* forward = GlobalsForward(globals);

	float eye[3];
	eye[0] = PevVector(pevOwner, PEV_ORIGIN).x + PevVector(pevOwner, PEV_VIEWOFS).x;
	eye[1] = PevVector(pevOwner, PEV_ORIGIN).y + PevVector(pevOwner, PEV_VIEWOFS).y;
	eye[2] = PevVector(pevOwner, PEV_ORIGIN).z + PevVector(pevOwner, PEV_VIEWOFS).z;

	float drop[3];
	drop[0] = eye[0] + (forward ? forward[0] : 0.0f) * 64.0f;
	drop[1] = eye[1] + (forward ? forward[1] : 0.0f) * 64.0f;
	drop[2] = eye[2] + (forward ? forward[2] : 0.0f) * 64.0f;

	if (EnginePointContents(drop) == CONTENTS_SOLID)
	{
		drop[0] = eye[0] - (forward ? forward[0] : 0.0f) * 64.0f;
		drop[1] = eye[1] - (forward ? forward[1] : 0.0f) * 64.0f;
		drop[2] = eye[2] - (forward ? forward[2] : 0.0f) * 64.0f;

		if (EnginePointContents(drop) == CONTENTS_SOLID)
			return NULL;
	}

	pSatchel->Deploy(itemId, drop);
	return pSatchel;
}
