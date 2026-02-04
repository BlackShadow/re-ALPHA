#ifndef MENU_H
#define MENU_H

 // =========================================================
 // Menu Types
 // =========================================================

typedef enum {
	m_none = 0,
	m_main,
	m_singleplayer,
	m_load,
	m_save,
	m_multiplayer,
	m_setup,
	m_net,
	m_options,
	m_video,
	m_keys,
	m_help,
	m_quit,
	m_serialconfig,
	m_modemconfig,
	m_lanconfig,
	m_gameoptions,
	m_search,
	m_slist
} menu_state_t;

 // =========================================================
 // Menu Functions
 // =========================================================

void    M_Init(void);
void    M_Draw(void);
void    M_Keydown(int key);
void    M_ToggleMenu_f(void);

 // Specific Menu Entry Points
void    M_Menu_Main_f(void);
void    M_Menu_SinglePlayer_f(void);
void    M_Menu_Load_f(void);
void    M_Menu_Save_f(void);
void    M_Menu_MultiPlayer_f(void);
void    M_Menu_Setup_f(void);
void    M_Menu_Net_f(void);
void    M_Menu_Options_f(void);
void    M_Menu_Keys_f(void);
void    M_Menu_Video_f(void);
void    M_Menu_Help_f(void);
void    M_Menu_Quit_f(void);
void    M_Menu_SerialConfig_f(void);
void    M_Menu_ModemConfig_f(void);
void    M_Menu_LanConfig_f(void);
void    M_Menu_GameOptions_f(void);
void    M_Menu_Search_f(void);
void    M_Menu_ServerList_f(void);

#endif // MENU_H
