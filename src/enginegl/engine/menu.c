/***
*
*     Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*     This product contains software technology licensed from Id
*     Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*     All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

#include "quakedef.h"
#include "host.h"
#include "nullsubs.h"

#define MAX_SAVEGAMES 12

menu_state_t m_state;
int m_return_state;
qboolean m_return_onerror;
char m_return_reason[60];

static qboolean m_entersound;
static qboolean m_recursiveDraw;
static int m_save_demonum;

static int m_main_cursor;
static int m_singleplayer_cursor;
static int m_multiplayer_cursor;

static int m_loadsave_cursor;

static int m_options_cursor;
static int m_keys_cursor;
static qboolean m_keys_bind_grab;

static int m_net_cursor;
static int m_net_items;

static int m_quit_prevstate;
static int m_quit_message;
static qboolean m_quit_wasopen;

static int m_serialconfig_cursor;
static int m_modemconfig_cursor;
static int m_lanconfig_cursor = -1;

static int gameoptions_cursor;
static int gameoptions_maxplayers;
static qboolean gameoptions_showwarning;
static double gameoptions_warntime;

static qboolean searchtime_set;
static double searchtime;

static int slist_cursor;
static qboolean slist_sorted;

static char m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH];
static int m_filevalid[MAX_SAVEGAMES];

static char setup_hostname[16];
static char setup_myname[16];
static int setup_top;
static int setup_bottom;
static int setup_oldtop;
static int setup_oldbottom;
static int setup_cursor;

static char serialconfig_phone[16];
static int serialconfig_comport;
static int serialconfig_baud;
static int serialconfig_irq;

static char modem_dialtype;
static char modem_clear[16];
static char modem_init[30];
static char modem_hangup[16];

static char lanconfig_joinname[22];
static char lanconfig_portname[6];
static int lanconfig_port;

byte identityTable[256];
byte translationTable[256];

extern int host_in_intermission;

extern server_t sv;
extern server_static_t svs;

extern client_static_t cls;

extern double realtime;

extern cvar_t scr_viewsize;
extern cvar_t sensitivity;
extern cvar_t bgmvolume;
extern cvar_t volume;
extern cvar_t cl_forwardspeed;
extern cvar_t cl_backspeed;
extern cvar_t m_pitch;
extern cvar_t lookspring;
extern cvar_t lookstrafe;
extern cvar_t _windowed_mouse;
extern cvar_t cv_v_gamma;

extern cvar_t coop;
extern cvar_t teamplay;
extern cvar_t skill;
extern cvar_t fraglimit;
extern cvar_t timelimit;

extern cvar_t cl_name;
extern cvar_t hostname;
extern cvar_t cl_color;

extern void (*vid_menudrawfn)(void);
extern int (*vid_menukeyfn)(int key);
extern int vid_fullscreen;

extern unsigned short hostshort;

extern cvar_t config_com_port;
extern cvar_t config_com_irq;
extern cvar_t config_com_baud;
extern cvar_t config_com_modem;
extern cvar_t config_modem_dialtype;
extern cvar_t config_modem_clear;
extern cvar_t config_modem_init;
extern cvar_t config_modem_hangup;

extern void (*NET_SetComPortConfig)(int mode, int port, int irq, int baud, qboolean useModem);
extern void (*NET_SetModemConfig)(int mode, const char *dialtype, const char *clear, const char *init, const char *hangup);

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
static void M_NetStart_Change(int dir);

static void M_Search_Draw(void);
static void M_ServerList_Draw(void);
static void M_ServerList_Key(int key);

static void M_ConfigureNetSubsystem(void);

int M_DrawCharacter(int x, int y, int num)
{
      return Draw_Character(x + (((unsigned int)vid.width - 320u) >> 1), y, (unsigned char)num);
}

void M_Print(int x, int y, const char *str)
{
      for (const unsigned char *s = (const unsigned char *)str; *s; ++s)
              x += M_DrawCharacter(x, y, (int)(*s + 0x80));
}

void M_PrintWhite(int x, int y, const char *str)
{
      for (const unsigned char *s = (const unsigned char *)str; *s; ++s)
              x += M_DrawCharacter(x, y, (int)*s);
}

void M_DrawTransPic(int x, int y, qpic_t *pic)
{
      Draw_TransPic(x + (((unsigned int)vid.width - 320u) >> 1), y, pic);
}

void M_DrawPic(int x, int y, qpic_t *pic)
{
      Draw_Pic(x + (((unsigned int)vid.width - 320u) >> 1), y, pic);
}

void M_BuildTranslationTable(int top, int bottom)
{
      int j;

      for (j = 0; j < 256; ++j)
              identityTable[j] = (byte)j;

      Q_memcpy(translationTable, identityTable, 256);

      if (top < 128)
              Q_memcpy(translationTable + TOP_RANGE, identityTable + top, 16);
      else
      {
              for (j = 0; j < 16; ++j)
                      translationTable[TOP_RANGE + j] = identityTable[top + 15 - j];
      }

      if (bottom < 128)
              Q_memcpy(translationTable + BOTTOM_RANGE, identityTable + bottom, 16);
      else
      {
              for (j = 0; j < 16; ++j)
                      translationTable[BOTTOM_RANGE + j] = identityTable[bottom + 15 - j];
      }
}

void M_DrawTransPicTranslate(int x, int y, qpic_t *pic)
{
      Draw_TransPicTranslate(x + (((unsigned int)vid.width - 320u) >> 1), y, pic);
}

void M_DrawTextBox(int x, int y, int width, int lines)
{
      qpic_t *p;
      int cx;
      int cy;
      int n;
      int columns;

      cx = x;
      cy = y;
      p = Draw_CachePic("gfx/box_tl.lmp");
      M_DrawTransPic(cx, cy, p);
      p = Draw_CachePic("gfx/box_ml.lmp");
      for (n = 0; n < lines; ++n)
      {
              cy += 8;
              M_DrawTransPic(cx, cy, p);
      }
      p = Draw_CachePic("gfx/box_bl.lmp");
      M_DrawTransPic(cx, cy + 8, p);

      cx += 8;
      columns = (width + 1) >> 1;
      for (n = 0; n < columns; ++n)
      {
              cy = y;
              p = Draw_CachePic("gfx/box_tm.lmp");
              M_DrawTransPic(cx, cy, p);
              p = Draw_CachePic("gfx/box_mm.lmp");
              for (int row = 0; row < lines; ++row)
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

      cy = y;
      p = Draw_CachePic("gfx/box_tr.lmp");
      M_DrawTransPic(cx, cy, p);
      p = Draw_CachePic("gfx/box_mr.lmp");
      for (n = 0; n < lines; ++n)
      {
              cy += 8;
              M_DrawTransPic(cx, cy, p);
      }
      p = Draw_CachePic("gfx/box_br.lmp");
      M_DrawTransPic(cx, cy + 8, p);
}

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

void M_Menu_SinglePlayer_f(void)
{
      key_dest = key_menu;
      m_state = m_singleplayer;
      m_entersound = true;
}

void M_Menu_Load_f(void)
{
      m_entersound = true;
      m_state = m_load;
      key_dest = key_menu;

      M_ScanSaves();
}

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

void M_Menu_MultiPlayer_f(void)
{
      key_dest = key_menu;
      m_state = m_multiplayer;
      m_entersound = true;
}

void M_Menu_Setup_f(void)
{
      int playercolor;

      key_dest = key_menu;
      m_state = m_setup;
      m_entersound = true;

      Q_strcpy(setup_myname, cl_name.string);
      Q_strcpy(setup_hostname, hostname.string);

      playercolor = (int)cl_color.value;
      setup_oldbottom = playercolor & 0x0F;
      setup_oldtop = playercolor >> 4;
      setup_top = setup_oldtop;
      setup_bottom = setup_oldbottom;
}

void M_Menu_Net_f(void)
{

      key_dest = key_menu;
      m_state = m_net;
      m_entersound = true;
      m_net_items = 4;

      if (m_net_cursor >= m_net_items)
              m_net_cursor = 0;

      --m_net_cursor;
      M_Net_Key(K_DOWNARROW);
}

void M_Menu_Options_f(void)
{
      key_dest = key_menu;
      m_state = m_options;
      m_entersound = true;
}

void M_Menu_Keys_f(void)
{
      key_dest = key_menu;
      m_state = m_keys;
      m_entersound = true;
}

void M_Menu_Video_f(void)
{
      key_dest = key_menu;
      m_state = m_video;
      m_entersound = true;
}

void M_Menu_Help_f(void)
{

}

void M_Menu_Quit_f(void)
{
      if (m_state == m_quit)
              return;

      m_quit_prevstate = m_state;
      m_quit_wasopen = (key_dest == key_menu);

      key_dest = key_menu;
      m_state = m_quit;
      m_entersound = true;

      m_quit_message = rand() & 7;
}

void M_Menu_SerialConfig_f(void)
{
      int port;
      int baud;
      qboolean useModem;
      int i;

      key_dest = key_menu;
      m_state = m_serialconfig;
      m_entersound = true;

      if (m_multiplayer_cursor || m_net_cursor)
              m_serialconfig_cursor = 5;
      else
              m_serialconfig_cursor = 4;

      port = (int)config_com_port.value;
      serialconfig_irq = (int)config_com_irq.value;
      baud = (int)config_com_baud.value;
      useModem = (config_com_modem.value != 0.0f);

      static const int isa_uarts[4] = {0x3F8, 0x2F8, 0x3E8, 0x2E8};

      for (i = 0; i < 4; ++i)
      {
              if (isa_uarts[i] == port)
                      break;
      }

      if (i == 4)
      {
              serialconfig_irq = 4;
              i = 0;
      }

      serialconfig_comport = i + 1;

      static const int serial_baudrates[6] = {9600, 14400, 19200, 28800, 38400, 57600};

      for (i = 0; i < 6; ++i)
      {
              if (serial_baudrates[i] == baud)
                      break;
      }

      if (i == 6)
              i = 5;

      serialconfig_baud = i;

      m_return_onerror = false;
      m_return_reason[0] = 0;
}

void M_Menu_ModemConfig_f(void)
{
      key_dest = key_menu;
      m_state = m_modemconfig;
      m_entersound = true;

      modem_dialtype = (config_modem_dialtype.string && config_modem_dialtype.string[0]) ? config_modem_dialtype.string[0] : 'T';
      Q_strncpy(modem_clear, config_modem_clear.string, sizeof(modem_clear));
      Q_strncpy(modem_init, config_modem_init.string, sizeof(modem_init));
      Q_strncpy(modem_hangup, config_modem_hangup.string, sizeof(modem_hangup));
}

void M_Menu_LanConfig_f(void)
{
      key_dest = key_menu;
      m_state = m_lanconfig;
      m_entersound = true;

      if (m_lanconfig_cursor == -1)
      {
              if (m_multiplayer_cursor || m_net_cursor != 3)
                      m_lanconfig_cursor = 1;
              else
                      m_lanconfig_cursor = 2;
      }

      if (m_multiplayer_cursor == 1 && m_lanconfig_cursor == 2)
              m_lanconfig_cursor = 1;

      lanconfig_port = (int)hostshort;
      sprintf(lanconfig_portname, "%u", (unsigned int)hostshort);

      m_return_onerror = false;
      m_return_reason[0] = 0;
}

void M_Menu_GameOptions_f(void)
{
      key_dest = key_menu;
      m_state = m_gameoptions;
      m_entersound = true;

      if (!gameoptions_maxplayers)
              gameoptions_maxplayers = svs.maxclients;

      if (gameoptions_maxplayers < 2)
              gameoptions_maxplayers = svs.maxclientslimit;
}

void M_Menu_Search_f(void)
{
      key_dest = key_menu;
      m_state = m_search;
      m_entersound = false;

      extern int slistSilent;
      extern int slistComplete;

      slistSilent = 1;
      slistComplete = 0;
      searchtime_set = false;

      Slist_Send();
}

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

void M_Draw(void)
{
      if (!m_state || key_dest != key_menu)
              return;

      if (m_recursiveDraw)
      {
              m_recursiveDraw = false;
      }
      else
      {
              extern int scr_copyeverything;
              extern float scr_con_current;
              extern int scr_fullupdate;

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
              case m_main:                    M_Main_Draw(); break;
              case m_singleplayer:    M_SinglePlayer_Draw(); break;
              case m_load:                    M_Load_Draw(); break;
              case m_save:                    M_Save_Draw(); break;
              case m_multiplayer:             M_MultiPlayer_Draw(); break;
              case m_setup:                   M_Setup_Draw(); break;
              case m_net:                             M_Net_Draw(); break;
              case m_options:                 M_Options_Draw(); break;
              case m_video:                   M_Video_Draw(); break;
              case m_keys:                    M_Keys_Draw(); break;
              case m_help:                    M_Help_Draw(); break;
              case m_quit:                    M_Quit_Draw(); break;
              case m_serialconfig:    M_SerialConfig_Draw(); break;
              case m_modemconfig:             M_ModemConfig_Draw(); break;
              case m_lanconfig:               M_LanConfig_Draw(); break;
              case m_gameoptions:             M_GameOptions_Draw(); break;
              case m_search:                  M_Search_Draw(); break;
              case m_slist:                   M_ServerList_Draw(); break;
              default: break;
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

void M_Keydown(int key)
{
      switch (m_state)
      {
              case m_main:                    M_Main_Key(key); break;
              case m_singleplayer:    M_SinglePlayer_Key(key); break;
              case m_load:                    M_Load_Key(key); break;
              case m_save:                    M_Save_Key(key); break;
              case m_multiplayer:             M_MultiPlayer_Key(key); break;
              case m_setup:                   M_Setup_Key(key); break;
              case m_net:                             M_Net_Key(key); break;
              case m_options:                 M_Options_Key(key); break;
              case m_video:                   M_Video_Key(key); break;
              case m_keys:                    M_Keys_Key(key); break;
              case m_help:                    M_Help_Key(key); break;
              case m_quit:                    M_Quit_Key(key); break;
              case m_serialconfig:    M_SerialConfig_Key(key); break;
              case m_modemconfig:             M_ModemConfig_Key(key); break;
              case m_lanconfig:               M_LanConfig_Key(key); break;
              case m_gameoptions:             M_GameOptions_Key(key); break;
              case m_search:                  /* no key handler in this build */ break;
              case m_slist:                   M_ServerList_Key(key); break;
              default: break;
      }
}

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

