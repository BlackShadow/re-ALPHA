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
// FuncBreak - Breakable / pushable / glass brush entities
//
//   func_breakable (vtable, size 356)
//       Spawn, KeyValue, Touch, Use,
//       Die (vtable slot 14, invoked by Killed)
//   func_pushable (vtable, size 32)
//       Spawn, KeyValue, Touch
//   func_glass (vtable, size 340)
//       Spawn, Use, Touch,
//       Die (slot 14)
//
// Classify (slot 9), TakeDamage (slot 17)
// and the shared Killed/gib helper are CBaseEntity
// defaults shared with the monster family - inherited, not retranslated.
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "cbase_toggle.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "utils.h"

//=========================================================
// Material types (KeyValue "material"; clamped to 0..5).
// Selects the gib model, debris sounds and shard count used
// by Spawn / Die.
//=========================================================
enum Materials
{
	matGlass = 0,
	matWood,
	matMetal,
	matFlesh,
	matCinderBlock,
	matCeilingTile,
};

//=========================================================
// func_breakable private-data fields (beyond the CBaseToggle
// base).  The alpha breakable is a monster-sized object; these
// live well past the 132-byte mover layout, so they are reached
// by byte offset rather than as C++ members.
//
//   [336] m_Material        (int, KeyValue "material")
//   [340] m_idExplodeType   (int, KeyValue "explosion": 1=directed)
//   [344] m_idShard         (int, shard count; spawn default 6)
//   [348] m_idGibModel      (int, precached gib model index)
//   [352] m_angle           (float, saved pev->angles.y spin)
//=========================================================
enum
{
	BREAK_MATERIAL		= 336,
	BREAK_EXPLODETYPE	= 340,
	BREAK_SHARD			= 344,
	BREAK_GIBMODEL		= 348,
	BREAK_ANGLE			= 352,
};

//=========================================================
// func_glass private-data fields.
//   [336] m_idGibModel      (int, precached "sprites/shard.spr")
//=========================================================
enum
{
	GLASS_GIBMODEL		= 336,
};

//=========================================================
// pev byte offsets used here that are not named PEV_* yet.
//   [148] pev->friction       (KeyValue caps the pushable speed)
//   [236] pev->groundentity   (ent index the entity rests on)
//=========================================================
enum
{
	PEV_FRICTION		= 148,
	PEV_GROUNDENTITY	= 236,
};

//=========================================================
// globalvars_t offsets: the activator/toucher index stashed by
// the engine before a Touch dispatch, and the world time.
//=========================================================
enum
{
	GLOBALS_OTHER_INDEX	= 116,
};

//=========================================================
// Temp-entity message constants (svc_temporary_entity).
//=========================================================
#define MSG_BROADCAST		0
#define SVC_TEMP_ENTITY		23
#define TE_BREAKMODEL		108
#define TE_GUNSHOT			15

//=========================================================
// Breakable flag bits.
//   spawnflags 1  : pev->spawnflags bit 0 - "only trigger" (no
//                   touch / no aim-takedamage)
//   pev->flags 8  : FL_CLIENT - the toucher must be a player/client.
//                   The binary CBreakable::Touch and
//                   CGlass::Touch test pev->flags & 8;
//                   bit 8 is FL_CLIENT (matching bmodels.cpp RopeTouch),
//                   NOT FL_ONGROUND (0x200).
//=========================================================
#define SF_BREAK_TRIGGER_ONLY	1
#define FL_CLIENT				8

//=========================================================
// Model / sound tables (indexed by Materials).
//=========================================================
static const char kSpriteShard[]	= "sprites/shard.spr";
static const char kModelWoodGibs[]	= "models/woodgibs.mdl";
static const char kModelShrapnel[]	= "models/shrapnel.mdl";
static const char kModelGibBGib[]	= "models/gib_b_gib.mdl";
static const char kModelCinderGibs[]	= "models/cindergibs.mdl";
static const char kModelCeilingGibs[]	= "models/ceilinggibs.mdl";

static const char kSndGlass[]		= "common/glass.wav";
static const char kSndBustGlass1[]	= "debris/bustglass1.wav";
static const char kSndBustGlass2[]	= "debris/bustglass2.wav";
static const char kSndBustGlass3[]	= "debris/bustglass3.wav";
static const char kSndBustCrate1[]	= "debris/bustcrate1.wav";
static const char kSndBustCrate3[]	= "debris/bustcrate3.wav";
static const char kSndBustCeiling[]	= "debris/bustceiling.wav";

