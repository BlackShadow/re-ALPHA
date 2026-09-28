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
// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#include "winquake.h"
#include "nullsubs.h"

#define MAX_MODE_LIST		40
#define MAX_FULLDIB_MODES	30		// modelist entries filled by VID_InitFullDIB
#define MAXWIDTH			10000
#define MAXHEIGHT			10000

#define MODE_WINDOWED		0
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

// modestate / vmode_t type
#define MS_WINDOWED			0
#define MS_FULLDIB			2

#define MAX_COLUMN_SIZE		9
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 2)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE * 3)
#define VID_ROW_SIZE		3

#define MOVIE_BLOCK_ID		(('M' << 24) | ('R' << 16) | ('M' << 8) | 'F')

viddef_t	vid;	// global video state
int			glx, gly, glwidth, glheight;

const char	*gl_vendor;
const char	*gl_renderer;
const char	*gl_version;
const char	*gl_extensions;

cvar_t		vid_mode = {"vid_mode", "0"};
cvar_t		vid_wait = {"vid_wait", "0"};
cvar_t		vid_nopageflip = {"vid_nopageflip", "0"};
cvar_t		vid_default_mode = {"vid_default_mode", "0"};
cvar_t		vid_config_x = {"vid_config_x", "800"};
cvar_t		vid_config_y = {"vid_config_y", "600"};

cvar_t		gl_vsync = {"gl_vsync", "0"};
cvar_t		gl_ztrick = {"gl_ztrick", "1"};
cvar_t		gl_d3dflip = {"gl_d3dflip", "0"};
cvar_t		gl_zfix = {"gl_zfix", "0"};

cvar_t		_windowed_mouse = {"_windowed_mouse", "1"};

float		gldepthmin, gldepthmax;

typedef struct
{
	int			type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[20];
} vmode_t;

vmode_t		modelist[MAX_MODE_LIST];

int			vid_desktop_width;
int			vid_desktop_height;

int			nummodes;
int			windowed_mouse;
int			leavecurrentmode;	// -current: keep the desktop resolution
int			windowed;

typedef struct
{
	int			modenum;
	char		*desc;
	int			iscur;
} modedesc_t;

modedesc_t	modedescs[100];
int			vid_wmodes;

static vmode_t	badmode;
static char		vid_extmodedesc[100];
static char		vid_modedesc[100];

HWND		dibwindow;
HWND		mainwindow;
HWND		hwnd_dialog;	// startup splash window
HDC			maindc;
HGLRC		baseRC;
HINSTANCE	g_hInstance;
LPARAM		hIcon;

int			vid_initialized;
int			vid_modenum;
int			vid_default;

int			vid_window_height;
int			vid_window_width;
int			window_width;
int			window_height;
int			vid_window_x;
int			vid_window_y;
int			vid_border_width;
int			vid_border_height;
int			window_x;
int			window_y;

int			scr_vrect_width;
int			scr_vrect_height;
int			vid_fullscreen_mode;
int			scr_width = VIRTUAL_WIDTH;
int			scr_height = VIRTUAL_HEIGHT;
int			vid_maxwarpwidth;
int			vid_maxwarpheight;
int			vid_buffer;
int			vid_rowbytes;

BINDTEXFUNCPTR	bindTexFunc;
int			vid_skip_swap;

int			app_active_state;
int			app_active_flag;
static int	vid_wassuspended;	// fullscreen mode was dropped on focus loss

RECT		g_Rect;
int			g_X, g_Y;
int			window_center_x, window_center_y;
RECT		window_rect;
int			DIBWidth, DIBHeight;

char		g_ClassName[] = "HalfLife";
char		g_WindowName[] = "HalfLife-GL";

PIXELFORMATDESCRIPTOR pfd =
{
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,								// version number
	PFD_DRAW_TO_WINDOW				// support window
	| PFD_SUPPORT_OPENGL			// support OpenGL
	| PFD_DOUBLEBUFFER,				// double buffered
	PFD_TYPE_RGBA,					// RGBA type
	24,								// 24-bit color depth
	0, 0, 0, 0, 0, 0,				// color bits ignored
	0,								// no alpha buffer
	0,								// shift bit ignored
	0,								// no accumulation buffer
	0, 0, 0, 0,						// accum bits ignored
	32,								// 32-bit z-buffer
	0,								// no stencil buffer
	0,								// no auxiliary buffer
	PFD_MAIN_PLANE,					// main layer
	0,								// reserved
	0, 0, 0							// layer masks ignored
};

DEVMODEA	gdevmode;

