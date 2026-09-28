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
// sys_win.c -- Win32 system interface code, with the dedicated server console

#include "quakedef.h"
#include "winquake.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include <direct.h>
#include <ctype.h>
#include <conio.h>

#define MINIMUM_WIN_MEMORY	0x0800000
#define MAXIMUM_WIN_MEMORY	0x1000000

#define MAX_NUM_ARGVS		50
#define MAX_HANDLES			10

#define PAUSE_SLEEP			50		// sleep time on pause
#define NOT_FOCUS_SLEEP		20		// sleep time when not focus

#define IDD_DIALOG1			108		// the startup splash dialog

#define MAX_TIMER_FREQ		2000000.0	// timer counts are scaled down to about 1 microsecond
#define TIMER_WRAP_LIMIT	0x10000000	// a larger step back is taken as the counter wrapping
#define SAMETIME_LIMIT		100000
#define ERROR_WAIT_TIME		60.0		// seconds a dedicated server waits for Enter after an error

#define ERROR_SEPARATOR		"********************************\n"

#define ASCII_DEL			127

// Sys_PageIn skips 16 pages ahead, so Win95 doesn't think we're trying to page ourselves in
#define PAGEIN_SKIP			(16 * 0x1000)

// QS_ALLINPUT as the Win95 SDK had it, without the newer input types
#define QS_ALLINPUT_95		(QS_KEY | QS_MOUSE | QS_POSTMESSAGE | QS_TIMER | QS_PAINT | QS_SENDMESSAGE | QS_HOTKEY)

// dedicated server console requests (from the parent process in the shared buffer)
#define CCOM_WRITE_TEXT		0x2		// Param1 : Text
#define CCOM_GET_TEXT		0x3		// Param1 : Begin line, Param2 : End line
#define CCOM_GET_SCR_LINES	0x4		// No params
#define CCOM_SET_SCR_LINES	0x5		// Param1 : Number of lines

#define CONSOLE_COLUMNS		80
#define CONSOLE_LINES		25

// keyboard scan codes
#define SCAN_RETURN			28
#define SCAN_A				30
#define SCAN_DIGITS			2

HWND		h_dialog;
HINSTANCE	global_hInstance;
int			global_nCmdShow;
char		Buffer[1024];
static char	*argv[MAX_NUM_ARGVS];
static char	sys_exe_path[MAX_PATH];
char		*exe_path = sys_exe_path;
HANDLE		h_event;
HANDLE		h_console_input;
HANDLE		h_console_output;
int			hfile_param;
int			hparent_param;
int			hchild_param;

int			cl_paused = 0;
int			menu_active = 0;
int			console_active = 0;
int			force_active = 0;
int			sleep_state = 0;
int			first_time = 1;
int			perf_shift = 0;
double		perf_scale = 0.0;
DWORD		last_perf_count = 0;
double		sys_curtime = 0.0;
double		sys_oldtime = 0.0;
int			time_overflow = 0;
int			is_os_version_4 = 0;
int			is_winnt = 0;
int			sys_error_active = 0;
int			console_buffer_len = 0;
char		console_buffer[256];
float		dedicated_frame_time = 0.05f;

FILE		*sys_handles[MAX_HANDLES];
char		*sys_filemode = "rb";

// dedicated server console, driven by the parent process
HANDLE		heventDone = NULL;
HANDLE		hfileBuffer = NULL;
HANDLE		heventParentSend = NULL;
HANDLE		heventChildSend = NULL;
HANDLE		hStdout = NULL;
HANDLE		hStdin = NULL;

void Sys_InitConProc(HANDLE hFile, HANDLE heventParent, HANDLE heventChild);
DWORD WINAPI ConProcThreadFunc(LPVOID lpParameter);
void Sys_SetConProcEvent(void);
LPVOID Sys_MapViewOfFile(HANDLE hFileMappingObject);
BOOL Sys_GetConsoleHeight(DWORD *height);
BOOL Sys_ResizeConsoleBuffer(HANDLE hConsoleOutput, int cx, int cy);

void VID_ForceLockState(void)
{
}

void VID_ForceUnlockedAndReturnState(void)
{
}

void Con_EchoCharacter(int ch)
{
	_putch(ch);
}

static void Sys_FPU_Save(void)
{
}

