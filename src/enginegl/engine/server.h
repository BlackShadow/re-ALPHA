
#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "edict.h"
#include "usercmd.h"

typedef enum
{
	ss_loading = 0,
	ss_active = 1,
} server_state_t;

 // =============================================================================
 // Server constants
 // =============================================================================

 // MAX_SOUNDS, MAX_MODELS, MAX_LIGHTSTYLES defined in quakedef.h
 // MAX_MSGLEN defined in common.h

 // =============================================================================
 // Server structures
 // =============================================================================

typedef struct server_client_s
{
	qboolean    active;
	qboolean    spawned;
	qboolean    dropasap;
	qboolean    privileged;
	qboolean    sendsignon;
	int         reserved_0x14; // padding (unused)
	double      last_message;
	struct qsocket_s *netconnection;

	usercmd_t   cmd;
	byte        reserved_0x40[12]; // padding

	sizebuf_t   message;
	byte        msgbuf[MAX_MSGLEN]; // message buffer

	edict_t     *edict;
	char        name[32];
	int         colors;
	float       ping_times[16];
	int			ping_time_index;
	float       spawn_parms[NUM_SPAWN_PARMS];
	int         old_frags;
} server_client_t;

typedef struct server_static_s
{
	int         maxclients;
	int         maxclientslimit;
	server_client_t    *clients;
	int         serverflags;
	qboolean    changelevel_issued;
} server_static_t;

typedef struct server_s
{
	qboolean    active;
	qboolean    paused;
	qboolean    loadgame;

	server_state_t state;
	double      time;

 // PF_checkclient state (pr_cmds.c)
	double      lastchecktime;
	int         lastcheck;

	char        name[64];
	char        startspot[64];
	char        modelname[64];

	struct model_s *worldmodel;
	struct model_s *models[MAX_MODELS]; // loaded models array

	sizebuf_t   datagram;
	byte        datagram_buf[MAX_DATAGRAM];
	sizebuf_t   reliable_datagram;
	byte        reliable_datagram_buf[MAX_DATAGRAM];
	sizebuf_t   signon;
	byte        signon_buf[MAX_SIGNON];

	int         num_edicts;
	int         max_edicts;
	edict_t     *edicts;

	int         areanode_count;
	void        *areanode_data;

	char        *sound_precache[MAX_SOUNDS];
	char        *model_precache[MAX_MODELS];
	char        *lightstyles[MAX_LIGHTSTYLES];
} server_t;

 // =============================================================================
 // Global variables
 // =============================================================================

extern server_t sv;
extern server_static_t svs;

 // Compatibility: some translations refer to sv.time as sv_time.
#define sv_time (sv.time)
 // Compatibility: some ports/ translations refer to these fields as globals.
#define sv_paused (sv.paused)
#define sv_loadgame (sv.loadgame)
#define sv_signon (sv.signon)
#define sv_lightstyles (sv.lightstyles)

 // Server cvars (defined in sv_main.c)
extern cvar_t sv_maxvelocity;
extern cvar_t sv_gravity;
extern cvar_t sv_friction;
extern cvar_t sv_edgefriction;
extern cvar_t sv_stopspeed;
extern cvar_t sv_maxspeed;
extern cvar_t sv_accelerate;
extern cvar_t sv_idealpitchscale;
extern cvar_t sv_aim;
extern cvar_t sv_nostep;

int SV_ClientPrintf(char *format, ...);
int SV_ClientCommand(char *format, ...);
int SV_BroadcastPrintf(char *format, ...);
void SV_DropClient(qboolean crash);
void SV_SetIdealPitch(void);
void SV_Init(void);
void SV_Physics(void);
void SV_ClearDatagram(void);
void SV_CheckForNewClients(void);
void SV_RunClients(void);
void SV_SendClientMessages(void);
void SV_SpawnServer(char *server, char *startspot);
void SV_SaveSpawnparms(void);
void SV_StartSound(edict_t *entity, int channel, const char *sample, int volume, float attenuation);
int SV_ModelIndex(char *name);
void SV_WriteClientdataToMessage(edict_t *ent, sizebuf_t *msg);
void SV_ClientThink(void);

#endif // SERVER_H
