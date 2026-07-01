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
// Player - CBasePlayer (the local player)
//
//   PutClientInServer export the binary (size 452, vtable)
//   Spawn vtable slot 0 the binary
//   SetAnimation vtable slot 10 the binary
//   Death vtable slot 14 the binary
//   WaterMove vtable slot 19 the binary (drown/jump out of water)
//   UpdateWaterLevel vtable slot 20 the binary (duck/uncrouch box swap)
//   PreThink vtable slot 21 the binary
//   PostThink vtable slot 22 the binary
//
// entvars_t is opaque (alpha byte offsets via PevFloat/PevInt/
// PevVector). Player-private state past the CBaseMonster C++
// members is reached at the binary byte offsets the original
// methods used (SelfInt/SelfFloat), since the object is a flat
// 452-byte block constructed in place.
//=========================================================

#include <new>
#include <math.h>
#include <string.h>
#include "basemonster.h"
#include "cbase.h"
#include "cbase_toggle.h"
#include "player.h"
#include "enginefuncs.h"
#include "hl_exports.h"
#include "monsters.h"
#include "utils.h"
#include "weapons.h"

//=========================================================
// player private-data layout (byte offsets into the object)
//=========================================================
enum
{
	PLR_VIEWMODEL			= 272,	// active weapon id (drives view model)  pev+0x110
	PLR_WEAPONMODEL			= 276,	// items / weapon-model bitfield (23 at spawn) pev+0x114
	PLR_VIEWMODEL_STR		= 280,	// view model string index (pev+0x118)
	PLR_FIRE_LATCH			= 284,	// pev+0x11c per-fire latch zeroed by the
									// fire dispatch (the binary clears 0x11c
									// NOT the 0x118 view-model string)
	PLR_VIEWMODEL_FRAME		= 288,	// view model frame
	PLR_FREEZE_TIME			= 292,	// this+0x124 freeze/duck-hold time (zeroes velocity)
	PLR_FOV					= 296,	// this+0x128 70 byte at spawn (m_iFOV-ish)
	PLR_300					= 300,	// scratch float (anim-done flag)
	PLR_USE_TIME			= 340,	// this+0x154 next +USE scan time
	PLR_NEXT_GRENADE		= 332,	// this+0x14c next IN_ATTACK2 grenade-launch time
	PLR_FLASH_TIME			= 336,	// this+0x150 next flashlight battery tick
	PLR_NEXT_FOOTSTEP		= 364,	// this+0x16c next footstep sound time
	PLR_NEXT_FLASH			= 368,	// this+0x170 next flashlight think time
	PLR_CLIMB_LATCH			= 372,	// this+0x174 ladder-grab latch (bit0)
	PLR_FALL_VELOCITY		= 348,	// this+0x15c fall velocity
	PLR_WATER_SOUND_TIME	= 344,	// this+0x158 next deep-water entry-splash sound time
	PLR_STEP_COUNT			= 440,	// this+0x1b8 ladder-climb gait accumulator
	PLR_PAIN_SIDE			= 444,	// this+0x1bc climb view-punch left/right toggle
	PLR_STEP_SIZE			= 448,	// this+0x1c0 ladder-climb stride (signed short)
	PLR_GAIT_FRAME			= 264,	// this+0x108 SetAnimation gait-frame scratch
	PLR_GAIT_YAW			= 268,	// this+0x10c SetAnimation gait-yaw scratch
};

//=========================================================
// entvars_t byte offsets used by the player that are not yet
// named in utils.h (values verified against the decompile)
//=========================================================
enum
{
	PEV_PUNCHANGLE_X		= 112,	// pev+0x70 view punch.x
	PEV_PUNCHANGLE_Z		= 120,	// pev+0x78 view punch.z component reused by code
	PEV_WATERLEVEL			= 408,	// pev+0x198 water level (0,1,2,3 as floats *0.5)
	PEV_WATERTYPE			= 412,	// pev+0x19c water contents type (-1..-5 as floats)
	PEV_DEADFLAG			= 324,	// pev+0x144 dead state (0,1,2,3 as floats)
	PEV_BUTTON				= 340,	// pev+0x154 pev->button bitfield
	PEV_THINK_ANGLES		= 352,	// pev+0x160 angle vector fed to MakeVectors in PreThink
	PEV_PLR_VIEWOFS			= 328,	// pev+0x148 view offset vector (== PEV_VIEWOFS); the
									// PreThink/PostThink guard compares it against
									// the engine spawn-sentinel vector g_vecSpawnSentinel
	PEV_DMGTAKE				= 500,	// pev+0x1f4 damage taken this frame (drown dmg)
	PEV_DMGSAVE				= 504,	// pev+0x1f8 dmg save (lava/slime re-tick)
	PEV_AIRTIME				= 508,	// pev+0x1fc drown air clock
	PEV_PAINTIME			= 512,	// pev+0x200 pain finished
	PEV_MAXSPEED			= 392,	// pev+0x188 max speed (alias of gravity slot in alpha)
	PEV_WATERJUMPTIME		= 396,	// pev+0x18c FL_WATERJUMP timeout
	PEV_IMPULSE				= 344,	// pev+0x158 pev->impulse
	PEV_EXTRA1				= 232,	// pev+0xe8 nextthink / extra scratch reused on respawn
	PEV_WEAPONMODEL_FRAME	= 280,	// pev+0x118 weapon model frame
};

//=========================================================
// flags (pev->flags, pev+380)
//=========================================================
enum
{
	FL_ONGROUND				= 0x200,
	FL_INWATER				= 0x10,
	FL_WATERJUMP			= 0x800,
	FL_ONGROUND_HINT		= 0x1000,	// engine "want ground" bit toggled each frame
	FL_DUCKING				= 0x4000,
};

//=========================================================
// player-state accessors (opaque, byte offset into object)
//=========================================================
static inline float& SelfFloat(void* self, size_t off)
{
	return *(float *)((unsigned char *)self + off);
}

static inline int& SelfInt(void* self, size_t off)
{
	return *(int *)((unsigned char *)self + off);
}

static inline short& SelfShort(void* self, size_t off)
{
	return *(short *)((unsigned char *)self + off);
}

//=========================================================
// constants pulled from the decompile
//=========================================================
#define PLR_VOL				1.0f
#define PLR_GLAUNCHER_VOL	0.75f
#define PLR_ATTN			0.8f	// 0x3f4ccccd

static const char kPlayer[]			= "player";
static const char kModelsDoctor[]	= "models/doctor.mdl";

static const char kStep1[] = "player/pl_step1.wav";
static const char kStep2[] = "player/pl_step2.wav";
static const char kStep3[] = "player/pl_step3.wav";
static const char kStep4[] = "player/pl_step4.wav";

static const char kJump1[] = "player/pl_jump1.wav";
static const char kJump2[] = "player/pl_jump2.wav";
static const char kJumpLand[] = "player/pl_jumpland2.wav";

static const char kWater1[] = "common/water1.wav";
static const char kWater2[] = "common/water2.wav";

static const char kOutWater[] = "common/outwater.wav";
static const char kInH2O[] = "player/inh2o.wav";
static const char kInLava[] = "player/inlava.wav";
static const char kSlimeBurn[] = "player/slimbrn2.wav";

static const char kGasp1[] = "player/gasp1.wav";
static const char kGasp2[] = "player/gasp2.wav";

static const char kGLauncher1[] = "weapons/glauncher.wav";
static const char kGLauncher2[] = "weapons/glauncher2.wav";

static const char kPain2[] = "player/pl_pain2.wav";	// ladder-climb gait
static const char kPain4[] = "player/pl_pain4.wav";
static const char kPain5[] = "player/pl_pain5.wav";
static const char kPain6[] = "player/pl_pain6.wav";
static const char kPain7[] = "player/pl_pain7.wav";

static const char kFallPain1[] = "player/pl_fallpain1.wav";	// hard-landing pain
static const char kFallPain2[] = "player/pl_fallpain2.wav";
static const char kFallPain3[] = "player/pl_fallpain3.wav";

static const char kClassname[] = "classname";
static const char kTargetname[] = "targetname";
static const char kInfoPlayerCoop[] = "info_player_coop";
static const char kInfoPlayerStart[] = "info_player_start";
static const char kInfoPlayerStart2[] = "info_player_start2";
static const char kInfoPlayerDeathmatch[] = "info_player_deathmatch";

static const char kFunc_ladder[] = "func_ladder";
static const char kFuncButton[] = "func_button";
static const char kFuncRotButton[] = "func_rot_button";
static const char kMomentaryRotButton[] = "momentary_rot_button";
static const char kFuncDoor[] = "func_door";
static const char kFuncDoorRotating[] = "func_door_rotating";
static const char kMonsterScientist[] = "monster_scientist";
static const char kMonsterBarney[] = "monster_barney";

//=========================================================
// "last spawn spot" round-robin cursor (DLL .data
// The binary). The spawn selector advances this so that
// successive players spawn at successive info_player spots.
//=========================================================
int g_LastSpawnEntIndex = 0;

//=========================================================
// Doctor player model index (DLL.data). Spawn
// stamps it after SetModel; PostThink and the
// engine ClientKill/Killed handlers restore pev->modelindex
// from it so a respawning/killed player keeps the doctor model.
// Shared with h_export.cpp (ClientKill).
//=========================================================
int g_PlayerModelIndex = 0;

// Engine respawn handshake, defined in h_export.cpp.
extern int ClientRespawn(entvars_t *pev);

//=========================================================
// IN_ATTACK2 / IN_ATTACK edge-debounce latches (DLL .data
// The binary). PlayerImpulseCommands fires
// the secondary (satchel / grenade) and primary attacks once
// per press; these gate the repeat until the button releases.
//=========================================================
static int g_fAttack2Pressed = 0;
static int g_fAttackPressed = 0;
int g_fDrawLines = 0;				// toggled by impulse 200

static void PlayerWeaponEvent(entvars_t* pev, void* globals, int animValue);
static void PlayerUseEntity(CBasePlayer* pPlayer, CBaseEntity* pEntity, int setToggleActivator);

//=========================================================
// spawn-sentinel view-offset vector (DLL.data).
// PreThink / PostThink run only
// when pev->view_ofs (pev+328) is NOT equal to this vector; it
// is the value the engine stores while the player slot is not
// yet fully in the world.
//=========================================================
static float g_vecSpawnSentinel[3] = { 0.0f, 0.0f, 0.0f };

static inline int ViewOfsIsSentinel(entvars_t* pev)
{
	return PevVector(pev, PEV_PLR_VIEWOFS).x == g_vecSpawnSentinel[0]
		&& PevVector(pev, PEV_PLR_VIEWOFS).y == g_vecSpawnSentinel[1]
		&& PevVector(pev, PEV_PLR_VIEWOFS).z == g_vecSpawnSentinel[2];
}

static void PlayerUseEntity(CBasePlayer* pPlayer, CBaseEntity* pEntity, int setToggleActivator)
{
	if (!pPlayer || !pEntity)
		return;

	int playerIndex = EngineIndexOfEdict(EdictFromEntvars(pPlayer->pev));

	if (setToggleActivator)
		((CBaseToggle*)pEntity)->SetActivator(playerIndex);

	void* globals = pPlayer->m_pGlobals;
	if (!globals)
	{
		pEntity->Use(pPlayer);
		return;
	}

	int oldSelf = *GlobalsInt(globals, GLOBALS_SELF_ENTINDEX);
	int oldActivator = *GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX);

	*GlobalsInt(globals, GLOBALS_SELF_ENTINDEX) = EngineIndexOfEdict(EdictFromEntvars(pEntity->pev));
	*GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX) = playerIndex;

	pEntity->Use(pPlayer);

	*GlobalsInt(globals, GLOBALS_SELF_ENTINDEX) = oldSelf;
	*GlobalsInt(globals, GLOBALS_OTHER_ENTINDEX) = oldActivator;
}

