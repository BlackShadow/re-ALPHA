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
// CBasePlayer - the player: movement, water, ladders,
// weapons and impulse commands
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "doors.h"
#include "weapons.h"
#include "studio.h"
#include "ggrenade.h"
#include "client.h"

// SetAnimation() values: the monster states the player animates
#define PLAYER_IDLE					MONSTERSTATE_IDLE
#define PLAYER_WALK					MONSTERSTATE_WALK
#define PLAYER_ATTACK1				MONSTERSTATE_RANGE_ATTACK
#define PLAYER_DIE					MONSTERSTATE_DIE1
#define PLAYER_JUMP					40

// models/doctor.mdl sequences
enum
{
	PLAYER_SEQ_RUN = 0,
	PLAYER_SEQ_ATTACK_MOVE = 2,
	PLAYER_SEQ_ATTACK = 3,
	PLAYER_SEQ_JUMP = 5,
	PLAYER_SEQ_DIE = 6,
	PLAYER_SEQ_IDLE = 7,
	PLAYER_SEQ_SPAWN = 9,
};

#define PLAYER_ANIM_INTERVAL		0.1f	// the end of a sequence is flagged this long before

#define VEC_DEAD_VIEW				Vector(0.0f, 0.0f, -8.0f)

#define PLAYER_MAX_HEALTH			100.0f
#define PLAYER_GIB_HEALTH			-40.0f	// below this the player dies without sound or animation
#define PLAYER_DEATH_TOSS			300.0f	// most upward speed added when killed

#define DM_SPAWN_TRIES				25		// deathmatch spawns skip up to this many spots

#define PLAYER_JUMP_SPEED			270.0f
#define PLAYER_SWIM_SPEED_WATER		100.0f	// upward speed with jump held in deep water
#define PLAYER_SWIM_SPEED_SLIME		80.0f
#define PLAYER_SWIM_SPEED_OTHER		50.0f
#define PLAYER_STEP_SPEED			200.0f	// faster than this plays footsteps
#define PLAYER_STEP_INTERVAL		0.3f
#define PLAYER_CORPSE_FRICTION		20.0f	// speed a corpse on the ground loses each frame

// drowning, lava and slime
#define PLAYER_AIR_TIME				12.0f	// seconds under water before drowning
#define PLAYER_GASP_AIR_TIME		9.0f	// surfacing with less air left than this gasps
#define DROWN_DAMAGE				2.0f	// the drowning damage grows by this every second
#define DROWN_DAMAGE_MAX			15.0f	// then drops back to DROWN_DAMAGE_RESET
#define DROWN_DAMAGE_RESET			10.0f
#define LAVA_DAMAGE					10.0f	// per water level
#define SLIME_DAMAGE				4.0f	// per water level
#define WATER_FRICTION				0.8f

// jumping out of water onto a ledge
#define WATERJUMP_HEIGHT			8.0f	// the wall probe starts this far above the origin
#define WATERJUMP_DIST				24.0f
#define WATERJUMP_WALL_PUSH			50.0f
#define WATERJUMP_SPEED				225.0f
#define WATERJUMP_TIME				2.0f	// FL_WATERJUMP times out after this

// ladders
#define LADDER_DIST					24.0f	// a ladder is found this far in front of the player
#define LADDER_TOP_HEIGHT			8.0f	// the top of the ladder is checked this far up
#define LADDER_CLIMB_SPEED			200		// speed at the start of a climbing stroke
#define LADDER_CLIMB_DECEL			15		// speed lost every frame of the stroke
#define LADDER_SIDE_FRICTION		0.6f	// horizontal speed kept each frame while climbing up
#define LADDER_DISMOUNT_SPEED		200.0f
#define LADDER_DISMOUNT_LIFT		275.0f
#define LADDER_STEP_FRAMES			22		// climbing frames between two climbing sounds
#define LADDER_PUNCH				7.0f

// +use
#define PLAYER_USE_RADIUS			64.0f
#define PLAYER_USE_DOT				0.7f	// in front of the player, about 45 degrees each side
#define PLAYER_USE_DELAY			0.5f

// landing
#define PLAYER_FALL_LAND_SPEED		-300.0f	// landing faster than this plays a sound
#define PLAYER_FALL_DAMAGE_SPEED	-650.0f	// landing faster than this hurts
#define PLAYER_FALL_DAMAGE			5.0f
#define PLAYER_FALL_PUNCH_PITCH		-0.018f	// view punch per unit of fall speed
#define PLAYER_FALL_PUNCH_ROLL		-0.009f

// impulse commands (1 to 8 select that weapon)
#define IMPULSE_NEXT_WEAPON			10
#define IMPULSE_PREV_WEAPON			11
#define IMPULSE_FLASHLIGHT			100
#define IMPULSE_DRAW_LINES			200
#define IMPULSE_SPRAY_LOGO			201
#define IMPULSE_SPRAY_BLOOD			202
#define IMPULSE_REMOVE_ENTITY		203

#define REMOVE_ENTITY_DIST			1024.0f	// impulse 203 range
#define REMOVE_ENTITY_HEIGHT		16.0f	// impulse 203 aims from this far above the origin
#define SPRAY_DISTANCE				128.0f	// impulses 201 and 202 need a wall this close

// weapons
#define PLAYER_START_WEAPONS		((1 << WEAPON_NONE) | (1 << WEAPON_CROWBAR) | (1 << WEAPON_GLOCK) | (1 << WEAPON_MP5))
#define WEAPON_AMMO_DISPLAY			99.0f	// pev->currentammo of every weapon with a view model

// pev->weapon: the low byte is the weapon in use. While a new weapon is being
// selected, the low byte is the new weapon, WEAPON_SELECTING is set and the third
// byte is the weapon to go back to on +cancel. +attack takes the new weapon.
#define WEAPON_ID_MASK				0x000000FF
#define WEAPON_SELECTING			0x00000700
#define WEAPON_PREVIOUS_MASK		0x00FF0000
#define WEAPON_PENDING_MASK			0xFFFF0000
#define WEAPON_PREVIOUS_SHIFT		16

// view model animations
#define CROWBAR_ATTACK1				1
#define CROWBAR_ATTACK2				2
#define GLOCK_SHOOT					0
#define MP5_FIRE					0
#define MP5_IDLE					1
#define MP5_LAUNCH					2

#define CROWBAR_RANGE				64.0f
#define CROWBAR_AIM_SPEED			1000.0f
#define CROWBAR_SPREAD				0.025f
#define CROWBAR_DELAY				1.0f
#define GLOCK_RANGE					2048.0f
#define GLOCK_SPREAD				0.025f
#define GLOCK_DELAY					0.3f
#define MP5_RANGE					2048.0f
#define MP5_SPREAD					0.01f
#define MP5_DELAY					0.1f
#define MP5_IDLE_DELAY				1.0f	// no idle animation this soon after firing
#define MP5_IDLE_TIME				10.0f	// the idle animation plays every 10 to 15 seconds
#define MP5_IDLE_TIME_RANDOM		5.0f
#define MP5_GRENADE_SPEED			800.0f
#define MP5_GRENADE_OFFSET			24.0f	// the grenade starts this far in front of the eyes
#define MP5_GRENADE_DELAY			1.0f