static void M_Main_Draw(void)
{
      qpic_t *p;
      const char *dotname;

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/ttl_main.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      p = Draw_CachePic("gfx/mainmenu.lmp");
      M_DrawTransPic(72, 32, p);

      dotname = va("gfx/menudot%i.lmp", ((int)(realtime * 10.0) % 6) + 1);
      p = Draw_CachePic((char *)dotname);
      M_DrawTransPic(54, 20 * m_main_cursor + 32, p);
}

static void M_Main_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      m_entersound = true;
                      switch (m_main_cursor)
                      {
                              case 0: M_Menu_SinglePlayer_f(); return;
                              case 1: M_Menu_MultiPlayer_f(); return;
                              case 2: M_Menu_Options_f(); return;
                              case 4: M_Menu_Quit_f(); return;
                              default: break;
                      }
                      break;

              case K_ESCAPE:
                      key_dest = key_game;
                      m_state = m_none;
                      cls.demonum = m_save_demonum;
                      if (m_save_demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
                              CL_NextDemo();
                      break;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_main_cursor < 0)
                              m_main_cursor = 4;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_main_cursor >= 5)
                              m_main_cursor = 0;
                      break;
      }
}

static void M_SinglePlayer_Draw(void)
{
      qpic_t *p;
      const char *dotname;

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/ttl_sgl.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      p = Draw_CachePic("gfx/sp_menu.lmp");
      M_DrawTransPic(72, 32, p);

      dotname = va("gfx/menudot%i.lmp", ((int)(realtime * 10.0) % 6) + 1);
      p = Draw_CachePic((char *)dotname);
      M_DrawTransPic(54, 20 * m_singleplayer_cursor + 32, p);
}

