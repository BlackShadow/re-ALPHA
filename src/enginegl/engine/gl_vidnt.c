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
#include <windows.h>
#include <gl/gl.h>
#include "quakedef.h"

viddef_t vid;
int glx, gly, glwidth, glheight;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

cvar_t vid_mode = { "vid_mode", "0" };
cvar_t vid_wait = { "vid_wait", "0" };
cvar_t vid_nopageflip = { "vid_nopageflip", "0" };
cvar_t vid_default_mode = { "vid_default_mode", "0" };
cvar_t vid_config_x = { "vid_config_x", "800" };
cvar_t vid_config_y = { "vid_config_y", "600" };

cvar_t gl_vsync = { "gl_vsync", "0" };
cvar_t gl_ztrick = { "gl_ztrick", "1" };
cvar_t gl_d3dflip = { "gl_d3dflip", "0" };
cvar_t gl_zfix = { "gl_zfix", "0" };

cvar_t _windowed_mouse = { "_windowed_mouse", "1" };

float gldepthmin, gldepthmax;

typedef struct
{
	int type;
	int width;
	int height;
	int flags;
	int fullscreen;
	int stretch;
	int bpp;
	int scale;
	char desc[20];
} vmode_t;

vmode_t gamemode_array[40];

int vid_desktop_width;
int vid_desktop_height;

int vid_nummodes;
int vid_windowed_mouse_last;
int vid_fullscreen_active;
int vid_validate_modes;
typedef struct
{
	int modeIndex;
	char *desc;
	int isActive;
} vid_menu_item_t;

vid_menu_item_t vid_menu_items[100];
int vid_menu_count;

static vmode_t vid_badmode;
static char vid_desc_string[100];
static char vid_desktop_string[100];

HWND g_hWnd;
HWND mainwindow;
HWND g_hWndSplash;
HDC g_hDC;
HGLRC g_hRC;
HINSTANCE g_hInstance;
LPARAM g_lParam;

int vid_initialized;
int vid_current_mode;
int vid_next_mode;

int vid_window_height;
int vid_window_width;
int vid_view_width;
int vid_view_height;
int vid_window_x;
int vid_window_y;
int vid_border_width;
int vid_border_height;
int vid_fullscreen_off_w;
int vid_fullscreen_off_h;

int scr_vrect_width;
int scr_vrect_height;
int vid_fullscreen_mode;
int scr_width = 320;
int scr_height = 200;
int vid_maxwarpwidth;
int vid_maxwarpheight;
int vid_buffer;
int vid_rowbytes;

BINDTEXFUNCPTR bindTexFunc;
int vid_skip_swap;

int app_active_state;
int app_active_flag;

RECT g_Rect;
int g_X, g_Y;
int window_center_x, window_center_y;
RECT window_rect;
int g_w, g_h;

char g_ClassName[] = "HalfLife";
char g_WindowName[] = "HalfLife-GL";

PIXELFORMATDESCRIPTOR g_ppfd =
{
	sizeof(PIXELFORMATDESCRIPTOR),
	1,
	PFD_DRAW_TO_WINDOW
	| PFD_SUPPORT_OPENGL
	| PFD_DOUBLEBUFFER,
	PFD_TYPE_RGBA,
	24,
	0, 0, 0, 0, 0, 0,
	0,
	0,
	0,
	0, 0, 0, 0,
	32,
	0,
	0,
	PFD_MAIN_PLANE,
	0,
	0, 0, 0
};

extern int vid_suppressModeChangePrint;

extern int scr_disabled_for_loading;
extern int vid_fullscreen;
extern void (*vid_menudrawfn)(void);
extern int (*vid_menukeyfn)(int key);

DEVMODEA g_DevMode;