//=========================================================
// Self-relative private-data accessors.  The breakable / glass
// extra fields sit past the modelled base, so they are reached
// by fixed byte offset (exactly as the binary does).
//=========================================================
static inline int& SelfInt(void* self, size_t byteOffset)
{
	return *(int*)((unsigned char*)self + byteOffset);
}

static inline float& SelfFloat(void* self, size_t byteOffset)
{
	return *(float*)((unsigned char*)self + byteOffset);
}

//=========================================================
// EntVarsFromIndex - resolve an ent index to its entvars.
//=========================================================
static entvars_t* EntVarsFromIndex(int index)
{
	edict_t* edict = EnginePEntityOfEntIndex(index);
	return edict ? EngineGetVarsOfEnt(edict) : NULL;
}

//=========================================================
// CBreakable - func_breakable / func_glass
//
// Derives from CBaseToggle so the KeyValue / move framework base
// is available; the breakable-specific fields are addressed by
// byte offset (see BREAK_* / GLASS_*).
//=========================================================
class CBreakable : public CBaseToggle
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Touch(CBaseEntity* pOther);
	void Use(CBaseEntity* pOther);

	// TakeDamage (vtable slot 17) - reduce pev->health and break when it
	// runs out.  In the binary the breakable shares the monster TakeDamage
	// which subtracts ceil(damage) from pev->health (0x108)
	// and, on health <= 0, routes through the shared Killed/gib helper
	// to the break virtual (slot 14 = Die). The translated
	// The base damage slot is a no-op, so the brush family re-implements
	// the break threshold here.
	virtual int TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage);

	// Die (vtable slot 14) - shatter into gibs and schedule removal.
	void Die();

	// SUB_Remove (think parked by Die) - delete the brush.
	void RemoveThink(CBaseEntity* pOther);
};

// The extra breakable fields live past the modelled base and are
// reached by byte offset; the private-data block is 356 bytes.
HL_COMPILE_TIME_ASSERT(sizeof(CBreakable) <= 356, CBreakable_private_data_size);

//=========================================================
// CBreakable::Spawn
//
// Precache the material's gib model + debris sounds, set up the
// solid pushable brush, save the spawn spin, and bind the model.
//=========================================================
void CBreakable::Spawn()
{
	EnginePrecacheSound(kSndGlass);

	// spawnflag bit 0 ("trigger only") suppresses aim-takedamage
	int spawnFlags = (int)PevFloat(pev, PEV_SPAWNFLAGS);
	if ((spawnFlags & SF_BREAK_TRIGGER_ONLY) == 0)
		PevFloat(pev, PEV_TAKEDAMAGE) = 2.0f;	// DAMAGE_AIM
	else
		PevFloat(pev, PEV_TAKEDAMAGE) = 0.0f;	// DAMAGE_NO

	PevFloat(pev, PEV_SOLID) = 4.0f;		// SOLID_BSP
	PevFloat(pev, PEV_MOVETYPE) = 7.0f;		// MOVETYPE_PUSH

	// save and clear the spin baked into pev->angles.y
	SelfFloat(this, BREAK_ANGLE) = PevFloat(pev, PEV_ANGLES + 4);
	PevFloat(pev, PEV_ANGLES + 4) = 0.0f;

	SelfInt(this, BREAK_SHARD) = 6;

	switch (SelfInt(this, BREAK_MATERIAL))
	{
	case matGlass:
		SelfInt(this, BREAK_GIBMODEL) = EnginePrecacheModel(kSpriteShard);
		EnginePrecacheSound(kSndBustGlass1);
		EnginePrecacheSound(kSndBustGlass2);
		EnginePrecacheSound(kSndBustGlass3);
		break;
	case matWood:
		SelfInt(this, BREAK_GIBMODEL) = EnginePrecacheModel(kModelWoodGibs);
		EnginePrecacheSound(kSndBustCrate1);
		EnginePrecacheSound(kSndBustCrate3);
		break;
	case matMetal:
		SelfInt(this, BREAK_GIBMODEL) = EnginePrecacheModel(kModelShrapnel);
		break;
	case matFlesh:
		SelfInt(this, BREAK_GIBMODEL) = EnginePrecacheModel(kModelGibBGib);
		break;
	case matCinderBlock:
		SelfInt(this, BREAK_GIBMODEL) = EnginePrecacheModel(kModelCinderGibs);
		break;
	case matCeilingTile:
		SelfInt(this, BREAK_GIBMODEL) = EnginePrecacheModel(kModelCeilingGibs);
		EnginePrecacheSound(kSndBustCeiling);
		break;
	default:
		break;
	}

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));
}

