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
// Lights - switchable light entities (light, light_spot)
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define SF_LIGHT_START_OFF		1

// light styles from 32 up belong to named lights and can be switched
#define LIGHTSTYLE_SWITCHABLE	32

class CLight : public CBaseEntity
{
public:
	void Spawn();
	void KeyValue(KeyValueData *pkvd);
	void Use(CBaseEntity *pOther);

private:
	int		m_iStyle;
};

void CLight::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "style"))
	{
		m_iStyle = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	// light_spot pitch, accepted but not used
	if (FStrEq(pkvd->szKeyName, "pitch"))
		pkvd->fHandled = TRUE;
}

//=========================================================
// Spawn - unnamed lights are already in the lightmaps and
// are removed, named ones set up their switchable style
//=========================================================
void CLight::Spawn()
{
	if (FStringNull(pev->targetname))
	{
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	if (m_iStyle >= LIGHTSTYLE_SWITCHABLE)
	{
		// "a" is dark, "m" is normal brightness
		if ((int)pev->spawnflags & SF_LIGHT_START_OFF)
			LIGHT_STYLE(m_iStyle, "a");
		else
			LIGHT_STYLE(m_iStyle, "m");
	}
}

//=========================================================
// Use - toggles the light, SF_LIGHT_START_OFF holds the
// current state
//=========================================================
void CLight::Use(CBaseEntity *pOther)
{
	if (m_iStyle < LIGHTSTYLE_SWITCHABLE)
		return;

	if ((int)pev->spawnflags & SF_LIGHT_START_OFF)
	{
		LIGHT_STYLE(m_iStyle, "m");
		pev->spawnflags = (float)((int)pev->spawnflags & ~SF_LIGHT_START_OFF);
	}
	else
	{
		LIGHT_STYLE(m_iStyle, "a");
		pev->spawnflags = (float)((int)pev->spawnflags | SF_LIGHT_START_OFF);
	}
}

LINK_ENTITY_TO_CLASS(light, CLight);
LINK_ENTITY_TO_CLASS(light_spot, CLight);
