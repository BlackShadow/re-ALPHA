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

#include "quakedef.h"

#define ZONEID 1919505
#define HUNK_SENTINEL 502268397

typedef struct memblock_s
{
	int size;
	int tag;
	int id;
	struct memblock_s *next;
	struct memblock_s *prev;
	int pad;
} memblock_t;

typedef struct memzone_s
{
	int size;
	memblock_t blocklist;
	memblock_t *rover;
} memzone_t;

static memzone_t *g_MainZone;

byte *g_HunkBase;
int g_HunkSize;
int g_HunkLowUsed;
int g_HunkHighUsed;
int g_HunkTempActive;
int g_HunkTempMarks[1];

cache_system_t g_CacheSentinel;

static void Cache_UnlinkLRU(cache_system_t *cache);
static void Cache_MakeLRU(cache_system_t *cache);
static cache_system_t *Cache_TryAlloc(int size, int noBottom);
static void Cache_Move(cache_system_t *cache);
static void Cache_FreeHigh(int newLowHunkUsed);
static void Cache_FreeLow(int newHighHunkUsed);

void Z_CheckHeap(void)
{
	memblock_t *block;

	block = g_MainZone->blocklist.next;
	while (block->next != &g_MainZone->blocklist)
	{

		if ((byte *)block + block->size != (byte *)block->next)
			Sys_Error("Z_CheckHeap: block size does not touch the next block\n");

		if (block->next->prev != block)
			Sys_Error("Z_CheckHeap: next block doesn't have proper back link\n");

		if (!block->tag && !block->next->tag)
			Sys_Error("Z_CheckHeap: two consecutive free blocks\n");

		block = block->next;
	}
}

static void Z_Print(memzone_t *zone)
{
	memblock_t *block;

	Con_Printf("zone size: %i  location: %p\n", zone->size, (const void *)zone);

	for (block = zone->blocklist.next; ; block = block->next)
	{
		Con_Printf("block:%p    size:%7i    tag:%3i\n", block, block->size, block->tag);

		if (block->next == &zone->blocklist)
			break;

		if ((byte *)block + block->size != (byte *)block->next)
			Con_Printf("ERROR: block size does not touch the next block\n");

		if (block->next->prev != block)
			Con_Printf("ERROR: next block doesn't have proper back link\n");

		if (!block->tag && !block->next->tag)
			Con_Printf("ERROR: two consecutive free blocks\n");
	}
}

void *Z_TagMalloc(int size, int tag)
{
	memblock_t *base;
	memblock_t *rover;
	memblock_t *start;
	int allocsize;
	int extra;

	if (!tag)
		Sys_Error("Z_TagMalloc: tried to use a 0 tag");

	allocsize = (size + 35) & ~7;

	rover = g_MainZone->rover;
	base = rover;
	start = rover->prev;

	do
	{
		qboolean wasFree;

		if (rover == start)
			return NULL;

		wasFree = (rover->tag == 0);
		rover = rover->next;
		if (!wasFree)
			base = rover;
	} while (base->tag || base->size < allocsize);

	extra = base->size - allocsize;
	if (extra > 64)
	{
		memblock_t *newblock;

		newblock = (memblock_t *)((byte *)base + allocsize);
		newblock->prev = base;
		newblock->tag = 0;
		newblock->id = ZONEID;
		newblock->size = extra;

		newblock->next = base->next;
		base->size = allocsize;

		newblock->next->prev = newblock;
		base->next = newblock;
	}

	base->tag = tag;
	g_MainZone->rover = base->next;

	base->id = ZONEID;
	*(int *)((byte *)base + base->size - 4) = ZONEID;

	return (void *)(base + 1);
}

void *Z_Malloc(int size)
{
	void *ptr;

	Z_CheckHeap();
	ptr = Z_TagMalloc(size, 1);

	if (!ptr)
		Sys_Error("Z_Malloc: failed on allocation of %i bytes", size);

	Q_memset(ptr, 0, size);
	return ptr;
}

