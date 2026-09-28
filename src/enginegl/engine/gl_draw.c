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
// gl_draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#include "nullsubs.h"

#define GLQUAKE_VERSION		0.91
#define QUAKE_VERSION		1.07

#define CONBACK_WIDTH		320
#define CONBACK_HEIGHT		200

#define CONCHARS_WIDTH		256		// width of the conchars texture

#define MAX_CACHED_PICS		128
#define MAX_DECAL_NAMES		255
#define MAX_DECAL_CACHE		128		// decals.wad cache entries

#define SCRAP_WIDTH			256
#define SCRAP_HEIGHT		256

#define MAX_UPLOAD_PIXELS	(512 * 1024)

typedef struct
{
	int			texnum;
	float		sl, tl, sh, th;
} glpic_t;

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

typedef struct
{
	char			name[64];
	cache_user_t	cache;
} cacheentry_t;

typedef struct cachewad_s
{
	const char		*name;
	cacheentry_t	*cache;
	int				cacheCount;
	int				cacheMax;
	lumpinfo_t		*lumps;
	int				lumpCount;
	int				cacheExtra;		// bytes reserved in front of each lump
	int				(*pfnCacheBuild)(struct cachewad_s *wad, void *buffer);
} cachewad_t;

cvar_t		gl_nobind = {"gl_nobind", "0"};
cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t		gl_round_down = {"gl_round_down", "3"};
cvar_t		gl_picmip = {"gl_picmip", "0"};

int			gl_lightmap_format = GL_RGBA_FORMAT;
int			gl_solid_format = GL_RGB_FORMAT;
int			gl_alpha_format = GL_RGBA_FORMAT;

int			gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int			gl_filter_max = GL_LINEAR;

typedef struct
{
	const char	*name;
	int			minimize, maximize;
} glmode_t;

#define NUM_GL_MODES	6

