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

// glquake.h -- OpenGL renderer definitions

#ifndef GLQUAKE_H
#define GLQUAKE_H

// some OpenGL headers on Windows lack newer constants
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#endif

// disable data conversion warnings

#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4136)	// X86
#pragma warning(disable : 4051)	// ALPHA

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

void GL_BeginRendering(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);
void VID_GetWindowSize(int *x, int *y, int *width, int *height);

// gl_vidnt.c
int VID_Init(void);
BOOL VID_WriteBuffer(const char *filename);
void VID_Shutdown(void);
BOOL VID_TakeSnapshot(const char *filename);
void VID_ShiftPalette(void);

// gl_warp.c
int SetupSkyPolygonClipping(int r, int g, int b);
void EmitWaterPolys(msurface_t *fa, int direction);
void EmitSkyPolys(msurface_t *fa);
void R_DrawSkyChain(msurface_t *s);
void R_DrawSkyBox(void);
void InitSkyPolygonBounds(void);
void GL_SubdivideSurface(msurface_t *fa);
void R_LoadSkys(void);

#ifdef _WIN32

typedef void (APIENTRY *BINDTEXFUNCPTR)(GLenum target, GLuint texture);
typedef void (APIENTRY *DELTEXFUNCPTR)(GLsizei n, const GLuint *textures);
typedef void (APIENTRY *TEXSUBIMAGEPTR)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *ARRAYELEMENTPTR)(GLint i);
typedef void (APIENTRY *COLORPOINTERPTR)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY *TEXTUREPOINTERPTR)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY *VERTEXPOINTERPTR)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

extern BINDTEXFUNCPTR		bindTexFunc;
extern DELTEXFUNCPTR		delTexFunc;
extern TEXSUBIMAGEPTR		TexSubImage2DFunc;
extern ARRAYELEMENTPTR		ArrayElementFunc;
extern COLORPOINTERPTR		ColorPointerFunc;
extern TEXTUREPOINTERPTR	TexturePointerFunc;
extern VERTEXPOINTERPTR		VertexPointerFunc;

#endif // _WIN32

extern int		texture_extension_number;

extern float	gldepthmin, gldepthmax;

extern int		solidskytexture;
extern int		alphaskytexture;
extern float	speedscale;		// for top sky and bottom sky
extern byte		g_WaterColor[4];

extern float	turbsin[256];

#define GL_RGB_FORMAT	3
#define GL_RGBA_FORMAT	4

extern int		gl_lightmap_format;
extern int		gl_solid_format;
extern int		gl_alpha_format;

extern int		gl_filter_min, gl_filter_max;

// gl_texloader.c
extern byte				*targa_rgba;
extern unsigned short	targa_width, targa_height;
void LoadTGA(FILE *fin);

#define MAX_GLTEXTURES	1024

typedef struct
{
	int		texnum;
	char	identifier[64];
	int		width, height;
	int		mipmap;
} gltexture_t;

extern gltexture_t	gltextures[MAX_GLTEXTURES];
extern int			numgltextures;

// GL_LoadTexture alpha: false, true (index 255 is transparent) or
// TEX_ALPHA_DECAL (the index is the alpha, the color is palette entry 255)
#define TEX_ALPHA_DECAL	2

void GL_Upload32(unsigned *data, int width, int height, qboolean mipmap, qboolean alpha);
void GL_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean alpha, byte *palette);
int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mipmap, int alpha, byte *palette);
int GL_FindTexture(char *identifier);
void GL_Bind(int texnum);
void GL_SelectTexture(int unit);
void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *hdr);

typedef unsigned char pixel_t;

