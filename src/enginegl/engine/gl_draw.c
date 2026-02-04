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
#include "nullsubs.h"

typedef struct
{
	int texnum;
	float sl, tl, sh, th;
} glpic_t;

typedef struct
{
	char name[64];
	qpic_t pic;
	byte padding[32];
} cachepic_t;

typedef struct
{
	char name[64];
	cache_user_t cache;
} cacheentry_t;

typedef struct cachewad_s
{
	const char *name;
	cacheentry_t *cache;
	int cacheCount;
	int cacheMax;
	lumpinfo_t *lumps;
	int lumpCount;
	int cacheExtra;
	int (*pfnCacheBuild)(struct cachewad_s *wad, void *buffer);
} cachewad_t;

cvar_t gl_nobind = { "gl_nobind", "0" };
cvar_t gl_max_size = { "gl_max_size", "1024" };
cvar_t gl_round_down = { "gl_round_down", "3" };
cvar_t gl_picmip = { "gl_picmip", "0" };

int gl_lightmap_format = GL_RGBA_FORMAT;
int gl_solid_format = GL_RGB_FORMAT;
int gl_alpha_format = GL_RGBA_FORMAT;

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

typedef struct
{
	const char *name;
	int minimize;
	int maximize;
} glmode_t;

static glmode_t gl_modes[6] =
{
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

int texture_extension_number = 1;
int currenttexture = -1;

gltexture_t gltextures[MAX_GLTEXTURES];
int numgltextures;

qfont_t *draw_chars;
static int char_texture;

static byte conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
static qpic_t *conback = (qpic_t *)&conback_buffer[0];

static int translate_texture;
static int scrap_texnum;

static qpic_t *draw_disc;
static qpic_t *draw_backtile;

static cachewad_t decal_wad;

static byte decal_names[255][16];

static cachepic_t menu_cachepics[128];
static int menu_numcachepics;

static byte menuplyr_pixels[4096];

int texels;
int pic_count;
int pic_texels;

byte gammatable[256];

static int scrap_uploads;
static qboolean scrap_dirty;
static qboolean g_skip_gammatable;

#define SCRAP_WIDTH 256
#define SCRAP_HEIGHT 256

static int scrap_allocated[SCRAP_WIDTH];
static byte scrap_texels[SCRAP_WIDTH * SCRAP_HEIGHT];

#define GL_UPLOAD_MAX_PIXELS 0x80000
static byte gl_upload_buffer[GL_UPLOAD_MAX_PIXELS * 4];
static unsigned int gl_upload_temp[GL_UPLOAD_MAX_PIXELS];

static int Scrap_AllocBlock(int w, int h, int *x, int *y);
static void Scrap_Upload(void);

static byte *COM_LoadTempFile(char *path);

void Draw_LoadWad(const char *name, int cacheMax, cachewad_t *wad);
void Draw_SetWadCallback(cachewad_t *wad, int (*pfnCacheBuild)(cachewad_t *wad, void *buffer), int cacheExtra);

int Draw_MiptexTexture(cachewad_t *wad, void *buffer);

char *Draw_NameToDecal(int decal, char *name);
int Draw_DecalIndex(int decal);

int Draw_CacheIndex(cachewad_t *wad, const char *name);
void *Draw_CacheGet(cachewad_t *wad, int index);
void *Draw_GetDecal(int index);

void GL_Bind(int texnum)
{
	if (gl_nobind.value != 0.0f)
		texnum = char_texture;

	if (texnum == currenttexture)
		return;

	currenttexture = texnum;
	bindTexFunc(GL_TEXTURE_2D, texnum);
}

void GL_Texels_f(void)
{
	Con_Printf("Current uploaded texels: %i\n", texels);
}

void Draw_TextureMode_f(void)
{
	if (Cmd_Argc() == 1)
	{
		for (int i = 0; i < 6; ++i)
		{
			if (gl_filter_min == gl_modes[i].minimize)
			{
				Con_Printf("%s\n", gl_modes[i].name);
				return;
			}
		}

		Con_Printf("current filter is unknown???\n");
		return;
	}

	const char *requested = Cmd_Argv(1);
	int modeIndex = -1;

	for (int i = 0; i < 6; ++i)
	{
		if (!Q_strcasecmp((char *)gl_modes[i].name, (char *)requested))
		{
			modeIndex = i;
			break;
		}
	}

	if (modeIndex == -1)
	{
		Con_Printf("bad filter name\n");
		return;
	}

	gl_filter_min = gl_modes[modeIndex].minimize;
	gl_filter_max = gl_modes[modeIndex].maximize;

	for (int i = 0; i < numgltextures; ++i)
	{
		if (!gltextures[i].mipmap)
			continue;

		GL_Bind(gltextures[i].texnum);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (float)gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)gl_filter_max);
	}
}