int g_LastSpawnEntIndex = 0;		// spawn spot searches start after this one
int g_PlayerModelIndex = 0;			// models/doctor.mdl

// the attack buttons act once per press
static int g_fAttack2Pressed = 0;
static int g_fAttackPressed = 0;

int g_fDrawLines = 0;				// toggled by impulse 200

static void SendWeaponAnim(entvars_t *pev, int iAnim);

//=========================================================
// PlayerUseEntity - pEntity->Use() with the player as the
// activator (m_hActivator and gpGlobals->other)
//=========================================================
static void PlayerUseEntity(CBasePlayer *pPlayer, CBaseEntity *pEntity)
{
	EOFFSET eoffsetPlayer = OFFSET(pPlayer->pev);

	// alpha bug: a momentary_rot_button is no CBaseToggle, this writes past its end
	((CBaseToggle *)pEntity)->m_hActivator = eoffsetPlayer;

	EOFFSET eoffsetSelf = gpGlobals->self;
	EOFFSET eoffsetOther = gpGlobals->other;

	gpGlobals->self = OFFSET(pEntity->pev);
	gpGlobals->other = eoffsetPlayer;

	pEntity->Use(pPlayer);

	gpGlobals->self = eoffsetSelf;
	gpGlobals->other = eoffsetOther;
}

CBasePlayer::CBasePlayer()
{
	m_iSquadSize = 0;	// players are never in a squad
}

//=========================================================
// PlayerResetSequenceInfo - picks up the frame rate and
// ground speed (per flInterval) of the player's sequence
//=========================================================
static void PlayerResetSequenceInfo(CBasePlayer *pPlayer, float flInterval)
{
	entvars_t *pev = pPlayer->pev;

	GetSequenceInfo(GET_MODEL_PTR(ENT(pev)), pev, &pPlayer->m_flPlayerFrameRate, &pPlayer->m_flPlayerGroundSpeed);

	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0f;

	pPlayer->m_fPlayerSequenceFinished = FALSE;
	pPlayer->m_flPlayerGroundSpeed = pPlayer->m_flPlayerGroundSpeed * flInterval;
}

//=========================================================
// PlayerAdvanceAnimation - advances the frame and flags the
// end of the sequence when it will be reached within
// flInterval
//=========================================================
static void PlayerAdvanceAnimation(CBasePlayer *pPlayer, float flInterval)
{
	entvars_t *pev = pPlayer->pev;
	float flTime = gpGlobals->time;

	if (pev->animtime != 0.0f)
		pev->frame = (flTime - pev->animtime) * pev->framerate * pPlayer->m_flPlayerFrameRate + pev->frame;

	pev->animtime = flTime;

	// keep the frame within 0..256
	if (pev->frame != 0.0f)
		pev->frame -= (int)(pev->frame / 256.0f) * 256.0f;

	if (pev->frame >= 256.0f)
		pev->frame -= (int)(pev->frame / 256.0f) * 256.0f;

	pPlayer->m_fPlayerSequenceFinished = FALSE;

	float flNextFrame = pev->framerate * pPlayer->m_flPlayerFrameRate * flInterval + pev->frame;

	if (pPlayer->m_flPlayerFrameRate > 0.0f && flNextFrame > 256.0f)
		pPlayer->m_fPlayerSequenceFinished = TRUE;
	else if (flNextFrame <= 0.0f)
		pPlayer->m_fPlayerSequenceFinished = TRUE;
}

//=========================================================
// SetAnimation - plays the sequence for a player animation
//=========================================================
int CBasePlayer::SetAnimation(int playerAnim)
{
	entvars_t *pev = this->pev;
	int iSequence;

	switch (playerAnim)
	{
	case PLAYER_IDLE:
		iSequence = PLAYER_SEQ_IDLE;
		break;
	case PLAYER_WALK:
		iSequence = PLAYER_SEQ_RUN;
		break;
	case PLAYER_ATTACK1:
		if (pev->velocity.x != 0.0f || pev->velocity.y != 0.0f)
			iSequence = PLAYER_SEQ_ATTACK_MOVE;
		else
			iSequence = PLAYER_SEQ_ATTACK;
		break;
	case PLAYER_DIE:
		iSequence = PLAYER_SEQ_DIE;
		break;
	case PLAYER_JUMP:
		iSequence = PLAYER_SEQ_JUMP;
		break;
	default:
		return playerAnim - 1;
	}

	if (pev->sequence != iSequence)
	{
		pev->sequence = iSequence;
		pev->frame = 0.0f;
		PlayerResetSequenceInfo(this, PLAYER_ANIM_INTERVAL);

		// the idle pose does not animate
		if (iSequence > PLAYER_SEQ_DIE)
		{
			m_flPlayerFrameRate = 0.0f;
			m_flPlayerGroundSpeed = 0.0f;
		}
	}
	return iSequence;
}

void CBasePlayer::SetActivity(int activity)
{
	SetAnimation(activity);
}

static void PlayerDeathSound(entvars_t *pev)
{
	const char *pszSound = NULL;

	switch (RANDOM_LONG(1, 5))
	{
	case 1:
		pszSound = "player/pl_pain5.wav";
		break;
	case 2:
		pszSound = "player/pl_pain6.wav";
		break;
	case 3:
		pszSound = "player/pl_pain7.wav";
		break;
	}

	if (pszSound)
		EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_NORM);
}

//=========================================================
// Death
//=========================================================
void CBasePlayer::Death(int iDeathType)
{
	entvars_t *pev = this->pev;

	pev->modelindex = (float)(unsigned int)g_PlayerModelIndex;
	pev->weaponmodel = 0;
	pev->view_ofs = VEC_DEAD_VIEW;

	pev->deadflag = DEAD_DYING;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->flags = (float)((int)pev->flags & ~FL_ONGROUND);

	if (pev->velocity.z < 10.0f)
		pev->velocity.z += RANDOM_FLOAT(0.0f, PLAYER_DEATH_TOSS);

	if (pev->health >= PLAYER_GIB_HEALTH)
	{
		PlayerDeathSound(pev);
		pev->angles.x = 0.0f;
		pev->angles.z = 0.0f;
		SetAnimation(PLAYER_DIE);
		SetThink(&CBasePlayer::DeadThink);
		pev->nextthink = 0.1f;
	}
}

