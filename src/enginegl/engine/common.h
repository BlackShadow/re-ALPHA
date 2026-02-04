#ifndef COMMON_H
#define COMMON_H

 // =========================================================
 // Basic types
 // =========================================================

#if !defined BYTE_DEFINED
typedef unsigned char   byte;
#define BYTE_DEFINED 1
#endif

 // Fixed-size integer types (used throughout engine)
typedef unsigned char		u8;
typedef signed char			s8;
typedef unsigned short		u16;
typedef signed short		s16;
typedef unsigned int		u32;
typedef signed int			s32;
typedef unsigned __int64	u64;
typedef signed __int64		s64;

#undef true
#undef false
typedef enum { false, true } qboolean;

 // =========================================================
 // Constants
 // =========================================================

#define MAX_QPATH       64
#define MAX_OSPATH      260
#define MAX_MSGLEN      8000
#define MAX_DATAGRAM    1024
#define MAX_SIGNON      8192
#define MAX_FILES_IN_PACK 4096
#define PAK0_COUNT      339
#define PAK0_CRC        0x80D5


#ifndef NULL
#define NULL ((void *)0)
#endif

 // =========================================================
 // Size buffer
 // =========================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;
	qboolean	overflowed;
	byte		*data;
	int			maxsize;
	int			cursize;
} sizebuf_t;

typedef struct savebuf_s
{
    void    *pBuffer; // Base pointer
    byte    *curpos; // Current write position
    int     cursize; // Current size used
} savebuf_t;

void SZ_Alloc(sizebuf_t *buf, int startsize);
void SZ_Free(sizebuf_t *buf);
void SZ_Clear(sizebuf_t *buf);
byte *SZ_GetSpace(sizebuf_t *buf, int length);
int SZ_Write(sizebuf_t *buf, const void *data, int length);
void SZ_Print(sizebuf_t *buf, char *data);

 // =========================================================
 // Linked list
 // =========================================================

typedef struct link_s
{
	struct link_s   *prev, *next;
} link_t;

void ClearLink(link_t *l);
void RemoveLink(link_t *l);
void InsertLinkBefore(link_t *l, link_t *before);
void InsertLinkAfter(link_t *l, link_t *after);

#define STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (int)&(((t *)0)->m)))

 // =========================================================
 // Endianness and Message I/ O
 // =========================================================

extern short (*LittleShort)(short l);
extern int (*LittleLong)(int l);
extern float (*LittleFloat)(float l);
extern int (*BigLong)(int l);

extern int msg_readcount;
extern qboolean msg_badread;

extern char com_cmdline[256];
extern char com_serveractive[256];

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

byte *MSG_WriteChar(sizebuf_t *sb, int c);
byte *MSG_WriteByte(sizebuf_t *sb, int c);
short *MSG_WriteShort(sizebuf_t *sb, int c);
int *MSG_WriteLong(sizebuf_t *sb, int c);
int MSG_WriteFloat(sizebuf_t *sb, float f);
int MSG_WriteString(sizebuf_t *sb, const char *s);
short *MSG_WriteCoord(sizebuf_t *sb, float f);
byte *MSG_WriteAngle(sizebuf_t *sb, float f);

 // =========================================================
 // Utilities
 // =========================================================

int Q_strlen(const char *str);
char *Q_strcpy(char *dest, const char *src);
char *Q_strncpy(char *dest, const char *src, int n);
int Q_strcmp(const char *s1, const char *s2);
int Q_strncmp(const char *s1, const char *s2, int n);
int Q_stricmp(const char *s1, const char *s2);
char *Q_strrchr(const char *s, char c);
char *Q_strcat(char *dest, const char *src);
char *Q_strstr(const char *str, const char *substr);
int Q_atoi(const char *str);
double Q_atof(const char *str);
int Q_strcasecmp(const char *s1, const char *s2);
int Q_strncasecmp(const char *s1, const char *s2, int n);

char *va(char *format, ...);

int Q_memset(void *dest, int fill, int count);
int Q_memcpy(void *dest, const void *src, int count);
int Q_memcmp(const void *m1, const void *m2, int count);

 // =========================================================
 // File system
 // =========================================================

void COM_DefaultExtension(char *path, const char *extension);
void COM_FileBase(const char *in, char *out);
void COM_StripExtension(const char *in, char *out);
char *COM_Parse(char *data);
extern char com_token[1024];

void COM_Init(void);
int COM_OpenFile(const char *filename, int *handle);
void COM_CloseFile(int h);
int COM_FOpenFile(const char *filename, FILE **file);

byte *COM_LoadFile(const char *path, int usehunk);
void COM_WriteFile(const char *filename, void *data, int len);

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef dpackfile_t packfile_t;

typedef struct pack_s
{
	char	filename[MAX_QPATH];
	int		handle;
	int		numfiles;
	dpackfile_t *files;
} pack_t;

typedef struct searchpath_s
{
	char	filename[MAX_QPATH];
	pack_t	*pack;
	struct searchpath_s *next;
} searchpath_t;

extern int		com_argc;
extern char **com_argv;
int COM_CheckParm(const char *parm);

 // File loading helpers (allocation mode via Hunk/ Zone/ Cache/ Stack)
byte *COM_LoadHunkFile(const char *path);
byte *COM_LoadCacheFile(const char *path, struct cache_user_s *cu);
byte *COM_LoadStackFile(const char *path, void *buffer, int bufsize);

extern int com_filesize;

extern qboolean com_modified;
extern char com_gamedir[MAX_OSPATH];

 // Mod/ mission pack flags (set from command line)
extern qboolean rogue;
extern qboolean hipnotic;

#endif // COMMON_H