void Draw_Init(void)
{
	Draw_LoadWad("decals.wad", 128, &decal_wad);
	Draw_SetWadCallback(&decal_wad, Draw_MiptexTexture, 32);

	Cvar_RegisterVariable(&gl_nobind);
	Cvar_RegisterVariable(&gl_max_size);
	Cvar_RegisterVariable(&gl_round_down);
	Cvar_RegisterVariable(&gl_picmip);

	memset(decal_names, 0, sizeof(decal_names));

	if (gl_renderer && !Q_strncasecmp((char *)gl_renderer, "3dfx", 4))
		Cvar_Set("gl_max_size", "256");

	Cmd_AddCommand("gl_texturemode", Draw_TextureMode_f);
	Cmd_AddCommand("gl_texels", GL_Texels_f);

	for (int i = 0; i < 256; ++i)
		gammatable[i] = (byte)i;

	memset(scrap_allocated, 0, sizeof(scrap_allocated));
	memset(scrap_texels, 0xFF, sizeof(scrap_texels));
	scrap_dirty = false;

	draw_chars = (qfont_t *)W_GetLumpName("conchars");

	qpic_t *cb = (qpic_t *)COM_LoadTempFile("gfx/conback.lmp");
	if (!cb)
		Sys_Error("Couldn't load gfx/conback.lmp");

	SwapPic((miptex_t *)cb);

	{
		char buffer[40];
		sprintf(buffer, "(gl %4.2f) %4.2f", 0.91f, 1.07f);
		strlen(buffer);

		const unsigned int length = (unsigned int)strlen(buffer) + 1u;
		for (unsigned int i = 0; i < length - 1u; ++i)
			Loop_Shutdown();
	}

	const int fontHeight = draw_chars->rowHeight * draw_chars->rowCount;
	byte *fontPixels = (byte *)draw_chars + 1036;
	byte *fontPalette = fontPixels + (fontHeight << 8) + 2;

	char_texture = GL_LoadTexture("conchars", 256, fontHeight, fontPixels, 0, 1, fontPalette);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (float)GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)GL_NEAREST);

	conback->width = cb->width;
	conback->height = cb->height;

	glpic_t *gl = (glpic_t *)conback->data;
	gl->texnum = GL_LoadTexture("conback", 320, 200, cb->data, 0, 0, (byte *)cb + 64010);
	gl->sl = 0.0f;
	gl->tl = 0.0f;
	gl->sh = 1.0f;
	gl->th = 1.0f;

	translate_texture = texture_extension_number;
	scrap_texnum = texture_extension_number + 1;
	texture_extension_number += 2;

	draw_disc = Draw_PicFromWad_NoScrap("lambda");
	draw_backtile = Draw_PicFromWad_NoScrap("xlambda1");

	glPixelTransferf(GL_RED_SCALE, 1.0f);
	glPixelTransferf(GL_GREEN_SCALE, 1.0f);
	glPixelTransferf(GL_BLUE_SCALE, 1.0f);

	glPixelTransferf(GL_RED_BIAS, 0.0f);
	glPixelTransferf(GL_GREEN_BIAS, 0.0f);
	glPixelTransferf(GL_BLUE_BIAS, 0.0f);
}

static int Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	int best = SCRAP_HEIGHT;
	int bestx = -1;

	if (w <= 0 || w > SCRAP_WIDTH || h <= 0 || h > SCRAP_HEIGHT)
		return -1;

	for (int i = 0; i <= SCRAP_WIDTH - w; ++i)
	{
		int best2 = 0;
		int j;

		for (j = 0; j < w; ++j)
		{
			if (scrap_allocated[i + j] >= best)
				break;
			if (scrap_allocated[i + j] > best2)
				best2 = scrap_allocated[i + j];
		}

		if (j == w)
		{
			*x = i;
			*y = best2;
			best = best2;
			bestx = i;
		}
	}

	if (bestx == -1 || best + h > SCRAP_HEIGHT)
		return -1;

	for (int i = 0; i < w; ++i)
		scrap_allocated[bestx + i] = best + h;

	return 0;
}

