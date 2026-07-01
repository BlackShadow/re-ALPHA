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
// Sound - Sound entities (ambient_generic, env_sound)
//=========================================================

#include <new>
#include <string.h>
#include <stdlib.h>
#include "cbase.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "player.h"
#include "utils.h"

//=========================================================
// Constants
//=========================================================

// channel passed to the sound engine
#define SND_CHANNEL_STATIC		1

// ambient_generic spawnflags
#define AMBIENT_SF_EVERYWHERE	1	// play with attenuation 0 (audible map-wide)
#define AMBIENT_SF_STARTSILENT	2	// start silent (not active)

#define AMBIENT_ATTN_NORM		2.25f
#define AMBIENT_ATTN_EVERYWHERE	0.0f
#define AMBIENT_VOLUME_SCALE	0.1		// pev.health holds 0..10 volume; * 0.1 -> 0..1 (binary multiplies by a double)

#define SVC_ROOMTYPE			37		// engine message: set client DSP roomtype

static const char kNullSound[] = "common/null.wav";

// env_sound keyvalue names
static const char kKeyRadius[] = "radius";
static const char kKeyRoomtype[] = "roomtype";

//=========================================================
// entvars_t field offsets used here that are not yet in utils.h.
// (values must match the IDA code exactly)
//=========================================================

#define PEV_SND_LOOPING			440		// non-zero: ambient loops / is toggleable
#define PEV_SND_SAMPLE			472		// string index of the sound sample to play

//=========================================================
// globalvars_t field offset (engine message recipient entity index)
//=========================================================

#define GLOBALS_MSG_ENTITY		328

//=========================================================
// CBasePlayer private-data layout - per-player roomtype tracking.
// CBasePlayer lives in a separate translation unit; here we only touch
// the three fields env_sound needs, by byte offset.
//=========================================================

#define PLR_ROOM_ENV_EDICT		352		// env_sound edict controlling this player
#define PLR_ROOM_TYPE			356		// roomtype value applied to this player
#define PLR_ROOM_RANGE			360		// distance to the controlling env_sound

enum
{
	PLAYER_PRIVATE_DATA_SIZE = 452
};

//=========================================================
// CEnvSound private-data layout - m_flRadius (the activation
// radius) sits at byte offset 28, immediately after CBaseEntity.
//=========================================================

#define ENVSOUND_RADIUS_OFFSET	28

//=========================================================
// CAmbientGeneric - looping / one-shot ambient sound source
//=========================================================

class CAmbientGeneric : public CBaseEntity
{
public:
	void Spawn();

private:
	void RampThink(CBaseEntity* pOther);
	void ToggleUse(CBaseEntity* pOther);

	int m_fActive;		// is the looping sound currently playing
};

HL_COMPILE_TIME_ASSERT(sizeof(CAmbientGeneric) <= 32, CAmbientGeneric_private_data_size);

//=========================================================
// Spawn
//=========================================================
void CAmbientGeneric::Spawn()
{
	const char* sample = EngineStringFromIndex(PevInt(pev, PEV_SND_SAMPLE));
	EnginePrecacheSound(sample);

	PevFloat(pev, PEV_SOLID) = 0.0f;
	PevFloat(pev, PEV_MOVETYPE) = 0.0f;

	if (PevInt(pev, PEV_SND_LOOPING))
	{
		// Looping, toggleable ambient. Start active unless flagged silent.
		SetUse(&CAmbientGeneric::ToggleUse);

		int spawnFlags = (int)PevFloat(pev, PEV_SPAWNFLAGS);
		m_fActive = 0;
		if ((spawnFlags & AMBIENT_SF_STARTSILENT) == 0)
			m_fActive = 1;

		SetThink(&CAmbientGeneric::RampThink);
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 1.0f;
	}
	else
	{
		// One-shot ambient: emit once and forget.
		float attenuation;
		if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & AMBIENT_SF_EVERYWHERE) != 0)
			attenuation = AMBIENT_ATTN_EVERYWHERE;
		else
			attenuation = AMBIENT_ATTN_NORM;

		float volume = (float)(PevFloat(pev, PEV_HEALTH) * AMBIENT_VOLUME_SCALE);	// binary uses a double multiply; cast keeps fidelity + silences C4244

		Vector vecOrigin = PevVector(pev, PEV_ORIGIN);
		EngineEmitAmbientSound(VecPtr(vecOrigin), sample, volume, attenuation);
	}
}