static void M_SinglePlayer_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      m_entersound = true;
                      if (m_singleplayer_cursor == 0)
                      {
                              if (!sv.active || SCR_DrawDialog("Are you sure you want to start a new game?"))
                              {
                                      key_dest = key_game;
                                      if (sv.active)
                                              Cbuf_AddText("disconnect\n");
                                      Cbuf_AddText("maxplayers 1\n");
                                      Cbuf_AddText("map warez_dm\n");
                              }
                              return;
                      }
                      if (m_singleplayer_cursor == 1)
                      {
                              M_Menu_Load_f();
                              return;
                      }
                      if (m_singleplayer_cursor == 2)
                      {
                              M_Menu_Save_f();
                              return;
                      }
                      break;

              case K_ESCAPE:
                      M_Menu_Main_f();
                      break;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_singleplayer_cursor < 0)
                              m_singleplayer_cursor = 2;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_singleplayer_cursor >= 3)
                              m_singleplayer_cursor = 0;
                      break;
      }
}

static void M_ScanSaves(void)
{
      char filename[MAX_OSPATH];
      char comment[80];
      FILE *f;
      int i;

      for (i = 0; i < MAX_SAVEGAMES; ++i)
      {
              int version;
              int j;

              Q_strcpy(m_filenames[i], "--- UNUSED SLOT ---");
              m_filevalid[i] = 0;

              sprintf(filename, "%s/slot%i.sav", com_gamedir, i);
              f = fopen(filename, "r");
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

              strncpy(m_filenames[i], comment, SAVEGAME_COMMENT_LENGTH - 1);
              m_filenames[i][SAVEGAME_COMMENT_LENGTH - 1] = 0;
              for (j = 0; j < 39; ++j)
              {
                      if (m_filenames[i][j] == '_')
                              m_filenames[i][j] = ' ';
              }
              m_filevalid[i] = 1;
              fclose(f);
      }
}

static void M_Load_Draw(void)
{
      qpic_t *p;
      int i;

      p = Draw_CachePic("gfx/p_load.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      for (i = 0; i < MAX_SAVEGAMES; ++i)
              M_Print(16, 32 + 8 * i, m_filenames[i]);

      M_DrawCharacter(8, 32 + 8 * m_loadsave_cursor, 0x8D);
}

static void M_Save_Draw(void)
{
      qpic_t *p;
      int i;

      p = Draw_CachePic("gfx/p_save.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      for (i = 0; i < MAX_SAVEGAMES; ++i)
              M_Print(16, 32 + 8 * i, m_filenames[i]);

      M_DrawCharacter(8, 32 + 8 * m_loadsave_cursor, 0x29);
}

static void M_Load_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      S_LocalSound("common/menu2.wav");
                      if (!m_filevalid[m_loadsave_cursor])
                              return;

                      m_state = m_none;
                      key_dest = key_game;
                      SCR_BeginLoadingPlaque();
                      Cbuf_AddText(va("load slot%i\n", m_loadsave_cursor));
                      return;

              case K_ESCAPE:
                      M_Menu_SinglePlayer_f();
                      return;

              case K_UPARROW:
              case K_LEFTARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_loadsave_cursor < 0)
                              m_loadsave_cursor = MAX_SAVEGAMES - 1;
                      break;

              case K_DOWNARROW:
              case K_RIGHTARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_loadsave_cursor >= MAX_SAVEGAMES)
                              m_loadsave_cursor = 0;
                      break;
      }
}

static void M_Save_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      m_state = m_none;
                      key_dest = key_game;
                      Cbuf_AddText(va("save slot%i\n", m_loadsave_cursor));
                      return;

              case K_ESCAPE:
                      M_Menu_SinglePlayer_f();
                      return;

              case K_UPARROW:
              case K_LEFTARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_loadsave_cursor < 0)
                              m_loadsave_cursor = MAX_SAVEGAMES - 1;
                      break;

              case K_DOWNARROW:
              case K_RIGHTARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_loadsave_cursor >= MAX_SAVEGAMES)
                              m_loadsave_cursor = 0;
                      break;
      }
}

static void M_MultiPlayer_Draw(void)
{
      qpic_t *p;
      const char *dotname;

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/p_multi.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      p = Draw_CachePic("gfx/mp_menu.lmp");
      M_DrawTransPic(72, 32, p);

      dotname = va("gfx/menudot%i.lmp", ((int)(realtime * 10.0) % 6) + 1);
      p = Draw_CachePic((char *)dotname);
      M_DrawTransPic(54, 20 * m_multiplayer_cursor + 32, p);

      if (!serialAvailable && !ipxAvailable && !tcpipAvailable)
              M_PrintWhite(52, 148, "No Communications Available");
}

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
                                      if (!serialAvailable && !ipxAvailable && !tcpipAvailable)
                                              return;
                                      M_Menu_Net_f();
                                      return;
                              case 2:
                                      M_Menu_Setup_f();
                                      return;
                              default:
                                      return;
                      }

              case K_ESCAPE:
                      M_Menu_Main_f();
                      return;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_multiplayer_cursor < 0)
                              m_multiplayer_cursor = 2;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_multiplayer_cursor >= 3)
                              m_multiplayer_cursor = 0;
                      break;
      }
}

static void M_Setup_Draw(void)
{
      static const int setup_cursor_table[5] = {40, 56, 80, 104, 140};
      qpic_t *p;

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/p_multi.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      M_Print(64, 40, "Hostname");
      M_DrawTextBox(160, 32, 16, 1);
      M_Print(168, 40, setup_hostname);

      M_Print(64, 56, "Your Name");
      M_DrawTextBox(160, 48, 16, 1);
      M_Print(168, 56, setup_myname);

      M_Print(64, 80, "Shirt Color");
      M_Print(64, 104, "Pants Color");

      M_DrawTextBox(64, 132, 14, 1);
      M_Print(72, 140, "Accept Changes");

      p = Draw_CachePic("gfx/bigbox.lmp");
      M_DrawTransPic(160, 64, p);

      p = Draw_CachePic("gfx/menuplyr.lmp");
      M_BuildTranslationTable(16 * setup_top, 16 * setup_bottom);
      M_DrawTransPicTranslate(172, 72, p);

      M_DrawCharacter(56, setup_cursor_table[setup_cursor], 0x8D);

      if (setup_cursor == 0)
              M_DrawCharacter(160 + 8 * (Q_strlen(setup_hostname) + 1), setup_cursor_table[0], 0x8D);
      if (setup_cursor == 1)
              M_DrawCharacter(160 + 8 * (Q_strlen(setup_myname) + 1), setup_cursor_table[1], 0x8D);
}

