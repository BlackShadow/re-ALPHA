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

// zone.h -- memory allocation

/*
Hunk: the whole memory block, allocated as a stack from both ends.
Zone: small dynamic allocations (strings, command text) at the bottom of the hunk.
Cache: purgable data that can stay loaded between levels, kept between the hunk ends.
*/

#ifndef ZONE_H
#define ZONE_H

void Memory_Init(void *buf, int size);

void Z_Free(void *ptr);
void *Z_Malloc(int size);		// returns 0 filled memory
void *Z_TagMalloc(int size, int tag);
void Z_CheckHeap(void);

void *Hunk_Alloc(int size);		// returns 0 filled memory
void *Hunk_AllocName(int size, const char *name);

void *Hunk_HighAllocName(int size, const char *name);

int Hunk_LowMark(void);
int Hunk_FreeToLowMark(int mark);

int Hunk_HighMark(void);
void Hunk_FreeToHighMark(int mark);

void *Hunk_TempAlloc(int size);

int Hunk_Check(void);

extern int g_HunkSize;
extern int g_HunkLowUsed;
extern int g_HunkHighUsed;

typedef struct cache_user_s
{
	void	*data;
} cache_user_t;

void Cache_Flush(void);

void *Cache_Check(cache_user_t *c);
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL

void Cache_Free(cache_user_t *c);

void *Cache_Alloc(cache_user_t *c, int size, const char *name);
// Returns NULL if all purgable data was tossed and there still
// wasn't enough room.

void Cache_Report(void);

#endif // ZONE_H