static void Z_ClearZone(memzone_t *zone, int size)
{
	memblock_t *block;

	zone->size = size;

	block = (memblock_t *)((byte *)zone + sizeof(*zone));

	zone->blocklist.size = 0;
	zone->blocklist.tag = 1;
	zone->blocklist.id = 0;
	zone->blocklist.next = block;
	zone->blocklist.prev = block;

	zone->rover = block;

	block->size = size - (int)sizeof(*zone);
	block->tag = 0;
	block->id = ZONEID;
	block->next = &zone->blocklist;
	block->prev = &zone->blocklist;
	block->pad = 0;
}

void Z_Free(void *ptr)
{
	memblock_t *block;
	memblock_t *other;

	if (!ptr)
		Sys_Error("Z_Free: NULL pointer");

	block = (memblock_t *)((byte *)ptr - sizeof(*block));
	if (block->id != ZONEID)
		Sys_Error("Z_Free: freed a pointer without ZONEID");

	if (!block->tag)
		Sys_Error("Z_Free: freed a freed pointer");

	block->tag = 0;

	other = block->prev;
	if (!other->tag)
	{
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
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;

		if (g_MainZone->rover == other)
			g_MainZone->rover = block;
	}
}

int Hunk_Check(void)
{
	int *block;
	int blocksize;

	block = (int *)g_HunkBase;
	while ((byte *)block != g_HunkBase + g_HunkLowUsed)
	{

		if (block[0] != HUNK_SENTINEL)
			Sys_Error("Hunk_Check: trashed sentinel");

		blocksize = block[1];

		if (blocksize < 16 || (byte *)block + blocksize - g_HunkBase > g_HunkSize)
			Sys_Error("Hunk_Check: bad size");

		block = (int *)((byte *)block + blocksize);
	}

	return g_HunkLowUsed;
}

int Hunk_LowMark(void)
{
	return g_HunkLowUsed;
}

int Hunk_FreeToLowMark(int mark)
{
	if (mark < 0 || g_HunkLowUsed < mark)
		Sys_Error("Hunk_FreeToLowMark: bad mark %i", mark);

	memset(g_HunkBase + mark, 0, g_HunkLowUsed - mark);
	g_HunkLowUsed = mark;

	return 0;
}

int Hunk_HighMark(void)
{
	if (g_HunkTempActive)
	{
		g_HunkTempActive = 0;
		Hunk_FreeToHighMark(g_HunkTempMarks[0]);
	}

	return g_HunkHighUsed;
}

void Hunk_FreeToHighMark(int mark)
{
	if (g_HunkTempActive)
	{
		g_HunkTempActive = 0;
		Hunk_FreeToHighMark(g_HunkTempMarks[0]);
	}

	if (mark < 0 || mark > g_HunkHighUsed)
		Sys_Error("Hunk_FreeToHighMark: bad mark %i", mark);

	memset(g_HunkBase + g_HunkSize - g_HunkHighUsed, 0, g_HunkHighUsed - mark);
	g_HunkHighUsed = mark;
}

void *Hunk_AllocName(int size, const char *name)
{
	int allocsize;
	int *hunk;
	void *memptr;

	if (size < 0)
		Sys_Error("Hunk_Alloc: bad size: %i", size);

	allocsize = ((size + 15) & 0xFFFFFFF0) + 16;

	if (g_HunkSize - g_HunkLowUsed - g_HunkHighUsed < allocsize)
		Sys_Error("Hunk_Alloc: failed on %i bytes", allocsize);

	g_HunkLowUsed += allocsize;
	Cache_FreeHigh(g_HunkLowUsed);

	hunk = (int *)(g_HunkBase + (g_HunkLowUsed - allocsize));
	memptr = (void *)(g_HunkBase + (g_HunkLowUsed - allocsize));

	memset(memptr, 0, allocsize);

	hunk[1] = allocsize;
	hunk[0] = HUNK_SENTINEL;
	Q_strncpy((char *)(hunk + 2), name, 8);

	return hunk + 4;
}

