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

#ifndef KEYS_H
#define KEYS_H

 // =============================================================================
 // Key button state structure (for input handling)
 // =============================================================================


 // Key constants
#define K_TAB			9
#define K_ENTER			13
#define K_ESCAPE		27
#define K_SPACE			32
#define K_BACKSPACE		127
#define K_UPARROW		128
#define K_DOWNARROW		129
#define K_LEFTARROW		130
#define K_RIGHTARROW	131
#define K_ALT			132
#define K_CTRL			133
#define K_SHIFT			134
#define K_F1			135
#define K_F2			136
#define K_F3			137
#define K_F4			138
#define K_F5			139
#define K_F6			140
#define K_F7			141
#define K_F8			142
#define K_F9			143
#define K_F10			144
#define K_F11			145
#define K_F12			146
#define K_INS			147
#define K_DEL			148
#define K_PGDN			149
#define K_PGUP			150
#define K_HOME			151
#define K_END			152
#define K_PAUSE			255

 // Mouse buttons
#define K_MOUSE1		200
#define K_MOUSE2		201
#define K_MOUSE3		202
#define K_MWHEELUP      239
#define K_MWHEELDOWN    240

 // Joystick buttons (Quake-style key range)
#define K_JOY1          203
#define K_JOY2          204
#define K_JOY3          205
#define K_JOY4          206

 // Auxiliary keys (Quake-style key range)
#define K_AUX1          207
#define K_AUX2          208
#define K_AUX3          209
#define K_AUX4          210
#define K_AUX5          211
#define K_AUX6          212
#define K_AUX7          213
#define K_AUX8          214
#define K_AUX9          215
#define K_AUX10         216
#define K_AUX11         217
#define K_AUX12         218
#define K_AUX13         219
#define K_AUX14         220
#define K_AUX15         221
#define K_AUX16         222
#define K_AUX17         223
#define K_AUX18         224
#define K_AUX19         225
#define K_AUX20         226
#define K_AUX21         227
#define K_AUX22         228
#define K_AUX23         229
#define K_AUX24         230
#define K_AUX25         231
#define K_AUX26         232
#define K_AUX27         233
#define K_AUX28         234
#define K_AUX29         235
#define K_AUX30         236
#define K_AUX31         237
#define K_AUX32         238

 // Key types
typedef enum {
	key_game,
	key_console,
	key_message,
	key_menu
} keydest_t;

extern keydest_t key_dest;
extern char *keybindings[256];
extern int key_repeats[256];
extern qboolean keydown[256];
extern int key_count;
extern int key_lastpress;

typedef struct kbutton_s
{
	int down[2]; // key numbers holding it down
	int state; // bit 0 = held, bit 1 = down this frame, bit 2 = up this frame
} kbutton_t;

 // =============================================================================
 // Key functions
 // =============================================================================

void Key_Init(void);
void Key_Event(int key, int down);
void Key_ClearStates(void);
void Key_WriteBindings(FILE *f);
void Key_SetBinding(int keynum, const char *binding);
const char *Key_KeynumToString(int keynum);

#endif // KEYS_H