//=========================================================
// CBreakable::KeyValue
//
//   "explosion"  -> m_idExplodeType (1 if value is "directed",
//                   else 0; "random" and anything else -> 0)
//   "material"   -> m_Material (clamped to 0..5)
//   "deadmodel"  -> accepted (no runtime field in the alpha)
//   "shards"     -> m_idShard
//   "lip","wait" -> accepted (no-op)
//=========================================================
void CBreakable::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "explosion") == 0)
	{
		if (_stricmp(pkvd->szValue, "directed") == 0)
			SelfInt(this, BREAK_EXPLODETYPE) = 1;
		else
			SelfInt(this, BREAK_EXPLODETYPE) = 0;	// "random" / default

		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "material") == 0)
	{
		int type = atoi(pkvd->szValue);
		if (type < 0 || type > 5)
			SelfInt(this, BREAK_MATERIAL) = matWood;
		else
			SelfInt(this, BREAK_MATERIAL) = type;

		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "deadmodel") == 0)
	{
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "shards") == 0)
	{
		SelfInt(this, BREAK_SHARD) = (int)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
	else if (strcmp(pkvd->szKeyName, "lip") == 0 || strcmp(pkvd->szKeyName, "wait") == 0)
	{
		pkvd->fHandled = 1;
	}
}

//=========================================================
// CBreakable::Touch
//
// A fast-moving mover that brushes the breakable damages it in
// proportion to its speed (unless this brush is trigger-only).
//=========================================================
void CBreakable::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = GlobalsFromEntvars(pev);
	entvars_t* pevToucher = EntVarsFromIndex(*GlobalsInt(globals, GLOBALS_OTHER_INDEX));
	if (!pevToucher)
		return;

	int toucherFlags = (int)PevFloat(pevToucher, PEV_FLAGS);
	int myFlags = (int)PevFloat(pev, PEV_SPAWNFLAGS);

	if ((toucherFlags & FL_CLIENT) != 0 && (myFlags & SF_BREAK_TRIGGER_ONLY) == 0)
	{
		Vector& vel = PevVector(pevToucher, PEV_VELOCITY);
		float flDamage = sqrtf(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z) * 0.01f;

		// only break when the impact damage meets or exceeds the brush's
		// remaining health (binary FCOM pev->health vs flDamage)
		if (flDamage >= PevFloat(pev, PEV_HEALTH))
			TakeDamage(pevToucher, pevToucher, flDamage);
	}
}

//=========================================================
// CBreakable::Use
//
// Restore the saved spin, point the attack direction along the
// brush's forward vector, then break it (vtable slot 14 = Die).
//=========================================================
void CBreakable::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	// restore the saved spin onto pev->angles.y
	PevFloat(pev, PEV_ANGLES + 4) = SelfFloat(this, BREAK_ANGLE);

	// stash the brush forward vector as the attack direction
	EngineMakeVectors(VecPtr(PevVector(pev, PEV_ANGLES)));

	void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
	const float* forward = GlobalsForward(globals);
	g_vecAttackDir[0] = forward[0];
	g_vecAttackDir[1] = forward[1];
	g_vecAttackDir[2] = forward[2];

	Die();
}

//=========================================================
// CBreakable::TakeDamage (binary slot 17 = shared)
//
// Faithful to the break path of the shared monster TakeDamage:
//   - gate on pev->takedamage (+0x13c) & 0x7fffffff
//   - flesh damage = ceil(damage) (the armor split at +0x190/+0x194
//     is zero for a brush, so all damage is flesh)
//   - pev->health (+0x108) -= flesh
//   - on health <= 0, break (binary CMP [+0x108],0 / JLE -> Killed
//     -> Die slot 14).
//=========================================================
int CBreakable::TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage)
{
	HL_UNUSED(inflictor);
	HL_UNUSED(attacker);

	if (!pev)
		return 0;

	if ((PevInt(pev, PEV_TAKEDAMAGE) & 0x7FFFFFFF) == 0)
		return 0;

	float flDamage = (float)ceil((double)damage);
	PevFloat(pev, PEV_HEALTH) -= flDamage;

	if (PevFloat(pev, PEV_HEALTH) <= 0.0f)
		Die();

	return 1;
}

