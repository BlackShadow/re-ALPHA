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
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include <direct.h>
#include <ctype.h>
#include <conio.h>

extern qboolean is_dedicated;
HWND h_dialog;
HINSTANCE hInstance;
int n_show_cmd;
char Buffer[1024];
int argv_ptr[50];
static char sys_exe_path[MAX_PATH];
char *exe_path = sys_exe_path;
extern int  com_argc;
extern char** com_argv;
HANDLE h_event;
HANDLE h_console_input;
HANDLE h_console_output;
int hfile_param;
int hparent_param;
int hchild_param;

int  cl_paused = 0;
int  menu_active = 0;
int  console_active = 0;
int  force_active = 0;
int  sleep_state = 0;
int  first_time = 1;
int  perf_shift = 0;
double perf_scale = 0.0;
DWORD last_perf_count = 0;
double sys_curtime = 0.0;
double sys_oldtime = 0.0;
int  time_overflow = 0;
int  is_os_version_4 = 0;
int  is_winnt = 0;
int  sys_error_active = 0;
int  console_buffer_len = 0;
char console_buffer[256];
float dedicated_frame_time = 0.05f;

FILE *g_FileHandles[10];
char *g_FileMode = "rb";

HANDLE g_hConProcEvent = NULL;
HANDLE g_hFileMappingObject = NULL;
HANDLE g_hParentEvent = NULL;
HANDLE g_hChildEvent = NULL;
HANDLE g_hConProcConsoleOutput = NULL;
HANDLE g_hConProcConsoleInput = NULL;

char Sys_InitConProc(HANDLE hFileParam, HANDLE hParentParam, HANDLE hChildParam);
DWORD WINAPI ConProcThreadFunc(LPVOID lpParameter);
BOOL Sys_SetConProcEvent(void);
LPVOID Sys_MapViewOfFile(HANDLE hFileMappingObject);
BOOL Sys_GetConsoleHeight(DWORD *height);
BOOL Sys_ResizeConsoleBuffer(HANDLE hConsoleOutput, int width, int height);

extern void Memory_Init(void *base, int size);
extern void Host_Shutdown(void);
extern void COM_InitArgv(int argc, char **argv);

extern const char szFmtPathSlash[];
extern const char szFmtPathFile[];
extern const char szFmtPathSubdir[];
extern const char szFmtPathSubfile[];
extern const char szConsoleBorder[];
extern const char szConsoleSeparator[];
extern const char szNewLine[];
extern const char szBackspaceErase[];

extern int g_pfnDispatchSpawn;
extern int g_pfnDispatchThink;
extern int g_pfnDispatchUse;
extern int g_pfnDispatchTouch;
extern int g_pfnDispatchSave;
extern int g_pfnDispatchRestore;
extern int g_pfnDispatchKeyValue;
extern int g_pfnDispatchBlocked;

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

void Sys_InitFloatTime(void)
{
	int p;

	Sys_FloatTime();
	p = COM_CheckParm("-starttime");

	if (p)
		sys_curtime = Q_atof(com_argv[p + 1]);
	else
		sys_curtime = 0.0;

	sys_oldtime = sys_curtime;

}

double Sys_FloatTime(void)
{
	DWORD low_count, high_count;
	LARGE_INTEGER PerformanceCount;
	double elapsed;

	Sys_FPU_Save();

	QueryPerformanceCounter(&PerformanceCount);

	low_count = (PerformanceCount.LowPart >> perf_shift) |
	     (PerformanceCount.HighPart << (32 - perf_shift));

	if (first_time)
	{
		last_perf_count = low_count;
		first_time = 0;
	}
	else if (low_count > last_perf_count || last_perf_count - low_count >= 0x10000000)
	{
		high_count = low_count - last_perf_count;
		last_perf_count = low_count;
		elapsed = (double)high_count * perf_scale;
		sys_curtime += elapsed;

		if (++time_overflow > 100000)
		{
			time_overflow = 0;
		}

		sys_oldtime = sys_curtime;
	}
	else
	{
		last_perf_count = low_count;
	}

	Sys_FPU_Restore();

	return sys_curtime;
}

