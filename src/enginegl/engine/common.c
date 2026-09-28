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

// common.c -- misc functions used in client and server

#include "quakedef.h"
#include "crc.h"

#define MAX_NUM_ARGVS	50

void COM_InitFilesystem(void);

char com_token[1024];

static char *com_argv_storage[MAX_NUM_ARGVS];
int com_argc;
char **com_argv = com_argv_storage;
char com_cmdline[CMDLINE_LENGTH];
char com_serveractive[CMDLINE_LENGTH];

char com_gamedir[MAX_OSPATH];
qboolean com_modified;		// set true if using non-id files
static searchpath_t *com_searchpaths;

int com_filesize;
static int static_registered = 1;
qboolean rogue;
qboolean hipnotic;
static qboolean proghack;

static char com_cachedir[MAX_OSPATH];
static cvar_t registered = {"registered", "0"};
static cvar_t cmdline = {"cmdline", "0"};

// COM_LoadCacheFile / COM_LoadStackFile arguments for COM_LoadFile
static cache_user_t *loadcache;
static void *loadbuf;
static int loadsize;

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

static short (*BigShort)(short l);
short (*LittleShort)(short l);
int (*BigLong)(int l);
int (*LittleLong)(int l);
static float (*BigFloat)(float l);
float (*LittleFloat)(float l);

