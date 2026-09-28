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

// zone.c -- memory allocation

#include "quakedef.h"

#define DYNAMIC_SIZE	0xc000

#define ZONEID			0x1d4a11
#define MINFRAGMENT		64

typedef struct memblock_s
{
	int		size;		// including the header and possibly tiny fragments
	int		tag;		// a tag of 0 is a free block
	int		id;			// should be ZONEID
	struct memblock_s	*next, *prev;
	int		pad;		// pad to 64 bit boundary
} memblock_t;

typedef struct memzone_s
{
	int			size;		// total bytes malloced, including header
	memblock_t	blocklist;	// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;

static memzone_t *g_MainZone;

/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

/*
========================
Z_CheckHeap
========================
*/
void Z_CheckHeap(void)
{
	memblock_t *block;

	for (block = g_MainZone->blocklist.next; block->next != &g_MainZone->blocklist; block = block->next)
	{
		if ((byte *)block + block->size != (byte *)block->next)
			Sys_Error("Z_CheckHeap: block size does not touch the next block\n");
		if (block->next->prev != block)
			Sys_Error("Z_CheckHeap: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Sys_Error("Z_CheckHeap: two consecutive free blocks\n");
	}
}

/*
========================
Z_Print
========================
*/
static void Z_Print(memzone_t *zone)
{
	memblock_t *block;

	Con_Printf("zone size: %i  location: %p\n", zone->size, (const void *)zone);

	for (block = zone->blocklist.next; ; block = block->next)
	{
		Con_Printf("block:%p    size:%7i    tag:%3i\n", block, block->size, block->tag);

		if (block->next == &zone->blocklist)
			break;	// all blocks have been hit
		if ((byte *)block + block->size != (byte *)block->next)
			Con_Printf("ERROR: block size does not touch the next block\n");
		if (block->next->prev != block)
			Con_Printf("ERROR: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Con_Printf("ERROR: two consecutive free blocks\n");
	}
}

/*
========================
Z_TagMalloc
========================
*/
void *Z_TagMalloc(int size, int tag)
{
	int extra;
	memblock_t *start, *rover, *new, *base;

	if (!tag)
		Sys_Error("Z_TagMalloc: tried to use a 0 tag");

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof(memblock_t);	// account for size of block header
	size += 4;					// space for memory trash tester
	size = (size + 7) & ~7;		// align to 8-byte boundary

	base = rover = g_MainZone->rover;
	start = base->prev;

	do
	{
		if (rover == start)	// scanned all the way around the list
			return NULL;
		if (rover->tag)
			base = rover = rover->next;
		else
			rover = rover->next;
	} while (base->tag || base->size < size);

	//
	// found a block big enough
	//
	extra = base->size - size;
	if (extra > MINFRAGMENT)
	{
		// there will be a free fragment after the allocated block
		new = (memblock_t *)((byte *)base + size);
		new->prev = base;
		new->tag = 0;			// free block
		new->id = ZONEID;
		new->size = extra;
		new->next = base->next;
		base->size = size;
		new->next->prev = new;
		base->next = new;
	}

	base->tag = tag;				// no longer a free block

	g_MainZone->rover = base->next;	// next allocation will start looking here

	base->id = ZONEID;

	// marker for memory trash testing
	*(int *)((byte *)base + base->size - 4) = ZONEID;

	return (void *)((byte *)base + sizeof(memblock_t));
}

/*
========================
Z_Malloc
========================
*/
void *Z_Malloc(int size)
{
	void *buf;

	Z_CheckHeap();	// DEBUG
	buf = Z_TagMalloc(size, 1);
	if (!buf)
		Sys_Error("Z_Malloc: failed on allocation of %i bytes", size);
	Q_memset(buf, 0, size);

	return buf;
}

/*
========================
Z_ClearZone
========================
*/
static void Z_ClearZone(memzone_t *zone, int size)
{
	memblock_t *block;

	// set the entire zone to one free block
	zone->size = size;

	block = (memblock_t *)((byte *)zone + sizeof(memzone_t));

	zone->blocklist.size = 0;
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.next = block;
	zone->blocklist.prev = block;

	zone->rover = block;

	block->size = size - (int)sizeof(memzone_t);
	block->tag = 0;				// free block
	block->id = ZONEID;
	block->next = &zone->blocklist;
	block->prev = &zone->blocklist;
	block->pad = 0;
}

/*
========================
Z_Free
========================
*/
void Z_Free(void *ptr)
{
	memblock_t *block, *other;

	if (!ptr)
		Sys_Error("Z_Free: NULL pointer");

	block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		Sys_Error("Z_Free: freed a pointer without ZONEID");
	if (!block->tag)
		Sys_Error("Z_Free: freed a freed pointer");

	block->tag = 0;		// mark as free

	other = block->prev;
	if (!other->tag)
	{
		// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (g_MainZone->rover == block)
			g_MainZone->rover = other;
		block = other;
	}

	other = block->next;
	if (!other->tag)
	{
		// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (g_MainZone->rover == other)
			g_MainZone->rover = block;
	}
}

//============================================================================

#define HUNK_SENTINEL	0x1df001ed

typedef struct
{
	int		sentinel;
	int		size;		// including sizeof(hunk_t)
	char	name[8];
} hunk_t;

byte *g_HunkBase;
int g_HunkSize;

int g_HunkLowUsed;
int g_HunkHighUsed;

qboolean g_HunkTempActive;
int g_HunkTempMark;

static void Cache_FreeLow(int new_low_hunk);
static void Cache_FreeHigh(int new_high_hunk);

/*
==============
Hunk_Check

Run consistency and sentinel trashing checks
==============
*/
int Hunk_Check(void)
{
	hunk_t *h;

	for (h = (hunk_t *)g_HunkBase; (byte *)h != g_HunkBase + g_HunkLowUsed; )
	{
		if (h->sentinel != HUNK_SENTINEL)
			Sys_Error("Hunk_Check: trashed sentinel");
		if (h->size < (int)sizeof(hunk_t) || (byte *)h + h->size - g_HunkBase > g_HunkSize)
			Sys_Error("Hunk_Check: bad size");
		h = (hunk_t *)((byte *)h + h->size);
	}

	return g_HunkLowUsed;
}

/*
===================
Hunk_LowMark
===================
*/
int Hunk_LowMark(void)
{
	return g_HunkLowUsed;
}

/*
===================
Hunk_FreeToLowMark
===================
*/
int Hunk_FreeToLowMark(int mark)
{
	if (mark < 0 || mark > g_HunkLowUsed)
		Sys_Error("Hunk_FreeToLowMark: bad mark %i", mark);

	memset(g_HunkBase + mark, 0, g_HunkLowUsed - mark);
	g_HunkLowUsed = mark;

	return 0;
}

/*
===================
Hunk_HighMark
===================
*/
int Hunk_HighMark(void)
{
	if (g_HunkTempActive)
	{
		g_HunkTempActive = false;
		Hunk_FreeToHighMark(g_HunkTempMark);
	}

	return g_HunkHighUsed;
}

/*
===================
Hunk_FreeToHighMark
===================
*/
void Hunk_FreeToHighMark(int mark)
{
	if (g_HunkTempActive)
	{
		g_HunkTempActive = false;
		Hunk_FreeToHighMark(g_HunkTempMark);
	}

	if (mark < 0 || mark > g_HunkHighUsed)
		Sys_Error("Hunk_FreeToHighMark: bad mark %i", mark);

	memset(g_HunkBase + g_HunkSize - g_HunkHighUsed, 0, g_HunkHighUsed - mark);
	g_HunkHighUsed = mark;
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName(int size, const char *name)
{
	hunk_t *h;

	if (size < 0)
		Sys_Error("Hunk_Alloc: bad size: %i", size);

	size = ((size + 15) & ~15) + sizeof(hunk_t);

	if (g_HunkSize - g_HunkLowUsed - g_HunkHighUsed < size)
		Sys_Error("Hunk_Alloc: failed on %i bytes", size);

	g_HunkLowUsed += size;

	Cache_FreeLow(g_HunkLowUsed);

	h = (hunk_t *)(g_HunkBase + (g_HunkLowUsed - size));
	memset(h, 0, size);

	h->size = size;
	h->sentinel = HUNK_SENTINEL;
	Q_strncpy(h->name, name, sizeof(h->name));

	return (void *)(h + 1);
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc(int size)
{
	return Hunk_AllocName(size, "unknown");
}

/*
===================
Hunk_HighAllocName
===================
*/
void *Hunk_HighAllocName(int size, const char *name)
{
	hunk_t *h;

	if (size < 0)
		Sys_Error("Hunk_HighAllocName: bad size: %i", size);

	if (g_HunkTempActive)
	{
		Hunk_FreeToHighMark(g_HunkTempMark);
		g_HunkTempActive = false;
	}

	size = ((size + 15) & ~15) + sizeof(hunk_t);

	if (g_HunkSize - g_HunkLowUsed - g_HunkHighUsed >= size)
	{
		g_HunkHighUsed += size;

		Cache_FreeHigh(g_HunkHighUsed);

		h = (hunk_t *)(g_HunkBase + g_HunkSize - g_HunkHighUsed);
		memset(h, 0, size);

		h->size = size;
		h->sentinel = HUNK_SENTINEL;
		Q_strncpy(h->name, name, sizeof(h->name));

		return (void *)(h + 1);
	}

	Con_Printf("Hunk_HighAlloc: failed on %i bytes\n", size);
	return NULL;
}

/*
===============================================================================

RENDERER SETUP

R_Init and the renderer console commands

===============================================================================
*/

/*
==================
R_InitParticleTexture
==================
*/
static void R_InitParticleTexture(void)
{
	static const byte dottexture[8][8] =
	{
		{0,1,1,0,0,0,0,0},
		{1,1,1,1,0,0,0,0},
		{1,1,1,1,0,0,0,0},
		{0,1,1,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
	};
	int x, y;
	byte data[8][8][4];

	//
	// particle texture
	//
	particletexture = texture_extension_number++;
	GL_Bind(particletexture);

	for (y = 0; y < 8; y++)
	{
		for (x = 0; x < 8; x++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] ? 255 : 0;
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
static void R_Envmap_f(void)
{
	byte buffer[256 * 256 * 4];
	int glx, gly, glwidth, glheight;

	glDrawBuffer(GL_FRONT);
	glReadBuffer(GL_FRONT);

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env0.rgb", buffer, sizeof(buffer));

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env1.rgb", buffer, sizeof(buffer));

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env2.rgb", buffer, sizeof(buffer));

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env3.rgb", buffer, sizeof(buffer));

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env4.rgb", buffer, sizeof(buffer));

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env5.rgb", buffer, sizeof(buffer));

	glDrawBuffer(GL_BACK);
	glReadBuffer(GL_BACK);
	GL_EndRendering();
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
static void R_TimeRefresh_f(void)
{
	int i;
	float start, time;

	glDrawBuffer(GL_FRONT);
	glFinish();

	start = Sys_FloatTime();
	for (i = 0; i < 128; i++)
		R_RenderView();

	glFinish();
	time = (float)Sys_FloatTime() - start;
	Con_Printf("%f seconds (%f fps)\n", time, 128 / time);

	glDrawBuffer(GL_BACK);
	GL_EndRendering();
}

/*
===============
R_Init
===============
*/
void R_Init(void)
{
	Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand("envmap", R_Envmap_f);
	Cmd_AddCommand("pointfile", R_ReadPointFile_f);

	Cvar_RegisterVariable(&r_norefresh);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_lightmap);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_shadows);
	Cvar_RegisterVariable(&r_mirroralpha);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_decals);

	Cvar_RegisterVariable(&gl_clear);
	Cvar_RegisterVariable(&gl_cull);
	Cvar_RegisterVariable(&gl_texsort);
	Cvar_RegisterVariable(&gl_smoothmodels);
	Cvar_RegisterVariable(&gl_affinemodels);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&gl_flashblend);
	Cvar_RegisterVariable(&gl_playermip);
	Cvar_RegisterVariable(&gl_nocolors);
	Cvar_RegisterVariable(&gl_keeptjunctions);
	Cvar_RegisterVariable(&gl_reporttjunctions);
	Cvar_RegisterVariable(&gl_wateramp);

	R_InitParticles();
	R_InitParticleTexture();

	// player skin textures
	playertextures = texture_extension_number;
	texture_extension_number += 16;
}

/*
=================
Hunk_TempAlloc

Return space from the top of the hunk
=================
*/
void *Hunk_TempAlloc(int size)
{
	void *buf;

	if (g_HunkTempActive)
	{
		Hunk_FreeToHighMark(g_HunkTempMark);
		g_HunkTempActive = false;
	}

	g_HunkTempMark = Hunk_HighMark();

	size = (size + 15) & ~15;
	buf = Hunk_HighAllocName(size, "temp");

	g_HunkTempActive = true;

	return buf;
}

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

typedef struct cache_system_s
{
	int						size;		// including this header
	cache_user_t			*user;
	char					name[16];
	struct cache_system_s	*prev, *next;
	struct cache_system_s	*lru_prev, *lru_next;	// for LRU flushing
} cache_system_t;

static cache_system_t g_CacheSentinel;

static cache_system_t *Cache_TryAlloc(int size, qboolean nobottom);

/*
===========
Cache_Move
===========
*/
static void Cache_Move(cache_system_t *c)
{
	cache_system_t *new;

	// we are clearing up space at the bottom, so only allocate it late
	new = Cache_TryAlloc(c->size, true);
	if (!new)
	{
		// tough luck...
		Cache_Free(c->user);
		return;
	}

	memcpy(new + 1, c + 1, c->size - sizeof(cache_system_t));
	new->user = c->user;
	memcpy(new->name, c->name, sizeof(new->name));
	Cache_Free(c->user);
	new->user->data = (void *)(new + 1);
}

/*
============
Cache_FreeLow

Throw things out until the hunk can be expanded to the given point
============
*/
static void Cache_FreeLow(int new_low_hunk)
{
	cache_system_t *c;

	while (1)
	{
		c = g_CacheSentinel.next;
		if (c == &g_CacheSentinel)
			return;		// nothing in cache at all
		if (g_HunkBase + new_low_hunk <= (byte *)c)
			return;		// there is space to grow the hunk
		Cache_Move(c);	// reclaim the space
	}
}

/*
============
Cache_FreeHigh

Throw things out until the hunk can be expanded to the given point
============
*/
static void Cache_FreeHigh(int new_high_hunk)
{
	cache_system_t *c, *prev;

	prev = NULL;
	while (1)
	{
		c = g_CacheSentinel.prev;
		if (c == &g_CacheSentinel)
			return;		// nothing in cache at all
		if (g_HunkBase + g_HunkSize - new_high_hunk >= (byte *)c + c->size)
			return;		// there is space to grow the hunk
		if (c == prev)
		{
			Cache_Free(c->user);	// didn't move out of the way
		}
		else
		{
			Cache_Move(c);			// try to move it
			prev = c;
		}
	}
}

/*
============
Cache_UnlinkLRU
============
*/
static void Cache_UnlinkLRU(cache_system_t *cs)
{
	cache_system_t *next, *prev;

	next = cs->lru_next;
	if (!next || !cs->lru_prev)
		Sys_Error("Cache_UnlinkLRU");
	prev = cs->lru_prev;

	next->lru_prev = prev;
	prev->lru_next = next;

	cs->lru_next = NULL;
	cs->lru_prev = NULL;
}

/*
============
Cache_MakeLRU
============
*/
static void Cache_MakeLRU(cache_system_t *cs)
{
	cache_system_t *head;

	if (cs->lru_next || cs->lru_prev)
		Sys_Error("Cache_MakeLRU_Active");

	head = g_CacheSentinel.lru_next;
	head->lru_prev = cs;
	cs->lru_prev = &g_CacheSentinel;
	cs->lru_next = head;
	g_CacheSentinel.lru_next = cs;
}

/*
============
Cache_TryAlloc

Looks for a free block of memory between the high and low hunk marks
Size should already include the header and padding
============
*/
static cache_system_t *Cache_TryAlloc(int size, qboolean nobottom)
{
	cache_system_t *cs, *new;

	if (nobottom || g_CacheSentinel.prev != &g_CacheSentinel)
	{
		// search from the bottom up for space
		new = (cache_system_t *)(g_HunkBase + g_HunkLowUsed);
		cs = g_CacheSentinel.next;

		do
		{
			if ((!nobottom || cs != g_CacheSentinel.next) && (byte *)cs - (byte *)new >= size)
			{
				// found space
				memset(new, 0, sizeof(*new));
				new->size = size;

				new->next = cs;
				new->prev = cs->prev;
				cs->prev = new;
				new->prev->next = new;

				Cache_MakeLRU(new);

				return new;
			}

			// continue looking
			new = (cache_system_t *)((byte *)cs + cs->size);
			cs = cs->next;
		} while (cs != &g_CacheSentinel);

		// try to allocate one at the very end
		if (g_HunkBase + g_HunkSize - g_HunkHighUsed - (byte *)new < size)
			return NULL;	// couldn't allocate

		memset(new, 0, sizeof(*new));
		new->size = size;

		new->next = &g_CacheSentinel;
		new->prev = g_CacheSentinel.prev;
		g_CacheSentinel.prev->next = new;
		g_CacheSentinel.prev = new;

		Cache_MakeLRU(new);

		return new;
	}

	// the cache is completely empty
	if (g_HunkSize - g_HunkLowUsed - g_HunkHighUsed < size)
		Sys_Error("Cache_TryAlloc: %i is greater then free hunk", size);

	new = (cache_system_t *)(g_HunkBase + g_HunkLowUsed);
	memset(new, 0, sizeof(*new));
	new->size = size;

	g_CacheSentinel.next = new;
	g_CacheSentinel.prev = new;
	new->next = &g_CacheSentinel;
	new->prev = &g_CacheSentinel;

	Cache_MakeLRU(new);

	return new;
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush(void)
{
	while (g_CacheSentinel.next != &g_CacheSentinel)
		Cache_Free(g_CacheSentinel.next->user);	// reclaim the space
}

/*
============
Cache_Report
============
*/
void Cache_Report(void)
{
	Con_DPrintf("%4.1f megabyte data cache\n", (g_HunkSize - g_HunkHighUsed - g_HunkLowUsed) / (float)(1024 * 1024));
}

/*
============
Cache_Init
============
*/
void Cache_Init(void)
{
	g_CacheSentinel.next = g_CacheSentinel.prev = &g_CacheSentinel;
	g_CacheSentinel.lru_next = g_CacheSentinel.lru_prev = &g_CacheSentinel;

	Cmd_AddCommand("flush", Cache_Flush);
}

/*
==============
Cache_Free

Frees the memory and removes it from the LRU list
==============
*/
void Cache_Free(cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		Sys_Error("Cache_FreeNotAllocated");

	cs = ((cache_system_t *)c->data) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = NULL;

	c->data = NULL;

	Cache_UnlinkLRU(cs);
}

/*
==============
Cache_Check
==============
*/
void *Cache_Check(cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		return NULL;

	cs = ((cache_system_t *)c->data) - 1;

	// move to head of LRU
	Cache_UnlinkLRU(cs);
	Cache_MakeLRU(cs);

	return c->data;
}

/*
==============
Cache_Alloc
==============
*/
void *Cache_Alloc(cache_user_t *c, int size, const char *name)
{
	cache_system_t *cs;

	if (c->data)
		Sys_Error("Cache_AllocAlloc");

	if (size <= 0)
		Sys_Error("Cache_Alloc: size %i", size);

	size = (size + (int)sizeof(cache_system_t) + 15) & ~15;

	// find memory for it
	while (1)
	{
		cs = Cache_TryAlloc(size, false);
		if (cs)
		{
			strncpy(cs->name, name, sizeof(cs->name) - 1);
			c->data = (void *)(cs + 1);
			cs->user = c;
			break;
		}

		// free the least recently used cache data
		if (g_CacheSentinel.lru_prev == &g_CacheSentinel)
			Sys_Error("Cache_AllocOutOfMemory");	// not enough memory at all
		Cache_Free(g_CacheSentinel.lru_prev->user);
	}

	return Cache_Check(c);
}

//============================================================================

/*
========================
Memory_Init
========================
*/
void Memory_Init(void *buf, int size)
{
	int p;
	int zonesize = DYNAMIC_SIZE;

	g_HunkBase = buf;
	g_HunkSize = size;
	g_HunkLowUsed = 0;
	g_HunkHighUsed = 0;

	Cache_Init();

	p = COM_CheckParm("-zone");
	if (p)
	{
		if (com_argc - 1 <= p)
			Sys_Error("Memory_Init: you must specify a size in KB after -zone");
		zonesize = Q_atoi(com_argv[p + 1]) * 1024;
	}

	g_MainZone = Hunk_AllocName(zonesize, "zone");
	Z_ClearZone(g_MainZone, zonesize);
}