static void M_Setup_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      if ((unsigned int)setup_cursor < 2)
                              return;
                      if (setup_cursor == 2 || setup_cursor == 3)
                      {
                              S_LocalSound("common/menu3.wav");
                              if (setup_cursor == 2)
                                      ++setup_top;
                              if (setup_cursor == 3)
                                      ++setup_bottom;
                              break;
                      }

                      if (Q_strcmp(cl_name.string, setup_myname))
                              Cbuf_AddText(va("name \"%s\"\n", setup_myname));

                      if (Q_strcmp(hostname.string, setup_hostname))
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
                      if (setup_cursor == 0 && Q_strlen(setup_hostname))
                              setup_hostname[Q_strlen(setup_hostname) - 1] = 0;
                      if (setup_cursor == 1 && Q_strlen(setup_myname))
                              setup_myname[Q_strlen(setup_myname) - 1] = 0;
                      break;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--setup_cursor < 0)
                              setup_cursor = 4;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++setup_cursor >= 5)
                              setup_cursor = 0;
                      break;

              case K_LEFTARROW:
                      if (setup_cursor < 2)
                              return;
                      S_LocalSound("common/menu3.wav");
                      if (setup_cursor == 2)
                              --setup_top;
                      if (setup_cursor == 3)
                              --setup_bottom;
                      break;

              case K_RIGHTARROW:
                      if (setup_cursor < 2)
                              return;
                      S_LocalSound("common/menu3.wav");
                      if (setup_cursor == 2)
                              ++setup_top;
                      if (setup_cursor == 3)
                              ++setup_bottom;
                      break;

              default:
                      if (key >= 32 && key <= 127)
                      {
                              if (setup_cursor == 0)
                              {
                                      int l = Q_strlen(setup_hostname);
                                      if (l < 15)
                                      {
                                              setup_hostname[l] = (char)key;
                                              setup_hostname[l + 1] = 0;
                                      }
                              }
                              if (setup_cursor == 1)
                              {
                                      int l = Q_strlen(setup_myname);
                                      if (l < 15)
                                      {
                                              setup_myname[l] = (char)key;
                                              setup_myname[l + 1] = 0;
                                      }
                              }
                      }
                      break;
      }

      if (setup_top > 13)
              setup_top = 0;
      if (setup_top < 0)
              setup_top = 13;
      if (setup_bottom > 13)
              setup_bottom = 0;
      if (setup_bottom < 0)
              setup_bottom = 13;
}

static const char *net_helpMessage[16] =
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

static void M_Net_Draw(void)
{
      qpic_t *p;
      const char *dotname;

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/p_multi.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

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

      dotname = va("gfx/menudot%i.lmp", ((int)(realtime * 10.0) % 6) + 1);
      p = Draw_CachePic((char *)dotname);
      M_DrawTransPic(54, 20 * m_net_cursor + 32, p);
}

static void M_Net_Key(int key)
{
      do
      {
              switch (key)
              {
                      case K_ENTER:
                              m_entersound = true;
                              if ((unsigned int)m_net_cursor < 2)
                                      M_Menu_SerialConfig_f();
                              else if (m_net_cursor == 2 || m_net_cursor == 3)
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
      } while ((!m_net_cursor && !serialAvailable)
              || (m_net_cursor == 1 && !serialAvailable)
              || (m_net_cursor == 2 && !ipxAvailable)
              || (m_net_cursor == 3 && !tcpipAvailable));
}

static void M_DrawSlider(int x, int y, float value)
{
      int i;

      if (value < 0.0f)
              value = 0.0f;
      if (value > 1.0f)
              value = 1.0f;

      M_DrawCharacter(x - 8, y, 0x80);
      for (i = 0; i < 10; ++i)
              M_DrawCharacter(x + 8 * i, y, 0x81);
      M_DrawCharacter(x + 80, y, 0x82);
      M_DrawCharacter((int)(x + value * 72.0f), y, 0x83);
}

static void M_DrawCheckbox(int x, int y, qboolean on)
{
      if (on)
              M_Print(x, y, "on");
      else
              M_Print(x, y, "off");
}

static void M_AdjustSliders(int dir)
{
      S_LocalSound("common/menu3.wav");

      switch (m_options_cursor)
      {
              case 3:
                      scr_viewsize.value += (float)(10 * dir);
                      if (scr_viewsize.value < 30.0f)
                              scr_viewsize.value = 30.0f;
                      if (scr_viewsize.value > 120.0f)
                              scr_viewsize.value = 120.0f;
                      Cvar_SetValue("viewsize", scr_viewsize.value);
                      break;

              case 4:
                      cv_v_gamma.value += (float)dir * -0.05f;
                      if (cv_v_gamma.value < 0.5f)
                              cv_v_gamma.value = 0.5f;
                      if (cv_v_gamma.value > 1.0f)
                              cv_v_gamma.value = 1.0f;
                      Cvar_SetValue("gamma", cv_v_gamma.value);
                      break;

              case 5:
                      sensitivity.value += (float)dir * 0.5f;
                      if (sensitivity.value < 1.0f)
                              sensitivity.value = 1.0f;
                      if (sensitivity.value > 11.0f)
                              sensitivity.value = 11.0f;
                      Cvar_SetValue("sensitivity", sensitivity.value);
                      break;

              case 6:
                      bgmvolume.value += (float)dir;
                      if (bgmvolume.value < 0.0f)
                              bgmvolume.value = 0.0f;
                      if (bgmvolume.value > 1.0f)
                              bgmvolume.value = 1.0f;
                      Cvar_SetValue("bgmvolume", bgmvolume.value);
                      break;

              case 7:
                      volume.value += (float)dir * 0.1f;
                      if (volume.value < 0.0f)
                              volume.value = 0.0f;
                      if (volume.value > 1.0f)
                              volume.value = 1.0f;
                      Cvar_SetValue("volume", volume.value);
                      break;

              case 8:
                      if (cl_forwardspeed.value <= 200.0f)
                      {
                              Cvar_SetValue("cl_forwardspeed", 400.0f);
                              Cvar_SetValue("cl_backspeed", 400.0f);
                      }
                      else
                      {
                              Cvar_SetValue("cl_forwardspeed", 200.0f);
                              Cvar_SetValue("cl_backspeed", 200.0f);
                      }
                      break;

              case 9:
                      Cvar_SetValue("m_pitch", -m_pitch.value);
                      break;

              case 10:
                      Cvar_SetValue("lookspring", (float)(lookspring.value == 0.0f));
                      break;

              case 11:
                      Cvar_SetValue("lookstrafe", (float)(lookstrafe.value == 0.0f));
                      break;

              case 13:
                      Cvar_SetValue("_windowed_mouse", (float)(_windowed_mouse.value == 0.0f));
                      break;

              default:
                      break;
      }
}

static void M_Options_Draw(void)
{
      qpic_t *p;

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/p_option.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      M_Print(16, 32, "    Customize controls");
      M_Print(16, 40, "         Go to console");
      M_Print(16, 48, "     Reset to defaults");
      M_Print(16, 56, "           Screen size");
      M_DrawSlider(220, 56, (scr_viewsize.value - 30.0f) / 90.0f);

      M_Print(16, 64, "            Brightness");
      M_DrawSlider(220, 64, (1.0f - cv_v_gamma.value) / 0.5f);

      M_Print(16, 72, "           Mouse Speed");
      M_DrawSlider(220, 72, (sensitivity.value - 1.0f) / 10.0f);

      M_Print(16, 80, "       CD Music Volume");
      M_DrawSlider(220, 80, bgmvolume.value);

      M_Print(16, 88, "          Sound Volume");
      M_DrawSlider(220, 88, volume.value);

      M_Print(16, 96, "            Always Run");
      M_DrawCheckbox(220, 96, cl_forwardspeed.value > 200.0f);

      M_Print(16, 104, "          Invert Mouse");
      M_DrawCheckbox(220, 104, m_pitch.value < 0.0f);

      M_Print(16, 112, "            Lookspring");
      M_DrawCheckbox(220, 112, (qboolean)(int)lookspring.value);

      M_Print(16, 120, "            Lookstrafe");
      M_DrawCheckbox(220, 120, (qboolean)(int)lookstrafe.value);

      if (vid_menudrawfn)
              M_Print(16, 128, "         Video Options");

      if (!vid_fullscreen)
      {
              M_Print(16, 136, "             Use Mouse");
              M_DrawCheckbox(220, 136, (qboolean)(int)_windowed_mouse.value);
      }

      M_DrawCharacter(200, 32 + 8 * m_options_cursor, 0x8D);
}

static void M_Options_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      m_entersound = true;
                      if (m_options_cursor == 0)
                              M_Menu_Keys_f();
                      else
                      {
                              switch (m_options_cursor)
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
                      }
                      return;

              case K_ESCAPE:
                      M_Menu_Main_f();
                      break;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_options_cursor < 0)
                              m_options_cursor = 13;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_options_cursor >= 14)
                              m_options_cursor = 0;
                      break;

              case K_LEFTARROW:
                      M_AdjustSliders(-1);
                      break;

              case K_RIGHTARROW:
                      M_AdjustSliders(1);
                      break;
      }

      if (m_options_cursor == 12 && !vid_menudrawfn)
              m_options_cursor = (key == K_UPARROW) ? 11 : 0;

      if (m_options_cursor == 13 && vid_fullscreen)
              m_options_cursor = (key == K_UPARROW) ? 12 : 0;
}