//=========================================================
// HL_COMPILE_TIME_ASSERT - object must fit the 452 byte block
//=========================================================
HL_COMPILE_TIME_ASSERT(sizeof(CBasePlayer) <= 452, CBasePlayer_private_data_size);

//=========================================================
// CBasePlayer ctor
//=========================================================
CBasePlayer::CBasePlayer()
	: m_playerData()	// zero the trailing private-data filler (already memset(0) before placement-new; satisfies C26495)
{
}

//=========================================================
// AdvanceAnimation / move-anim integration
//=========================================================
typedef struct PlayerStudioHeader
{
	unsigned char	pad0[92];
	int				numseq;
	int				seqindex;
} PlayerStudioHeader;

static unsigned char* PlayerGetSequenceDesc(const PlayerStudioHeader* hdr, int sequence)
{
	unsigned char* base;

	if (!hdr)
		return NULL;

	if (sequence < 0 || sequence >= hdr->numseq)
		return NULL;

	base = (unsigned char *)hdr;
	return base + hdr->seqindex + 104 * sequence;
}

static void PlayerGetSequenceInfo(void* pModel, entvars_t* pev, float* pFrameRate, float* pGroundSpeed)
{
	PlayerStudioHeader* hdr;
	unsigned char* seq;
	int sequence;
	int numFrames;
	float fps;
	float moveX;
	float moveY;
	float moveZ;
	float moveLength;

	if (!pFrameRate || !pGroundSpeed)
		return;

	if (!pModel || !pev)
		return;

	hdr = (PlayerStudioHeader *)pModel;
	sequence = PevInt(pev, PEV_SEQUENCE);
	seq = PlayerGetSequenceDesc(hdr, sequence);
	if (!seq)
	{
		*pFrameRate = 0.0f;
		*pGroundSpeed = 0.0f;
		return;
	}

	numFrames = *(int *)(seq + 48);
	if (!numFrames)
	{
		*pFrameRate = 256.0f;
		*pGroundSpeed = 0.0f;
		return;
	}

	fps = *(float *)(seq + 32);
	*pFrameRate = fps / (float)numFrames * 256.0f;

	moveX = *(float *)(seq + 76);
	moveY = *(float *)(seq + 80);
	moveZ = *(float *)(seq + 84);
	moveLength = (float)sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
	*pGroundSpeed = moveLength / (float)numFrames * fps;
}

static void PlayerResetSequenceInfo(CBasePlayer* pPlayer, float intervalScale)
{
	entvars_t* pev;
	void* self;
	float* pFrameRate;
	float* pGroundSpeed;
	void* pModel;

	if (!pPlayer || !pPlayer->pev)
		return;

	pev = pPlayer->pev;
	self = (void *)pPlayer;
	pFrameRate = &SelfFloat(self, PLR_GAIT_FRAME);
	pGroundSpeed = &SelfFloat(self, PLR_GAIT_YAW);
	pModel = EngineGetModelPtr(EdictFromEntvars(pev));

	PlayerGetSequenceInfo(pModel, pev, pFrameRate, pGroundSpeed);
	PevFloat(pev, PEV_ANIMTIME) = GlobalsTime(pPlayer->m_pGlobals);
	PevFloat(pev, PEV_FRAMERATE) = 1.0f;
	SelfFloat(self, PLR_300) = 0.0f;
	*pGroundSpeed = *pGroundSpeed * intervalScale;
}

static void PlayerStudioFrame(CBasePlayer* pPlayer, float interval)
{
	entvars_t* pev = pPlayer->pev;
	void* self = (void*)pPlayer;
	void* globals = pPlayer->m_pGlobals;
	float now = GlobalsTime(globals);

	// integrate yaw toward ideal
	if ((PevInt(pev, PEV_ANIMTIME) & 0x7FFFFFFF) != 0)
	{
		PevFloat(pev, PEV_ANIMTIME + 4) =
			(now - PevFloat(pev, PEV_ANIMTIME)) * PevFloat(pev, PEV_FRAMERATE)
			* SelfFloat(self, PLR_GAIT_FRAME) + PevFloat(pev, PEV_ANIMTIME + 4);
	}

	PevFloat(pev, PEV_ANIMTIME) = now;

	float& frame = PevFloat(pev, PEV_FRAME);
	if (PevInt(pev, PEV_FRAME) > (int)0x80000000)
		frame = (float)(int)(frame * 0.00390625f) * -256.0f + frame;
	if (PevInt(pev, PEV_FRAME) >= 1132462080)
		frame = (float)(int)(frame * 0.00390625f) * -256.0f + frame;

	SelfInt(self, PLR_300) = 0;

	float advance = PevFloat(pev, PEV_FRAMERATE) * SelfFloat(self, PLR_GAIT_FRAME) * interval
		+ PevFloat(pev, PEV_FRAME);

	if (SelfFloat(self, PLR_GAIT_FRAME) > 0.0f && advance > 256.0f)
		SelfInt(self, PLR_300) = 1;
	else if (advance <= 0.0f)
		SelfInt(self, PLR_300) = 1;
}

//=========================================================
// SetAnimation (vtable slot 10)
//   maps a player activity to a sequence and resets frame.
//=========================================================
int CBasePlayer::SetAnimation(int playerAnim)
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	int seq;

	switch (playerAnim)
	{
	case 1:
		seq = 7;
		break;
	case 4:
		seq = 0;
		break;
	case 30:
		if ((PevInt(pev, PEV_VELOCITY) & 0x7FFFFFFF) != 0
			|| (PevInt(pev, PEV_VELOCITY + 4) & 0x7FFFFFFF) != 0)
			seq = 2;
		else
			seq = 3;
		break;
	case 35:
		seq = 6;
		break;
	case 40:
		seq = 5;
		break;
	default:
		return playerAnim - 1;
	}

	if (PevInt(pev, PEV_SEQUENCE) != seq)
	{
		PevInt(pev, PEV_SEQUENCE) = seq;
		PevFloat(pev, PEV_FRAME) = 0.0f;
		PlayerResetSequenceInfo(this, 0.1f);
		if (seq > 6)
		{
			SelfInt(self, PLR_GAIT_FRAME) = 0;
			SelfInt(self, PLR_GAIT_YAW) = 0;
		}
	}
	return seq;
}

void CBasePlayer::SetActivity(int playerAnim)
{
	SetAnimation(playerAnim);
}

//=========================================================
// PlayerDeathSound
//=========================================================
static void PlayerDeathSound(entvars_t* pev)
{
	const char* sound = NULL;

	switch (RandomLong(1, 5))
	{
	case 1:
		sound = kPain5;
		break;
	case 2:
		sound = kPain6;
		break;
	case 3:
		sound = kPain7;
		break;
	}

	if (sound)
		EngineEmitSound(EdictFromEntvars(pev), 2, sound, PLR_VOL, PLR_ATTN);
}

//=========================================================
// Death (vtable slot 14)
//=========================================================
void CBasePlayer::Death(int gibType)
{
	HL_UNUSED(gibType);

	entvars_t* pev = this->pev;
	if (!pev)
		return;

	PevFloat(pev, PEV_MODELINDEX) = (float)(unsigned int)g_PlayerModelIndex;
	PevInt(pev, PLR_VIEWMODEL_STR) = 0;
	VecSet(VecPtr(PevVector(pev, PEV_PLR_VIEWOFS)), 0.0f, 0.0f, -8.0f);	// VEC_DEAD_VIEW z = -8.0 (0xC1000000), binary

	PevInt(pev, PEV_DEADFLAG) = 0x3F800000;
	PevInt(pev, PEV_SOLID) = 0;
	PevFloat(pev, PEV_MOVETYPE) = 6.0f;
	PevFloat(pev, PEV_FLAGS) = (float)(((int)PevFloat(pev, PEV_FLAGS)) & ~FL_ONGROUND);

	if (PevInt(pev, PEV_VELOCITY + 8) < 0x41200000)
		PevFloat(pev, PEV_VELOCITY + 8) += RandomFloat(0.0f, 300.0f);

	if ((unsigned int)PevInt(pev, PEV_HEALTH) <= 0xC2200000u)
	{
		PlayerDeathSound(pev);
		PevInt(pev, PEV_ANGLES) = 0;
		PevInt(pev, PEV_ANGLES + 8) = 0;
		SetAnimation(35);
		SetThink(&CBasePlayer::DeadThink);
		PevFloat(pev, PEV_NEXTTHINK) = 0.1f;
	}
}

//=========================================================
// SwitchWeaponModel
//=========================================================
void CBasePlayer::SwitchWeaponModel()
{
	entvars_t* pev = this->pev;
	switch (PevInt(pev, PLR_VIEWMODEL))
	{
	case 1:
		PevInt(pev, PLR_VIEWMODEL_STR) = EngineAllocString("models/v_crowbar.mdl");
		PevFloat(pev, PLR_VIEWMODEL_FRAME) = 99.0f;	// 1120272384
		break;
	case 2:
		PevInt(pev, PLR_VIEWMODEL_STR) = EngineAllocString("models/v_glock.mdl");
		PevFloat(pev, PLR_VIEWMODEL_FRAME) = 99.0f;	// 1120272384
		break;
	case 4:
		PevInt(pev, PLR_VIEWMODEL_STR) = EngineAllocString("models/v_mp5.mdl");
		PevFloat(pev, PLR_VIEWMODEL_FRAME) = 99.0f;	// 1120272384
		break;
	default:
		PevInt(pev, PLR_VIEWMODEL_STR) = 0;
		PevInt(pev, PLR_VIEWMODEL_FRAME) = 0;
		break;
	}
}

//=========================================================
// WaterMove (vtable slot 19)
//   handle jumping/wading out of and dropping into water.
//=========================================================
void CBasePlayer::WaterMove()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	int flags = (int)PevFloat(pev, PEV_FLAGS);

	if ((flags & FL_WATERJUMP) != 0)		// FL_WATERJUMP set: skip
		return;

	if (PevInt(pev, PEV_WATERLEVEL) < 0x40000000)	// waterlevel < 2.0f
	{
		// FL_ONGROUND + FL_ONGROUND_HINT -> jump out of water
		if ((flags & FL_ONGROUND) != 0 && (flags & FL_ONGROUND_HINT) != 0)
		{
			PevFloat(pev, PEV_FLAGS) = (float)(flags & ~FL_ONGROUND_HINT);
			int f2 = (int)PevFloat(pev, PEV_FLAGS);
			PevFloat(pev, PEV_FLAGS) = (float)(f2 & ~FL_ONGROUND);

			SetAnimation(40);
			PevInt(pev, PEV_BUTTON) &= ~2u;

			if (RandomFloat(0.0f, 1.0f) < 0.5f)
				EngineEmitSound(EdictFromEntvars(pev), 4, kJump1, PLR_VOL, PLR_ATTN);
			else
				EngineEmitSound(EdictFromEntvars(pev), 4, kJump2, PLR_VOL, PLR_ATTN);

			PevFloat(pev, PEV_VELOCITY + 8) += 270.0f;	// velocity.z
		}
	}
	else	// fully submerged: drop velocity by content + entry splash
	{
		int watertype = (int)PevFloat(pev, PEV_WATERTYPE);
		float depthVel;
		if (watertype == -4)
			depthVel = 80.0f;	// 0x42a00000
		else if (watertype == -3)
			depthVel = 100.0f;	// 0x42c80000
		else
			depthVel = 50.0f;	// 0x42480000

		PevFloat(pev, PEV_VELOCITY + 8) = depthVel;	// velocity.z

		float now = GlobalsTime(this->m_pGlobals);
		if (SelfFloat(self, PLR_WATER_SOUND_TIME) < now)
		{
			SelfFloat(self, PLR_WATER_SOUND_TIME) = now + 1.0f;
			if (RandomFloat(0.0f, 1.0f) >= 0.5f)
				EngineEmitSound(EdictFromEntvars(pev), 4, kWater2, PLR_VOL, PLR_ATTN);
			else
				EngineEmitSound(EdictFromEntvars(pev), 4, kWater1, PLR_VOL, PLR_ATTN);
		}
	}
}