static glmode_t modes[NUM_GL_MODES] =
{
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

int			texture_extension_number = 1;
int			currenttexture = -1;	// to avoid unnecessary texture sets

gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

qfont_t		*draw_chars;
static int	char_texture;

static byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
static qpic_t	*conback = (qpic_t *)&conback_buffer;

static int		translate_texture;
static int		scrap_texnum;

static qpic_t	*draw_disc;
static qpic_t	*draw_backtile;

static cachewad_t	decal_wad;

static char			decal_names[MAX_DECAL_NAMES][16];

static cachepic_t	menu_cachepics[MAX_CACHED_PICS];
static int			menu_numcachepics;

static byte		menuplyr_pixels[4096];

int			texels;
int			pic_count;
int			pic_texels;

byte		gammatable[256];

static int		scrap_uploads;
static qboolean	scrap_dirty;
static qboolean	skip_gammatable;	// pic palettes are already gamma corrected

static int		scrap_allocated[SCRAP_WIDTH];
static byte		scrap_texels[SCRAP_WIDTH * SCRAP_HEIGHT];

static byte		upload_scaled[MAX_UPLOAD_PIXELS * 4];
static unsigned	upload_trans[MAX_UPLOAD_PIXELS];

static int Scrap_AllocBlock(int w, int h, int *x, int *y);
static void Scrap_Upload(void);
static byte *COM_LoadTempFile(char *path);

void Draw_LoadWad(const char *name, int cacheMax, cachewad_t *wad);
void Draw_SetWadCallback(cachewad_t *wad, int (*pfnCacheBuild)(cachewad_t *wad, void *buffer), int cacheExtra);
int Draw_MiptexTexture(cachewad_t *wad, void *buffer);
int Draw_CacheIndex(cachewad_t *wad, const char *name);
void *Draw_CacheGet(cachewad_t *wad, int index);

/*
================
GL_Bind
================
*/
void GL_Bind(int texnum)
{
	if (gl_nobind.value)
		texnum = char_texture;
	if (texnum == currenttexture)
		return;
	currenttexture = texnum;
	bindTexFunc(GL_TEXTURE_2D, texnum);
}

/*
================
GL_Texels_f
================
*/
void GL_Texels_f(void)
{
	Con_Printf("Current uploaded texels: %i\n", texels);
}

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f(void)
{
	int			i;
	char		*name;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i = 0; i < NUM_GL_MODES; i++)
		{
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf("%s\n", modes[i].name);
				return;
			}
		}
		Con_Printf("current filter is unknown???\n");
		return;
	}

	name = Cmd_Argv(1);
	for (i = 0; i < NUM_GL_MODES; i++)
	{
		if (!Q_strcasecmp(modes[i].name, name))
			break;
	}
	if (i == NUM_GL_MODES)
	{
		Con_Printf("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind(glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
================
Draw_CharToConback

The version string is not drawn into the console background.
================
*/
static void Draw_CharToConback(int num, byte *dest)
{
}

/*
===============
Draw_Init
===============
*/
void Draw_Init(void)
{
	int			i;
	qpic_t		*cb;
	glpic_t		*gl;
	char		ver[40];
	byte		*dest;
	int			x, y;
	int			height;
	byte		*pixels;

	Draw_LoadWad("decals.wad", MAX_DECAL_CACHE, &decal_wad);
	Draw_SetWadCallback(&decal_wad, Draw_MiptexTexture, sizeof(texture_t) - sizeof(miptex_t));

	Cvar_RegisterVariable(&gl_nobind);
	Cvar_RegisterVariable(&gl_max_size);
	Cvar_RegisterVariable(&gl_round_down);
	Cvar_RegisterVariable(&gl_picmip);

	memset(decal_names, 0, sizeof(decal_names));

	// 3dfx can only handle 256 wide textures
	if (gl_renderer && !Q_strncasecmp(gl_renderer, "3dfx", 4))
		Cvar_Set("gl_max_size", "256");

	Cmd_AddCommand("gl_texturemode", Draw_TextureMode_f);
	Cmd_AddCommand("gl_texels", GL_Texels_f);

	for (i = 0; i < 256; i++)
		gammatable[i] = i;

	memset(scrap_allocated, 0, sizeof(scrap_allocated));
	memset(scrap_texels, 255, sizeof(scrap_texels));
	scrap_dirty = false;

	// load the console background and the charset
	draw_chars = W_GetLumpName("conchars");

	cb = (qpic_t *)COM_LoadTempFile("gfx/conback.lmp");
	if (!cb)
		Sys_Error("Couldn't load gfx/conback.lmp");
	SwapPic((miptex_t *)cb);

	// hack the version number directly into the pic
	sprintf(ver, "(gl %4.2f) %4.2f", (float)GLQUAKE_VERSION, (float)QUAKE_VERSION);
	dest = cb->data + CONBACK_WIDTH * 186 + CONBACK_WIDTH - 11 - 8 * strlen(ver);
	y = strlen(ver);
	for (x = 0; x < y; x++)
		Draw_CharToConback(ver[x], dest + (x << 3));

	// the palette follows the pixels and a color count
	height = draw_chars->rowHeight * draw_chars->rowCount;
	pixels = draw_chars->data;
	char_texture = GL_LoadTexture("conchars", CONCHARS_WIDTH, height, pixels, false, true, pixels + height * CONCHARS_WIDTH + 2);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	conback->width = cb->width;
	conback->height = cb->height;

	gl = (glpic_t *)conback->data;
	gl->texnum = GL_LoadTexture("conback", CONBACK_WIDTH, CONBACK_HEIGHT, cb->data, false, false, cb->data + CONBACK_WIDTH * CONBACK_HEIGHT + 2);
	gl->sl = 0;
	gl->tl = 0;
	gl->sh = 1;
	gl->th = 1;

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number++;

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad_NoScrap("lambda");
	draw_backtile = Draw_PicFromWad_NoScrap("xlambda1");

	glPixelTransferf(GL_RED_SCALE, 1);
	glPixelTransferf(GL_GREEN_SCALE, 1);
	glPixelTransferf(GL_BLUE_SCALE, 1);

	glPixelTransferf(GL_RED_BIAS, 0);
	glPixelTransferf(GL_GREEN_BIAS, 0);
	glPixelTransferf(GL_BLUE_BIAS, 0);
}

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar objects into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

/*
================
Scrap_AllocBlock

Returns -1 if the block doesn't fit
================
*/
static int Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		bestx;

	if (w <= 0 || w > SCRAP_WIDTH || h <= 0 || h > SCRAP_HEIGHT)
		return -1;

	best = SCRAP_HEIGHT;
	bestx = -1;

	for (i = 0; i <= SCRAP_WIDTH - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (scrap_allocated[i + j] >= best)
				break;
			if (scrap_allocated[i + j] > best2)
				best2 = scrap_allocated[i + j];
		}
		if (j == w)
		{
			// this is a valid spot
			*x = i;
			*y = best = best2;
			bestx = i;
		}
	}

	if (bestx == -1 || best + h > SCRAP_HEIGHT)
		return -1;

	for (i = 0; i < w; i++)
		scrap_allocated[bestx + i] = best + h;

	return 0;
}

