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

class CBasePlayer : public CBaseMonster
{
public:
	unsigned char	m_padding1[68];				// transitional: removed with the layout placeholders
	float			m_flPlayerFrameRate;		// the player animates with its own sequence info
	float			m_flPlayerGroundSpeed;
	float			m_flAttackFinished;			// cannot fire again until this time
	unsigned char	m_padding2[16];
	float			m_flPauseTime;				// velocity is held at zero until this time
	unsigned char	m_padding3[4];
	int				m_fPlayerSequenceFinished;
	unsigned char	m_padding4[28];
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
	unsigned char	m_padding5[64];
	int				m_iLadderStepCount;
	int				m_iLadderPunchSide;
	short			m_iLadderClimbSpeed;

	CBasePlayer();

	void Spawn();
	int Classify();
	void SetActivity(int playerAnim);
	void Death(int iDeathType);

	int SetAnimation(int playerAnim);
	virtual void WaterMove();
	virtual void UpdateWaterLevel();
	virtual void PreThink();
	virtual void PostThink();
	int TimeBasedDamage();

	void WaterMoveSplash();
	void CheckWaterJump();
	void DeadThink(CBaseEntity *pOther);
	void PlayerClimb();
	void FlashlightThink();
	void FlashlightToggle();
	void PlayerUse();
	void PlayerImpulseCommands();
	void ItemPreFrame();
	void ItemPostFrame();
	void ApplyFallDamage(float fallVel);
	void SwitchWeaponModel();
	void SelectWeapon(int id);
	void SelectWeaponReverse(int id);
	void StudioFrameAdvance();
	void PlayerRespawn();
};

float GlobalsFrameTime(void *globals);

#endif // PLAYER_H