//=========================================================
// CBreakable::Die (vtable slot 14)
//
// Play the material's break sound, blast a TE_BREAKMODEL shower
// of gibs from the brush centre, clear the FL_ONGROUND flag of
// any entity resting on the brush, then remove the brush after a
// short delay.
//=========================================================
void CBreakable::Die()
{
	edict_t* edict = EdictFromEntvars(pev);
	unsigned char shardCount = 0;

	switch (SelfInt(this, BREAK_MATERIAL))
	{
	case matGlass:
	{
		shardCount = 1;
		int pick = rand() % 3;
		if (pick == 0)
			EngineEmitSound(edict, 2, kSndBustGlass1, 1.0f, 0.8f);
		else if (pick == 1)
			EngineEmitSound(edict, 2, kSndBustGlass2, 1.0f, 0.8f);
		else
			EngineEmitSound(edict, 2, kSndBustGlass3, 1.0f, 0.8f);
		break;
	}
	case matWood:
	{
		shardCount = 8;
		int pick = rand() % 2;
		if (pick == 0)
			EngineEmitSound(edict, 2, kSndBustCrate1, 1.0f, 0.8f);
		else
			EngineEmitSound(edict, 2, kSndBustCrate3, 1.0f, 0.8f);
		break;
	}
	case matMetal:
		shardCount = 18;
		EngineEmitSound(edict, 2, kSndGlass, 1.0f, 0.8f);
		break;
	case matFlesh:
		shardCount = 4;
		EngineEmitSound(edict, 2, kSndGlass, 1.0f, 0.8f);
		break;
	case matCinderBlock:
		// shardCount stays 0 (binary never sets BL for this material)
		EngineEmitSound(edict, 2, kSndGlass, 1.0f, 0.8f);
		break;
	case matCeilingTile:
		// shardCount stays 0 (binary never sets BL for this material)
		EngineEmitSound(edict, 2, kSndBustCeiling, 1.0f, 0.8f);
		break;
	default:
		break;
	}

	// "random" explosion type scatters the gib direction
	if (SelfInt(this, BREAK_EXPLODETYPE) != 1)
	{
		RandomFloat(-1.0f, 1.0f);
		RandomFloat(-1.0f, 1.0f);
		RandomFloat(-1.0f, 1.0f);
	}

	// gib shower at the brush centre
	Vector& mins = PevVector(pev, PEV_MINS);
	Vector& maxs = PevVector(pev, PEV_MAXS);
	float center[3];
	center[0] = (mins.x + maxs.x) * 0.5f;
	center[1] = (mins.y + maxs.y) * 0.5f;
	center[2] = (mins.z + maxs.z) * 0.5f;

	EngineWriteByte(MSG_BROADCAST, SVC_TEMP_ENTITY);
	EngineWriteByte(MSG_BROADCAST, TE_BREAKMODEL);
	EngineWriteCoord(MSG_BROADCAST, center[0]);
	EngineWriteCoord(MSG_BROADCAST, center[1]);
	EngineWriteCoord(MSG_BROADCAST, center[2]);

	Vector& size = PevVector(pev, PEV_SIZE);
	EngineWriteCoord(MSG_BROADCAST, size.x);
	EngineWriteCoord(MSG_BROADCAST, size.y);
	EngineWriteCoord(MSG_BROADCAST, size.z);

	EngineWriteCoord(MSG_BROADCAST, 1.0f);
	EngineWriteCoord(MSG_BROADCAST, 1.0f);
	EngineWriteCoord(MSG_BROADCAST, 1.0f);

	EngineWriteShort(MSG_BROADCAST, SelfInt(this, BREAK_GIBMODEL));
	EngineWriteByte(MSG_BROADCAST, 0);
	EngineWriteByte(MSG_BROADCAST, TE_GUNSHOT);
	EngineWriteByte(MSG_BROADCAST, (int)shardCount);

	// search a sphere the radius of the largest brush half-extent and
	// drop the FL_ONGROUND (0x200) flag on anything resting on us, so
	// it falls once the brush vanishes
	float radius = size.x;
	if (radius < size.y)
		radius = size.y;
	if (radius < size.z)
		radius = size.z;

	// The engine threads the sphere-search results through pev->chain;
	// walk that chain rather than re-issuing the search.
	edict_t* pSearch = EngineFindEntityInSphere(center, radius);
	while (pSearch)
	{
		if (EngineIndexOfEdict(pSearch) == 0)
			break;

		entvars_t* pevHit = EngineGetVarsOfEnt(pSearch);
		int hitFlags = (int)PevFloat(pevHit, PEV_FLAGS);
		if ((hitFlags & 0x200) != 0)
		{
			// clear FL_ONGROUND (0x200) and drop pev->groundentity so
			// the entity falls once the brush is gone
			PevFloat(pevHit, PEV_FLAGS) = (float)(hitFlags & ~0x200);
			PevInt(pevHit, PEV_GROUNDENTITY) = 0;
		}

		pSearch = EnginePEntityOfEntIndex(PevInt(pevHit, PEV_CHAIN));
	}

	// remove the brush a tick later
	SetRemoveThink();
	PevFloat(pev, PEV_NEXTTHINK) = (float)(PevFloat(pev, PEV_LTIME) + 0.1);
}

