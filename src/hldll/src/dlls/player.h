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
#ifndef PLAYER_H
#define PLAYER_H

#include "basemonster.h"

// m_afPhysicsFlags
#define PFLAG_ONLADDER		(1<<0)

// Player state carried across level changes in gpGlobals->pSpawnParms
typedef struct
{
	float	items;
	float	unused[2];
	float	health;
	float	armorvalue;
	float	ammo[4];
	float	weapon;
	float	armortype;				// armortype * 100
	int		iFlags;					// FL_DUCKING when the player was ducked
	int		fLandmark;				// the level change found an info_landmark
	char	szLandmarkName[20];
	Vector	vecLandmarkOffset;		// player origin relative to the landmark
	Vector	velocity;
	Vector	angles;
	Vector	v_angle;
} SPAWNPARMS;

class CBasePlayer : public CBaseMonster
{
public:
	float			m_flPlayerFrameRate;		// the player animates with its own sequence info
	float			m_flPlayerGroundSpeed;
	float			m_flAttackFinished;			// cannot fire again until this time
	float			m_flPauseTime;				// velocity is held at zero until this time
	unsigned char	m_bBloodColor;				// set at spawn, never read
	int				m_fPlayerSequenceFinished;
	float			m_flNextGrenadeTime;
	float			m_flFlashLightTime;
	float			m_flNextUseTime;
	float			m_flSwimSoundTime;
	float			m_flFallVelocity;
	edict_t			*m_pentSndLast;				// last sound entity to modify player room type
	float			m_flSndRoomtype;			// last roomtype set by sound entity
	float			m_flSndRange;				// dist from player to sound entity
	float			m_flTimeStepSound;
	float			m_flTimeWeaponIdle;
	int				m_afPhysicsFlags;
	int				m_iLadderStepCount;			// climbing frames since the last climbing sound
	int				m_iLadderPunchSide;			// the climbing view punch alternates sides
	short			m_iLadderClimbSpeed;

	CBasePlayer();

	void Spawn();
	int Classify();
	void SetActivity(int activity);
	void Death(int iDeathType);

	int SetAnimation(int playerAnim);
	virtual void Jump();
	virtual void Duck();
	virtual void PreThink();
	virtual void PostThink();

	void WaterMove();
	void CheckWaterJump();
	void DeadThink(CBaseEntity *pOther);
	void PlayerClimb();
	void WeaponIdle();
	void PlayerUse();
	void ItemPostFrame();
	void ImpulseCommands();
	void PrimaryAttack();
	void ApplyFallDamage(float flFallVelocity);
	void SwitchWeaponModel();
	void SelectWeapon(int iWeapon);
	void SelectWeaponReverse(int iWeapon);
};

#endif // PLAYER_H