static void Sys_FPU_Restore(void)
{
}

/*
================
Sys_InitFloatTime
================
*/
void Sys_InitFloatTime(void)
{
	int		j;

	Sys_FloatTime();

	j = COM_CheckParm("-starttime");

	if (j)
		sys_curtime = Q_atof(com_argv[j + 1]);
	else
		sys_curtime = 0.0;

	sys_oldtime = sys_curtime;
}

/*
================
Sys_FloatTime
================
*/
double Sys_FloatTime(void)
{
	DWORD			temp, t2;
	LARGE_INTEGER	PerformanceCount;
	double			time;

	Sys_FPU_Save();

	QueryPerformanceCounter(&PerformanceCount);

	temp = (PerformanceCount.LowPart >> perf_shift) |
		(PerformanceCount.HighPart << (32 - perf_shift));

	if (first_time)
	{
		last_perf_count = temp;
		first_time = 0;
	}
	else if (temp > last_perf_count || last_perf_count - temp >= TIMER_WRAP_LIMIT)
	{
		t2 = temp - last_perf_count;
		last_perf_count = temp;
		time = (double)t2 * perf_scale;
		sys_curtime += time;

		if (++time_overflow > SAMETIME_LIMIT)
			time_overflow = 0;

		sys_oldtime = sys_curtime;
	}
	else
	{
		// check for turnover or backward time
		last_perf_count = temp;		// so we can't get stuck
	}

	Sys_FPU_Restore();

	return sys_curtime;
}

/*
================
Sys_Init
================
*/
void Sys_Init(void)
{
	LARGE_INTEGER	PerformanceFreq;
	int				i;
	BOOL			result;
	OSVERSIONINFOA	vinfo;

	if (!QueryPerformanceFrequency(&PerformanceFreq))
		Sys_Error("No hardware time available");

// get 32 out of the 64 time bits such that we have around
// 1 microsecond resolution
	for (i = 0; ; i++)
	{
		if (!PerformanceFreq.HighPart)
		{
			perf_shift = i;

			if ((double)PerformanceFreq.LowPart <= MAX_TIMER_FREQ)
				break;
		}

		PerformanceFreq.QuadPart = PerformanceFreq.QuadPart / 2;
	}

	perf_scale = 1.0 / (double)PerformanceFreq.LowPart;

	Sys_InitFloatTime();

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);
#pragma warning(push)
#pragma warning(disable:4996)
	result = GetVersionExA(&vinfo);
#pragma warning(pop)

	if (!result)
		Sys_Error("Couldn't get OS info");

	is_os_version_4 = vinfo.dwMajorVersion >= 4;

	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error("Valve games require Windows 95 or NT");

	is_winnt = vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT;
}

void Sys_SetupDedicated(int hfile, int hparent, int hchild)
{
	Sys_InitConProc((HANDLE)hfile, (HANDLE)hparent, (HANDLE)hchild);
}

void Sys_CleanupDedicated(void)
{
	Sys_SetConProcEvent();
}