typedef struct viddef_s
{
	pixel_t			*buffer;		// invisible buffer
	pixel_t			*colormap;		// 256 * VID_GRADES size
	unsigned short	*colormap16;	// 256 * VID_GRADES size
	int				fullbright;		// index of first fullbright color
	int				bits;
	int				is15bit;
	unsigned		rowbytes;		// may be > width if displayed in a window
	unsigned		width;
	unsigned		height;
	float			aspect;			// width / height -- < 0 is taller than wide
	int				numpages;
	int				recalc_refdef;	// if true, recalc vid-based stuff
	pixel_t			*conbuffer;
	int				conrowbytes;
	unsigned		conwidth;
	unsigned		conheight;
	int				maxwarpwidth;
	int				maxwarpheight;
	pixel_t			*direct;		// direct drawing to framebuffer, if not NULL
} viddef_t;

extern viddef_t	vid;	// global video state
extern cvar_t	_windowed_mouse;

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

extern int		glx, gly, glwidth, glheight;

extern int		vid_skip_swap;	// don't draw or swap buffers

#define ALIAS_BASE_SIZE_RATIO	(1.0f / 11)
#define MAX_LBM_HEIGHT			480

#define TILE_SIZE		128

#define SKYSHIFT		7
#define SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

#define BACKFACE_EPSILON	0.01

#define BLOCK_WIDTH		128		// lightmap block size
#define BLOCK_HEIGHT	128

#define VIRTUAL_WIDTH	320		// 2D drawing is done on a 320x200 screen
#define VIRTUAL_HEIGHT	200

extern cvar_t	r_norefresh;
extern cvar_t	r_drawentities;
extern cvar_t	r_drawviewmodel;
extern cvar_t	r_drawworld;
extern cvar_t	r_speeds;
extern cvar_t	r_timegraph;
extern cvar_t	r_fullbright;
extern cvar_t	r_lightmap;
extern cvar_t	r_shadows;
extern cvar_t	r_drawflat;
extern cvar_t	r_flowmap;
extern cvar_t	r_mirroralpha;
extern cvar_t	r_wateralpha;
extern cvar_t	r_dynamic;
extern cvar_t	r_novis;
extern cvar_t	r_fastturb;
extern cvar_t	r_decals;

extern cvar_t	r_part_explode;
extern cvar_t	r_part_trails;
extern cvar_t	r_part_sparks;
extern cvar_t	r_part_gunshots;
extern cvar_t	r_part_blood;
extern cvar_t	r_part_telesplash;

extern cvar_t	gl_clear;
extern cvar_t	gl_cull;
extern cvar_t	gl_texsort;
extern cvar_t	gl_smoothmodels;
extern cvar_t	gl_affinemodels;
extern cvar_t	gl_flashblend;
extern cvar_t	gl_polyblend;
extern cvar_t	gl_keeptjunctions;
extern cvar_t	gl_max_size;
extern cvar_t	gl_playermip;
extern cvar_t	gl_nocolors;
extern cvar_t	gl_reporttjunctions;
extern cvar_t	gl_wateramp;
extern cvar_t	gl_ztrick;

extern struct edict_s	*r_worldentity;

extern entity_t	*currententity;

extern int		r_framecount;
extern int		r_visframecount;

extern int		currenttexture;
extern int		particletexture;
extern int		playertextures;

extern int		skytexturenum;
extern int		mirrortexturenum;

extern qboolean	mirror;
extern mplane_t	*mirror_plane;

extern float	r_world_matrix[16];
extern float	r_base_world_matrix[16];

extern const char	*gl_vendor;
extern const char	*gl_renderer;
extern const char	*gl_version;
extern const char	*gl_extensions;

// multitexture
#define TEXTURE0_SGIS	0x835E
#define TEXTURE1_SGIS	0x835F

#ifndef _WIN32
#define APIENTRY	/* */
#endif

typedef void (APIENTRY *lpMTexFUNC)(GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC)(GLenum);

extern lpMTexFUNC	qglMTexCoord2fSGIS;
extern lpSelTexFUNC	qglSelectTextureSGIS;
extern qboolean		gl_mtexable;

void GL_DisableMultitexture(void);
void GL_EnableMultitexture(void);

#endif // GLQUAKE_H