//=========================================================
// CBreakable::RemoveThink
//
// Delete the brush edict.
//=========================================================
void CBreakable::RemoveThink(CBaseEntity* pOther)
{
	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineRemoveEntity(edict);
}

//=========================================================
// CGlass - func_glass
//
// A breakable specialised for glass: it always uses the glass
// material, dies when bumped fast enough by a mover, and shatters
// through its own (simpler) gib message.
//=========================================================
class CGlass : public CBreakable
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pOther);
	void Touch(CBaseEntity* pOther);

	// TakeDamage (vtable slot 17) - same break threshold as CBreakable,
	// but glass shatters through its own break method (slot 14 = GlassDie).
	virtual int TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage);

	// Die (vtable slot 14)
	void GlassDie();
};

HL_COMPILE_TIME_ASSERT(sizeof(CGlass) <= 340, CGlass_private_data_size);

//=========================================================
// CGlass::KeyValue
//
// func_glass parses no keys of its own (the alpha vtable slot is
// empty); it does NOT inherit the breakable material handling.
//=========================================================
void CGlass::KeyValue(KeyValueData* pkvd)
{
	HL_UNUSED(pkvd);
}

//=========================================================
// CGlass::TakeDamage (binary slot 17 = shared)
//
// Identical break path to CBreakable::TakeDamage, except a dead
// pane routes to the glass-specific break (slot 14 = GlassDie).
//=========================================================
int CGlass::TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage)
{
	HL_UNUSED(inflictor);
	HL_UNUSED(attacker);

	if (!pev)
		return 0;

	if ((PevInt(pev, PEV_TAKEDAMAGE) & 0x7FFFFFFF) == 0)
		return 0;

	float flDamage = (float)ceil((double)damage);
	PevFloat(pev, PEV_HEALTH) -= flDamage;

	if (PevFloat(pev, PEV_HEALTH) <= 0.0f)
		GlassDie();

	return 1;
}

//=========================================================
// CGlass::Spawn
//=========================================================
void CGlass::Spawn()
{
	EnginePrecacheSound(kSndGlass);
	SelfInt(this, GLASS_GIBMODEL) = EnginePrecacheModel(kSpriteShard);

	// spawnflag bit 0 ("trigger only") suppresses aim-takedamage
	int spawnFlags = (int)PevFloat(pev, PEV_SPAWNFLAGS);
	if ((spawnFlags & SF_BREAK_TRIGGER_ONLY) == 0)
		PevFloat(pev, PEV_TAKEDAMAGE) = 1.0f;	// DAMAGE_YES
	else
		PevFloat(pev, PEV_TAKEDAMAGE) = 0.0f;	// DAMAGE_NO

	PevFloat(pev, PEV_SOLID) = 4.0f;		// SOLID_BSP
	PevFloat(pev, PEV_MOVETYPE) = 7.0f;		// MOVETYPE_PUSH

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));
}