unsigned char g_keymap[128] =
{
	0, 27, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', K_BACKSPACE, 9,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', 13, K_CTRL, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', K_SHIFT, '*',
	K_ALT, ' ', 0, K_F1, K_F2, K_F3, K_F4, K_F5,
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, 0, K_HOME,
	K_UPARROW, K_PGUP, '-', K_LEFTARROW, '5', K_RIGHTARROW, '+', K_END,
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, 0, K_F11,
	K_F12, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static HANDLE movie_file = (HANDLE)-1;
static char movie_filename[256];

char *VID_GetModeDescription(int modenum);
char *VID_GetModeDescription2(int modenum);
int VID_InitFullDIB(void);
int VID_UpdateWindowStatus(void);

extern void Sys_Error(const char *fmt, ...);
extern void Con_Printf(const char *fmt, ...);
extern void Con_DPrintf(const char *fmt, ...);
extern void Cvar_RegisterVariable(cvar_t *var);
extern int mouseinitialized;
extern int mouseactive;
extern BOOL IN_ActivateMouse(void);
extern BOOL IN_DeactivateMouse(void);
extern int IN_ShowMouse(void);
extern int IN_HideMouse(void);
extern int IN_ClearMouseState(void);
extern int IN_MouseEvent(int buttons);
extern void Key_Event(int key, int down);
extern void Key_ClearStates(void);
extern void S_BlockSound(void);
extern void S_UnblockSound(void);
extern void SCR_UpdateScreen(void);
extern void M_Menu_Options_f(void);
extern void S_LocalSound(const char *name);
extern void VID_HandlePause(void);
extern void CDAudio_Pause(void);
extern void CDAudio_Resume(void);
extern LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern void V_StartPitchDrift(void);
extern void V_StopPitchDrift(void);
extern void V_CalcBob(void);
extern void V_CalcRoll(void);
int ClearAllStates(void);
void AppActivate(int fActive, int minimized, int a3, int a4);
extern int VID_SetWindowedMode(int modenum);
extern int VID_SetFullscreenMode(int modenum);
extern void S_EndPrecaching(void);
extern void S_BeginPrecaching(void);
extern int bSetupPixelFormat(HDC hDC);

#define VID_WINDOWED     0
#define VID_FULLSCREEN   2

int VID_Stub_ReturnZero(void)
{
	return 0;
}

//=========================================================
// VID_HandlePause - OpenGL DD compat stub
//=========================================================
void VID_HandlePause(void)
{
}

//=========================================================
// VID_HandlePause2 - OpenGL DD compat stub
//=========================================================
void VID_HandlePause2(void)
{
}

//=========================================================
// VID_LockBuffer - OpenGL DD compat stub
//=========================================================
void VID_LockBuffer(void)
{
}

//=========================================================
// VID_UnlockBuffer - OpenGL DD compat stub
//=========================================================
void VID_UnlockBuffer(void)
{
}

//=========================================================
// VID_ForceLockState2 - OpenGL DD compat stub
//=========================================================
void VID_ForceLockState2(void)
{
}

//=========================================================
// VID_Shutdown_Stub - OpenGL video cleanup stub
//=========================================================
void VID_Shutdown_Stub(void)
{
}

//=========================================================
// D_BeginDirectRect - software-only stub
//=========================================================
void D_BeginDirectRect(void)
{
}

//=========================================================
// D_EndDirectRect - software-only stub
//=========================================================
void D_EndDirectRect(void)
{
}

//=========================================================
// GL_ErrorString_Null - OpenGL error string stub
//=========================================================
void GL_ErrorString_Null(void)
{
}

BOOL VID_CenterWindow(HWND hWnd, int width, int height)
{
	int centerX;
	int centerY;

	centerX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	centerY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

	if (2 * centerY < centerX)
		centerX >>= 1;

	if (centerX <= 0)
		centerX = 0;
	if (centerY <= 0)
		centerY = 0;

	return SetWindowPos(hWnd, NULL, centerX, centerY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

int VID_SetWindowedMode(int modenum)
{
	LONG height;
	HDC DC;
	RECT Rect;

	vid_border_width = 0;
	vid_border_height = 0;

	height = gamemode_array[modenum].height;
	g_w = gamemode_array[modenum].width;
	g_h = height;
	vid_window_width = g_w;
	vid_window_height = height;

	Rect.left = 0;
	Rect.top = 0;
	Rect.right = g_w;
	Rect.bottom = height;
	AdjustWindowRectEx(&Rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

	g_hWnd = CreateWindowExA(
		0,
		g_ClassName,
		g_WindowName,
		WS_OVERLAPPEDWINDOW,
		Rect.left,
		Rect.top,
		Rect.right - Rect.left,
		Rect.bottom - Rect.top,
		NULL,
		NULL,
		g_hInstance,
		NULL);

	if (!g_hWnd)
		Sys_Error("Couldn't create DIB window");

	mainwindow = g_hWnd;

	VID_CenterWindow(g_hWnd, g_w - vid_border_width, g_h - vid_border_height);
	ShowWindow(g_hWnd, SW_SHOWDEFAULT);
	UpdateWindow(g_hWnd);

	vid_fullscreen = 0;
	DC = GetDC(g_hWnd);
	PatBlt(DC, 0, 0, g_w, g_h, BLACKNESS);
	ReleaseDC(g_hWnd, DC);

	vid_fullscreen_mode = 2;
	scr_height = 200;
	scr_vrect_height = 200;
	scr_width = 320;
	scr_vrect_width = 320;
	vid.width = 320;
	vid.height = 200;
	vid.conwidth = 320;
	vid.conheight = 200;

	SendMessageA(g_hWnd, WM_SETICON, ICON_BIG, g_lParam);
	SendMessageA(g_hWnd, WM_SETICON, ICON_SMALL, g_lParam);

	return 1;
}

int VID_SetFullscreenMode(int modenum)
{
	DWORD bpp;
	BYTE scale;
	DWORD dmWidth;
	DWORD dmHeight;
	LONG height;
	HDC DC;
	RECT Rect;

	if (!vid_fullscreen_active)
	{
		g_DevMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		bpp = gamemode_array[modenum].bpp;
		scale = gamemode_array[modenum].scale;
		g_DevMode.dmSize = sizeof(DEVMODEA);
		g_DevMode.dmBitsPerPel = bpp;
		dmWidth = gamemode_array[modenum].width << scale;
		dmHeight = gamemode_array[modenum].height;
		g_DevMode.dmPelsWidth = dmWidth;
		g_DevMode.dmPelsHeight = dmHeight;

		if (ChangeDisplaySettingsA(&g_DevMode, CDS_FULLSCREEN))
			Sys_Error("Couldn't set fullscreen DIB mode");
	}

	vid_border_width = 0;
	vid_border_height = 0;
	vid_fullscreen = 2;

	height = gamemode_array[modenum].height;
	g_w = gamemode_array[modenum].width;
	g_h = height;
	vid_window_width = g_w;
	vid_window_height = height;

	Rect.left = 0;
	Rect.top = 0;
	Rect.right = g_w;
	Rect.bottom = height;
	AdjustWindowRectEx(&Rect, WS_POPUP, FALSE, 0);

	g_hWnd = CreateWindowExA(
		0,
		"HalfLife",
		"HalfLife-GL",
		WS_POPUP,
		Rect.left,
		Rect.top,
		Rect.right - Rect.left,
		Rect.bottom - Rect.top,
		NULL,
		NULL,
		g_hInstance,
		NULL);

	if (!g_hWnd)
		Sys_Error("Couldn't create DIB window");

	mainwindow = g_hWnd;

	ShowWindow(g_hWnd, SW_SHOWDEFAULT);
	UpdateWindow(g_hWnd);

	DC = GetDC(g_hWnd);
	PatBlt(DC, 0, 0, g_w, g_h, BLACKNESS);
	ReleaseDC(g_hWnd, DC);

	vid_fullscreen_mode = 2;
	vid_fullscreen_off_w = 0;
	scr_height = 200;
	scr_vrect_height = 200;
	vid_fullscreen_off_h = 0;
	scr_width = 320;
	scr_vrect_width = 320;
	vid.width = 320;
	vid.height = 200;
	vid.conwidth = 320;
	vid.conheight = 200;

	SendMessageA(g_hWnd, WM_SETICON, ICON_BIG, g_lParam);
	SendMessageA(g_hWnd, WM_SETICON, ICON_SMALL, g_lParam);

	return 1;
}

int VID_SetMode(int modenum)
{
	int oldInRefresh;
	int modeType;
	int success;
	MSG Msg;
	float fModeNum;

	if (vid_validate_modes && modenum || !vid_validate_modes && (modenum < 1 || vid_nummodes <= modenum))
		Sys_Error("Bad video mode\n");

	oldInRefresh = scr_disabled_for_loading;
	scr_disabled_for_loading = 1;

	S_EndPrecaching();
	CDAudio_Pause();

	modeType = gamemode_array[modenum].type;

	if (modeType)
	{
		if (modeType != 2)
			Sys_Error("VID_SetMode: Bad mode type");

		success = VID_SetFullscreenMode(modenum);
		(void)IN_ActivateMouse();
		IN_HideMouse();
	}
	else if (_windowed_mouse.value == 0.0f)
	{
		(void)IN_DeactivateMouse();
		IN_ShowMouse();
		success = VID_SetWindowedMode(modenum);
	}
	else
	{
		success = VID_SetWindowedMode(modenum);
		(void)IN_ActivateMouse();
		IN_HideMouse();
	}

	vid_view_width = vid_window_width;
	vid_view_height = vid_window_height;
	glx = 0;
	gly = 0;
	glwidth = vid_view_width;
	glheight = vid_view_height;

	VID_UpdateWindowStatus();
	CDAudio_Resume();
	S_BeginPrecaching();

	scr_disabled_for_loading = oldInRefresh;

	if (!success)
		Sys_Error("Couldn't set video mode");

	SetForegroundWindow(g_hWnd);
	vid_current_mode = modenum;

	fModeNum = (float)modenum;
	Cvar_SetValue("vid_mode", fModeNum);

	while (PeekMessageA(&Msg, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&Msg);
		DispatchMessageA(&Msg);
	}

	Sleep(100);
	SetWindowPos(g_hWnd, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
	SetForegroundWindow(g_hWnd);
	ClearAllStates();

	if (!vid_suppressModeChangePrint)
	{
		char *modeDesc = VID_GetModeDescription(vid_current_mode);
		Con_DPrintf("%s\n", modeDesc);
	}

	vid.recalc_refdef = 1;
	return 1;
}

int VID_UpdateWindowStatus(void)
{
	int result;

	g_Rect.left = vid_fullscreen_off_w;
	g_Rect.top = vid_fullscreen_off_h;
	g_Rect.right = vid_fullscreen_off_w + vid_view_width;
	g_Rect.bottom = vid_fullscreen_off_h + vid_view_height;

	g_X = (vid_fullscreen_off_w + vid_fullscreen_off_w + vid_view_width) / 2;
	result = (vid_fullscreen_off_h + vid_fullscreen_off_h + vid_view_height) / 2;
	g_Y = result;

	window_rect = g_Rect;
	window_center_x = g_X;
	window_center_y = g_Y;

	if (mouseinitialized && mouseactive)
		return ClipCursor(&g_Rect);

	return result;
}

FARPROC CheckTextureExtensions(void)
{
	int hasExtension;
	const char *extensions;
	FARPROC result;
	HMODULE hLib;

	hasExtension = 0;
	extensions = (const char *)glGetString(GL_EXTENSIONS);

	for (; *extensions; ++extensions)
	{
		if (!strncmp(extensions, "GL_EXT_texture_object", 21))
			hasExtension = 1;
	}

	if (!hasExtension || COM_CheckParm("-gl11"))
	{
		hLib = LoadLibraryA("opengl32.dll");
		if (!hLib)
			Sys_Error("Couldn't load opengl32.dll");

		result = GetProcAddress(hLib, "glBindTexture");
		bindTexFunc = (BINDTEXFUNCPTR)result;

		if (!result)
			Sys_Error("No texture object support in GL");
	}
	else
	{
		result = wglGetProcAddress("glBindTextureEXT");
		bindTexFunc = (BINDTEXFUNCPTR)result;

		if (!result)
			Sys_Error("GetProcAddress failed for glBindTextureEXT");
	}

	return result;
}

void GL_Init(void)
{
	gl_vendor = (const char *)glGetString(GL_VENDOR);
	gl_renderer = (const char *)glGetString(GL_RENDERER);
	gl_version = (const char *)glGetString(GL_VERSION);
	gl_extensions = (const char *)glGetString(GL_EXTENSIONS);

	Con_DPrintf("GL_VENDOR: %s\n", gl_vendor);
	Con_DPrintf("GL_RENDERER: %s\n", gl_renderer);
	Con_DPrintf("GL_VERSION: %s\n", gl_version);
	Con_DPrintf("GL_EXTENSIONS: %s\n", gl_extensions);

	CheckTextureExtensions();

	glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void VID_GetWindowSize(int *x, int *y, int *width, int *height)
{
	*y = 0;
	*x = 0;
	*width = g_w - vid_window_x;
	*height = g_h - vid_window_y;
}

void GL_EndRendering(void)
{
	if (!vid_skip_swap)
	{
		SwapBuffers(g_hDC);
	}

	if (!vid_fullscreen)
	{
		if ((int)_windowed_mouse.value != vid_windowed_mouse_last)
		{
			if (_windowed_mouse.value == 0.0f)
			{
				(void)IN_DeactivateMouse();
				IN_ShowMouse();
			}
			else
			{
				(void)IN_ActivateMouse();
				IN_HideMouse();
			}
			vid_windowed_mouse_last = (int)_windowed_mouse.value;
		}
	}
}

void VID_Shutdown(void)
{
	HGLRC CurrentContext;
	HDC CurrentDC;

	if (vid_initialized)
	{
		CurrentContext = wglGetCurrentContext();
		CurrentDC = wglGetCurrentDC();
		wglMakeCurrent(NULL, NULL);

		if (CurrentContext)
			wglDeleteContext(CurrentContext);

		if (CurrentDC && g_hWnd)
			ReleaseDC(g_hWnd, CurrentDC);

		if (vid_fullscreen == 2)
			ChangeDisplaySettingsA(NULL, 0);

		if (g_hDC)
		{
			if (g_hWnd)
				ReleaseDC(g_hWnd, g_hDC);
		}

		AppActivate(0, 0, 0, 0);
	}
}

int bSetupPixelFormat(HDC hdc)
{
	int pixelFormat;

	pixelFormat = ChoosePixelFormat(hdc, &g_ppfd);
	if (pixelFormat)
	{
		if (SetPixelFormat(hdc, pixelFormat, &g_ppfd))
			return 1;
		MessageBoxA(NULL, "SetPixelFormat failed", "Error", MB_OK);
	}
	else
	{
		MessageBoxA(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
	}

	return 0;
}

int MapKey(int key)
{
	if (((key & 0xFF0000) >> 16) <= 0x7F)
		return g_keymap[(key & 0xFF0000) >> 16];
	else
		return 0;
}

int ClearAllStates(void)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		Key_Event(i, 0);
	}

	Key_ClearStates();
	return IN_ClearMouseState();
}

void AppActivate(int fActive, int minimized, int a3, int a4)
{
	app_active_state = a4;
	app_active_flag = a3;

	static int sound_blocked = 0;

	if (a3)
	{
		if (!sound_blocked)
		{
			S_UnblockSound();
			sound_blocked = 1;
		}
	}
	else
	{
		if (sound_blocked)
		{
			S_BlockSound();
			sound_blocked = 0;
		}
	}

	if (a3)
	{
		if (vid_fullscreen == 2 || (!vid_fullscreen && _windowed_mouse.value != 0.0f))
		{
			(void)IN_ActivateMouse();
			IN_HideMouse();
		}

		if (a3)
			return;
	}

	if (vid_fullscreen == 2 || (!vid_fullscreen && _windowed_mouse.value != 0.0f))
	{
		(void)IN_DeactivateMouse();
		IN_ShowMouse();
	}
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	int key;
	int buttonState;

	if (Msg > WM_CLOSE)
	{
		if (Msg <= WM_LBUTTONUP)
		{
			if (Msg < WM_MOUSEMOVE)
			{
				if (Msg != WM_KEYDOWN)
				{
					if (Msg == WM_KEYUP)
						goto keyup;

					if (Msg != WM_SYSKEYDOWN)
					{
						if (Msg != WM_SYSKEYUP)
						{
							if (Msg != WM_SYSCHAR)
								return DefWindowProcA(hWnd, Msg, wParam, lParam);
							return 1;
						}

					keyup:
						key = MapKey(lParam);
						Key_Event(key, 0);
						return 1;
					}
				}

				key = MapKey(lParam);
				Key_Event(key, 1);
				return 1;
			}

		mousebutton:
			buttonState = (wParam & MK_LBUTTON) != 0;
			if (wParam & MK_RBUTTON)
				buttonState |= 2;
			if (wParam & MK_MBUTTON)
				buttonState |= 4;
			IN_MouseEvent(buttonState);
			return 1;
		}

		if (Msg >= WM_RBUTTONDOWN)
		{
			if (Msg <= WM_RBUTTONUP)
				goto mousebutton;
			if (Msg >= WM_MBUTTONDOWN)
			{
				if (Msg <= WM_MBUTTONUP)
					goto mousebutton;
				if (Msg == 953)
					return CDAudio_MessageHandler(hWnd, Msg, wParam, lParam);
			}
		}

		return DefWindowProcA(hWnd, Msg, wParam, lParam);
	}

	if (Msg == WM_CLOSE)
	{
		if (MessageBoxA(g_hWnd, "Are you sure you want to quit?", "Confirm Exit", MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
			Sys_Quit();
	}
	else if (Msg != WM_CREATE)
	{
		if (Msg == WM_DESTROY)
		{
			if (g_hWnd)
				DestroyWindow(g_hWnd);
			PostQuitMessage(0);
		}
		else if (Msg == WM_MOVE)
		{
			vid_fullscreen_off_w = (lParam & 0xFFFF);
			vid_fullscreen_off_h = ((lParam >> 16) & 0xFFFF);
			VID_UpdateWindowStatus();
		}
		else if (Msg != WM_SIZE)
		{
			if (Msg != WM_ACTIVATE)
				return DefWindowProcA(hWnd, Msg, wParam, lParam);

			AppActivate((wParam & 0xFFFF), ((wParam >> 16) & 0xFFFF), (wParam & 0xFFFF) != 0, ((wParam >> 16) & 0xFFFF));
			ClearAllStates();
		}
	}

	return 1;
}

int VID_NumModes(void)
{
	return vid_nummodes;
}

vmode_t *VID_GetModePtr(int modenum)
{
	if (modenum < 0 || modenum >= vid_nummodes)
		return &vid_badmode;

	return &gamemode_array[modenum];
}

char *VID_GetModeDescription(int modenum)
{
	if (modenum < 0 || modenum >= vid_nummodes)
		return NULL;

	if (!vid_fullscreen_active)
		return gamemode_array[modenum].desc;

	sprintf(vid_desktop_string, "Desktop resolution (%dx%d)", vid_desktop_width, vid_desktop_height);
	return vid_desktop_string;
}

char *VID_GetModeDescription2(int modenum)
{
	vmode_t *mode;

	if (modenum < 0 || modenum >= vid_nummodes)
		return NULL;

	mode = &gamemode_array[modenum];

	if (mode->type == 2)
	{
		if (vid_fullscreen_active)
			sprintf(vid_desc_string, "Desktop resolution (%dx%d)", vid_desktop_width, vid_desktop_height);
		else
			sprintf(vid_desc_string, "%s fullscreen", mode->desc);
	}
	else if (vid_fullscreen)
	{
		sprintf(vid_desc_string, "Windowed");
	}
	else
	{
		sprintf(vid_desc_string, "%s windowed", mode->desc);
	}

	return vid_desc_string;
}

void VID_DescribeCurrentMode_f(void)
{
	Con_Printf("%s\n", VID_GetModeDescription2(vid_current_mode));
}

void VID_NumModes_f(void)
{
	if (vid_nummodes == 1)
		Con_Printf("%d video mode is available\n", 1);
	else
		Con_Printf("%d video modes are available\n", vid_nummodes);
}

void VID_DescribeMode_f(void)
{
	int modenum;
	int savedDesktopFlag;
	char *desc;

	modenum = Q_atoi(Cmd_Argv(1));
	savedDesktopFlag = vid_fullscreen_active;
	vid_fullscreen_active = 0;
	desc = VID_GetModeDescription2(modenum);
	Con_Printf("%s\n", desc);
	vid_fullscreen_active = savedDesktopFlag;
}

void VID_DescribeModes_f(void)
{
	int i;
	int numModes;
	int savedDesktopFlag;

	i = 1;
	numModes = VID_NumModes();
	savedDesktopFlag = vid_fullscreen_active;
	vid_fullscreen_active = 0;

	if (numModes > 1)
	{
		do
		{
			VID_GetModePtr(i);
			const char *desc = VID_GetModeDescription2(i);
			Con_Printf("%2d: %s\n", i, desc);
			++i;
		} while (i < numModes);
	}

	vid_fullscreen_active = savedDesktopFlag;
}

BOOL VID_TakeSnapshot(LPCSTR lpFileName)
{
	int size;
	HANDLE hFile;
	char *buffer;
	char *ptr;
	char *end;
	char temp;
	int count;
	DWORD bytesWritten;

	struct {
		short bfType;
		int bfSize;
		short bfReserved1;
		short bfReserved2;
		int bfOffBits;
	} bmfh;

	struct {
		int biSize;
		int biWidth;
		int biHeight;
		short biPlanes;
		short biBitCount;
		int biCompression;
		int biSizeImage;
		int biXPelsPerMeter;
		int biYPelsPerMeter;
		int biClrUsed;
		int biClrImportant;
	} bmih;

	size = 3 * vid_window_width * vid_window_height;

	hFile = CreateFileA(lpFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		Sys_Error("Couldn't create file %s", lpFileName);

	bmfh.bfType = 0x4D42;
	bmfh.bfSize = size + 54;
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = 54;

	if (!WriteFile(hFile, &bmfh, 14, &bytesWritten, NULL))
		Sys_Error("Couldn't write file header");

	bmih.biSize = 40;
	bmih.biWidth = vid_window_width;
	bmih.biHeight = vid_window_height;
	bmih.biPlanes = 1;
	bmih.biBitCount = 24;
	bmih.biCompression = 0;
	bmih.biSizeImage = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;

	if (!WriteFile(hFile, &bmih, 40, &bytesWritten, NULL))
		Sys_Error("Couldn't write bitmap header");

	buffer = (char *)malloc(size);
	if (!buffer)
		Sys_Error("Couldn't allocate memory for screenshot");

	glReadPixels(0, 0, vid_window_width, vid_window_height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	ptr = buffer;
	end = buffer + 2;
	count = vid_window_width * vid_window_height;
	if (count > 0)
	{
		do
		{
			temp = *ptr;
			*ptr = *end;
			ptr += 3;
			*end = temp;
			end += 3;
			--count;
		} while (count);
	}

	if (!WriteFile(hFile, buffer, size, &bytesWritten, NULL))
		Sys_Error("Couldn't write bitmap data");

	free(buffer);

	if (!CloseHandle(hFile))
		Sys_Error("Couldn't close file");

	return TRUE;
}

BOOL VID_WriteBuffer(const char *filename)
{
	DWORD createMode;
	DWORD frameSize;
	void *pixelBuffer;
	BOOL result;
	DWORD blockHeader[2];
	WORD frameHeader[4];
	LONG seekHigh;

	createMode = OPEN_EXISTING;
	frameSize = 3 * vid_window_width * vid_window_height;

	if (filename)
	{
		strcpy(movie_filename, filename);
		if (movie_file != (HANDLE)-1)
			CloseHandle(movie_file);
		createMode = CREATE_ALWAYS;
	}

	movie_file = CreateFileA(movie_filename, GENERIC_WRITE, 0, 0, createMode, FILE_ATTRIBUTE_NORMAL, 0);
	if (movie_file == (HANDLE)-1)
		Sys_Error("Couldn't open movie file");

	seekHigh = 0;
	SetFilePointer(movie_file, 0, &seekHigh, FILE_END);

	frameHeader[0] = (WORD)vid_window_width;
	frameHeader[1] = (WORD)vid_window_height;
	frameHeader[2] = 24;

	blockHeader[0] = 0x4D524D46;
	blockHeader[1] = frameSize + 6;

	pixelBuffer = malloc(3 * vid_window_width * vid_window_height);
	if (!pixelBuffer)
		Sys_Error("Couldn't allocate memory for movie frame");

	glReadPixels(0, 0, vid_window_width, vid_window_height, GL_RGB, GL_UNSIGNED_BYTE, pixelBuffer);

	if (!WriteFile(movie_file, blockHeader, 8, (LPDWORD)&seekHigh, 0))
		Sys_Error("Couldn't write block header");

	if (!WriteFile(movie_file, frameHeader, 6, (LPDWORD)&seekHigh, 0))
		Sys_Error("Couldn't write frame header");

	if (!WriteFile(movie_file, pixelBuffer, frameSize, (LPDWORD)&seekHigh, 0))
		Sys_Error("Couldn't write frame data");

	free(pixelBuffer);

	result = CloseHandle(movie_file);
	if (!result)
		Sys_Error("Couldn't close file");

	movie_file = (HANDLE)-1;
	return result;
}

int VID_MenuKey(int key)
{
	if (key == K_ESCAPE)
	{
		S_LocalSound("misc/menu1.wav");
		M_Menu_Options_f();
	}
	return 0;
}

void VID_MenuDraw(void)
{
	void *pic;
	int modeIndex;
	int picWidth;
	int numModes;
	void *modeDesc;
	int x, y;
	int row;

	pic = Draw_CachePic("gfx/vidmodes.lmp");
	picWidth = *(int *)pic;
	Draw_Pic((320 - picWidth) / 2, 4, pic);

	vid_menu_count = 0;
	numModes = VID_NumModes();

	if (numModes > 1)
	{
		modeIndex = 1;
		do
		{
			if (vid_menu_count >= 27)
				break;

			modeDesc = VID_GetModeDescription(modeIndex);
			vid_menu_items[vid_menu_count].modeIndex = modeIndex;
			vid_menu_items[vid_menu_count].desc = (char *)modeDesc;
			vid_menu_items[vid_menu_count].isActive = 0;

			if (modeIndex == vid_current_mode)
				vid_menu_items[vid_menu_count].isActive = 1;

			++modeIndex;
			++vid_menu_count;
		} while (modeIndex < numModes);
	}

	row = 0;
	if (vid_menu_count > 0)
	{
		x = 8;
		y = 52;
		M_Print(16, 36, "Fullscreen Modes");

		vid_menu_item_t *menuItem = vid_menu_items;
		do
		{
			if (menuItem->isActive)
				M_PrintWhite(x, y, menuItem->desc);
			else
				M_Print(x, y, menuItem->desc);

			x += 104;
			if (row % 3 == 2)
			{
				x = 8;
				y += 8;
			}

			menuItem++;
			++row;
		} while (row < vid_menu_count);
	}

	M_Print(24, 140, "Video modes must be set from the");
	M_Print(24, 148, "command line with -width <width>");
	M_Print(24, 156, "and -bpp <bits-per-pixel>");
	M_Print(24, 172, "Select windowed mode with -window");
}

int VID_RegisterWindowClass(HINSTANCE hInstance)
{
	WNDCLASSA wc;
	int height;

	wc.hInstance = hInstance;
	wc.style = 0;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = 0;
	wc.lpfnWndProc = MainWndProc;
	wc.hCursor = LoadCursorA(0, (LPCSTR)IDC_ARROW);
	wc.hbrBackground = 0;
	wc.lpszMenuName = 0;
	wc.lpszClassName = "HalfLife";

	if (!RegisterClassA(&wc))
		Sys_Error("Couldn't register window class");

	gamemode_array[0].type = 0;

	if (COM_CheckParm("-width"))
	{
		int widthParm = COM_CheckParm("-width");
		gamemode_array[0].width = Q_atoi(com_argv[widthParm + 1]);
	}
	else
	{
		gamemode_array[0].width = 640;
	}

	if (gamemode_array[0].width < 320)
		gamemode_array[0].width = 320;

	if (COM_CheckParm("-height"))
	{
		int heightParm = COM_CheckParm("-height");
		height = Q_atoi(com_argv[heightParm + 1]);
	}
	else
	{
		height = 240 * gamemode_array[0].width / 320;
	}

	gamemode_array[0].height = height;
	if (height < 240)
		gamemode_array[0].height = 240;

	sprintf(gamemode_array[0].desc, "%dx%d", gamemode_array[0].width, gamemode_array[0].height);

	gamemode_array[0].flags = 0;
	gamemode_array[0].fullscreen = 1;
	gamemode_array[0].stretch = 0;
	gamemode_array[0].scale = 0;
	gamemode_array[0].bpp = 0;
	vid_nummodes = 1;

	return 0;
}

int VID_InitFullDIB(void)
{
	int baseNumModes;
	int result;
	DWORD dmWidth, dmHeight, dmBpp;
	int modeOffset;
	DEVMODEA dm;
	DWORD iModeNum;
	int duplicate;

	iModeNum = 0;
	baseNumModes = vid_nummodes;

	do
	{
		result = EnumDisplaySettingsA(0, iModeNum, &dm);
		if (dm.dmBitsPerPel >= 15 && dm.dmPelsWidth <= 10000 && dm.dmPelsHeight <= 10000 && vid_nummodes < 30)
		{
			dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
			result = ChangeDisplaySettingsA(&dm, CDS_TEST);

			if (!result)
			{
				dmWidth = dm.dmPelsWidth;
				dmHeight = dm.dmPelsHeight;

				gamemode_array[vid_nummodes].type = 2;
				dmBpp = dm.dmBitsPerPel;
				gamemode_array[vid_nummodes].width = dmWidth;
				gamemode_array[vid_nummodes].height = dmHeight;
				gamemode_array[vid_nummodes].flags = 0;
				gamemode_array[vid_nummodes].scale = 0;
				gamemode_array[vid_nummodes].fullscreen = 1;
				gamemode_array[vid_nummodes].stretch = 1;
				gamemode_array[vid_nummodes].bpp = dmBpp;

				sprintf(gamemode_array[vid_nummodes].desc, "%dx%dx%d", dmWidth, dmHeight, dmBpp);

				result = COM_CheckParm("-noadjustaspect");
				if (!result)
				{
					if (2 * gamemode_array[vid_nummodes].height < gamemode_array[vid_nummodes].width)
					{
						gamemode_array[vid_nummodes].width >>= 1;
						gamemode_array[vid_nummodes].scale = 1;
						sprintf(gamemode_array[vid_nummodes].desc, "%dx%dx%d",
							gamemode_array[vid_nummodes].width,
							gamemode_array[vid_nummodes].height,
							gamemode_array[vid_nummodes].bpp);
					}
				}

				duplicate = 0;

				if (baseNumModes < vid_nummodes)
				{
					for (modeOffset = baseNumModes; modeOffset < vid_nummodes; modeOffset++)
					{
						if (gamemode_array[modeOffset].width == gamemode_array[vid_nummodes].width &&
							gamemode_array[modeOffset].height == gamemode_array[vid_nummodes].height &&
							gamemode_array[modeOffset].bpp == gamemode_array[vid_nummodes].bpp)
						{
							duplicate = 1;
							break;
						}
					}
				}

				if (!duplicate)
					++vid_nummodes;
			}
		}
		++iModeNum;
	} while (result);

	if (baseNumModes == vid_nummodes)
	{
		Con_Printf("No fullscreen DIB modes found\n");
		return 0;
	}

	return result;
}

int VID_Init(void)
{
	HDC DC;
	int widthVal, heightVal, bppVal;
	int forcedBpp;
	int foundMode;

	Cvar_RegisterVariable(&vid_mode);
	Cvar_RegisterVariable(&vid_wait);
	Cvar_RegisterVariable(&vid_nopageflip);
	Cvar_RegisterVariable(&vid_default_mode);
	Cvar_RegisterVariable(&vid_config_x);
	Cvar_RegisterVariable(&vid_config_y);
	Cvar_RegisterVariable(&gl_vsync);
	Cvar_RegisterVariable(&gl_ztrick);
	Cvar_RegisterVariable(&gl_d3dflip);
	Cvar_RegisterVariable(&gl_zfix);
	Cvar_RegisterVariable(&_windowed_mouse);

	Cmd_AddCommand("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand("vid_describemodes", VID_DescribeModes_f);

	g_lParam = (LPARAM)LoadIconA(g_hInstance, (LPCSTR)1);

	VID_RegisterWindowClass(g_hInstance);
	vid_nummodes = 1;
	VID_InitFullDIB();

	if (COM_CheckParm("-window"))
	{
		DC = GetDC(0);
		if ((GetDeviceCaps(DC, RASTERCAPS) & RC_PALETTE) != 0)
			Sys_Error("Can't run in non-RGB mode");
		ReleaseDC(0, DC);

		vid_validate_modes = 1;
		vid_next_mode = 0;
		goto INIT_COMPLETE;
	}

	if (vid_nummodes == 1)
		Sys_Error("No RGB fullscreen modes available");

	vid_validate_modes = 0;

	if (COM_CheckParm("-mode"))
	{
		int modeParm = COM_CheckParm("-mode");
		vid_next_mode = Q_atoi(com_argv[modeParm + 1]);
		goto INIT_COMPLETE;
	}

	if (COM_CheckParm("-current"))
	{
		vid_desktop_width = GetSystemMetrics(SM_CXSCREEN);
		vid_desktop_height = GetSystemMetrics(SM_CYSCREEN);
		vid_next_mode = 1;
		vid_fullscreen_active = 1;
		goto INIT_COMPLETE;
	}

	if (COM_CheckParm("-width"))
	{
		int widthParm = COM_CheckParm("-width");
		widthVal = Q_atoi(com_argv[widthParm + 1]);
	}
	else
	{
		widthVal = 640;
	}

	if (COM_CheckParm("-bpp"))
	{
		int bppParm = COM_CheckParm("-bpp");
		bppVal = Q_atoi(com_argv[bppParm + 1]);
		forcedBpp = 0;
	}
	else
	{
		forcedBpp = 1;
		bppVal = 15;
	}

	if (COM_CheckParm("-height"))
	{
		int heightParm = COM_CheckParm("-height");
		heightVal = Q_atoi(com_argv[heightParm + 1]);
	}
	else
	{
		heightVal = 480;
	}

	if (COM_CheckParm("-force") && vid_nummodes < 30)
	{
		vmode_t *mode = &gamemode_array[vid_nummodes];
		mode->type = 2;
		mode->width = widthVal;
		mode->height = heightVal;
		mode->flags = 0;
		mode->fullscreen = 1;
		mode->stretch = 1;
		mode->bpp = bppVal;
		mode->scale = 0;
		sprintf(mode->desc, "%dx%dx%d", widthVal, heightVal, bppVal);

		vid_next_mode = vid_nummodes++;
	}

	foundMode = 0;
	while (!foundMode)
	{
		vid_next_mode = 0;

		if (COM_CheckParm("-height"))
		{
			const int heightArg = COM_CheckParm("-height");
			const int wantedHeight = Q_atoi(com_argv[heightArg + 1]);

			for (int i = 1; i < vid_nummodes; ++i)
			{
				if (gamemode_array[i].width == widthVal && gamemode_array[i].height == wantedHeight && gamemode_array[i].bpp == bppVal)
				{
					vid_next_mode = i;
					foundMode = 1;
					break;
				}
			}
		}
		else
		{
			for (int i = 1; i < vid_nummodes; ++i)
			{
				if (gamemode_array[i].width == widthVal && gamemode_array[i].bpp == bppVal)
				{
					vid_next_mode = i;
					foundMode = 1;
					break;
				}
			}
		}

		if (foundMode)
			break;

		if (!forcedBpp)
			break;

		switch (bppVal)
		{
		case 15:
			bppVal = 16;
			break;
		case 16:
			bppVal = 32;
			break;
		case 32:
			bppVal = 24;
			break;
		case 24:
			forcedBpp = 0;
			break;
		}
	}

	if (!vid_next_mode)
		Sys_Error("Specified video mode not available");

INIT_COMPLETE:
	vid_initialized = 1;
	vid_maxwarpwidth = 320;
	vid_maxwarpheight = 200;

	vid.colormap = (pixel_t *)host_colormap;
	vid.fullbright = 256 - Q_strlen((const char *)((byte *)host_colormap + 0x2000));
	vid_buffer = 0;
	vid_rowbytes = 0;

	DestroyWindow(g_hWndSplash);
	VID_SetMode(vid_next_mode);

	g_hDC = GetDC(g_hWnd);
	bSetupPixelFormat(g_hDC);
	g_hRC = wglCreateContext(g_hDC);
	if (!g_hRC)
	{
		Sys_Error("wglCreateContext failed");
	}
	if (!wglMakeCurrent(g_hDC, g_hRC))
	{
		Sys_Error("wglMakeCurrent failed");
	}

	GL_Init();

	char glquakePath[MAX_OSPATH];
	sprintf(glquakePath, "%s/glquake", com_gamedir);
	Sys_mkdir(glquakePath);

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy(vid_badmode.desc, "Bad mode");

	return vid_current_mode;
}

void VID_ShiftPalette(void)
{
}
