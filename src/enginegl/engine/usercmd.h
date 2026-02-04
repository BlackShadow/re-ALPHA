#ifndef USERCMD_H
#define USERCMD_H

#include "common.h"

 // Client movement command (sent from client to server).

 // Note: lightlevel is derived from a computed light level and sent along with the move message.
typedef struct usercmd_s
{
	float	viewangles[3];
	float	forwardmove;
	float	sidemove;
	float	upmove;

	byte	lightlevel; // computed light level
	byte	msec;
	byte	buttons;
	byte	impulse;
} usercmd_t;

#endif // USERCMD_H
