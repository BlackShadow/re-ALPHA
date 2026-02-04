#ifndef HOST_H
#define HOST_H

 // Savegame format version (see host.c)
#define SAVEGAME_VERSION 16

 // Note: savebuf_t is defined in common.h (included via quakedef.h)

 // =========================================================
 // Host Functions
 // =========================================================

void Host_Init(quakeparms_t *parms);
void Host_Frame(float time);
void Host_ShutdownServer(qboolean crash);
void Host_Error(const char *error, ...);
void Host_EndGame(const char *message, ...);
void Host_InitCommands(void);
void Host_FindMaxClients(void);
int Host_ClearMemory(void);

 // =========================================================
 // Host cvars (defined in host.c)
 // =========================================================

extern cvar_t host_framerate;
extern cvar_t host_speeds;
extern cvar_t sys_ticrate;
extern cvar_t serverprofile;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t teamplay;
extern cvar_t samelevel;
extern cvar_t noexit;
extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t pausable;
extern cvar_t developer;

 // Savegame operations
void Savegame_WriteInt(savebuf_t *sb, int value);
void Savegame_WriteInt2(savebuf_t *sb, int value);
void Savegame_WriteString(savebuf_t *sb, const char *str);

void ED_Write(savebuf_t *sb, edict_t *ed);
void ED_WriteGlobals(savebuf_t *sb);

 // Host command handlers
void Host_Status_f(void);
void Host_Quit_f(void);
void Host_God_f(void);
void Host_Notarget_f(void);
void Host_Fly_f(void);
void Host_Map_f(void);
void Host_Restart_f(void);
void Host_Changelevel_f(void);
void Host_Changelevel2_f(void);
void Host_Connect_f(void);
void Host_Reconnect_f(void);
void Host_Name_f(void);
void Host_Noclip_f(void);
void Host_Version_f(void);
void Host_Say_f(void);
void Host_Say_Team_f(void);
void Host_Tell_f(void);
void Host_Color_f(void);
void Host_Kill_f(void);
void Host_Pause_f(void);
void Host_Spawn_f(void);
void Host_Begin_f(void);
void Host_PreSpawn_f(void);
void Host_Kick_f(void);
void Host_Ping_f(void);
void Host_Loadgame_f(void);
void Host_Savegame_f(void);
void Host_Startdemos_f(void);
void Host_Demos_f(void);
void Host_Stopdemo_f(void);
void Host_Viewmodel_f(void);
void Host_Viewframe_f(void);
void Host_Viewnext_f(void);
void Host_Viewprev_f(void);
void Host_Interp_f(void);

 // Other host utilities
void Host_BuildSaveComment(byte *comment);
qboolean Host_CanSave(void);
char *Host_ConvertPathSlashes(char *path);

#endif // HOST_H
