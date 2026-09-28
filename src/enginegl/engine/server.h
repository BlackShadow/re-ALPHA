// server.h -- server state and interface

#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "edict.h"
#include "usercmd.h"

typedef enum
{
	ss_loading,
	ss_active
} server_state_t;

#define NUM_PING_TIMES		16

typedef struct server_client_s
{
	qboolean	active;				// false = client is free
	qboolean	spawned;			// false = don't send datagrams
	qboolean	dropasap;			// has been told to go to another level
	qboolean	privileged;			// can execute any host command
	qboolean	sendsignon;			// only valid before spawned

	double		last_message;		// reliable messages must be sent
									// periodically

	struct qsocket_s *netconnection;	// communications handle

	usercmd_t	cmd;				// movement
	vec3_t		wishdir;			// unused

	sizebuf_t	message;			// can be added to at any time,
									// copied and clear once per frame
	byte		msgbuf[MAX_MSGLEN];
	edict_t		*edict;				// EDICT_NUM(clientnum+1)
	char		name[32];			// for printing to other people
	int			colors;

	float		ping_times[NUM_PING_TIMES];
	int			num_pings;			// ping_times[num_pings & (NUM_PING_TIMES - 1)]

	// spawn parms are carried from level to level
	float		spawn_parms[NUM_SPAWN_PARMS];

	// client known data for deltas
	int			old_frags;
} server_client_t;

typedef struct server_static_s
{
	int			maxclients;
	int			maxclientslimit;
	server_client_t	*clients;		// [maxclients]
	int			serverflags;		// episode completion information
	qboolean	changelevel_issued;	// cleared when at SV_SpawnServer
} server_static_t;

typedef struct server_s
{
	qboolean	active;				// false if only a net client

	qboolean	paused;
	qboolean	loadgame;			// handle connections specially

	server_state_t	state;			// some actions are only valid during load

	double		time;

	double		lastchecktime;		// PF_checkclient
	int			lastcheck;

	char		name[64];			// map name
	char		startspot[64];		// landmark of a level transition
	char		modelname[64];		// maps/<name>.bsp, for model_precache[1]

	struct model_s	*worldmodel;
	struct model_s	*models[MAX_MODELS];

	sizebuf_t	datagram;
	byte		datagram_buf[MAX_DATAGRAM];

	sizebuf_t	reliable_datagram;	// copied to all clients at end of frame
	byte		reliable_datagram_buf[MAX_DATAGRAM];

	sizebuf_t	signon;
	byte		signon_buf[MAX_SIGNON];

	int			num_edicts;
	int			max_edicts;
	edict_t		*edicts;			// can NOT be array indexed, because
									// edict_t is variable sized, but can
									// be used to reference the world ent

	int			areanode_count;
	void		*areanode_data;

	char		*sound_precache[MAX_SOUNDS];	// NULL terminated
	char		*model_precache[MAX_MODELS];	// NULL terminated
	char		*lightstyles[MAX_LIGHTSTYLES];
} server_t;

//============================================================================

extern server_t			sv;					// local server
extern server_static_t	svs;				// persistant server info

// SV_StartSound channels and attenuation
#define CHAN_AUTO		0
#define CHAN_WEAPON		1
#define CHAN_VOICE		2
#define CHAN_ITEM		3
#define CHAN_BODY		4

#define ATTN_NORM		1

extern cvar_t	sv_maxvelocity;
extern cvar_t	sv_gravity;
extern cvar_t	sv_friction;
extern cvar_t	sv_edgefriction;
extern cvar_t	sv_stopspeed;
extern cvar_t	sv_maxspeed;
extern cvar_t	sv_accelerate;
extern cvar_t	sv_idealpitchscale;
extern cvar_t	sv_aim;
extern cvar_t	sv_nostep;

//============================================================================

//
// sv_main.c
//
void SV_Init(void);
void SV_StartSound(edict_t *entity, int channel, const char *sample, int volume, float attenuation);
void SV_CheckForNewClients(void);
void SV_ClearDatagram(void);
void SV_WriteClientdataToMessage(edict_t *ent, sizebuf_t *msg);
void SV_SendClientMessages(void);
int SV_ModelIndex(char *name);
void SV_SaveSpawnparms(void);
void SV_SpawnServer(char *server, char *startspot);
void SV_ClientPrintf(char *fmt, ...);
void SV_ClientCommand(char *fmt, ...);
void SV_RunClients(void);
void SV_BroadcastPrintf(char *fmt, ...);
void SV_DropClient(qboolean crash);

//
// sv_phys.c
//
void SV_Physics(void);
trace_t *SV_Trace_Toss(trace_t *result, edict_t *ent, edict_t *passent);

//
// sv_user.c
//
void SV_SetIdealPitch(void);
void SV_ClientThink(void);

//
// sv_move.c
//
qboolean SV_CheckBottom(edict_t *ent);
qboolean SV_movestep(edict_t *ent, vec3_t move, qboolean relink);
void SV_ChangeYaw(edict_t *ent);
void SV_ChangePitch(edict_t *ent);
void PF_changeyaw(void);
int SV_MoveToGoal_step(edict_t *ent, float dist);
float SV_MoveToGoal_internal(edict_t *self, float *goal, float dist, int chase);
int PF_MoveToGoal(void);

//
// sv_client.c
//
int SV_EntityChangeLevelCallback(const char *level, const char *startspot);
int SV_EntityFirstChangeLevelCallback(void);

#endif // SERVER_H