//=========================================================
// UpdateWaterLevel / duck box swap (vtable slot 20)
//   IN_DUCK toggles the hull between standing and crouched.
//   Standing back up is gated on a WalkMove (engine slot 0x60)
//   test: if blocked, the player stays crouched.
//=========================================================
void CBasePlayer::UpdateWaterLevel()
{
	entvars_t* pev = this->pev;

	if ((PevInt(pev, PEV_BUTTON) & 4) == 0)		// not crouching -> try to stand up
	{
		float savedOrigin[3];
		VecCopy(savedOrigin, VecPtr(PevVector(pev, PEV_ORIGIN)));

		float standMins[3];
		float standMaxs[3];
		VecSet(standMaxs, 16.0f, 16.0f, 36.0f);
		VecSet(standMins, -16.0f, -16.0f, -36.0f);
		EngineSetSize(EdictFromEntvars(pev), standMins, standMaxs);

		PevVector(pev, PEV_ORIGIN).z += 18.0f;
		PevFloat(pev, PEV_FLAGS) = (float)((int)PevFloat(pev, PEV_FLAGS) | 0x400);	// : OR 0x400 before WalkMove

		// gate the stand-up on whether the player fits there now
		if (EngineWalkMove(EdictFromEntvars(pev), 0.0f, 0.0f) != 0.0f)
		{
			// clear, finish standing: un-duck and raise the eye
			PevFloat(pev, PEV_FLAGS) = (float)((int)PevFloat(pev, PEV_FLAGS) & ~FL_DUCKING);
			PevVector(pev, PEV_PLR_VIEWOFS).x = 0.0f;
			PevVector(pev, PEV_PLR_VIEWOFS).y = 0.0f;
			PevVector(pev, PEV_PLR_VIEWOFS).z = 28.0f;	// 0x41e00000
		}
		else
		{
			// blocked: shrink back to the crouch hull and restore origin
			float crouchMins[3];
			float crouchMaxs[3];
			VecSet(crouchMaxs, 16.0f, 16.0f, 18.0f);
			VecSet(crouchMins, -16.0f, -16.0f, -18.0f);
			EngineSetSize(EdictFromEntvars(pev), crouchMins, crouchMaxs);
			VecCopy(VecPtr(PevVector(pev, PEV_ORIGIN)), savedOrigin);
		}
	}
	else	// IN_DUCK held -> crouch
	{
		int flags = (int)PevFloat(pev, PEV_FLAGS);
		if ((flags & FL_DUCKING) == 0)				// not already ducked
		{
			float crouchMins[3];
			float crouchMaxs[3];
			VecSet(crouchMaxs, 16.0f, 16.0f, 18.0f);
			VecSet(crouchMins, -16.0f, -16.0f, -18.0f);
			EngineSetSize(EdictFromEntvars(pev), crouchMins, crouchMaxs);

			PevVector(pev, PEV_PLR_VIEWOFS).x = 0.0f;
			PevVector(pev, PEV_PLR_VIEWOFS).y = 0.0f;
			PevVector(pev, PEV_PLR_VIEWOFS).z = 12.0f;	// 0x41400000
			PevFloat(pev, PEV_FLAGS) = (float)(flags | FL_DUCKING);
		}
	}
}

//=========================================================
// SelectSpawnPoint
//   round-robins through the info_player_* entities, picking
//   the next one for this player. Returns the spawn-spot edict.
//=========================================================
static edict_t* SelectSpawnPoint(void* globals)
{
	// globals+0x90 / +0x94 / +0x9c are the deathmatch / coop /
	// start2 mode flags the engine stamps onto the worldspawn
	// globals block before PutClientInServer.
	int deathmatchFlag = *(int*)((unsigned char*)globals + 144) & 0x7FFFFFFF;	// +0x90 (a1[36])
	int coopFlag = *(int*)((unsigned char*)globals + 148) & 0x7FFFFFFF;	// +0x94 (a1[37])
	int start2Flag = *(int*)((unsigned char*)globals + 156) & 0x7FFFFFFF;	// +0x9c (a1[39])

	edict_t* spot = NULL;

	if (coopFlag != 0)
	{
		// coop: prefer info_player_coop, then info_player_start
		edict_t* start = EnginePEntityOfEntIndex(g_LastSpawnEntIndex);
		spot = EngineFindEntityByString(start, kClassname, kInfoPlayerCoop);
		if (spot && EngineIndexOfEdict(spot))
			goto found;

		start = EnginePEntityOfEntIndex(g_LastSpawnEntIndex);
		spot = EngineFindEntityByString(start, kClassname, kInfoPlayerStart);
		if (spot && EngineIndexOfEdict(spot))
			goto found;
	}
	else if (deathmatchFlag != 0)
	{
		// deathmatch: walk a random number of info_player_deathmatch spots
		int skip = (int)((double)(rand() & 0x7FFF) * 0.00076296274);
		int tries = 25;
		while (tries-- != 0)
		{
			edict_t* start = EnginePEntityOfEntIndex(g_LastSpawnEntIndex);
			spot = EngineFindEntityByString(start, kClassname, kInfoPlayerDeathmatch);
			if (spot && EngineIndexOfEdict(spot))
			{
				if (skip-- <= 0)
					goto found;
			}
		}
	}

	// single-player / fallback: info_player_start2 then info_player_start
	if (start2Flag == 0
		|| (spot = EngineFindEntityByString(NULL, kClassname, kInfoPlayerStart2)) == NULL
		|| EngineIndexOfEdict(spot) == 0)
	{
		spot = EngineFindEntityByString(NULL, kClassname, kInfoPlayerStart);
		if (!spot || EngineIndexOfEdict(spot) == 0)
			EngineAlertMessage(3, "PutClientInServer: no info_player_start on level\n", NULL);
	}

found:
	g_LastSpawnEntIndex = EngineIndexOfEdict(spot);
	return spot;
}

//=========================================================
// Spawn (vtable slot 0)
//=========================================================
void CBasePlayer::Spawn()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	void* globals = this->m_pGlobals;

	// landmark/transition parms left for us by the engine (globals+176)
	void* parms = *(void**)((unsigned char*)globals + 176);

	PevInt(pev, PEV_CLASSNAME) = EngineAllocString(kPlayer);
	PevFloat(pev, PEV_HEALTH) = 100.0f;			// 1120403456
	PevFloat(pev, PEV_TAKEDAMAGE) = 2.0f;
	PevFloat(pev, PEV_SOLID) = 3.0f;
	PevFloat(pev, PEV_MOVETYPE) = 3.0f;
	PevFloat(pev, PEV_MAXSPEED) = 100.0f;		// 1120403456
	PevFloat(pev, PEV_FLAGS) = 8.0f;			// 1090519040 (FL_CLIENT)
	PevFloat(pev, PEV_AIRTIME) = GlobalsTime(globals) + 12.0f;
	PevFloat(pev, PEV_DMGTAKE) = 2.0f;
	PevInt(pev, PEV_STUCK) = 0;
	PevInt(pev, PEV_SEQUENCE) = 9;
	PevInt(pev, PEV_DEADFLAG) = 0;

	// restore saved spawn parms
	{
		void* p2 = *(void**)((unsigned char*)globals + 176);
		if (!p2)
		{
			SetNewParms((client_t*)globals);
			p2 = *(void**)((unsigned char*)globals + 176);
		}
		if (p2)
		{
			float* p = (float*)p2;
			int* pi = (int*)p2;
			PevInt(pev, PEV_ITEMS_HIGH) = (int)p[0];	// pev+308
			PevInt(pev, PEV_HEALTH) = pi[3];			// pev+264
			PevInt(pev, 404) = pi[4];
			PevInt(pev, 292) = (int)p[5];
			PevInt(pev, 296) = (int)p[6];
			PevInt(pev, 300) = (int)p[7];
			PevInt(pev, 304) = (int)p[8];
			PevInt(pev, PLR_VIEWMODEL) = (int)p[9];		// pev+272 (active weapon)
			PevFloat(pev, 400) = p[10] * 0.01f;
			if ((((unsigned char*)p2)[45] & 0x40) != 0)
			{
				int fl = (int)PevFloat(pev, PEV_FLAGS) | FL_DUCKING;
				PevFloat(pev, PEV_FLAGS) = (float)fl;
				float mins[3];
				float maxs[3];
				VecSet(maxs, 16.0f, 16.0f, 18.0f);
				VecSet(mins, -16.0f, -16.0f, -18.0f);
				EngineSetSize(EdictFromEntvars(pev), mins, maxs);
			}
		}
	}

	// player-object scratch (object byte offsets, not pev)
	SelfInt(self, PLR_FLASH_TIME) = 0;
	SelfInt(self, PLR_NEXT_FOOTSTEP) = 0;
	*((unsigned char*)self + PLR_FOV) = 70;
	SelfInt(self, PLR_VIEWMODEL) = *(int*)((unsigned char*)globals + 124);	// next-idle time
	SelfInt(self, PLR_FREEZE_TIME) = 0;

	// place at spawn point (the binary selects the spot; the
	// landmark path copies the relative transition offset)
	if (!parms || *(int*)((unsigned char*)parms + 48) == 0)
	{
		edict_t* spot = SelectSpawnPoint(globals);
		PevInt(pev, PLR_VIEWMODEL) = 2;
		entvars_t* spotVars = spot ? EngineGetVarsOfEnt(spot) : NULL;
		if (spotVars)
		{
			PevVector(pev, PEV_ORIGIN).x = PevVector(spotVars, PEV_ORIGIN).x;
			PevVector(pev, PEV_ORIGIN).y = PevVector(spotVars, PEV_ORIGIN).y;
			PevVector(pev, PEV_ORIGIN).z = PevVector(spotVars, PEV_ORIGIN).z + 1.0f;
			PevVector(pev, PEV_ANGLES).x = PevVector(spotVars, PEV_ANGLES).x;
			PevVector(pev, PEV_ANGLES).y = PevVector(spotVars, PEV_ANGLES).y;
			PevVector(pev, PEV_ANGLES).z = PevVector(spotVars, PEV_ANGLES).z;
		}
	}
	else
	{
		// landmark transition: find the named landmark and place
		// relative to it, copying the saved view angles. The landmark
		// name is stored at parms+52.
		const char* landmarkName = (const char*)((unsigned char*)parms + 52);
		edict_t* landmark = EngineFindEntityByString(NULL, kTargetname, landmarkName);
		if (!landmark || EngineIndexOfEdict(landmark) == 0)
		{
			EngineAlertMessage(1, "No Landmark:%s\n", landmarkName);
			edict_t* spot = SelectSpawnPoint(globals);
			entvars_t* spotVars = spot ? EngineGetVarsOfEnt(spot) : NULL;
			if (spotVars)
			{
				PevVector(pev, PEV_ORIGIN).x = PevVector(spotVars, PEV_ORIGIN).x;
				PevVector(pev, PEV_ORIGIN).y = PevVector(spotVars, PEV_ORIGIN).y;
				PevVector(pev, PEV_ORIGIN).z = PevVector(spotVars, PEV_ORIGIN).z + 1.0f;
				PevVector(pev, PEV_ANGLES).x = PevVector(spotVars, PEV_ANGLES).x;
				PevVector(pev, PEV_ANGLES).y = PevVector(spotVars, PEV_ANGLES).y;
				PevVector(pev, PEV_ANGLES).z = PevVector(spotVars, PEV_ANGLES).z;
			}
		}
		else
		{
			entvars_t* lmVars = EngineGetVarsOfEnt(landmark);
			float* rel = (float*)((unsigned char*)parms + 72);	// saved relative offset
			PevVector(pev, PEV_ORIGIN).x = rel[0] + PevVector(lmVars, PEV_ORIGIN).x;
			PevVector(pev, PEV_ORIGIN).y = rel[1] + PevVector(lmVars, PEV_ORIGIN).y;
			PevVector(pev, PEV_ORIGIN).z = rel[2] + PevVector(lmVars, PEV_ORIGIN).z;
			// active weapon id saved at parms+36 ( (int)*(float*)(v4+36) )
			PevInt(pev, PLR_VIEWMODEL) = (int)*(float*)((unsigned char*)parms + 36);
			// saved view angles (parms+96) -> pev->angles
			float* savedAng = (float*)((unsigned char*)parms + 96);
			PevVector(pev, PEV_ANGLES).x = savedAng[0];
			PevVector(pev, PEV_ANGLES).y = savedAng[1];
			PevVector(pev, PEV_ANGLES).z = savedAng[2];
			// saved v_angle (parms+108) -> pev->v_angle (pev+352)
			float* savedView = (float*)((unsigned char*)parms + 108);
			PevVector(pev, PEV_THINK_ANGLES).x = savedView[0];
			PevVector(pev, PEV_THINK_ANGLES).y = savedView[1];
			PevVector(pev, PEV_THINK_ANGLES).z = savedView[2];
			PevFloat(pev, PEV_THINK_ANGLES) = -PevFloat(pev, PEV_THINK_ANGLES);	// negate pitch
		}
	}

	PevFloat(pev, PLR_FALL_VELOCITY) = 1.0f;		// 1065353216

	free(parms);
	*(void**)((unsigned char*)globals + 176) = NULL;

	EngineSaveSpawnParms(EdictFromEntvars(pev));
	EngineSetModel(EdictFromEntvars(pev), kModelsDoctor);

	// The binary = (int)pev->modelindex (the doctor model index the
	// engine just assigned). Stashed for ClientKill / PostThink to
	// restore the model after a respawn.
	g_PlayerModelIndex = (int)PevFloat(pev, PEV_MODELINDEX);

	{
		float mins[3];
		float maxs[3];
		VecSet(maxs, 16.0f, 16.0f, 36.0f);
		VecSet(mins, -16.0f, -16.0f, -36.0f);
		EngineSetSize(EdictFromEntvars(pev), mins, maxs);
	}

	PevInt(pev, PLR_WEAPONMODEL) = 23;
	SwitchWeaponModel();

	PevVector(pev, PEV_VIEWOFS).x = 0.0f;
	PevVector(pev, PEV_VIEWOFS).y = 0.0f;
	PevVector(pev, PEV_VIEWOFS).z = 28.0f;	// 1105199104 / 0x41e00000
}