//=========================================================
// RampThink
//
// Fires once after Spawn: if the looping sound should be active, start
// it on the static channel, then stop thinking.
//=========================================================
void CAmbientGeneric::RampThink(CBaseEntity* pOther)
{
	const char* sample = EngineStringFromIndex(PevInt(pev, PEV_SND_SAMPLE));

	if (m_fActive)
	{
		float attenuation;
		if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & AMBIENT_SF_EVERYWHERE) != 0)
			attenuation = AMBIENT_ATTN_EVERYWHERE;
		else
			attenuation = AMBIENT_ATTN_NORM;

		float volume = (float)(PevFloat(pev, PEV_HEALTH) * AMBIENT_VOLUME_SCALE);	// binary uses a double multiply; cast keeps fidelity + silences C4244

		edict_t* edict = EdictFromEntvars(pev);
		if (edict)
			EngineEmitSound(edict, SND_CHANNEL_STATIC, sample, volume, attenuation);
	}

	// Stop thinking (binary points the think at the empty SUB_DoNothing
	// stub the binary).
	SetDoNothingThink();
}

//=========================================================
// ToggleUse - toggle the looping sound on/off
//
// Installed as the use handler by Spawn for looping ambients only;
// one-shot ambients never set a use handler.
//=========================================================
void CAmbientGeneric::ToggleUse(CBaseEntity* pOther)
{
	HL_UNUSED(pOther);

	const char* sample = EngineStringFromIndex(PevInt(pev, PEV_SND_SAMPLE));

	float attenuation;
	if (((int)PevFloat(pev, PEV_SPAWNFLAGS) & AMBIENT_SF_EVERYWHERE) != 0)
		attenuation = AMBIENT_ATTN_EVERYWHERE;
	else
		attenuation = AMBIENT_ATTN_NORM;

	float volume = (float)(PevFloat(pev, PEV_HEALTH) * AMBIENT_VOLUME_SCALE);	// binary uses a double multiply; cast keeps fidelity + silences C4244

	edict_t* edict = EdictFromEntvars(pev);

	if (m_fActive)
	{
		// Currently playing: replace it with the null sound to silence it.
		m_fActive = 0;
		if (edict)
			EngineEmitSound(edict, SND_CHANNEL_STATIC, kNullSound, volume, attenuation);
	}
	else
	{
		// Currently silent: restart the sample.
		m_fActive = 1;
		if (edict)
			EngineEmitSound(edict, SND_CHANNEL_STATIC, sample, volume, attenuation);
	}
}

//=========================================================
// CEnvSound - per-player DSP / roomtype trigger
//=========================================================

class CEnvSound : public CBaseEntity
{
public:
	void Spawn();
	void Think(CBaseEntity* pOther);
	void KeyValue(KeyValueData* pkvd);

private:
	float m_flRadius;		// activation radius around the entity
	float m_flRoomtype;		// roomtype index to apply to the player
};

HL_COMPILE_TIME_ASSERT(sizeof(CEnvSound) <= 36, CEnvSound_private_data_size);