byte		scantokey[128] =
{
//	0			1			2			3			4			5			6			7
//	8			9			A			B			C			D			E			F
	0,			27,			'1',		'2',		'3',		'4',		'5',		'6',
	'7',		'8',		'9',		'0',		'-',		'=',		K_BACKSPACE, 9,			// 0
	'q',		'w',		'e',		'r',		't',		'y',		'u',		'i',
	'o',		'p',		'[',		']',		13,			K_CTRL,		'a',		's',	// 1
	'd',		'f',		'g',		'h',		'j',		'k',		'l',		';',
	'\'',		'`',		K_SHIFT,	'\\',		'z',		'x',		'c',		'v',	// 2
	'b',		'n',		'm',		',',		'.',		'/',		K_SHIFT,	'*',
	K_ALT,		' ',		0,			K_F1,		K_F2,		K_F3,		K_F4,		K_F5,	// 3
	K_F6,		K_F7,		K_F8,		K_F9,		K_F10,		K_PAUSE,	0,			K_HOME,
	K_UPARROW,	K_PGUP,		'-',		K_LEFTARROW, '5',		K_RIGHTARROW, '+',		K_END,	// 4
	K_DOWNARROW, K_PGDN,	K_INS,		K_DEL,		0,			0,			0,			K_F11,
	K_F12,		0,			0,			0,			0,			0,			0,			0,		// 5
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0,		// 6
	0,			0,			0,			0,			0,			0,			0,			0,
	0,			0,			0,			0,			0,			0,			0,			0		// 7
};

static HANDLE	movie_file = INVALID_HANDLE_VALUE;
static char		movie_filename[256];

char *VID_GetModeDescription(int mode);
char *VID_GetExtModeDescription(int mode);
int VID_InitFullDIB(void);
int VID_UpdateWindowStatus(void);
int ClearAllStates(void);
void AppActivate(BOOL fActive, BOOL minimize);

int VID_Stub_ReturnZero(void)
{
	return 0;
}

// software renderer entry points, nothing to do with GL

void VID_HandlePause(void)
{
}

void VID_HandlePause2(void)
{
}

void VID_LockBuffer(void)
{
}

void VID_UnlockBuffer(void)
{
}

void VID_ForceLockState2(void)
{
}

void VID_Shutdown_Stub(void)
{
}

void D_BeginDirectRect(void)
{
}

void D_EndDirectRect(void)
{
}

void GL_ErrorString_Null(void)
{
}