static void Scrap_Upload(void)
{
	static unsigned int scrap_rgba[SCRAP_WIDTH * SCRAP_HEIGHT];

	++scrap_uploads;

	GL_Bind(scrap_texnum);

	if (host_basepal)
	{
		const byte *pal = (const byte *)host_basepal;
		for (int i = 0; i < SCRAP_WIDTH * SCRAP_HEIGHT; ++i)
		{
			const byte idx = scrap_texels[i];
			if (idx == 255)
			{
				scrap_rgba[i] = 0;
				continue;
			}
			const int p = idx * 3;
			scrap_rgba[i] = (unsigned int)pal[p] | ((unsigned int)pal[p + 1] << 8) | ((unsigned int)pal[p + 2] << 16) | 0xFF000000u;
		}
	}
	else
	{
		memset(scrap_rgba, 0, sizeof(scrap_rgba));
	}

	GL_Upload32(scrap_rgba, SCRAP_WIDTH, SCRAP_HEIGHT, false, true);
	scrap_dirty = false;
}

static byte *COM_LoadTempFile(char *path)
{
	return COM_LoadFile(path, 2);
}

int GL_LoadPicTexture(qpic_t *pic, char *name)
{
	const int pixelCount = pic->width * pic->height;
	byte *palette = (byte *)pic + pixelCount + 10;

	return GL_LoadTexture(name, pic->width, pic->height, pic->data, 0, 1, palette);
}

qpic_t *Draw_PicFromWad(char *name)
{
	qpic_t *pic;
	glpic_t *gl;
	int scrapnum;
	int x, y;

	pic = (qpic_t *)W_GetLumpName(name);
	gl = (glpic_t *)pic->data;

	if (pic->width >= 64 || pic->height >= 64 || (scrapnum = Scrap_AllocBlock(pic->width, pic->height, &x, &y)) == -1)
	{
		gl->texnum = GL_LoadPicTexture(pic, name);
		gl->sl = 0.0f;
		gl->tl = 0.0f;
		gl->sh = 1.0f;
		gl->th = 1.0f;
		return pic;
	}

	scrap_dirty = true;
	for (int i = 0; i < pic->height; ++i)
		memcpy(&scrap_texels[(y + i) * SCRAP_WIDTH + x], &pic->data[i * pic->width], pic->width);

	gl->texnum = scrap_texnum + scrapnum;
	gl->sl = ((float)x + 0.01f) / (float)SCRAP_WIDTH;
	gl->sh = ((float)(x + pic->width) - 0.01f) / (float)SCRAP_WIDTH;
	gl->tl = ((float)y + 0.01f) / (float)SCRAP_HEIGHT;
	gl->th = ((float)(y + pic->height) - 0.01f) / (float)SCRAP_HEIGHT;

	++pic_count;
	pic_texels += pic->width * pic->height;

	return pic;
}

qpic_t *Draw_PicFromWad_NoScrap(char *name)
{
	qpic_t *pic;
	glpic_t *gl;
	unsigned int *rgba;
	byte *palette;
	int pixelCount;
	int texnum;
	char identifier[64];
	char *id = name;

	pic = (qpic_t *)W_GetLumpName(name);
	if (!pic)
		return NULL;

	gl = (glpic_t *)pic->data;

	if (name[0])
	{
		Q_strncpy(identifier, name, (int)(sizeof(identifier) - 1));
		identifier[sizeof(identifier) - 1] = 0;
		id = identifier;

		while (1)
		{
			for (int i = 0; i < numgltextures; ++i)
			{
				if (!strcmp(id, gltextures[i].identifier))
				{
					if (gltextures[i].width == pic->width && gltextures[i].height == pic->height)
						return pic;

					++id[3];
					if (!id[3])
						goto allocate;

					goto restart;
				}
			}
			break;

		restart:;
		}
	}

allocate:
	if (numgltextures + 1 >= MAX_GLTEXTURES)
		Sys_Error("Texture Overflow: MAX_GLTEXTURES");

	gltextures[numgltextures].texnum = texture_extension_number;
	strcpy(gltextures[numgltextures].identifier, id);
	gltextures[numgltextures].width = pic->width;
	gltextures[numgltextures].height = pic->height;
	gltextures[numgltextures].mipmap = 0;
	++numgltextures;

	GL_Bind(texture_extension_number);

	pixelCount = pic->width * pic->height;
	rgba = (unsigned int *)malloc((size_t)pixelCount * sizeof(unsigned int));
	if (!rgba)
		Sys_Error("Draw_PicFromWad_NoScrap: not enough memory");

	palette = (byte *)pic + pixelCount + 10;
	for (int i = 0; i < 768; ++i)
		palette[i] = gammatable[palette[i]];

	for (int i = 0; i < pixelCount; ++i)
	{
		const byte idx = pic->data[i];
		const int p = idx * 3;
		const unsigned int color = (unsigned int)palette[p] | ((unsigned int)palette[p + 1] << 8) | ((unsigned int)palette[p + 2] << 16);
		rgba[i] = (idx != 0xFF) ? (color | 0xFF000000u) : color;
	}

	GL_Upload32(rgba, pic->width, pic->height, false, true);

	texnum = texture_extension_number++;
	gl->texnum = texnum;
	gl->sl = 0.0f;
	gl->tl = 0.0f;
	gl->sh = 1.0f;
	gl->th = 1.0f;

	free(rgba);

	return pic;
}