//=========================================================
// FEnvSoundInRange
//
// Trace from the env_sound to the player's eyes. If the path is clear,
// store the distance in *pflRange and return true.
//=========================================================
static int FEnvSoundInRange(entvars_t* pevSound, entvars_t* pevPlayer, float* pflRange)
{
	if (!pevSound || !pevPlayer)
		return 0;

	float vecSpot1[3];
	float vecSpot2[3];

	// env_sound emitter point (origin + view offset)
	vecSpot1[0] = PevVector(pevSound, PEV_VIEWOFS).x + PevVector(pevSound, PEV_ORIGIN).x;
	vecSpot1[1] = PevVector(pevSound, PEV_VIEWOFS).y + PevVector(pevSound, PEV_ORIGIN).y;
	vecSpot1[2] = PevVector(pevSound, PEV_VIEWOFS).z + PevVector(pevSound, PEV_ORIGIN).z;

	// player eye point (origin + view offset)
	vecSpot2[0] = PevVector(pevPlayer, PEV_VIEWOFS).x + PevVector(pevPlayer, PEV_ORIGIN).x;
	vecSpot2[1] = PevVector(pevPlayer, PEV_VIEWOFS).y + PevVector(pevPlayer, PEV_ORIGIN).y;
	vecSpot2[2] = PevVector(pevPlayer, PEV_VIEWOFS).z + PevVector(pevPlayer, PEV_ORIGIN).z;

	edict_t* skip = EdictFromEntvars(pevSound);

	// Original wrapper flag is 0 here; EngineTraceLine converts that to the
	// engine's ignore-monsters flag.
	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(vecSpot1, vecSpot2, 0, skip, &tr);

	// Blocked (didn't reach the player) -> not in range.
	if ((tr.fInOpen && tr.fInWater) || tr.flFraction != 1.0f)
		return 0;

	float vecDelta[3];
	vecDelta[0] = tr.vecEndPos[0] - vecSpot1[0];
	vecDelta[1] = tr.vecEndPos[1] - vecSpot1[1];
	vecDelta[2] = tr.vecEndPos[2] - vecSpot1[2];

	float flRange = VecLength(vecDelta);

	// Reject players outside this env_sound's activation radius. m_flRadius
	// lives at byte offset 28 in the CEnvSound private data (object[7]).
	void* pSoundPriv = EngineGetPrivateData(skip);
	float flRadius = pSoundPriv ? *(float*)((unsigned char*)pSoundPriv + ENVSOUND_RADIUS_OFFSET) : 0.0f;
	if (flRadius < flRange)
		return 0;

	if (pflRange)
		*pflRange = flRange;

	return 1;
}

//=========================================================
// PlayerPrivateData
//
// Return a client's CBasePlayer private data as a raw byte pointer so we can
// read/write its roomtype-tracking fields.
//=========================================================
static unsigned char* PlayerPrivateData(edict_t* pClient)
{
	if (!pClient)
		return NULL;

	void* priv = EngineGetPrivateData(pClient);
	if (priv)
		return (unsigned char*)priv;

	entvars_t* pevClient = EngineGetVarsOfEnt(pClient);
	if (!pevClient)
		return NULL;

	priv = EngineAllocPrivateData(pClient, PLAYER_PRIVATE_DATA_SIZE);
	if (!priv)
		return NULL;

	memset(priv, 0, PLAYER_PRIVATE_DATA_SIZE);

	CBasePlayer* pPlayer = new (priv) CBasePlayer();
	pPlayer->pev = pevClient;
	pPlayer->m_pGlobals = GlobalsFromEntvars(pevClient);

	return (unsigned char*)priv;
}

//=========================================================
// Spawn
//=========================================================
void CEnvSound::Spawn()
{
	// Stagger the first think so multiple env_sounds don't all fire together.
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + RandomFloat(0.0f, 0.5f);
}

//=========================================================
// KeyValue
//=========================================================
void CEnvSound::KeyValue(KeyValueData* pkvd)
{
	if (!pkvd)
		return;

	if (strcmp(pkvd->szKeyName, kKeyRadius) == 0)
	{
		m_flRadius = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}

	if (strcmp(pkvd->szKeyName, kKeyRoomtype) == 0)
	{
		m_flRoomtype = (float)atof(pkvd->szValue);
		pkvd->fHandled = 1;
	}
}

