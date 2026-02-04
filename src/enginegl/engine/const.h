#ifndef CONST_H
#define CONST_H

 // Constants shared between engine + game DLL.

 // edict->movetype values
#define MOVETYPE_NONE           0
#define MOVETYPE_WALK           3
#define MOVETYPE_STEP           4
#define MOVETYPE_FLY            5
#define MOVETYPE_TOSS           6
#define MOVETYPE_PUSH           7
#define MOVETYPE_NOCLIP         8
#define MOVETYPE_FLYMISSILE     9
#define MOVETYPE_BOUNCE         10
#define MOVETYPE_BOUNCEMISSILE  11
#define MOVETYPE_FOLLOW         12
#define MOVETYPE_PUSHSTEP       13

 // edict->solid values
#define SOLID_NOT        0
#define SOLID_TRIGGER    1
#define SOLID_BBOX       2
#define SOLID_SLIDEBOX   3
#define SOLID_BSP        4

 // Entity effect flags
#define EF_BRIGHTFIELD   1 // swirling cloud of particles
#define EF_MUZZLEFLASH   2 // single frame ELIGHT on entity attachment 0
#define EF_BRIGHTLIGHT   4 // DLIGHT centered at entity origin
#define EF_DIMLIGHT      8 // dim/ flashlight
#define EF_INVLIGHT      16 // get lighting from ceiling
#define EF_NOINTERP      32 // don't interpolate the next frame
#define EF_LIGHT         64 // rocket flare glow sprite
#define EF_NODRAW        128 // don't draw entity

#endif // CONST_H
