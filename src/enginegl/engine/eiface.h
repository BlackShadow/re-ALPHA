#ifndef EIFACE_H
#define EIFACE_H

#include "edict.h"
#include "mathlib.h"

#define MAX_GAME_DLLS	50

typedef enum
{
	at_notice,		// "NOTE:" message box in developer mode
	at_console,		// plain console text
	at_warning,		// "WARNING:" message box in developer mode
	at_error		// "ERROR:" message box in developer mode
} ALERT_TYPE;

// message destinations for the PF_Write* builtins and MSG_Write*_Dest
#define MSG_BROADCAST	0		// unreliable to all
#define MSG_ONE			1		// reliable to one (msg_entity)
#define MSG_ALL			2		// reliable to all
#define MSG_INIT		3		// write to the init string

// DispatchEntityCallback: game DLL exports, or the progs function of the same name
#define ENTITYFUNC_CLIENTDISCONNECT		0
#define ENTITYFUNC_PLAYERPRETHINK		1
#define ENTITYFUNC_PLAYERPOSTTHINK		2
#define ENTITYFUNC_STARTFRAME			3
#define ENTITYFUNC_SETNEWPARMS			4
#define ENTITYFUNC_SETCHANGEPARMS		5
#define ENTITYFUNC_CLIENTKILL			6
#define ENTITYFUNC_CLIENTCONNECT		7
#define ENTITYFUNC_PUTCLIENTINSERVER	8
#define NUM_ENTITYFUNCS					9

typedef struct TraceResult_s
{
	int		fAllSolid;			// if true, plane is not valid
	int		fStartSolid;		// if true, the initial point was in a solid area
	int		fInOpen;
	int		fInWater;
	float	flFraction;			// time completed, 1.0 = didn't hit anything
	vec3_t	vecEndPos;			// final position
	float	flPlaneDist;
	vec3_t	vecPlaneNormal;		// surface normal at impact
	int		pHit;				// entity the surface is on, as an edict offset
} TraceResult;

typedef struct
{
	char	*szClassName;
	char	*szKeyName;
	char	*szValue;
	int		fHandled;
} KeyValueData;

extern void *g_pfnDispatchSpawn;
extern void *g_pfnDispatchThink;
extern void *g_pfnDispatchUse;
extern void *g_pfnDispatchTouch;
extern void *g_pfnDispatchSave;
extern void *g_pfnDispatchRestore;
extern void *g_pfnDispatchKeyValue;
extern void *g_pfnDispatchBlocked;

//
// eiface.c
//
void *GetDispatch(const char *pszProcName);
void *GetEntityInit(const char *pszClassName);
void *LoadThisDll(const char *szDLLPath);
int UnloadEntityDLLs(void);
int EngineFprintf(FILE *Stream, const char *Format, ...);
void AlertMessage(ALERT_TYPE atype, const char *fmt, ...);
void DispatchEntityCallback(int callbackIndex);

//
// pr_cmds.c
//
int PF_precache_model_internal(const char *s);
void PF_precache_sound_internal(const char *s);
void PF_setmodel_I(edict_t *e, const char *m);
int PF_modelindex(const char *name);
void PF_setsize_I(edict_t *e, float *min, float *max);
int PF_setspawnparms(edict_t *ent);
float VectorToYaw(float *value1);
void VectorAngles(float *forward, float *angles);
edict_t *FindEntityByString(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue);
int GetEntityIllum(edict_t *pEnt);
edict_t *FindEntityInSphere(vec3_t org, float rad);
edict_t *PF_checkclient_I(void);
void PF_makevectors_I(float *angles);
edict_t *PF_Spawn_I(void);
void PF_Remove_I(edict_t *ed);
void PF_makestatic_I(edict_t *ent);
int PF_checkbottom_I(edict_t *ent);
float PF_droptofloor_I(edict_t *ent);
float PF_walkmove_I(edict_t *ent, float yaw, float dist);
void PF_setorigin_I(edict_t *e, float *org);
void PF_sound_I(edict_t *entity, int channel, const char *sample, float volume, float attenuation);
void PF_ambientsound_I(float *pos, const char *samp, float vol, float attenuation);
void PF_traceline_DLL(float *v1, float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr);
void PF_TraceToss_DLL(edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr);
void PF_aim_I(edict_t *ent, float speed, float *dir);
void PF_localcmd_I(char *str);
void PF_stuffcmd_I(edict_t *pEdict, char *szFmt, ...);
void SV_StartParticle(float *org, float *dir, float color, float count);
void SV_BroadcastLightStyle(int style, const char *value);
char *SV_DecalSetName(int decal, const char *name);
int PF_pointcontents_I(vec3_t p);
byte *MSG_WriteByte_Dest(int dest, int c);
byte *MSG_WriteChar_Dest(int dest, int c);
short *MSG_WriteShort_Dest(int dest, int i);
int *MSG_WriteLong_Dest(int dest, int i);
byte *MSG_WriteAngle_Dest(int dest, float f);
short *MSG_WriteCoord_Dest(int dest, float f);
int MSG_WriteString_Dest(int dest, char *s);
short *MSG_WriteEntity_Dest(int dest, int i);

//
// pr_edict.c
//
float ED_GetCvarValue(const char *cvarname);
char *ED_GetCvarString(const char *cvarname);
void ED_SetCvarValue(const char *cvarname, float value);
void ED_SetCvarString(const char *cvarname, const char *value);
void ED_AssertMethodNotChanged(void);
void ED_EntVarsToCl(edict_t *ent);
void *ED_FindModel(edict_t *ent);

#endif // EIFACE_H
