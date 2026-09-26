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
#ifndef CBASE_H
#define CBASE_H

/*

Class Hierarchy

CBaseEntity
	CBaseDelay
		CBaseToggle
			CBaseTrigger
	CBaseAnimating
		CBaseMonster
			CBasePlayer
*/

#include "extdll.h"
#include "util.h"

// Classify() values
#define CLASS_NONE				0
#define CLASS_HUMAN_MILITARY	1
#define CLASS_ALIEN_MILITARY	2
#define CLASS_HOUNDEYE			3
#define CLASS_PLAYER			4
#define CLASS_PLAYER_ALLY		5	// never attacks the player
#define CLASS_MACHINE			6
#define CLASS_BULLCHICKEN		7
#define CLASS_HEADCRAB			8
#define CLASS_PANTHER			9

// BloodColor() values
#define BLOOD_COLOR_RED			70
#define BLOOD_COLOR_YELLOW		195

class CBaseEntity;

typedef void (CBaseEntity::*ENTITYFUNCPTR)(CBaseEntity *pOther);

// direction of the last damage, towards the inflictor
extern Vector g_vecAttackDir;

//
// Base Entity. All entity types derive from this.
//
class CBaseEntity
{
public:
	entvars_t		*pev;			// the engine's variables for this entity

	ENTITYFUNCPTR	m_pfnThink;
	ENTITYFUNCPTR	m_pfnTouch;
	ENTITYFUNCPTR	m_pfnUse;
	ENTITYFUNCPTR	m_pfnBlocked;

	CBaseEntity();

	virtual void	Spawn();
	virtual void	KeyValue(KeyValueData *pkvd);
	virtual int		ObjectCaps();
	virtual int		Save(SAVERESTOREDATA *pSaveData);
	virtual int		Restore(SAVERESTOREDATA *pSaveData);
	virtual void	Think(CBaseEntity *pOther);
	virtual void	Touch(CBaseEntity *pOther);
	virtual void	Use(CBaseEntity *pOther);
	virtual void	Blocked(CBaseEntity *pOther);
	virtual int		Classify();
	virtual void	SetActivity(int activity);
	virtual int		BloodColor();
	virtual void	AlertSound();
	virtual void	Pain(float flDamage);
	virtual void	Death(int iDeathType);
	virtual void	IdleSound();
	virtual int		CheckAttacks(entvars_t *pevEnemy, float flDist);
	virtual int		TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage);

	void FireBullets(int cShots, const Vector &vecDirShooting, float flSpreadRight, float flSpreadUp, int iBulletType, float flDistance);

	// common think/touch/use functions
	void SUB_Remove(CBaseEntity *pOther);
	void SUB_DoNothing(CBaseEntity *pOther);

	template <typename T> void SetThink(void (T::*pfn)(CBaseEntity *))		{ m_pfnThink = static_cast<ENTITYFUNCPTR>(pfn); }
	template <typename T> void SetTouch(void (T::*pfn)(CBaseEntity *))		{ m_pfnTouch = static_cast<ENTITYFUNCPTR>(pfn); }
	template <typename T> void SetUse(void (T::*pfn)(CBaseEntity *))		{ m_pfnUse = static_cast<ENTITYFUNCPTR>(pfn); }
	template <typename T> void SetBlocked(void (T::*pfn)(CBaseEntity *))	{ m_pfnBlocked = static_cast<ENTITYFUNCPTR>(pfn); }

	// clear with SetThink(NULL) etc.
	void SetThink(ENTITYFUNCPTR pfn)	{ m_pfnThink = pfn; }
	void SetTouch(ENTITYFUNCPTR pfn)	{ m_pfnTouch = pfn; }
	void SetUse(ENTITYFUNCPTR pfn)		{ m_pfnUse = pfn; }
	void SetBlocked(ENTITYFUNCPTR pfn)	{ m_pfnBlocked = pfn; }

	static CBaseEntity *Instance(edict_t *pent)		{ return (CBaseEntity *)GET_PRIVATE(pent); }
	static CBaseEntity *Instance(entvars_t *pev)	{ return Instance(ENT(pev)); }
	static CBaseEntity *Instance(EOFFSET eoffset)	{ return Instance(ENT(eoffset)); }

	// private data is allocated by the engine, which zero fills it
	void *operator new(size_t stAllocateBlock, entvars_t *pev)	{ return ALLOC_PRIVATE(ENT(pev), (int)stAllocateBlock); }
	void operator delete(void *pMem, entvars_t *pev)			{ }
};

