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

 // glquake.h -- OpenGL definitions for Half-Life Alpha 0.52
 // NOTE: This is a skeleton header derived from reverse engineering.
 // OpenGL-specific types and declarations will be filled during translation.

#ifndef GLQUAKE_H
#define GLQUAKE_H

 // Some OpenGL headers on Windows can be missing newer constants when building
 // against older SDKs.
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#endif

 // =============================================================================
 // Disable MSVC data conversion warnings for OpenGL
 // =============================================================================

#pragma warning(disable : 4244) // float/ double conversion
#pragma warning(disable : 4136) // X86
#pragma warning(disable : 4051) // ALPHA

 // =============================================================================
 // Windows and OpenGL includes
 // =============================================================================

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

 // =============================================================================
 // OpenGL renderer interface
 // =============================================================================

 // NOTE: To be filled from reverse engineering

 // Rendering functions
void GL_BeginRendering(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);

 // Video system (gl_vidnt.c)
int VID_Init(void);
BOOL VID_WriteBuffer(const char *filename);
void VID_Shutdown(void);
BOOL VID_TakeSnapshot(const char *filename);

 // Sky polygon clipping (gl_warp.c)
int SetupSkyPolygonClipping(int param1, int param2, int param3);
void EmitWaterPolys(msurface_t *fa, int direction);
void EmitSkyPolys(msurface_t *fa);
void R_DrawSkyChain(msurface_t *surface);
void R_DrawSkyBox(void);
int InitSkyPolygonBounds(void);
void GL_SubdivideSurface(msurface_t *fa);

 // =============================================================================
 // OpenGL extension function pointers (Windows)
 // =============================================================================

#ifdef _WIN32

typedef void (APIENTRY *BINDTEXFUNCPTR)(GLenum target, GLuint texture);
typedef void (APIENTRY *DELTEXFUNCPTR)(GLsizei n, const GLuint *textures);
typedef void (APIENTRY *TEXSUBIMAGEPTR)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *ARRAYELEMENTPTR)(GLint i);
typedef void (APIENTRY *COLORPOINTERPTR)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY *TEXTUREPOINTERPTR)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY *VERTEXPOINTERPTR)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

extern BINDTEXFUNCPTR bindTexFunc;
extern DELTEXFUNCPTR delTexFunc;
extern TEXSUBIMAGEPTR TexSubImage2DFunc;
extern ARRAYELEMENTPTR ArrayElementFunc;
extern COLORPOINTERPTR ColorPointerFunc;
extern TEXTUREPOINTERPTR TexturePointerFunc;
extern VERTEXPOINTERPTR VertexPointerFunc;

#endif // _WIN32

 // =============================================================================
 // OpenGL state and globals
 // =============================================================================

extern int texture_extension_number;
extern int texture_mode;

extern float gldepthmin, gldepthmax;

 // Sky texture ids (defined in gl_warp.c)
extern int solidskytexture;
extern int alphaskytexture;

 // Render modes
#define GL_RGB_FORMAT 3
#define GL_RGBA_FORMAT 4

extern int gl_lightmap_format;
extern int gl_solid_format;
extern int gl_alpha_format;

extern int gl_filter_min, gl_filter_max;

 // =============================================================================
 // Texture management
 // =============================================================================

#define MAX_GLTEXTURES  1024

typedef struct
{
    int     texnum;
    char    identifier[64];
    int     width;
    int     height;
    int     mipmap;
} gltexture_t;

extern gltexture_t gltextures[MAX_GLTEXTURES];
extern int numgltextures;

void GL_Upload32(unsigned *data, int width, int height, qboolean mipmap, qboolean alpha);
void GL_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean alpha, byte *palette);
int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mode, int alpha, byte *palette);
int GL_FindTexture(char *identifier);
void GL_Bind(int texnum);
void GL_SelectTexture(int unit); // For multitexture
void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *hdr);

 // =============================================================================
 // Video definition structure
 // =============================================================================

typedef unsigned char pixel_t;

typedef struct viddef_s
{
	pixel_t     *buffer; // invisible buffer
	pixel_t     *colormap; // 256 * VID_GRADES size
	unsigned short *colormap16; // 256 * VID_GRADES size
	int         fullbright; // index of first fullbright color
	int         bits;
	int         is15bit;
	unsigned    rowbytes; // may be > width if displayed in a window
	unsigned    width;
	unsigned    height;
	float       aspect; // width/ height -- < 0 is taller than wide
	int         numpages;
	int         recalc_refdef; // if true, recalc vid-based stuff
	pixel_t     *conbuffer;
	int         conrowbytes;
	unsigned    conwidth;
	unsigned    conheight;
	int         maxwarpwidth;
	int         maxwarpheight;
	pixel_t     *direct; // direct drawing (software)
} viddef_t;

