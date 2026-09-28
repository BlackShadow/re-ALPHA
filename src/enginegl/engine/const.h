// const.h -- constants shared by the engine and the game DLL

#ifndef CONST_H
#define CONST_H

// edict->movetype values
#define MOVETYPE_NONE			0
#define MOVETYPE_WALK			3
#define MOVETYPE_STEP			4
#define MOVETYPE_FLY			5
#define MOVETYPE_TOSS			6
#define MOVETYPE_PUSH			7
#define MOVETYPE_NOCLIP			8
#define MOVETYPE_FLYMISSILE		9
#define MOVETYPE_BOUNCE			10
#define MOVETYPE_BOUNCEMISSILE	11
#define MOVETYPE_FOLLOW			12
#define MOVETYPE_PUSHSTEP		13

// edict->solid values
#define SOLID_NOT				0
#define SOLID_TRIGGER			1
#define SOLID_BBOX				2
#define SOLID_SLIDEBOX			3
#define SOLID_BSP				4

// Entity effect flags
#define EF_BRIGHTFIELD			1	// swirling cloud of particles
#define EF_MUZZLEFLASH			2	// single frame ELIGHT on entity attachment 0
#define EF_BRIGHTLIGHT			4	// DLIGHT centered at entity origin
#define EF_DIMLIGHT				8	// dim light (flashlight)
#define EF_INVLIGHT				16	// get lighting from ceiling
#define EF_NOINTERP				32	// don't interpolate the next frame
#define EF_LIGHT				64	// rocket flare glow sprite
#define EF_NODRAW				128	// don't draw entity

// entity rendermode values
#define kRenderNormal			0
#define kRenderTransColor		1	// solid color, blended by renderamt
#define kRenderTransTexture		2	// texture, blended by renderamt
#define kRenderTransAdd			5	// texture, added to the framebuffer

// entity renderfx values
#define kRenderFxNone			0
#define kRenderFxPulseSlow		1
#define kRenderFxPulseFast		2
#define kRenderFxPulseSlowWide	3
#define kRenderFxPulseFastWide	4
#define kRenderFxFadeSlow		5
#define kRenderFxFadeFast		6
#define kRenderFxSolidSlow		7
#define kRenderFxSolidFast		8
#define kRenderFxStrobeSlow		9
#define kRenderFxStrobeFast		10
#define kRenderFxStrobeFaster	11
#define kRenderFxFlickerSlow	12
#define kRenderFxFlickerFast	13

// entvars_t::waterlevel
#define WATERLEVEL_DRY			0
#define WATERLEVEL_FEET			1
#define WATERLEVEL_WAIST		2
#define WATERLEVEL_HEAD			3

// usercmd_t::buttons
#define IN_ATTACK				(1<<0)
#define IN_JUMP					(1<<1)
#define IN_DUCK					(1<<2)
#define IN_FORWARD				(1<<3)
#define IN_BACK					(1<<4)
#define IN_USE					(1<<5)
#define IN_CANCEL				(1<<6)
#define IN_LEFT					(1<<7)
#define IN_RIGHT				(1<<8)
#define IN_MOVELEFT				(1<<9)
#define IN_MOVERIGHT			(1<<10)
#define IN_ATTACK2				(1<<11)

#endif // CONST_H