//=========================================================
// SwitchWeaponModel - view model of the weapon in use
//=========================================================
void CBasePlayer::SwitchWeaponModel()
{
	entvars_t *pev = this->pev;

	switch (pev->weapon)
	{
	case WEAPON_CROWBAR:
		pev->weaponmodel = ALLOC_STRING("models/v_crowbar.mdl");
		pev->currentammo = WEAPON_AMMO_DISPLAY;
		break;
	case WEAPON_GLOCK:
		pev->weaponmodel = ALLOC_STRING("models/v_glock.mdl");
		pev->currentammo = WEAPON_AMMO_DISPLAY;
		break;
	case WEAPON_MP5:
		pev->weaponmodel = ALLOC_STRING("models/v_mp5.mdl");
		pev->currentammo = WEAPON_AMMO_DISPLAY;
		break;
	default:
		pev->weaponmodel = 0;
		pev->currentammo = 0.0f;
		break;
	}
}

//=========================================================
// Jump - jumps off the ground, or swims up in deep water
//=========================================================
void CBasePlayer::Jump()
{
	entvars_t *pev = this->pev;
	int iFlags = (int)pev->flags;

	if (iFlags & FL_WATERJUMP)
		return;

	if (pev->waterlevel < WATERLEVEL_WAIST)
	{
		// don't pogo stick
		if ((iFlags & FL_ONGROUND) && (iFlags & FL_JUMPRELEASED))
		{
			pev->flags = (float)(iFlags & ~FL_JUMPRELEASED);
			pev->flags = (float)((int)pev->flags & ~FL_ONGROUND);	// don't stairwalk

			SetAnimation(PLAYER_JUMP);
			pev->button &= ~IN_JUMP;

			if (RANDOM_FLOAT(0.0f, 1.0f) < 0.5f)
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_jump1.wav", VOL_NORM, ATTN_NORM);
			else
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_jump2.wav", VOL_NORM, ATTN_NORM);

			pev->velocity.z += PLAYER_JUMP_SPEED;
		}
	}
	else
	{
		int iWaterType = (int)pev->watertype;
		float flSpeed;

		if (iWaterType == CONTENTS_SLIME)
			flSpeed = PLAYER_SWIM_SPEED_SLIME;
		else if (iWaterType == CONTENTS_WATER)
			flSpeed = PLAYER_SWIM_SPEED_WATER;
		else
			flSpeed = PLAYER_SWIM_SPEED_OTHER;

		pev->velocity.z = flSpeed;

		// swimming sound
		float flTime = gpGlobals->time;
		if (m_flSwimSoundTime < flTime)
		{
			m_flSwimSoundTime = flTime + 1.0f;
			if (RANDOM_FLOAT(0.0f, 1.0f) >= 0.5f)
				EMIT_SOUND(ENT(pev), CHAN_BODY, "common/water2.wav", VOL_NORM, ATTN_NORM);
			else
				EMIT_SOUND(ENT(pev), CHAN_BODY, "common/water1.wav", VOL_NORM, ATTN_NORM);
		}
	}
}

