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

#pragma once

#include "basemonster.h"

//=========================================================
// CBasePlayer
//
// The local player. Derives from CBaseMonster because the
// binary player vtable extends the monster
// vtable (slots 0-18 mirror the monster defaults; the player
// adds slots 19-22: WaterMove, FallDamage, PreThink,
// PostThink).
//
// The player private data block is 452 bytes. Player-specific
// state past the CBaseMonster C++ members is reached by byte
// offset (the binary methods read those offsets directly),
// exactly as entvars_t is treated as opaque elsewhere.
//=========================================================
class CBasePlayer : public CBaseMonster
{
public:
	CBasePlayer();

	// virtual vtable overrides
	void Spawn();				// slot 0
	int Classify();				// slot 9
	void SetActivity(int playerAnim);	// slot 10, forwards to SetAnimation
	void Death(int gibType);		// slot 14

	// player-unique vtable methods (called through the object)
	int SetAnimation(int playerAnim);	// slot 10
	virtual void WaterMove();			// slot 19
	virtual void UpdateWaterLevel();		// slot 20 (duck/uncrouch)
	virtual void PreThink();			// slot 21
	virtual void PostThink();			// slot 22
	int TimeBasedDamage();

	// internal sub-routines reached by Pre/PostThink
	void WaterMoveSplash();				// drowning
	void CheckWaterJump();				// water-jump probe
	void DeadThink(CBaseEntity* pOther);
	void PlayerClimb();					// tail (ladder grab + climb gait)
	void FlashlightThink();
	void FlashlightToggle();
	void PlayerUse();					// USE scan
	void PlayerImpulseCommands();
	void ItemPreFrame();				// impulse selector
	void ItemPostFrame();				// weapon attack
	void ApplyFallDamage(float fallVel);
	void SwitchWeaponModel();
	void SelectWeapon(int id);
	void SelectWeaponReverse(int id);
	void StudioFrameAdvance();
	void PlayerRespawn();

private:
	// Trailing player state. The CBaseMonster portion above
	// consumes the first part of the 452-byte block; this
	// buffer extends the object so the raw byte offsets the
	// binary methods use stay inside the allocation.
	unsigned char m_playerData[452 - sizeof(CBaseMonster)];
};

float GlobalsFrameTime(void* globals);