static const char *bindnames[][2] =
{
      {"+attack",     "attack"},
      {"impulse 10",  "change weapon"},
      {"+jump",       "jump / swim up"},
      {"+forward",    "walk forward"},
      {"+back",       "backpedal"},
      {"+left",       "turn left"},
      {"+right",      "turn right"},
      {"+speed",      "run"},
      {"+moveleft",   "step left"},
      {"+moveright",  "step right"},
      {"+strafe",     "sidestep"},
      {"+lookup",     "look up"},
      {"+lookdown",   "look down"},
      {"centerview",  "center view"},
      {"+mlook",      "mouse look"},
      {"+klook",      "keyboard look"},
      {"+moveup",     "swim up"},
      {"+movedown",   "swim down"},
};

static void M_FindKeysForCommand(const char *command, int twokeys[2])
{
      int count;
      int l;
      int j;

      twokeys[0] = twokeys[1] = -1;
      l = Q_strlen((char *)command);
      count = 0;

      for (j = 0; j < 256; ++j)
      {
              if (!keybindings[j])
                      continue;

              if (!strncmp(keybindings[j], command, (unsigned int)l))
              {
                      twokeys[count] = j;
                      if (++count == 2)
                              break;
              }
      }
}

static void M_UnbindCommand(const char *command)
{
      int i;
      int l;

      l = Q_strlen((char *)command);
      for (i = 0; i < 256; ++i)
      {
              if (!keybindings[i])
                      continue;
              if (!strncmp(keybindings[i], command, (unsigned int)l))
                      Key_SetBinding(i, "");
      }
}

static void M_Keys_Draw(void)
{
      qpic_t *p;
      int i;
      int y;
      int keys[2];

      p = Draw_CachePic("gfx/ttl_cstm.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      if (m_keys_bind_grab)
              M_Print(12, 32, "Press a key or button for this action");
      else
              M_Print(18, 32, "Enter to change, backspace to clear");

      for (i = 0, y = 48; i < (int)(sizeof(bindnames) / sizeof(bindnames[0])); ++i, y += 8)
      {
              const char *keyname1;
              const char *keyname2;
              int len;

              M_Print(16, y, bindnames[i][1]);

              M_FindKeysForCommand(bindnames[i][0], keys);
              if (keys[0] == -1)
              {
                      M_Print(140, y, "???");
                      continue;
              }

              keyname1 = Key_KeynumToString(keys[0]);
              M_Print(140, y, keyname1);
              len = 8 * (Q_strlen((char *)keyname1) + 1) - 8;

              if (keys[1] != -1)
              {
                      M_Print(len + 148, y, "or");
                      keyname2 = Key_KeynumToString(keys[1]);
                      M_Print(len + 172, y, keyname2);
              }
      }

      M_DrawCharacter(130, 48 + 8 * m_keys_cursor, 0x8D);
}

static void M_Keys_Key(int key)
{
      char cmd[80];

      if (m_keys_bind_grab)
      {
              S_LocalSound("common/menu1.wav");
              if (key != K_ESCAPE && key != '`')
              {
                      const char *command = bindnames[m_keys_cursor][0];
                      const char *keyname = Key_KeynumToString(key);
                      sprintf(cmd, "bind \"%s\" \"%s\"\n", keyname, command);
                      Cbuf_InsertText(cmd);
              }
              m_keys_bind_grab = false;
              return;
      }

      switch (key)
      {
              case K_ESCAPE:
                      M_Menu_Options_f();
                      return;

              case K_ENTER:
              {
                      int keys[2];
                      M_FindKeysForCommand(bindnames[m_keys_cursor][0], keys);
                      S_LocalSound("common/menu2.wav");
                      if (keys[1] != -1)
                              M_UnbindCommand(bindnames[m_keys_cursor][0]);
                      m_keys_bind_grab = true;
                      return;
              }

              case K_BACKSPACE:
              case K_DEL:
                      S_LocalSound("common/menu2.wav");
                      M_UnbindCommand(bindnames[m_keys_cursor][0]);
                      return;

              case K_UPARROW:
              case K_LEFTARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_keys_cursor < 0)
                              m_keys_cursor = 17;
                      break;

              case K_DOWNARROW:
              case K_RIGHTARROW:
                      S_LocalSound("common/menu1.wav");
                      if ((unsigned int)++m_keys_cursor >= 18)
                              m_keys_cursor = 0;
                      break;
      }
}

static void M_Video_Draw(void)
{

      if (vid_menudrawfn)
              vid_menudrawfn();
}

static void M_Video_Key(int key)
{
      vid_menukeyfn(key);
}

static void M_Help_Draw(void)
{
      qpic_t *p;
      p = Draw_CachePic((char *)va("gfx/help%i.lmp", m_loadsave_cursor));
      M_DrawPic(0, 0, p);
}

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
              if (++m_loadsave_cursor >= 6)
                      m_loadsave_cursor = 0;
              return;
      }

      if (key == K_DOWNARROW || key == K_LEFTARROW)
      {
              m_entersound = true;
              if (--m_loadsave_cursor < 0)
                      m_loadsave_cursor = 5;
      }
}

static void M_Quit_Draw(void)
{
      if (m_quit_wasopen)
      {
              m_recursiveDraw = true;
              m_state = (menu_state_t)m_quit_prevstate;
              M_Draw();
              m_state = m_quit;
      }

      if (W_GetLumpinfo("credits", false))
      {
              qpic_t *pic = (qpic_t *)W_GetLumpName("credits");
              Draw_Pic((vid.width - pic->width) >> 1, (vid.height - pic->height) >> 2, pic);
      }
      else
      {
              M_DrawTextBox(56, 76, 24, 4);
              M_Print(64, 92, "Press Y to exit");
      }
}

static void M_Quit_Key(int key)
{
      if (key != K_ESCAPE && key != 'N')
      {
              if (key != 'Y' && key != 'n' && key != 'y')
                      return;

              key_dest = key_console;
              CL_Disconnect();
              Host_ShutdownServer(false);
              Sys_Quit();
      }

      if (m_quit_wasopen)
      {
              m_entersound = true;
              m_state = (menu_state_t)m_quit_prevstate;
      }
      else
      {
              key_dest = key_game;
              m_state = m_none;
      }
}