void Sys_Init(void)
{
	LARGE_INTEGER Frequency;
	int i;
	BOOL result;
	OSVERSIONINFOA VersionInformation;

	if (!QueryPerformanceFrequency(&Frequency))
		Sys_Error("No hardware time available");

	for (i = 0; ; ++i)
	{
		if (!Frequency.HighPart)
		{
			perf_shift = i;

			if ((double)Frequency.LowPart <= 2000000.0)
				break;
		}

		Frequency.QuadPart = Frequency.QuadPart / 2;
	}

	perf_scale = 1.0 / (double)Frequency.LowPart;

	Sys_InitFloatTime();

	VersionInformation.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
#pragma warning(push)
#pragma warning(disable:4996)
	result = GetVersionExA(&VersionInformation);
#pragma warning(pop)

	if (!result)
		Sys_Error("Couldn't get OS info");

	is_os_version_4 = VersionInformation.dwMajorVersion >= 4;

	if (!VersionInformation.dwPlatformId)
		Sys_Error("Valve games require Windows 95 or NT");

	is_winnt = VersionInformation.dwPlatformId == 2;

}

void Sys_SetupDedicated(int hfile, int hparent, int hchild)
{
	(void)Sys_InitConProc((HANDLE)hfile, (HANDLE)hparent, (HANDLE)hchild);
}

void Sys_CleanupDedicated(void)
{
	Sys_SetConProcEvent();
}