//=========================================================
// CGlass::Use
//
// Route straight to Die (vtable slot 14).
//=========================================================
void CGlass::Use(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	GlassDie();
}

//=========================================================
// CGlass::Touch
//
// A grounded mover travelling faster than the remaining health
// shatters the glass.
//=========================================================
void CGlass::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = GlobalsFromEntvars(pev);
	entvars_t* pevToucher = EntVarsFromIndex(*GlobalsInt(globals, GLOBALS_OTHER_INDEX));
	if (!pevToucher)
		return;

	int toucherFlags = (int)PevFloat(pevToucher, PEV_FLAGS);
	if ((toucherFlags & FL_CLIENT) == 0)
		return;

	if ((PevInt(pev, PEV_TAKEDAMAGE) & 0x7FFFFFFF) == 0)
		return;

	Vector& vel = PevVector(pevToucher, PEV_VELOCITY);
	float flSpeed = sqrtf(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z) * 0.1f;
	if (flSpeed > PevFloat(pev, PEV_HEALTH))
		TakeDamage(pevToucher, pevToucher, PevFloat(pev, PEV_HEALTH));
}

//=========================================================
// CGlass::GlassDie (vtable slot 14)
//
// Shatter: one glass sound + a TE_BREAKMODEL shower from the
// brush centre, made non-solid, then removed next tick.
//=========================================================
void CGlass::GlassDie()
{
	edict_t* edict = EdictFromEntvars(pev);
	EngineEmitSound(edict, 2, kSndGlass, 1.0f, 0.8f);

	Vector& angles = PevVector(pev, PEV_ANGLES);
	EngineMakeVectors(VecPtr(angles));

	Vector& mins = PevVector(pev, PEV_MINS);
	Vector& maxs = PevVector(pev, PEV_MAXS);

	EngineWriteByte(MSG_BROADCAST, SVC_TEMP_ENTITY);
	EngineWriteByte(MSG_BROADCAST, TE_BREAKMODEL);
	EngineWriteCoord(MSG_BROADCAST, (mins.x + maxs.x) * 0.5f);
	EngineWriteCoord(MSG_BROADCAST, (mins.y + maxs.y) * 0.5f);
	EngineWriteCoord(MSG_BROADCAST, (mins.z + maxs.z) * 0.5f);

	Vector& size = PevVector(pev, PEV_SIZE);
	EngineWriteCoord(MSG_BROADCAST, size.x);
	EngineWriteCoord(MSG_BROADCAST, size.y);
	EngineWriteCoord(MSG_BROADCAST, size.z);

	EngineWriteCoord(MSG_BROADCAST, 1.0f);
	EngineWriteCoord(MSG_BROADCAST, 1.0f);
	EngineWriteCoord(MSG_BROADCAST, 1.0f);

	EngineWriteShort(MSG_BROADCAST, SelfInt(this, GLASS_GIBMODEL));
	EngineWriteByte(MSG_BROADCAST, 12);
	EngineWriteByte(MSG_BROADCAST, TE_GUNSHOT);
	EngineWriteByte(MSG_BROADCAST, 1);

	PevFloat(pev, PEV_SOLID) = 0.0f;	// SOLID_NOT
	PevInt(pev, PEV_MODEL) = 0;			// hide the brush

	SetRemoveThink();
	PevFloat(pev, PEV_NEXTTHINK) = PevFloat(pev, PEV_LTIME);
}

//=========================================================
// CPushable - func_pushable
//
// A small crate the player can shove around.  Derives directly
// from CBaseEntity (32-byte private data) plus a single speed cap.
//=========================================================
class CPushable : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData* pkvd);
	void Touch(CBaseEntity* pOther);

private:
	float m_maxSpeed;	// [28]
};

HL_COMPILE_TIME_ASSERT(sizeof(CPushable) <= 32, CPushable_private_data_size);