static void M_SerialConfig_Draw(void)
{
      qpic_t *p;
      int basex;
      const char *action;

      static const int serialconfig_cursor_table[6] = {40, 56, 72, 88, 104, 124};
      static const int isa_uarts[5] = {0, 0x3F8, 0x2F8, 0x3E8, 0x2E8};
      static const int isa_irqs[5] = {0, 4, 3, 4, 3};
      static const int baudrates[6] = {9600, 14400, 19200, 28800, 38400, 57600};

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/p_multi.lmp");
      basex = (320 - p->width) / 2;
      M_DrawPic(basex, 4, p);

      action = (m_multiplayer_cursor == 1) ? "New Game" : "Join Game";
      M_Print(basex, 32, va("%s", action));

      basex += 8;

      M_Print(basex, serialconfig_cursor_table[0], "Port");
      M_DrawTextBox(160, 40, 4, 1);
      M_Print(168, serialconfig_cursor_table[0], va("COM%u", serialconfig_comport));

      M_Print(basex, serialconfig_cursor_table[1], "IRQ");
      M_DrawTextBox(160, serialconfig_cursor_table[1] - 8, 1, 1);
      M_Print(168, serialconfig_cursor_table[1], va("%u", serialconfig_irq));

      M_Print(basex, serialconfig_cursor_table[2], "Baud");
      M_DrawTextBox(160, serialconfig_cursor_table[2] - 8, 5, 1);
      M_Print(168, serialconfig_cursor_table[2], va("%u", baudrates[serialconfig_baud]));

      if (!m_net_cursor)
      {
              M_Print(basex, serialconfig_cursor_table[3], "Modem Setup");
              if (!m_multiplayer_cursor)
              {
                      M_Print(basex, serialconfig_cursor_table[4], "Phone Number");
                      M_DrawTextBox(160, serialconfig_cursor_table[4] - 8, 16, 1);
                      M_Print(168, serialconfig_cursor_table[4], serialconfig_phone);
              }
      }

      if (!m_multiplayer_cursor)
      {
              M_DrawTextBox(basex, serialconfig_cursor_table[5] - 8, 7, 1);
              M_Print(basex + 8, serialconfig_cursor_table[5], "Connect");
      }
      else
      {
              M_DrawTextBox(basex, serialconfig_cursor_table[5] - 8, 2, 1);
              M_Print(basex + 8, serialconfig_cursor_table[5], "OK");
      }

      M_DrawCharacter(basex - 8, serialconfig_cursor_table[m_serialconfig_cursor], 0x8D);

      if (m_serialconfig_cursor == 4)
              M_DrawCharacter(160 + 8 * (Q_strlen(serialconfig_phone) + 1), serialconfig_cursor_table[4], 0x8D);

      if (m_return_reason[0])
              M_PrintWhite(basex, 148, m_return_reason);

      (void)isa_uarts;
      (void)isa_irqs;
}

static void M_SerialConfig_Key(int key)
{
      static const int isa_uarts[5] = {0, 0x3F8, 0x2F8, 0x3E8, 0x2E8};
      static const int isa_irqs[5] = {0, 4, 3, 4, 3};
      static const int baudrates[6] = {9600, 14400, 19200, 28800, 38400, 57600};

      switch (key)
      {
              case K_ENTER:
                      if (m_serialconfig_cursor >= 3)
                      {
                              m_entersound = true;
                              if (m_serialconfig_cursor == 3)
                              {
                                      NET_SetComPortConfig(0, isa_uarts[serialconfig_comport], serialconfig_irq, baudrates[serialconfig_baud], (m_net_cursor == 0));
                                      M_Menu_ModemConfig_f();
                              }
                              else if (m_serialconfig_cursor == 4)
                              {
                                      m_serialconfig_cursor = 5;
                              }
                              else
                              {
                                      NET_SetComPortConfig(0, isa_uarts[serialconfig_comport], serialconfig_irq, baudrates[serialconfig_baud], (m_net_cursor == 0));
                                      M_ConfigureNetSubsystem();
                                      if (m_multiplayer_cursor == 1)
                                      {
                                              M_Menu_GameOptions_f();
                                      }
                                      else
                                      {
                                              m_return_onerror = true;
                                              m_return_state = m_state;
                                              key_dest = key_game;
                                              m_state = m_none;
                                              if (m_net_cursor)
                                                      Cbuf_AddText("connect\n");
                                              else
                                                      Cbuf_AddText(va("connect %s\n", serialconfig_phone));
                                      }
                              }
                      }
                      else
                      {
                              S_LocalSound("common/menu3.wav");
                              if (m_serialconfig_cursor == 0)
                              {
                                      if (++serialconfig_comport > 4)
                                              serialconfig_comport = 1;
                                      serialconfig_irq = isa_irqs[serialconfig_comport];
                              }
                              if (m_serialconfig_cursor == 1)
                              {
                                      if (++serialconfig_irq == 6)
                                              serialconfig_irq = 7;
                                      if (serialconfig_irq == 8)
                                              serialconfig_irq = 2;
                              }
                              if (m_serialconfig_cursor == 2 && ++serialconfig_baud > 5)
                                      serialconfig_baud = 0;
                      }
                      break;

              case K_ESCAPE:
                      M_Menu_Net_f();
                      break;

              case K_BACKSPACE:
                      if (m_serialconfig_cursor == 4 && Q_strlen(serialconfig_phone))
                              serialconfig_phone[Q_strlen(serialconfig_phone) - 1] = 0;
                      break;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_serialconfig_cursor < 0)
                              m_serialconfig_cursor = 5;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_serialconfig_cursor >= 6)
                              m_serialconfig_cursor = 0;
                      break;

              case K_LEFTARROW:
                      if (m_serialconfig_cursor <= 2)
                      {
                              S_LocalSound("common/menu3.wav");
                              if (m_serialconfig_cursor == 0)
                              {
                                      if (!--serialconfig_comport)
                                              serialconfig_comport = 4;
                                      serialconfig_irq = isa_irqs[serialconfig_comport];
                              }
                              if (m_serialconfig_cursor == 1)
                              {
                                      if (--serialconfig_irq == 6)
                                              serialconfig_irq = 5;
                                      if (serialconfig_irq == 1)
                                              serialconfig_irq = 7;
                              }
                              if (m_serialconfig_cursor == 2 && --serialconfig_baud < 0)
                                      serialconfig_baud = 5;
                      }
                      break;

              case K_RIGHTARROW:
                      if (m_serialconfig_cursor <= 2)
                      {
                              S_LocalSound("common/menu3.wav");
                              if (m_serialconfig_cursor == 0)
                              {
                                      if (++serialconfig_comport > 4)
                                              serialconfig_comport = 1;
                                      serialconfig_irq = isa_irqs[serialconfig_comport];
                              }
                              if (m_serialconfig_cursor == 1)
                              {
                                      if (++serialconfig_irq == 6)
                                              serialconfig_irq = 7;
                                      if (serialconfig_irq == 8)
                                              serialconfig_irq = 2;
                              }
                              if (m_serialconfig_cursor == 2 && ++serialconfig_baud > 5)
                                      serialconfig_baud = 0;
                      }
                      break;

              default:
                      if (key >= 32 && key <= 127 && m_serialconfig_cursor == 4)
                      {
                              int l = Q_strlen(serialconfig_phone);
                              if (l < 15)
                              {
                                      serialconfig_phone[l] = (char)key;
                                      serialconfig_phone[l + 1] = 0;
                              }
                      }
                      break;
      }

      if (m_net_cursor == 1 && (m_serialconfig_cursor == 3 || m_serialconfig_cursor == 4))
              m_serialconfig_cursor = (key == K_UPARROW) ? 2 : 5;

      if (!m_net_cursor && m_multiplayer_cursor == 1 && m_serialconfig_cursor == 4)
              m_serialconfig_cursor = (key == K_UPARROW) ? 3 : 5;

      (void)isa_uarts;
}