//=========================================================
// Duck - IN_DUCK crouches; standing up again needs room
// for the standing hull
//=========================================================
void CBasePlayer::Duck()
{
	entvars_t *pev = this->pev;

	if (!(pev->button & IN_DUCK))
	{
		Vector vecSavedOrigin = pev->origin;

		SET_SIZE(ENT(pev), VEC_HULL_MIN, VEC_HULL_MAX);

		// keep the feet in place
		pev->origin.z += VEC_HULL_MAX.z - VEC_DUCK_HULL_MAX.z;
		pev->flags = (float)((int)pev->flags | FL_PARTIALGROUND);

		// a zero move tells whether the standing hull fits here
		if (WALK_MOVE(ENT(pev), 0.0f, 0.0f) != 0.0f)
		{
			pev->flags = (float)((int)pev->flags & ~FL_DUCKING);
			pev->view_ofs = VEC_VIEW;
		}
		else
		{
			SET_SIZE(ENT(pev), VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
			pev->origin = vecSavedOrigin;
		}
	}
	else
	{
		int iFlags = (int)pev->flags;
		if (!(iFlags & FL_DUCKING))
		{
			SET_SIZE(ENT(pev), VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);

			pev->view_ofs = VEC_DUCK_VIEW;
			pev->flags = (float)(iFlags | FL_DUCKING);
		}
	}
}

//=========================================================
// SelectSpawnPoint - the next info_player_coop (coop), a
// random info_player_deathmatch (deathmatch), else
// info_player_start
//=========================================================
static edict_t *SelectSpawnPoint()
{
	edict_t *pSpot = NULL;

	if (gpGlobals->coop != 0.0f)
	{
		pSpot = FIND_ENTITY_BY_STRING(ENT(g_LastSpawnEntIndex), "classname", "info_player_coop");
		if (!FNullEnt(pSpot))
			goto ReturnSpot;

		pSpot = FIND_ENTITY_BY_STRING(ENT(g_LastSpawnEntIndex), "classname", "info_player_start");
		if (!FNullEnt(pSpot))
			goto ReturnSpot;
	}
	else if (gpGlobals->deathmatch != 0.0f)
	{
		// alpha bug: every search starts at the last spot, so they all find the same one
		int iSkip = (rand() & RAND_MAX) * DM_SPAWN_TRIES / RAND_MAX;
		int iTries = DM_SPAWN_TRIES;

		while (iTries-- != 0)
		{
			pSpot = FIND_ENTITY_BY_STRING(ENT(g_LastSpawnEntIndex), "classname", "info_player_deathmatch");
			if (!FNullEnt(pSpot))
			{
				if (iSkip-- <= 0)
					goto ReturnSpot;
			}
		}
	}

	if (gpGlobals->serverflags != 0.0f)
	{
		pSpot = FIND_ENTITY_BY_STRING(NULL, "classname", "info_player_start2");
		if (!FNullEnt(pSpot))
			goto ReturnSpot;
	}

	pSpot = FIND_ENTITY_BY_STRING(NULL, "classname", "info_player_start");
	if (FNullEnt(pSpot))
		ALERT(at_error, "PutClientInServer: no info_player_start on level\n", NULL);

ReturnSpot:
	g_LastSpawnEntIndex = OFFSET(pSpot);
	return pSpot;
}

//=========================================================
// Spawn
//=========================================================
void CBasePlayer::Spawn()
{
	entvars_t *pev = this->pev;

	// set by the engine after a level change
	SPAWNPARMS *pSavedParms = (SPAWNPARMS *)gpGlobals->pSpawnParms;

	pev->classname = ALLOC_STRING("player");
	pev->health = PLAYER_MAX_HEALTH;
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_WALK;
	pev->max_health = PLAYER_MAX_HEALTH;
	pev->flags = FL_CLIENT;
	pev->air_finished = gpGlobals->time + PLAYER_AIR_TIME;
	pev->dmg = DROWN_DAMAGE;
	pev->effects = 0;
	pev->sequence = PLAYER_SEQ_SPAWN;
	pev->deadflag = DEAD_NO;

	// restore the player state from the spawn parms
	{
		SPAWNPARMS *pParms = (SPAWNPARMS *)gpGlobals->pSpawnParms;
		if (!pParms)
		{
			SetNewParms(gpGlobals);
			pParms = (SPAWNPARMS *)gpGlobals->pSpawnParms;
		}

		if (pParms)
		{
			pev->items = (int)pParms->items;
			pev->health = pParms->health;
			pev->armorvalue = pParms->armorvalue;
			pev->ammo_1 = (int)pParms->ammo[0];
			pev->ammo_2 = (int)pParms->ammo[1];
			pev->ammo_3 = (int)pParms->ammo[2];
			pev->ammo_4 = (int)pParms->ammo[3];
			pev->weapon = (int)pParms->weapon;
			pev->armortype = pParms->armortype * 0.01f;

			if (pParms->iFlags & FL_DUCKING)
			{
				pev->flags = (float)((int)pev->flags | FL_DUCKING);
				SET_SIZE(ENT(pev), VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
			}
		}
	}

	m_flFlashLightTime = 0.0f;
	m_flTimeStepSound = 0.0f;
	m_bBloodColor = BLOOD_COLOR_RED;
	m_flAttackFinished = gpGlobals->time;
	m_flPauseTime = 0.0f;

	if (!pSavedParms || !pSavedParms->fLandmark)
	{
		edict_t *pSpot = SelectSpawnPoint();
		pev->weapon = WEAPON_GLOCK;

		if (pSpot)
		{
			entvars_t *pevSpot = VARS(pSpot);
			pev->origin = pevSpot->origin;
			pev->origin.z += 1.0f;
			pev->angles = pevSpot->angles;
		}
	}
	else
	{
		// level change: keep the position relative to the landmark
		const char *pszLandmark = pSavedParms->szLandmarkName;
		edict_t *pentLandmark = FIND_ENTITY_BY_STRING(NULL, "targetname", pszLandmark);

		if (FNullEnt(pentLandmark))
		{
			ALERT(at_console, "No Landmark:%s\n", pszLandmark);

			edict_t *pSpot = SelectSpawnPoint();
			if (pSpot)
			{
				entvars_t *pevSpot = VARS(pSpot);
				pev->origin = pevSpot->origin;
				pev->origin.z += 1.0f;
				pev->angles = pevSpot->angles;
			}
		}
		else
		{
			entvars_t *pevLandmark = VARS(pentLandmark);

			pev->origin = pSavedParms->vecLandmarkOffset + pevLandmark->origin;
			pev->weapon = (int)pSavedParms->weapon;
			pev->angles = pSavedParms->angles;
			pev->v_angle = pSavedParms->v_angle;
			pev->v_angle.x = -pev->v_angle.x;
		}
	}

	pev->fixangle = TRUE;

	free(pSavedParms);
	gpGlobals->pSpawnParms = NULL;

	SAVE_SPAWN_PARMS(ENT(pev));
	SET_MODEL(ENT(pev), "models/doctor.mdl");
	g_PlayerModelIndex = (int)pev->modelindex;

	SET_SIZE(ENT(pev), VEC_HULL_MIN, VEC_HULL_MAX);

	pev->weapons = PLAYER_START_WEAPONS;
	SwitchWeaponModel();

	pev->view_ofs = VEC_VIEW;
}

int CBasePlayer::Classify()
{
	return CLASS_PLAYER;
}

//=========================================================
// CheckWaterJump - jumps out of the water when there is a
// wall in front at waist height and room above it
//=========================================================
void CBasePlayer::CheckWaterJump()
{
	entvars_t *pev = this->pev;

	Vector vecStart = pev->origin;
	vecStart.z += WATERJUMP_HEIGHT;

	UTIL_MakeVectors(pev->angles);

	// flat forward
	gpGlobals->v_forward.z = 0.0f;
	gpGlobals->v_forward = gpGlobals->v_forward.Normalize();

	TraceResult tr;
	UTIL_TraceLine(vecStart, gpGlobals->v_forward * WATERJUMP_DIST + vecStart, ignore_monsters, ENT(pev), &tr);

	// nothing solid at waist height
	if (tr.flFraction >= 1.0f)
		return;

	// check at eye level
	vecStart = pev->origin;
	vecStart.z += pev->maxs.z;

	pev->movedir = tr.vecPlaneNormal * -WATERJUMP_WALL_PUSH;

	UTIL_TraceLine(vecStart, gpGlobals->v_forward * WATERJUMP_DIST + vecStart, ignore_monsters, ENT(pev), &tr);

	if (tr.flFraction == 1.0f)
	{
		// open at eye level
		pev->flags = (float)((int)pev->flags | FL_WATERJUMP);
		pev->velocity.z = WATERJUMP_SPEED;
		pev->flags = (float)((int)pev->flags & ~FL_JUMPRELEASED);
		pev->teleport_time = gpGlobals->time + WATERJUMP_TIME;
	}
}

//=========================================================
// PlayerClimb - holds on to a func_ladder in front of the
// player and moves up and down it
//=========================================================
void CBasePlayer::PlayerClimb()
{
	entvars_t *pev = this->pev;

	UTIL_MakeVectors(pev->angles);

	Vector vecStart = pev->origin;

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecStart + gpGlobals->v_forward * LADDER_DIST, ignore_monsters, ENT(pev), &tr);

	BOOL fOnLadder = !FNullEnt(tr.pHit) && FClassnameIs(VARS(tr.pHit), "func_ladder");

	if (!fOnLadder || (pev->button & IN_JUMP))
	{
		// let go
		int iPhysicsFlags = m_afPhysicsFlags;
		if (iPhysicsFlags & PFLAG_ONLADDER)
		{
			m_afPhysicsFlags = iPhysicsFlags - PFLAG_ONLADDER;
			pev->movetype = MOVETYPE_WALK;
		}
		return;
	}

	pev->movetype = MOVETYPE_FLY;
	m_afPhysicsFlags |= PFLAG_ONLADDER;

	int iButtons = pev->button;
	if (iButtons & IN_FORWARD)
	{
		// climb up in strokes that start fast and slow down
		if (m_iLadderClimbSpeed <= 0)
			m_iLadderClimbSpeed = LADDER_CLIMB_SPEED;

		pev->velocity.x = pev->velocity.x * LADDER_SIDE_FRICTION;
		pev->velocity.y = pev->velocity.y * LADDER_SIDE_FRICTION;
		pev->velocity.z = m_iLadderClimbSpeed;
		m_iLadderClimbSpeed -= LADDER_CLIMB_DECEL;
		m_iLadderStepCount += 1;

		// is there still ladder above?
		Vector vecTop = vecStart + gpGlobals->v_forward * LADDER_DIST + gpGlobals->v_up * LADDER_TOP_HEIGHT;
		Vector vecAbove = vecStart + gpGlobals->v_up * LADDER_TOP_HEIGHT;

		UTIL_TraceLine(vecAbove, vecTop, ignore_monsters, ENT(pev), &tr);

		// reached the top, climb over the edge
		if (FNullEnt(tr.pHit) || !FClassnameIs(VARS(tr.pHit), "func_ladder"))
		{
			pev->velocity = gpGlobals->v_forward * LADDER_DISMOUNT_SPEED;
			pev->velocity.z += LADDER_DISMOUNT_LIFT;
		}
	}
	else if (iButtons & IN_BACK)
	{
		if (m_iLadderClimbSpeed >= 0)
			m_iLadderClimbSpeed = -LADDER_CLIMB_SPEED;

		pev->velocity.x = 0.0f;
		pev->velocity.y = 0.0f;
		pev->velocity.z = m_iLadderClimbSpeed;
		m_iLadderClimbSpeed += LADDER_CLIMB_DECEL;
		m_iLadderStepCount += 1;
	}
	else
	{
		pev->velocity = g_vecZero;
	}

	// climbing sound and view punch
	if (m_iLadderStepCount >= LADDER_STEP_FRAMES)
	{
		float flRand = RANDOM_FLOAT(0.0f, 1.0f);
		m_iLadderStepCount = 0;
		m_iLadderClimbSpeed = LADDER_CLIMB_SPEED;

		const char *pszSound;
		if (flRand < 0.25f)
			pszSound = "player/pl_pain2.wav";
		else if (flRand < 0.5f)
			pszSound = "player/pl_pain4.wav";
		else if (flRand < 0.75f)
			pszSound = "player/pl_pain5.wav";
		else
			pszSound = "player/pl_pain6.wav";

		EMIT_SOUND(ENT(pev), CHAN_VOICE, pszSound, VOL_NORM, ATTN_NORM);

		if (m_iLadderPunchSide)
		{
			pev->punchangle.z = LADDER_PUNCH;
			pev->punchangle.x = -LADDER_PUNCH;
			m_iLadderPunchSide = 0;
		}
		else
		{
			pev->punchangle.z = -LADDER_PUNCH;
			pev->punchangle.x = -LADDER_PUNCH;
			m_iLadderPunchSide = 1;
		}
	}
}

//=========================================================
// PreThink
//=========================================================
void CBasePlayer::PreThink()
{
	entvars_t *pev = this->pev;

	// intermission or finale
	if (pev->view_ofs == g_vecZero)
		return;

	UTIL_MakeVectors(pev->v_angle);

	WaterMove();
	if (pev->waterlevel == WATERLEVEL_WAIST)
		CheckWaterJump();

	if (pev->deadflag >= DEAD_DYING)
	{
		DeadThink(NULL);
		return;
	}

	int iButtons = pev->button;
	if (iButtons & IN_JUMP)
		Jump();
	else
		pev->flags = (float)((int)pev->flags | FL_JUMPRELEASED);

	if ((pev->button & IN_DUCK) || ((int)pev->flags & FL_DUCKING))
		Duck();

	// teleporters can hold the player still for a while
	if (gpGlobals->time < m_flPauseTime)
		pev->velocity = g_vecZero;

	// footsteps
	if (gpGlobals->time > m_flTimeStepSound && ((int)pev->flags & FL_ONGROUND))
	{
		if (pev->velocity != g_vecZero)
		{
			if (pev->velocity.Length() > PLAYER_STEP_SPEED)
			{
				float flRand = RANDOM_FLOAT(0.0f, 1.0f);
				SetAnimation(PLAYER_WALK);

				const char *pszStep;
				if (flRand <= 0.25f)
					pszStep = "player/pl_step1.wav";
				else if (flRand <= 0.5f)
					pszStep = "player/pl_step2.wav";
				else if (flRand <= 0.75f)
					pszStep = "player/pl_step3.wav";
				else
					pszStep = "player/pl_step4.wav";

				EMIT_SOUND(ENT(pev), CHAN_BODY, pszStep, VOL_NORM, ATTN_NORM);
				m_flTimeStepSound = gpGlobals->time + PLAYER_STEP_INTERVAL;
			}
		}
	}

	if (gpGlobals->time > m_flTimeWeaponIdle)
		WeaponIdle();

	PlayerClimb();
}

//=========================================================
// WeaponIdle - the MP5 idle animation
//=========================================================
void CBasePlayer::WeaponIdle()
{
	entvars_t *pev = this->pev;

	if (pev->weapon == WEAPON_MP5)
	{
		SendWeaponAnim(pev, MP5_IDLE);
		m_flTimeWeaponIdle = RANDOM_FLOAT(0.0f, MP5_IDLE_TIME_RANDOM) + gpGlobals->time + MP5_IDLE_TIME;
	}
}

//=========================================================
// WaterMove - drowning, lava and slime damage, water
// sounds and friction
//=========================================================
void CBasePlayer::WaterMove()
{
	entvars_t *pev = this->pev;
	float flTime = gpGlobals->time;

	if (pev->movetype == MOVETYPE_NOCLIP || pev->health < 0.0f)
		return;

	if (pev->waterlevel == WATERLEVEL_HEAD)
	{
		// drown!
		if (pev->air_finished < flTime && pev->pain_finished < flTime)
		{
			pev->dmg += DROWN_DAMAGE;
			if (pev->dmg > DROWN_DAMAGE_MAX)
				pev->dmg = DROWN_DAMAGE_RESET;

			entvars_t *pevWorld = VARS(ENT(0));
			TakeDamage(pevWorld, pevWorld, pev->dmg);
			pev->pain_finished = flTime + 1.0f;
		}
	}
	else
	{
		if (pev->air_finished < flTime)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/gasp2.wav", VOL_NORM, ATTN_NORM);
		else if (pev->air_finished < flTime + PLAYER_GASP_AIR_TIME)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/gasp1.wav", VOL_NORM, ATTN_NORM);

		pev->air_finished = flTime + PLAYER_AIR_TIME;
		pev->dmg = DROWN_DAMAGE;
	}

	if ((int)pev->waterlevel == WATERLEVEL_DRY)
	{
		if ((int)pev->flags & FL_INWATER)
		{
			EMIT_SOUND(ENT(pev), CHAN_BODY, "common/outwater.wav", VOL_NORM, ATTN_NORM);
			pev->flags = (float)((int)pev->flags & ~FL_INWATER);
		}
		return;
	}

	entvars_t *pevWorld = VARS(ENT(0));
	if (pev->watertype == CONTENTS_LAVA)
	{
		if (flTime > pev->dmgtime)
			TakeDamage(pevWorld, pevWorld, pev->waterlevel * LAVA_DAMAGE);
	}
	else if (pev->watertype == CONTENTS_SLIME)
	{
		pev->dmgtime = flTime + 1.0f;
		TakeDamage(pevWorld, pevWorld, pev->waterlevel * SLIME_DAMAGE);
	}

	if (!((int)pev->flags & FL_INWATER))
	{
		if (pev->watertype == CONTENTS_LAVA)
			EMIT_SOUND(ENT(pev), CHAN_BODY, "player/inlava.wav", VOL_NORM, ATTN_NORM);
		if (pev->watertype == CONTENTS_WATER)
			EMIT_SOUND(ENT(pev), CHAN_BODY, "player/inh2o.wav", VOL_NORM, ATTN_NORM);
		if (pev->watertype == CONTENTS_SLIME)
			EMIT_SOUND(ENT(pev), CHAN_BODY, "player/slimbrn2.wav", VOL_NORM, ATTN_NORM);

		pev->flags = (float)((int)pev->flags | FL_INWATER);
		pev->dmgtime = 0.0f;
	}

	if (!((int)pev->flags & FL_WATERJUMP))
	{
		float flFriction = gpGlobals->frametime * pev->waterlevel * WATER_FRICTION;
		pev->velocity = pev->velocity - pev->velocity * flFriction;
	}
}

//=========================================================
// PlayerUse - uses the buttons, doors and talking monsters
// in front of the player
//=========================================================
void CBasePlayer::PlayerUse()
{
	entvars_t *pev = this->pev;

	Vector vecOrigin = pev->origin;

	edict_t *pentEnt = FIND_ENTITY_IN_SPHERE(vecOrigin, PLAYER_USE_RADIUS);
	UTIL_MakeVectors(pev->angles);

	while (!FNullEnt(pentEnt))
	{
		entvars_t *pevEnt = VARS(pentEnt);
		Vector vecCenter = pevEnt->origin + (pevEnt->mins + pevEnt->maxs) * 0.5f;

		if (DotProduct(vecCenter - vecOrigin, gpGlobals->v_forward) > PLAYER_USE_DOT)
		{
			const char *pszClassname = STRING(pevEnt->classname);

			if (FStrEq(pszClassname, "func_button")
				|| FStrEq(pszClassname, "func_rot_button")
				|| FStrEq(pszClassname, "momentary_rot_button"))
			{
				// shootable buttons can't be used
				if (pevEnt->takedamage != DAMAGE_NO)
					return;

				CBaseEntity *pEntity = CBaseEntity::Instance(pentEnt);
				if (pEntity)
					PlayerUseEntity(this, pEntity);
			}
			else if (FStrEq(pszClassname, "func_door") || FStrEq(pszClassname, "func_door_rotating"))
			{
				if ((int)pevEnt->spawnflags & SF_DOOR_USE_ONLY)
				{
					CBaseEntity *pEntity = CBaseEntity::Instance(pentEnt);
					if (pEntity)
						PlayerUseEntity(this, pEntity);
				}
			}
			else if (FStrEq(pszClassname, "monster_scientist") || FStrEq(pszClassname, "monster_barney"))
			{
				// start or stop following the player
				CBaseEntity *pEntity = CBaseEntity::Instance(pentEnt);
				if (pEntity)
					((CBaseMonster *)pEntity)->TogglePlayerUse(pev);
				return;
			}
		}

		pentEnt = ENT(pevEnt->chain);
	}
}

//=========================================================
// ItemPostFrame - the weapon buttons, once the last attack
// is finished
//=========================================================
void CBasePlayer::ItemPostFrame()
{
	entvars_t *pev = this->pev;
	int iWeapon = pev->weapon;

	if (m_flAttackFinished > gpGlobals->time)
		return;

	ImpulseCommands();

	int iButtons = pev->button;

	// +cancel: go back to the previous weapon
	if (iButtons & IN_CANCEL)
	{
		if (iWeapon & WEAPON_PENDING_MASK)
			pev->weapon = (iWeapon & WEAPON_PREVIOUS_MASK) >> WEAPON_PREVIOUS_SHIFT;
		return;
	}

	if (iButtons & IN_ATTACK2)
	{
		// while selecting: drop the selected weapon
		if ((iWeapon & WEAPON_PENDING_MASK) && !g_fAttack2Pressed)
		{
			int iDrop = iWeapon & WEAPON_ID_MASK;
			unsigned int iPrevious = ((unsigned int)iWeapon & WEAPON_PREVIOUS_MASK) >> WEAPON_PREVIOUS_SHIFT;

			if (iDrop != WEAPON_NONE && DropItem(pev, iDrop))
			{
				pev->weapons &= ~(1 << (iDrop & (MAX_WEAPONS - 1)));
				SelectWeapon(iDrop + 1);

				if (iPrevious == (unsigned int)iWeapon)
				{
					pev->weapon = WEAPON_NONE;
					SwitchWeaponModel();
				}
			}

			pev->button &= ~IN_ATTACK2;
			g_fAttack2Pressed = TRUE;
			return;
		}

		// MP5 grenade launcher
		if (pev->weapon == WEAPON_MP5 && gpGlobals->time > m_flNextGrenadeTime)
		{
			g_fAttack2Pressed = TRUE;

			UTIL_MakeVectors(pev->v_angle);
			SendWeaponAnim(pev, MP5_LAUNCH);

			EMIT_SOUND(ENT(pev), CHAN_WEAPON, (RANDOM_FLOAT(0.0f, 1.0f) < 0.5f) ? "weapons/glauncher.wav" : "weapons/glauncher2.wav", 0.75f, ATTN_NORM);

			Vector vecVelocity = gpGlobals->v_forward * MP5_GRENADE_SPEED;
			Vector vecStart = pev->origin + pev->view_ofs + gpGlobals->v_forward * MP5_GRENADE_OFFSET;

			ShootTimedGrenade(pev, vecStart, vecVelocity);

			pev->button &= ~IN_ATTACK2;
			m_flNextGrenadeTime = gpGlobals->time + MP5_GRENADE_DELAY;
		}
	}
	else
	{
		g_fAttack2Pressed = FALSE;
	}

	if (iButtons & IN_ATTACK)
	{
		if (!g_fAttackPressed)
		{
			if (iWeapon & WEAPON_PENDING_MASK)
			{
				// take the selected weapon instead of firing
				pev->weapon = iWeapon & WEAPON_ID_MASK;
				pev->button &= ~IN_ATTACK;
				SwitchWeaponModel();
				g_fAttackPressed = TRUE;
			}
			else
			{
				PrimaryAttack();
			}
		}
	}
	else
	{
		g_fAttackPressed = FALSE;
	}
}

//=========================================================
// CanSprayDecal - impulses 201 and 202 only spray on a wall
// close in front of the player
//=========================================================
static BOOL CanSprayDecal(entvars_t *pev)
{
	UTIL_MakeVectors(pev->v_angle);

	Vector vecStart = pev->origin + pev->view_ofs;

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecStart + gpGlobals->v_forward * SPRAY_DISTANCE, ignore_monsters, ENT(pev), &tr);

	return tr.flFraction != 1.0f;
}