void *Hunk_Alloc(int size)
{
	return Hunk_AllocName(size, "unknown");
}

void *Hunk_HighAllocName(int size, const char *name)
{
	int allocsize_temp;
	int allocsize;
	int *hunk;

	if (size < 0)
		Sys_Error("Hunk_HighAllocName: bad size: %i", size);

	if (g_HunkTempActive)
	{
		Hunk_FreeToHighMark(*(int *)g_HunkTempMarks);
		g_HunkTempActive = 0;
	}

	allocsize_temp = size + 15;
	allocsize_temp &= 0xFFFFFFF0;
	allocsize = allocsize_temp + 16;

	if (g_HunkSize - g_HunkLowUsed - g_HunkHighUsed >= allocsize)
	{
		g_HunkHighUsed += allocsize;
		Cache_FreeLow(g_HunkHighUsed);

		hunk = (int *)(g_HunkBase + g_HunkSize - g_HunkHighUsed);
		memset(hunk, 0, allocsize);

		hunk[1] = allocsize;
		hunk[0] = HUNK_SENTINEL;
		Q_strncpy((char *)(hunk + 2), name, 8);

		return hunk + 4;
	}
	else
	{
		Con_Printf("Hunk_HighAlloc: failed on %i bytes\n", allocsize);
		return NULL;
	}
}

void R_InitParticleTexture(void)
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

	byte pixels[8 * 8 * 4];
	int x, y;

	extern int texture_extension_number;
	extern int particletexture;
	extern void GL_Bind(int texnum);

	particletexture = texture_extension_number++;
	GL_Bind(particletexture);

	for (y = 0; y < 8; ++y)
	{
		for (x = 0; x < 8; ++x)
		{
			byte *p = &pixels[(y * 8 + x) * 4];
			p[0] = 0xFF;
			p[1] = 0xFF;
			p[2] = 0xFF;
			p[3] = (byte)(dottexture[x][y] ? 0xFF : 0x00);
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, 8448.0f);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 9729.0f);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 9729.0f);
}