//=========================================================
// Classify
//=========================================================
int CBasePlayer::Classify()
{
	return 4;
}

//=========================================================
// TimeBasedDamage (vtable slot 20 helper hook)
//=========================================================
int CBasePlayer::TimeBasedDamage()
{
	UpdateWaterLevel();
	return 0;
}

//=========================================================
// CheckWaterJump
//   traces 24 units forward along the (flattened) view vector
//   and, if there is a wall in front but clear space above, sets
//   FL_WATERJUMP and a +Z velocity to hop the player onto the
//   ledge. Called from PreThink whenever waterlevel == 2.0f.
//=========================================================
void CBasePlayer::CheckWaterJump()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	void* globals = this->m_pGlobals;

	float origin[3];
	VecCopy(origin, VecPtr(PevVector(pev, PEV_ORIGIN)));
	float flatStart[3];
	flatStart[0] = origin[0];
	flatStart[1] = origin[1];
	flatStart[2] = origin[2] + 8.0f;

	EngineMakeVectors(VecPtr(PevVector(pev, PEV_ANGLES)));

	// flatten the forward vector (globals+240) onto the XY plane
	float* fwd = (float*)GlobalsForward(globals);
	fwd[2] = 0.0f;
	float len = sqrtf(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
	if (len == 0.0f)
	{
		fwd[0] = 0.0f;
		fwd[1] = 0.0f;
		fwd[2] = 0.0f;
	}
	else
	{
		float inv = 1.0f / len;
		fwd[0] = fwd[0] * inv;
		fwd[1] = fwd[1] * inv;
		fwd[2] = fwd[2] * inv;
	}

	// trace forward 24 units from the eye-ish start
	float end[3];
	end[0] = fwd[0] * 24.0f + flatStart[0];
	end[1] = fwd[1] * 24.0f + flatStart[1];
	end[2] = fwd[2] * 24.0f + flatStart[2];

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(flatStart, end, 0, EdictFromEntvars(pev), &tr);

	if (tr.flFraction >= 1.0f)
		return;	// nothing in front: no ledge to climb

	// there is a wall; trace from higher up (origin.z + maxs.z) to
	// see if the space over the ledge is clear
	float maxsZ = PevVector(pev, PEV_MAXS).z;	// pev+200
	float highStart[3];
	highStart[0] = origin[0];
	highStart[1] = origin[1];
	highStart[2] = origin[2] + maxsZ;
	float highEnd[3];
	highEnd[0] = fwd[0] * 24.0f + origin[0];
	highEnd[1] = fwd[1] * 24.0f + origin[1];
	highEnd[2] = fwd[2] * 24.0f + highStart[2];

	// The binary writes -50 * planeNormal to pev+0x1CC (movedir, an unused
	// scratch here) at the binary..f93 -- NOT to velocity. Writing velocity
	// perturbed the player every time the waterjump probe ran.
	PevFloat(pev, PEV_MOVEDIR) = tr.vecPlaneNormal[0] * -50.0f;
	PevFloat(pev, PEV_MOVEDIR + 4) = tr.vecPlaneNormal[1] * -50.0f;
	PevFloat(pev, PEV_MOVEDIR + 8) = tr.vecPlaneNormal[2] * -50.0f;

	TraceResult tr2;
	memset(&tr2, 0, sizeof(tr2));
	EngineTraceLine(highStart, highEnd, 0, EdictFromEntvars(pev), &tr2);

	if (tr2.flFraction == 1.0f)
	{
		// clear above: hop onto the ledge
		int flags = (int)PevFloat(pev, PEV_FLAGS);
		PevFloat(pev, PEV_FLAGS) = (float)(flags | FL_WATERJUMP);
		PevFloat(pev, PEV_VELOCITY + 8) = 225.0f;		// 0x43610000
		flags = (int)PevFloat(pev, PEV_FLAGS);
		PevFloat(pev, PEV_FLAGS) = (float)(flags & ~FL_ONGROUND_HINT);
		PevFloat(pev, PEV_WATERJUMPTIME) = GlobalsTime(globals) + 2.0f;
	}

	HL_UNUSED(self);
}

