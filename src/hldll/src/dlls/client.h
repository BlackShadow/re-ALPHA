#pragma once

#include <stdint.h>

struct client_t
{
	uint8_t pad0[112];
	int entIndex;
	uint8_t pad1[60];
	void *parms;
};