short ShortSwap(short l)
{
	byte b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

short ShortNoSwap(short l)
{
	return l;
}

int LongSwap(int l)
{
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int LongNoSwap(int l)
{
	return l;
}

float FloatSwap(float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap(float f)
{
	return f;
}

/*
============================================================================

							LINKED LIST

============================================================================
*/

// ClearLink is used for new headnodes
void ClearLink(link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink(link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore(link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter(link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

void Q_memset(void *dest, int fill, int count)
{
	int i;
	int n;
	int fill32;

	if (((int)dest | count) & 3)
	{
		// unaligned: whole dwords first, then the remaining bytes
		if (count > 0)
		{
			fill32 = (byte)fill;
			fill32 |= fill32 << 8;
			fill32 |= fill32 << 16;

			n = (unsigned int)count >> 2;
			for (i = 0; i < n; i++)
				((int *)dest)[i] = fill32;

			memset((byte *)dest + 4 * n, fill, count & 3);
		}
	}
	else
	{
		fill32 = fill | (fill << 8) | (fill << 16) | (fill << 24);
		n = count >> 2;
		for (i = 0; i < n; i++)
			((int *)dest)[i] = fill32;
	}
}

int Q_atoi(const char *str)
{
	int val;
	int sign;
	int c;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val * sign;
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}

	//
	// assume decimal
	//
	while (1)
	{
		c = *str++;
		if (c < '0' || c > '9')
			return val * sign;
		val = val * 10 + c - '0';
	}
}

int Q_memcpy(void *dest, const void *src, int count)
{
	int i;
	int result;

	result = (int)dest | (int)src | count;

	if (result & 3)
	{
		if (count > 0)
		{
			memcpy(dest, src, count);
			return count;
		}
	}
	else if ((count >> 2) > 0)
	{
		for (i = 0; i < (count >> 2); i++)
			((int *)dest)[i] = ((int *)src)[i];
	}

	return result;
}

char *Q_strcpy(char *dest, const char *src)
{
	char *p = dest;

	while (*src)
		*p++ = *src++;
	*p = 0;

	return dest;
}

char *Q_strncpy(char *dest, const char *src, int count)
{
	char *p = dest;

	while (*src && count--)
		*p++ = *src++;
	if (count > 0)
		*p = 0;

	return dest;
}

int Q_strlen(const char *str)
{
	int count;

	count = 0;
	if (*str)
	{
		while (str[count])
			count++;
	}

	return count;
}

char *Q_strrchr(const char *s, char c)
{
	int len = Q_strlen(s);

	s += len;
	while (len--)
	{
		if (*--s == c)
			return (char *)s;
	}

	return NULL;
}

char *Q_strcat(char *dest, const char *src)
{
	dest += Q_strlen(dest);
	return Q_strcpy(dest, src);
}

char *Q_strstr(const char *str, const char *substr)
{
	return strstr(str, substr);
}

int Q_strcmp(const char *s1, const char *s2)
{
	while (1)
	{
		if (*s1 != *s2)
			return -1;		// strings not equal
		if (!*s1)
			return 0;		// strings are equal
		s1++;
		s2++;
	}
}

int Q_strncmp(const char *s1, const char *s2, int count)
{
	while (count--)
	{
		if (*s1 != *s2)
			return -1;		// strings not equal
		if (!*s1)
			return 0;		// strings are equal
		s1++;
		s2++;
	}

	return 0;
}

int Q_strncasecmp(const char *s1, const char *s2, int n)
{
	int c1, c2;

	while (1)
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;	// strings not equal
		}

		if (!c1)
			return 0;		// strings are equal
	}
}

int Q_strcasecmp(const char *s1, const char *s2)
{
	return Q_strncasecmp(s1, s2, 99999);
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

byte *MSG_WriteChar(sizebuf_t *sb, int c)
{
	byte *buf;

	buf = SZ_GetSpace(sb, 1);
	buf[0] = c;
	return buf;
}

byte *MSG_WriteByte(sizebuf_t *sb, int c)
{
	byte *buf;

	buf = SZ_GetSpace(sb, 1);
	buf[0] = c;
	return buf;
}

short *MSG_WriteShort(sizebuf_t *sb, int c)
{
	short *buf;

	buf = (short *)SZ_GetSpace(sb, 2);
	buf[0] = c;
	return buf;
}

short *MSG_WriteShort2(sizebuf_t *sb, int c)
{
	short *buf;

	buf = (short *)SZ_GetSpace(sb, 2);
	buf[0] = c;
	return buf;
}

int *MSG_WriteLong(sizebuf_t *sb, int c)
{
	int *buf;

	buf = (int *)SZ_GetSpace(sb, 4);
	buf[0] = c;
	return buf;
}

int MSG_WriteFloat(sizebuf_t *sb, float f)
{
	union
	{
		float	f;
		int		l;
	} dat;

	dat.f = f;
	dat.l = LittleLong(dat.l);

	return SZ_Write(sb, &dat.l, 4);
}

int MSG_WriteString(sizebuf_t *sb, const char *s)
{
	if (!s)
		return SZ_Write(sb, "", 1);
	else
		return SZ_Write(sb, s, Q_strlen(s) + 1);
}

short *MSG_WriteCoord(sizebuf_t *sb, float f)
{
	return MSG_WriteShort(sb, (int)(f * 8.0));
}

byte *MSG_WriteAngle(sizebuf_t *sb, float f)
{
	return MSG_WriteByte(sb, ((int)f * 256) / 360);
}

//
// reading functions
//
int msg_readcount;
qboolean msg_badread;

void MSG_BeginReading(void)
{
	msg_readcount = 0;
	msg_badread = false;
}

void MSG_BadRead_Null(void)
{
}

void MSG_ReadByte_Null(void)
{
}

void MSG_ReadShort_Null(void)
{
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar(void)
{
	int c;

	if (net_message.cursize >= msg_readcount + 1)
	{
		c = (signed char)net_message.data[msg_readcount];
		msg_readcount++;
		return c;
	}

	msg_badread = true;
	return -1;
}

int MSG_ReadByte(void)
{
	int c;

	if (net_message.cursize >= msg_readcount + 1)
	{
		c = (unsigned char)net_message.data[msg_readcount];
		msg_readcount++;
		return c;
	}

	msg_badread = true;
	return -1;
}

int MSG_ReadShort(void)
{
	int c;

	if (net_message.cursize >= msg_readcount + 2)
	{
		c = (short)(net_message.data[msg_readcount] + (net_message.data[msg_readcount + 1] << 8));
		msg_readcount += 2;
		return c;
	}

	msg_badread = true;
	return -1;
}

int MSG_ReadUShort(void)
{
	byte *p;

	if (net_message.cursize >= msg_readcount + 2)
	{
		p = &net_message.data[msg_readcount];
		msg_readcount += 2;
		return (p[1] << 8) + p[0];
	}

	msg_badread = true;
	return -1;
}

int MSG_ReadLong(void)
{
	int c;

	if (net_message.cursize >= msg_readcount + 4)
	{
		c = net_message.data[msg_readcount]
			+ (net_message.data[msg_readcount + 1] << 8)
			+ (net_message.data[msg_readcount + 2] << 16)
			+ (net_message.data[msg_readcount + 3] << 24);
		msg_readcount += 4;
		return c;
	}

	msg_badread = true;
	return -1;
}

float MSG_ReadFloat(void)
{
	union
	{
		int		l;
		float	f;
	} dat;

	memcpy(&dat.l, net_message.data + msg_readcount, sizeof(dat.l));
	msg_readcount += 4;
	dat.l = LittleLong(dat.l);

	return dat.f;
}

double MSG_ReadTime(void)
{
	return MSG_ReadFloat();
}

char *MSG_ReadString(void)
{
	static char string[2048];
	int l, c;

	l = 0;
	do
	{
		c = MSG_ReadChar();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < (int)sizeof(string) - 1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord(void)
{
	return MSG_ReadShort() * (1.0 / 8);
}

float MSG_ReadAngle(void)
{
	return MSG_ReadChar() * (360.0 / 256);
}

//===========================================================================

void SZ_Alloc(sizebuf_t *buf, int startsize)
{
	if (startsize < 256)
		startsize = 256;
	buf->data = Hunk_AllocName(startsize, "sizebuf");
	buf->maxsize = startsize;
	buf->cursize = 0;
}

void SZ_Free(sizebuf_t *buf)
{
	buf->cursize = 0;
}

void SZ_Clear(sizebuf_t *buf)
{
	buf->cursize = 0;
}

byte *SZ_GetSpace(sizebuf_t *buf, int length)
{
	byte *data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error("SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Sys_Error("SZ_GetSpace: %i is > full buffer size", length);

		buf->overflowed = true;
		Con_Printf("SZ_GetSpace: overflow");
		SZ_Clear(buf);
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

int SZ_Write(sizebuf_t *buf, const void *data, int length)
{
	return Q_memcpy(SZ_GetSpace(buf, length), data, length);
}

void SZ_Print(sizebuf_t *buf, char *data)
{
	int len;

	len = Q_strlen(data) + 1;

	if (buf->data[buf->cursize - 1])
		Q_memcpy(SZ_GetSpace(buf, len), data, len);				// no trailing 0
	else
		Q_memcpy(SZ_GetSpace(buf, len - 1) - 1, data, len);		// write over trailing 0
}

//============================================================================

/*
============
COM_StripExtension
============
*/
void COM_StripExtension(const char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

void COM_StripExtension_Null(void)
{
}

/*
============
COM_FileExtension
============
*/
static char *COM_FileExtension(char *in)
{
	static char exten[8];
	int i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i = 0; i < 7 && *in; i++, in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

void COM_NullSub(void)
{
}

short NullSub_ReturnShort(short value)
{
	return value;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase(const char *in, char *out)
{
	int len, start, end;

	len = Q_strlen(in);

	// scan backward for '.'
	end = len - 1;
	while (end && in[end] != '.' && in[end] != '/')
		end--;

	if (in[end] != '.')		// no '.', copy to end
		end = len - 1;
	else
		end--;				// found ',', copy to left of '.'

	// scan backward for '/'
	start = len - 1;
	while (start >= 0 && in[start] != '/')
		start--;

	if (in[start] != '/')
		start = 0;
	else
		start++;

	// copy partial string
	len = end - start + 1;
	Q_memcpy(out, &in[start], len);
	out[len] = 0;
}

/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension(char *path, const char *extension)
{
	char *src;

	//
	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	//
	src = path + Q_strlen(path) - 1;

	while (src != path && *src != '/')
	{
		if (*src == '.')
			return;		// it has an extension
		src--;
	}

	Q_strcat(path, extension);
}

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse(char *data)
{
	int c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

	// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;	// end of file
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
			break;
	} while (c > ' ');

	com_token[len] = 0;
	return data;
}

/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm(const char *parm)
{
	int i;

	for (i = 1; i < com_argc; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP sometimes clears appkit vars.
		if (!Q_strcmp(parm, com_argv[i]))
			return i;
	}

	return 0;
}

/*
================
COM_InitArgv
================
*/
void COM_InitArgv(int argc, char **argv)
{
	static const char *argvdummy = " ";
	static const char *const safeargvs[] =
		{"-stdvid", "-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse", "-dibonly"};
	qboolean safe;
	int i, n;

	safe = false;

	// reconstitute the command line for the cmdline externally visible cvar
	n = 0;
	for (i = 0; i < argc && i < MAX_NUM_ARGVS; i++)
	{
		const char *p;

		if (n >= CMDLINE_LENGTH - 1)
			break;

		p = argv[i];
		while (*p && n < CMDLINE_LENGTH - 1)
			com_cmdline[n++] = *p++;

		if (n >= CMDLINE_LENGTH - 1)
			break;

		com_cmdline[n++] = ' ';
	}
	com_cmdline[n] = 0;

	for (i = 0; i < argc && i < MAX_NUM_ARGVS; i++)
	{
		com_argv[i] = argv[i];
		if (!Q_strcmp("-safe", argv[i]))
			safe = true;
	}
	com_argc = i;

	if (safe)
	{
		// force all the safe-mode switches. Note that we reserved extra space in
		// case we need to add these, so we don't need an overflow check
		for (i = 0; i < (int)(sizeof(safeargvs) / sizeof(safeargvs[0])) && com_argc < MAX_NUM_ARGVS; i++)
			com_argv[com_argc++] = (char *)safeargvs[i];
	}

	if (com_argc < MAX_NUM_ARGVS)
		com_argv[com_argc] = (char *)argvdummy;

	if (COM_CheckParm("-rogue"))
	{
		rogue = true;
		proghack = true;
	}

	if (COM_CheckParm("-hipnotic"))
	{
		hipnotic = true;
		proghack = true;
	}
}

/*
============
COM_Path_f
============
*/
static void COM_Path_f(void)
{
	searchpath_t *s;

	Con_Printf("Current search path:\n");
	for (s = com_searchpaths; s; s = s->next)
	{
		if (s->pack)
			Con_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Con_Printf("%s\n", s->filename);
	}
}

/*
================
COM_Init
================
*/
void COM_Init(void)
{
	byte swaptest[2] = {1, 0};

	// set the byte swapping variables in a portable manner
	if (*(short *)swaptest == 1)
	{
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	Cvar_RegisterVariable(&registered);
	Cvar_RegisterVariable(&cmdline);
	Cmd_AddCommand("path", COM_Path_f);

	COM_InitFilesystem();
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *va(char *format, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, format);
	vsprintf(string, format, argptr);
	va_end(argptr);

	return string;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile(const char *filename, void *data, int len)
{
	int handle;
	char name[MAX_OSPATH];

	sprintf(name, "%s/%s", com_gamedir, filename);

	handle = Sys_FileOpenWrite(name);
	if (handle == -1)
	{
		Sys_Printf("COM_WriteFile: failed on %s\n", name);
		return;
	}

	Sys_Printf("COM_WriteFile: %s\n", name);
	Sys_FileWrite(handle, data, len);
	Sys_FileClose(handle);
}

/*
============
COM_CreatePath

Only used for CopyFile
============
*/
static void COM_CreatePath(char *path)
{
	char *ofs;

	for (ofs = path + 1; *ofs; ofs++)
	{
		if (*ofs == '/')
		{
			// create the directory
			*ofs = 0;
			Sys_mkdir(path);
			*ofs = '/';
		}
	}
}

/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
static void COM_CopyFile(char *netpath, char *cachepath)
{
	int in, out;
	int remaining, count;
	char buf[4096];

	remaining = Sys_FileOpenRead(netpath, &in);
	COM_CreatePath(cachepath);	// create directories up to the cache file
	out = Sys_FileOpenWrite(cachepath);

	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		Sys_FileRead(in, buf, count);
		Sys_FileWrite(out, buf, count);
		remaining -= count;
	}

	Sys_FileClose(in);
	Sys_FileClose(out);
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
static int COM_FindFile(const char *filename, int *handle, FILE **file)
{
	searchpath_t *search;
	char netpath[MAX_OSPATH];
	char cachepath[MAX_OSPATH];
	pack_t *pak;
	int i;
	int findtime, cachetime;
	int h;

	if (file && handle)
		Sys_Error("COM_FindFile: both handle and file set");
	if (!file && !handle)
		Sys_Error("COM_FindFile: neither handle or file set");

	//
	// search through the path, one element at a time
	//
	for (search = com_searchpaths; search; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
			{
				if (!Q_strcmp(pak->files[i].name, filename))
				{
					// found it!
					Sys_Printf("PackFile: %s : %s\n", pak->filename, filename);
					if (handle)
					{
						*handle = pak->handle;
						Sys_FileSeek(pak->handle, pak->files[i].filepos);
					}
					else
					{
						// open a new file on the pakfile
						*file = fopen(pak->filename, "rb");
						if (*file)
							fseek(*file, pak->files[i].filepos, SEEK_SET);
					}
					com_filesize = pak->files[i].filelen;
					return com_filesize;
				}
			}
		}
		else
		{
			// check a file in the directory tree
			if (!static_registered)
			{
				// if not a registered version, don't ever go beyond base
				if (strchr(filename, '/') || strchr(filename, '\\'))
					continue;
			}

			sprintf(netpath, "%s/%s", search->filename, filename);

			findtime = Sys_FileTime(netpath);
			if (findtime == -1)
				continue;

			// see if the file needs to be updated in the cache
			if (com_cachedir[0])
			{
				if (netpath[1] == ':')
					sprintf(cachepath, "%s%s", com_cachedir, netpath + 2);
				else
					sprintf(cachepath, "%s%s", com_cachedir, netpath);

				cachetime = Sys_FileTime(cachepath);

				if (cachetime < findtime)
					COM_CopyFile(netpath, cachepath);
				Q_strcpy(netpath, cachepath);
			}

			Sys_Printf("FindFile: %s\n", netpath);
			com_filesize = Sys_FileOpenRead(netpath, &h);
			if (handle)
				*handle = h;
			else
				Sys_FileClose(h);

			if (file)
				*file = fopen(netpath, "rb");

			return com_filesize;
		}
	}

	Sys_Printf("FindFile: can't find %s\n", filename);

	if (handle)
		*handle = -1;
	else
		*file = NULL;
	com_filesize = -1;
	return -1;
}

/*
===========
COM_OpenFile

filename never has a leading slash, but may contain directory walks
returns a handle and a length
it may actually be inside a pak file
===========
*/
int COM_OpenFile(const char *filename, int *handle)
{
	return COM_FindFile(filename, handle, NULL);
}

/*
===========
COM_FOpenFile

If the requested file is inside a packfile, a new FILE * will be opened
into the file.
===========
*/
int COM_FOpenFile(const char *filename, FILE **file)
{
	return COM_FindFile(filename, NULL, file);
}

/*
============
COM_CloseFile

If it is a pak file handle, don't really close it
============
*/
void COM_CloseFile(int h)
{
	searchpath_t *s;

	for (s = com_searchpaths; s; s = s->next)
	{
		if (s->pack && s->pack->handle == h)
			return;
	}

	Sys_FileClose(h);
}

/*
============
COM_LoadFile

Filename are relative to the quake directory.
Allways appends a 0 byte.
============
*/
byte *COM_LoadFile(const char *path, int usehunk)
{
	int h;
	byte *buf;
	char base[32];
	int len;

	buf = NULL;	// quiet compiler warning

	// look for it in the filesystem or pack files
	len = COM_OpenFile(path, &h);
	if (h == -1)
		return NULL;

	// extract the filename base name for hunk tag
	COM_FileBase(path, base);

	if (usehunk == LOADFILE_HUNK)
		buf = Hunk_AllocName(len + 1, base);
	else if (usehunk == LOADFILE_TEMPHUNK)
		buf = Hunk_TempAlloc(len + 1);
	else if (usehunk == LOADFILE_ZONE)
		buf = Z_Malloc(len + 1);
	else if (usehunk == LOADFILE_CACHE)
		buf = Cache_Alloc(loadcache, len + 1, base);
	else if (usehunk == LOADFILE_STACK)
	{
		if (len + 1 > loadsize)
			buf = Hunk_TempAlloc(len + 1);
		else
			buf = loadbuf;
	}
	else
		Sys_Error("COM_LoadFile: bad usehunk");

	if (!buf)
		Sys_Error("COM_LoadFile: not enough space for %s", path);

	buf[len] = 0;

	Draw_BeginDisc();
	Sys_FileRead(h, buf, len);
	COM_CloseFile(h);
	Draw_EndDisc();

	return buf;
}

byte *COM_LoadHunkFile(const char *path)
{
	return COM_LoadFile(path, LOADFILE_HUNK);
}

byte *COM_LoadCacheFile(const char *path, cache_user_t *cu)
{
	loadcache = cu;
	return COM_LoadFile(path, LOADFILE_CACHE);
}

// uses temp hunk if larger than bufsize
byte *COM_LoadStackFile(const char *path, void *buffer, int bufsize)
{
	loadbuf = buffer;
	loadsize = bufsize;
	return COM_LoadFile(path, LOADFILE_STACK);
}

/*
=================
COM_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static pack_t *COM_LoadPackFile(char *packfile)
{
	dpackheader_t header;
	int i;
	packfile_t *newfiles;
	int numpackfiles;
	pack_t *pack;
	int packhandle;
	unsigned short crc;

	if (Sys_FileOpenRead(packfile, &packhandle) == -1)
		return NULL;

	Sys_FileRead(packhandle, &header, sizeof(header));
	if (header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error("%s is not a packfile", packfile);
	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error("%s has %i files", packfile, numpackfiles);

	if (numpackfiles != PAK0_COUNT)
		com_modified = true;	// not the release pak0.pak

	newfiles = Hunk_AllocName(numpackfiles * sizeof(packfile_t), "packfile");

	Sys_FileSeek(packhandle, header.dirofs);
	Sys_FileRead(packhandle, newfiles, header.dirlen);

	// crc the directory to check for modifications
	CRC_Init(&crc);
	for (i = 0; i < header.dirlen; i++)
		CRC_ProcessByte(&crc, ((byte *)newfiles)[i]);
	if (crc != PAK0_CRC)
		com_modified = true;

	// parse the directory
	for (i = 0; i < numpackfiles; i++)
	{
		newfiles[i].filepos = LittleLong(newfiles[i].filepos);
		newfiles[i].filelen = LittleLong(newfiles[i].filelen);
	}

	pack = Hunk_Alloc(sizeof(pack_t));
	Q_strcpy(pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Con_Printf("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}

/*
================
COM_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
static void COM_AddGameDirectory(char *dir)
{
	int i;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];

	Q_strcpy(com_gamedir, dir);

	//
	// add the directory to the search path
	//
	search = Hunk_Alloc(sizeof(searchpath_t));
	Q_strcpy(search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	//
	for (i = 0; ; i++)
	{
		sprintf(pakfile, "%s/pak%i.pak", dir, i);
		pak = COM_LoadPackFile(pakfile);
		if (!pak)
			break;
		search = Hunk_Alloc(sizeof(searchpath_t));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}

/*
================
COM_InitFilesystem
================
*/
void COM_InitFilesystem(void)
{
	int i, j;
	char basedir[128];
	char *arg;
	searchpath_t *search;

	//
	// -basedir <path>
	// Overrides the system supplied base directory (under GAMENAME)
	//
	i = COM_CheckParm("-basedir");
	if (i && i < com_argc - 1)
		Q_strcpy(basedir, com_argv[i + 1]);
	else
		Q_strcpy(basedir, host_parms.basedir);

	j = Q_strlen(basedir);

	if (j > 0)
	{
		if ((basedir[j - 1] == '\\') || (basedir[j - 1] == '/'))
			basedir[j - 1] = 0;
	}

	//
	// -cachedir <path>
	// Overrides the system supplied cache directory (NULL or /qcache)
	// -cachedir - will disable caching.
	//
	i = COM_CheckParm("-cachedir");
	if (i && i < com_argc - 1)
	{
		arg = com_argv[i + 1];
		if (arg[0] == '-')
			com_cachedir[0] = 0;
		else
			Q_strcpy(com_cachedir, arg);
	}
	else if (host_parms.cachedir)
		Q_strcpy(com_cachedir, host_parms.cachedir);
	else
		com_cachedir[0] = 0;

	//
	// start up with valve by default
	//
	COM_AddGameDirectory(va("%s/valve", basedir));

	//
	// -game <gamedir>
	// Adds basedir/gamedir as an override game
	//
	i = COM_CheckParm("-game");
	if (i && i < com_argc - 1)
	{
		com_modified = true;
		COM_AddGameDirectory(va("%s/%s", basedir, com_argv[i + 1]));
	}

	//
	// -path <dir or packfile> [<dir or packfile>] ...
	// Fully specifies the exact search path, overriding the generated one
	//
	i = COM_CheckParm("-path");
	if (i)
	{
		com_modified = true;
		com_searchpaths = NULL;
		while (++i < com_argc)
		{
			arg = com_argv[i];
			if (!arg || arg[0] == '+' || arg[0] == '-')
				break;

			search = Hunk_Alloc(sizeof(searchpath_t));
			if (!Q_strcmp(COM_FileExtension(arg), "pak"))
			{
				search->pack = COM_LoadPackFile(arg);
				if (!search->pack)
					Sys_Error("Couldn't load packfile: %s", arg);
			}
			else
				Q_strcpy(search->filename, arg);
			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}

	if (COM_CheckParm("-proghack"))
		proghack = true;
}

/*
================
_fpreset

Masks all floating point exceptions.
================
*/
#define FPU_EXCEPTION_MASKS	0x3f	// control word bits that mask the six FPU exceptions

static unsigned int fpu_env[28];

void _fpreset(void)
{
	__asm { fnstenv [fpu_env] }

	fpu_env[0] |= FPU_EXCEPTION_MASKS;	// the control word comes first

	__asm { fldenv [fpu_env] }
}

/*
================
COM_PrintModelFrame
================
*/
void COM_PrintModelFrame(model_t *model, int frame)
{
	aliashdr_t *hdr;

	hdr = (aliashdr_t *)Mod_Extradata(model);
	if (hdr)
		Con_Printf("frame %i: %s\n", frame, hdr->frames[frame].name);
}

/*
================
Q_atof

The result is rounded to float precision
================
*/
double Q_atof(const char *str)
{
	double val;
	int sign;
	int c;
	int decimal, total;

	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;

	val = 0;

	//
	// check for hex
	//
	if (str[0] == '0' && ((c = str[1]) == 'x' || c == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val * 16) + c - ('a' - 10);
			else if (c >= 'A' && c <= 'F')
				val = (val * 16) + c - ('A' - 10);
			else
				return (float)(sign * val);
		}
	}

	//
	// check for character
	//
	if (str[0] == '\'')
		return (float)(sign * str[1]);

	//
	// assume decimal
	//
	decimal = -1;
	for (total = 0; ; total++)
	{
		while ((c = *str++) == '.')
			decimal = total;
		if (c < '0' || c > '9')
			break;
		val = (val * 10) + c - '0';
	}

	if (decimal == -1)
		return (float)(sign * val);

	if (total > decimal)
	{
		total -= decimal;
		do
		{
			val /= 10;
		} while (--total);
	}

	return (float)(sign * val);
}

/*
================
MSG_ReadCoords
================
*/
int MSG_ReadCoords(void)
{
	int x, y, z;

	x = MSG_ReadByte();
	y = MSG_ReadByte();
	z = MSG_ReadByte();
	MSG_ReadByte();

	return SetupSkyPolygonClipping(x, y, z);
}