//
// Returns the private data of an entity, creating the entity
// and/or its private data when needed.
//
template <class T> T *GetClassPtr(T *a)
{
	entvars_t *pev = (entvars_t *)a;

	// allocate entity if necessary
	if (pev == NULL)
		pev = VARS(CREATE_ENTITY());

	// get the private data
	a = (T *)GET_PRIVATE(ENT(pev));

	if (a == NULL)
	{
		// allocate private data
		a = new (pev) T;
		a->pev = pev;
	}

	return a;
}

//
// Map entity factory: the engine calls mapClassName with the entity's variables.
//
#define LINK_ENTITY_TO_CLASS(mapClassName, DLLClassName) \
	extern "C" DLLEXPORT void mapClassName(entvars_t *pev); \
	void mapClassName(entvars_t *pev) \
	{ \
		gpGlobals = pev->pSystemGlobals; \
		GetClassPtr((DLLClassName *)pev); \
	}

//
// Generic delay entity: fires its targets after an optional delay.
//
class CBaseDelay : public CBaseEntity
{
public:
	float		m_flDelay;
	string_t	m_iszKillTarget;

	CBaseDelay();

	void KeyValue(KeyValueData *pkvd);

	void SUB_UseTargets();
	void DelayThink(CBaseEntity *pOther);
};

//
// Studio model animation.
//
class CBaseAnimating : public CBaseEntity
{
public:
	float	m_flFrameRate;			// computed FPS for current sequence
	float	m_flGroundSpeed;		// computed linear movement rate for current sequence
	int		m_fSequenceFinished;	// flag set when the current sequence ends

	CBaseAnimating();

	int GetAnimationEventFlags(float flInterval);
	void ResetSequenceInfo(float flInterval);
	void AdvanceAnimation(float flInterval);
};

void GetSequenceInfo(void *pmodel, entvars_t *pev, float *pflFrameRate, float *pflGroundSpeed);

//
// generic Toggle entity.
//
#define TS_AT_TOP		0
#define TS_AT_BOTTOM	1
#define TS_GOING_UP		2
#define TS_GOING_DOWN	3

class CBaseToggle : public CBaseDelay
{
public:
	int				m_toggle_state;
	float			m_flActivateFinished;	// like attack_finished, but for doors
	float			m_flMoveDistance;		// how far a door should slide or rotate
	float			m_flWait;
	float			m_flLip;
	float			m_flTWidth;				// for plats
	float			m_flTLength;			// for plats

	Vector			m_vecPosition1;
	Vector			m_vecPosition2;

	int				m_cTriggersLeft;		// trigger_counter only, # of activations remaining
	float			m_flHeight;
	EOFFSET			m_hActivator;
	ENTITYFUNCPTR	m_pfnCallWhenMoveDone;
	Vector			m_vecFinalDest;
	Vector			m_vecFinalAngle;

	unsigned char	m_bHealthValue;			// some entities use this for health values
	unsigned char	m_bMoveSnd;				// sound a door makes while moving
	unsigned char	m_bStopSnd;				// sound a door makes when it stops

	CBaseToggle();

	void KeyValue(KeyValueData *pkvd);

	void LinearMove(const Vector &vecDest, float flSpeed);
	void LinearMoveDone(CBaseEntity *pOther);
	void AngularMove(const Vector &vecDestAngle, float flSpeed);
	void AngularMoveDone(CBaseEntity *pOther);

	template <typename T> void SetMoveDone(void (T::*pfn)(CBaseEntity *))	{ m_pfnCallWhenMoveDone = static_cast<ENTITYFUNCPTR>(pfn); }
	void SetMoveDone(ENTITYFUNCPTR pfn)										{ m_pfnCallWhenMoveDone = pfn; }
};

//
// Brush trigger volume.
//
class CBaseTrigger : public CBaseToggle
{
public:
	void InitTrigger();
};

void SetMovedir(entvars_t *pev);

#endif // CBASE_H