//=========================================================
// PlayerClimb (ladder-climb tail of)
//   trace 24 units along the view forward vector; if it hits a
//   func_ladder set MOVETYPE_FLY and drive the climbing gait
//   (footstep/pain sounds + view-punch), otherwise release.
//=========================================================
void CBasePlayer::PlayerClimb()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	void* globals = this->m_pGlobals;

	// The binary re-derives the aim vectors from pev->angles (pev+76)
	// before the ladder trace, NOT from the v_angle used at the top of
	// PreThink. Without this the climb traces along the wrong forward.
	EngineMakeVectors(VecPtr(PevVector(pev, PEV_ANGLES)));

	float* fwd = (float*)GlobalsForward(globals);
	float start[3];
	VecCopy(start, VecPtr(PevVector(pev, PEV_ORIGIN)));
	float end[3];
	end[0] = start[0] + fwd[0] * 24.0f;
	end[1] = start[1] + fwd[1] * 24.0f;
	end[2] = start[2] + fwd[2] * 24.0f;

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(start, end, 0, EdictFromEntvars(pev), &tr);

	int onLadder = 0;
	if (TraceHitIndex(&tr))
	{
		edict_t* pHit = TraceHitEdict(&tr);
		entvars_t* pevHit = pHit ? EngineGetVarsOfEnt(pHit) : NULL;
		if (pevHit)
		{
			const char* cls = EngineStringFromIndex(PevInt(pevHit, PEV_CLASSNAME));
			if (cls && strcmp(cls, kFunc_ladder) == 0)
				onLadder = 1;
		}
	}

	if (!onLadder || (PevInt(pev, PEV_BUTTON) & 2) != 0)
	{
		// not on a ladder (or jumping off): release the grab
		int latch = SelfInt(self, PLR_CLIMB_LATCH);
		if ((latch & 1) != 0)
		{
			SelfInt(self, PLR_CLIMB_LATCH) = latch - 1;
			PevFloat(pev, PEV_MOVETYPE) = 3.0f;	// MOVETYPE_WALK (1077936128)
		}
		return;
	}

	// on a ladder: fly and drive the climbing gait
	PevFloat(pev, PEV_MOVETYPE) = 5.0f;		// MOVETYPE_FLY (1084227584)
	SelfInt(self, PLR_CLIMB_LATCH) |= 1;

	int button = PevInt(pev, PEV_BUTTON);
	if ((button & 8) != 0)				// IN_FORWARD: climb up
	{
		if (SelfShort(self, PLR_STEP_SIZE) <= 0)
			SelfShort(self, PLR_STEP_SIZE) = 200;
		PevFloat(pev, PEV_VELOCITY) = PevFloat(pev, PEV_VELOCITY) * 0.6f;
		PevFloat(pev, PEV_VELOCITY + 4) = PevFloat(pev, PEV_VELOCITY + 4) * 0.6f;
		PevFloat(pev, PEV_VELOCITY + 8) = (float)(int)SelfShort(self, PLR_STEP_SIZE);
		SelfShort(self, PLR_STEP_SIZE) -= 15;
		SelfInt(self, PLR_STEP_COUNT) += 1;

		// re-trace forward 24 + up 8 to check for the top of the ladder
		float climbStart[3];
		climbStart[0] = start[0] + fwd[0] * 24.0f;
		climbStart[1] = start[1] + fwd[1] * 24.0f;
		climbStart[2] = start[2] + fwd[2] * 24.0f;
		const float* up = GlobalsUp(globals);
		climbStart[0] += up[0] * 8.0f;
		climbStart[1] += up[1] * 8.0f;
		climbStart[2] += up[2] * 8.0f;
		float climbEnd[3];
		climbEnd[0] = start[0] + up[0] * 8.0f;
		climbEnd[1] = start[1] + up[1] * 8.0f;
		climbEnd[2] = start[2] + up[2] * 8.0f;

		TraceResult tr2;
		memset(&tr2, 0, sizeof(tr2));
		EngineTraceLine(climbEnd, climbStart, 0, EdictFromEntvars(pev), &tr2);	// binary traces NEAR(origin+up*8) -> FAR(+fwd*24); endpoints were swapped

		int topIsLadder = 0;
		if (TraceHitIndex(&tr2))
		{
			edict_t* pHit2 = TraceHitEdict(&tr2);
			entvars_t* pevHit2 = pHit2 ? EngineGetVarsOfEnt(pHit2) : NULL;
			if (pevHit2)
			{
				const char* cls2 = EngineStringFromIndex(PevInt(pevHit2, PEV_CLASSNAME));
				if (cls2 && strcmp(cls2, kFunc_ladder) == 0)
					topIsLadder = 1;
			}
		}

		if (!topIsLadder)
		{
			// reached the top: fling onto the ledge
			PevFloat(pev, PEV_VELOCITY) = fwd[0] * 200.0f;
			PevFloat(pev, PEV_VELOCITY + 4) = fwd[1] * 200.0f;
			PevFloat(pev, PEV_VELOCITY + 8) = fwd[2] * 200.0f + 275.0f;
		}
	}
	else if ((button & 0x10) != 0)		// IN_BACK: climb down
	{
		if (SelfShort(self, PLR_STEP_SIZE) >= 0)
			SelfShort(self, PLR_STEP_SIZE) = -200;
		PevFloat(pev, PEV_VELOCITY) = 0.0f;
		PevFloat(pev, PEV_VELOCITY + 4) = 0.0f;
		PevFloat(pev, PEV_VELOCITY + 8) = (float)(int)SelfShort(self, PLR_STEP_SIZE);
		SelfShort(self, PLR_STEP_SIZE) += 15;
		SelfInt(self, PLR_STEP_COUNT) += 1;
	}
	else								// idle on the ladder
	{
		PevFloat(pev, PEV_VELOCITY) = 0.0f;
		PevFloat(pev, PEV_VELOCITY + 4) = 0.0f;
		PevFloat(pev, PEV_VELOCITY + 8) = 0.0f;
	}

	// every 22 gait ticks emit a climbing scuff and a view-punch
	if (SelfInt(self, PLR_STEP_COUNT) >= 22)
	{
		float r = RandomFloat(0.0f, 1.0f);
		SelfInt(self, PLR_STEP_COUNT) = 0;
		SelfShort(self, PLR_STEP_SIZE) = 200;

		// The binary: the original uses 0.25/0.5/0.75 buckets.
		const char* sound;
		if (r < 0.25f)
			sound = kPain2;
		else if (r < 0.5f)
			sound = kPain4;
		else if (r < 0.75f)
			sound = kPain5;
		else
			sound = kPain6;
		EngineEmitSound(EdictFromEntvars(pev), 2, sound, PLR_VOL, PLR_ATTN);

		if (SelfInt(self, PLR_PAIN_SIDE))
		{
			PevFloat(pev, PEV_PUNCHANGLE_Z) = 7.0f;		// 0x40e00000
			PevFloat(pev, PEV_PUNCHANGLE_X) = -7.0f;	// 0xc0e00000
			SelfInt(self, PLR_PAIN_SIDE) = 0;
		}
		else
		{
			PevFloat(pev, PEV_PUNCHANGLE_Z) = -7.0f;	// 0xc0e00000
			PevFloat(pev, PEV_PUNCHANGLE_X) = -7.0f;	// 0xc0e00000
			SelfInt(self, PLR_PAIN_SIDE) = 1;
		}
	}
}

//=========================================================
// PreThink (vtable slot 21)
//=========================================================
void CBasePlayer::PreThink()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	void* globals = this->m_pGlobals;

	// The binary runs the think body only while the player is
	// in the world: it returns when pev->view_ofs (pev+328) equals
	// the engine spawn-sentinel vector. (FCOMP float compare.)
	if (ViewOfsIsSentinel(pev))
		return;

	EngineMakeVectors(VecPtr(PevVector(pev, PEV_THINK_ANGLES)));

	// drown / splash content damage and sounds, then the forward
	// water-jump probe whenever the player is chest-deep (== 2.0f)
	WaterMoveSplash();
	if (PevInt(pev, PEV_WATERLEVEL) == 0x40000000)	// 2.0f
		CheckWaterJump();

	if (PevInt(pev, PEV_DEADFLAG) >= 1065353216)	// dead (>= 1.0f)
	{
		DeadThink(NULL);
		return;
	}

	int button = PevInt(pev, PEV_BUTTON);
	if ((button & 2) != 0)
		WaterMove();								// IN_JUMP
	else
		PevFloat(pev, PEV_FLAGS) = (float)((int)PevFloat(pev, PEV_FLAGS) | FL_ONGROUND_HINT);

	if ((PevInt(pev, PEV_BUTTON) & 4) != 0
		|| ((int)PevFloat(pev, PEV_FLAGS) & FL_DUCKING) != 0)
		UpdateWaterLevel();							// IN_DUCK

	// freeze: zero velocity while the duck/idle hold timer is active
	if (GlobalsTime(globals) < SelfFloat(self, PLR_FREEZE_TIME))
	{
		PevVector(pev, PEV_VELOCITY).x = 0.0f;
		PevVector(pev, PEV_VELOCITY).y = 0.0f;
		PevVector(pev, PEV_VELOCITY).z = 0.0f;
	}

	// footstep sounds when moving fast on the ground
	Vector vel = PevVector(pev, PEV_VELOCITY);
	if (GlobalsTime(globals) > SelfFloat(self, PLR_NEXT_FOOTSTEP)
		&& ((int)PevFloat(pev, PEV_FLAGS) & FL_ONGROUND) != 0)
	{
		if (vel.x != 0.0f || vel.y != 0.0f || vel.z != 0.0f)
		{
			float speed = sqrtf(vel.x*vel.x + vel.y*vel.y + vel.z*vel.z);
			if (speed > 200.0f)
			{
				float r = RandomFloat(0.0f, 1.0f);
				SetAnimation(4);
				const char* step;
				if (r <= 0.25f)
					step = kStep1;
				else if (r <= 0.5f)
					step = kStep2;
				else if (r <= 0.75f)
					step = kStep3;
				else
					step = kStep4;
				EngineEmitSound(EdictFromEntvars(pev), 4, step, PLR_VOL, PLR_ATTN);
				SelfFloat(self, PLR_NEXT_FOOTSTEP) = GlobalsTime(globals) + 0.3f;
			}
		}
	}

	if (GlobalsTime(globals) > SelfFloat(self, PLR_NEXT_FLASH))
		FlashlightThink();

	// ladder grab / climbing gait
	PlayerClimb();
}

//=========================================================
// FlashlightThink hook
//=========================================================
void CBasePlayer::FlashlightThink()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	if (PevInt(pev, PLR_VIEWMODEL) == 4)	// flashlight item active
	{
		void* globals = this->m_pGlobals;
		*(int*)((unsigned char*)globals + 328) = EngineIndexOfEdict(EdictFromEntvars(pev));
		EngineWriteByte(1, 35);
		EngineWriteByte(1, 1);
		*(int*)((unsigned char*)globals + 328) = 0;
		float r = RandomFloat(0.0f, 5.0f);
		SelfFloat(self, PLR_NEXT_FLASH) = r + GlobalsTime(globals) + 10.0f;
	}
}

//=========================================================
// WaterMoveSplash / drowning
//=========================================================
void CBasePlayer::WaterMoveSplash()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	void* globals = this->m_pGlobals;
	float now = GlobalsTime(globals);

	if (PevInt(pev, PEV_MOVETYPE) == 1090519040			// MOVETYPE_NOCLIP (8.0f)
		|| (unsigned int)PevInt(pev, PEV_HEALTH) > 0x80000000)	// dead
		return;

	if (PevInt(pev, PEV_WATERLEVEL) == 0x40400000)		// waterlevel == 3.0f (head under)
	{
		// drowning: tick the air clock and apply drown damage
		if (PevFloat(pev, PEV_AIRTIME) < now && PevFloat(pev, PEV_PAINTIME) < now)
		{
			PevFloat(pev, PEV_DMGTAKE) += 2.0f;
			if (PevInt(pev, PEV_DMGTAKE) > 0x41700000)	// clamp drown dmg (> 15)
				PevInt(pev, PEV_DMGTAKE) = 0x41200000;	// to 10
			// apply the primed drown damage (vtable slot 17 == shared
			// CBaseMonster::TakeDamage / the binary, inflictor and
			// attacker = worldspawn entvars).
			entvars_t* world = EngineGetVarsOfEnt(EnginePEntityOfEntIndex(0));
			TakeDamage(world, world, PevFloat(pev, PEV_DMGTAKE));
			PevFloat(pev, PEV_PAINTIME) = now + 1.0f;
		}
	}
	else
	{
		// surfacing: gasp for air keyed to how long we were under
		if (PevFloat(pev, PEV_AIRTIME) < now)
			EngineEmitSound(EdictFromEntvars(pev), 2, kGasp2, PLR_VOL, PLR_ATTN);
		else if (PevFloat(pev, PEV_AIRTIME) < now + 9.0f)
			EngineEmitSound(EdictFromEntvars(pev), 2, kGasp1, PLR_VOL, PLR_ATTN);

		PevFloat(pev, PEV_AIRTIME) = now + 12.0f;	// reset air clock
		PevFloat(pev, PEV_DMGTAKE) = 2.0f;			// reset drown dmg
	}

	// out of water: clear contents flags and play the exit sound
	if (((int)PevFloat(pev, PEV_WATERLEVEL) & 0x7FFFFFFF) == 0)
	{
		if (((int)PevFloat(pev, PEV_FLAGS) & FL_INWATER) != 0)
		{
			EngineEmitSound(EdictFromEntvars(pev), 4, kOutWater, PLR_VOL, PLR_ATTN);
			PevFloat(pev, PEV_FLAGS) = (float)((int)PevFloat(pev, PEV_FLAGS) & ~FL_INWATER);
		}
		return;
	}

	// harmful contents content damage (vtable slot 17 == shared
	// CBaseMonster::TakeDamage / the binary, inflictor and attacker
	// = worldspawn entvars).
	entvars_t* worldDmg = EngineGetVarsOfEnt(EnginePEntityOfEntIndex(0));
	if (PevInt(pev, PEV_WATERTYPE) == -1063256064)		// CONTENTS_LAVA
	{
		// lava re-tick gated on the per-second damage clock
		if (now > PevFloat(pev, PEV_DMGSAVE))
			TakeDamage(worldDmg, worldDmg, PevFloat(pev, PEV_WATERLEVEL) * 10.0f);
	}
	else if (PevInt(pev, PEV_WATERTYPE) == -1065353216)	// CONTENTS_SLIME
	{
		PevFloat(pev, PEV_DMGSAVE) = now + 1.0f;
		TakeDamage(worldDmg, worldDmg, PevFloat(pev, PEV_WATERLEVEL) * 4.0f);
	}

	// first frame in harmful water: content entry sound + FL_INWATER
	if (((int)PevFloat(pev, PEV_FLAGS) & FL_INWATER) == 0)
	{
		if (PevInt(pev, PEV_WATERTYPE) == -1063256064)	// lava
			EngineEmitSound(EdictFromEntvars(pev), 4, kInLava, PLR_VOL, PLR_ATTN);
		if (PevInt(pev, PEV_WATERTYPE) == -1069547520)	// water
			EngineEmitSound(EdictFromEntvars(pev), 4, kInH2O, PLR_VOL, PLR_ATTN);
		if (PevInt(pev, PEV_WATERTYPE) == -1065353216)	// slime
			EngineEmitSound(EdictFromEntvars(pev), 4, kSlimeBurn, PLR_VOL, PLR_ATTN);
		PevFloat(pev, PEV_FLAGS) = (float)((int)PevFloat(pev, PEV_FLAGS) | FL_INWATER);
		PevFloat(pev, PEV_DMGSAVE) = 0.0f;
	}

	// water friction drag on velocity (unless mid water-jump)
	if (((int)PevFloat(pev, PEV_FLAGS) & FL_WATERJUMP) == 0)
	{
		float drag = GlobalsFrameTime(globals) * PevFloat(pev, PEV_WATERLEVEL) * 0.8f;
		Vector& v = PevVector(pev, PEV_VELOCITY);
		v.x = v.x - v.x * drag;
		v.y = v.y - v.y * drag;
		v.z = v.z - v.z * drag;
	}

	HL_UNUSED(self);
}

