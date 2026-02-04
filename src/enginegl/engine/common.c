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
#include "crc.h"

void COM_InitFilesystem(void);

int msg_readcount;
qboolean msg_badread;
char com_token[1024];

static char *com_argv_storage[50];
int com_argc;
char **com_argv = com_argv_storage;
char com_cmdline[256];
char com_serveractive[256];

char com_gamedir[MAX_OSPATH];
qboolean com_modified;
static searchpath_t *com_searchpaths;

static short (*BigShort)(short l);
short (*LittleShort)(short l);
int (*BigLong)(int l);
int (*LittleLong)(int l);
static float (*BigFloat)(float l);
float (*LittleFloat)(float l);

int com_filesize;
static int static_registered = 1;
static int static_cmdline;
qboolean rogue;
qboolean hipnotic;
static qboolean proghack;

static char com_cachedir[MAX_OSPATH];
static cvar_t registered = {"registered", "0"};
static cvar_t cmdline = {"cmdline", "0"};
static cache_user_t *loadcache;
static char string_buffer[2048];
static char filename_buffer[128];

static void *loadbuf;
static int loadsize;
static int loadbuf_inuse;

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
		float f;
		byte b[4];
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

void ClearLink(link_t *l)
{
	l->next = l;
	l->prev = l;
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

int Q_memset(void *dest, int fill, int count)
{
	int fill32;
	int i;
	int dwordCount;
	int result;

	result = ((int)(unsigned short)(int)dest) | count;

	if ((((unsigned char)(int)dest | (unsigned char)count) & 3) != 0)
	{
		if (count > 0)
		{
			fill32 = (unsigned char)fill;
			fill32 |= fill32 << 8;
			fill32 |= fill32 << 16;

			dwordCount = (unsigned int)count >> 2;
			for (i = 0; i < dwordCount; ++i)
			{
				((int *)dest)[i] = fill32;
			}

			memset((byte *)dest + 4 * dwordCount, fill, count & 3);
			result = fill32;
		}
	}
	else
	{
		result = 0;
		fill32 = fill | (fill << 8) | (fill << 16) | (fill << 24);
		dwordCount = count >> 2;
		if (dwordCount > 0)
		{
			for (i = 0; i < dwordCount; ++i)
			{
				((int *)dest)[i] = fill32;
			}
			result = fill32;
		}
	}

	return result;
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

	if (str[0] == '\'')
	{
		return sign * str[1];
	}

	while (1)
	{
		c = *str++;
		if (c < '0' || c > '9')
			return val * sign;
		val = val * 10 + c - '0';
	}

	return 0;
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
		for (i = 0; i < (count >> 2); ++i)
		{
			((int *)dest)[i] = ((const int *)src)[i];
		}
	}

	return result;
}

char *Q_strcpy(char *dest, const char *src)
{
	char *p = dest;

	while (*src)
	{
		*p++ = *src++;
	}
	*p = 0;

	return dest;
}

char *Q_strncpy(char *dest, const char *src, int count)
{
	char *p = dest;

	while (*src && count--)
	{
		*p++ = *src++;
	}
	if (count > 0)
		*p = 0;

	return dest;
}

int Q_strlen(const char *str)
{
	int count = 0;

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
	const char *p = s + len;

	while (len--)
	{
		if (*--p == c)
			return (char *)p;
	}

	return NULL;
}

char *Q_strcat(char *dest, const char *src)
{
	int len = Q_strlen(dest);
	return Q_strcpy(dest + len, src);
}

char *Q_strstr(const char *str, const char *substr)
{
	return (char *)strstr(str, substr);
}

int Q_strcmp(const char *s1, const char *s2)
{
#ifdef _DEBUG
	if (!s1 || !s2)
	{
		extern char __ImageBase;

		void *returnAddress;
		FILE *log;
		const char *logPath;
		unsigned int imageBase;
		unsigned int retAddr;
		unsigned int rva;
		unsigned int idaAddr;

		returnAddress = _ReturnAddress();
		imageBase = (unsigned int)(size_t)&__ImageBase;
		retAddr = (unsigned int)(size_t)returnAddress;
		rva = retAddr - imageBase;
		idaAddr = 0x400000u + rva;

		logPath = getenv("QSTRCMP_LOG");
		if (!logPath || !*logPath)
			logPath = "qstrcmp_null.log";

		log = fopen(logPath, "a");
		if (log)
		{
			fprintf(log,
				"Q_strcmp NULL: s1=%p s2=%p ret=%p base=%08X ida=%08X\n",
				(const void *)s1,
				(const void *)s2,
				returnAddress,
				imageBase,
				idaAddr);
			fclose(log);
		}

		__debugbreak();
	}
#endif

	while (1)
	{
		if (*s1 != *s2)
			return -1;
		if (!*s1)
			return 0;
		s1++;
		s2++;
	}
}

