#pragma once

#include "../public/edict.h"
#include "../public/eiface.h"

class CBaseEntity
{
public:
	entvars_t* pev;
	void* m_pGlobals;

	CBaseEntity();

	virtual void Spawn();
	virtual void KeyValue(KeyValueData* pkvd);
	virtual int ObjectCaps();
	virtual int Save(SAVERESTOREDATA* pSaveData);
	virtual int Restore(SAVERESTOREDATA* pSaveData);
	virtual void Think(CBaseEntity* pOther);
	virtual void Touch(CBaseEntity* pOther);
	virtual void Use(CBaseEntity* pOther);
	virtual void Blocked(CBaseEntity* pOther);
	virtual int Classify();
	virtual void SetActivity(int activity);
	virtual int BloodColor();
	virtual void AlertSound();
	virtual void Pain(float flDamage);
	virtual void Death(int gibType);
	virtual void IdleSound();
	virtual int CheckAttacks(entvars_t* pevEnemy, float flDist);
	virtual int TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage);

	float GlobalTime() const;
	void SetNextThink(float delay);
	void SetRemoveThink();
	void FireBullets(int cShots, const float* vecDirShooting, float flSpreadUp, float flSpreadRight, int iBulletType, float flDistance);

protected:
	typedef void (CBaseEntity::*ThinkFunc)(CBaseEntity* pOther);
	typedef void (CBaseEntity::*EntityFunc)(CBaseEntity* pOther);
	typedef void (CBaseEntity::*UseFunc)(CBaseEntity* pOther);

	template <typename T>
	void SetThink(void (T::*pfn)(CBaseEntity*))
	{
		m_pfnThink = (ThinkFunc)pfn;
	}

	template <typename T>
	void SetTouch(void (T::*pfn)(CBaseEntity*))
	{
		m_pfnTouch = (EntityFunc)pfn;
	}

	template <typename T>
	void SetBlocked(void (T::*pfn)(CBaseEntity*))
	{
		m_pfnBlocked = (EntityFunc)pfn;
	}

	template <typename T>
	void SetUse(void (T::*pfn)(CBaseEntity*))
	{
		m_pfnUse = (UseFunc)pfn;
	}

	// Non-template overloads so SetThink(NULL)/SetTouch(NULL)/etc. clear the callback
	// (the templates above cannot deduce T from a null pointer constant).
	void SetThink(ThinkFunc pfn) { m_pfnThink = pfn; }
	void SetTouch(EntityFunc pfn) { m_pfnTouch = pfn; }
	void SetBlocked(EntityFunc pfn) { m_pfnBlocked = pfn; }
	void SetUse(UseFunc pfn) { m_pfnUse = pfn; }

	// CBaseEntity matches the original 28-byte layout exactly:
	//   [0] vtable  [4] pev  [8] m_pGlobals  [12] m_pfnThink
	//   [16] m_pfnTouch  [20] m_pfnUse  [24] m_pfnBlocked
	void RemoveThink(CBaseEntity* pOther);
	void SetDoNothingThink() { m_pfnThink = &CBaseEntity::DoNothingThink; }
	void SetDoNothingTouch() { m_pfnTouch = &CBaseEntity::DoNothingTouch; }
	void SetDoNothingUse() { m_pfnUse = &CBaseEntity::DoNothingUse; }
	void DoNothingThink(CBaseEntity* pOther);
	void DoNothingTouch(CBaseEntity* pOther);
	void DoNothingUse(CBaseEntity* pOther);

	ThinkFunc m_pfnThink;
	EntityFunc m_pfnTouch;
	UseFunc m_pfnUse;
	EntityFunc m_pfnBlocked;
};

CBaseEntity* GetEntity(edict_t* pent);