//=========================================================
// RemoveAimedEntity - impulse 203 removes the damageable
// entity the player looks at
//=========================================================
static void RemoveAimedEntity(entvars_t *pev)
{
	UTIL_MakeVectors(pev->v_angle);

	Vector vecStart = pev->origin;
	vecStart.z += REMOVE_ENTITY_HEIGHT;

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecStart + gpGlobals->v_forward * REMOVE_ENTITY_DIST, dont_ignore_monsters, ENT(pev), &tr);

	if (FNullEnt(tr.pHit))
		return;

	edict_t *pentHit = ENT(tr.pHit);
	if (VARS(pentHit)->takedamage != DAMAGE_NO)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(pentHit);
		if (pEntity)
			pEntity->SetThink(&CBaseEntity::SUB_Remove);
	}
}

//=========================================================
// ImpulseCommands - +use and the impulse commands
//=========================================================
void CBasePlayer::ImpulseCommands()
{
	entvars_t *pev = this->pev;

	if ((pev->button & IN_USE) && gpGlobals->time > m_flNextUseTime)
	{
		PlayerUse();
		m_flNextUseTime = gpGlobals->time + PLAYER_USE_DELAY;
	}

	int iImpulse = (int)pev->impulse;
	switch (iImpulse)
	{
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		SelectWeapon(iImpulse);
		break;
	case IMPULSE_NEXT_WEAPON:
		SelectWeapon((pev->weapon & WEAPON_ID_MASK) + 1);
		break;
	case IMPULSE_PREV_WEAPON:
	{
		int iWeapon = (pev->weapon & WEAPON_ID_MASK) - 1;
		if (iWeapon < 0)
			iWeapon = MAX_WEAPONS - 1;
		SelectWeaponReverse(iWeapon);
		break;
	}
	case IMPULSE_FLASHLIGHT:
	{
		int iEffects = (int)pev->effects;
		if (iEffects & EF_DIMLIGHT)
			pev->effects = (float)(unsigned int)(iEffects & ~EF_DIMLIGHT);
		else
			pev->effects = (float)(iEffects | EF_DIMLIGHT);
		break;
	}
	case IMPULSE_DRAW_LINES:
		if (g_fDrawLines)
		{
			g_fDrawLines = FALSE;
			ALERT(at_console, "Lines Off\n");
		}
		else
		{
			g_fDrawLines = TRUE;
			ALERT(at_console, "Lines On\n");
		}
		break;
	case IMPULSE_SPRAY_LOGO:
		if (CanSprayDecal(pev))
			SprayLambdas(pev);
		break;
	case IMPULSE_SPRAY_BLOOD:
		if (CanSprayDecal(pev))
			SprayBlood(pev);
		break;
	case IMPULSE_REMOVE_ENTITY:
		RemoveAimedEntity(pev);
		break;
	default:
		break;
	}

	pev->impulse = 0.0f;
}