__declspec(noreturn) void Sys_Error(const char *Format, ...)
{
	char Buffer[1024];
	char Text[1024];
	double startTime;
	DWORD NumberOfBytesWritten;
	va_list va;

	va_start(va, Format);

	VID_ForceLockState();

	vsprintf(Text, Format, va);

	if (IsDebuggerPresent())
	{
		OutputDebugStringA(Text);
		OutputDebugStringA("\n");
		DebugBreak();
	}

	if (is_dedicated)
	{
		sprintf(Buffer, "ERROR: %s\n", Text);

		WriteFile(h_console_output, "\n********************************\n", 33, &NumberOfBytesWritten, 0);
		WriteFile(h_console_output, Buffer, strlen(Buffer), &NumberOfBytesWritten, 0);
		WriteFile(h_console_output, "Press Enter to exit.\n", 21, &NumberOfBytesWritten, 0);
		WriteFile(h_console_output, "********************************\n", 33, &NumberOfBytesWritten, 0);

		startTime = Sys_FloatTime();
		sys_error_active = 1;

		while (Sys_FloatTime() - startTime < 60.0 && !Sys_ConsoleInput())
			;
	}
	else
	{
		VID_ForceUnlockedAndReturnState();
		MessageBoxA(0, Text, "Engine Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
	}

	Host_Shutdown();
	Sys_CleanupDedicated();

	exit(1);
}

char* Sys_ConsoleInput(void)
{
	INPUT_RECORD Buffer;
	DWORD NumberOfEvents;
	DWORD NumberOfEventsRead;
	DWORD NumberOfBytesWritten;
	int AsciiChar;

	if (!is_dedicated)
		return NULL;

	while (1)
	{
		if (!GetNumberOfConsoleInputEvents(h_console_input, &NumberOfEvents))
			Sys_Error("Error getting console input events");

		if ((int)NumberOfEvents <= 0)
			break;

		if (!ReadConsoleInputA(h_console_input, &Buffer, 1, &NumberOfEventsRead))
			Sys_Error("Error reading console input");

		if (NumberOfEventsRead != 1)
			Sys_Error("Couldn't read console input");

		if (Buffer.EventType == KEY_EVENT && !Buffer.Event.KeyEvent.bKeyDown)
		{
			AsciiChar = Buffer.Event.KeyEvent.uChar.AsciiChar;

			if (AsciiChar == 8)
			{
				WriteFile(h_console_output, "\b \b", 3, &NumberOfBytesWritten, NULL);

				if (console_buffer_len)
				{
					--console_buffer_len;
					Con_EchoCharacter(8);
				}
			}
			else if (AsciiChar == 13)
			{
				WriteFile(h_console_output, "\r\n", 2, &NumberOfBytesWritten, NULL);

				if (console_buffer_len)
				{
					console_buffer[console_buffer_len] = 0;
					console_buffer_len = 0;
					return console_buffer;
				}

				if (sys_error_active)
				{
					console_buffer[0] = 13;
					console_buffer_len = 0;
					return console_buffer;
				}
			}
			else if (AsciiChar >= 32)
			{
				WriteFile(h_console_output, &AsciiChar, 1, &NumberOfBytesWritten, NULL);

				console_buffer[console_buffer_len] = AsciiChar;
				console_buffer_len++;
			}
		}
	}

	return NULL;
}

void Sys_Sleep(void)
{
	Sleep(1u);
}

static int g_PageInChecksum = 0;

static int Sys_PageIn(void *base, int size)
{
	int limit;
	int passes;
	int offset;
	int tail_value = 0;

	limit = size - 0x10000;
	passes = 4;

	do
	{
		for (offset = 0; offset < limit; offset += 4)
		{
			g_PageInChecksum += *(int *)((char *)base + offset);
			tail_value = *(int *)((char *)base + offset + 65532);
			g_PageInChecksum += tail_value;
		}
		--passes;
	} while (passes);

	return tail_value;
}

DWORD Sys_MSleep(DWORD dwMilliseconds)
{
	return MsgWaitForMultipleObjects(1u, &h_event, 0, dwMilliseconds, 0xFFu);
}

int __stdcall WinMain(HINSTANCE hInstance_arg, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	char cwd[1024];
	char exe_dir[MAX_PATH];
	LPSTR cmd;
	int argc;
	int i;
	HWND h_splash;
	MEMORYSTATUS mem_status;
	double curtime, oldtime, frametime;
	size_t heapsize;
	void *membase;
	quakeparms_t parms;

	if (hPrevInstance)
		return 0;

	h_splash = CreateDialogParamA(hInstance_arg, (LPCSTR)0x6C, 0, NULL, 0);
	h_dialog = h_splash;

	if (h_splash)
	{
		ShowWindow(h_splash, SW_SHOW);
		UpdateWindow(h_splash);
		SetForegroundWindow(h_splash);
	}

	hInstance = hInstance_arg;
	n_show_cmd = nShowCmd;

	mem_status.dwLength = sizeof(mem_status);
	GlobalMemoryStatus(&mem_status);

	sys_exe_path[0] = 0;
	GetModuleFileNameA(NULL, sys_exe_path, sizeof(sys_exe_path));

	Q_strncpy(exe_dir, sys_exe_path, sizeof(exe_dir) - 1);
	exe_dir[sizeof(exe_dir) - 1] = 0;
	{
		char *slash = Q_strrchr(exe_dir, '\\');
		if (slash)
			*slash = 0;

		if (IsDebuggerPresent() && exe_dir[0])
			SetCurrentDirectoryA(exe_dir);
	}

	if (!GetCurrentDirectoryA(sizeof(cwd), cwd))
		Sys_Error("Couldn't determine current directory");

	i = Q_strlen(cwd);
	if (i > 0 && (cwd[i - 1] == '/' || cwd[i - 1] == '\\'))
		cwd[i - 1] = 0;

	argv_ptr[0] = (int)exe_path;
	cmd = lpCmdLine;
	argc = 1;

	if (*lpCmdLine)
	{
		while (argc < 50)
		{
			while (*cmd && (*cmd <= 32 || *cmd == 127))
				++cmd;

			if (!*cmd)
				break;

			argv_ptr[argc++] = (int)cmd;

			while (*cmd && *cmd > 32 && *cmd != 127)
				++cmd;

			if (!*cmd)
				break;

			*cmd++ = 0;
		}
	}

	COM_InitArgv(argc, (char**)argv_ptr);

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

	heapsize = mem_status.dwAvailPhys;
	if (heapsize < 0x800000) heapsize = 0x800000;
	if (heapsize < (mem_status.dwTotalPhys / 2)) heapsize = mem_status.dwTotalPhys / 2;
	if (heapsize > 0x1000000) heapsize = 0x1000000;

	int p = COM_CheckParm("-heapsize");
	if (p && p + 1 < com_argc)
	{
		heapsize = Q_atoi(com_argv[p + 1]) << 10;
	}

	membase = malloc(heapsize);
	if (!membase)
		Sys_Error("Not enough memory for heap");

	Sys_PageIn(membase, (int)heapsize);

	h_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!h_event) Sys_Error("Couldn't create main loop event");

	if (is_dedicated)
	{
		if (!AllocConsole())
			Sys_Error("Couldn't create dedicated server console");

		h_console_input = GetStdHandle(STD_INPUT_HANDLE);
		h_console_output = GetStdHandle(STD_OUTPUT_HANDLE);

		p = COM_CheckParm("-hfile");
		if (p && p + 1 < com_argc) hfile_param = Q_atoi(com_argv[p + 1]);
		p = COM_CheckParm("-hparent");
		if (p && p + 1 < com_argc) hparent_param = Q_atoi(com_argv[p + 1]);
		p = COM_CheckParm("-hchild");
		if (p && p + 1 < com_argc) hchild_param = Q_atoi(com_argv[p + 1]);

		Sys_SetupDedicated(hfile_param, hparent_param, hchild_param);
	}

	Sys_Init();
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
	while (1)
	{
		if (is_dedicated)
		{
			curtime = Sys_FloatTime();
			while (curtime - oldtime < dedicated_frame_time)
			{
				Sys_Sleep();
				curtime = Sys_FloatTime();
			}
		}
		else
		{

			if (cl_paused && !menu_active && !console_active || force_active)
			{
				MsgWaitForMultipleObjects(1, &h_event, FALSE, 50, QS_ALLINPUT);
				sleep_state = 1;
			}
			else if (!menu_active && !console_active)
			{
				MsgWaitForMultipleObjects(1, &h_event, FALSE, 20, QS_ALLINPUT);
			}

			curtime = Sys_FloatTime();
		}

		frametime = curtime - oldtime;
		oldtime = curtime;

		Host_Frame((float)frametime);
	}

	return 0;
}

