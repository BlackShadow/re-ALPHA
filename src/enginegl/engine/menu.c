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

// menu.c -- the full screen menus

#include "quakedef.h"
#include "nullsubs.h"

#define MENU_WIDTH			320			// menus are laid out for a 320 wide screen, centered

#define CURSOR_CHAR			0x8d		// arrow in front of the selected item
#define SAVE_CURSOR_CHAR	')'

#define SLIDER_LEFT_CHAR	0x80
#define SLIDER_MID_CHAR		0x81
#define SLIDER_RIGHT_CHAR	0x82
#define SLIDER_KNOB_CHAR	0x83
#define SLIDER_RANGE		10

#define NUM_MENUDOTS		6			// frames of the spinning menu dot

#define MAIN_ITEMS			5
#define SINGLEPLAYER_ITEMS	3
#define MAX_SAVEGAMES		12
#define MULTIPLAYER_ITEMS	3
#define NUM_SETUP_CMDS		5
#define NUM_PLAYER_COLORS	14
#define OPTIONS_ITEMS		14
#define NUM_HELP_PAGES		6
#define NUM_SERIALCONFIG_CMDS	6
#define NUM_MODEMCONFIG_CMDS	5
#define NUM_LANCONFIG_CMDS	3
#define NUM_GAMEOPTIONS		7

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define SerialConfig	(m_net_cursor == 0)
#define DirectConfig	(m_net_cursor == 1)
#define IPXConfig		(m_net_cursor == 2)
#define TCPIPConfig		(m_net_cursor == 3)

menu_state_t	m_state;
int				m_return_state;
qboolean		m_return_onerror;
char			m_return_reason[60];

static qboolean	m_entersound;			// play after drawing a frame, so caching
										// won't disrupt the sound
static qboolean	m_recursiveDraw;
static int		m_save_demonum;

static int		m_main_cursor;
static int		m_singleplayer_cursor;
static int		m_multiplayer_cursor;

static int		load_cursor;			// also the help page number

static int		options_cursor;
static int		keys_cursor;
static qboolean	bind_grab;

static int		m_net_cursor;
static int		m_net_items;

static int		m_quit_prevstate;
static int		msgNumber;
static qboolean	wasInMenus;

static int		serialConfig_cursor;
static int		modemConfig_cursor;
static int		lanConfig_cursor = -1;

static int		gameoptions_cursor;
static int		maxplayers;
static qboolean	m_serverInfoMessage = false;
static double	m_serverInfoMessageTime;

static qboolean	searchComplete = false;
static double	searchCompleteTime;

static int		slist_cursor;
static qboolean	slist_sorted;

static char		m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH];
static int		loadable[MAX_SAVEGAMES];

static char		setup_hostname[16];
static char		setup_myname[16];
static int		setup_top;
static int		setup_bottom;
static int		setup_oldtop;
static int		setup_oldbottom;
static int		setup_cursor;

static char		serialConfig_phone[16];
static int		serialConfig_comport;
static int		serialConfig_baud;
static int		serialConfig_irq;

static char		modemConfig_dialing;
static char		modemConfig_clear[16];
static char		modemConfig_init[30];
static char		modemConfig_hangup[16];

static char		lanConfig_joinname[22];
static char		lanConfig_portname[6];
static int		lanConfig_port;

byte			identityTable[256];
byte			translationTable[256];

static void M_Main_Draw(void);
static void M_Main_Key(int key);
static void M_SinglePlayer_Draw(void);
static void M_SinglePlayer_Key(int key);
static void M_ScanSaves(void);
static void M_Load_Draw(void);
static void M_Load_Key(int key);
static void M_Save_Draw(void);
static void M_Save_Key(int key);
static void M_MultiPlayer_Draw(void);
static void M_MultiPlayer_Key(int key);
static void M_Setup_Draw(void);
static void M_Setup_Key(int key);
static void M_Net_Draw(void);
static void M_Net_Key(int key);
static void M_Options_Draw(void);
static void M_Options_Key(int key);
static void M_Keys_Draw(void);
static void M_Keys_Key(int key);
static void M_Video_Draw(void);
static void M_Video_Key(int key);
static void M_Help_Draw(void);
static void M_Help_Key(int key);
static void M_Quit_Draw(void);
static void M_Quit_Key(int key);
static void M_SerialConfig_Draw(void);
static void M_SerialConfig_Key(int key);
static void M_ModemConfig_Draw(void);
static void M_ModemConfig_Key(int key);
static void M_LanConfig_Draw(void);
static void M_LanConfig_Key(int key);
static void M_GameOptions_Draw(void);
static void M_GameOptions_Key(int key);
static void M_Search_Draw(void);
static void M_ServerList_Draw(void);
static void M_ServerList_Key(int key);
static void M_ConfigureNetSubsystem(void);

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
int M_DrawCharacter(int cx, int line, int num)
{
	return Draw_Character(cx + ((vid.width - MENU_WIDTH) >> 1), line, num);
}

/*
================
M_Print

Prints in the alternate (highlighted) half of the charset.
================
*/
void M_Print(int cx, int cy, const char *str)
{
	const byte	*s;

	for (s = (const byte *)str; *s; s++)
		cx += M_DrawCharacter(cx, cy, *s + 128);
}

/*
================
M_PrintWhite
================
*/
void M_PrintWhite(int cx, int cy, const char *str)
{
	const byte	*s;

	for (s = (const byte *)str; *s; s++)
		cx += M_DrawCharacter(cx, cy, *s);
}

/*
================
M_DrawTransPic
================
*/
void M_DrawTransPic(int x, int y, qpic_t *pic)
{
	Draw_TransPic(x + ((vid.width - MENU_WIDTH) >> 1), y, pic);
}

/*
================
M_DrawPic
================
*/
void M_DrawPic(int x, int y, qpic_t *pic)
{
	Draw_Pic(x + ((vid.width - MENU_WIDTH) >> 1), y, pic);
}

/*
================
M_BuildTranslationTable

Builds the player color translation for the setup menu preview.
================
*/
void M_BuildTranslationTable(int top, int bottom)
{
	int		j;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;

	Q_memcpy(translationTable, identityTable, 256);

	if (top < 128)	// the artists made some backwards ranges.  sigh.
		Q_memcpy(translationTable + TOP_RANGE, identityTable + top, 16);
	else
	{
		for (j = 0; j < 16; j++)
			translationTable[TOP_RANGE + j] = identityTable[top + 15 - j];
	}

	if (bottom < 128)
		Q_memcpy(translationTable + BOTTOM_RANGE, identityTable + bottom, 16);
	else
	{
		for (j = 0; j < 16; j++)
			translationTable[BOTTOM_RANGE + j] = identityTable[bottom + 15 - j];
	}
}

/*
================
M_DrawTransPicTranslate
================
*/
void M_DrawTransPicTranslate(int x, int y, qpic_t *pic)
{
	Draw_TransPicTranslate(x + ((vid.width - MENU_WIDTH) >> 1), y, pic);
}

/*
================
M_DrawTextBox

width is in characters; the middle is drawn two characters at a time.
================
*/
void M_DrawTextBox(int x, int y, int width, int lines)
{
	qpic_t	*p;
	int		cx, cy;
	int		n;
	int		columns;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic("gfx/box_tl.lmp");
	M_DrawTransPic(cx, cy, p);
	p = Draw_CachePic("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic(cx, cy, p);
	}
	p = Draw_CachePic("gfx/box_bl.lmp");
	M_DrawTransPic(cx, cy + 8, p);

	// draw middle
	cx += 8;
	columns = (width + 1) >> 1;
	for (n = 0; n < columns; n++)
	{
		cy = y;
		p = Draw_CachePic("gfx/box_tm.lmp");
		M_DrawTransPic(cx, cy, p);
		p = Draw_CachePic("gfx/box_mm.lmp");
		for (int row = 0; row < lines; row++)
		{
			cy += 8;
			if (row == 1)
				p = Draw_CachePic("gfx/box_mm2.lmp");
			M_DrawTransPic(cx, cy, p);
		}
		p = Draw_CachePic("gfx/box_bm.lmp");
		M_DrawTransPic(cx, cy + 8, p);
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic("gfx/box_tr.lmp");
	M_DrawTransPic(cx, cy, p);
	p = Draw_CachePic("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic(cx, cy, p);
	}
	p = Draw_CachePic("gfx/box_br.lmp");
	M_DrawTransPic(cx, cy + 8, p);
}

//=============================================================================

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f(void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state == m_main)
		{
			key_dest = key_game;
			m_state = m_none;
			return;
		}
	}
	else if (key_dest == key_console)
	{
		Con_ToggleConsole_f();
		return;
	}

	M_Menu_Main_f();
}

