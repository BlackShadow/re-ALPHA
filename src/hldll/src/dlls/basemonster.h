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
#ifndef BASEMONSTER_H
#define BASEMONSTER_H

#include "cbase.h"

//
// Monster states. MonsterThink runs the AI for the current state and
// SetActivity plays the matching animation.
//
enum
{
	MONSTERSTATE_NONE = 0,
	MONSTERSTATE_IDLE,					// stand around, play idle sounds
	MONSTERSTATE_IDLE2,
	MONSTERSTATE_IDLE3,
	MONSTERSTATE_WALK,					// follow the path_corner in pev->goalentity
	MONSTERSTATE_COMBAT_FACE,			// face the enemy and attack when possible
	MONSTERSTATE_COMBAT_IDLE,			// wait for the enemy to come into view
	MONSTERSTATE_COMBAT,				// enemy in view, chase when it gets too far
	MONSTERSTATE_CHASE,					// run to the enemy's last known position
	MONSTERSTATE_HUNT,					// search for the enemy along the route
	MONSTERSTATE_FOLLOW,				// follow the player that used us
	MONSTERSTATE_MOVE_LEFT,				// move to m_vecMoveGoal, on the left
	MONSTERSTATE_MOVE_RIGHT,			// move to m_vecMoveGoal, on the right
	MONSTERSTATE_PAIN = 18,
	MONSTERSTATE_ALERT = 20,			// turn to the enemy, then attack
	MONSTERSTATE_WAIT = 22,				// do nothing until the state is changed
	MONSTERSTATE_RETREAT,				// move to m_vecMoveGoal, then face the enemy
	MONSTERSTATE_FIND_COVER = 25,
	MONSTERSTATE_FIND_RETREAT,
	MONSTERSTATE_FIND_SHOOT_POSITION,
	MONSTERSTATE_MELEE_ATTACK = 29,
	MONSTERSTATE_RANGE_ATTACK,
	MONSTERSTATE_ATTACK,				// keep attacking while CheckAttacks succeeds
	MONSTERSTATE_RELOAD,
	MONSTERSTATE_HEAVY_PAIN,
	MONSTERSTATE_FLINCH,
	MONSTERSTATE_DIE1,					// death animations, see SetDeathActivity
	MONSTERSTATE_DIE2,
	MONSTERSTATE_DIE3,
	MONSTERSTATE_DIE4,
	MONSTERSTATE_DIE5,
	MONSTERSTATE_DEAD = 42,				// death animation finished
};

// SetDeathActivity types, each plays MONSTERSTATE_DIE1 + type
#define DEATH_NORMAL		0
#define DEATH_VIOLENT		1			// health driven below GIB_HEALTH
#define NUM_DEATH_TYPES		5

#define GIB_HEALTH			-30.0f		// health below which a monster is blown apart

// m_afEnemyFlags
#define ENEMY_IN_VIEWCONE	(1<<0)
#define ENEMY_VISIBLE		(1<<1)
#define ENEMY_SEEN			(1<<2)		// m_vecEnemyLKP was updated from a sighting

// spawnflags
#define SF_MONSTER_WAIT_TILL_SEEN	1	// only notice the player when the player looks at us

#define MONSTER_ROUTE_SIZE	5

#define MONSTER_THINK_INTERVAL	0.1f

//
// generic Monster
//
class CBaseMonster : public CBaseAnimating
{
public:
	int				m_MonsterState;
	int				m_IdealMonsterState;	// state to return to after attacks and moves
	float			m_flNextAttack;			// cannot attack again until this time
	int				m_bloodColor;

	Vector			m_vecMoveGoal;			// destination of the move states
	Vector			m_vecEnemyLKP;			// last known position of enemy
	Vector			m_vecRoute[MONSTER_ROUTE_SIZE];

	entvars_t		*m_pMoveTarget;			// entity we follow
	entvars_t		*m_pSquadLeader;
	entvars_t		*m_pSquadNext;			// next member of the squad ring
	unsigned int	m_iSquadSize;

	float			m_flGoalRadius;			// how close to get to m_pMoveTarget
	float			m_flDistTooFar;			// chase the enemy when it is further than this
	float			m_flNextSoundTime;
	float			m_flLastEnemySightTime;

	int				m_iAmmo;
	unsigned char	m_afEnemyFlags;
	unsigned char	m_iRouteIndex;
	unsigned char	m_iRouteGoal;
	unsigned char	m_fSquadLeader;

	entvars_t		*m_pevAttacker;			// last attacker that became our enemy
	Vector			m_vecAttackerLKP;		// point towards the last attacker

	CBaseMonster();

	virtual void	SetActivity(int activity);
	virtual int		BloodColor();
	virtual void	AlertSound();
	virtual void	Pain(float flDamage);
	virtual void	Death(int iDeathType);
	virtual void	IdleSound();
	virtual int		CheckAttacks(entvars_t *pevEnemy, float flDist);
	virtual int		TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage);

	void WalkMonsterStart(CBaseEntity *pOther);
	void MonsterThink(CBaseEntity *pOther);

	void Killed(EOFFSET eoffsetAttacker);
	void SetDeathActivity(int iDeathType);

	int SquadRecruit(int searchRadius);
	void TogglePlayerUse(entvars_t *pevPlayer);

	float UpdateEnemyInfo(entvars_t *pevEnemy);
	BOOL CheckMeleeAttack(entvars_t *pevEnemy);
	BOOL CheckRangeAttack(entvars_t *pevEnemy);
	Vector FindCover(entvars_t *pevEnemy);
	Vector FindRetreat(entvars_t *pevEnemy);
	Vector FindShootPosition(entvars_t *pevEnemy);
};

#endif // BASEMONSTER_H