int Sys_FileLength(FILE *stream)
{
	int current_pos;
	int file_size;

	VID_ForceLockState();
	current_pos = ftell(stream);
	fseek(stream, 0, SEEK_END);
	file_size = ftell(stream);
	fseek(stream, current_pos, SEEK_SET);
	VID_ForceUnlockedAndReturnState();
	return file_size;
}

static int Sys_AllocFileHandle(void)
{
	int handle;

	for (handle = 1; handle < (int)(sizeof(g_FileHandles) / sizeof(g_FileHandles[0])); ++handle)
	{
		if (!g_FileHandles[handle])
			return handle;
	}

	Sys_Error("out of handles");
	return -1;
}

int Sys_FileOpenRead(char *filename, int *handle_out)
{
	int handle;
	FILE *fp;
	int size;

	VID_ForceLockState();
	handle = Sys_AllocFileHandle();
	fp = fopen(filename, g_FileMode);
	if (fp)
	{
		g_FileHandles[handle] = fp;
		*handle_out = handle;
		size = Sys_FileLength(fp);
	}
	else
	{
		size = -1;
		*handle_out = -1;
	}
	VID_ForceUnlockedAndReturnState();
	return size;
}

int Sys_FileOpenWrite(char *filename)
{
	int handle;
	FILE *fp;
	int *errno_ptr;
	const char *error_string;

	VID_ForceLockState();
	handle = Sys_AllocFileHandle();
	fp = fopen(filename, "wb");
	if (!fp)
	{
		errno_ptr = _errno();
		error_string = strerror(*errno_ptr);
		Sys_Error("Error opening %s: %s", filename, error_string);
	}
	g_FileHandles[handle] = fp;
	VID_ForceUnlockedAndReturnState();
	return handle;
}