static void M_ModemConfig_Draw(void)
{
      qpic_t *p;
      int basex;

      static const int modem_cursor_table[5] = {40, 56, 88, 120, 156};

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/p_multi.lmp");
      basex = (320 - p->width) / 2;
      M_DrawPic(basex, 4, p);

      basex += 8;

      if (modem_dialtype == 'P')
              M_Print(basex, modem_cursor_table[0], "Pulse Dialing");
      else
              M_Print(basex, modem_cursor_table[0], "Touch Tone Dialing");

      M_Print(basex, modem_cursor_table[1], "Clear");
      M_DrawTextBox(basex, modem_cursor_table[1] + 4, 16, 1);
      M_Print(basex + 8, modem_cursor_table[1] + 12, modem_clear);
      if (m_modemconfig_cursor == 1)
              M_DrawCharacter(basex + 8 * (Q_strlen(modem_clear) + 1), modem_cursor_table[1] + 12, 0x8D);

      M_Print(basex, modem_cursor_table[2], "Init");
      M_DrawTextBox(basex, modem_cursor_table[2] + 4, 30, 1);
      M_Print(basex + 8, modem_cursor_table[2] + 12, modem_init);
      if (m_modemconfig_cursor == 2)
              M_DrawCharacter(basex + 8 * (Q_strlen(modem_init) + 1), modem_cursor_table[2] + 12, 0x8D);

      M_Print(basex, modem_cursor_table[3], "Hangup");
      M_DrawTextBox(basex, modem_cursor_table[3] + 4, 16, 1);
      M_Print(basex + 8, modem_cursor_table[3] + 12, modem_hangup);
      if (m_modemconfig_cursor == 3)
              M_DrawCharacter(basex + 8 * (Q_strlen(modem_hangup) + 1), modem_cursor_table[3] + 12, 0x8D);

      M_DrawTextBox(basex, modem_cursor_table[4] - 8, 2, 1);
      M_Print(basex + 8, modem_cursor_table[4], "OK");

      M_DrawCharacter(basex - 8, modem_cursor_table[m_modemconfig_cursor], 0x8D);
}

static void M_ModemConfig_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      if (m_modemconfig_cursor == 0)
                      {
                              modem_dialtype = (modem_dialtype == 'P') ? 'T' : 'P';
                              m_entersound = true;
                      }
                      if (m_modemconfig_cursor == 4)
                      {
                              NET_SetModemConfig(0, va("%c", modem_dialtype), modem_clear, modem_init, modem_hangup);
                              m_entersound = true;
                              M_Menu_SerialConfig_f();
                      }
                      break;

              case K_ESCAPE:
                      M_Menu_SerialConfig_f();
                      break;

              case K_BACKSPACE:
                      if (m_modemconfig_cursor == 1 && Q_strlen(modem_clear))
                              modem_clear[Q_strlen(modem_clear) - 1] = 0;
                      if (m_modemconfig_cursor == 2 && Q_strlen(modem_init))
                              modem_init[Q_strlen(modem_init) - 1] = 0;
                      if (m_modemconfig_cursor == 3 && Q_strlen(modem_hangup))
                              modem_hangup[Q_strlen(modem_hangup) - 1] = 0;
                      break;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_modemconfig_cursor < 0)
                              m_modemconfig_cursor = 4;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_modemconfig_cursor >= 5)
                              m_modemconfig_cursor = 0;
                      break;

              default:
                      if (key == K_LEFTARROW || key == K_RIGHTARROW)
                      {
                              if (m_modemconfig_cursor == 0)
                              {
                                      modem_dialtype = (modem_dialtype == 'P') ? 'T' : 'P';
                                      S_LocalSound("common/menu1.wav");
                              }
                      }
                      else if (key >= 32 && key <= 127)
                      {
                              if (m_modemconfig_cursor == 1)
                              {
                                      int l = Q_strlen(modem_clear);
                                      if (l < 15)
                                      {
                                              modem_clear[l] = (char)key;
                                              modem_clear[l + 1] = 0;
                                      }
                              }
                              if (m_modemconfig_cursor == 2)
                              {
                                      int l = Q_strlen(modem_init);
                                      if (l < 29)
                                      {
                                              modem_init[l] = (char)key;
                                              modem_init[l + 1] = 0;
                                      }
                              }
                              if (m_modemconfig_cursor == 3)
                              {
                                      int l = Q_strlen(modem_hangup);
                                      if (l < 15)
                                      {
                                              modem_hangup[l] = (char)key;
                                              modem_hangup[l + 1] = 0;
                                      }
                              }
                      }
                      break;
      }
}

static void M_LanConfig_Draw(void)
{
      qpic_t *p;
      int basex;
      const char *action;

      static const int lan_cursor_table[3] = {72, 92, 124};

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/p_multi.lmp");
      basex = (320 - p->width) / 2;
      M_DrawPic(basex, 4, p);

      action = (m_multiplayer_cursor == 1) ? "New Game" : "Join Game";
      M_Print(basex, 32, va("%s", action));

      basex += 8;

      M_Print(basex, 52, "Address");
      if (m_net_cursor == 2)
              M_Print(basex + 72, 52, my_ipx_address);
      else
              M_Print(basex + 72, 52, my_tcpip_address);

      M_Print(basex, lan_cursor_table[0], "Port");
      M_DrawTextBox(basex + 64, lan_cursor_table[0] - 8, 6, 1);
      M_Print(basex + 72, lan_cursor_table[0], lanconfig_portname);

      if (m_multiplayer_cursor)
      {
              M_DrawTextBox(basex, lan_cursor_table[1] - 8, 2, 1);
              M_Print(basex + 8, lan_cursor_table[1], "OK");
      }
      else
      {
              M_Print(basex, lan_cursor_table[1], "Search for local games");
              M_Print(basex, 108, "Join game at:");
              M_DrawTextBox(basex + 8, lan_cursor_table[2] - 8, 22, 1);
              M_Print(basex + 16, lan_cursor_table[2], lanconfig_joinname);
      }

      M_DrawCharacter(basex - 8, lan_cursor_table[m_lanconfig_cursor], 0x8D);
      if (m_lanconfig_cursor == 0)
              M_DrawCharacter(basex + 64 + 8 * (Q_strlen(lanconfig_portname) + 1), lan_cursor_table[0], 0x8D);
      if (m_lanconfig_cursor == 2)
              M_DrawCharacter(basex + 8 + 8 * (Q_strlen(lanconfig_joinname) + 1), lan_cursor_table[2], 0x8D);

      if (m_return_reason[0])
              M_PrintWhite(basex, 148, m_return_reason);
}

static void M_LanConfig_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      if (m_lanconfig_cursor)
                      {
                              m_entersound = true;
                              M_ConfigureNetSubsystem();
                              if (m_lanconfig_cursor == 1)
                              {
                                      if (m_multiplayer_cursor == 1)
                                              M_Menu_GameOptions_f();
                                      else
                                              M_Menu_Search_f();
                              }
                              else if (m_lanconfig_cursor == 2)
                              {
                                      m_return_state = m_state;
                                      m_return_onerror = true;
                                      key_dest = key_game;
                                      m_state = m_none;
                                      Cbuf_AddText(va("connect %s\n", lanconfig_joinname));
                              }
                      }
                      break;

              case K_ESCAPE:
                      M_Menu_Net_f();
                      break;

              case K_BACKSPACE:
                      if (!m_lanconfig_cursor && Q_strlen(lanconfig_portname))
                              lanconfig_portname[Q_strlen(lanconfig_portname) - 1] = 0;
                      if (m_lanconfig_cursor == 2 && Q_strlen(lanconfig_joinname))
                              lanconfig_joinname[Q_strlen(lanconfig_joinname) - 1] = 0;
                      break;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--m_lanconfig_cursor < 0)
                              m_lanconfig_cursor = 2;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++m_lanconfig_cursor >= 3)
                              m_lanconfig_cursor = 0;
                      break;

              default:
                      if (key >= 32 && key <= 127)
                      {
                              if (m_lanconfig_cursor == 2)
                              {
                                      int l = Q_strlen(lanconfig_joinname);
                                      if (l < 21)
                                      {
                                              lanconfig_joinname[l] = (char)key;
                                              lanconfig_joinname[l + 1] = 0;
                                      }
                              }
                              if (key >= '0' && key <= '9' && !m_lanconfig_cursor)
                              {
                                      int l = Q_strlen(lanconfig_portname);
                                      if (l < 5)
                                      {
                                              lanconfig_portname[l] = (char)key;
                                              lanconfig_portname[l + 1] = 0;
                                      }
                              }
                      }
                      break;
      }

      if (m_multiplayer_cursor == 1 && m_lanconfig_cursor == 2)
              m_lanconfig_cursor = (key == K_UPARROW);

      lanconfig_port = Q_atoi(lanconfig_portname);
      if ((unsigned int)lanconfig_port <= 0xFFFFu)
              hostshort = (unsigned short)lanconfig_port;
      sprintf(lanconfig_portname, "%u", (unsigned int)hostshort);
}

