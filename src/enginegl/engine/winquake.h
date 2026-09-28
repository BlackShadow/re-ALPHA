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
// winquake.h -- Win32-specific Quake header file

#ifndef WINQUAKE_H
#define WINQUAKE_H

#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#define DS_LOCK_RETRIES		10000		// attempts at restoring a lost DirectSound buffer

extern LPDIRECTSOUNDBUFFER	pDSBuf;
extern int					gSndBufSize;

extern HWND			mainwindow;
extern int			app_active_flag;

extern int			window_center_x, window_center_y;
extern RECT			window_rect;

BOOL IN_ActivateMouse(void);
BOOL IN_DeactivateMouse(void);
int IN_ShowMouse(void);
int IN_HideMouse(void);
int IN_MouseEvent(int buttons);
int IN_ClearMouseState(void);

extern int			mouseinitialized;
extern int			mouseactive;

LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Winsock entry points, loaded from wsock32.dll by WINS_Init
extern int (PASCAL FAR *pWSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
extern int (PASCAL FAR *pWSACleanup)(void);
extern int (PASCAL FAR *pWSAGetLastError)(void);
extern SOCKET (PASCAL FAR *psocket)(int af, int type, int protocol);
extern int (PASCAL FAR *pioctlsocket)(SOCKET s, long cmd, u_long FAR *argp);
extern int (PASCAL FAR *psetsockopt)(SOCKET s, int level, int optname, const char FAR *optval, int optlen);
extern int (PASCAL FAR *precvfrom)(SOCKET s, char FAR *buf, int len, int flags, struct sockaddr FAR *from, int FAR *fromlen);
extern int (PASCAL FAR *psendto)(SOCKET s, const char FAR *buf, int len, int flags, const struct sockaddr FAR *to, int tolen);
extern int (PASCAL FAR *pclosesocket)(SOCKET s);
extern int (PASCAL FAR *pgethostname)(char FAR *name, int namelen);
extern struct hostent FAR *(PASCAL FAR *pgethostbyname)(const char FAR *name);
extern struct hostent FAR *(PASCAL FAR *pgethostbyaddr)(const char FAR *addr, int len, int type);
extern int (PASCAL FAR *pgetsockname)(SOCKET s, struct sockaddr FAR *name, int FAR *namelen);

#endif // _WIN32

#endif // WINQUAKE_H