/*
================
CenterWindow
================
*/
BOOL CenterWindow(HWND hWndCenter, int width, int height)
{
	int		CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterY * 2 < CenterX)
		CenterX >>= 1;	// dual screens
	if (CenterX <= 0)
		CenterX = 0;
	if (CenterY <= 0)
		CenterY = 0;
	return SetWindowPos(hWndCenter, NULL, CenterX, CenterY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

/*
================
VID_SetWindowedMode
================
*/
int VID_SetWindowedMode(int modenum)
{
	HDC		hdc;
	RECT	rect;
	LONG	height;

	vid_border_width = 0;
	vid_border_height = 0;

	height = modelist[modenum].height;
	DIBWidth = modelist[modenum].width;
	DIBHeight = height;
	vid_window_width = DIBWidth;
	vid_window_height = height;

	rect.left = 0;
	rect.top = 0;
	rect.right = DIBWidth;
	rect.bottom = height;
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

	// create the DIB window
	dibwindow = CreateWindowExA(0, g_ClassName, g_WindowName, WS_OVERLAPPEDWINDOW,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, g_hInstance, NULL);

	if (!dibwindow)
		Sys_Error("Couldn't create DIB window");

	mainwindow = dibwindow;

	// center and show the DIB window
	CenterWindow(dibwindow, DIBWidth - vid_border_width, DIBHeight - vid_border_height);

	ShowWindow(dibwindow, SW_SHOWDEFAULT);
	UpdateWindow(dibwindow);

	vid_fullscreen = MS_WINDOWED;

	// because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc, 0, 0, DIBWidth, DIBHeight, BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	vid_fullscreen_mode = 2;
	scr_height = VIRTUAL_HEIGHT;
	scr_vrect_height = VIRTUAL_HEIGHT;
	scr_width = VIRTUAL_WIDTH;
	scr_vrect_width = VIRTUAL_WIDTH;
	vid.width = VIRTUAL_WIDTH;
	vid.height = VIRTUAL_HEIGHT;
	vid.conwidth = VIRTUAL_WIDTH;
	vid.conheight = VIRTUAL_HEIGHT;

	SendMessageA(dibwindow, WM_SETICON, ICON_BIG, hIcon);
	SendMessageA(dibwindow, WM_SETICON, ICON_SMALL, hIcon);

	return true;
}

/*
================
VID_SetFullDIBMode
================
*/
int VID_SetFullDIBMode(int modenum)
{
	HDC		hdc;
	RECT	rect;
	LONG	height;

	if (!leavecurrentmode)
	{
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmSize = sizeof(gdevmode);
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width << modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;

		if (ChangeDisplaySettingsA(&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			Sys_Error("Couldn't set fullscreen DIB mode");
	}

	vid_border_width = 0;
	vid_border_height = 0;
	vid_fullscreen = MS_FULLDIB;

	height = modelist[modenum].height;
	DIBWidth = modelist[modenum].width;
	DIBHeight = height;
	vid_window_width = DIBWidth;
	vid_window_height = height;

	rect.left = 0;
	rect.top = 0;
	rect.right = DIBWidth;
	rect.bottom = height;
	AdjustWindowRectEx(&rect, WS_POPUP, FALSE, 0);

	// create the DIB window
	dibwindow = CreateWindowExA(0, "HalfLife", "HalfLife-GL", WS_POPUP,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, g_hInstance, NULL);

	if (!dibwindow)
		Sys_Error("Couldn't create DIB window");

	mainwindow = dibwindow;

	ShowWindow(dibwindow, SW_SHOWDEFAULT);
	UpdateWindow(dibwindow);

	// because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc, 0, 0, DIBWidth, DIBHeight, BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	vid_fullscreen_mode = 2;
	window_x = 0;
	scr_height = VIRTUAL_HEIGHT;
	scr_vrect_height = VIRTUAL_HEIGHT;
	window_y = 0;
	scr_width = VIRTUAL_WIDTH;
	scr_vrect_width = VIRTUAL_WIDTH;
	vid.width = VIRTUAL_WIDTH;
	vid.height = VIRTUAL_HEIGHT;
	vid.conwidth = VIRTUAL_WIDTH;
	vid.conheight = VIRTUAL_HEIGHT;

	SendMessageA(dibwindow, WM_SETICON, ICON_BIG, hIcon);
	SendMessageA(dibwindow, WM_SETICON, ICON_SMALL, hIcon);

	return true;
}

/*
================
VID_SetMode
================
*/
int VID_SetMode(int modenum)
{
	int		original_mode, temp;
	int		stat;
	MSG		msg;

	if ((windowed && modenum) || (!windowed && (modenum < 1 || modenum >= nummodes)))
		Sys_Error("Bad video mode\n");

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	S_EndPrecaching();
	CDAudio_Pause();

	original_mode = modelist[modenum].type;

	// set either the fullscreen or windowed mode
	if (original_mode != MS_WINDOWED)
	{
		if (original_mode != MS_FULLDIB)
			Sys_Error("VID_SetMode: Bad mode type");

		stat = VID_SetFullDIBMode(modenum);
		IN_ActivateMouse();
		IN_HideMouse();
	}
	else if (_windowed_mouse.value == 0)
	{
		IN_DeactivateMouse();
		IN_ShowMouse();
		stat = VID_SetWindowedMode(modenum);
	}
	else
	{
		stat = VID_SetWindowedMode(modenum);
		IN_ActivateMouse();
		IN_HideMouse();
	}

	window_width = vid_window_width;
	window_height = vid_window_height;
	glx = 0;
	gly = 0;
	glwidth = window_width;
	glheight = window_height;

	VID_UpdateWindowStatus();

	CDAudio_Resume();
	S_BeginPrecaching();
	scr_disabled_for_loading = temp;

	if (!stat)
		Sys_Error("Couldn't set video mode");

	// make sure we get the focus on the mode switch
	SetForegroundWindow(dibwindow);
	vid_modenum = modenum;
	Cvar_SetValue("vid_mode", (float)vid_modenum);

	while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	Sleep(100);

	SetWindowPos(dibwindow, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

	SetForegroundWindow(dibwindow);

	// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates();

	if (!vid_suppressModeChangePrint)
		Con_DPrintf("%s\n", VID_GetModeDescription(vid_modenum));

	vid.recalc_refdef = 1;

	return true;
}

/*
================
VID_UpdateWindowStatus
================
*/
int VID_UpdateWindowStatus(void)
{
	g_Rect.left = window_x;
	g_Rect.top = window_y;
	g_Rect.right = window_x + window_width;
	g_Rect.bottom = window_y + window_height;
	g_X = (2 * window_x + window_width) / 2;
	g_Y = (2 * window_y + window_height) / 2;

	window_rect = g_Rect;
	window_center_x = g_X;
	window_center_y = g_Y;

	if (mouseinitialized && mouseactive)
		return ClipCursor(&g_Rect);

	return g_Y;
}

//====================================

/*
===============
CheckTextureExtensions
===============
*/
FARPROC CheckTextureExtensions(void)
{
	char		*tmp;
	qboolean	texture_ext;
	HINSTANCE	hInstGL;
	FARPROC		proc;

	texture_ext = false;
	// check for texture extension
	tmp = (char *)glGetString(GL_EXTENSIONS);
	while (*tmp)
	{
		if (!strncmp(tmp, "GL_EXT_texture_object", strlen("GL_EXT_texture_object")))
			texture_ext = true;
		tmp++;
	}

	if (!texture_ext || COM_CheckParm("-gl11"))
	{
		hInstGL = LoadLibraryA("opengl32.dll");

		if (hInstGL == NULL)
			Sys_Error("Couldn't load opengl32.dll");

		proc = GetProcAddress(hInstGL, "glBindTexture");
		bindTexFunc = (BINDTEXFUNCPTR)proc;
		if (!bindTexFunc)
			Sys_Error("No texture object support in GL");
	}
	else
	{
		// load library and get procedure addresses for texture extension API
		proc = wglGetProcAddress("glBindTextureEXT");
		bindTexFunc = (BINDTEXFUNCPTR)proc;
		if (!bindTexFunc)
			Sys_Error("GetProcAddress failed for glBindTextureEXT");
	}

	return proc;
}

/*
===============
GL_Init
===============
*/
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

	glClearColor(1, 0, 0, 0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
=================
VID_GetWindowSize
=================
*/
void VID_GetWindowSize(int *x, int *y, int *width, int *height)
{
	*y = 0;
	*x = 0;
	*width = DIBWidth - vid_window_x;
	*height = DIBHeight - vid_window_y;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering(void)
{
	if (!vid_skip_swap)
		SwapBuffers(maindc);

	// handle the mouse state when windowed if that's changed
	if (vid_fullscreen == MS_WINDOWED)
	{
		if ((int)_windowed_mouse.value != windowed_mouse)
		{
			if (_windowed_mouse.value == 0)
			{
				IN_DeactivateMouse();
				IN_ShowMouse();
			}
			else
			{
				IN_ActivateMouse();
				IN_HideMouse();
			}

			windowed_mouse = (int)_windowed_mouse.value;
		}
	}
}

/*
=================
VID_Shutdown
=================
*/
void VID_Shutdown(void)
{
	HGLRC	hRC;
	HDC		hDC;

	if (vid_initialized)
	{
		hRC = wglGetCurrentContext();
		hDC = wglGetCurrentDC();

		wglMakeCurrent(NULL, NULL);

		if (hRC)
			wglDeleteContext(hRC);

		if (hDC && dibwindow)
			ReleaseDC(dibwindow, hDC);

		if (vid_fullscreen == MS_FULLDIB)
			ChangeDisplaySettingsA(NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC(dibwindow, maindc);

		AppActivate(false, false);
	}
}

//==========================================================================

/*
=================
bSetupPixelFormat
=================
*/
BOOL bSetupPixelFormat(HDC hDC)
{
	int		pixelformat;

	pixelformat = ChoosePixelFormat(hDC, &pfd);
	if (pixelformat)
	{
		if (SetPixelFormat(hDC, pixelformat, &pfd))
			return TRUE;
		MessageBoxA(NULL, "SetPixelFormat failed", "Error", MB_OK);
	}
	else
	{
		MessageBoxA(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
	}

	return FALSE;
}

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey(int key)
{
	key = (key >> 16) & 255;
	if (key > 127)
		return 0;

	return scantokey[key];
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
int ClearAllStates(void)
{
	int		i;

	// send an up event for each key, to make sure the server clears them all
	for (i = 0; i < 256; i++)
	{
		Key_Event(i, false);
	}

	Key_ClearStates();
	return IN_ClearMouseState();
}

/*
================
AppActivate
================
*/
void AppActivate(BOOL fActive, BOOL minimize)
{
	static BOOL	sound_active;

	app_active_state = minimize;
	app_active_flag = fActive;

	// enable/disable sound on focus gain/loss
	if (fActive)
	{
		if (!sound_active)
		{
			S_UnblockSound();
			sound_active = true;
		}
	}
	else
	{
		if (sound_active)
		{
			S_BlockSound();
			sound_active = false;
		}
	}

	if (fActive)
	{
		if (vid_fullscreen == MS_FULLDIB || (vid_fullscreen == MS_WINDOWED && _windowed_mouse.value))
		{
			IN_ActivateMouse();
			IN_HideMouse();
		}
	}
	else
	{
		if (vid_fullscreen == MS_FULLDIB || (vid_fullscreen == MS_WINDOWED && _windowed_mouse.value))
		{
			IN_DeactivateMouse();
			IN_ShowMouse();
		}
	}

	if (vid_fullscreen == MS_FULLDIB && dibwindow)
	{
		if (!fActive && !vid_wassuspended)
		{
			vid_wassuspended = true;
			if (!leavecurrentmode)
				ChangeDisplaySettingsA(NULL, 0);
			ShowWindow(dibwindow, SW_SHOWMINNOACTIVE);
		}
		else if (fActive && vid_wassuspended)
		{
			vid_wassuspended = false;
			if (!leavecurrentmode)
				ChangeDisplaySettingsA(&gdevmode, CDS_FULLSCREEN);
			ShowWindow(dibwindow, SW_SHOWNORMAL);
			MoveWindow(dibwindow, 0, 0, DIBWidth, DIBHeight, FALSE);
			VID_UpdateWindowStatus();
		}
	}
}

/*
===================
MainWndProc

main window procedure
===================
*/
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT	lRet = 1;
	int		temp;

	switch (uMsg)
	{
	case WM_CREATE:
		break;

	case WM_MOVE:
		window_x = LOWORD(lParam);
		window_y = HIWORD(lParam);
		VID_UpdateWindowStatus();
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		Key_Event(MapKey(lParam), true);
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		Key_Event(MapKey(lParam), false);
		break;

	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
		temp = 0;

		if (wParam & MK_LBUTTON)
			temp |= 1;

		if (wParam & MK_RBUTTON)
			temp |= 2;

		if (wParam & MK_MBUTTON)
			temp |= 4;

		IN_MouseEvent(temp);
		break;

	case WM_SIZE:
		break;

	case WM_CLOSE:
		if (MessageBoxA(dibwindow, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			Sys_Quit();
		break;

	case WM_ACTIVATE:
		AppActivate(LOWORD(wParam) != WA_INACTIVE, HIWORD(wParam));

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
		ClearAllStates();
		break;

	case WM_DESTROY:
		if (dibwindow)
			DestroyWindow(dibwindow);
		PostQuitMessage(0);
		break;

	case MM_MCINOTIFY:
		lRet = CDAudio_MessageHandler(hWnd, uMsg, wParam, lParam);
		break;

	default:
		// pass all unhandled messages to DefWindowProc
		lRet = DefWindowProcA(hWnd, uMsg, wParam, lParam);
		break;
	}

	// return 1 if handled message, 0 if not
	return lRet;
}

/*
=================
VID_NumModes
=================
*/
int VID_NumModes(void)
{
	return nummodes;
}

/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr(int modenum)
{
	if (modenum < 0 || modenum >= nummodes)
		return &badmode;

	return &modelist[modenum];
}

/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription(int mode)
{
	if (mode < 0 || mode >= nummodes)
		return NULL;

	if (!leavecurrentmode)
		return modelist[mode].modedesc;

	sprintf(vid_modedesc, "Desktop resolution (%dx%d)", vid_desktop_width, vid_desktop_height);
	return vid_modedesc;
}

/*
=================
VID_GetExtModeDescription

Tacks on "windowed" or "fullscreen"
=================
*/
char *VID_GetExtModeDescription(int mode)
{
	vmode_t	*pv;

	if (mode < 0 || mode >= nummodes)
		return NULL;

	pv = &modelist[mode];
	if (pv->type == MS_FULLDIB)
	{
		if (leavecurrentmode)
			sprintf(vid_extmodedesc, "Desktop resolution (%dx%d)", vid_desktop_width, vid_desktop_height);
		else
			sprintf(vid_extmodedesc, "%s fullscreen", pv->modedesc);
	}
	else if (vid_fullscreen != MS_WINDOWED)
	{
		sprintf(vid_extmodedesc, "Windowed");
	}
	else
	{
		sprintf(vid_extmodedesc, "%s windowed", pv->modedesc);
	}

	return vid_extmodedesc;
}

/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f(void)
{
	Con_Printf("%s\n", VID_GetExtModeDescription(vid_modenum));
}

/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f(void)
{
	if (nummodes == 1)
		Con_Printf("%d video mode is available\n", nummodes);
	else
		Con_Printf("%d video modes are available\n", nummodes);
}

/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f(void)
{
	int		t, modenum;

	modenum = Q_atoi(Cmd_Argv(1));

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf("%s\n", VID_GetExtModeDescription(modenum));

	leavecurrentmode = t;
}

/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f(void)
{
	int		i, lnummodes, t;
	char	*pinfo;

	lnummodes = VID_NumModes();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i = 1; i < lnummodes; i++)
	{
		VID_GetModePtr(i);
		pinfo = VID_GetExtModeDescription(i);
		Con_Printf("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}

/*
=================
VID_TakeSnapshot

Writes the screen to a 24 bit BMP file
=================
*/
BOOL VID_TakeSnapshot(const char *pFilename)
{
	int					size;
	HANDLE				hFile;
	byte				*pBits;
	byte				*p, *q;
	byte				temp;
	int					count;
	DWORD				dwWritten;
	struct
	{
		short	bfType;
		int		bfSize;
		short	bfReserved1;
		short	bfReserved2;
		int		bfOffBits;
	}					bmfh;	// not packed, the fields after bfType land 2 bytes late
	BITMAPINFOHEADER	bmih;

	size = 3 * vid_window_width * vid_window_height;

	hFile = CreateFileA(pFilename, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		Sys_Error("Couldn't create file %s", pFilename);

	// write the file header
	bmfh.bfType = ('M' << 8) | 'B';
	bmfh.bfSize = size + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	if (!WriteFile(hFile, &bmfh, sizeof(BITMAPFILEHEADER), &dwWritten, NULL))
		Sys_Error("Couldn't write file header");

	// write the info header
	bmih.biSize = sizeof(bmih);
	bmih.biWidth = vid_window_width;
	bmih.biHeight = vid_window_height;
	bmih.biPlanes = 1;
	bmih.biBitCount = 24;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;

	if (!WriteFile(hFile, &bmih, sizeof(bmih), &dwWritten, NULL))
		Sys_Error("Couldn't write bitmap header");

	pBits = malloc(size);
	if (!pBits)
		Sys_Error("Couldn't allocate memory for screenshot");

	glReadPixels(0, 0, vid_window_width, vid_window_height, GL_RGB, GL_UNSIGNED_BYTE, pBits);

	// swap rgb to bgr
	p = pBits;
	q = pBits + 2;
	count = vid_window_width * vid_window_height;
	if (count > 0)
	{
		do
		{
			temp = *p;
			*p = *q;
			p += 3;
			*q = temp;
			q += 3;
			--count;
		} while (count);
	}

	if (!WriteFile(hFile, pBits, size, &dwWritten, NULL))
		Sys_Error("Couldn't write bitmap data");

	free(pBits);

	if (!CloseHandle(hFile))
		Sys_Error("Couldn't close file");

	return TRUE;
}

/*
=================
VID_WriteBuffer

Appends the current frame to the movie file; a filename starts a new movie.
=================
*/
BOOL VID_WriteBuffer(const char *pFilename)
{
	DWORD	dwCreationDisposition;
	DWORD	size;
	byte	*pBits;
	BOOL	result;
	DWORD	blockheader[2];
	WORD	frameheader[4];
	LONG	dwWritten;

	dwCreationDisposition = OPEN_EXISTING;
	size = 3 * vid_window_width * vid_window_height;

	if (pFilename)
	{
		strcpy(movie_filename, pFilename);
		if (movie_file != INVALID_HANDLE_VALUE)
			CloseHandle(movie_file);
		dwCreationDisposition = CREATE_ALWAYS;
	}

	movie_file = CreateFileA(movie_filename, GENERIC_WRITE, 0, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
	if (movie_file == INVALID_HANDLE_VALUE)
		Sys_Error("Couldn't open movie file");

	dwWritten = 0;
	SetFilePointer(movie_file, 0, &dwWritten, FILE_END);

	frameheader[0] = vid_window_width;
	frameheader[1] = vid_window_height;
	frameheader[2] = 24;	// bits per pixel

	blockheader[0] = MOVIE_BLOCK_ID;
	blockheader[1] = size + 3 * sizeof(WORD);

	pBits = malloc(3 * vid_window_width * vid_window_height);
	if (!pBits)
		Sys_Error("Couldn't allocate memory for movie frame");

	glReadPixels(0, 0, vid_window_width, vid_window_height, GL_RGB, GL_UNSIGNED_BYTE, pBits);

	if (!WriteFile(movie_file, blockheader, sizeof(blockheader), (LPDWORD)&dwWritten, NULL))
		Sys_Error("Couldn't write block header");

	if (!WriteFile(movie_file, frameheader, 3 * sizeof(WORD), (LPDWORD)&dwWritten, NULL))
		Sys_Error("Couldn't write frame header");

	if (!WriteFile(movie_file, pBits, size, (LPDWORD)&dwWritten, NULL))
		Sys_Error("Couldn't write frame data");

	free(pBits);

	result = CloseHandle(movie_file);
	if (!result)
		Sys_Error("Couldn't close file");

	movie_file = INVALID_HANDLE_VALUE;
	return result;
}

//==========================================================================

/*
================
VID_MenuKey
================
*/
int VID_MenuKey(int key)
{
	if (key == K_ESCAPE)
	{
		S_LocalSound("misc/menu1.wav");
		M_Menu_Options_f();
	}

	return 0;
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw(void)
{
	qpic_t		*p;
	int			i;
	int			lnummodes;
	char		*ptr;
	int			column, row;
	int			n;
	modedesc_t	*pdesc;

	p = Draw_CachePic("gfx/vidmodes.lmp");
	Draw_Pic((VIRTUAL_WIDTH - p->width) / 2, 4, p);

	vid_wmodes = 0;
	lnummodes = VID_NumModes();

	if (lnummodes > 1)
	{
		i = 1;
		do
		{
			if (vid_wmodes >= MAX_MODEDESCS)
				break;

			ptr = VID_GetModeDescription(i);
			modedescs[vid_wmodes].modenum = i;
			modedescs[vid_wmodes].desc = ptr;
			modedescs[vid_wmodes].iscur = 0;

			if (i == vid_modenum)
				modedescs[vid_wmodes].iscur = 1;

			++i;
			++vid_wmodes;
		} while (i < lnummodes);
	}

	n = 0;
	if (vid_wmodes > 0)
	{
		column = 8;
		row = 36 + 2 * 8;
		M_Print(2 * 8, 36 + 0 * 8, "Fullscreen Modes");

		pdesc = modedescs;
		do
		{
			if (pdesc->iscur)
				M_PrintWhite(column, row, pdesc->desc);
			else
				M_Print(column, row, pdesc->desc);

			column += 13 * 8;
			if (n % VID_ROW_SIZE == VID_ROW_SIZE - 1)
			{
				column = 8;
				row += 8;
			}

			pdesc++;
			++n;
		} while (n < vid_wmodes);
	}

	M_Print(3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 2, "Video modes must be set from the");
	M_Print(3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 3, "command line with -width <width>");
	M_Print(3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 4, "and -bpp <bits-per-pixel>");
	M_Print(3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 6, "Select windowed mode with -window");
}

//==========================================================================

/*
================
VID_InitDIB
================
*/
int VID_InitDIB(HINSTANCE hInstance)
{
	WNDCLASSA	wc;
	int			height;

	// register the frame class
	wc.hInstance = hInstance;
	wc.style = 0;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = 0;
	wc.lpfnWndProc = MainWndProc;
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = 0;
	wc.lpszClassName = "HalfLife";

	if (!RegisterClassA(&wc))
		Sys_Error("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;

	if (COM_CheckParm("-width"))
		modelist[0].width = Q_atoi(com_argv[COM_CheckParm("-width") + 1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm("-height"))
		height = Q_atoi(com_argv[COM_CheckParm("-height") + 1]);
	else
		height = 240 * modelist[0].width / 320;

	modelist[0].height = height;
	if (height < 240)
		modelist[0].height = 240;

	sprintf(modelist[0].modedesc, "%dx%d", modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;

	return 0;
}

/*
=================
VID_InitFullDIB
=================
*/
int VID_InitFullDIB(void)
{
	DEVMODEA	devmode;
	int			i, modenum, originalnummodes, existingmode;
	BOOL		stat;

	// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do
	{
		stat = EnumDisplaySettingsA(NULL, modenum, &devmode);

		if (devmode.dmBitsPerPel >= 15 && devmode.dmPelsWidth <= MAXWIDTH && devmode.dmPelsHeight <= MAXHEIGHT && nummodes < MAX_FULLDIB_MODES)
		{
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettingsA(&devmode, CDS_TEST) == DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf(modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

				// if the width is more than twice the height, reduce it by half because this
				// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (2 * modelist[nummodes].height < modelist[nummodes].width)
					{
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf(modelist[nummodes].modedesc, "%dx%dx%d", modelist[nummodes].width, modelist[nummodes].height, modelist[nummodes].bpp);
					}
				}

				for (i = originalnummodes, existingmode = 0; i < nummodes; i++)
				{
					if (modelist[i].width == modelist[nummodes].width
					&& modelist[i].height == modelist[nummodes].height
					&& modelist[i].bpp == modelist[nummodes].bpp)
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
					nummodes++;
			}
		}

		modenum++;
	} while (stat);

	if (originalnummodes == nummodes)
	{
		Con_Printf("No fullscreen DIB modes found\n");
		return 0;
	}

	return stat;
}

/*
===================
VID_Init
===================
*/
int VID_Init(void)
{
	int		width, height, bpp, findbpp, done;
	HDC		hdc;

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

	hIcon = (LPARAM)LoadIconA(g_hInstance, MAKEINTRESOURCEA(1));

	VID_InitDIB(g_hInstance);
	nummodes = 1;

	VID_InitFullDIB();

	if (COM_CheckParm("-window"))
	{
		hdc = GetDC(NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			Sys_Error("Can't run in non-RGB mode");

		ReleaseDC(NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm("-mode"))
		{
			vid_default = Q_atoi(com_argv[COM_CheckParm("-mode") + 1]);
		}
		else
		{
			if (COM_CheckParm("-current"))
			{
				vid_desktop_width = GetSystemMetrics(SM_CXSCREEN);
				vid_desktop_height = GetSystemMetrics(SM_CYSCREEN);
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
				if (COM_CheckParm("-width"))
					width = Q_atoi(com_argv[COM_CheckParm("-width") + 1]);
				else
					width = 640;

				if (COM_CheckParm("-bpp"))
				{
					bpp = Q_atoi(com_argv[COM_CheckParm("-bpp") + 1]);
					findbpp = 0;
				}
				else
				{
					bpp = 15;
					findbpp = 1;
				}

				if (COM_CheckParm("-height"))
					height = Q_atoi(com_argv[COM_CheckParm("-height") + 1]);
				else
					height = 480;

				// if they want to force it, add the specified mode to the list
				if (COM_CheckParm("-force") && nummodes < MAX_FULLDIB_MODES)
				{
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					modelist[nummodes].halfscreen = 0;
					sprintf(modelist[nummodes].modedesc, "%dx%dx%d", width, height, bpp);

					vid_default = nummodes++;
				}

				done = 0;

				do
				{
					vid_default = 0;

					if (COM_CheckParm("-height"))
					{
						height = Q_atoi(com_argv[COM_CheckParm("-height") + 1]);

						for (int i = 1; i < nummodes; i++)
						{
							if (modelist[i].width == width && modelist[i].height == height && modelist[i].bpp == bpp)
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}
					else
					{
						for (int i = 1; i < nummodes; i++)
						{
							if (modelist[i].width == width && modelist[i].bpp == bpp)
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					if (done)
						break;

					if (!findbpp)
						break;

					switch (bpp)
					{
					case 15:
						bpp = 16;
						break;
					case 16:
						bpp = 32;
						break;
					case 32:
						bpp = 24;
						break;
					case 24:
						findbpp = 0;
						break;
					}
				} while (!done);

				if (!vid_default)
					Sys_Error("Specified video mode not available");
			}
		}
	}

	vid_initialized = true;

	vid_maxwarpwidth = VIRTUAL_WIDTH;
	vid_maxwarpheight = VIRTUAL_HEIGHT;

	vid.colormap = host_colormap;
	vid.fullbright = 256 - Q_strlen((char *)((int *)host_colormap + 2048));
	vid_buffer = 0;
	vid_rowbytes = 0;

	DestroyWindow(hwnd_dialog);

	VID_SetMode(vid_default);

	maindc = GetDC(dibwindow);
	bSetupPixelFormat(maindc);

	baseRC = wglCreateContext(maindc);
	if (!baseRC)
		Sys_Error("wglCreateContext failed");
	if (!wglMakeCurrent(maindc, baseRC))
		Sys_Error("wglMakeCurrent failed");

	GL_Init();

	char	gldir[MAX_OSPATH];

	sprintf(gldir, "%s/glquake", com_gamedir);
	Sys_mkdir(gldir);

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy(badmode.modedesc, "Bad mode");

	return vid_modenum;
}

/*
================
VID_ShiftPalette
================
*/
void VID_ShiftPalette(void)
{
}