//=============================================================================
/* MAIN MENU */

/*
================
M_Menu_Main_f
================
*/
void M_Menu_Main_f(void)
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}

//=============================================================================
/* SINGLE PLAYER MENU */

/*
================
M_Menu_SinglePlayer_f
================
*/
void M_Menu_SinglePlayer_f(void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}

//=============================================================================
/* LOAD/SAVE MENU */

/*
================
M_Menu_Load_f
================
*/
void M_Menu_Load_f(void)
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves();
}

/*
================
M_Menu_Save_f
================
*/
void M_Menu_Save_f(void)
{
	if (!sv.active)
		return;
	if (host_in_intermission)
		return;
	if (svs.maxclients != 1)
		return;

	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves();
}

//=============================================================================
/* MULTIPLAYER MENU */

/*
================
M_Menu_MultiPlayer_f
================
*/
void M_Menu_MultiPlayer_f(void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}

//=============================================================================
/* SETUP MENU */

/*
================
M_Menu_Setup_f
================
*/
void M_Menu_Setup_f(void)
{
	int		color;

	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	Q_strcpy(setup_myname, cl_name.string);
	Q_strcpy(setup_hostname, hostname.string);
	color = (int)cl_color.value;
	setup_oldbottom = color & 15;
	setup_oldtop = color >> 4;
	setup_top = setup_oldtop;
	setup_bottom = setup_oldbottom;
}

//=============================================================================
/* NET MENU */

/*
================
M_Menu_Net_f
================
*/
void M_Menu_Net_f(void)
{
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;
	m_net_items = 4;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;
	m_net_cursor--;
	M_Net_Key(K_DOWNARROW);	// moves onto the first available item
}

//=============================================================================
/* OPTIONS MENU */

/*
================
M_Menu_Options_f
================
*/
void M_Menu_Options_f(void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}

//=============================================================================
/* KEYS MENU */

/*
================
M_Menu_Keys_f
================
*/
void M_Menu_Keys_f(void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}

//=============================================================================
/* VIDEO MENU */

/*
================
M_Menu_Video_f
================
*/
void M_Menu_Video_f(void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;
}

//=============================================================================
/* HELP MENU */

/*
================
M_Menu_Help_f

The help screens are not reachable in this build.
================
*/
void M_Menu_Help_f(void)
{
}

//=============================================================================
/* QUIT MENU */

/*
================
M_Menu_Quit_f
================
*/
void M_Menu_Quit_f(void)
{
	if (m_state == m_quit)
		return;
	m_quit_prevstate = m_state;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = rand() & 7;
}

//=============================================================================
/* SERIAL CONFIG MENU */

static const int	ISA_uarts[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
static const int	ISA_IRQs[] = { 4, 3, 4, 3 };
static const int	serialConfig_baudrate[] = { 9600, 14400, 19200, 28800, 38400, 57600 };

#define NUM_COMPORTS	4
#define NUM_BAUDRATES	6

/*
================
M_Menu_SerialConfig_f
================
*/
void M_Menu_SerialConfig_f(void)
{
	int		n;
	int		port;
	int		baudrate;

	key_dest = key_menu;
	m_state = m_serialconfig;
	m_entersound = true;
	if (m_multiplayer_cursor || m_net_cursor)
		serialConfig_cursor = 5;
	else
		serialConfig_cursor = 4;	// joining over a modem: start on the phone number

	port = (int)config_com_port.value;
	serialConfig_irq = (int)config_com_irq.value;
	baudrate = (int)config_com_baud.value;

	// map uart's port to COMx
	for (n = 0; n < NUM_COMPORTS; n++)
	{
		if (ISA_uarts[n] == port)
			break;
	}
	if (n == NUM_COMPORTS)
	{
		serialConfig_irq = 4;
		n = 0;
	}
	serialConfig_comport = n + 1;

	// map baudrate to index
	for (n = 0; n < NUM_BAUDRATES; n++)
	{
		if (serialConfig_baudrate[n] == baudrate)
			break;
	}
	if (n == NUM_BAUDRATES)
		n = NUM_BAUDRATES - 1;
	serialConfig_baud = n;

	m_return_onerror = false;
	m_return_reason[0] = 0;
}

//=============================================================================
/* MODEM CONFIG MENU */

/*
================
M_Menu_ModemConfig_f
================
*/
void M_Menu_ModemConfig_f(void)
{
	key_dest = key_menu;
	m_state = m_modemconfig;
	m_entersound = true;

	modemConfig_dialing = (config_modem_dialtype.string && config_modem_dialtype.string[0]) ? config_modem_dialtype.string[0] : 'T';
	Q_strncpy(modemConfig_clear, config_modem_clear.string, sizeof(modemConfig_clear));
	Q_strncpy(modemConfig_init, config_modem_init.string, sizeof(modemConfig_init));
	Q_strncpy(modemConfig_hangup, config_modem_hangup.string, sizeof(modemConfig_hangup));
}

//=============================================================================
/* LAN CONFIG MENU */

/*
================
M_Menu_LanConfig_f
================
*/
void M_Menu_LanConfig_f(void)
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame && TCPIPConfig)
			lanConfig_cursor = 2;
		else
			lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = hostshort;
	sprintf(lanConfig_portname, "%u", hostshort);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}

//=============================================================================
/* GAME OPTIONS MENU */

/*
================
M_Menu_GameOptions_f
================
*/
void M_Menu_GameOptions_f(void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}

//=============================================================================
/* SEARCH MENU */

/*
================
M_Menu_Search_f
================
*/
void M_Menu_Search_f(void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	Slist_Send();
}

//=============================================================================
/* SLIST MENU */

/*
================
M_Menu_ServerList_f
================
*/
void M_Menu_ServerList_f(void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	slist_sorted = false;
}

//=============================================================================
/* Menu Subsystem */

/*
================
M_Draw
================
*/
void M_Draw(void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (m_recursiveDraw)
	{
		m_recursiveDraw = false;
	}
	else
	{
		scr_copyeverything = 1;

		if (scr_con_current == 0.0f)
			Draw_FadeScreen();
		else
		{
			Draw_ConsoleBackground(vid.height);
			VID_ForceLockState2();
			S_ExtraUpdate();
			VID_HandlePause2();
		}

		scr_fullupdate = 0;
	}

	switch (m_state)
	{
	case m_main:
		M_Main_Draw();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw();
		break;

	case m_load:
		M_Load_Draw();
		break;

	case m_save:
		M_Save_Draw();
		break;

	case m_multiplayer:
		M_MultiPlayer_Draw();
		break;

	case m_setup:
		M_Setup_Draw();
		break;

	case m_net:
		M_Net_Draw();
		break;

	case m_options:
		M_Options_Draw();
		break;

	case m_video:
		M_Video_Draw();
		break;

	case m_keys:
		M_Keys_Draw();
		break;

	case m_help:
		M_Help_Draw();
		break;

	case m_quit:
		M_Quit_Draw();
		break;

	case m_serialconfig:
		M_SerialConfig_Draw();
		break;

	case m_modemconfig:
		M_ModemConfig_Draw();
		break;

	case m_lanconfig:
		M_LanConfig_Draw();
		break;

	case m_gameoptions:
		M_GameOptions_Draw();
		break;

	case m_search:
		M_Search_Draw();
		break;

	case m_slist:
		M_ServerList_Draw();
		break;

	default:
		break;
	}

	if (m_entersound)
	{
		S_LocalSound("common/menu2.wav");
		m_entersound = false;
	}

	VID_ForceLockState2();
	S_ExtraUpdate();
	VID_HandlePause2();
}