//=========================================================
// PlayerUse - the +USE scan
//   FindEntityInSphere around the player; for each entity whose
//   center lies in the player's forward cone (dot > 0.7) and is
//   an activatable class, fire its Use. Called from ItemPreFrame
//   when IN_USE (button & 0x20) is held.
//=========================================================
void CBasePlayer::PlayerUse()
{
	entvars_t* pev = this->pev;

	float origin[3];
	VecCopy(origin, VecPtr(PevVector(pev, PEV_ORIGIN)));

	edict_t* pEnt = EngineFindEntityInSphere(origin, 64.0f);	// 0x42800000
	EngineMakeVectors(VecPtr(PevVector(pev, PEV_ANGLES)));

	const float* fwd = GlobalsForward(this->m_pGlobals);

	while (pEnt && EngineIndexOfEdict(pEnt))
	{
		entvars_t* pevEnt = EngineGetVarsOfEnt(pEnt);
		if (pevEnt)
		{
			// center of the candidate's bounding box
			float center[3];
			center[0] = PevVector(pevEnt, PEV_ORIGIN).x
				+ 0.5f * (PevVector(pevEnt, PEV_MINS).x + PevVector(pevEnt, PEV_MAXS).x);
			center[1] = PevVector(pevEnt, PEV_ORIGIN).y
				+ 0.5f * (PevVector(pevEnt, PEV_MINS).y + PevVector(pevEnt, PEV_MAXS).y);
			center[2] = PevVector(pevEnt, PEV_ORIGIN).z
				+ 0.5f * (PevVector(pevEnt, PEV_MINS).z + PevVector(pevEnt, PEV_MAXS).z);

			float dot = (center[0] - origin[0]) * fwd[0]
				+ (center[1] - origin[1]) * fwd[1]
				+ (center[2] - origin[2]) * fwd[2];

			if (dot > 0.69999999f)
			{
				const char* cls = EngineStringFromIndex(PevInt(pevEnt, PEV_CLASSNAME));
				if (cls
					&& (strcmp(cls, kFuncButton) == 0
						|| strcmp(cls, kFuncRotButton) == 0
						|| strcmp(cls, kMomentaryRotButton) == 0))
				{
					// buttons only fire when not already locked/busy
					if ((PevInt(pevEnt, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
						return;
					CBaseEntity* pEntity = GetEntity(pEnt);
					if (pEntity)
					{
						// The binary stores the player
						// index into the target's m_hActivator (+0x60) UNCONDITIONALLY
						// for all three button classes, including momentary_rot_button.
						PlayerUseEntity(this, pEntity, 1);
					}
				}
				else if (cls
					&& (strcmp(cls, kFuncDoor) == 0
						|| strcmp(cls, kFuncDoorRotating) == 0))
				{
					// only USE-able doors (spawnflag 0x100) respond
					if (((int)PevFloat(pevEnt, PEV_SPAWNFLAGS) & 0x100) != 0)
					{
						CBaseEntity* pEntity = GetEntity(pEnt);
						if (pEntity)
							PlayerUseEntity(this, pEntity, 1);
					}
				}
				else if (cls
					&& (strcmp(cls, kMonsterScientist) == 0
						|| strcmp(cls, kMonsterBarney) == 0))
				{
					// talk-monster follow toggle handled by the monster
					CBaseEntity* pEntity = GetEntity(pEnt);
					if (pEntity)
						((CBaseMonster*)pEntity)->TogglePlayerUse(pev);
					return;
				}
			}
		}

		// advance to the next entity in the sphere (pev->chain)
		pEnt = EnginePEntityOfEntIndex(PevInt(pevEnt, PEV_CHAIN));
	}
}

//=========================================================
// PlayerImpulseCommands
//=========================================================
void CBasePlayer::PlayerImpulseCommands()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	void* globals = this->m_pGlobals;

	int viewmodel = PevInt(pev, PLR_VIEWMODEL);

	// gate the whole pass on the weapon's next-ready time (this+0x110)
	if (SelfFloat(self, PLR_VIEWMODEL) > GlobalsTime(globals))
		return;

	ItemPreFrame();

	// all three attack gates key off pev->button (pev+0x154), NOT pev->flags
	int button = PevInt(pev, PEV_BUTTON);

	// IN_CANCEL (0x40): force-select the holstered weapon, skip the attacks
	if ((button & 0x40) != 0)
	{
		if ((viewmodel & 0xFFFF0000) != 0)
			PevInt(pev, PLR_VIEWMODEL) = (int)(double)((unsigned int)(viewmodel & 0xFF0000) >> 16);
		return;
	}

	// IN_ATTACK2 (0x800): secondary fire - deploy a satchel or lob a grenade
	if ((button & 0x800) != 0)
	{
		// a satchel-capable selection (high weapon bits set): drop one
		// satchel per press, then release the secondary button.
		if ((viewmodel & 0xFFFF0000) != 0 && !g_fAttack2Pressed)
		{
			int itemId = viewmodel & 0xFF;
			unsigned int backupId = ((unsigned int)viewmodel & 0xFF0000u) >> 16;
			if (itemId != 0 && DeploySatchel(pev, itemId))
			{
				PevInt(pev, PLR_WEAPONMODEL) &= ~(1 << (itemId & 0x1F));
				SelectWeapon(itemId + 1);
				if (backupId == (unsigned int)viewmodel)
				{
					PevInt(pev, PLR_VIEWMODEL) = 0;
					SwitchWeaponModel();
				}
			}
			PevInt(pev, PEV_BUTTON) &= ~0x800;
			g_fAttack2Pressed = 1;
			return;
		}

		// MP5 grenade launcher (active weapon id 4) on its own cadence
		if (PevInt(pev, PLR_VIEWMODEL) == 4
			&& GlobalsTime(globals) > SelfFloat(self, PLR_NEXT_GRENADE))
		{
			g_fAttack2Pressed = 1;

			EngineMakeVectors(VecPtr(PevVector(pev, PEV_THINK_ANGLES)));
			PlayerWeaponEvent(pev, globals, 2);
			EngineEmitSound(EdictFromEntvars(pev), 1,
				(RandomFloat(0.0f, 1.0f) < 0.5f) ? kGLauncher1 : kGLauncher2,
				PLR_GLAUNCHER_VOL, PLR_ATTN);

			const float* fwd = GlobalsForward(globals);

			// velocity = forward * 800; start = eye + forward * 24
			float velocity[3];
			velocity[0] = fwd[0] * 800.0f;
			velocity[1] = fwd[1] * 800.0f;
			velocity[2] = fwd[2] * 800.0f;

			float start[3];
			start[0] = PevVector(pev, PEV_ORIGIN).x + PevVector(pev, PEV_VIEWOFS).x + fwd[0] * 24.0f;
			start[1] = PevVector(pev, PEV_ORIGIN).y + PevVector(pev, PEV_VIEWOFS).y + fwd[1] * 24.0f;
			start[2] = PevVector(pev, PEV_ORIGIN).z + PevVector(pev, PEV_VIEWOFS).z + fwd[2] * 24.0f;

			ThrowGrenade(pev, start, velocity);

			PevInt(pev, PEV_BUTTON) &= ~0x800;
			SelfFloat(self, PLR_NEXT_GRENADE) = GlobalsTime(globals) + 1.0f;
		}
	}
	else
	{
		g_fAttack2Pressed = 0;
	}

	// IN_ATTACK (0x1): primary fire / deploy-on-select
	if ((button & 1) != 0)
	{
		if (!g_fAttackPressed)
		{
			if ((viewmodel & 0xFFFF0000) != 0)
			{
				// a freshly selected weapon: commit its low byte and
				// swap the view model instead of firing this frame.
				PevInt(pev, PLR_VIEWMODEL) = viewmodel & 0xFF;
				PevInt(pev, PEV_BUTTON) &= ~1;
				SwitchWeaponModel();
				g_fAttackPressed = 1;
			}
			else
			{
				ItemPostFrame();
			}
		}
	}
	else
	{
		g_fAttackPressed = 0;
	}
}

//=========================================================
// SprayerAimHitsWall (the impulse 201/202 spawn gate inside
// The binary): MakeVectors from the player's aim, then trace
// 128 units forward from the eye. The sprayer projectile is only
// spawned when that probe strikes a surface (fraction != 1.0).
//=========================================================
static int SprayerAimHitsWall(entvars_t* pev, void* globals)
{
	EngineMakeVectors(VecPtr(PevVector(pev, PEV_THINK_ANGLES)));
	const float* fwd = GlobalsForward(globals);

	float start[3];
	start[0] = PevVector(pev, PEV_ORIGIN).x + PevVector(pev, PEV_VIEWOFS).x;
	start[1] = PevVector(pev, PEV_ORIGIN).y + PevVector(pev, PEV_VIEWOFS).y;
	start[2] = PevVector(pev, PEV_ORIGIN).z + PevVector(pev, PEV_VIEWOFS).z;

	float end[3];
	end[0] = start[0] + fwd[0] * 128.0f;
	end[1] = start[1] + fwd[1] * 128.0f;
	end[2] = start[2] + fwd[2] * 128.0f;

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(start, end, 0, EdictFromEntvars(pev), &tr);

	return tr.flFraction != 1.0f;
}

//=========================================================
// RemoveAimedDamageable (impulse 203 in)
//=========================================================
static void RemoveAimedDamageable(entvars_t* pev, void* globals)
{
	EngineMakeVectors(VecPtr(PevVector(pev, PEV_THINK_ANGLES)));
	const float* fwd = GlobalsForward(globals);

	float start[3];
	start[0] = PevVector(pev, PEV_ORIGIN).x;
	start[1] = PevVector(pev, PEV_ORIGIN).y;
	start[2] = PevVector(pev, PEV_ORIGIN).z + 16.0f;

	float end[3];
	end[0] = start[0] + fwd[0] * 1024.0f;
	end[1] = start[1] + fwd[1] * 1024.0f;
	end[2] = start[2] + fwd[2] * 1024.0f;

	TraceResult tr;
	memset(&tr, 0, sizeof(tr));
	EngineTraceLine(start, end, 1, EdictFromEntvars(pev), &tr);

	edict_t* pHit = TraceHitEdict(&tr);
	entvars_t* pevHit = pHit ? EngineGetVarsOfEnt(pHit) : NULL;
	if (pevHit && (PevInt(pevHit, PEV_TAKEDAMAGE) & 0x7FFFFFFF) != 0)
	{
		CBaseEntity* pEntity = GetEntity(pHit);
		if (pEntity)
			pEntity->SetRemoveThink();
	}
}

//=========================================================
// ItemPreFrame (impulse selector)
//=========================================================
void CBasePlayer::ItemPreFrame()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;
	void* globals = this->m_pGlobals;

	// IN_USE (button & 0x20): scan for a nearby entity to activate
	if ((PevInt(pev, PEV_BUTTON) & 0x20) != 0
		&& GlobalsTime(globals) > SelfFloat(self, PLR_USE_TIME))
	{
		PlayerUse();
		SelfFloat(self, PLR_USE_TIME) = GlobalsTime(globals) + 0.5f;
	}

	// The binary: fld dword [edi+0x158]; call __ftol.
	// pev->impulse (0x158) is FLOAT-encoded in this alpha; ftol-truncate before
	// switching. Reading it as a raw int yields the float bit-pattern (e.g.
	// 0x41200000 for impulse 10), which matches no case label, so the whole
	// switch fell through to default and NO impulse command (weapon select,
	// flashlight, sprayer) ever ran.
	int impulse = (int)PevFloat(pev, PEV_IMPULSE);
	switch (impulse)
	{
	case 1: case 2: case 3: case 4:
	case 5: case 6: case 7: case 8:
		SelectWeapon(impulse);
		break;
	case 10:
		// The binary case 10: MOV EAX,[pev+0x110]; AND 0xff; INC EAX
		// (RAW INT read of weapon_id, NOT a float). The previous
		// (int)PevFloat read garbage so next-weapon never advanced.
		SelectWeapon((PevInt(pev, PLR_VIEWMODEL) & 0xFF) + 1);
		break;
	case 11:
	{
		// The binary case 0xb: MOV EAX,[pev+0x110]; AND 0xff; DEC EAX;
		// JNS (signed) -> if (low - 1) < 0 use 0x1f. RAW INT read.
		int w = (PevInt(pev, PLR_VIEWMODEL) & 0xFF) - 1;
		if (w < 0)
			w = 31;
		SelectWeaponReverse(w);
		break;
	}
	case 100:
	{
		int f = (int)PevFloat(pev, PEV_STUCK);
		if ((f & 8) != 0)
			PevFloat(pev, PEV_STUCK) = (float)(f & 0xFFFFFFF7);
		else
			PevFloat(pev, PEV_STUCK) = (float)(f | 8);
		break;
	}
	case 200:
		if (g_fDrawLines)
		{
			g_fDrawLines = 0;
			EngineAlertMessage(1, "Lines Off\n");
		}
		else
		{
			g_fDrawLines = 1;
			EngineAlertMessage(1, "Lines On\n");
		}
		break;
	case 201:
		// repeating sprayer: paints a
		// six-decal trail along the aim, gated on hitting a wall.
		if (SprayerAimHitsWall(pev, globals))
			DeploySprayerRepeat(pev);
		break;
	case 202:
		// single-shot sprayer: one decal.
		if (SprayerAimHitsWall(pev, globals))
			DeploySprayerSingle(pev);
		break;
	case 203:
		RemoveAimedDamageable(pev, globals);
		break;
	default:
		break;
	}

	PevFloat(pev, PEV_IMPULSE) = 0.0f;	// stores 0 (field is float)
}

//=========================================================
// Weapon-fire constants (decoded from the binary
// The binary and the shared bullet-fire
// The binary).
//=========================================================
enum
{
	PEV_EFFECTS			= 140,	// pev+0x8c  effects bitfield (FLOAT-encoded int)
	EF_MUZZLEFLASH		= 2,	// pev->effects |= 2 on every weapon fire

	// per-weapon view-model SetAnimation activity argument
	PLAYER_ATTACK1		= 30,	// vtable SetAnimation(0x1e) after each fire
};

// glock muzzle/view-weapon event sent to the firing client
// (globals->msg_entity = IndexOfEdict(self); WriteByte(1, 35); ...).
static const char kGlock1[] = "weapons/pl_gun1.wav";
static const char kGlock2[] = "weapons/pl_gun2.wav";

static const char kMP5_1[] = "weapons/hks1.wav";
static const char kMP5_2[] = "weapons/hks2.wav";
static const char kMP5_3[] = "weapons/hks3.wav";

// muzzle / weapon-animation directed message (the WriteByte(1,35)
// pattern shared by all three fires, == the binary prologue and
// the FlashlightThink directed-message convention). 'animValue' is
// the second byte (0 for glock/mp5, swing dir 1/2 for the crowbar).
static void PlayerWeaponEvent(entvars_t* pev, void* globals, int animValue)
{
	*(int*)((unsigned char*)globals + 328) = EngineIndexOfEdict(EdictFromEntvars(pev));	// globals+0x148 msg_entity
	EngineWriteByte(1, 35);
	EngineWriteByte(1, animValue);
	*(int*)((unsigned char*)globals + 328) = 0;
}

//=========================================================
// FireGlock (weapon id 2)
//   muzzleflash + random pl_gun sound, GetAimVector-derived
//   forward, 0.025 cone, 2048-unit bullet, 0.3s cadence.
//=========================================================
static void FireGlock(CBasePlayer* pPlayer)
{
	entvars_t* pev = pPlayer->pev;
	void* globals = pPlayer->m_pGlobals;

	// pev->effects |= EF_MUZZLEFLASH (FLOAT-encoded int, ftol round-trip)
	PevFloat(pev, PEV_EFFECTS) = (float)((int)PevFloat(pev, PEV_EFFECTS) | EF_MUZZLEFLASH);

	// directed muzzle/weapon-anim message to the firing client
	PlayerWeaponEvent(pev, globals, 0);

	EngineEmitSound(EdictFromEntvars(pev), 1,
		(rand() & 1) ? kGlock1 : kGlock2, PLR_VOL, PLR_ATTN);

	// shooting direction == engine aim vector.
	EngineMakeVectors(VecPtr(PevVector(pev, PEV_THINK_ANGLES)));	// pev+0x160
	float dir[3];
	EngineGetAimVector(EdictFromEntvars(pev), 2048.0f, dir);

	pPlayer->FireBullets(1, dir, 0.025f, 0.025f, 0, 2048.0f);

	// next-attack time = globals->time + 0.3 (this+0x110)
	SelfFloat(pPlayer, PLR_VIEWMODEL) = GlobalsTime(globals) + 0.3f;
}

//=========================================================
// FireCrowbar (weapon id 1)
//   muzzle/swing message (1 or 2), no sound, 0.025 cone,
//   64-unit melee trace, 1.0s cadence.
//=========================================================
static void FireCrowbar(CBasePlayer* pPlayer)
{
	entvars_t* pev = pPlayer->pev;
	void* globals = pPlayer->m_pGlobals;

	// swing-direction byte = (rand & 1) ? 1: 2 (parity test)
	int swing = ((rand() & 1) == 1) ? 1 : 2;
	PlayerWeaponEvent(pev, globals, swing);

	EngineMakeVectors(VecPtr(PevVector(pev, PEV_THINK_ANGLES)));	// pev+0x160
	RandomFloat(100.0f, 150.0f);
	RandomFloat(50.0f, 70.0f);
	RandomFloat(-25.0f, 25.0f);

	float dir[3];
	EngineGetAimVector(EdictFromEntvars(pev), 1000.0f, dir);	// 0x447A0000 aim distance

	pPlayer->FireBullets(1, dir, 0.025f, 0.025f, 0, 64.0f);	// range 0x42800000

	// next-attack time = globals->time + 1.0 (this+0x110)
	SelfFloat(pPlayer, PLR_VIEWMODEL) = GlobalsTime(globals) + 1.0f;
}

//=========================================================
// FireMP5 (weapon id 4)
//   muzzleflash + random hks sound, 0.01 cone, 2048-unit
//   bullet, 0.1s cadence, sets the dynamic-light timer.
//=========================================================
static void FireMP5(CBasePlayer* pPlayer)
{
	entvars_t* pev = pPlayer->pev;
	void* globals = pPlayer->m_pGlobals;

	PevFloat(pev, PEV_EFFECTS) = (float)((int)PevFloat(pev, PEV_EFFECTS) | EF_MUZZLEFLASH);

	PlayerWeaponEvent(pev, globals, 0);

	const char* snd;
	float pick = RandomFloat(0.0f, 1.0f);
	if (pick < 0.33f)
		snd = kMP5_1;
	else if (pick < 0.66f)
		snd = kMP5_2;
	else
		snd = kMP5_3;
	EngineEmitSound(EdictFromEntvars(pev), 1, snd, PLR_VOL, PLR_ATTN);

	EngineMakeVectors(VecPtr(PevVector(pev, PEV_THINK_ANGLES)));	// pev+0x160
	float dir[3];
	EngineGetAimVector(EdictFromEntvars(pev), 2048.0f, dir);

	pPlayer->FireBullets(1, dir, 0.01f, 0.01f, 0, 2048.0f);

	// next-attack time = globals->time + 0.1 (this+0x110)
	SelfFloat(pPlayer, PLR_VIEWMODEL) = GlobalsTime(globals) + 0.1f;
	// muzzle dynamic-light timer = globals->time + 1.0 (this+0x170)
	SelfFloat(pPlayer, PLR_NEXT_FLASH) = GlobalsTime(globals) + 1.0f;
}

//=========================================================
// ItemPostFrame == fire dispatch
//   On primary attack: if alive, clear pev+0x11c (the
//   per-frame fire latch, NOT the viewmodel string at
//   0x118), run the active weapon's fire, then set the
//   attack animation. The old stub zeroed pev+0x118 which
//   hid the view model and never fired.
//=========================================================
void CBasePlayer::ItemPostFrame()
{
	entvars_t* pev = this->pev;

	// The binary: bail while permanently dead (deadflag == 2.0f)
	if (PevInt(pev, PEV_DEADFLAG) == 0x40000000)
		return;

	// pev->[0x11c] = 0 (fire-latch / button-hold scratch). 0x11c == 284.
	PevInt(pev, PLR_FIRE_LATCH) = 0;

	switch (PevInt(pev, PLR_VIEWMODEL))	// pev->weapon_id (pev+0x110)
	{
	case 1:
		FireCrowbar(this);
		break;
	case 2:
		FireGlock(this);
		break;
	case 4:
		FireMP5(this);
		break;
	default:
		return;	// nothing equipped that fires
	}

	// vtable SetAnimation(0x1e) == player attack activity
	SetAnimation(PLAYER_ATTACK1);
}

//=========================================================
// PostThink (vtable slot 22)
//   fall damage, landing sound, idle / run select.
//=========================================================
void CBasePlayer::PostThink()
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;

	// The binary: return if pev->view_ofs (pev+328) equals the
	// engine spawn-sentinel vector, or if pev+324 (dead/frags slot)
	// has any non-sign bit set.
	if (ViewOfsIsSentinel(pev)
		|| (PevInt(pev, PEV_DEADFLAG) & 0x7FFFFFFF) != 0)
	{
		return;
	}

	PlayerImpulseCommands();

	// fall damage: landed (FL_ONGROUND) this frame with health left.
	// The binary keys on the raw float bits (0xC3960000), i.e.
	// strictly more negative than -300.0f.
	float fallVel = SelfFloat(self, PLR_FALL_VELOCITY);
	if (fallVel < -300.0f)
	{
		if (((int)PevFloat(pev, PEV_FLAGS) & FL_ONGROUND) != 0
			&& PevInt(pev, PEV_HEALTH) > 0)
		{
			ApplyFallDamage(fallVel);
		}
	}

	// track fall velocity while airborne (not on ground)
	if (((int)PevFloat(pev, PEV_FLAGS) & FL_ONGROUND) == 0)
		SelfFloat(self, PLR_FALL_VELOCITY) = PevFloat(pev, PEV_VELOCITY + 8);

	// idle vs run animation while alive
	if (((int)PevFloat(pev, PEV_DEADFLAG) & 0x7FFFFFFF) == 0)
	{
		if (((int)PevFloat(pev, PEV_VELOCITY) & 0x7FFFFFFF) != 0
			|| ((int)PevFloat(pev, PEV_VELOCITY + 4) & 0x7FFFFFFF) != 0)
		{
			if (((int)PevFloat(pev, PEV_FLAGS) & FL_ONGROUND) != 0)
				SetAnimation(4);
		}
		else
		{
			SetAnimation(1);
		}
	}

	PlayerStudioFrame(this, 0.1f);
	// The binary: while alive, force pev->modelindex back to the
	// doctor model index (the engine may have changed it for gibs).
	if (PevInt(pev, PEV_HEALTH) > 0)
		PevFloat(pev, PEV_MODELINDEX) = (float)g_PlayerModelIndex;
}

