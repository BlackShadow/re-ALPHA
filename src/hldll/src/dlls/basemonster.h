#pragma once

#include "hl_types.h"
#include "animating.h"
#include "../public/vector.h"

class CBaseMonster : public CBaseAnimating
{
public:
	CBaseMonster();

	void WalkMonsterStart(CBaseEntity* pOther);
	void MonsterThink(CBaseEntity* pOther);
	int SquadRecruit(int searchRadius);

	void SetMonsterActivity(int activity) { m_Activity = activity; }
	void SetMonsterIdealActivity(int activity) { m_IdealActivity = activity; }
	int MonsterActivity() const { return m_Activity; }
	int MonsterIdealActivity() const { return m_IdealActivity; }
	void TogglePlayerUse(entvars_t* pevPlayer);

	void SetSquadSize(unsigned int size) { m_iSquadSize = size; }
	unsigned int SquadSize() const { return m_iSquadSize; }
	entvars_t* SquadNext() const { return m_pSquadNext; }

	// Shared monster damage handler (binary vtable slot 17).
	// All ~13 monsters whose slot-17 resolves to the binary inherit this.
	int TakeDamage(entvars_t* inflictor, entvars_t* attacker, float damage);

	// Shared death/gib dispatch. Drives the per-monster
	// death virtual (binary slot 14 = Death) once a monster runs out of
	// health. The argument is the attacker's entity index, which the routine
	// stashes into pev->enemy. Also reachable directly from custom TakeDamage
	// handlers (e.g. the turret) like the original.
	void Killed(int attackerIndex);

protected:
	float UpdateEnemyInfo(entvars_t* pevEnemy);
	BOOL CheckMeleeAttack(entvars_t* pevEnemy);
	BOOL CheckRangeAttack(entvars_t* pevEnemy);
	Vector FindCover(entvars_t* pevEnemy);
	Vector FindRetreat(entvars_t* pevEnemy);
	Vector FindShootPosition(entvars_t* pevEnemy);

	// Shared death-activity selector. Used as the default
	// Death behavior (binary slot 14 stub == SetDeathActivity(0)).
	void SetDeathActivity(int type);

	virtual void SetActivity(int activity);
	virtual void IdleSound();
	virtual void AlertSound();
	virtual int CheckAttacks(entvars_t* pevEnemy, float flDist);
	virtual int BloodColor();

	// Pain reaction (binary vtable slot 13). The shared TakeDamage dispatches
	// this with the inbound damage when a monster survives a hit. Default does
	// nothing (nullsub for monsters without a pain reaction).
	virtual void Pain(float flDamage);

	// Death reaction (binary vtable slot 14). The shared Killed dispatch raises
	// this with the death/gib type. Default reproduces the binary
	// SetDeathActivity(0); monsters with custom deaths override it.
	virtual void Death(int gibType);

	// Binary slot 18 is present on the monster/player vtable but has no known
	// translated caller yet. Keeping it here preserves player slots 19-22.
	virtual void MonsterSlot18();

protected:
	int m_Activity;
	int m_IdealActivity;
	float m_flNextAttack;
	int m_bloodColor;

	Vector m_vecMoveGoal;
	Vector m_vecEnemyLKP;
	Vector m_vecRoute[5];

	entvars_t* m_pMoveTarget;
	entvars_t* m_pSquadLeader;
	entvars_t* m_pSquadNext;
	unsigned int m_iSquadSize;

	float m_flGoalRadius;
	float m_flDistTooFar;
	float m_flNextSoundTime;
	float m_flLastEnemySightTime;

	int m_iAmmo;
	unsigned char m_afEnemyFlags;
	unsigned char m_iRouteIndex;
	unsigned char m_iRouteGoal;
	unsigned char m_fSquadLeader;	// binary +0x138 squad-leader flag, DISTINCT from attack-bits m_afEnemyFlags (+0xf8); fills char-group padding (no size change)

	// Set by the shared TakeDamage when an attacker becomes the new enemy
	// (this+244 in the binary). The gib-direction goal the same path computes
	// (this+232..240) is reproduced via m_vecDeathGoal.
	entvars_t* m_hActivator;
	Vector m_vecDeathGoal;
};
