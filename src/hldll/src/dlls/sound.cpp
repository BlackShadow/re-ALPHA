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
// Sound - ambient sounds (ambient_generic) and the room
// type (DSP) of the player (env_sound)
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

// ambient_generic spawnflags
#define AMBIENT_SOUND_EVERYWHERE	1		// no attenuation, heard in the whole level
#define AMBIENT_SOUND_START_SILENT	2

#define AMBIENT_VOLUME_SCALE		0.1		// pev->health is the volume, 0 to 10

//=========================================================
// ambient_generic - plays pev->message. An unnamed ambient
// is a static sound, a named one is switched by use.
//=========================================================
class CAmbientGeneric : public CBaseEntity
{
public:
	void Spawn();

private:
	void RampThink(CBaseEntity *pOther);
	void ToggleUse(CBaseEntity *pOther);

	BOOL	m_fActive;		// the sound is on
};

void CAmbientGeneric::Spawn()
{
	const char *szSoundFile = STRING(pev->message);
	PRECACHE_SOUND(szSoundFile);

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if (!FStringNull(pev->targetname))
	{
		SetUse(&CAmbientGeneric::ToggleUse);

		int spawnflags = (int)pev->spawnflags;
		m_fActive = FALSE;
		if (!(spawnflags & AMBIENT_SOUND_START_SILENT))
			m_fActive = TRUE;

		SetThink(&CAmbientGeneric::RampThink);
		pev->nextthink = gpGlobals->time + 1.0f;
	}
	else
	{
		// an unnamed ambient plays once, as a static sound
		float flAttenuation;
		if ((int)pev->spawnflags & AMBIENT_SOUND_EVERYWHERE)
			flAttenuation = ATTN_NONE;
		else
			flAttenuation = ATTN_STATIC;

		float flVolume = (float)(pev->health * AMBIENT_VOLUME_SCALE);

		EMIT_AMBIENT_SOUND(pev->origin, szSoundFile, flVolume, flAttenuation);
	}
}

//=========================================================
// RampThink - starts the sound, unless the ambient
// starts silent
//=========================================================
void CAmbientGeneric::RampThink(CBaseEntity *pOther)
{
	const char *szSoundFile = STRING(pev->message);

	if (m_fActive)
	{
		float flAttenuation;
		if ((int)pev->spawnflags & AMBIENT_SOUND_EVERYWHERE)
			flAttenuation = ATTN_NONE;
		else
			flAttenuation = ATTN_STATIC;

		float flVolume = (float)(pev->health * AMBIENT_VOLUME_SCALE);

		EMIT_SOUND(ENT(pev), CHAN_WEAPON, szSoundFile, flVolume, flAttenuation);
	}

	SetThink(&CBaseEntity::SUB_DoNothing);
}

//=========================================================
// ToggleUse - switches the sound on and off. The null
// sound on the same channel stops it.
//=========================================================
void CAmbientGeneric::ToggleUse(CBaseEntity *pOther)
{
	const char *szSoundFile = STRING(pev->message);

	float flAttenuation;
	if ((int)pev->spawnflags & AMBIENT_SOUND_EVERYWHERE)
		flAttenuation = ATTN_NONE;
	else
		flAttenuation = ATTN_STATIC;

	float flVolume = (float)(pev->health * AMBIENT_VOLUME_SCALE);

	if (m_fActive)
	{
		m_fActive = FALSE;
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/null.wav", flVolume, flAttenuation);
	}
	else
	{
		m_fActive = TRUE;
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, szSoundFile, flVolume, flAttenuation);
	}
}

//=========================================================
// env_sound - gives its room type to the player when it is
// the closest env_sound in view, within m_flRadius
//=========================================================
class CEnvSound : public CBaseEntity
{
public:
	void Spawn();
	void Think(CBaseEntity *pOther);
	void KeyValue(KeyValueData *pkvd);

	float	m_flRadius;
	float	m_flRoomtype;
};