extern viddef_t vid;

 // =============================================================================
 // OpenGL vertex structure
 // =============================================================================

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

 // =============================================================================
 // OpenGL viewport globals
 // =============================================================================

extern int glx, gly, glwidth, glheight;

 // =============================================================================
 // OpenGL renderer constants
 // =============================================================================

#define ALIAS_BASE_SIZE_RATIO		1.0
#define MAX_LBM_HEIGHT				480
#define TILE_SIZE					128
#define SKYSHIFT					7
#define SKYSIZE						(1 << SKYSHIFT)
#define SKYMASK						(SKYSIZE - 1)
#define BACKFACE_EPSILON			0.01

 // =============================================================================
 // Particle system types
 // =============================================================================

 // Half-Life extended particle types (base ptype_t defined in render.h)
 // These extend the base Quake particle types with HL-specific ones
#define pt_vox_grav      ((ptype_t)8)
#define pt_vox_slowgrav  ((ptype_t)9)
#define pt_blur          ((ptype_t)10)
#define pt_clientcustom  ((ptype_t)11)

 // =============================================================================
 // OpenGL renderer cvars
 // =============================================================================

extern cvar_t r_norefresh;
extern cvar_t r_drawentities;
extern cvar_t r_drawviewmodel;
extern cvar_t r_drawworld;
extern cvar_t r_speeds;
extern cvar_t r_timegraph;
extern cvar_t r_fullbright;
extern cvar_t r_lightmap;
extern cvar_t r_shadows;
extern cvar_t r_drawflat;
extern cvar_t r_flowmap;
extern cvar_t r_mirroralpha;
extern cvar_t r_wateralpha;
extern cvar_t r_dynamic;
extern cvar_t r_novis;
extern cvar_t r_fastturb;
extern cvar_t r_decals;

extern cvar_t r_part_explode;
extern cvar_t r_part_trails;
extern cvar_t r_part_sparks;
extern cvar_t r_part_gunshots;
extern cvar_t r_part_blood;
extern cvar_t r_part_telesplash;

extern cvar_t gl_clear;
extern cvar_t gl_cull;
extern cvar_t gl_texsort;
extern cvar_t gl_smoothmodels;
extern cvar_t gl_affinemodels;
extern cvar_t gl_flashblend;
extern cvar_t gl_polyblend;
extern cvar_t gl_keeptjunctions;
extern cvar_t gl_max_size;
extern cvar_t gl_playermip;
extern cvar_t gl_nocolors;
extern cvar_t gl_reporttjunctions;
extern cvar_t gl_wateramp;
extern cvar_t gl_ztrick;

 // =============================================================================
 // OpenGL renderer state
 // =============================================================================

extern struct edict_s *r_worldentity;

extern int r_framecount;
extern int r_visframecount;

extern int currenttexture;
extern int particletexture;
extern int playertextures;

extern int skytexturenum;
extern int mirrortexturenum;

extern qboolean mirror;
extern mplane_t *mirror_plane;

extern float r_world_matrix[16];
extern float r_base_world_matrix[16];

 // =============================================================================
 // Vendor/ version strings
 // =============================================================================

extern const char *gl_vendor;
extern const char *gl_renderer;
extern const char *gl_version;
extern const char *gl_extensions;

 // =============================================================================
 // Multitexture support (SGIS extension)
 // =============================================================================

#define TEXTURE0_SGIS   0x835E
#define TEXTURE1_SGIS   0x835F

#ifndef _WIN32
#define APIENTRY /* */
#endif

typedef void (APIENTRY *lpMTexFUNC)(GLenum, GLfloat, GLfloat);
typedef void (APIENTRY *lpSelTexFUNC)(GLenum);

extern lpMTexFUNC qglMTexCoord2fSGIS;
extern lpSelTexFUNC qglSelectTextureSGIS;
extern qboolean gl_mtexable;

void GL_DisableMultitexture(void);
void GL_EnableMultitexture(void);

 // =============================================================================

#endif // GLQUAKE_H