//=========================================================
// SendWeaponAnim - plays a view model animation
//=========================================================
static void SendWeaponAnim(entvars_t *pev, int iAnim)
{
	gpGlobals->msg_entity = OFFSET(pev);
	WRITE_BYTE(MSG_ONE, SVC_WEAPONANIM);
	WRITE_BYTE(MSG_ONE, iAnim);
	gpGlobals->msg_entity = 0;
}

static void FireGlock(CBasePlayer *pPlayer)
{
	entvars_t *pev = pPlayer->pev;

	pev->effects = (float)((int)pev->effects | EF_MUZZLEFLASH);

	SendWeaponAnim(pev, GLOCK_SHOOT);

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, (rand() & 1) ? "weapons/pl_gun1.wav" : "weapons/pl_gun2.wav", VOL_NORM, ATTN_NORM);

	UTIL_MakeVectors(pev->v_angle);

	Vector vecAiming;
	GET_AIM_VECTOR(ENT(pev), GLOCK_RANGE, vecAiming);

	pPlayer->FireBullets(1, vecAiming, GLOCK_SPREAD, GLOCK_SPREAD, BULLET_NONE, GLOCK_RANGE);

	pPlayer->m_flAttackFinished = gpGlobals->time + GLOCK_DELAY;
}

static void FireCrowbar(CBasePlayer *pPlayer)
{
	entvars_t *pev = pPlayer->pev;

	if ((rand() & 1) == 1)
		SendWeaponAnim(pev, CROWBAR_ATTACK1);
	else
		SendWeaponAnim(pev, CROWBAR_ATTACK2);

	UTIL_MakeVectors(pev->v_angle);

	// unused random numbers
	RANDOM_FLOAT(100.0f, 150.0f);
	RANDOM_FLOAT(50.0f, 70.0f);
	RANDOM_FLOAT(-25.0f, 25.0f);

	Vector vecAiming;
	GET_AIM_VECTOR(ENT(pev), CROWBAR_AIM_SPEED, vecAiming);

	pPlayer->FireBullets(1, vecAiming, CROWBAR_SPREAD, CROWBAR_SPREAD, BULLET_NONE, CROWBAR_RANGE);

	pPlayer->m_flAttackFinished = gpGlobals->time + CROWBAR_DELAY;
}