//=========================================================
// CPushable::Spawn
//
// Solid, pushable crate; the maximum push speed is derived from
// pev->friction.
//=========================================================
void CPushable::Spawn()
{
	PevFloat(pev, PEV_MOVETYPE) = 13.0f;	// MOVETYPE_PUSHSTEP
	PevFloat(pev, PEV_SOLID) = 2.0f;		// SOLID_BBOX

	edict_t* edict = EdictFromEntvars(pev);
	if (edict)
		EngineSetModel(edict, EngineStringFromIndex(PevInt(pev, PEV_MODEL)));

	float friction = PevFloat(pev, PEV_FRICTION);
	if (friction > 399.0f)
	{
		friction = 399.0f;
		PevFloat(pev, PEV_FRICTION) = friction;
	}

	m_maxSpeed = 400.0f - PevFloat(pev, PEV_FRICTION);
	PevFloat(pev, PEV_FRICTION) = 0.0f;
}

//=========================================================
// CPushable::KeyValue
//
// "size" selects the crate's bounding box (0/default small,
// 2 medium, 3 tall-narrow).
//=========================================================
void CPushable::KeyValue(KeyValueData* pkvd)
{
	if (strcmp(pkvd->szKeyName, "size") != 0)
		return;

	int size = atoi(pkvd->szValue);
	pkvd->fHandled = 1;

	edict_t* edict = EdictFromEntvars(pev);
	float mins[3];
	float maxs[3];

	if (size == 2)
	{
		VecSet(maxs, 16.0f, 16.0f, 18.0f);
		Vec3Scale(maxs, maxs, 2.0f);
		VecSet(mins, -16.0f, -16.0f, -18.0f);
		Vec3Scale(mins, mins, 2.0f);
	}
	else if (size == 3)
	{
		VecSet(maxs, 16.0f, 16.0f, 18.0f);
		VecSet(mins, -16.0f, -16.0f, -18.0f);
	}
	else if (size != 0)
	{
		VecSet(maxs, 16.0f, 16.0f, 36.0f);
		VecSet(mins, -16.0f, -16.0f, -36.0f);
	}
	else
	{
		VecSet(maxs, 8.0f, 8.0f, 8.0f);
		VecSet(mins, -8.0f, -8.0f, -8.0f);
	}

	if (edict)
		EngineSetSize(edict, mins, maxs);
}

//=========================================================
// CPushable::Touch
//
// A player walking into the crate shoves it along the ground:
// add half the player's horizontal velocity, then clamp the
// crate's speed to m_maxSpeed.
//=========================================================
void CPushable::Touch(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	void* globals = GlobalsFromEntvars(pev);
	entvars_t* pevToucher = EntVarsFromIndex(*GlobalsInt(globals, GLOBALS_OTHER_INDEX));
	if (!pevToucher)
		return;

	const char* classname = EngineStringFromIndex(PevInt(pevToucher, PEV_CLASSNAME));
	if (strcmp(classname, "player") != 0)
		return;

	// ignore the toucher if it is already standing on this crate
	int toucherFlags = (int)PevFloat(pevToucher, PEV_FLAGS);
	if ((toucherFlags & 0x200) != 0)
	{
		entvars_t* pevGround = EntVarsFromIndex(PevInt(pevToucher, PEV_GROUNDENTITY));
		if (pevGround == pev)
			return;
	}

	Vector& myVel = PevVector(pev, PEV_VELOCITY);
	Vector& playerVel = PevVector(pevToucher, PEV_VELOCITY);

	myVel.x += playerVel.x * 0.5f;
	myVel.y += playerVel.y * 0.5f;

	float speed = sqrtf(myVel.y * myVel.y + myVel.x * myVel.x);
	if (speed != 0.0f)
	{
		myVel.x = myVel.x / speed * m_maxSpeed;
		myVel.y = myVel.y / speed * m_maxSpeed;
	}
}

//=========================================================
// Construction wrappers
//=========================================================

//=========================================================
// func_breakable
//=========================================================
DLLEXPORT void func_breakable(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 356);
		if (!privateData)
			return;

		memset(privateData, 0, 356);

		CBreakable* self = new (privateData) CBreakable();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_glass
//=========================================================
DLLEXPORT void func_glass(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 340);
		if (!privateData)
			return;

		memset(privateData, 0, 340);

		CGlass* self = new (privateData) CGlass();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// func_pushable
//=========================================================
DLLEXPORT void func_pushable(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 32);
		if (!privateData)
			return;

		memset(privateData, 0, 32);

		CPushable* self = new (privateData) CPushable();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
