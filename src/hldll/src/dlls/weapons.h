#pragma once

#include "hl_types.h"

struct entvars_t;

//=========================================================
// weapons.h - Spawn API for the player-instantiated weapon
// and projectile entity classes (Half-Life 0.52 alpha).
//
// None of these are map entities (no DLLEXPORT factory): the
// player code creates the entity, installs the C++ vtable via
// placement-new, then calls one of the Deploy* helpers below
// to run the original constructor (sub_*).
//=========================================================

// The binary (336 bytes) - thrown contact grenade
// (IN_ATTACK2). Bounces, explodes on a 2s fuse (or sooner via Touch),
// damages on impact. start[3]/velocity[3] are world-space.
void ThrowGrenade(entvars_t* pevOwner, const float* start, const float* velocity);

// The binary (32 bytes) - repeating "sprayer" decal
// projectile (impulse 201). Paints up to six decals along the owner's
// aim then removes itself.
void DeploySprayerRepeat(entvars_t* pevOwner);

// The binary (28 bytes) - single-shot "sprayer" decal
// projectile (impulse 202). Paints one random decal then removes itself.
void DeploySprayerSingle(entvars_t* pevOwner);

// The binary (32 bytes) - deployed satchel / "Item"
// pickup (impulse). itemId is the weapon-slot bit the satchel grants
// back to whoever touches it. Returns the spawned entity, or NULL.
void* DeploySatchel(entvars_t* pevOwner, int itemId);