/*
================
M_Keydown
================
*/
void M_Keydown(int key)
{
	switch (m_state)
	{
	case m_main:
		M_Main_Key(key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key(key);
		return;

	case m_load:
		M_Load_Key(key);
		return;

	case m_save:
		M_Save_Key(key);
		return;

	case m_multiplayer:
		M_MultiPlayer_Key(key);
		return;

	case m_setup:
		M_Setup_Key(key);
		return;

	case m_net:
		M_Net_Key(key);
		return;

	case m_options:
		M_Options_Key(key);
		return;

	case m_video:
		M_Video_Key(key);
		return;

	case m_keys:
		M_Keys_Key(key);
		return;

	case m_help:
		M_Help_Key(key);
		return;

	case m_quit:
		M_Quit_Key(key);
		return;

	case m_serialconfig:
		M_SerialConfig_Key(key);
		return;

	case m_modemconfig:
		M_ModemConfig_Key(key);
		return;

	case m_lanconfig:
		M_LanConfig_Key(key);
		return;

	case m_gameoptions:
		M_GameOptions_Key(key);
		return;

	case m_search:
		// no key handler; the search ends by itself
		return;

	case m_slist:
		M_ServerList_Key(key);
		return;

	default:
		return;
	}
}

/*
================
M_Init
================
*/
void M_Init(void)
{
	Cmd_AddCommand("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand("menu_main", M_Menu_Main_f);
	Cmd_AddCommand("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand("menu_load", M_Menu_Load_f);
	Cmd_AddCommand("menu_save", M_Menu_Save_f);
	Cmd_AddCommand("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand("menu_options", M_Menu_Options_f);
	Cmd_AddCommand("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand("menu_video", M_Menu_Video_f);
	Cmd_AddCommand("help", M_Menu_Help_f);
	Cmd_AddCommand("menu_quit", M_Menu_Quit_f);
}

//=============================================================================

/*
================
M_Main_Draw
================
*/
static void M_Main_Draw(void)
{
	int		f;
	qpic_t	*p;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/ttl_main.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);
	p = Draw_CachePic("gfx/mainmenu.lmp");
	M_DrawTransPic(72, 32, p);

	f = (int)(realtime * 10.0) % NUM_MENUDOTS;
	p = Draw_CachePic(va("gfx/menudot%i.lmp", f + 1));
	M_DrawTransPic(54, 32 + m_main_cursor * 20, p);
}

/*
================
M_Main_Key
================
*/
static void M_Main_Key(int key)
{
	switch (key)
	{
	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_SinglePlayer_f();
			break;

		case 1:
			M_Menu_MultiPlayer_f();
			break;

		case 2:
			M_Menu_Options_f();
			break;

		case 4:
			M_Menu_Quit_f();
			break;
		}
		break;

	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		cls.demonum = m_save_demonum;
		if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
			CL_NextDemo();
		break;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;
	}
}

/*
================
M_SinglePlayer_Draw
================
*/
static void M_SinglePlayer_Draw(void)
{
	int		f;
	qpic_t	*p;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/ttl_sgl.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);
	p = Draw_CachePic("gfx/sp_menu.lmp");
	M_DrawTransPic(72, 32, p);

	f = (int)(realtime * 10.0) % NUM_MENUDOTS;
	p = Draw_CachePic(va("gfx/menudot%i.lmp", f + 1));
	M_DrawTransPic(54, 32 + m_singleplayer_cursor * 20, p);
}

/*
================
M_SinglePlayer_Key
================
*/
static void M_SinglePlayer_Key(int key)
{
	switch (key)
	{
	case K_ENTER:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active && !SCR_DrawDialog("Are you sure you want to start a new game?"))
				break;
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText("disconnect\n");
			Cbuf_AddText("maxplayers 1\n");
			Cbuf_AddText("map warez_dm\n");
			break;

		case 1:
			M_Menu_Load_f();
			break;

		case 2:
			M_Menu_Save_f();
			break;
		}
		break;

	case K_ESCAPE:
		M_Menu_Main_f();
		break;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;
	}
}

/*
================
M_ScanSaves
================
*/
static void M_ScanSaves(void)
{
	int		i, j;
	char	name[MAX_OSPATH];
	char	comment[80];
	FILE	*f;
	int		version;

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		Q_strcpy(m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		sprintf(name, "%s/slot%i.sav", com_gamedir, i);
		f = fopen(name, "r");
		if (!f)
			continue;

		version = 0;
		comment[0] = 0;
		if (fscanf(f, "%i\n", &version) != 1 || version != SAVEGAME_VERSION)
		{
			fclose(f);
			continue;
		}
		if (fscanf(f, "%79s\n", comment) != 1)
		{
			fclose(f);
			continue;
		}
		strncpy(m_filenames[i], comment, sizeof(m_filenames[i]) - 1);
		m_filenames[i][sizeof(m_filenames[i]) - 1] = 0;

		// change _ back to space
		for (j = 0; j < SAVEGAME_COMMENT_LENGTH - 1; j++)
		{
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		}
		loadable[i] = true;
		fclose(f);
	}
}