/*
================
Scrap_Upload
================
*/
static void Scrap_Upload(void)
{
	static unsigned	trans[SCRAP_WIDTH * SCRAP_HEIGHT];
	int				i;
	byte			p;
	byte			*pal;

	scrap_uploads++;
	GL_Bind(scrap_texnum);

	if (host_basepal)
	{
		pal = host_basepal;
		for (i = 0; i < SCRAP_WIDTH * SCRAP_HEIGHT; i++)
		{
			p = scrap_texels[i];
			if (p == 255)
				trans[i] = 0;	// transparent
			else
				trans[i] = pal[p * 3] | (pal[p * 3 + 1] << 8) | (pal[p * 3 + 2] << 16) | 0xff000000;
		}
	}
	else
	{
		memset(trans, 0, sizeof(trans));
	}

	GL_Upload32(trans, SCRAP_WIDTH, SCRAP_HEIGHT, false, true);
	scrap_dirty = false;
}

//=============================================================================

/*
================
COM_LoadTempFile
================
*/
static byte *COM_LoadTempFile(char *path)
{
	return COM_LoadFile(path, LOADFILE_TEMPHUNK);
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture(qpic_t *pic, char *name)
{
	// the palette follows the pixels and a color count
	return GL_LoadTexture(name, pic->width, pic->height, pic->data, false, true, &pic->data[pic->width * pic->height + 2]);
}

/*
================
Draw_PicFromWad
================
*/
qpic_t *Draw_PicFromWad(char *name)
{
	qpic_t	*p;
	glpic_t	*gl;
	int		texnum;
	int		x, y;
	int		i;

	p = W_GetLumpName(name);
	gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		texnum = Scrap_AllocBlock(p->width, p->height, &x, &y);
		if (texnum != -1)
		{
			scrap_dirty = true;
			for (i = 0; i < p->height; i++)
				memcpy(&scrap_texels[(y + i) * SCRAP_WIDTH + x], &p->data[i * p->width], p->width);
			gl->texnum = scrap_texnum + texnum;
			gl->sl = (x + 0.01f) / (float)SCRAP_WIDTH;
			gl->sh = (x + p->width - 0.01f) / (float)SCRAP_WIDTH;
			gl->tl = (y + 0.01f) / (float)SCRAP_HEIGHT;
			gl->th = (y + p->height - 0.01f) / (float)SCRAP_HEIGHT;

			pic_count++;
			pic_texels += p->width * p->height;
			return p;
		}
	}

	gl->texnum = GL_LoadPicTexture(p, name);
	gl->sl = 0;
	gl->tl = 0;
	gl->sh = 1;
	gl->th = 1;
	return p;
}

/*
================
Draw_PicFromWad_NoScrap

Uploads a wad pic as its own texture, with the palette gamma corrected in place.
================
*/
qpic_t *Draw_PicFromWad_NoScrap(char *name)
{
	qpic_t		*p;
	glpic_t		*gl;
	unsigned	*trans;
	byte		*pal;
	int			c;
	char		identifier[64];
	char		*id;

	id = name;

	p = W_GetLumpName(name);
	if (!p)
		return NULL;

	gl = (glpic_t *)p->data;

	// see if the texture is already present
	if (name[0])
	{
		Q_strncpy(identifier, name, sizeof(identifier) - 1);
		identifier[sizeof(identifier) - 1] = 0;
		id = identifier;
restart:
		for (int i = 0; i < numgltextures; i++)
		{
			if (!strcmp(id, gltextures[i].identifier))
			{
				if (gltextures[i].width == p->width && gltextures[i].height == p->height)
					return p;

				// same name, different size: change the name and search again
				if (++id[3])
					goto restart;
				break;
			}
		}
	}

	if (numgltextures + 1 >= MAX_GLTEXTURES)
		Sys_Error("Texture Overflow: MAX_GLTEXTURES");

	gltextures[numgltextures].texnum = texture_extension_number;
	strcpy(gltextures[numgltextures].identifier, id);
	gltextures[numgltextures].width = p->width;
	gltextures[numgltextures].height = p->height;
	gltextures[numgltextures].mipmap = false;
	numgltextures++;

	GL_Bind(texture_extension_number);

	c = p->width * p->height;
	trans = malloc(c * sizeof(unsigned));
	if (!trans)
		Sys_Error("Draw_PicFromWad_NoScrap: not enough memory");

	// the palette follows the pixels and a color count
	pal = &p->data[c + 2];
	for (int i = 0; i < 256 * 3; i++)
		pal[i] = gammatable[pal[i]];

	for (int i = 0; i < c; i++)
	{
		byte		idx;
		unsigned	color;

		idx = p->data[i];
		color = pal[idx * 3] | (pal[idx * 3 + 1] << 8) | (pal[idx * 3 + 2] << 16);
		if (idx != 255)
			color |= 0xff000000;	// 255 is transparent
		trans[i] = color;
	}

	GL_Upload32(trans, p->width, p->height, false, true);

	gl->texnum = texture_extension_number++;
	gl->sl = 0;
	gl->tl = 0;
	gl->sh = 1;
	gl->th = 1;

	free(trans);

	return p;
}