int Q_strncmp(const char *s1, const char *s2, int count)
{
	while (count--)
	{
		if (*s1 != *s2)
			return -1;
		if (!*s1)
			return 0;
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
			return 0;

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;
		}

		if (!c1)
			return 0;
	}
}

int Q_strcasecmp(const char *s1, const char *s2)
{
	return Q_strncasecmp(s1, s2, 99999);
}

byte *MSG_WriteChar(sizebuf_t *sb, int c)
{
	byte *buf = SZ_GetSpace(sb, 1);
	*buf = (byte)c;
	return buf;
}

byte *MSG_WriteByte(sizebuf_t *sb, int c)
{
	byte *buf = SZ_GetSpace(sb, 1);
	*buf = (byte)c;
	return buf;
}

short *MSG_WriteShort(sizebuf_t *sb, int c)
{
	short *buf = (short *)SZ_GetSpace(sb, 2);
	*buf = (short)c;
	return buf;
}

short *MSG_WriteShort2(sizebuf_t *sb, int c)
{
	short *buf = (short *)SZ_GetSpace(sb, 2);
	*buf = (short)c;
	return buf;
}

int *MSG_WriteLong(sizebuf_t *sb, int c)
{
	int *buf = (int *)SZ_GetSpace(sb, 4);
	*buf = c;
	return buf;
}

int MSG_WriteFloat(sizebuf_t *sb, float f)
{
	union
	{
		float f;
		int l;
	} dat;

	dat.f = f;
	dat.l = LittleLong(dat.l);

	return SZ_Write(sb, &dat.l, 4);
}

int MSG_WriteString(sizebuf_t *sb, const char *s)
{
	if (!s)
	{
		return SZ_Write(sb, "", 1);
	}
	else
	{
		int len = Q_strlen(s) + 1;
		return SZ_Write(sb, s, len);
	}
}

short *MSG_WriteCoord(sizebuf_t *sb, float f)
{
	return MSG_WriteShort(sb, (int)(f * 8.0));
}

byte *MSG_WriteAngle(sizebuf_t *sb, float f)
{
	return MSG_WriteByte(sb, ((int)f * 256) / 360);
}

void MSG_BeginReading(void)
{
	msg_readcount = 0;
	msg_badread = 0;
}

//=========================================================
// MSG_BadRead_Null - empty stub
//=========================================================
void MSG_BadRead_Null(void)
{
}

//=========================================================
// MSG_ReadByte_Null - empty stub
//=========================================================
void MSG_ReadByte_Null(void)
{
}

//=========================================================
// MSG_ReadShort_Null - empty stub
//=========================================================
void MSG_ReadShort_Null(void)
{
}

int MSG_ReadChar(void)
{
	int c;

	if (net_message.cursize >= msg_readcount + 1)
	{
		c = (signed char)net_message.data[msg_readcount];
		msg_readcount++;
		return c;
	}

	msg_badread = 1;
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

	msg_badread = 1;
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

	msg_badread = 1;
	return -1;
}

int MSG_ReadUShort(void)
{
	unsigned char *p;

	if (net_message.cursize >= msg_readcount + 2)
	{
		p = &net_message.data[msg_readcount];
		msg_readcount += 2;
		return (p[1] << 8) + p[0];
	}

	msg_badread = 1;
	return -1;
}

int MSG_ReadLong(void)
{
	int c;
	int high;

	if (net_message.cursize >= msg_readcount + 4)
	{
		high = net_message.data[msg_readcount + 2] + (net_message.data[msg_readcount + 3] << 8);
		c = net_message.data[msg_readcount] + ((net_message.data[msg_readcount + 1] + (high << 8)) << 8);
		msg_readcount += 4;
		return c;
	}

	msg_badread = 1;
	return -1;
}

float MSG_ReadFloat(void)
{
	union
	{
		int l;
		float f;
	} dat;

	memcpy(&dat.l, net_message.data + msg_readcount, sizeof(dat.l));
	msg_readcount += 4;
	dat.l = LittleLong(dat.l);

	return dat.f;
}

double MSG_ReadTime(void)
{
	return (double)MSG_ReadFloat();
}