//=========================================================
// FEnvSoundInRange - the player must be in view of the
// env_sound, without a water surface in between, and within
// its radius. *pflRange gets the distance.
//=========================================================
static BOOL FEnvSoundInRange(entvars_t *pevSound, entvars_t *pevPlayer, float *pflRange)
{
	Vector vecSpot1 = pevSound->view_ofs + pevSound->origin;
	Vector vecSpot2 = pevPlayer->view_ofs + pevPlayer->origin;

	TraceResult tr;
	UTIL_TraceLine(vecSpot1, vecSpot2, ignore_monsters, ENT(pevSound), &tr);

	if ((tr.fInOpen && tr.fInWater) || tr.flFraction != 1.0f)
		return FALSE;

	float flRange = (tr.vecEndPos - vecSpot1).Length();

	CEnvSound *pSound = GetClassPtr((CEnvSound *)pevSound);
	if (pSound->m_flRadius < flRange)
		return FALSE;

	if (pflRange)
		*pflRange = flRange;

	return TRUE;
}

void CEnvSound::Spawn()
{
	// spread the thinking of the env_sounds
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.0f, 0.5f);
}

void CEnvSound::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		m_flRadius = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	if (FStrEq(pkvd->szKeyName, "roomtype"))
	{
		m_flRoomtype = (float)atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

//=========================================================
// Think - checks the player in our PVS. The closest
// env_sound in range sets the player's room type and keeps
// it until another one is closer.
//=========================================================
void CEnvSound::Think(CBaseEntity *pOther)
{
	edict_t *pentPlayer = FIND_CLIENT_IN_PVS();

	// no player in the PVS, check again later
	if (FNullEnt(pentPlayer))
	{
		pev->nextthink = gpGlobals->time + 0.75f;
		return;
	}

	CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)VARS(pentPlayer));

	edict_t *pentSound = ENT(pev);

	// another env_sound, or none, sets the room type: the closest one wins
	if (FNullEnt(pPlayer->m_pentSndLast) || pentSound != pPlayer->m_pentSndLast)
	{
		entvars_t *pevPlayer = VARS(pentPlayer);

		float flRange;
		if (FEnvSoundInRange(pev, pevPlayer, &flRange)
			&& (pPlayer->m_flSndRange > flRange || pPlayer->m_flSndRange == 0.0f))
		{
			pPlayer->m_pentSndLast = pentSound;
			pPlayer->m_flSndRoomtype = m_flRoomtype;
			pPlayer->m_flSndRange = flRange;

			// send the new room type to the player
			gpGlobals->msg_entity = OFFSET(pentPlayer);
			WRITE_BYTE(MSG_ONE, SVC_ROOMTYPE);
			WRITE_SHORT(MSG_ONE, (short)m_flRoomtype);
			gpGlobals->msg_entity = 0;
		}

		pev->nextthink = gpGlobals->time + 0.25f;
		return;
	}

	// we set the room type, wait while it is cleared
	if (pPlayer->m_flSndRoomtype == 0.0f || pPlayer->m_flSndRange == 0.0f)
	{
		pev->nextthink = gpGlobals->time + 0.75f;
		return;
	}

	// out of range: clear the room type and range, the next
	// env_sound in range takes over
	entvars_t *pevPlayer = VARS(pentPlayer);

	float flRange;
	if (!FEnvSoundInRange(pev, pevPlayer, &flRange))
	{
		pPlayer->m_flSndRange = 0.0f;
		pPlayer->m_flSndRoomtype = 0.0f;
		pev->nextthink = gpGlobals->time + 0.75f;
		return;
	}

	pPlayer->m_flSndRange = flRange;
	pev->nextthink = gpGlobals->time + 0.25f;
}

LINK_ENTITY_TO_CLASS(ambient_generic, CAmbientGeneric);
LINK_ENTITY_TO_CLASS(env_sound, CEnvSound);
