#ifndef DRAW_H
#define DRAW_H

#include "common.h"

 // =========================================================
 // Draw System
 // =========================================================

typedef struct
{
	int width, height;
	byte data[4];
} qpic_t;

 // =========================================================
 // Font data (conchars)
 // =========================================================

typedef struct
{
	short startoffset;
	short charwidth;
} fontinfo_t;

typedef struct qfont_s
{
	int width; // texture width (expected 256)
	int rowCount; // number of character rows in the atlas
	int rowHeight; // height of one character row in pixels
	fontinfo_t fontinfo[256];
	byte data[4]; // variable-sized (pixel data follows)
} qfont_t;

extern qfont_t *draw_chars;

 // Functions
void    Draw_Init(void);
qpic_t  *HUD_Init(void);
int     Draw_StringWidth(const char *str);
int     Draw_Character(int x, int y, unsigned char num);
int     Draw_String(int x, int y, unsigned char *str);
void    Draw_Pic(int x, int y, qpic_t *pic);
void    Draw_StretchPic(int x, int y, int w, int h, qpic_t *pic);
void    Draw_TransPic(int x, int y, qpic_t *pic);
void    Draw_TransPicTranslate(int x, int y, qpic_t *pic);
void    Draw_TileClear(int x, int y, int w, int h);
void    Draw_Fill(int x, int y, int w, int h, int c);
void    Draw_FillRGB(int x, int y, int w, int h, int r, int g, int b);
void    Draw_FadeScreen(void);
void    Draw_BeginDisc(void);
void    Draw_EndDisc(void);
void    Draw_ConsoleBackground(int lines);
void    Draw_TextureMode_f(void);

 // 2D state
void    GL_Set2D(void);

 // Texture upload/ pic loading
int     GL_LoadPicTexture(qpic_t *pic, char *name);
qpic_t  *Draw_PicFromWad(char *name);
qpic_t  *Draw_PicFromWad_NoScrap(char *name);
qpic_t  *Draw_CachePic(char *path);

 // Decals (decals.wad cache)
char    *Draw_NameToDecal(int decal, char *name);
int     Draw_DecalIndex(int decal);
void    *Draw_GetDecal(int index);

 // =========================================================
 // Menu Draw Helpers (centered on 320x200)
 // =========================================================

extern byte identityTable[256];
extern byte translationTable[256];

int     M_DrawCharacter(int x, int y, int num);
void    M_Print(int x, int y, const char *str);
void    M_PrintWhite(int x, int y, const char *str);
void    M_DrawTransPic(int x, int y, qpic_t *pic);
void    M_DrawPic(int x, int y, qpic_t *pic);
void    M_BuildTranslationTable(int top, int bottom);
void    M_DrawTransPicTranslate(int x, int y, qpic_t *pic);
void    M_DrawTextBox(int x, int y, int width, int lines);

#endif // DRAW_H