char *MSG_ReadString(void)
{
	static char string[2048];
	int l = 0;
	int c;

	do
	{
		c = MSG_ReadChar();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < 2047);

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

		if (buf->maxsize < length)
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
	int len = Q_strlen(data) + 1;

	if (buf->data[buf->cursize - 1])
	{

		Q_memcpy(SZ_GetSpace(buf, len), data, len);
	}
	else
	{

		Q_memcpy((byte *)SZ_GetSpace(buf, len - 1) - 1, data, len);
	}
}

void COM_StripExtension(const char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

//=========================================================
// COM_StripExtension_Null - empty stub
//=========================================================
void COM_StripExtension_Null(void)
{
}

char *COM_FileExtension(char *in)
{
	static char exten[8];
	int i;

	while (*in && *in != '.')
		in++;

	if (!*in)
		return "";

	in++;

	for (i = 0; i < 7 && *in; i++)
		exten[i] = *in++;

	exten[i] = 0;

	return exten;
}

//=========================================================
// COM_NullSub - empty stub
//=========================================================
void COM_NullSub(void)
{
}

//=========================================================
// NullSub_ReturnShort - identity short stub
//=========================================================
short NullSub_ReturnShort(short value)
{
	return value;
}

void COM_FileBase(const char *in, char *out)
{
	int len, start, end;

	len = Q_strlen(in);

	end = len - 1;
	while (end && in[end] != '.' && in[end] != '/')
		end--;

	if (in[end] != '.')
		end = len - 1;
	else
		end--;

	start = len - 1;
	while (start >= 0 && in[start] != '/')
		start--;

	if (in[start] != '/')
		start = 0;
	else
		start++;

	len = end - start + 1;
	Q_memcpy(out, &in[start], len);
	out[len] = 0;
}

void COM_DefaultExtension(char *path, const char *extension)
{
	char *src;

	src = path + Q_strlen(path) - 1;

	while (src != path && *src != '/')
	{
		if (*src == '.')
			return;
		src--;
	}

	Q_strcat(path, extension);
}

char *COM_Parse(char *data)
{
	int c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;
		data++;
	}

	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

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

	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
	{
		com_token[0] = c;
		com_token[1] = 0;
		return data + 1;
	}

	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	} while (c > ' ' && c != '{' && c != '}' && c != ')' && c != '(' && c != '\'' && c != ':');

	com_token[len] = 0;
	return data;
}

int COM_CheckParm(const char *parm)
{
	int i;

	for (i = 1; i < com_argc; i++)
	{
		if (!com_argv[i])
			continue;
		if (!Q_strcmp(parm, com_argv[i]))
			return i;
	}

	return 0;
}

