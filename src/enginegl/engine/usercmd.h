// usercmd.h -- client movement command

#ifndef USERCMD_H
#define USERCMD_H

#include "common.h"

typedef struct usercmd_s
{
	float	viewangles[3];
	float	forwardmove;
	float	sidemove;
	float	upmove;

	byte	lightlevel;		// light level at the player
	byte	msec;
	byte	buttons;
	byte	impulse;
} usercmd_t;

#endif // USERCMD_H