void Sys_FileClose(int handle)
{
	FILE **fp_ptr;

	VID_ForceLockState();
	fp_ptr = &g_FileHandles[handle];
	fclose(*fp_ptr);
	*fp_ptr = NULL;
	VID_ForceUnlockedAndReturnState();
}

void Sys_FileSeek(int handle, int offset)
{
	VID_ForceLockState();
	fseek(g_FileHandles[handle], offset, SEEK_SET);
	VID_ForceUnlockedAndReturnState();
}

size_t Sys_FileRead(int handle, void *buffer, size_t count)
{
	size_t bytes_read;

	VID_ForceLockState();
	bytes_read = fread(buffer, 1, count, g_FileHandles[handle]);
	VID_ForceUnlockedAndReturnState();
	return bytes_read;
}

size_t Sys_FileWrite(int handle, void *buffer, size_t count)
{
	size_t bytes_written;

	VID_ForceLockState();
	bytes_written = fwrite(buffer, 1, count, g_FileHandles[handle]);
	VID_ForceUnlockedAndReturnState();
	return bytes_written;
}

int Sys_FileTime(char *filename)
{
	FILE *fp;
	int result;

	VID_ForceLockState();
	fp = fopen(filename, "rb");
	result = -1;
	if (fp)
	{
		result = 1;
		fclose(fp);
	}
	VID_ForceUnlockedAndReturnState();
	return result;
}

int Sys_mkdir(char *path)
{
	return _mkdir(path);
}

int Sys_SetMemoryProtection(void *address, size_t size)
{
	DWORD old_protect;
	BOOL result;

	result = VirtualProtect(address, size, PAGE_READONLY, &old_protect);
	if (!result)
		Sys_Error("Protection change failed");
	return result;
}

int Sys_Printf(char *fmt, ...)
{
	va_list args;
	char buffer[1024];
	DWORD bytes_written;

	va_start(args, fmt);

	if (is_dedicated)
	{
		vsprintf(buffer, fmt, args);
		return WriteFile(h_console_output, buffer, strlen(buffer), &bytes_written, NULL);
	}

	return 0;
}

void Sys_Quit(void)
{
	VID_ForceLockState();
	Host_Shutdown();

	if (h_event)
		CloseHandle(h_event);

	if (is_dedicated)
		FreeConsole();

	Sys_CleanupDedicated();
	exit(0);
}

int Sys_PumpMessages(void)
{
	MSG msg;
	int result;

	for (result = PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE); result; result = PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		sleep_state = 0;
		if (!GetMessageA(&msg, NULL, 0, 0))
			Sys_Quit();
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return result;
}

void Sys_SendKeyEvents(void)
{
	(void)Sys_PumpMessages();
}

DWORD Sys_WaitMessage(DWORD timeout_ms)
{
	return MsgWaitForMultipleObjects(1, &h_event, FALSE, timeout_ms, QS_ALLINPUT);
}

char Sys_InitConProc(HANDLE hFileParam, HANDLE hParentParam, HANDLE hChildParam)
{
	DWORD ThreadId;

	if (!hFileParam || !hParentParam || !hChildParam)
		return (char)(intptr_t)hFileParam;

	g_hFileMappingObject = hFileParam;
	g_hParentEvent = hParentParam;
	g_hChildEvent = hChildParam;

	g_hConProcEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!g_hConProcEvent)
	{
		Con_Printf("Couldn't create heventDone\n");
		return (char)(intptr_t)hFileParam;
	}

	if (!CreateThread(NULL, 0, ConProcThreadFunc, NULL, 0, &ThreadId))
	{
		CloseHandle(g_hConProcEvent);
		Con_Printf("Couldn't create QHOST thread\n");
		return (char)(intptr_t)hFileParam;
	}

	g_hConProcConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	g_hConProcConsoleInput = GetStdHandle(STD_INPUT_HANDLE);

	return (char)Sys_ResizeConsoleBuffer(g_hConProcConsoleOutput, 80, 25);
}

