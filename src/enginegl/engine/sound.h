#ifndef SOUND_H
#define SOUND_H

 // =========================================================
 // Sound constants
 // =========================================================

#define MAX_CHANNELS        128
#define MAX_SFX             512

 // =========================================================
 // DMA buffer structure
 // =========================================================

typedef struct dma_s
{
	qboolean    gamealive;
	qboolean    soundalive;
	qboolean    splitbuffer;
	int         channels;
	int         samples; // mono samples in buffer
	int         submission_chunk; // don't mix less than this #
	int         samplepos; // in mono samples
	int         samplebits;
	int         speed; // dmaspeed in HL
	unsigned char *buffer;
} dma_t;

extern volatile dma_t *shm;
extern volatile dma_t sn;

 // =========================================================
 // Sound structures
 // =========================================================

typedef struct sfxcache_s
{
    int length;
    int loopstart;
    int speed;
    int width;
    int stereo;
    byte data[1];
} sfxcache_t;

typedef struct sfx_s
{
    char name[MAX_QPATH];
    cache_user_t cache;
} sfx_t;

typedef struct
{
    sfx_t *sfx;
    int leftvol;
    int rightvol;
    int end;
    int pos;
    int looping;
    int entnum;
    int entchannel;
    vec3_t origin;
    vec_t dist_mult;
    int master_vol;
} channel_t;

 // =========================================================
 // Prototypes
 // =========================================================

int S_Init(void);
void S_Startup(void);
void S_Shutdown(void);
void S_BlockSound(void);
void S_UnblockSound(void);
void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation);
void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_StopSound(int entnum, int entchannel);
void S_StopAllSounds(qboolean clear);
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up);
void S_ClearBuffer(void);
void S_ExtraUpdate(void);
void S_AmbientOn(void);
void S_AmbientOff(void);
void S_LocalSound(const char *sound);
void S_InsertText(const char *text);
sfx_t *S_PrecacheSound(const char *sample);

 // CD audio (cd_win.c)
int CDAudio_Init(void);
void CDAudio_Shutdown(void);
void CDAudio_Update(void);

#endif // SOUND_H

