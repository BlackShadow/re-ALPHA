#pragma once

#include "progdefs.h"

typedef int qboolean;

#define MAX_ENT_LEAFS 48

struct link_t
{
	link_t *prev;
	link_t *next;
};

struct edict_t
{
	qboolean free;
	int serialnumber;
	link_t area;
	int headnode;
	int num_leafs;
	short leafnums[MAX_ENT_LEAFS];
	float freetime;
	void *pvPrivateData;
	entvars_t v;
};