/*
================
Draw_CachePic
================
*/
qpic_t *Draw_CachePic(char *path)
{
	cachepic_t	*pic;
	qpic_t		*dat;
	glpic_t		*gl;
	qboolean	oldskip;

	for (int i = 0; i < menu_numcachepics; i++)
	{
		if (!strcmp(path, menu_cachepics[i].name))
			return &menu_cachepics[i].pic;
	}

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error("menu_numcachepics == MAX_CACHED_PICS");
	pic = &menu_cachepics[menu_numcachepics++];
	strcpy(pic->name, path);

	//
	// load the pic from disk
	//
	dat = (qpic_t *)COM_LoadTempFile(path);
	if (!dat)
	{
		Con_Printf("Draw_CachePic: failed to load %s", path);
		return NULL;
	}
	SwapPic((miptex_t *)dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp(path, "gfx/menuplyr.lmp"))
		memcpy(menuplyr_pixels, dat->data, dat->width * dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	oldskip = skip_gammatable;
	skip_gammatable = true;
	gl->texnum = GL_LoadPicTexture(dat, path);
	skip_gammatable = oldskip;
	gl->sl = 0;
	gl->tl = 0;
	gl->sh = 1;
	gl->th = 1;

	return &pic->pic;
}

/*
================
Draw_StringWidth
================
*/
int Draw_StringWidth(const char *str)
{
	int		width;

	if (!str || !*str || !draw_chars)
		return 0;

	width = 0;
	while (*str)
	{
		width += draw_chars->fontinfo[(byte)*str].charwidth;
		str++;
	}

	return width;
}

/*
================
Draw_Character

Draws one proportional character and returns its width.
================
*/
int Draw_Character(int x, int y, unsigned char num)
{
	int		rowheight, rowcount;
	int		width, offset;
	int		row;
	float	frow;
	float	sl, sh, tl, th;

	if (!draw_chars)
		return 0;

	rowheight = draw_chars->rowHeight;
	if (y <= -rowheight)
		return 0;	// totally off screen

	width = draw_chars->fontinfo[num].charwidth;
	if (y < 0)
		return width;

	offset = draw_chars->fontinfo[num].startoffset;
	rowcount = draw_chars->rowCount;

	frow = (rowcount > 0) ? 1.0f / rowcount : 0;

	// the low byte of the offset is the column, the high byte the row
	sl = (offset & 0xff) * (1.0f / CONCHARS_WIDTH);
	sh = sl + width * (1.0f / CONCHARS_WIDTH);

	row = ((offset & 0xff00) >> 8) / rowheight;
	tl = row * frow;
	th = tl + frow;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	GL_Bind(char_texture);

	glBegin(GL_QUADS);
	glTexCoord2f(sl, tl);
	glVertex2f(x, y);
	glTexCoord2f(sh, tl);
	glVertex2f(x + width, y);
	glTexCoord2f(sh, th);
	glVertex2f(x + width, y + rowheight);
	glTexCoord2f(sl, th);
	glVertex2f(x, y + rowheight);
	glEnd();

	return width;
}

/*
================
Draw_String

Returns the x position after the string.
================
*/
int Draw_String(int x, int y, unsigned char *str)
{
	if (!str)
		return x;

	while (*str)
	{
		x += Draw_Character(x, y, *str);
		str++;
	}

	return x;
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic(int x, int y, qpic_t *pic)
{
	glpic_t	*gl;

	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload();

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);

	gl = (glpic_t *)pic->data;
	GL_Bind(gl->texnum);
	glBegin(GL_QUADS);
	glTexCoord2f(gl->sl, gl->tl);
	glVertex2f(x, y);
	glTexCoord2f(gl->sh, gl->tl);
	glVertex2f(x + pic->width, y);
	glTexCoord2f(gl->sh, gl->th);
	glVertex2f(x + pic->width, y + pic->height);
	glTexCoord2f(gl->sl, gl->th);
	glVertex2f(x, y + pic->height);
	glEnd();
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic(int x, int y, int w, int h, qpic_t *pic)
{
	glpic_t	*gl;

	if (!pic)
		return;

	if (scrap_dirty)
		Scrap_Upload();

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	gl = (glpic_t *)pic->data;
	GL_Bind(gl->texnum);
	glBegin(GL_QUADS);
	glTexCoord2f(gl->sl, gl->tl);
	glVertex2f(x, y);
	glTexCoord2f(gl->sh, gl->tl);
	glVertex2f(x + w, y);
	glTexCoord2f(gl->sh, gl->th);
	glVertex2f(x + w, y + h - 1);
	glTexCoord2f(gl->sl, gl->th);
	glVertex2f(x, y + h - 1);
	glEnd();
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic(int x, int y, qpic_t *pic)
{
	if (!pic)
		return;

	if (x < 0 || x + pic->width > (int)vid.width || y < 0 || y + pic->height > (int)vid.height)
		Sys_Error("Draw_TransPic: bad coordinates");

	Draw_Pic(x, y, pic);
}

/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate(int x, int y, qpic_t *pic)
{
	static unsigned	trans[64 * 64];
	int				width, height;
	byte			p;

	if (!pic)
		return;

	width = pic->width;
	height = pic->height;

	for (int v = 0; v < 64; v++)
	{
		for (int u = 0; u < 64; u++)
		{
			// no translation table, opaque pixels are drawn red
			p = menuplyr_pixels[((v * height) >> 6) * width + ((u * width) >> 6)];
			trans[v * 64 + u] = (p == 255) ? 255 : 0xff0000ff;
		}
	}

	GL_Bind(translate_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glColor3f(1, 1, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(x, y);
	glTexCoord2f(1, 0);
	glVertex2f(x + pic->width, y);
	glTexCoord2f(1, 1);
	glVertex2f(x + pic->width, y + pic->height);
	glTexCoord2f(0, 1);
	glVertex2f(x, y + pic->height);
	glEnd();
}

/*
================
Draw_ConsoleBackground
================
*/
void Draw_ConsoleBackground(int lines)
{
	char	ver[100];

	Draw_Pic(0, lines - VIRTUAL_HEIGHT, conback);

	sprintf(ver, "Half-Life Alpha v%5.2f", (float)VERSION);
	Draw_String(vid.conwidth - Draw_StringWidth(ver), 0, (unsigned char *)ver);
}

/*
=============
Draw_FillRGB

Fills a box of pixels with a single color
=============
*/
void Draw_FillRGB(int x, int y, int w, int h, int r, int g, int b)
{
	glDisable(GL_TEXTURE_2D);
	glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);

	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + w, y);
	glVertex2f(x + w, y + h);
	glVertex2f(x, y + h);
	glEnd();

	glColor3f(1, 1, 1);
	glEnable(GL_TEXTURE_2D);
}

/*
=============
Draw_TileClear
=============
*/
void Draw_TileClear(int x, int y, int w, int h)
{
	Draw_FillRGB(x, y, w, h, 0, 0, 0);
}

/*
=============
Draw_Fill

Fills a box of pixels with a palette color
=============
*/
void Draw_Fill(int x, int y, int w, int h, int c)
{
	byte	*pal;

	pal = host_basepal;
	c &= 255;
	Draw_FillRGB(x, y, w, h, pal[c * 3], pal[c * 3 + 1], pal[c * 3 + 2]);
}

//=============================================================================

/*
================
Draw_FadeScreen
================
*/
void Draw_FadeScreen(void)
{
	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glColor4f(0, 0, 0, 0.8f);
	glBegin(GL_QUADS);

	glVertex2f(0, 0);
	glVertex2f(VIRTUAL_WIDTH, 0);
	glVertex2f(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
	glVertex2f(0, VIRTUAL_HEIGHT);

	glEnd();
	glColor4f(1, 1, 1, 1);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	VID_HandlePause();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc(void)
{
	if (!draw_disc)
		return;

	glPushMatrix();

	// centered on a 640x480 screen
	glViewport(glx, gly, glwidth, glheight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 640, 480, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glColor4f(1, 1, 1, 1);

	glDrawBuffer(GL_FRONT);
	Draw_Pic(320 - draw_disc->width / 2, 240 - draw_disc->height / 2, draw_disc);
	glDrawBuffer(GL_BACK);

	glPopMatrix();
}

/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc(void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D(void)
{
	glViewport(glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	glColor4f(1, 1, 1, 1);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture(char *identifier)
{
	int		i;

	for (i = 0; i < numgltextures; i++)
	{
		if (!strcmp(identifier, gltextures[i].identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
static void GL_ResampleTexture(byte *in, int inwidth, int inheight, byte *out, int outwidth, int outheight)
{
	unsigned	p1[1024], p2[1024];
	unsigned	frac, fracstep, pos;
	int			i, j;
	byte		*inrow, *inrow2;
	byte		*pix1, *pix2, *pix3, *pix4;

	fracstep = ((unsigned)inwidth << 16) / (unsigned)outwidth;

	frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++)
	{
		pos = frac;
		frac += fracstep;
		p1[i] = 4 * (pos >> 16);
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++)
	{
		pos = frac;
		frac += fracstep;
		p2[i] = 4 * (pos >> 16);
	}

	for (i = 0; i < outheight; i++)
	{
		inrow = in + 4 * inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + 4 * inwidth * (int)((i + 0.75) * inheight / outheight);
		for (j = 0; j < outwidth; j++)
		{
			pix1 = inrow + p1[j];
			pix2 = inrow + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			*out++ = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			*out++ = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			*out++ = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			*out++ = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
static void GL_MipMap(byte *in, int width, int height)
{
	int		row;
	byte	*out;

	row = width * 4;
	out = in;
	for (int i = 0; i < (height >> 1); i++)
	{
		byte	*inrow = in + (i * 2) * row;
		byte	*inrow2 = inrow + row;

		for (int j = 0; j < width; j += 2, out += 4)
		{
			byte	*a = inrow + j * 4;
			byte	*b = a + 4;
			byte	*c = inrow2 + j * 4;
			byte	*d = c + 4;

			out[0] = (a[0] + b[0] + c[0] + d[0]) >> 2;
			out[1] = (a[1] + b[1] + c[1] + d[1]) >> 2;
			out[2] = (a[2] + b[2] + c[2] + d[2]) >> 2;
			out[3] = (a[3] + b[3] + c[3] + d[3]) >> 2;
		}
	}
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32(unsigned *data, int width, int height, qboolean mipmap, qboolean alpha)
{
	int		samples;
	int		scaled_width, scaled_height;
	int		upload_width, upload_height;
	int		rounddown, picmip;
	int		miplevel;

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
		;
	if (gl_round_down.value > 0 && width < scaled_width)
	{
		rounddown = gl_round_down.value;
		if (gl_round_down.value == 1 || (scaled_width >> rounddown) < scaled_width - width)
			scaled_width >>= 1;
	}

	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
		;
	if (gl_round_down.value > 0 && height < scaled_height)
	{
		rounddown = gl_round_down.value;
		if (gl_round_down.value == 1 || (scaled_height >> rounddown) < scaled_height - height)
			scaled_height >>= 1;
	}

	picmip = gl_picmip.value;
	upload_width = scaled_width >> picmip;
	upload_height = scaled_height >> picmip;

	if (upload_width < 1)
		upload_width = 1;
	if (upload_height < 1)
		upload_height = 1;

	if (upload_width > gl_max_size.value)
		upload_width = gl_max_size.value;
	if (upload_height > gl_max_size.value)
		upload_height = gl_max_size.value;

	if (upload_width < 1)
		upload_width = 1;
	if (upload_height < 1)
		upload_height = 1;

	if ((unsigned)(upload_width * upload_height) > MAX_UPLOAD_PIXELS)
		Sys_Error("GL_LoadTexture: too big");

	samples = alpha ? gl_alpha_format : gl_solid_format;

	if (width == upload_width && height == upload_height)
	{
		if (!mipmap)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, samples, upload_width, upload_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy(upload_scaled, data, width * height * 4);
	}
	else
		GL_ResampleTexture((byte *)data, width, height, upload_scaled, upload_width, upload_height);

	texels += upload_width * upload_height;
	glTexImage2D(GL_TEXTURE_2D, 0, samples, upload_width, upload_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload_scaled);
	if (mipmap)
	{
		miplevel = 0;
		while (upload_width > 1 || upload_height > 1)
		{
			GL_MipMap(upload_scaled, upload_width, upload_height);
			upload_width >>= 1;
			upload_height >>= 1;
			if (upload_width < 1)
				upload_width = 1;
			if (upload_height < 1)
				upload_height = 1;
			miplevel++;
			texels += upload_width * upload_height;
			glTexImage2D(GL_TEXTURE_2D, miplevel, samples, upload_width, upload_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload_scaled);
		}
	}
done:;

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? gl_filter_min : gl_filter_max);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean alpha, byte *palette)
{
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width * height;
	if ((unsigned)s > MAX_UPLOAD_PIXELS)
		Sys_Error("GL_Upload8: too big");

	if (!palette)
		return;

	if (!skip_gammatable)
	{
		for (i = 0; i < 256 * 3; i++)
			palette[i] = gammatable[palette[i]];
	}

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i = 0; i < s; i++)
		{
			p = data[i];
			if (alpha == TEX_ALPHA_DECAL)
			{
				// the index is the alpha, the color is the last palette entry
				upload_trans[i] = palette[255 * 3] | (palette[255 * 3 + 1] << 8) | (palette[255 * 3 + 2] << 16) | (p << 24);
				noalpha = false;
			}
			else if (p == 255)
			{
				upload_trans[i] = 0;
				noalpha = false;
			}
			else
				upload_trans[i] = palette[p * 3] | (palette[p * 3 + 1] << 8) | (palette[p * 3 + 2] << 16) | 0xff000000;
		}

		if (noalpha)
			alpha = false;
	}
	else
	{
		if (s & 3)
			Sys_Error("GL_Upload8: pixelCount & 3");
		for (i = 0; i < s; i++)
		{
			p = data[i] * 3;
			upload_trans[i] = palette[p] | (palette[p + 1] << 8) | (palette[p + 2] << 16) | 0xff000000;
		}
	}

	GL_Upload32(upload_trans, width, height, mipmap, alpha);
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mipmap, int alpha, byte *palette)
{
	gltexture_t	*glt;
	char		name[64];
	char		*id;

	id = identifier;

	// see if the texture is already present
	if (identifier[0])
	{
		// work on a copy, the name may be changed below
		Q_strncpy(name, identifier, sizeof(name) - 1);
		name[sizeof(name) - 1] = 0;
		id = name;
restart:
		for (int i = 0; i < numgltextures; i++)
		{
			if (!strcmp(id, gltextures[i].identifier))
			{
				if (gltextures[i].width == width && gltextures[i].height == height)
					return gltextures[i].texnum;

				// same name, different size: change the name and search again
				if (++id[3])
					goto restart;
				break;
			}
		}
	}

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

/*
=============================================================================

  cached wad files (decals.wad)

=============================================================================
*/

/*
================
Draw_MiptexTexture

Turns a miptex lump loaded behind cacheExtra bytes into a texture_t and
uploads it. Decals with a non-blue last palette color use the index as alpha.
================
*/
int Draw_MiptexTexture(cachewad_t *wad, void *buffer)
{
	texture_t	*tex;
	miptex_t	mt;
	int			i;
	int			width, height, size;
	byte		*pixels, *pal;

	if (wad->cacheExtra != sizeof(texture_t) - sizeof(miptex_t))
		Sys_Error("Draw_MiptexTexture: Bad cached wad %s\n", wad->name);

	tex = (texture_t *)buffer;
	memcpy(&mt, (byte *)buffer + wad->cacheExtra, sizeof(mt));
	memcpy(tex->name, mt.name, sizeof(tex->name));
	tex->width = LittleLong(mt.width);
	tex->height = LittleLong(mt.height);
	tex->anim_max = 0;
	tex->anim_min = 0;
	tex->anim_total = 0;
	tex->alternate_anims = NULL;
	tex->anim_next = NULL;

	for (i = 0; i < MIPLEVELS; i++)
		tex->offsets[i] = LittleLong(mt.offsets[i]) + wad->cacheExtra;

	width = tex->width;
	height = tex->height;
	size = width * height;

	// the palette follows the four mip levels and a color count
	pixels = (byte *)tex + (int)tex->offsets[0];
	pal = pixels + size + (size >> 2) + (size >> 4) + (size >> 6) + 2;

	if (pal[255 * 3] || pal[255 * 3 + 1] || pal[255 * 3 + 2] != 255)
	{
		tex->name[0] = '}';
		i = GL_LoadTexture(tex->name, width, height, pixels, true, TEX_ALPHA_DECAL, pal);
	}
	else
	{
		tex->name[0] = '{';
		i = GL_LoadTexture(tex->name, width, height, pixels, true, true, pal);
	}

	tex->gl_texturenum = i;
	return i;
}

/*
================
Draw_LoadWad

Reads the lump directory of a wad file; the lumps are loaded on demand.
================
*/
void Draw_LoadWad(const char *name, int cacheMax, cachewad_t *wad)
{
	int			i;
	int			handle;
	int			filelen;
	int			len;
	wadinfo_t	header;

	filelen = COM_OpenFile(name, &handle);
	if (handle == -1)
		Sys_Error("Draw_LoadWad: Couldn't open %s\n", name);

	Sys_FileRead(handle, &header, sizeof(header));
	if (memcmp(header.identification, "WAD3", 4))
		Sys_Error("Wad file %s doesn't have WAD3 id\n", name);

	len = filelen - header.infotableofs;
	wad->lumps = malloc(len);
	Sys_FileSeek(handle, header.infotableofs);
	Sys_FileRead(handle, wad->lumps, len);
	COM_CloseFile(handle);

	for (i = 0; i < header.numlumps; i++)
		W_CleanupName(wad->lumps[i].name, wad->lumps[i].name);

	wad->lumpCount = header.numlumps;
	wad->cacheCount = 0;
	wad->cacheMax = cacheMax;
	wad->name = name;
	wad->cache = malloc(cacheMax * sizeof(cacheentry_t));
	memset(wad->cache, 0, cacheMax * sizeof(cacheentry_t));
	wad->cacheExtra = 0;
	wad->pfnCacheBuild = NULL;
}

/*
================
Draw_SetWadCallback
================
*/
void Draw_SetWadCallback(cachewad_t *wad, int (*pfnCacheBuild)(cachewad_t *wad, void *buffer), int cacheExtra)
{
	wad->pfnCacheBuild = pfnCacheBuild;
	wad->cacheExtra = cacheExtra;
}

/*
================
Draw_NameToDecal
================
*/
char *Draw_NameToDecal(int decal, char *name)
{
	if (decal >= MAX_DECAL_NAMES)
		return NULL;

	strncpy(decal_names[decal], name, sizeof(decal_names[0]) - 1);
	decal_names[decal][sizeof(decal_names[0]) - 1] = 0;
	return decal_names[decal];
}

/*
================
Draw_DecalIndex
================
*/
int Draw_DecalIndex(int decal)
{
	if (!decal_names[decal][0])
		Sys_Error("Used decal #%d without no name\n", decal);

	return Draw_CacheIndex(&decal_wad, decal_names[decal]);
}

/*
================
Draw_CacheIndex

Returns the cache slot for name, adding it if needed
================
*/
int Draw_CacheIndex(cachewad_t *wad, const char *name)
{
	int				i;
	cacheentry_t	*pic;

	for (pic = wad->cache, i = 0; i < wad->cacheCount; pic++, i++)
	{
		if (!strcmp(name, pic->name))
			return i;
	}

	if (wad->cacheCount == wad->cacheMax)
		Sys_Error("Cache wad (%s) out of %d entries", wad->name, wad->cacheMax);

	wad->cacheCount++;
	strcpy(pic->name, name);
	return i;
}

/*
================
Draw_CacheGet

Returns the cached lump data, loading it from the wad file if it was flushed
================
*/
void *Draw_CacheGet(cachewad_t *wad, int index)
{
	cacheentry_t	*pic;
	void			*dat;
	lumpinfo_t		*pLump;
	byte			*buf;
	char			name[16];
	char			clean[16];

	if (wad->cacheCount <= index)
		Sys_Error("Cache wad indexed before load %s: %d", wad->name, index);

	pic = &wad->cache[index];
	dat = Cache_Check(&pic->cache);
	if (dat)
		return dat;

	// not cached, load it from the wad file
	COM_FileBase(pic->name, name);
	W_CleanupName(name, clean);

	pLump = NULL;
	for (int i = 0; i < wad->lumpCount; i++)
	{
		if (!strcmp(clean, wad->lumps[i].name))
		{
			pLump = &wad->lumps[i];
			break;
		}
	}

	if (!pLump)
		Sys_Error("Draw_CacheGet: couldn't find %s in %s", pic->name, wad->name);

	int		handle;

	COM_OpenFile(wad->name, &handle);
	if (handle == -1)
		return NULL;

	buf = Cache_Alloc(&pic->cache, pLump->disksize + wad->cacheExtra + 1, clean);
	if (!buf)
		Sys_Error("Draw_CacheGet: not enough space for %s in %s", pic->name, wad->name);

	buf[wad->cacheExtra + pLump->disksize] = 0;
	Sys_FileSeek(handle, pLump->filepos);
	Sys_FileRead(handle, buf + wad->cacheExtra, pLump->disksize);
	COM_CloseFile(handle);

	if (wad->pfnCacheBuild)
		wad->pfnCacheBuild(wad, buf);

	if (!pic->cache.data)
		Sys_Error("Draw_CacheGet: failed to load %s", pic->name);

	return pic->cache.data;
}

/*
================
Draw_GetDecal
================
*/
void *Draw_GetDecal(int index)
{
	return Draw_CacheGet(&decal_wad, index);
}