static void M_GameOptions_Draw(void)
{
      qpic_t *p;

      static const int gameoptions_cursor_table[7] = {40, 56, 64, 72, 80, 88, 96};

      p = Draw_CachePic("gfx/qplaque.lmp");
      M_DrawTransPic(16, 4, p);

      p = Draw_CachePic("gfx/p_multi.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      M_DrawTextBox(152, 32, 10, 1);
      M_Print(160, 40, "Begin Game");

      M_Print(0, 56, "Max Players");
      M_Print(160, 56, va("%i", gameoptions_maxplayers));

      M_Print(0, 64, "Game Type");
      M_Print(160, 64, (coop.value == 0.0f) ? "Deathmatch" : "Cooperative");

      M_Print(0, 72, "Teamplay");
      if ((int)teamplay.value == 1)
              M_Print(160, 72, "No Friendly Fire");
      else if ((int)teamplay.value == 2)
              M_Print(160, 72, "Friendly Fire");
      else
              M_Print(160, 72, "Off");

      M_Print(0, 80, "Skill");
      if (skill.value == 0.0f)
              M_Print(160, 80, "Easy difficulty");
      else if (skill.value == 1.0f)
              M_Print(160, 80, "Normal difficulty");
      else if (skill.value == 2.0f)
              M_Print(160, 80, "Hard difficulty");
      else
              M_Print(160, 80, "Nightmare difficulty");

      M_Print(0, 88, "Frag Limit");
      if (fraglimit.value == 0.0f)
              M_Print(160, 88, "none");
      else
              M_Print(160, 88, va("%i frags", (int)fraglimit.value));

      M_Print(0, 96, "Time Limit");
      if (timelimit.value == 0.0f)
              M_Print(160, 96, "none");
      else
              M_Print(160, 96, va("%i minutes", (int)timelimit.value));

      M_DrawCharacter(144, gameoptions_cursor_table[gameoptions_cursor], 0x8D);

      if (gameoptions_showwarning)
      {
              if (realtime - gameoptions_warntime >= 5.0)
                      gameoptions_showwarning = false;
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

static void M_NetStart_Change(int dir)
{
      switch (gameoptions_cursor)
      {
              case 1:
                      gameoptions_maxplayers += dir;
                      if (gameoptions_maxplayers > svs.maxclientslimit)
                      {
                              gameoptions_maxplayers = svs.maxclientslimit;
                              gameoptions_showwarning = true;
                              gameoptions_warntime = realtime;
                      }
                      if (gameoptions_maxplayers < 2)
                              gameoptions_maxplayers = 2;
                      break;

              case 2:
                      Cvar_SetValue("coop", (float)(coop.value == 0.0f));
                      break;

               case 3:
               {
                       const int teamplay_max = rogue ? 6 : 2;

                       Cvar_SetValue("teamplay", teamplay.value + (float)dir);
                       if (teamplay.value > (float)teamplay_max)
                               Cvar_SetValue("teamplay", 0.0f);
                       if (teamplay.value < 0.0f)
                               Cvar_SetValue("teamplay", (float)teamplay_max);
                       break;
               }

               case 4:
                       Cvar_SetValue("skill", skill.value + (float)dir);
                       if (skill.value > 3.0f)
                               Cvar_SetValue("skill", 0.0f);
                       if (skill.value < 0.0f)
                               Cvar_SetValue("skill", 3.0f);
                       break;

              case 5:
                      Cvar_SetValue("fraglimit", fraglimit.value + (float)(10 * dir));
                      if (fraglimit.value > 100.0f)
                              Cvar_SetValue("fraglimit", 0.0f);
                      if (fraglimit.value < 0.0f)
                              Cvar_SetValue("fraglimit", 100.0f);
                      break;

              case 6:
                      Cvar_SetValue("timelimit", timelimit.value + (float)(5 * dir));
                      if (timelimit.value > 60.0f)
                              Cvar_SetValue("timelimit", 0.0f);
                      if (timelimit.value < 0.0f)
                              Cvar_SetValue("timelimit", 60.0f);
                      break;
      }
}

static void M_GameOptions_Key(int key)
{
      switch (key)
      {
              case K_ENTER:
                      S_LocalSound("common/menu2.wav");
                      if (gameoptions_cursor)
                      {
                              M_NetStart_Change(1);
                              return;
                      }

                      if (sv.active)
                              Cbuf_AddText("disconnect\n");
                      Cbuf_AddText("listen 0\n");
                      Cbuf_AddText(va("maxplayers %u\n", (unsigned int)gameoptions_maxplayers));
                      SCR_BeginLoadingPlaque();
                      Cbuf_AddText("map warez_dm\n");
                      break;

              case K_ESCAPE:
                      M_Menu_Net_f();
                      return;

              case K_UPARROW:
                      S_LocalSound("common/menu1.wav");
                      if (--gameoptions_cursor < 0)
                              gameoptions_cursor = 6;
                      break;

              case K_DOWNARROW:
                      S_LocalSound("common/menu1.wav");
                      if (++gameoptions_cursor >= 7)
                              gameoptions_cursor = 0;
                      break;

              case K_LEFTARROW:
                      if (gameoptions_cursor)
                      {
                              S_LocalSound("common/menu3.wav");
                              M_NetStart_Change(-1);
                      }
                      break;

              case K_RIGHTARROW:
                      if (gameoptions_cursor)
                      {
                              S_LocalSound("common/menu3.wav");
                              M_NetStart_Change(1);
                      }
                      break;
      }
}

static void M_Search_Draw(void)
{
      extern int slistInProgress;

      qpic_t *p = Draw_CachePic("gfx/p_multi.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      M_DrawTextBox(108, 32, 12, 1);
      M_Print(116, 40, "Searching");

      if (slistInProgress)
      {
              NET_Poll();
              return;
      }

      if (!searchtime_set)
      {
              searchtime_set = true;
              searchtime = realtime;
      }

      if (hostCacheCount)
      {
              M_Menu_ServerList_f();
              return;
      }

      M_PrintWhite(72, 64, "No Quake servers found");
      if (realtime - searchtime >= 3.0)
              M_Menu_LanConfig_f();
}

static void M_ServerList_Draw(void)
{
      int n;

      if (!slist_sorted)
      {
              if (hostCacheCount > 1)
              {
                      for (int i = 0; i < hostCacheCount; ++i)
                      {
                              for (int j = i + 1; j < hostCacheCount; ++j)
                              {
                                      if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
                                      {
                                              hostcache_t temp = hostcache[i];
                                              hostcache[i] = hostcache[j];
                                              hostcache[j] = temp;
                                      }
                              }
                      }
              }
              slist_sorted = true;
      }

      qpic_t *p = Draw_CachePic("gfx/p_multi.lmp");
      M_DrawPic((320 - p->width) / 2, 4, p);

      for (n = 0; n < hostCacheCount; ++n)
      {
              char line[64];

              if (hostcache[n].maxusers)
                      sprintf(line, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
              else
                      sprintf(line, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);

              M_Print(16, 32 + 8 * n, line);
      }

      M_DrawCharacter(0, 32 + 8 * slist_cursor, 0x8D);

      if (m_return_reason[0])
              M_PrintWhite(16, 148, m_return_reason);
}

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
                      return;

              case K_ESCAPE:
                      M_Menu_LanConfig_f();
                      return;

              case K_SPACE:
                      M_Menu_Search_f();
                      return;

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

static void M_ConfigureNetSubsystem(void)
{
      Cbuf_AddText("stopdemo\n");

      if ((unsigned int)m_net_cursor <= 1)
              Cbuf_AddText("com1 enable\n");

      if (m_net_cursor == 2 || m_net_cursor == 3)
              hostshort = (unsigned short)lanconfig_port;
}