void COM_InitArgv(int argc, char **argv)
{
	static const char *s_argvDummy = " ";
	static const char *const s_safeArgv[] =
	{
		"-stdvid",
		"-nolan",
		"-nosound",
		"-nocdaudio",
		"-nojoy",
		"-nomouse",
		"-dibonly",
	};

	qboolean safe;
	int arg;
	int cmdlinePos;

	safe = false;
	cmdlinePos = 0;

	for (arg = 0; arg < argc && arg < 50; ++arg)
	{
		const char *p;

		if (cmdlinePos >= 255)
			break;

		p = argv[arg];
		while (*p && cmdlinePos < 255)
		{
			com_cmdline[cmdlinePos++] = *p++;
		}

		if (cmdlinePos >= 255)
			break;

		com_cmdline[cmdlinePos++] = ' ';
	}

	com_cmdline[cmdlinePos] = 0;

	for (arg = 0; arg < argc && arg < 50; ++arg)
	{
		com_argv[arg] = argv[arg];
		if (!Q_strcmp("-safe", argv[arg]))
			safe = true;
	}

	com_argc = arg;

	if (safe)
	{
		int i;

		for (i = 0; i < (int)(sizeof(s_safeArgv) / sizeof(s_safeArgv[0])) && com_argc < 50; ++i)
		{
			com_argv[com_argc++] = (char *)s_safeArgv[i];
		}
	}

	if (com_argc < 50)
		com_argv[com_argc] = (char *)s_argvDummy;

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

void COM_Path_f(void)
{
	searchpath_t *s;

	Con_Printf("Current search path:\n");
	for (s = com_searchpaths; s; s = s->next)
	{
		if (s->pack)
		{
			Con_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		}
		else
		{
			Con_Printf("%s\n", s->filename);
		}
	}
}

void COM_Init(void)
{
	byte swaptest[2] = {1, 0};

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

char *va(char *format, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, format);
	vsprintf(string, format, argptr);
	va_end(argptr);

	return string;
}

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

void COM_CreatePath(char *path)
{
	char *ofs;

	for (ofs = path + 1; *ofs; ofs++)
	{
		if (*ofs == '/')
		{
			*ofs = 0;
			Sys_mkdir(path);
			*ofs = '/';
		}
	}
}

void COM_CopyFile(char *netpath, char *cachepath)
{
	int in, out;
	int remaining, count;
	char buf[4096];

	remaining = Sys_FileOpenRead(netpath, &in);
	COM_CreatePath(cachepath);
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

int COM_FindFile(const char *filename, int *handle, FILE **file)
{
	searchpath_t *search;
	char netpath[MAX_OSPATH];
	char cachepath[MAX_OSPATH];
	pack_t *pak;
	int i;
	int findtime, cachetime;
	int file_handle;

	if (file && handle)
		Sys_Error("COM_FindFile: both handle and file set");
	if (!file && !handle)
		Sys_Error("COM_FindFile: neither handle or file set");

	for (search = com_searchpaths; search; search = search->next)
	{

		if (search->pack)
		{

			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
			{
				if (!Q_strcmp(pak->files[i].name, filename))
				{

					Sys_Printf("PackFile: %s : %s\n", pak->filename, filename);
					if (handle)
					{
						*handle = pak->handle;
						Sys_FileSeek(pak->handle, pak->files[i].filepos);
					}
					else
					{
						*file = fopen(pak->filename, "rb");
						if (*file)
							fseek(*file, pak->files[i].filepos, SEEK_SET);
					}
					com_filesize = pak->files[i].filelen;
					return com_filesize;
				}
			}
		}
		else if (static_registered || (!strchr(filename, '/') && !strchr(filename, '\\')))
		{

			sprintf(netpath, "%s/%s", search->filename, filename);

			findtime = Sys_FileTime(netpath);
			if (findtime == -1)
				continue;

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

			com_filesize = Sys_FileOpenRead(netpath, &file_handle);
			if (handle)
				*handle = file_handle;
			else
				Sys_FileClose(file_handle);

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

int COM_OpenFile(const char *filename, int *handle)
{
	return COM_FindFile(filename, handle, NULL);
}

int COM_FOpenFile(const char *filename, FILE **file)
{
	return COM_FindFile(filename, NULL, file);
}

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

byte *COM_LoadFile(const char *path, int usehunk)
{
	int h;
	byte *buf;
	char base[32];
	int len;

	len = COM_OpenFile(path, &h);
	if (h == -1)
		return NULL;

	COM_FileBase(path, base);

	if (usehunk == 1)
		buf = Hunk_AllocName(len + 1, base);
	else if (usehunk == 2)
		buf = Hunk_TempAlloc(len + 1);
	else if (usehunk == 0)
		buf = Z_Malloc(len + 1);
	else if (usehunk == 3)
		buf = Cache_Alloc(loadcache, len + 1, base);
	else if (usehunk == 4)
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

	((byte *)buf)[len] = 0;

	Draw_BeginDisc();
	Sys_FileRead(h, buf, len);
	COM_CloseFile(h);
	Draw_EndDisc();

	return buf;
}

byte *COM_LoadHunkFile(const char *path)
{
	return COM_LoadFile(path, 1);
}

byte *COM_LoadCacheFile(const char *path, cache_user_t *cu)
{
	loadcache = cu;
	return COM_LoadFile(path, 3);
}

byte *COM_LoadStackFile(const char *path, void *buffer, int bufsize)
{
	loadbuf = buffer;
	loadsize = bufsize;
	return COM_LoadFile(path, 4);
}

pack_t *COM_LoadPackFile(char *packfile)
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
		com_modified = true;

	newfiles = Hunk_AllocName(numpackfiles * sizeof(packfile_t), "packfile");

	Sys_FileSeek(packhandle, header.dirofs);
	Sys_FileRead(packhandle, newfiles, header.dirlen);

	CRC_Init(&crc);
	for (i = 0; i < header.dirlen; i++)
		CRC_ProcessByte(&crc, ((byte *)newfiles)[i]);
	if (crc != PAK0_CRC)
		com_modified = true;

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

void COM_AddGameDirectory(char *dir)
{
	int i;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];

	Q_strcpy(com_gamedir, dir);

	search = Hunk_Alloc(sizeof(searchpath_t));
	Q_strcpy(search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

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

void COM_InitFilesystem(void)
{
	int i;
	char basedir[128];
	int len;
	char *ext;
	searchpath_t *search;
	char *arg;

	i = COM_CheckParm("-basedir");
	if (i && i < com_argc - 1)
	{
		Q_strcpy(basedir, com_argv[i + 1]);
	}
	else
	{
		Q_strcpy(basedir, host_parms.basedir);
	}

	len = Q_strlen(basedir);
	if (len > 0)
	{
		if (basedir[len - 1] == '\\' || basedir[len - 1] == '/')
		{
			basedir[len - 1] = 0;
		}
	}

	i = COM_CheckParm("-cachedir");
	if (i && i < com_argc - 1)
	{
		arg = com_argv[i + 1];
		if (*arg == '-')
		{

			com_cachedir[0] = 0;
		}
		else
		{
			Q_strcpy(com_cachedir, arg);
		}
	}
	else if (host_parms.cachedir)
	{
		Q_strcpy(com_cachedir, host_parms.cachedir);
	}
	else
	{
		com_cachedir[0] = 0;
	}

	COM_AddGameDirectory(va("%s/valve", basedir));

	i = COM_CheckParm("-game");
	if (i && i < com_argc - 1)
	{
		com_modified = true;
		COM_AddGameDirectory(va("%s/%s", basedir, com_argv[i + 1]));
	}

	i = COM_CheckParm("-path");
	if (i)
	{
		com_modified = true;
		com_searchpaths = NULL;

		while (++i < com_argc)
		{
			arg = com_argv[i];
			if (!arg || *arg == '+' || *arg == '-')
				break;

			search = Hunk_Alloc(sizeof(searchpath_t));

			ext = COM_FileExtension(arg);
			if (!Q_strcmp(ext, "pak"))
			{

				search->pack = COM_LoadPackFile(arg);
				if (!search->pack)
					Sys_Error("Couldn't load packfile: %s", arg);
			}
			else
			{

				Q_strcpy(search->filename, arg);
			}

			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}

	if (COM_CheckParm("-proghack"))
	{
		proghack = true;
	}
}

static unsigned int s_fpuEnv[28];

void _fpreset(void)
{

	__asm { fnstenv [s_fpuEnv] }

	s_fpuEnv[0] |= 0x3F;

	__asm { fldenv [s_fpuEnv] }
}

void COM_PrintModelFrame(model_t *model, int frameNum)
{
	void *extra;

	extra = Mod_Extradata(model);
	if (extra)
	{
		Con_Printf("frame %i: %s\n", frameNum, (const char *)(40 * frameNum + (int)extra + 252));
	}
}

double Q_atof(const char *str)
{
	int sign;
	const char *s;
	char nextChar;
	int c;
	double val;
	int decimal_pos;
	int i;
	int decimal_places;

	sign = 1;
	s = str;
	if (*str == '-')
	{
		sign = -1;
		s = str + 1;
	}

	val = 0.0;

	if (*s == '0')
	{
		nextChar = s[1];
		if (nextChar == 'x' || nextChar == 'X')
		{
			s += 2;
			while (1)
			{
				c = *s++;

				if (c >= '0' && c <= '9')
				{
					val = val * 16.0 + (double)c - 48.0;
				}
				else if (c >= 'a' && c <= 'f')
				{
					val = val * 16.0 + (double)c - 87.0;
				}
				else if (c >= 'A' && c <= 'F')
				{
					val = val * 16.0 + (double)c - 55.0;
				}
				else
				{
					return (float)((double)sign * val);
				}
			}
		}
	}

	if (*s == '\'')
		return (float)(sign * s[1]);

	decimal_pos = -1;

	for (i = 0; ; ++i)
	{
		while (1)
		{
			c = *s++;
			if (c != '.')
				break;
			decimal_pos = i;
		}

		if (c < '0' || c > '9')
			break;

		val = val * 10.0 + (double)c - 48.0;
	}

	if (decimal_pos == -1)
		return (float)((double)sign * val);

	if (decimal_pos < i)
	{
		decimal_places = i - decimal_pos;
		while (1)
		{
			val = val / 10.0;
			if (!--decimal_places)
				break;
		}
	}

	return (float)((double)sign * val);
}

int MSG_ReadCoords(void)
{
	int x, y, z;

	x = MSG_ReadByte();
	y = MSG_ReadByte();
	z = MSG_ReadByte();
	MSG_ReadByte();

	return SetupSkyPolygonClipping(x, y, z);
}