BOOL Sys_UnmapViewOfFile(LPCVOID lpBaseAddress)
{
	return UnmapViewOfFile(lpBaseAddress);
}

BOOL Sys_ReadConsoleOutput(LPSTR out, int start_line, int end_line)
{
	DWORD number_of_chars_read;
	COORD read_coord;

	read_coord.X = 0;
	read_coord.Y = (SHORT)start_line;

	if (!ReadConsoleOutputCharacterA(
			g_hConProcConsoleOutput,
			out,
			(DWORD)(80 * (end_line - start_line + 1)),
			read_coord,
			&number_of_chars_read))
	{
		return FALSE;
	}

	out[number_of_chars_read] = 0;
	return TRUE;
}

BOOL Sys_SetConsoleHeight(int height)
{
	return Sys_ResizeConsoleBuffer(g_hConProcConsoleOutput, 80, height);
}

static WORD Sys_CharToScanCode(char c)
{
	unsigned char uc = (unsigned char)c;
	int upper = toupper(uc);

	if (uc == '\r')
		return 28;

	if (_isctype(uc, 0x103))
		return (WORD)(upper - 35);

	if (_isctype(uc, 4))
		return (WORD)(upper - 46);

	return (WORD)uc;
}

int Sys_WriteConsoleInput(char *text)
{
	char *p = text;
	INPUT_RECORD input;
	DWORD number_of_events_written;

	while (*p)
	{
		char ch = *p;
		int vk;
		int shift_pressed;

		if (ch == '\n')
			ch = '\r';

		vk = toupper((unsigned char)ch);

		input.EventType = KEY_EVENT;
		input.Event.KeyEvent.wRepeatCount = 1;
		input.Event.KeyEvent.bKeyDown = TRUE;
		input.Event.KeyEvent.wVirtualKeyCode = (WORD)vk;
		input.Event.KeyEvent.wVirtualScanCode = Sys_CharToScanCode(ch);
		input.Event.KeyEvent.uChar.AsciiChar = ch;
		input.Event.KeyEvent.uChar.UnicodeChar = (WCHAR)(unsigned char)ch;

		shift_pressed = _isctype((unsigned char)ch, 1);
		input.Event.KeyEvent.dwControlKeyState = shift_pressed ? 0x80 : 0;

		WriteConsoleInputA(g_hConProcConsoleInput, &input, 1, &number_of_events_written);
		input.Event.KeyEvent.bKeyDown = FALSE;
		WriteConsoleInputA(g_hConProcConsoleInput, &input, 1, &number_of_events_written);

		*p++ = ch;
	}

	return 1;
}