//=========================================================
// Think
//
// Each think: find a client in this entity's PVS. If this env_sound
// already owns that player's roomtype, keep it refreshed; otherwise, if
// the player is in line-of-sight range and we are closer than the
// current owner, take over and tell the client to switch DSP roomtype.
//=========================================================
void CEnvSound::Think(CBaseEntity* pOther)
{
	edict_t* pClient = EngineFindClientInPVS();

	// No client, or client has no index -> idle for 0.75s.
	if (!pClient || !EngineIndexOfEdict(pClient))
	{
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.75f;
		return;
	}

	unsigned char* pPlayer = PlayerPrivateData(pClient);
	if (!pPlayer)
	{
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.75f;
		return;
	}

	edict_t** ppRoomEnv = (edict_t**)(pPlayer + PLR_ROOM_ENV_EDICT);
	float* pRoomType = (float*)(pPlayer + PLR_ROOM_TYPE);
	float* pRoomRange = (float*)(pPlayer + PLR_ROOM_RANGE);

	edict_t* pSelf = EdictFromEntvars(pev);

	// Is this player's roomtype still owned by a valid env_sound, and is it us?
	if (!*ppRoomEnv
		|| !EngineIndexOfEdict(*ppRoomEnv)
		|| pSelf != *ppRoomEnv)
	{
		// Either nobody owns the roomtype, or a different env_sound does.
		// Try to take it over if this player is in range and we are closer.
		entvars_t* pevClient = EngineGetVarsOfEnt(pClient);

		float flRange;
		if (FEnvSoundInRange(pev, pevClient, &flRange)
			&& (*pRoomRange > (double)flRange || ((*(int*)pRoomRange) & 0x7FFFFFFF) == 0))
		{
			EngineGetVarsOfEnt(*ppRoomEnv);

			*ppRoomEnv = pSelf;
			*pRoomType = m_flRoomtype;
			*pRoomRange = flRange;

			// Tell this client to switch to our roomtype.
			void* globals = m_pGlobals ? m_pGlobals : GlobalsFromEntvars(pev);
			int* pMsgEntity = (int*)((unsigned char*)globals + GLOBALS_MSG_ENTITY);

			*pMsgEntity = EngineIndexOfEdict(pClient);
			EngineWriteByte(1, SVC_ROOMTYPE);
			EngineWriteShort(1, (short)(int)m_flRoomtype);
			*pMsgEntity = 0;
		}

		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.25f;
		return;
	}

	// We already own this player's roomtype.
	if (((*(int*)pRoomType) & 0x7FFFFFFF) == 0 || ((*(int*)pRoomRange) & 0x7FFFFFFF) == 0)
	{
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.75f;
		return;
	}

	// Re-check range; if the player walked out, release ownership.
	entvars_t* pevClient = EngineGetVarsOfEnt(pClient);

	float flRange;
	if (!FEnvSoundInRange(pev, pevClient, &flRange))
	{
		*pRoomRange = 0.0f;
		*pRoomType = 0.0f;
		PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.75f;
		return;
	}

	*pRoomRange = flRange;
	PevFloat(pev, PEV_NEXTTHINK) = GlobalTime() + 0.25f;
}

//=========================================================
// ambient_generic
//=========================================================
DLLEXPORT void ambient_generic(entvars_t* pev)
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

		CAmbientGeneric* self = new (privateData) CAmbientGeneric();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}

//=========================================================
// env_sound
//=========================================================
DLLEXPORT void env_sound(entvars_t* pev)
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
		privateData = EngineAllocPrivateData(edict, 36);
		if (!privateData)
			return;

		memset(privateData, 0, 36);

		CEnvSound* self = new (privateData) CEnvSound();
		self->pev = entvars;
		self->m_pGlobals = GlobalsFromEntvars(entvars);
	}
}