//=========================================================
// ApplyFallDamage (the landing branch of)
//=========================================================
void CBasePlayer::ApplyFallDamage(float fallVel)
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;

	// landing in water just splashes; otherwise pick a land sound by
	// impact speed. A hard landing (fallVel < -650) deals 5.0 fall
	// damage from the worldspawn and plays a random pl_fallpain sound.
	int waterType = PevInt(pev, PEV_WATERTYPE);
	const char* sound;
	int channel;
	if (waterType == -1069547520)	// CONTENTS_WATER
	{
		sound = kInH2O;
		channel = 4;
	}
	else if (fallVel < -650.0f)
	{
		// hard landing: 5.0 fall damage from the world (vtable slot 17 ==
		// shared CBaseMonster::TakeDamage / the binary, inflictor and
		// attacker = worldspawn entvars).
		entvars_t* world = EngineGetVarsOfEnt(EnginePEntityOfEntIndex(0));
		TakeDamage(world, world, 5.0f);

		float r = RandomFloat(0.0f, 1.0f);
		if (r <= 0.33f)
			sound = kFallPain3;
		else if (r <= 0.66f)
			sound = kFallPain2;
		else
			sound = kFallPain1;
		channel = 2;
	}
	else
	{
		sound = kJumpLand;
		channel = 2;
	}

	EngineEmitSound(EdictFromEntvars(pev), channel, sound, PLR_VOL, PLR_ATTN);

	// view punch proportional to fall velocity. The pitch punch is
	// always set; the roll punch is gated on a fresh random roll < 0.5.
	PevFloat(pev, PEV_PUNCHANGLE_X) = fallVel * -0.018f;
	if (RandomFloat(0.0f, 1.0f) < 0.5f)
		PevFloat(pev, PEV_PUNCHANGLE_Z) = fallVel * -0.009f;
	SelfFloat(self, PLR_FALL_VELOCITY) = 0.0f;
	SetAnimation(4);
}