/*
================
Sys_Error
================
*/
__declspec(noreturn) void Sys_Error(const char *error, ...)
{
	char		text2[1024];
	char		text[1024];
	double		starttime;
	DWORD		dummy;
	va_list		argptr;

	va_start(argptr, error);

	VID_ForceLockState();

	vsprintf(text, error, argptr);

	if (IsDebuggerPresent())
	{
		OutputDebugStringA(text);
		OutputDebugStringA("\n");
		DebugBreak();
	}

	if (is_dedicated)
	{
		sprintf(text2, "ERROR: %s\n", text);

		// the leading newline takes the place of the separator's own
		WriteFile(h_console_output, "\n" ERROR_SEPARATOR, strlen(ERROR_SEPARATOR), &dummy, NULL);
		WriteFile(h_console_output, text2, strlen(text2), &dummy, NULL);
		WriteFile(h_console_output, "Press Enter to exit.\n", strlen("Press Enter to exit.\n"), &dummy, NULL);
		WriteFile(h_console_output, ERROR_SEPARATOR, strlen(ERROR_SEPARATOR), &dummy, NULL);

		starttime = Sys_FloatTime();
		sys_error_active = 1;

		while (Sys_FloatTime() - starttime < ERROR_WAIT_TIME && !Sys_ConsoleInput())
			;
	}
	else
	{
		VID_ForceUnlockedAndReturnState();
		MessageBoxA(NULL, text, "Engine Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
	}

	Host_Shutdown();
	Sys_CleanupDedicated();

	exit(1);
}

/*
================
Sys_ConsoleInput

Line input for the dedicated server console.
================
*/
char *Sys_ConsoleInput(void)
{
	INPUT_RECORD	recs;
	DWORD			numevents;
	DWORD			numread;
	DWORD			dummy;
	int				ch;

	if (!is_dedicated)
		return NULL;

	while (1)
	{
		if (!GetNumberOfConsoleInputEvents(h_console_input, &numevents))
			Sys_Error("Error getting console input events");

		if ((int)numevents <= 0)
			break;

		if (!ReadConsoleInputA(h_console_input, &recs, 1, &numread))
			Sys_Error("Error reading console input");

		if (numread != 1)
			Sys_Error("Couldn't read console input");

		if (recs.EventType == KEY_EVENT && !recs.Event.KeyEvent.bKeyDown)
		{
			ch = recs.Event.KeyEvent.uChar.AsciiChar;

			if (ch == '\b')
			{
				WriteFile(h_console_output, "\b \b", strlen("\b \b"), &dummy, NULL);

				if (console_buffer_len)
				{
					console_buffer_len--;
					Con_EchoCharacter('\b');
				}
			}
			else if (ch == '\r')
			{
				WriteFile(h_console_output, "\r\n", strlen("\r\n"), &dummy, NULL);

				if (console_buffer_len)
				{
					console_buffer[console_buffer_len] = 0;
					console_buffer_len = 0;
					return console_buffer;
				}

				if (sys_error_active)
				{
					console_buffer[0] = '\r';
					console_buffer_len = 0;
					return console_buffer;
				}
			}
			else if (ch >= ' ')
			{
				WriteFile(h_console_output, &ch, 1, &dummy, NULL);

				console_buffer[console_buffer_len] = ch;
				console_buffer_len++;
			}
		}
	}

	return NULL;
}

void Sys_Sleep(void)
{
	Sleep(1);
}

static int	sys_checksum;

/*
================
Sys_PageIn

Touches all the memory to make sure it's there.
================
*/
static void Sys_PageIn(void *ptr, int size)
{
	byte	*x;
	int		m, n;

	x = (byte *)ptr;

	for (n = 0; n < 4; n++)
	{
		for (m = 0; m < size - PAGEIN_SKIP; m += 4)
		{
			sys_checksum += *(int *)&x[m];
			sys_checksum += *(int *)&x[m + PAGEIN_SKIP - 4];
		}
	}
}

DWORD Sys_MSleep(DWORD msec)
{
	return MsgWaitForMultipleObjects(1, &h_event, FALSE, msec, QS_ALLINPUT_95);
}

/*
==================
WinMain
==================
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	char			cwd[1024];
	char			exe_dir[MAX_PATH];
	char			*slash;
	HWND			hwnd_splash;
	LPSTR			cmd;
	int				argc;
	int				i, p;
	MEMORYSTATUS	lpBuffer;
	double			time, oldtime, newtime;
	size_t			heapsize;
	void			*membase;
	quakeparms_t	parms;

	// previous instances do not exist in Win32
	if (hPrevInstance)
		return 0;

	hwnd_splash = CreateDialogParamA(hInstance, MAKEINTRESOURCEA(IDD_DIALOG1), NULL, NULL, 0);
	h_dialog = hwnd_splash;

	if (hwnd_splash)
	{
		ShowWindow(hwnd_splash, SW_SHOW);
		UpdateWindow(hwnd_splash);
		SetForegroundWindow(hwnd_splash);
	}

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

	lpBuffer.dwLength = sizeof(lpBuffer);
	GlobalMemoryStatus(&lpBuffer);

	sys_exe_path[0] = 0;
	GetModuleFileNameA(NULL, sys_exe_path, sizeof(sys_exe_path));

	// under a debugger run from the directory of the executable
	Q_strncpy(exe_dir, sys_exe_path, sizeof(exe_dir) - 1);
	exe_dir[sizeof(exe_dir) - 1] = 0;

	slash = Q_strrchr(exe_dir, '\\');
	if (slash)
		*slash = 0;

	if (IsDebuggerPresent() && exe_dir[0])
		SetCurrentDirectoryA(exe_dir);

	if (!GetCurrentDirectoryA(sizeof(cwd), cwd))
		Sys_Error("Couldn't determine current directory");

	i = Q_strlen(cwd);
	if (i > 0 && (cwd[i - 1] == '/' || cwd[i - 1] == '\\'))
		cwd[i - 1] = 0;

	argv[0] = exe_path;
	cmd = lpCmdLine;
	argc = 1;

	if (*lpCmdLine)
	{
		while (argc < MAX_NUM_ARGVS)
		{
			while (*cmd && (*cmd <= ' ' || *cmd == ASCII_DEL))
				cmd++;

			if (!*cmd)
				break;

			argv[argc++] = cmd;

			while (*cmd && *cmd > ' ' && *cmd != ASCII_DEL)
				cmd++;

			if (!*cmd)
				break;

			*cmd++ = 0;
		}
	}

	COM_InitArgv(argc, argv);

	is_dedicated = (COM_CheckParm("-dedicated") != 0);

	if (!is_dedicated)
	{
		if (h_dialog)
		{
			ShowWindow(h_dialog, SW_SHOW);
			UpdateWindow(h_dialog);
			SetForegroundWindow(h_dialog);
		}
	}

	heapsize = lpBuffer.dwAvailPhys;

	if (heapsize < MINIMUM_WIN_MEMORY)
		heapsize = MINIMUM_WIN_MEMORY;

	if (heapsize < (lpBuffer.dwTotalPhys / 2))
		heapsize = lpBuffer.dwTotalPhys / 2;

	if (heapsize > MAXIMUM_WIN_MEMORY)
		heapsize = MAXIMUM_WIN_MEMORY;

	p = COM_CheckParm("-heapsize");
	if (p && p + 1 < com_argc)
		heapsize = Q_atoi(com_argv[p + 1]) * 1024;

	membase = malloc(heapsize);
	if (!membase)
		Sys_Error("Not enough memory for heap");

	Sys_PageIn(membase, (int)heapsize);

	h_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!h_event)
		Sys_Error("Couldn't create main loop event");

	if (is_dedicated)
	{
		if (!AllocConsole())
			Sys_Error("Couldn't create dedicated server console");

		h_console_input = GetStdHandle(STD_INPUT_HANDLE);
		h_console_output = GetStdHandle(STD_OUTPUT_HANDLE);

	// give QHOST a chance to hook into the console
		p = COM_CheckParm("-hfile");
		if (p && p + 1 < com_argc)
			hfile_param = Q_atoi(com_argv[p + 1]);

		p = COM_CheckParm("-hparent");
		if (p && p + 1 < com_argc)
			hparent_param = Q_atoi(com_argv[p + 1]);

		p = COM_CheckParm("-hchild");
		if (p && p + 1 < com_argc)
			hchild_param = Q_atoi(com_argv[p + 1]);

		Sys_SetupDedicated(hfile_param, hparent_param, hchild_param);
	}

	Sys_Init();

	// because sound is off until we become active
	S_BlockSound();

	memset(&parms, 0, sizeof(parms));
	parms.basedir = cwd;
	parms.cachedir = NULL;
	parms.argc = com_argc;
	parms.argv = com_argv;
	parms.membase = membase;
	parms.memsize = (int)heapsize;

	Host_Init(&parms);
	LoadEntityDLLs(parms.basedir);

	oldtime = Sys_FloatTime();

	// main window message loop
	while (1)
	{
		if (is_dedicated)
		{
			newtime = Sys_FloatTime();

			while (newtime - oldtime < dedicated_frame_time)
			{
				Sys_Sleep();
				newtime = Sys_FloatTime();
			}
		}
		else
		{
		// yield the CPU for a little while when paused, minimized, or not the focus
			if ((cl_paused && !menu_active && !console_active) || force_active)
			{
				MsgWaitForMultipleObjects(1, &h_event, FALSE, PAUSE_SLEEP, QS_ALLINPUT);
				sleep_state = 1;
			}
			else if (!menu_active && !console_active)
			{
				MsgWaitForMultipleObjects(1, &h_event, FALSE, NOT_FOCUS_SLEEP, QS_ALLINPUT);
			}

			newtime = Sys_FloatTime();
		}

		time = newtime - oldtime;
		oldtime = newtime;

		Host_Frame((float)time);
	}

	// return success of application
	return 0;
}

/*
===============================================================================

FILE IO

===============================================================================
*/

int Sys_FileLength(FILE *f)
{
	int		pos;
	int		end;

	VID_ForceLockState();

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	VID_ForceUnlockedAndReturnState();

	return end;
}

static int Sys_FindHandle(void)
{
	int		i;

	for (i = 1; i < MAX_HANDLES; i++)
	{
		if (!sys_handles[i])
			return i;
	}

	Sys_Error("out of handles");
	return -1;
}

int Sys_FileOpenRead(char *path, int *hndl)
{
	FILE	*f;
	int		i, retval;

	VID_ForceLockState();

	i = Sys_FindHandle();

	f = fopen(path, sys_filemode);
	if (f)
	{
		sys_handles[i] = f;
		*hndl = i;
		retval = Sys_FileLength(f);
	}
	else
	{
		retval = -1;
		*hndl = -1;
	}

	VID_ForceUnlockedAndReturnState();

	return retval;
}

int Sys_FileOpenWrite(char *path)
{
	FILE	*f;
	int		i;

	VID_ForceLockState();

	i = Sys_FindHandle();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error("Error opening %s: %s", path, strerror(errno));
	sys_handles[i] = f;

	VID_ForceUnlockedAndReturnState();

	return i;
}

void Sys_FileClose(int handle)
{
	FILE	**f;

	VID_ForceLockState();

	f = &sys_handles[handle];
	fclose(*f);
	*f = NULL;

	VID_ForceUnlockedAndReturnState();
}

void Sys_FileSeek(int handle, int position)
{
	VID_ForceLockState();
	fseek(sys_handles[handle], position, SEEK_SET);
	VID_ForceUnlockedAndReturnState();
}

size_t Sys_FileRead(int handle, void *dest, size_t count)
{
	size_t	x;

	VID_ForceLockState();
	x = fread(dest, 1, count, sys_handles[handle]);
	VID_ForceUnlockedAndReturnState();

	return x;
}

size_t Sys_FileWrite(int handle, void *data, size_t count)
{
	size_t	x;

	VID_ForceLockState();
	x = fwrite(data, 1, count, sys_handles[handle]);
	VID_ForceUnlockedAndReturnState();

	return x;
}

int Sys_FileTime(char *path)
{
	FILE	*f;
	int		retval;

	VID_ForceLockState();

	f = fopen(path, "rb");

	if (f)
	{
		fclose(f);
		retval = 1;
	}
	else
	{
		retval = -1;
	}

	VID_ForceUnlockedAndReturnState();

	return retval;
}

int Sys_mkdir(char *path)
{
	return _mkdir(path);
}

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

int Sys_SetMemoryProtection(void *addr, size_t length)
{
	DWORD	flOldProtect;
	BOOL	result;

	result = VirtualProtect(addr, length, PAGE_READONLY, &flOldProtect);
	if (!result)
		Sys_Error("Protection change failed");

	return result;
}

void Sys_Printf(char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	DWORD		dummy;

	va_start(argptr, fmt);

	if (is_dedicated)
	{
		vsprintf(text, fmt, argptr);
		WriteFile(h_console_output, text, strlen(text), &dummy, NULL);
	}
}

void Sys_Quit(void)
{
	VID_ForceLockState();

	Host_Shutdown();

	if (h_event)
		CloseHandle(h_event);

	if (is_dedicated)
		FreeConsole();

// shut down QHOST hooks if necessary
	Sys_CleanupDedicated();

	exit(0);
}

/*
================
Sys_PumpMessages
================
*/
void Sys_PumpMessages(void)
{
	MSG		msg;

	while (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
	// we always update if there are any event, even if we're paused
		sleep_state = 0;

		if (!GetMessageA(&msg, NULL, 0, 0))
			Sys_Quit();

		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}

void Sys_SendKeyEvents(void)
{
	Sys_PumpMessages();
}

DWORD Sys_WaitMessage(DWORD time)
{
	return MsgWaitForMultipleObjects(1, &h_event, FALSE, time, QS_ALLINPUT);
}

/*
===============================================================================

DEDICATED SERVER CONSOLE

A parent process (QHOST) can drive the console through a shared memory
buffer and a pair of events.

===============================================================================
*/

/*
================
Sys_InitConProc
================
*/
void Sys_InitConProc(HANDLE hFile, HANDLE heventParent, HANDLE heventChild)
{
	DWORD	dwID;

// ignore if we don't have all the events
	if (!hFile || !heventParent || !heventChild)
		return;

	hfileBuffer = hFile;
	heventParentSend = heventParent;
	heventChildSend = heventChild;

// so we'll know when to go away
	heventDone = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!heventDone)
	{
		Con_Printf("Couldn't create heventDone\n");
		return;
	}

	if (!CreateThread(NULL, 0, ConProcThreadFunc, NULL, 0, &dwID))
	{
		CloseHandle(heventDone);
		Con_Printf("Couldn't create QHOST thread\n");
		return;
	}

// save off the input/output handles
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	hStdin = GetStdHandle(STD_INPUT_HANDLE);

// force 80 character width, at least 25 character height
	Sys_ResizeConsoleBuffer(hStdout, CONSOLE_COLUMNS, CONSOLE_LINES);
}

BOOL Sys_UnmapViewOfFile(LPCVOID pBuffer)
{
	return UnmapViewOfFile(pBuffer);
}

/*
================
Sys_ReadConsoleOutput

Copies console lines iBeginLine to iEndLine into pszText.
================
*/
BOOL Sys_ReadConsoleOutput(LPSTR pszText, int iBeginLine, int iEndLine)
{
	DWORD	dwRead;
	COORD	coord;

	coord.X = 0;
	coord.Y = (SHORT)iBeginLine;

	if (!ReadConsoleOutputCharacterA(hStdout, pszText, CONSOLE_COLUMNS * (iEndLine - iBeginLine + 1), coord, &dwRead))
		return FALSE;

	// make sure it's null terminated
	pszText[dwRead] = 0;
	return TRUE;
}

BOOL Sys_SetConsoleHeight(int iLines)
{
	return Sys_ResizeConsoleBuffer(hStdout, CONSOLE_COLUMNS, iLines);
}

static WORD Sys_CharToScanCode(char c)
{
	unsigned char	uc = (unsigned char)c;
	int				upper = toupper(uc);

	if (uc == '\r')
		return SCAN_RETURN;

	if (_isctype(uc, _ALPHA))
		return (WORD)(SCAN_A + upper - 'A');

	if (_isctype(uc, _DIGIT))
		return (WORD)(SCAN_DIGITS + upper - '0');

	return (WORD)uc;
}

/*
================
Sys_WriteConsoleInput

Types szText into the console as key events.
================
*/
int Sys_WriteConsoleInput(char *szText)
{
	char			*sz;
	INPUT_RECORD	rec;
	DWORD			dwWritten;

	sz = szText;

	while (*sz)
	{
		char	c = *sz;
		int		upper;

	// 13 is the code for a carriage return (\n) instead of 10
		if (c == '\n')
			c = '\r';

		upper = toupper((unsigned char)c);

		rec.EventType = KEY_EVENT;
		rec.Event.KeyEvent.wRepeatCount = 1;
		rec.Event.KeyEvent.bKeyDown = TRUE;
		rec.Event.KeyEvent.wVirtualKeyCode = (WORD)upper;
		rec.Event.KeyEvent.wVirtualScanCode = Sys_CharToScanCode(c);
		rec.Event.KeyEvent.uChar.AsciiChar = c;
		rec.Event.KeyEvent.uChar.UnicodeChar = (WCHAR)(unsigned char)c;
		rec.Event.KeyEvent.dwControlKeyState = _isctype((unsigned char)c, _UPPER) ? CAPSLOCK_ON : 0;

		WriteConsoleInputA(hStdin, &rec, 1, &dwWritten);

		rec.Event.KeyEvent.bKeyDown = FALSE;

		WriteConsoleInputA(hStdin, &rec, 1, &dwWritten);

		*sz++ = c;
	}

	return 1;
}

/*
================
ConProcThreadFunc

Serves the requests of the parent process.
================
*/
DWORD WINAPI ConProcThreadFunc(LPVOID lpParameter)
{
	int		*pBuffer;
	int		result;
	HANDLE	heventWait[2];

	heventWait[0] = heventParentSend;
	heventWait[1] = heventDone;

	// heventDone fired, so we're exiting
	while (WaitForMultipleObjects(2, heventWait, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
	{
		pBuffer = (int *)Sys_MapViewOfFile(hfileBuffer);

		// hfileBuffer is invalid, just leave
		if (!pBuffer)
		{
			Con_Printf("Invalid hFileBuffer\n");
			return 0;
		}

		if (pBuffer[0] == CCOM_WRITE_TEXT)
		{
			result = Sys_WriteConsoleInput((char *)(pBuffer + 1));
		}
		else
		{
			switch (pBuffer[0])
			{
			case CCOM_GET_TEXT:
				result = Sys_ReadConsoleOutput((char *)(pBuffer + 1), pBuffer[1], pBuffer[2]);
				break;

			case CCOM_GET_SCR_LINES:
				result = Sys_GetConsoleHeight((DWORD *)(pBuffer + 1));
				break;

			case CCOM_SET_SCR_LINES:
				result = Sys_SetConsoleHeight(pBuffer[1]);
				break;

			default:
				result = 0;
				break;
			}
		}

		pBuffer[0] = result;

		Sys_UnmapViewOfFile(pBuffer);
		SetEvent(heventChildSend);
	}

	return 0;
}

LPVOID Sys_MapViewOfFile(HANDLE hFileMappingObject)
{
	return MapViewOfFile(hFileMappingObject, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
}

void Sys_SetConProcEvent(void)
{
	if (heventDone)
		SetEvent(heventDone);
}

BOOL Sys_GetConsoleHeight(DWORD *piLines)
{
	CONSOLE_SCREEN_BUFFER_INFO	info;

	if (!GetConsoleScreenBufferInfo(hStdout, &info))
		return FALSE;

	*piLines = info.dwSize.Y;
	return TRUE;
}

/*
================
Sys_ResizeConsoleBuffer

Sets the console to cx columns and cy lines, within the largest size allowed.
================
*/
BOOL Sys_ResizeConsoleBuffer(HANDLE hConsoleOutput, int cx, int cy)
{
	CONSOLE_SCREEN_BUFFER_INFO	info;
	COORD						coordMax;

	coordMax = GetLargestConsoleWindowSize(hConsoleOutput);

	if (cy > coordMax.Y)
		cy = coordMax.Y;

	if (cx > coordMax.X)
		cx = coordMax.X;

	if (!GetConsoleScreenBufferInfo(hConsoleOutput, &info))
		return FALSE;

// height
	info.srWindow.Left = 0;
	info.srWindow.Right = info.dwSize.X - 1;
	info.srWindow.Top = 0;
	info.srWindow.Bottom = cy - 1;

	if (cy < info.dwSize.Y)
	{
		if (!SetConsoleWindowInfo(hConsoleOutput, TRUE, &info.srWindow))
			return FALSE;

		info.dwSize.Y = cy;

		if (!SetConsoleScreenBufferSize(hConsoleOutput, info.dwSize))
			return FALSE;
	}
	else if (cy > info.dwSize.Y)
	{
		info.dwSize.Y = cy;

		if (!SetConsoleScreenBufferSize(hConsoleOutput, info.dwSize))
			return FALSE;

		if (!SetConsoleWindowInfo(hConsoleOutput, TRUE, &info.srWindow))
			return FALSE;
	}

	if (!GetConsoleScreenBufferInfo(hConsoleOutput, &info))
		return FALSE;

// width
	info.srWindow.Left = 0;
	info.srWindow.Right = cx - 1;
	info.srWindow.Top = 0;
	info.srWindow.Bottom = info.dwSize.Y - 1;

	if (cx < info.dwSize.X)
	{
		if (!SetConsoleWindowInfo(hConsoleOutput, TRUE, &info.srWindow))
			return FALSE;

		info.dwSize.X = cx;

		return SetConsoleScreenBufferSize(hConsoleOutput, info.dwSize);
	}

	if (cx <= info.dwSize.X)
		return TRUE;

	info.dwSize.X = cx;

	if (!SetConsoleScreenBufferSize(hConsoleOutput, info.dwSize))
		return FALSE;

	if (!SetConsoleWindowInfo(hConsoleOutput, TRUE, &info.srWindow))
		return FALSE;

	return TRUE;
}