qpic_t *Draw_CachePic(char *path)
{
	cachepic_t *pic;
	qpic_t *dat;
	glpic_t *gl;

	for (int i = 0; i < menu_numcachepics; ++i)
	{
		if (!strcmp(path, menu_cachepics[i].name))
			return &menu_cachepics[i].pic;
	}

	if (menu_numcachepics == 128)
		Sys_Error("menu_numcachepics == MAX_CACHED_PICS");

	pic = &menu_cachepics[menu_numcachepics++];
	strcpy(pic->name, path);

	dat = (qpic_t *)COM_LoadTempFile(path);
	if (!dat)
	{
		Con_Printf("Draw_CachePic: failed to load %s", path);
		return NULL;
	}

	SwapPic((miptex_t *)dat);

	if (!strcmp(path, "gfx/menuplyr.lmp"))
		memcpy(menuplyr_pixels, dat->data, dat->width * dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	const qboolean old_skip = g_skip_gammatable;
	g_skip_gammatable = true;
	gl->texnum = GL_LoadPicTexture(dat, path);
	g_skip_gammatable = old_skip;
	gl->sl = 0.0f;
	gl->tl = 0.0f;
	gl->sh = 1.0f;
	gl->th = 1.0f;

	return &pic->pic;
}

int Draw_StringWidth(const char *str)
{
	int total = 0;

	if (!str || !*str || !draw_chars)
		return 0;

	while (*str)
	{
		const unsigned char c = (unsigned char)*str++;
		total += draw_chars->fontinfo[c].charwidth;
	}

	return total;
}

int Draw_Character(int x, int y, unsigned char num)
{
	const int rowHeight = draw_chars ? draw_chars->rowHeight : 0;

	if (!draw_chars)
		return 0;

	if (-rowHeight >= y)
		return 0;

	const int charWidth = draw_chars->fontinfo[num].charwidth;
	if (y < 0)
		return charWidth;

	const int startoffset = draw_chars->fontinfo[num].startoffset;
	const int rowCount = draw_chars->rowCount;

	const float rowFrac = (rowCount > 0) ? (1.0f / (float)rowCount) : 0.0f;

	const float s0 = (float)(startoffset & 0xFF) * (1.0f / 256.0f);
	const float s1 = s0 + (float)charWidth * (1.0f / 256.0f);

	const int rowIndex = ((startoffset & 0xFF00) >> 8) / rowHeight;
	const float t0 = (float)rowIndex * rowFrac;
	const float t1 = t0 + rowFrac;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	GL_Bind(char_texture);

	glBegin(GL_QUADS);
	glTexCoord2f(s0, t0);
	glVertex2f((float)x, (float)y);

	glTexCoord2f(s1, t0);
	glVertex2f((float)(x + charWidth), (float)y);

	glTexCoord2f(s1, t1);
	glVertex2f((float)(x + charWidth), (float)(y + rowHeight));

	glTexCoord2f(s0, t1);
	glVertex2f((float)x, (float)(y + rowHeight));
	glEnd();

	return charWidth;
}

int Draw_String(int x, int y, unsigned char *str)
{
	if (!str)
		return x;

	while (*str)
	{
		const unsigned char c = *str++;
		x += Draw_Character(x, y, c);
	}

	return x;
}

void Draw_Pic(int x, int y, qpic_t *pic)
{
	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload();

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glpic_t *gl = (glpic_t *)pic->data;
	GL_Bind(gl->texnum);

	glBegin(GL_QUADS);
	glTexCoord2f(gl->sl, gl->tl);
	glVertex2f((float)x, (float)y);

	glTexCoord2f(gl->sh, gl->tl);
	glVertex2f((float)(x + pic->width), (float)y);

	glTexCoord2f(gl->sh, gl->th);
	glVertex2f((float)(x + pic->width), (float)(y + pic->height));

	glTexCoord2f(gl->sl, gl->th);
	glVertex2f((float)x, (float)(y + pic->height));
	glEnd();
}

void Draw_StretchPic(int x, int y, int w, int h, qpic_t *pic)
{
	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload();

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glpic_t *gl = (glpic_t *)pic->data;
	GL_Bind(gl->texnum);

	glBegin(GL_QUADS);
	glTexCoord2f(gl->sl, gl->tl);
	glVertex2f((float)x, (float)y);

	glTexCoord2f(gl->sh, gl->tl);
	glVertex2f((float)(x + w), (float)y);

	glTexCoord2f(gl->sh, gl->th);
	glVertex2f((float)(x + w), (float)(y + h - 1));

	glTexCoord2f(gl->sl, gl->th);
	glVertex2f((float)x, (float)(y + h - 1));
	glEnd();
}

void Draw_TransPic(int x, int y, qpic_t *pic)
{
	if (!pic)
		return;

	if (x < 0 || x + pic->width > (int)vid.width || y < 0 || y + pic->height > (int)vid.height)
		Sys_Error("Draw_TransPic: bad coordinates");

	Draw_Pic(x, y, pic);
}

void Draw_TransPicTranslate(int x, int y, qpic_t *pic)
{
	if (!pic)
		return;

	static unsigned int pixels[64 * 64];

	const int srcW = pic->width;
	const int srcH = pic->height;

	for (int oy = 0; oy < 64; ++oy)
	{
		const int sy = (oy * srcH) >> 6;
		for (int ox = 0; ox < 64; ++ox)
		{
			const int sx = (ox * srcW) >> 6;
			const byte src = menuplyr_pixels[sy * srcW + sx];
			pixels[oy * 64 + ox] = (src == 255) ? 255u : 0xFF0000FFu;
		}
	}

	GL_Bind(translate_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (float)GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)GL_LINEAR);

	glColor3f(1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f((float)x, (float)y);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f((float)(x + pic->width), (float)y);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f((float)(x + pic->width), (float)(y + pic->height));

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f((float)x, (float)(y + pic->height));
	glEnd();
}

void Draw_ConsoleBackground(int lines)
{
	char buffer[100];

	Draw_Pic(0, lines - 200, conback);
	sprintf(buffer, "Half-Life Alpha v%5.2f", 0.52f);

	const int x = (int)vid.conwidth - Draw_StringWidth(buffer);
	Draw_String(x, 0, (unsigned char *)buffer);
}

void Draw_FillRGB(int x, int y, int w, int h, int r, int g, int b)
{
	glDisable(GL_TEXTURE_2D);
	glColor3f((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f);

	glBegin(GL_QUADS);
	glVertex2f((float)x, (float)y);
	glVertex2f((float)(x + w), (float)y);
	glVertex2f((float)(x + w), (float)(y + h));
	glVertex2f((float)x, (float)(y + h));
	glEnd();

	glColor3f(1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
}

void Draw_TileClear(int x, int y, int w, int h)
{
	Draw_FillRGB(x, y, w, h, 0, 0, 0);
}

void Draw_Fill(int x, int y, int w, int h, int c)
{
	const byte *pal = (const byte *)host_basepal;
	const int p = (c & 0xFF) * 3;
	Draw_FillRGB(x, y, w, h, pal[p], pal[p + 1], pal[p + 2]);
}

void Draw_FadeScreen(void)
{
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glColor4f(0.0f, 0.0f, 0.0f, 0.8f);

	glBegin(GL_QUADS);
	glVertex2f(0.0f, 0.0f);
	glVertex2f(320.0f, 0.0f);
	glVertex2f(320.0f, 200.0f);
	glVertex2f(0.0f, 200.0f);
	glEnd();

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	VID_HandlePause();
}

void Draw_BeginDisc(void)
{
	if (!draw_disc)
		return;

	glPushMatrix();

	glViewport(glx, gly, glwidth, glheight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 640.0, 480.0, 0.0, -99999.0, 99999.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glDrawBuffer(GL_FRONT);
	Draw_Pic(320 - draw_disc->width / 2, 240 - draw_disc->height / 2, draw_disc);
	glDrawBuffer(GL_BACK);

	glPopMatrix();
}

void Draw_EndDisc(void)
{
	;
}

void GL_Set2D(void)
{
	glViewport(glx, gly, glwidth, glheight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 320.0, 200.0, 0.0, -99999.0, 99999.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

int GL_FindTexture(char *identifier)
{
	for (int i = 0; i < numgltextures; ++i)
	{
		if (!strcmp(identifier, gltextures[i].identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

static void GL_ResampleTexture(const byte *in, int inwidth, int inheight, byte *out, int outwidth, int outheight)
{
	unsigned int p1[1024];
	unsigned int p2[1024];

	const unsigned int fracstep = ((unsigned int)inwidth << 16) / (unsigned int)outwidth;

	unsigned int frac = fracstep >> 2;
	for (int i = 0; i < outwidth; ++i)
	{
		const unsigned int f = frac;
		frac += fracstep;
		p1[i] = 4u * (unsigned int)(f >> 16);
	}

	frac = (3u * (fracstep >> 2));
	for (int i = 0; i < outwidth; ++i)
	{
		const unsigned int f = frac;
		frac += fracstep;
		p2[i] = 4u * (unsigned int)(f >> 16);
	}

	for (int y = 0; y < outheight; ++y)
	{
		const double yd = (double)y;
		const double srcH = (double)inheight;
		const double dstH = (double)outheight;

		const byte *row1 = in + 4 * inwidth * (int)((yd + 0.25) * srcH / dstH);
		const byte *row2 = in + 4 * inwidth * (int)((yd + 0.75) * srcH / dstH);

		for (int x = 0; x < outwidth; ++x)
		{
			const byte *pix1 = row1 + p1[x];
			const byte *pix2 = row1 + p2[x];
			const byte *pix3 = row2 + p1[x];
			const byte *pix4 = row2 + p2[x];

			*out++ = (byte)((pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2);
			*out++ = (byte)((pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2);
			*out++ = (byte)((pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2);
			*out++ = (byte)((pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2);
		}
	}
}

static void GL_MipMap(byte *in, int width, int height)
{
	const int row = width * 4;
	byte *out = in;

	for (int y = 0; y < (height >> 1); ++y)
	{
		byte *inrow = in + (y * 2) * row;
		byte *inrow2 = inrow + row;

		for (int x = 0; x < width; x += 2)
		{
			const byte *a = inrow + x * 4;
			const byte *b = a + 4;
			const byte *c = inrow2 + x * 4;
			const byte *d = c + 4;

			out[0] = (byte)((a[0] + b[0] + c[0] + d[0]) >> 2);
			out[1] = (byte)((a[1] + b[1] + c[1] + d[1]) >> 2);
			out[2] = (byte)((a[2] + b[2] + c[2] + d[2]) >> 2);
			out[3] = (byte)((a[3] + b[3] + c[3] + d[3]) >> 2);
			out += 4;
		}
	}
}

void GL_Upload32(unsigned *data, int width, int height, qboolean mipmap, qboolean alpha)
{
	int scaledWidth = 1;
	int scaledHeight = 1;

	while (scaledWidth < width)
		scaledWidth <<= 1;

	if (gl_round_down.value > 0.0f && width < scaledWidth)
	{
		const int roundDown = (int)gl_round_down.value;
		if (gl_round_down.value == 1.0f || (scaledWidth >> roundDown) < (scaledWidth - width))
			scaledWidth >>= 1;
	}

	while (scaledHeight < height)
		scaledHeight <<= 1;

	if (gl_round_down.value > 0.0f && height < scaledHeight)
	{
		const int roundDown = (int)gl_round_down.value;
		if (gl_round_down.value == 1.0f || (scaledHeight >> roundDown) < (scaledHeight - height))
			scaledHeight >>= 1;
	}

	const int picmip = (int)gl_picmip.value;
	int uploadWidth = scaledWidth >> picmip;
	int uploadHeight = scaledHeight >> picmip;

	if (uploadWidth < 1)
		uploadWidth = 1;
	if (uploadHeight < 1)
		uploadHeight = 1;

	if ((float)uploadWidth > gl_max_size.value)
		uploadWidth = (int)gl_max_size.value;
	if ((float)uploadHeight > gl_max_size.value)
		uploadHeight = (int)gl_max_size.value;

	if (uploadWidth < 1)
		uploadWidth = 1;
	if (uploadHeight < 1)
		uploadHeight = 1;

	if ((unsigned int)(uploadWidth * uploadHeight) > GL_UPLOAD_MAX_PIXELS)
		Sys_Error("GL_LoadTexture: too big");

	const GLint internalFormat = alpha ? gl_alpha_format : gl_solid_format;
	const byte *pixels = (const byte *)data;

	if (width == uploadWidth && height == uploadHeight)
	{
		if (!mipmap)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, uploadWidth, uploadHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			goto set_filters;
		}

		memcpy(gl_upload_buffer, pixels, (size_t)width * (size_t)height * 4u);
	}
	else
	{
		GL_ResampleTexture(pixels, width, height, gl_upload_buffer, uploadWidth, uploadHeight);
	}

	texels += uploadWidth * uploadHeight;
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, uploadWidth, uploadHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, gl_upload_buffer);

	if (mipmap)
	{
		int level = 0;
		while (uploadWidth > 1 || uploadHeight > 1)
		{
			const int oldHeight = uploadHeight;
			uploadHeight >>= 1;
			GL_MipMap(gl_upload_buffer, uploadWidth, oldHeight);

			uploadWidth >>= 1;
			if (uploadWidth < 1)
				uploadWidth = 1;
			if (uploadHeight < 1)
				uploadHeight = 1;

			++level;
			texels += uploadWidth * uploadHeight;
			glTexImage2D(GL_TEXTURE_2D, level, internalFormat, uploadWidth, uploadHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, gl_upload_buffer);
		}
	}

set_filters:
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (float)(mipmap ? gl_filter_min : gl_filter_max));
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (float)gl_filter_max);
}

void GL_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean alpha, byte *palette)
{
	const int pixelCount = width * height;

	if ((unsigned int)pixelCount > GL_UPLOAD_MAX_PIXELS)
		Sys_Error("GL_Upload8: too big");

	if (!palette)
		return;

	if (!g_skip_gammatable)
	{
		for (int i = 0; i < 768; ++i)
			palette[i] = gammatable[palette[i]];
	}

	if (alpha)
	{
		qboolean noAlpha = true;

		for (int i = 0; i < pixelCount; ++i)
		{
			const byte idx = data[i];

			if (alpha == 2)
			{
				const unsigned int rgb = (unsigned int)palette[765] | ((unsigned int)palette[766] << 8) | ((unsigned int)palette[767] << 16);
				gl_upload_temp[i] = rgb | ((unsigned int)idx << 24);
				noAlpha = false;
			}
			else if (idx == 255)
			{
				gl_upload_temp[i] = 0;
				noAlpha = false;
			}
			else
			{
				const int p = idx * 3;
				gl_upload_temp[i] = (unsigned int)palette[p] | ((unsigned int)palette[p + 1] << 8) | ((unsigned int)palette[p + 2] << 16) | 0xFF000000u;
			}
		}

		if (noAlpha)
			alpha = false;
	}
	else
	{
		if ((pixelCount & 3) != 0)
			Sys_Error("GL_Upload8: pixelCount & 3");

		for (int i = 0; i < pixelCount; ++i)
		{
			const int p = data[i] * 3;
			gl_upload_temp[i] = (unsigned int)palette[p] | ((unsigned int)palette[p + 1] << 8) | ((unsigned int)palette[p + 2] << 16) | 0xFF000000u;
		}
	}

	GL_Upload32(gl_upload_temp, width, height, mipmap, alpha);
}

int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mipmap, int alpha, byte *palette)
{
	gltexture_t *glt;
	char workingIdentifier[64];
	char *id = identifier;

	if (identifier[0])
	{
		// NOTE: The original code mutates the identifier string in-place to
		// resolve collisions. Use a local copy to avoid writing into read-only
		// string literal storage on modern toolchains.
		Q_strncpy(workingIdentifier, identifier, (int)(sizeof(workingIdentifier) - 1));
		workingIdentifier[sizeof(workingIdentifier) - 1] = 0;
		id = workingIdentifier;

		while (1)
		{
			for (int i = 0; i < numgltextures; ++i)
			{
				if (!strcmp(id, gltextures[i].identifier))
				{
					if (gltextures[i].width == width && gltextures[i].height == height)
						return gltextures[i].texnum;

					++id[3];
					if (!id[3])
						goto allocate;

					goto restart;
				}
			}
			break;

		restart:;
		}
	}

allocate:
	if (++numgltextures >= MAX_GLTEXTURES)
		Sys_Error("Texture Overflow: MAX_GLTEXTURES");

	glt = &gltextures[numgltextures - 1];
	strcpy(glt->identifier, id);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	GL_Bind(texture_extension_number);
	GL_Upload8(data, width, height, mipmap, alpha, palette);

	return texture_extension_number++;
}

int Draw_MiptexTexture(cachewad_t *wad, void *buffer)
{
	miptex_t mt;

	if (wad->cacheExtra != 32)
		Sys_Error("Draw_MiptexTexture: Bad cached wad %s\n", wad->name);

	memcpy(&mt, (byte *)buffer + wad->cacheExtra, sizeof(mt));

	memcpy(buffer, &mt, 16);
	((int *)buffer)[4] = LittleLong(mt.width);
	((int *)buffer)[5] = LittleLong(mt.height);

	((int *)buffer)[10] = 0;
	((int *)buffer)[9] = 0;
	((int *)buffer)[8] = 0;
	((int *)buffer)[12] = 0;
	((int *)buffer)[11] = 0;

	int *offsets = (int *)buffer + 13;
	for (int i = 0; i < 4; ++i)
		offsets[i] = LittleLong(mt.offsets[i]) + wad->cacheExtra;

	const int w = ((int *)buffer)[4];
	const int h = ((int *)buffer)[5];
	const int mipSize = w * h;

	byte *pixels = (byte *)buffer + offsets[0];
	byte *palette = pixels + mipSize + (mipSize >> 2) + (mipSize >> 4) + (mipSize >> 6) + 2;

	int texnum;
	if (palette[765] || palette[766] || palette[767] != 0xFF)
	{
		*(byte *)buffer = 125;
		texnum = GL_LoadTexture((char *)buffer, w, h, pixels, 1, 2, palette);
	}
	else
	{
		*(byte *)buffer = 123;
		texnum = GL_LoadTexture((char *)buffer, w, h, pixels, 1, 1, palette);
	}

	((int *)buffer)[6] = texnum;
	return texnum;
}

void Draw_LoadWad(const char *name, int cacheMax, cachewad_t *wad)
{
	int handle;
	const int filelen = COM_OpenFile(name, &handle);

	if (handle == -1)
		Sys_Error("Draw_LoadWad: Couldn't open %s\n", name);

	wadinfo_t header;
	Sys_FileRead(handle, &header, sizeof(header));

	if (memcmp(header.identification, "WAD3", 4))
		Sys_Error("Wad file %s doesn't have WAD3 id\n", name);

	const int dirSize = filelen - header.infotableofs;
	wad->lumps = (lumpinfo_t *)malloc((size_t)dirSize);

	Sys_FileSeek(handle, header.infotableofs);
	Sys_FileRead(handle, wad->lumps, dirSize);
	COM_CloseFile(handle);

	for (int i = 0; i < header.numlumps; ++i)
		W_CleanupName(wad->lumps[i].name, wad->lumps[i].name);

	wad->lumpCount = header.numlumps;
	wad->cacheCount = 0;
	wad->cacheMax = cacheMax;
	wad->name = name;

	wad->cache = (cacheentry_t *)malloc((size_t)cacheMax * sizeof(*wad->cache));
	memset(wad->cache, 0, (size_t)cacheMax * sizeof(*wad->cache));

	wad->cacheExtra = 0;
	wad->pfnCacheBuild = NULL;
}

void Draw_SetWadCallback(cachewad_t *wad, int (*pfnCacheBuild)(cachewad_t *wad, void *buffer), int cacheExtra)
{
	wad->pfnCacheBuild = pfnCacheBuild;
	wad->cacheExtra = cacheExtra;
}

char *Draw_NameToDecal(int decal, char *name)
{
	if (decal < 255)
	{
		strncpy((char *)decal_names[decal], name, 15);
		decal_names[decal][15] = 0;
		return (char *)decal_names[decal];
	}

	return NULL;
}

int Draw_DecalIndex(int decal)
{
	if (!decal_names[decal][0])
		Sys_Error("Used decal #%d without no name\n", decal);

	return Draw_CacheIndex(&decal_wad, (const char *)decal_names[decal]);
}

int Draw_CacheIndex(cachewad_t *wad, const char *name)
{
	cacheentry_t *entry = wad->cache;
	int index = 0;

	while (index < wad->cacheCount)
	{
		if (!strcmp(name, entry->name))
			return index;

		++index;
		++entry;
	}

	if (wad->cacheCount == wad->cacheMax)
		Sys_Error("Cache wad (%s) out of %d entries", wad->name, wad->cacheMax);

	++wad->cacheCount;
	strcpy(entry->name, name);
	return index;
}

void *Draw_CacheGet(cachewad_t *wad, int index)
{
	if (wad->cacheCount <= index)
		Sys_Error("Cache wad indexed before load %s: %d", wad->name, index);

	cacheentry_t *entry = &wad->cache[index];
	void *data = Cache_Check(&entry->cache);
	if (data)
		return data;

	char base[16];
	char clean[16];
	COM_FileBase(entry->name, base);
	W_CleanupName(base, clean);

	lumpinfo_t *lump = NULL;
	for (int i = 0; i < wad->lumpCount; ++i)
	{
		if (!strcmp(clean, wad->lumps[i].name))
		{
			lump = &wad->lumps[i];
			break;
		}
	}

	if (!lump)
		Sys_Error("Draw_CacheGet: couldn't find %s in %s", entry->name, wad->name);

	int handle;
	COM_OpenFile(wad->name, &handle);
	if (handle == -1)
		return NULL;

	byte *buffer = Cache_Alloc(&entry->cache, lump->disksize + wad->cacheExtra + 1, clean);
	if (!buffer)
		Sys_Error("Draw_CacheGet: not enough space for %s in %s", entry->name, wad->name);

	buffer[wad->cacheExtra + lump->disksize] = 0;
	Sys_FileSeek(handle, lump->filepos);
	Sys_FileRead(handle, buffer + wad->cacheExtra, lump->disksize);
	COM_CloseFile(handle);

	if (wad->pfnCacheBuild)
		wad->pfnCacheBuild(wad, buffer);

	if (!entry->cache.data)
		Sys_Error("Draw_CacheGet: failed to load %s", entry->name);

	return entry->cache.data;
}

void *Draw_GetDecal(int index)
{
	return Draw_CacheGet(&decal_wad, index);
}