static void FireMP5(CBasePlayer *pPlayer)
{
	entvars_t *pev = pPlayer->pev;

	pev->effects = (float)((int)pev->effects | EF_MUZZLEFLASH);

	SendWeaponAnim(pev, MP5_FIRE);

	const char *pszSound;
	float flRand = RANDOM_FLOAT(0.0f, 1.0f);
	if (flRand < 0.33f)
		pszSound = "weapons/hks1.wav";
	else if (flRand < 0.66f)
		pszSound = "weapons/hks2.wav";
	else
		pszSound = "weapons/hks3.wav";
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, pszSound, VOL_NORM, ATTN_NORM);

	UTIL_MakeVectors(pev->v_angle);

	Vector vecAiming;
	GET_AIM_VECTOR(ENT(pev), MP5_RANGE, vecAiming);

	pPlayer->FireBullets(1, vecAiming, MP5_SPREAD, MP5_SPREAD, BULLET_NONE, MP5_RANGE);

	pPlayer->m_flAttackFinished = gpGlobals->time + MP5_DELAY;
	pPlayer->m_flTimeWeaponIdle = gpGlobals->time + MP5_IDLE_DELAY;
}

//=========================================================
// PrimaryAttack - fires the weapon in use
//=========================================================
void CBasePlayer::PrimaryAttack()
{
	entvars_t *pev = this->pev;

	if (pev->deadflag == DEAD_DEAD)
		return;

	pev->weaponframe = 0.0f;

	switch (pev->weapon)
	{
	case WEAPON_CROWBAR:
		FireCrowbar(this);
		break;
	case WEAPON_GLOCK:
		FireGlock(this);
		break;
	case WEAPON_MP5:
		FireMP5(this);
		break;
	default:
		return;
	}

	SetAnimation(PLAYER_ATTACK1);
}