//=========================================================
// DeadThink (the dead branch of PreThink)
//=========================================================
void CBasePlayer::DeadThink(CBaseEntity* pOther)
{
	entvars_t* pev = this->pev;
	void* self = (void*)this;

	// slide the corpse along ground if it has momentum
	if (((int)PevFloat(pev, PEV_FLAGS) & FL_ONGROUND) != 0)
	{
		Vector& v = PevVector(pev, PEV_VELOCITY);
		float speed = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z) - 20.0f;
		if (speed <= 0.0f)
		{
			v.x = 0.0f;
			v.y = 0.0f;
			v.z = 0.0f;
		}
		else
		{
			float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
			if (len <= 0.0f)
			{
				v.x = 0.0f;
				v.y = 0.0f;
				v.z = 0.0f;
			}
			else
			{
				float inv = 1.0f / len;
				v.x = v.x * inv * speed;
				v.y = v.y * inv * speed;
				v.z = v.z * inv * speed;
			}
		}
	}

	// respawn handshake on death animation completion
	if (SelfInt(self, PLR_300) == 0 && PevInt(pev, PEV_DEADFLAG) == 1065353216)
	{
		PlayerStudioFrame(this, 0.1f);
		return;
	}

	if (PevInt(pev, PEV_DEADFLAG) == 1065353216)
		PevInt(pev, PEV_DEADFLAG) = 0x40000000;

	int button = PevInt(pev, PEV_BUTTON);
	if (PevInt(pev, PEV_DEADFLAG) == 0x40000000)
	{
		if (button == 0)
			PevFloat(pev, PEV_DEADFLAG) = 3.0f;	// 1077936128
	}
	else if (button != 0)
	{
		// The binary fires the full engine respawn handshake
		// (ClientRespawn in h_export.cpp), not a local
		// deadflag reset.
		PevInt(pev, PEV_BUTTON) = 0;
		ClientRespawn(pev);
		PevFloat(pev, PEV_EXTRA1) = -1.0f;	// 232 -> -1082130432 (0xBF800000)
	}
}

//=========================================================
// thin helpers delegated to engine / respawn
//=========================================================
void CBasePlayer::StudioFrameAdvance()
{
	// integrate the studio frame (wrapper)
	PlayerStudioFrame(this, 0.1f);
}

void CBasePlayer::FlashlightToggle()
{
	entvars_t* pev = this->pev;
	int f = (int)PevFloat(pev, PEV_STUCK);
	if ((f & 8) != 0)
		PevFloat(pev, PEV_STUCK) = (float)(f & 0xFFFFFFF7);
	else
		PevFloat(pev, PEV_STUCK) = (float)(f | 8);
}

//=========================================================
// SelectWeapon
//   Selects the requested weapon slot, scanning FORWARD for the
//   next owned slot when the requested one is not owned, and
//   re-encodes pev->weapon_id (pev+0x110, RAW INT) into the
//   "pending" form the commit-on-attack path expects:
//
//     weapon_id = (preservedHighWord << 16) | (0x07 << 8) | id
//
//   where the non-zero high word is the freshly-selected flag that
//   PlayerImpulseCommands tests at '(viewmodel & 0xFFFF0000)' and
//   commits on the next +ATTACK (writing weapon_id = id & 0xFF and
//   running SwitchWeaponModel). The binary does NOT call
//   SwitchWeaponModel here; the committed clean id (1/2/4) is what
//   SwitchWeaponModel later keys on. The owned mask is pev+0x114
//   (== PLR_WEAPONMODEL, 0x17 at spawn => crowbar/glock/mp5).
//=========================================================
void CBasePlayer::SelectWeapon(int id)
{
	entvars_t* pev = this->pev;

	unsigned int weaponId = (unsigned int)PevInt(pev, PLR_VIEWMODEL);
	unsigned int ownedMask = (unsigned int)PevInt(pev, PLR_WEAPONMODEL);

	// preserved high word: if a selection is already pending keep its
	// high word and re-derive the request from (currentLow + 1);
	// otherwise the high word becomes the current low word (the
	// freshly-selected flag) and the request stays 'id'.
	unsigned int highWord;
	if ((weaponId & 0xFFFF0000) == 0)
	{
		highWord = (weaponId & 0xFFFF) << 16;
	}
	else
	{
		highWord = weaponId & 0xFFFF0000;
		id = (int)(signed char)((unsigned char)(weaponId & 0xFF) + 1);
	}

	// requested slot not owned: scan forward (wrapping through 0x1f)
	// for the next owned bit (forward-scan loop).
	if ((ownedMask & (1u << ((unsigned)id & 0x1f))) == 0)
	{
		int step = 1;
		do
		{
			if (step + id > 0x1f)
				id = -step;	// wrap: restart search from the low end
		} while ((ownedMask & (1u << (((unsigned)(step + id)) & 0x1f))) == 0
			&& (++step < 0x20));
		if (step == 0x20)
			step = 0;
		id += step;
	}

	// re-encode: byte0 = selected id, byte1 = 0x07, high word preserved
	PevInt(pev, PLR_VIEWMODEL) =
		(int)(highWord | 0x0700u | ((unsigned)id & 0xFF));
}

//=========================================================
// SelectWeaponReverse
//   Same pending re-encoding as SelectWeapon but scans BACKWARD for
//   the next owned slot (used by impulse 11 / previous weapon).
//=========================================================
void CBasePlayer::SelectWeaponReverse(int id)
{
	entvars_t* pev = this->pev;

	unsigned int weaponId = (unsigned int)PevInt(pev, PLR_VIEWMODEL);
	unsigned int ownedMask = (unsigned int)PevInt(pev, PLR_WEAPONMODEL);

	unsigned int highWord;
	if ((weaponId & 0xFFFF0000) == 0)
	{
		highWord = (weaponId & 0xFFFF) << 16;
	}
	else
	{
		highWord = weaponId & 0xFFFF0000;
		id = (int)(signed char)((unsigned char)(weaponId & 0xFF) - 1);
	}

	// scan downward from id while >= 0; if none owned there, restart
	// from 0x1f and scan down (two backward loops).
	int found = 0;
	if (id >= 0)
	{
		int i = id;
		while (i >= 0)
		{
			if ((ownedMask & (1u << ((unsigned)i & 0x1f))) != 0)
			{
				id = i;
				found = 1;
				break;
			}
			--i;
		}
	}
	if (!found)
	{
		int i = 0x1f;
		while (i >= 0 && (ownedMask & (1u << ((unsigned)i & 0x1f))) == 0)
			--i;
		id = i;	// i == -1 (stored as 0xFF) when nothing is owned, as in
	}

	PevInt(pev, PLR_VIEWMODEL) =
		(int)(highWord | 0x0700u | ((unsigned)id & 0xFF));
}

void CBasePlayer::PlayerRespawn()
{
	// respawn is driven by the engine-side ClientRespawn in h_export.cpp
	entvars_t* pev = this->pev;
	PevInt(pev, PEV_DEADFLAG) = 0;
}

//=========================================================
// globalvars_t frametime accessor (offset 128)
//=========================================================
float GlobalsFrameTime(void* globals)
{
	if (!globals)
		return 0.0f;
	return *(float*)((unsigned char*)globals + 128);
}