void R_Envmap_f(void)
{
	byte pixels[262144];
	int glx, gly, glwidth, glheight;

	extern void VID_GetWindowSize(int *x, int *y, int *width, int *height);
	extern void R_RenderView(void);
	extern void GL_EndRendering(void);

	glDrawBuffer(GL_FRONT);
	glReadBuffer(GL_FRONT);

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	COM_WriteFile("env0.rgb", pixels, 0x40000u);

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	COM_WriteFile("env1.rgb", pixels, 0x40000u);

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	COM_WriteFile("env2.rgb", pixels, 0x40000u);

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	COM_WriteFile("env3.rgb", pixels, 0x40000u);

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	COM_WriteFile("env4.rgb", pixels, 0x40000u);

	VID_GetWindowSize(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	COM_WriteFile("env5.rgb", pixels, 0x40000u);

	glDrawBuffer(GL_BACK);
	glReadBuffer(GL_BACK);
	GL_EndRendering();
}

void R_TimeRefresh_f(void)
{
	int i;
	float start;
	float elapsed;

	extern void R_RenderView(void);
	extern void GL_EndRendering(void);

	i = 0;
	glDrawBuffer(GL_FRONT);
	glFinish();

	start = (float)Sys_FloatTime();
	do
	{
		(void)i++;
		R_RenderView();
	} while (i < 128);

	glFinish();
	elapsed = (float)Sys_FloatTime() - start;
	Con_Printf("%f seconds (%f fps)\n", elapsed, 128.0f / elapsed);

	glDrawBuffer(GL_BACK);
	GL_EndRendering();
}

int R_InitBrushLighting(void)
{
	extern int texture_extension_number;
	extern int playertextures;
	extern int R_InitParticles(void);
	extern void R_ReadPointFile_f(void);

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

	playertextures = texture_extension_number;
	texture_extension_number += 16;
	return playertextures;
}

void *Hunk_TempAlloc(int size)
{
	void *result;

	if (g_HunkTempActive)
	{
		Hunk_FreeToHighMark(g_HunkTempMarks[0]);
		g_HunkTempActive = 0;
	}

	g_HunkTempMarks[0] = Hunk_HighMark();
	result = Hunk_HighAllocName((size + 15) & ~15, "temp");
	g_HunkTempActive = 1;
	return result;
}

static void Cache_Move(cache_system_t *cache)
{
	cache_system_t *newCache;
	void *newData;

	newCache = Cache_TryAlloc(cache->size, 1);
	if (!newCache)
	{
		Cache_Free(cache->user);
		return;
	}

	newData = (void *)(newCache + 1);

	memcpy(newData, (byte *)cache + sizeof(*cache), cache->size - (int)sizeof(*cache));
	newCache->user = cache->user;
	memcpy(newCache->name, cache->name, sizeof(newCache->name));

	Cache_Free(cache->user);
	newCache->user->data = newData;
}

static void Cache_FreeHigh(int newLowHunkUsed)
{
	cache_system_t *cache;

	while ((cache = g_CacheSentinel.next) != &g_CacheSentinel)
	{
		if (g_HunkBase + newLowHunkUsed <= (byte *)cache)
			break;
		Cache_Move(cache);
	}
}

static void Cache_FreeLow(int newHighHunkUsed)
{
	cache_system_t *cache;
	cache_system_t *lastCache;

	lastCache = NULL;
	while ((cache = g_CacheSentinel.prev) != &g_CacheSentinel)
	{
		byte *highHunkBase;

		highHunkBase = g_HunkBase + g_HunkSize - newHighHunkUsed;
		if (highHunkBase >= (byte *)cache + cache->size)
			break;

		if (lastCache == cache)
		{
			Cache_Free(cache->user);
		}
		else
		{
			lastCache = cache;
			Cache_Move(cache);
		}
	}
}

static void Cache_UnlinkLRU(cache_system_t *cache)
{
	cache_system_t *next;
	cache_system_t *prev;

	next = cache->lru_next;
	if (!next || !cache->lru_prev)
		Sys_Error("Cache_UnlinkLRU");

	prev = cache->lru_prev;
	next->lru_prev = prev;
	prev->lru_next = next;
	cache->lru_next = NULL;
	cache->lru_prev = NULL;
}

static void Cache_MakeLRU(cache_system_t *cache)
{
	cache_system_t *oldHead;

	if (cache->lru_next || cache->lru_prev)
		Sys_Error("Cache_MakeLRU_Active");

	oldHead = g_CacheSentinel.lru_next;
	oldHead->lru_prev = cache;

	cache->lru_prev = &g_CacheSentinel;
	cache->lru_next = oldHead;
	g_CacheSentinel.lru_next = cache;
}

static cache_system_t *Cache_TryAlloc(int size, int noBottom)
{
	cache_system_t *cache;
	cache_system_t *newCache;

	if (noBottom || g_CacheSentinel.prev != &g_CacheSentinel)
	{
		newCache = (cache_system_t *)(g_HunkBase + g_HunkLowUsed);
		cache = g_CacheSentinel.next;

		do
		{
			if ((!noBottom || cache != g_CacheSentinel.next) && (byte *)cache - (byte *)newCache >= size)
			{
				memset(newCache, 0, sizeof(*newCache));
				newCache->size = size;
				newCache->next = cache;
				newCache->prev = cache->prev;

				cache->prev = newCache;
				newCache->prev->next = newCache;

				Cache_MakeLRU(newCache);
				return newCache;
			}

			newCache = (cache_system_t *)((byte *)cache + cache->size);
			cache = cache->next;
		} while (cache != &g_CacheSentinel);

		if (g_HunkBase + g_HunkSize - g_HunkHighUsed - (byte *)newCache < size)
			return NULL;

		memset(newCache, 0, sizeof(*newCache));
		newCache->size = size;
		newCache->next = &g_CacheSentinel;
		newCache->prev = g_CacheSentinel.prev;
		g_CacheSentinel.prev->next = newCache;
		g_CacheSentinel.prev = newCache;

		Cache_MakeLRU(newCache);
		return newCache;
	}

	if (g_HunkSize - g_HunkLowUsed - g_HunkHighUsed < size)
		Sys_Error("Cache_TryAlloc: %i is greater then free hunk", size);

	cache = (cache_system_t *)(g_HunkBase + g_HunkLowUsed);
	memset(cache, 0, sizeof(*cache));
	cache->size = size;

	g_CacheSentinel.next = cache;
	g_CacheSentinel.prev = cache;

	cache->next = &g_CacheSentinel;
	cache->prev = &g_CacheSentinel;

	Cache_MakeLRU(cache);
	return cache;
}

void Cache_Flush(void)
{
	while (g_CacheSentinel.next != &g_CacheSentinel)
		Cache_Free(g_CacheSentinel.next->user);
}

void Cache_Report(void)
{
	const float free_bytes = (float)(g_HunkSize - g_HunkHighUsed - g_HunkLowUsed);
	Con_DPrintf("%4.1f megabyte data cache\n", free_bytes / 1048576.0f);
}

void Cache_Init(void)
{
	g_CacheSentinel.prev = &g_CacheSentinel;
	g_CacheSentinel.next = &g_CacheSentinel;
	g_CacheSentinel.lru_prev = &g_CacheSentinel;
	g_CacheSentinel.lru_next = &g_CacheSentinel;

	Cmd_AddCommand("flush", Cache_Flush);
}

void Cache_Free(cache_user_t *c)
{
	cache_system_t *cache;

	if (!c->data)
		Sys_Error("Cache_FreeNotAllocated");

	cache = (cache_system_t *)((byte *)c->data - sizeof(*cache));

	cache->prev->next = cache->next;
	cache->next->prev = cache->prev;
	cache->prev = NULL;
	cache->next = NULL;

	c->data = NULL;
	Cache_UnlinkLRU(cache);
}

void *Cache_Check(cache_user_t *c)
{
	cache_system_t *cache;

	if (!c->data)
		return NULL;

	cache = (cache_system_t *)((byte *)c->data - sizeof(*cache));
	Cache_UnlinkLRU(cache);
	Cache_MakeLRU(cache);
	return c->data;
}

void *Cache_Alloc(cache_user_t *c, int size, const char *name)
{
	cache_system_t *cache;

	if (c->data)
		Sys_Error("Cache_AllocAlloc");
	if (size <= 0)
		Sys_Error("Cache_Alloc: size %i", size);

	while (1)
	{
		cache = Cache_TryAlloc((size + 55) & ~15, 0);
		if (cache)
			break;

		if (g_CacheSentinel.lru_prev == &g_CacheSentinel)
			Sys_Error("Cache_AllocOutOfMemory");

		Cache_Free(g_CacheSentinel.lru_prev->user);
	}

	strncpy(cache->name, name, 15);
	c->data = (void *)(cache + 1);
	cache->user = c;
	return Cache_Check(c);
}

void Memory_Init(void *base, int size)
{
	int zoneSize;
	int zoneParm;

	zoneSize = 49152;
	g_HunkBase = (byte *)base;
	g_HunkSize = size;
	g_HunkLowUsed = 0;
	g_HunkHighUsed = 0;

	Cache_Init();

	zoneParm = COM_CheckParm("-zone");
	if (zoneParm)
	{
		if (com_argc - 1 <= zoneParm)
			Sys_Error("Memory_Init: you must specify a size in KB after -zone");

		zoneSize = Q_atoi(com_argv[zoneParm + 1]) << 10;
	}

	g_MainZone = (memzone_t *)Hunk_AllocName(zoneSize, "zone");
	Z_ClearZone(g_MainZone, zoneSize);
}