/*
================
M_Load_Draw
================
*/
static void M_Load_Draw(void)
{
	int		i;
	qpic_t	*p;

	p = Draw_CachePic("gfx/p_load.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print(16, 32 + 8 * i, m_filenames[i]);

	// line cursor
	M_DrawCharacter(8, 32 + load_cursor * 8, CURSOR_CHAR);
}

/*
================
M_Save_Draw
================
*/
static void M_Save_Draw(void)
{
	int		i;
	qpic_t	*p;

	p = Draw_CachePic("gfx/p_save.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print(16, 32 + 8 * i, m_filenames[i]);

	// line cursor
	M_DrawCharacter(8, 32 + load_cursor * 8, SAVE_CURSOR_CHAR);
}

/*
================
M_Load_Key
================
*/
static void M_Load_Key(int key)
{
	switch (key)
	{
	case K_ENTER:
		S_LocalSound("common/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

		SCR_BeginLoadingPlaque();
		Cbuf_AddText(va("load slot%i\n", load_cursor));
		return;

	case K_ESCAPE:
		M_Menu_SinglePlayer_f();
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound("common/menu1.wav");
		if (--load_cursor < 0)
			load_cursor = MAX_SAVEGAMES - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("common/menu1.wav");
		if (++load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

/*
================
M_Save_Key
================
*/
static void M_Save_Key(int key)
{
	switch (key)
	{
	case K_ENTER:
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText(va("save slot%i\n", load_cursor));
		return;

	case K_ESCAPE:
		M_Menu_SinglePlayer_f();
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound("common/menu1.wav");
		if (--load_cursor < 0)
			load_cursor = MAX_SAVEGAMES - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("common/menu1.wav");
		if (++load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

/*
================
M_MultiPlayer_Draw
================
*/
static void M_MultiPlayer_Draw(void)
{
	int		f;
	qpic_t	*p;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);
	p = Draw_CachePic("gfx/mp_menu.lmp");
	M_DrawTransPic(72, 32, p);

	f = (int)(realtime * 10.0) % NUM_MENUDOTS;
	p = Draw_CachePic(va("gfx/menudot%i.lmp", f + 1));
	M_DrawTransPic(54, 32 + m_multiplayer_cursor * 20, p);

	if (serialAvailable || ipxAvailable || tcpipAvailable)
		return;
	M_PrintWhite(52, 148, "No Communications Available");
}

/*
================
M_MultiPlayer_Key
================
*/
static void M_MultiPlayer_Key(int key)
{
	switch (key)
	{
	case K_ENTER:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
		case 1:
			if (serialAvailable || ipxAvailable || tcpipAvailable)
				M_Menu_Net_f();
			break;

		case 2:
			M_Menu_Setup_f();
			break;
		}
		break;

	case K_ESCAPE:
		M_Menu_Main_f();
		break;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;
	}
}

static const int	setup_cursor_table[] = { 40, 56, 80, 104, 140 };

/*
================
M_Setup_Draw
================
*/
static void M_Setup_Draw(void)
{
	qpic_t	*p;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);

	M_Print(64, 40, "Hostname");
	M_DrawTextBox(160, 32, 16, 1);
	M_Print(168, 40, setup_hostname);

	M_Print(64, 56, "Your Name");
	M_DrawTextBox(160, 48, 16, 1);
	M_Print(168, 56, setup_myname);

	M_Print(64, 80, "Shirt Color");
	M_Print(64, 104, "Pants Color");

	M_DrawTextBox(64, 140 - 8, 14, 1);
	M_Print(72, 140, "Accept Changes");

	p = Draw_CachePic("gfx/bigbox.lmp");
	M_DrawTransPic(160, 64, p);
	p = Draw_CachePic("gfx/menuplyr.lmp");
	M_BuildTranslationTable(setup_top * 16, setup_bottom * 16);
	M_DrawTransPicTranslate(172, 72, p);

	M_DrawCharacter(56, setup_cursor_table[setup_cursor], CURSOR_CHAR);

	if (setup_cursor == 0)
		M_DrawCharacter(168 + 8 * Q_strlen(setup_hostname), setup_cursor_table[0], CURSOR_CHAR);

	if (setup_cursor == 1)
		M_DrawCharacter(168 + 8 * Q_strlen(setup_myname), setup_cursor_table[1], CURSOR_CHAR);
}

/*
================
M_Setup_Key
================
*/
static void M_Setup_Key(int key)
{
	int		l;

	switch (key)
	{
	case K_ENTER:
		if (setup_cursor == 0 || setup_cursor == 1)
			return;

		if (setup_cursor == 2 || setup_cursor == 3)
			goto forward;

		// setup_cursor == 4 (OK)
		if (Q_strcmp(cl_name.string, setup_myname) != 0)
			Cbuf_AddText(va("name \"%s\"\n", setup_myname));
		if (Q_strcmp(hostname.string, setup_hostname) != 0)
			Cvar_Set("hostname", setup_hostname);
		if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
			Cbuf_AddText(va("color %i %i\n", setup_top, setup_bottom));
		m_entersound = true;
		M_Menu_MultiPlayer_f();
		break;

	case K_ESCAPE:
		M_Menu_MultiPlayer_f();
		return;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (Q_strlen(setup_hostname))
				setup_hostname[Q_strlen(setup_hostname) - 1] = 0;
		}

		if (setup_cursor == 1)
		{
			if (Q_strlen(setup_myname))
				setup_myname[Q_strlen(setup_myname) - 1] = 0;
		}
		break;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound("common/menu3.wav");
		if (setup_cursor == 2)
			setup_top--;
		if (setup_cursor == 3)
			setup_bottom--;
		break;

	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
forward:
		S_LocalSound("common/menu3.wav");
		if (setup_cursor == 2)
			setup_top++;
		if (setup_cursor == 3)
			setup_bottom++;
		break;

	default:
		if (key >= 32 && key <= 127)
		{
			if (setup_cursor == 0)
			{
				l = Q_strlen(setup_hostname);
				if (l < (int)sizeof(setup_hostname) - 1)
				{
					setup_hostname[l] = key;
					setup_hostname[l + 1] = 0;
				}
			}
			if (setup_cursor == 1)
			{
				l = Q_strlen(setup_myname);
				if (l < (int)sizeof(setup_myname) - 1)
				{
					setup_myname[l] = key;
					setup_myname[l + 1] = 0;
				}
			}
		}
	}

	if (setup_top > NUM_PLAYER_COLORS - 1)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = NUM_PLAYER_COLORS - 1;
	if (setup_bottom > NUM_PLAYER_COLORS - 1)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = NUM_PLAYER_COLORS - 1;
}

static const char *net_helpMessage[] =
{
	"                        ",
	" Two computers connected",
	"   through two modems.  ",
	"                        ",

	"                        ",
	" Two computers connected",
	" by a null-modem cable. ",
	"                        ",

	" Novell network LANs    ",
	" or Windows 95 DOS-box. ",
	"                        ",
	"(LAN=Local Area Network)",

	" Commonly used to play  ",
	" over the Internet, but ",
	" also used on a Local   ",
	" Area Network.          "
};

/*
================
M_Net_Draw
================
*/
static void M_Net_Draw(void)
{
	int		f;
	qpic_t	*p;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);

	if (serialAvailable)
	{
		p = Draw_CachePic("gfx/netmen1.lmp");
		M_DrawTransPic(72, 32, p);
		p = Draw_CachePic("gfx/netmen2.lmp");
		M_DrawTransPic(72, 51, p);
	}

	p = Draw_CachePic(ipxAvailable ? "gfx/netmen3.lmp" : "gfx/dim_ipx.lmp");
	M_DrawTransPic(72, 70, p);

	p = Draw_CachePic(tcpipAvailable ? "gfx/netmen4.lmp" : "gfx/dim_tcp.lmp");
	M_DrawTransPic(72, 89, p);

	if (m_net_items == 5)
	{
		p = Draw_CachePic("gfx/netmen5.lmp");
		M_DrawTransPic(72, 108, p);
	}

	M_DrawTextBox(56, 134, 24, 4);
	M_Print(64, 142, net_helpMessage[m_net_cursor * 4 + 0]);
	M_Print(64, 150, net_helpMessage[m_net_cursor * 4 + 1]);
	M_Print(64, 158, net_helpMessage[m_net_cursor * 4 + 2]);
	M_Print(64, 166, net_helpMessage[m_net_cursor * 4 + 3]);

	f = (int)(realtime * 10.0) % NUM_MENUDOTS;
	p = Draw_CachePic(va("gfx/menudot%i.lmp", f + 1));
	M_DrawTransPic(54, 32 + m_net_cursor * 20, p);
}

/*
================
M_Net_Key
================
*/
static void M_Net_Key(int key)
{
	do
	{
		switch (key)
		{
		case K_ENTER:
			m_entersound = true;
			if (SerialConfig || DirectConfig)
				M_Menu_SerialConfig_f();
			else if (IPXConfig || TCPIPConfig)
				M_Menu_LanConfig_f();
			break;

		case K_ESCAPE:
			M_Menu_MultiPlayer_f();
			break;

		case K_UPARROW:
			S_LocalSound("common/menu1.wav");
			if (--m_net_cursor < 0)
				m_net_cursor = m_net_items - 1;
			break;

		case K_DOWNARROW:
			S_LocalSound("common/menu1.wav");
			if (++m_net_cursor >= m_net_items)
				m_net_cursor = 0;
			break;
		}
	// skip the protocols that are not available
	} while ((SerialConfig && !serialAvailable)
		|| (DirectConfig && !serialAvailable)
		|| (IPXConfig && !ipxAvailable)
		|| (TCPIPConfig && !tcpipAvailable));
}

//=============================================================================
/* OPTIONS MENU */

/*
================
M_AdjustSliders
================
*/
static void M_AdjustSliders(int dir)
{
	S_LocalSound("common/menu3.wav");

	switch (options_cursor)
	{
	case 3:	// screen size
		scr_viewsize.value += dir * 10;
		if (scr_viewsize.value < 30)
			scr_viewsize.value = 30;
		if (scr_viewsize.value > 120)
			scr_viewsize.value = 120;
		Cvar_SetValue("viewsize", scr_viewsize.value);
		break;

	case 4:	// gamma
		cv_v_gamma.value += dir * -0.05f;
		if (cv_v_gamma.value < 0.5f)
			cv_v_gamma.value = 0.5f;
		if (cv_v_gamma.value > 1)
			cv_v_gamma.value = 1;
		Cvar_SetValue("gamma", cv_v_gamma.value);
		break;

	case 5:	// mouse speed
		sensitivity.value += dir * 0.5f;
		if (sensitivity.value < 1)
			sensitivity.value = 1;
		if (sensitivity.value > 11)
			sensitivity.value = 11;
		Cvar_SetValue("sensitivity", sensitivity.value);
		break;

	case 6:	// music volume
		bgmvolume.value += dir;
		if (bgmvolume.value < 0)
			bgmvolume.value = 0;
		if (bgmvolume.value > 1)
			bgmvolume.value = 1;
		Cvar_SetValue("bgmvolume", bgmvolume.value);
		break;

	case 7:	// sfx volume
		volume.value += dir * 0.1f;
		if (volume.value < 0)
			volume.value = 0;
		if (volume.value > 1)
			volume.value = 1;
		Cvar_SetValue("volume", volume.value);
		break;

	case 8:	// always run
		if (cl_forwardspeed.value <= 200)
		{
			Cvar_SetValue("cl_forwardspeed", 400);
			Cvar_SetValue("cl_backspeed", 400);
		}
		else
		{
			Cvar_SetValue("cl_forwardspeed", 200);
			Cvar_SetValue("cl_backspeed", 200);
		}
		break;

	case 9:	// invert mouse
		Cvar_SetValue("m_pitch", -m_pitch.value);
		break;

	case 10:	// lookspring
		Cvar_SetValue("lookspring", !lookspring.value);
		break;

	case 11:	// lookstrafe
		Cvar_SetValue("lookstrafe", !lookstrafe.value);
		break;

	case 13:	// _windowed_mouse
		Cvar_SetValue("_windowed_mouse", !_windowed_mouse.value);
		break;
	}
}

/*
================
M_DrawSlider
================
*/
static void M_DrawSlider(int x, int y, float range)
{
	int		i;

	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawCharacter(x - 8, y, SLIDER_LEFT_CHAR);
	for (i = 0; i < SLIDER_RANGE; i++)
		M_DrawCharacter(x + i * 8, y, SLIDER_MID_CHAR);
	M_DrawCharacter(x + i * 8, y, SLIDER_RIGHT_CHAR);
	M_DrawCharacter(x + (SLIDER_RANGE - 1) * 8 * range, y, SLIDER_KNOB_CHAR);
}

/*
================
M_DrawCheckbox
================
*/
static void M_DrawCheckbox(int x, int y, int on)
{
	if (on)
		M_Print(x, y, "on");
	else
		M_Print(x, y, "off");
}

/*
================
M_Options_Draw
================
*/
static void M_Options_Draw(void)
{
	float	r;
	qpic_t	*p;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);

	M_Print(16, 32, "    Customize controls");
	M_Print(16, 40, "         Go to console");
	M_Print(16, 48, "     Reset to defaults");

	M_Print(16, 56, "           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider(220, 56, r);

	M_Print(16, 64, "            Brightness");
	r = (1.0f - cv_v_gamma.value) / 0.5f;
	M_DrawSlider(220, 64, r);

	M_Print(16, 72, "           Mouse Speed");
	r = (sensitivity.value - 1) / 10;
	M_DrawSlider(220, 72, r);

	M_Print(16, 80, "       CD Music Volume");
	r = bgmvolume.value;
	M_DrawSlider(220, 80, r);

	M_Print(16, 88, "          Sound Volume");
	r = volume.value;
	M_DrawSlider(220, 88, r);

	M_Print(16, 96, "            Always Run");
	M_DrawCheckbox(220, 96, cl_forwardspeed.value > 200);

	M_Print(16, 104, "          Invert Mouse");
	M_DrawCheckbox(220, 104, m_pitch.value < 0);

	M_Print(16, 112, "            Lookspring");
	M_DrawCheckbox(220, 112, lookspring.value);

	M_Print(16, 120, "            Lookstrafe");
	M_DrawCheckbox(220, 120, lookstrafe.value);

	if (vid_menudrawfn)
		M_Print(16, 128, "         Video Options");

	if (!vid_fullscreen)
	{
		M_Print(16, 136, "             Use Mouse");
		M_DrawCheckbox(220, 136, _windowed_mouse.value);
	}

	// cursor
	M_DrawCharacter(200, 32 + options_cursor * 8, CURSOR_CHAR);
}

/*
================
M_Options_Key
================
*/
static void M_Options_Key(int key)
{
	switch (key)
	{
	case K_ENTER:
		m_entersound = true;
		if (options_cursor == 0)
		{
			M_Menu_Keys_f();
			return;
		}
		switch (options_cursor)
		{
		case 1:
			m_state = m_none;
			Con_ToggleConsole_f();
			break;
		case 2:
			Cbuf_AddText("exec default.cfg\n");
			break;
		case 12:
			M_Menu_Video_f();
			break;
		default:
			M_AdjustSliders(1);
			break;
		}
		return;

	case K_ESCAPE:
		M_Menu_Main_f();
		break;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--options_cursor < 0)
			options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_LEFTARROW:
		M_AdjustSliders(-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders(1);
		break;
	}

	// skip the items that are not available
	if (options_cursor == 12 && vid_menudrawfn == NULL)
		options_cursor = (key == K_UPARROW) ? 11 : 0;

	if (options_cursor == 13 && vid_fullscreen)
		options_cursor = (key == K_UPARROW) ? 12 : 0;
}

//=============================================================================
/* KEYS MENU */

static const char *bindnames[][2] =
{
	{ "+attack",	"attack" },
	{ "impulse 10",	"change weapon" },
	{ "+jump",		"jump / swim up" },
	{ "+forward",	"walk forward" },
	{ "+back",		"backpedal" },
	{ "+left",		"turn left" },
	{ "+right",		"turn right" },
	{ "+speed",		"run" },
	{ "+moveleft",	"step left" },
	{ "+moveright",	"step right" },
	{ "+strafe",	"sidestep" },
	{ "+lookup",	"look up" },
	{ "+lookdown",	"look down" },
	{ "centerview",	"center view" },
	{ "+mlook",		"mouse look" },
	{ "+klook",		"keyboard look" },
	{ "+moveup",	"swim up" },
	{ "+movedown",	"swim down" }
};

#define NUMCOMMANDS		(sizeof(bindnames) / sizeof(bindnames[0]))

/*
================
M_FindKeysForCommand
================
*/
static void M_FindKeysForCommand(const char *command, int *twokeys)
{
	int		count;
	int		l;
	int		j;

	twokeys[0] = twokeys[1] = -1;
	l = Q_strlen(command);
	count = 0;

	for (j = 0; j < 256; j++)
	{
		if (!keybindings[j])
			continue;
		if (!strncmp(keybindings[j], command, l))
		{
			twokeys[count] = j;
			if (++count == 2)
				break;
		}
	}
}

/*
================
M_UnbindCommand
================
*/
static void M_UnbindCommand(const char *command)
{
	int		j;
	int		l;

	l = Q_strlen(command);

	for (j = 0; j < 256; j++)
	{
		if (!keybindings[j])
			continue;
		if (!strncmp(keybindings[j], command, l))
			Key_SetBinding(j, "");
	}
}

/*
================
M_Keys_Draw
================
*/
static void M_Keys_Draw(void)
{
	int		i, y;
	int		keys[2];
	qpic_t	*p;

	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);

	if (bind_grab)
		M_Print(12, 32, "Press a key or button for this action");
	else
		M_Print(18, 32, "Enter to change, backspace to clear");

	// search for known bindings
	for (i = 0, y = 48; i < (int)NUMCOMMANDS; i++, y += 8)
	{
		const char	*name;
		int			x;

		M_Print(16, y, bindnames[i][1]);

		M_FindKeysForCommand(bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print(140, y, "???");
			continue;
		}

		name = Key_KeynumToString(keys[0]);
		M_Print(140, y, name);
		x = Q_strlen(name) * 8;
		if (keys[1] != -1)
		{
			M_Print(140 + x + 8, y, "or");
			name = Key_KeynumToString(keys[1]);
			M_Print(140 + x + 32, y, name);
		}
	}

	M_DrawCharacter(130, 48 + keys_cursor * 8, CURSOR_CHAR);
}

/*
================
M_Keys_Key
================
*/
static void M_Keys_Key(int key)
{
	char		cmd[80];
	int			keys[2];
	const char	*command;
	const char	*name;

	if (bind_grab)
	{
		// defining a key
		S_LocalSound("common/menu1.wav");
		if (key != K_ESCAPE && key != '`')
		{
			command = bindnames[keys_cursor][0];
			name = Key_KeynumToString(key);
			sprintf(cmd, "bind \"%s\" \"%s\"\n", name, command);
			Cbuf_InsertText(cmd);
		}

		bind_grab = false;
		return;
	}

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Options_f();
		break;

	case K_ENTER:	// go into bind mode
		M_FindKeysForCommand(bindnames[keys_cursor][0], keys);
		S_LocalSound("common/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand(bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:	// delete bindings
	case K_DEL:
		S_LocalSound("common/menu2.wav");
		M_UnbindCommand(bindnames[keys_cursor][0]);
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--keys_cursor < 0)
			keys_cursor = NUMCOMMANDS - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("common/menu1.wav");
		if (++keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;
	}
}

//=============================================================================
/* VIDEO MENU */

/*
================
M_Video_Draw
================
*/
static void M_Video_Draw(void)
{
	if (vid_menudrawfn)
		vid_menudrawfn();
}

/*
================
M_Video_Key
================
*/
static void M_Video_Key(int key)
{
	vid_menukeyfn(key);
}

//=============================================================================
/* HELP MENU */

/*
================
M_Help_Draw
================
*/
static void M_Help_Draw(void)
{
	M_DrawPic(0, 0, Draw_CachePic(va("gfx/help%i.lmp", load_cursor)));
}

/*
================
M_Help_Key
================
*/
static void M_Help_Key(int key)
{
	if (key == K_ESCAPE)
	{
		M_Menu_Main_f();
		return;
	}

	if (key == K_UPARROW || key == K_RIGHTARROW)
	{
		m_entersound = true;
		if (++load_cursor >= NUM_HELP_PAGES)
			load_cursor = 0;
		return;
	}

	if (key == K_DOWNARROW || key == K_LEFTARROW)
	{
		m_entersound = true;
		if (--load_cursor < 0)
			load_cursor = NUM_HELP_PAGES - 1;
	}
}

//=============================================================================
/* QUIT MENU */

/*
================
M_Quit_Draw
================
*/
static void M_Quit_Draw(void)
{
	qpic_t	*pic;

	if (wasInMenus)
	{
		m_recursiveDraw = true;
		m_state = m_quit_prevstate;
		M_Draw();
		m_state = m_quit;
	}

	if (W_GetLumpinfo("credits", false))
	{
		pic = W_GetLumpName("credits");
		Draw_Pic((vid.width - pic->width) >> 1, (vid.height - pic->height) >> 2, pic);
	}
	else
	{
		M_DrawTextBox(56, 76, 24, 4);
		M_Print(64, 92, "Press Y to exit");
	}
}

/*
================
M_Quit_Key
================
*/
static void M_Quit_Key(int key)
{
	if (key != K_ESCAPE && key != 'N')
	{
		// FIXME: lowercase 'n' quits as well
		if (key != 'Y' && key != 'n' && key != 'y')
			return;

		key_dest = key_console;
		CL_Disconnect();
		Host_ShutdownServer(false);
		Sys_Quit();
	}

	if (wasInMenus)
	{
		m_entersound = true;
		m_state = m_quit_prevstate;
	}
	else
	{
		key_dest = key_game;
		m_state = m_none;
	}
}

//=============================================================================
/* SERIAL CONFIG MENU */

static const int	serialConfig_cursor_table[] = { 40, 56, 72, 88, 104, 124 };

/*
================
M_SerialConfig_Draw
================
*/
static void M_SerialConfig_Draw(void)
{
	qpic_t	*p;
	int		basex;
	char	*startJoin;
	char	*directModem;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/p_multi.lmp");
	basex = (MENU_WIDTH - p->width) / 2;
	M_DrawPic(basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	M_Print(basex, 32, va("%s", startJoin));
	basex += 8;

	M_Print(basex, serialConfig_cursor_table[0], "Port");
	M_DrawTextBox(160, 40, 4, 1);
	M_Print(168, serialConfig_cursor_table[0], va("COM%u", serialConfig_comport));

	M_Print(basex, serialConfig_cursor_table[1], "IRQ");
	M_DrawTextBox(160, serialConfig_cursor_table[1] - 8, 1, 1);
	M_Print(168, serialConfig_cursor_table[1], va("%u", serialConfig_irq));

	M_Print(basex, serialConfig_cursor_table[2], "Baud");
	M_DrawTextBox(160, serialConfig_cursor_table[2] - 8, 5, 1);
	M_Print(168, serialConfig_cursor_table[2], va("%u", serialConfig_baudrate[serialConfig_baud]));

	if (SerialConfig)
	{
		M_Print(basex, serialConfig_cursor_table[3], "Modem Setup");
		if (JoiningGame)
		{
			M_Print(basex, serialConfig_cursor_table[4], "Phone Number");
			M_DrawTextBox(160, serialConfig_cursor_table[4] - 8, 16, 1);
			M_Print(168, serialConfig_cursor_table[4], serialConfig_phone);
		}
	}

	if (JoiningGame)
	{
		M_DrawTextBox(basex, serialConfig_cursor_table[5] - 8, 7, 1);
		M_Print(basex + 8, serialConfig_cursor_table[5], "Connect");
	}
	else
	{
		M_DrawTextBox(basex, serialConfig_cursor_table[5] - 8, 2, 1);
		M_Print(basex + 8, serialConfig_cursor_table[5], "OK");
	}

	M_DrawCharacter(basex - 8, serialConfig_cursor_table[serialConfig_cursor], CURSOR_CHAR);

	if (serialConfig_cursor == 4)
		M_DrawCharacter(168 + 8 * Q_strlen(serialConfig_phone), serialConfig_cursor_table[4], CURSOR_CHAR);

	if (*m_return_reason)
		M_PrintWhite(basex, 148, m_return_reason);
}

/*
================
M_SerialConfig_Key
================
*/
static void M_SerialConfig_Key(int key)
{
	int		l;

	switch (key)
	{
	case K_ENTER:
		if (serialConfig_cursor < 3)
			goto forward;

		m_entersound = true;

		if (serialConfig_cursor == 3)
		{
			NET_SetComPortConfig(0, ISA_uarts[serialConfig_comport - 1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

			M_Menu_ModemConfig_f();
			break;
		}

		if (serialConfig_cursor == 4)
		{
			serialConfig_cursor = 5;
			break;
		}

		// serialConfig_cursor == 5 (OK/CONNECT)
		NET_SetComPortConfig(0, ISA_uarts[serialConfig_comport - 1], serialConfig_irq, serialConfig_baudrate[serialConfig_baud], SerialConfig);

		M_ConfigureNetSubsystem();

		if (StartingGame)
		{
			M_Menu_GameOptions_f();
			break;
		}

		m_return_onerror = true;
		m_return_state = m_state;
		key_dest = key_game;
		m_state = m_none;

		if (!SerialConfig)
			Cbuf_AddText("connect\n");
		else
			Cbuf_AddText(va("connect %s\n", serialConfig_phone));
		break;

	case K_ESCAPE:
		M_Menu_Net_f();
		break;

	case K_BACKSPACE:
		if (serialConfig_cursor == 4)
		{
			if (Q_strlen(serialConfig_phone))
				serialConfig_phone[Q_strlen(serialConfig_phone) - 1] = 0;
		}
		break;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--serialConfig_cursor < 0)
			serialConfig_cursor = NUM_SERIALCONFIG_CMDS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++serialConfig_cursor >= NUM_SERIALCONFIG_CMDS)
			serialConfig_cursor = 0;
		break;

	case K_LEFTARROW:
		if (serialConfig_cursor > 2)
			break;
		S_LocalSound("common/menu3.wav");

		if (serialConfig_cursor == 0)
		{
			serialConfig_comport--;
			if (serialConfig_comport == 0)
				serialConfig_comport = NUM_COMPORTS;
			serialConfig_irq = ISA_IRQs[serialConfig_comport - 1];
		}

		if (serialConfig_cursor == 1)
		{
			serialConfig_irq--;
			if (serialConfig_irq == 6)
				serialConfig_irq = 5;
			if (serialConfig_irq == 1)
				serialConfig_irq = 7;
		}

		if (serialConfig_cursor == 2)
		{
			serialConfig_baud--;
			if (serialConfig_baud < 0)
				serialConfig_baud = NUM_BAUDRATES - 1;
		}
		break;

	case K_RIGHTARROW:
		if (serialConfig_cursor > 2)
			break;
forward:
		S_LocalSound("common/menu3.wav");

		if (serialConfig_cursor == 0)
		{
			serialConfig_comport++;
			if (serialConfig_comport > NUM_COMPORTS)
				serialConfig_comport = 1;
			serialConfig_irq = ISA_IRQs[serialConfig_comport - 1];
		}

		if (serialConfig_cursor == 1)
		{
			serialConfig_irq++;
			if (serialConfig_irq == 6)
				serialConfig_irq = 7;
			if (serialConfig_irq == 8)
				serialConfig_irq = 2;
		}

		if (serialConfig_cursor == 2)
		{
			serialConfig_baud++;
			if (serialConfig_baud > NUM_BAUDRATES - 1)
				serialConfig_baud = 0;
		}
		break;

	default:
		if (key >= 32 && key <= 127)
		{
			if (serialConfig_cursor == 4)
			{
				l = Q_strlen(serialConfig_phone);
				if (l < (int)sizeof(serialConfig_phone) - 1)
				{
					serialConfig_phone[l] = key;
					serialConfig_phone[l + 1] = 0;
				}
			}
		}
	}

	if (DirectConfig && (serialConfig_cursor == 3 || serialConfig_cursor == 4))
	{
		if (key == K_UPARROW)
			serialConfig_cursor = 2;
		else
			serialConfig_cursor = 5;
	}

	if (SerialConfig && StartingGame && serialConfig_cursor == 4)
	{
		if (key == K_UPARROW)
			serialConfig_cursor = 3;
		else
			serialConfig_cursor = 5;
	}
}

//=============================================================================
/* MODEM CONFIG MENU */

static const int	modemConfig_cursor_table[] = { 40, 56, 88, 120, 156 };

/*
================
M_ModemConfig_Draw
================
*/
static void M_ModemConfig_Draw(void)
{
	qpic_t	*p;
	int		basex;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/p_multi.lmp");
	basex = (MENU_WIDTH - p->width) / 2;
	M_DrawPic(basex, 4, p);
	basex += 8;

	if (modemConfig_dialing == 'P')
		M_Print(basex, modemConfig_cursor_table[0], "Pulse Dialing");
	else
		M_Print(basex, modemConfig_cursor_table[0], "Touch Tone Dialing");

	M_Print(basex, modemConfig_cursor_table[1], "Clear");
	M_DrawTextBox(basex, modemConfig_cursor_table[1] + 4, 16, 1);
	M_Print(basex + 8, modemConfig_cursor_table[1] + 12, modemConfig_clear);
	if (modemConfig_cursor == 1)
		M_DrawCharacter(basex + 8 + 8 * Q_strlen(modemConfig_clear), modemConfig_cursor_table[1] + 12, CURSOR_CHAR);

	M_Print(basex, modemConfig_cursor_table[2], "Init");
	M_DrawTextBox(basex, modemConfig_cursor_table[2] + 4, 30, 1);
	M_Print(basex + 8, modemConfig_cursor_table[2] + 12, modemConfig_init);
	if (modemConfig_cursor == 2)
		M_DrawCharacter(basex + 8 + 8 * Q_strlen(modemConfig_init), modemConfig_cursor_table[2] + 12, CURSOR_CHAR);

	M_Print(basex, modemConfig_cursor_table[3], "Hangup");
	M_DrawTextBox(basex, modemConfig_cursor_table[3] + 4, 16, 1);
	M_Print(basex + 8, modemConfig_cursor_table[3] + 12, modemConfig_hangup);
	if (modemConfig_cursor == 3)
		M_DrawCharacter(basex + 8 + 8 * Q_strlen(modemConfig_hangup), modemConfig_cursor_table[3] + 12, CURSOR_CHAR);

	M_DrawTextBox(basex, modemConfig_cursor_table[4] - 8, 2, 1);
	M_Print(basex + 8, modemConfig_cursor_table[4], "OK");

	M_DrawCharacter(basex - 8, modemConfig_cursor_table[modemConfig_cursor], CURSOR_CHAR);
}

/*
================
M_ModemConfig_Key
================
*/
static void M_ModemConfig_Key(int key)
{
	int		l;

	switch (key)
	{
	case K_ENTER:
		if (modemConfig_cursor == 0)
		{
			if (modemConfig_dialing == 'P')
				modemConfig_dialing = 'T';
			else
				modemConfig_dialing = 'P';
			m_entersound = true;
		}

		if (modemConfig_cursor == 4)
		{
			NET_SetModemConfig(0, va("%c", modemConfig_dialing), modemConfig_clear, modemConfig_init, modemConfig_hangup);
			m_entersound = true;
			M_Menu_SerialConfig_f();
		}
		break;

	case K_ESCAPE:
		M_Menu_SerialConfig_f();
		break;

	case K_BACKSPACE:
		if (modemConfig_cursor == 1)
		{
			if (Q_strlen(modemConfig_clear))
				modemConfig_clear[Q_strlen(modemConfig_clear) - 1] = 0;
		}

		if (modemConfig_cursor == 2)
		{
			if (Q_strlen(modemConfig_init))
				modemConfig_init[Q_strlen(modemConfig_init) - 1] = 0;
		}

		if (modemConfig_cursor == 3)
		{
			if (Q_strlen(modemConfig_hangup))
				modemConfig_hangup[Q_strlen(modemConfig_hangup) - 1] = 0;
		}
		break;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--modemConfig_cursor < 0)
			modemConfig_cursor = NUM_MODEMCONFIG_CMDS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++modemConfig_cursor >= NUM_MODEMCONFIG_CMDS)
			modemConfig_cursor = 0;
		break;

	default:
		if (key == K_LEFTARROW || key == K_RIGHTARROW)
		{
			if (modemConfig_cursor == 0)
			{
				if (modemConfig_dialing == 'P')
					modemConfig_dialing = 'T';
				else
					modemConfig_dialing = 'P';
				S_LocalSound("common/menu1.wav");
			}
		}
		else if (key >= 32 && key <= 127)
		{
			if (modemConfig_cursor == 1)
			{
				l = Q_strlen(modemConfig_clear);
				if (l < (int)sizeof(modemConfig_clear) - 1)
				{
					modemConfig_clear[l] = key;
					modemConfig_clear[l + 1] = 0;
				}
			}

			if (modemConfig_cursor == 2)
			{
				l = Q_strlen(modemConfig_init);
				if (l < (int)sizeof(modemConfig_init) - 1)
				{
					modemConfig_init[l] = key;
					modemConfig_init[l + 1] = 0;
				}
			}

			if (modemConfig_cursor == 3)
			{
				l = Q_strlen(modemConfig_hangup);
				if (l < (int)sizeof(modemConfig_hangup) - 1)
				{
					modemConfig_hangup[l] = key;
					modemConfig_hangup[l + 1] = 0;
				}
			}
		}
	}
}

//=============================================================================
/* LAN CONFIG MENU */

static const int	lanConfig_cursor_table[] = { 72, 92, 124 };

/*
================
M_LanConfig_Draw
================
*/
static void M_LanConfig_Draw(void)
{
	qpic_t	*p;
	int		basex;
	char	*startJoin;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/p_multi.lmp");
	basex = (MENU_WIDTH - p->width) / 2;
	M_DrawPic(basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	M_Print(basex, 32, va("%s", startJoin));
	basex += 8;

	M_Print(basex, 52, "Address");
	if (IPXConfig)
		M_Print(basex + 9 * 8, 52, my_ipx_address);
	else
		M_Print(basex + 9 * 8, 52, my_tcpip_address);

	M_Print(basex, lanConfig_cursor_table[0], "Port");
	M_DrawTextBox(basex + 8 * 8, lanConfig_cursor_table[0] - 8, 6, 1);
	M_Print(basex + 9 * 8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print(basex, lanConfig_cursor_table[1], "Search for local games");
		M_Print(basex, 108, "Join game at:");
		M_DrawTextBox(basex + 8, lanConfig_cursor_table[2] - 8, 22, 1);
		M_Print(basex + 16, lanConfig_cursor_table[2], lanConfig_joinname);
	}
	else
	{
		M_DrawTextBox(basex, lanConfig_cursor_table[1] - 8, 2, 1);
		M_Print(basex + 8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter(basex - 8, lanConfig_cursor_table[lanConfig_cursor], CURSOR_CHAR);

	if (lanConfig_cursor == 0)
		M_DrawCharacter(basex + 9 * 8 + 8 * Q_strlen(lanConfig_portname), lanConfig_cursor_table[0], CURSOR_CHAR);

	if (lanConfig_cursor == 2)
		M_DrawCharacter(basex + 16 + 8 * Q_strlen(lanConfig_joinname), lanConfig_cursor_table[2], CURSOR_CHAR);

	if (*m_return_reason)
		M_PrintWhite(basex, 148, m_return_reason);
}

/*
================
M_LanConfig_Key
================
*/
static void M_LanConfig_Key(int key)
{
	int		l;

	switch (key)
	{
	case K_ENTER:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_ConfigureNetSubsystem();

		if (lanConfig_cursor == 1)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f();
				break;
			}
			M_Menu_Search_f();
			break;
		}

		if (lanConfig_cursor == 2)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText(va("connect %s\n", lanConfig_joinname));
			break;
		}
		break;

	case K_ESCAPE:
		M_Menu_Net_f();
		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (Q_strlen(lanConfig_portname))
				lanConfig_portname[Q_strlen(lanConfig_portname) - 1] = 0;
		}

		if (lanConfig_cursor == 2)
		{
			if (Q_strlen(lanConfig_joinname))
				lanConfig_joinname[Q_strlen(lanConfig_joinname) - 1] = 0;
		}
		break;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	default:
		if (key >= 32 && key <= 127)
		{
			if (lanConfig_cursor == 2)
			{
				l = Q_strlen(lanConfig_joinname);
				if (l < (int)sizeof(lanConfig_joinname) - 1)
				{
					lanConfig_joinname[l] = key;
					lanConfig_joinname[l + 1] = 0;
				}
			}

			if (key >= '0' && key <= '9' && !lanConfig_cursor)
			{
				l = Q_strlen(lanConfig_portname);
				if (l < (int)sizeof(lanConfig_portname) - 1)
				{
					lanConfig_portname[l] = key;
					lanConfig_portname[l + 1] = 0;
				}
			}
		}
	}

	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = (key == K_UPARROW);

	lanConfig_port = Q_atoi(lanConfig_portname);
	if (lanConfig_port >= 0 && lanConfig_port <= 65535)
		hostshort = lanConfig_port;
	sprintf(lanConfig_portname, "%u", hostshort);
}

//=============================================================================
/* GAME OPTIONS MENU */

static const int	gameoptions_cursor_table[] = { 40, 56, 64, 72, 80, 88, 96 };

/*
================
M_GameOptions_Draw
================
*/
static void M_GameOptions_Draw(void)
{
	qpic_t	*p;

	p = Draw_CachePic("gfx/qplaque.lmp");
	M_DrawTransPic(16, 4, p);
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);

	M_DrawTextBox(152, 32, 10, 1);
	M_Print(160, 40, "Begin Game");

	M_Print(0, 56, "Max Players");
	M_Print(160, 56, va("%i", maxplayers));

	M_Print(0, 64, "Game Type");
	M_Print(160, 64, (coop.value == 0) ? "Deathmatch" : "Cooperative");

	M_Print(0, 72, "Teamplay");
	if ((int)teamplay.value == 1)
		M_Print(160, 72, "No Friendly Fire");
	else if ((int)teamplay.value == 2)
		M_Print(160, 72, "Friendly Fire");
	else
		M_Print(160, 72, "Off");

	M_Print(0, 80, "Skill");
	if (skill.value == 0)
		M_Print(160, 80, "Easy difficulty");
	else if (skill.value == 1)
		M_Print(160, 80, "Normal difficulty");
	else if (skill.value == 2)
		M_Print(160, 80, "Hard difficulty");
	else
		M_Print(160, 80, "Nightmare difficulty");

	M_Print(0, 88, "Frag Limit");
	if (fraglimit.value == 0)
		M_Print(160, 88, "none");
	else
		M_Print(160, 88, va("%i frags", (int)fraglimit.value));

	M_Print(0, 96, "Time Limit");
	if (timelimit.value == 0)
		M_Print(160, 96, "none");
	else
		M_Print(160, 96, va("%i minutes", (int)timelimit.value));

	// line cursor
	M_DrawCharacter(144, gameoptions_cursor_table[gameoptions_cursor], CURSOR_CHAR);

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) >= 5.0)
		{
			m_serverInfoMessage = false;
		}
		else
		{
			M_DrawTextBox(56, 138, 24, 4);
			M_Print(64, 146, "More than 4 players");
			M_Print(64, 154, "requires using command");
			M_Print(64, 162, "line parameters.");
			M_Print(64, 170, "See techinfo.txt.");
		}
	}
}

