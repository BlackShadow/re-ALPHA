#ifndef ZONE_H
#define ZONE_H

 // =========================================================
 // Memory tags
 // =========================================================

#define Z_MAINZONE      1
#define Z_STATZONE      2

 // =========================================================
 // Zone memory
 // =========================================================

void Z_Free(void *ptr);
void *Z_Malloc(int size);
void *Z_TagMalloc(int size, int tag);
void Z_CheckHeap(void);

 // =========================================================
 // Hunk memory
 // =========================================================

 // Globals backing the hunk/ cache allocators.
extern byte *g_HunkBase;
extern int g_HunkSize;
extern int g_HunkLowUsed;
extern int g_HunkHighUsed;
extern int g_HunkTempActive;
extern int g_HunkTempMarks[1];

void *Hunk_Alloc(int size);
void *Hunk_AllocName(int size, const char *name);

int Hunk_LowMark(void);
int Hunk_FreeToLowMark(int mark);

void *Hunk_HighAllocName(int size, const char *name);
int Hunk_HighMark(void);
void Hunk_FreeToHighMark(int mark);

void *Hunk_TempAlloc(int size);
int Hunk_Check(void);
void Memory_Init(void *base, int size);

 // =========================================================
 // Cache memory
 // =========================================================

typedef struct cache_user_s
{
    void    *data;
} cache_user_t;

typedef struct cache_system_s
{
	int					size; // total size including header
	cache_user_t			*user; // owning cache user (points at c->data)
	char				name[16];
	struct cache_system_s	*prev;
	struct cache_system_s	*next;
	struct cache_system_s	*lru_prev;
	struct cache_system_s	*lru_next;
} cache_system_t;

extern cache_system_t g_CacheSentinel;

void Cache_Flush(void);
void Cache_Report(void);
void Cache_Init(void);
void *Cache_Check(cache_user_t *c);
void Cache_Free(cache_user_t *c);
void *Cache_Alloc(cache_user_t *c, int size, const char *name);

#endif // ZONE_H
