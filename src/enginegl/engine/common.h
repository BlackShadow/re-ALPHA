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

// common.h -- general definitions

#ifndef COMMON_H
#define COMMON_H

#if !defined BYTE_DEFINED
typedef unsigned char	byte;
#define BYTE_DEFINED 1
#endif

#undef true
#undef false

typedef enum {false, true} qboolean;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define MAX_QPATH		64		// max length of a quake game pathname
#define MAX_OSPATH		260		// max length of a filesystem pathname

#define MAX_MSGLEN		8000	// max length of a reliable message
#define MAX_DATAGRAM	1024	// max length of unreliable message
#define MAX_SIGNON		8192	// max length of the signon message

//============================================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Sys_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte		*data;
	int			maxsize;
	int			cursize;
} sizebuf_t;

void SZ_Alloc(sizebuf_t *buf, int startsize);
void SZ_Free(sizebuf_t *buf);
void SZ_Clear(sizebuf_t *buf);
byte *SZ_GetSpace(sizebuf_t *buf, int length);
int SZ_Write(sizebuf_t *buf, const void *data, int length);
void SZ_Print(sizebuf_t *buf, char *data);	// strcats onto the sizebuf

// savegame output buffer (Savegame_Write* in host_cmd.c)
typedef struct savebuf_s
{
	void	*pBuffer;
	byte	*curpos;
	int		cursize;
} savebuf_t;

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

void ClearLink(link_t *l);
void RemoveLink(link_t *l);
void InsertLinkBefore(link_t *l, link_t *before);
void InsertLinkAfter(link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (int)&(((t *)0)->m)))

//============================================================================

extern short (*LittleShort)(short l);
extern int (*BigLong)(int l);
extern int (*LittleLong)(int l);
extern float (*LittleFloat)(float l);

//============================================================================

byte *MSG_WriteChar(sizebuf_t *sb, int c);
byte *MSG_WriteByte(sizebuf_t *sb, int c);
short *MSG_WriteShort(sizebuf_t *sb, int c);
int *MSG_WriteLong(sizebuf_t *sb, int c);
int MSG_WriteFloat(sizebuf_t *sb, float f);
int MSG_WriteString(sizebuf_t *sb, const char *s);
short *MSG_WriteCoord(sizebuf_t *sb, float f);
byte *MSG_WriteAngle(sizebuf_t *sb, float f);

extern int msg_readcount;
extern qboolean msg_badread;		// set if a read goes beyond end of message

void MSG_BeginReading(void);
int MSG_ReadChar(void);
int MSG_ReadByte(void);
int MSG_ReadShort(void);
int MSG_ReadUShort(void);
int MSG_ReadLong(void);
float MSG_ReadFloat(void);
double MSG_ReadTime(void);
char *MSG_ReadString(void);
float MSG_ReadCoord(void);
float MSG_ReadAngle(void);

//============================================================================

void Q_memset(void *dest, int fill, int count);
int Q_memcpy(void *dest, const void *src, int count);

char *Q_strcpy(char *dest, const char *src);
char *Q_strncpy(char *dest, const char *src, int count);
int Q_strlen(const char *str);
char *Q_strrchr(const char *s, char c);
char *Q_strcat(char *dest, const char *src);
char *Q_strstr(const char *str, const char *substr);
int Q_strcmp(const char *s1, const char *s2);
int Q_strncmp(const char *s1, const char *s2, int count);
int Q_strcasecmp(const char *s1, const char *s2);
int Q_strncasecmp(const char *s1, const char *s2, int n);
int Q_atoi(const char *str);
double Q_atof(const char *str);

//============================================================================

#define CMDLINE_LENGTH	256

extern char com_token[1024];
extern char com_cmdline[CMDLINE_LENGTH];
extern char com_serveractive[CMDLINE_LENGTH];	// map arguments after the map name

extern int com_argc;
extern char **com_argv;

int COM_CheckParm(const char *parm);
void COM_Init(void);
void COM_InitArgv(int argc, char **argv);

char *COM_Parse(char *data);

void COM_StripExtension(const char *in, char *out);
void COM_FileBase(const char *in, char *out);
void COM_DefaultExtension(char *path, const char *extension);

char *va(char *format, ...);
// does a varargs printf into a temp buffer

//============================================================================

#define MAX_FILES_IN_PACK	4096

// the expected pak0.pak directory; anything else sets com_modified
#define PAK0_COUNT		339
#define PAK0_CRC		0x80D5

//
// on disk
//
typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

//
// in memory
//
typedef dpackfile_t packfile_t;

typedef struct pack_s
{
	char		filename[MAX_QPATH];
	int			handle;
	int			numfiles;
	packfile_t	*files;
} pack_t;

typedef struct searchpath_s
{
	char				filename[MAX_QPATH];
	pack_t				*pack;		// only one of filename / pack will be used
	struct searchpath_s	*next;
} searchpath_t;

extern int com_filesize;
struct cache_user_s;

extern char com_gamedir[MAX_OSPATH];
extern qboolean com_modified;

void COM_WriteFile(const char *filename, void *data, int len);
int COM_OpenFile(const char *filename, int *handle);
int COM_FOpenFile(const char *filename, FILE **file);
void COM_CloseFile(int h);

// COM_LoadFile usehunk values
#define LOADFILE_ZONE		0
#define LOADFILE_HUNK		1
#define LOADFILE_TEMPHUNK	2
#define LOADFILE_CACHE		3
#define LOADFILE_STACK		4

byte *COM_LoadFile(const char *path, int usehunk);
byte *COM_LoadHunkFile(const char *path);
byte *COM_LoadCacheFile(const char *path, struct cache_user_s *cu);
byte *COM_LoadStackFile(const char *path, void *buffer, int bufsize);

extern qboolean rogue;
extern qboolean hipnotic;

#endif // COMMON_H