/*
================
M_NetStart_Change
================
*/
static void M_NetStart_Change(int dir)
{
	int		count;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		Cvar_SetValue("coop", coop.value == 0);
		break;

	case 3:
		count = rogue ? 6 : 2;

		Cvar_SetValue("teamplay", teamplay.value + dir);
		if (teamplay.value > count)
			Cvar_SetValue("teamplay", 0);
		if (teamplay.value < 0)
			Cvar_SetValue("teamplay", count);
		break;

	case 4:
		Cvar_SetValue("skill", skill.value + dir);
		if (skill.value > 3)
			Cvar_SetValue("skill", 0);
		if (skill.value < 0)
			Cvar_SetValue("skill", 3);
		break;

	case 5:
		Cvar_SetValue("fraglimit", fraglimit.value + dir * 10);
		if (fraglimit.value > 100)
			Cvar_SetValue("fraglimit", 0);
		if (fraglimit.value < 0)
			Cvar_SetValue("fraglimit", 100);
		break;

	case 6:
		Cvar_SetValue("timelimit", timelimit.value + dir * 5);
		if (timelimit.value > 60)
			Cvar_SetValue("timelimit", 0);
		if (timelimit.value < 0)
			Cvar_SetValue("timelimit", 60);
		break;
	}
}

/*
================
M_GameOptions_Key
================
*/
static void M_GameOptions_Key(int key)
{
	switch (key)
	{
	case K_ENTER:
		S_LocalSound("common/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText("disconnect\n");
			Cbuf_AddText("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText(va("maxplayers %u\n", maxplayers));
			SCR_BeginLoadingPlaque();
			Cbuf_AddText("map warez_dm\n");
			return;
		}

		M_NetStart_Change(1);
		break;

	case K_ESCAPE:
		M_Menu_Net_f();
		return;

	case K_UPARROW:
		S_LocalSound("common/menu1.wav");
		if (--gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("common/menu1.wav");
		if (++gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound("common/menu3.wav");
		M_NetStart_Change(-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound("common/menu3.wav");
		M_NetStart_Change(1);
		break;
	}
}

//=============================================================================
/* SEARCH MENU */

/*
================
M_Search_Draw
================
*/
static void M_Search_Draw(void)
{
	qpic_t	*p;

	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);
	M_DrawTextBox(108, 32, 12, 1);
	M_Print(116, 40, "Searching");

	if (slistInProgress)
	{
		NET_Poll();
		return;
	}

	if (!searchComplete)
	{
		searchComplete = true;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_ServerList_f();
		return;
	}

	M_PrintWhite(72, 64, "No Quake servers found");
	if ((realtime - searchCompleteTime) >= 3.0)
		M_Menu_LanConfig_f();
}

//=============================================================================
/* SLIST MENU */

/*
================
M_ServerList_Draw
================
*/
static void M_ServerList_Draw(void)
{
	int		n;
	char	string[64];
	qpic_t	*p;

	if (!slist_sorted)
	{
		if (hostCacheCount > 1)
		{
			int			i, j;
			hostcache_t	temp;

			for (i = 0; i < hostCacheCount; i++)
			{
				for (j = i + 1; j < hostCacheCount; j++)
				{
					if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
					{
						temp = hostcache[i];
						hostcache[i] = hostcache[j];
						hostcache[j] = temp;
					}
				}
			}
		}
		slist_sorted = true;
	}

	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((MENU_WIDTH - p->width) / 2, 4, p);
	for (n = 0; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			sprintf(string, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			sprintf(string, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
		M_Print(16, 32 + 8 * n, string);
	}
	M_DrawCharacter(0, 32 + slist_cursor * 8, CURSOR_CHAR);

	if (*m_return_reason)
		M_PrintWhite(16, 148, m_return_reason);
}

/*
================
M_ServerList_Key
================
*/
static void M_ServerList_Key(int key)
{
	switch (key)
	{
	case K_ENTER:
		S_LocalSound("common/menu2.wav");
		m_return_onerror = true;
		m_return_state = m_state;
		slist_sorted = false;
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText(va("connect %s\n", hostcache[slist_cursor].cname));
		break;

	case K_ESCAPE:
		M_Menu_LanConfig_f();
		break;

	case K_SPACE:
		M_Menu_Search_f();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound("common/menu1.wav");
		if (--slist_cursor < 0)
			slist_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("common/menu1.wav");
		if (++slist_cursor >= hostCacheCount)
			slist_cursor = 0;
		break;
	}
}

/*
================
M_ConfigureNetSubsystem
================
*/
static void M_ConfigureNetSubsystem(void)
{
	// enable/disable net systems to match desired config
	Cbuf_AddText("stopdemo\n");
	if (SerialConfig || DirectConfig)
		Cbuf_AddText("com1 enable\n");

	if (IPXConfig || TCPIPConfig)
		hostshort = lanConfig_port;
}