//=========================================================
// PostThink - weapons, landing, animation
//=========================================================
void CBasePlayer::PostThink()
{
	entvars_t *pev = this->pev;

	// intermission or finale, or dead
	if (pev->view_ofs == g_vecZero || pev->deadflag != DEAD_NO)
		return;

	ItemPostFrame();

	// landed
	float flFallVelocity = m_flFallVelocity;
	if (flFallVelocity < PLAYER_FALL_LAND_SPEED)
	{
		if (((int)pev->flags & FL_ONGROUND) && pev->health > 0.0f)
			ApplyFallDamage(flFallVelocity);
	}

	if (!((int)pev->flags & FL_ONGROUND))
		m_flFallVelocity = pev->velocity.z;

	if ((int)pev->deadflag == DEAD_NO)
	{
		if ((int)pev->velocity.x != 0 || (int)pev->velocity.y != 0)
		{
			if ((int)pev->flags & FL_ONGROUND)
				SetAnimation(PLAYER_WALK);
		}
		else
		{
			SetAnimation(PLAYER_IDLE);
		}
	}

	PlayerAdvanceAnimation(this, PLAYER_ANIM_INTERVAL);

	if (pev->health > 0.0f)
		pev->modelindex = (float)g_PlayerModelIndex;
}

//=========================================================
// ApplyFallDamage - landing sound, damage and view punch
//=========================================================
void CBasePlayer::ApplyFallDamage(float flFallVelocity)
{
	entvars_t *pev = this->pev;

	const char *pszSound;
	int iChannel;

	if (pev->watertype == CONTENTS_WATER)
	{
		pszSound = "player/inh2o.wav";
		iChannel = CHAN_BODY;
	}
	else if (flFallVelocity < PLAYER_FALL_DAMAGE_SPEED)
	{
		entvars_t *pevWorld = VARS(ENT(0));
		TakeDamage(pevWorld, pevWorld, PLAYER_FALL_DAMAGE);

		float flRand = RANDOM_FLOAT(0.0f, 1.0f);
		if (flRand <= 0.33f)
			pszSound = "player/pl_fallpain3.wav";
		else if (flRand <= 0.66f)
			pszSound = "player/pl_fallpain2.wav";
		else
			pszSound = "player/pl_fallpain1.wav";
		iChannel = CHAN_VOICE;
	}
	else
	{
		pszSound = "player/pl_jumpland2.wav";
		iChannel = CHAN_VOICE;
	}

	EMIT_SOUND(ENT(pev), iChannel, pszSound, VOL_NORM, ATTN_NORM);

	pev->punchangle.x = flFallVelocity * PLAYER_FALL_PUNCH_PITCH;
	if (RANDOM_FLOAT(0.0f, 1.0f) < 0.5f)
		pev->punchangle.z = flFallVelocity * PLAYER_FALL_PUNCH_ROLL;

	m_flFallVelocity = 0.0f;
	SetAnimation(PLAYER_WALK);
}

//=========================================================
// DeadThink - plays the death animation, then respawns on
// a button press
//=========================================================
void CBasePlayer::DeadThink(CBaseEntity *pOther)
{
	entvars_t *pev = this->pev;

	// the corpse slides to a halt
	if ((int)pev->flags & FL_ONGROUND)
	{
		float flForward = pev->velocity.Length() - PLAYER_CORPSE_FRICTION;
		if (flForward <= 0.0f)
			pev->velocity = g_vecZero;
		else
			pev->velocity = pev->velocity.Normalize() * flForward;
	}

	// the death animation is still playing
	if (!m_fPlayerSequenceFinished && pev->deadflag == DEAD_DYING)
	{
		PlayerAdvanceAnimation(this, PLAYER_ANIM_INTERVAL);
		return;
	}

	if (pev->deadflag == DEAD_DYING)
		pev->deadflag = DEAD_DEAD;

	int iButtons = pev->button;
	if (pev->deadflag == DEAD_DEAD)
	{
		// wait for all buttons released
		if (!iButtons)
			pev->deadflag = DEAD_RESPAWNABLE;
	}
	else if (iButtons)
	{
		// any button respawns
		pev->button = 0;
		respawn(pev);
		pev->nextthink = -1.0f;
	}
}

//=========================================================
// SelectWeapon - selects iWeapon, or the next weapon owned
// after it
//=========================================================
void CBasePlayer::SelectWeapon(int iWeapon)
{
	entvars_t *pev = this->pev;

	unsigned int iCurrent = (unsigned int)pev->weapon;
	unsigned int iOwned = (unsigned int)pev->weapons;
	unsigned int iPending;

	if (!(iCurrent & WEAPON_PENDING_MASK))
	{
		iPending = iCurrent << WEAPON_PREVIOUS_SHIFT;
	}
	else
	{
		// already selecting: move on to the next weapon
		iPending = iCurrent & WEAPON_PENDING_MASK;
		iWeapon = (signed char)((iCurrent & WEAPON_ID_MASK) + 1);
	}

	if (!(iOwned & (1u << (iWeapon & (MAX_WEAPONS - 1)))))
	{
		int i = 1;
		do
		{
			// wrap around to weapon 0
			if (iWeapon + i > MAX_WEAPONS - 1)
				iWeapon = -i;
		} while (!(iOwned & (1u << ((iWeapon + i) & (MAX_WEAPONS - 1)))) && ++i < MAX_WEAPONS);

		if (i == MAX_WEAPONS)
			i = 0;

		iWeapon += i;
	}

	pev->weapon = (int)(iPending | WEAPON_SELECTING | ((unsigned int)iWeapon & WEAPON_ID_MASK));
}

//=========================================================
// SelectWeaponReverse - selects iWeapon, or the previous
// weapon owned before it
//=========================================================
void CBasePlayer::SelectWeaponReverse(int iWeapon)
{
	entvars_t *pev = this->pev;

	unsigned int iCurrent = (unsigned int)pev->weapon;
	unsigned int iOwned = (unsigned int)pev->weapons;
	unsigned int iPending;

	if (!(iCurrent & WEAPON_PENDING_MASK))
	{
		iPending = iCurrent << WEAPON_PREVIOUS_SHIFT;
	}
	else
	{
		// already selecting: move on to the previous weapon
		iPending = iCurrent & WEAPON_PENDING_MASK;
		iWeapon = (signed char)((iCurrent & WEAPON_ID_MASK) - 1);
	}

	BOOL fFound = FALSE;
	if (iWeapon >= 0)
	{
		for (int i = iWeapon; i >= 0; i--)
		{
			if (iOwned & (1u << (i & (MAX_WEAPONS - 1))))
			{
				iWeapon = i;
				fFound = TRUE;
				break;
			}
		}
	}

	// wrap around to the last weapon
	if (!fFound)
	{
		int i = MAX_WEAPONS - 1;
		while (i >= 0 && !(iOwned & (1u << (i & (MAX_WEAPONS - 1)))))
			i--;
		iWeapon = i;
	}

	pev->weapon = (int)(iPending | WEAPON_SELECTING | ((unsigned int)iWeapon & WEAPON_ID_MASK));
}