DWORD WINAPI ConProcThreadFunc(LPVOID lpParameter)
{
	HANDLE handles[2];

	(void)lpParameter;

	handles[0] = g_hParentEvent;
	handles[1] = g_hConProcEvent;

	while (WaitForMultipleObjects(2u, handles, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
	{
		char *view = (char *)Sys_MapViewOfFile(g_hFileMappingObject);
		int command;
		int result;

		if (!view)
		{
			Con_Printf("Invalid hFileBuffer\n");
			return 0;
		}

		command = *(int *)view;
		result = 0;

		if (command == 2)
		{
			result = Sys_WriteConsoleInput(view + 4);
		}
		else
		{
			switch (command)
			{
				case 3:
					result = Sys_ReadConsoleOutput(view + 4, *((int *)view + 1), *((int *)view + 2));
					break;
				case 4:
					result = Sys_GetConsoleHeight((DWORD *)(view + 4));
					break;
				case 5:
					result = Sys_SetConsoleHeight(*((int *)view + 1));
					break;
				default:
					result = 0;
					break;
			}
		}

		*(int *)view = result;
		Sys_UnmapViewOfFile(view);
		SetEvent(g_hChildEvent);
	}

	return 0;
}

LPVOID Sys_MapViewOfFile(HANDLE hFileMappingObject)
{
	return MapViewOfFile(hFileMappingObject, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
}

BOOL Sys_SetConProcEvent(void)
{
	extern HANDLE g_hConProcEvent;

	if (g_hConProcEvent)
		return SetEvent(g_hConProcEvent);

	return FALSE;
}

BOOL Sys_GetConsoleHeight(DWORD *height)
{
	extern HANDLE g_hConProcConsoleOutput;
	CONSOLE_SCREEN_BUFFER_INFO info;

	if (GetConsoleScreenBufferInfo(g_hConProcConsoleOutput, &info))
	{
		*height = info.dwSize.Y;
		return TRUE;
	}

	return FALSE;
}

BOOL Sys_ResizeConsoleBuffer(HANDLE hConsoleOutput, int width, int height)
{
	int actual_height;
	int actual_width;
	BOOL result;
	COORD largest_size;
	CONSOLE_SCREEN_BUFFER_INFO buffer_info;

	largest_size = GetLargestConsoleWindowSize(hConsoleOutput);

	actual_height = height;
	if (largest_size.Y < height)
		actual_height = largest_size.Y;

	actual_width = width;
	if (largest_size.X < width)
		actual_width = largest_size.X;

	if (!GetConsoleScreenBufferInfo(hConsoleOutput, &buffer_info))
		return FALSE;

	buffer_info.srWindow.Left = 0;
	buffer_info.srWindow.Right = buffer_info.dwSize.X - 1;
	buffer_info.srWindow.Top = 0;
	buffer_info.srWindow.Bottom = actual_height - 1;

	if (buffer_info.dwSize.Y <= actual_height)
	{
		if (buffer_info.dwSize.Y < actual_height)
		{
			buffer_info.dwSize.Y = actual_height;
			if (!SetConsoleScreenBufferSize(hConsoleOutput, buffer_info.dwSize))
				return FALSE;

			if (!SetConsoleWindowInfo(hConsoleOutput, TRUE, &buffer_info.srWindow))
				return FALSE;
		}
	}
	else
	{
		if (!SetConsoleWindowInfo(hConsoleOutput, TRUE, &buffer_info.srWindow))
			return FALSE;

		buffer_info.dwSize.Y = actual_height;
		if (!SetConsoleScreenBufferSize(hConsoleOutput, buffer_info.dwSize))
			return FALSE;
	}

	if (!GetConsoleScreenBufferInfo(hConsoleOutput, &buffer_info))
		return FALSE;

	buffer_info.srWindow.Left = 0;
	buffer_info.srWindow.Right = actual_width - 1;
	buffer_info.srWindow.Top = 0;
	buffer_info.srWindow.Bottom = buffer_info.dwSize.Y - 1;

	if (buffer_info.dwSize.X > actual_width)
	{
		if (!SetConsoleWindowInfo(hConsoleOutput, TRUE, &buffer_info.srWindow))
			return FALSE;

		buffer_info.dwSize.X = actual_width;
		return SetConsoleScreenBufferSize(hConsoleOutput, buffer_info.dwSize);
	}

	if (buffer_info.dwSize.X >= actual_width)
		return TRUE;

	buffer_info.dwSize.X = actual_width;
	if (!SetConsoleScreenBufferSize(hConsoleOutput, buffer_info.dwSize))
		return FALSE;

	result = SetConsoleWindowInfo(hConsoleOutput, TRUE, &buffer_info.srWindow);
	if (!result)
		return FALSE;

	return TRUE;
}
