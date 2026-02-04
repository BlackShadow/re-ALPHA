#ifndef WAD_H
#define WAD_H

#include "quakedef.h"

 // =========================================================
 // WAD File Format Structures
 // =========================================================

#define CMP_NONE        0
#define CMP_LZSS        1

#define TYP_NONE        0
#define TYP_LABEL       1

#define TYP_LUMPY       64 // 64 + grab command number
#define TYP_PALETTE     64
#define TYP_QTEX        65
#define TYP_QPIC        66
#define TYP_SOUND       67
#define TYP_MIPTEX      68

typedef struct
{
	int			filepos;
	int			disksize;
	int			size; // uncompressed
	char		type;
	char		compression;
	char		pad1, pad2;
	char		name[16]; // must be null terminated
} lumpinfo_t;

typedef struct
{
	char		identification[4]; // should be WAD2 or WAD3
	int			numlumps;
	int			infotableofs;
} wadinfo_t;

 // =========================================================
 // Globals
 // =========================================================

extern byte			*wad_base;
extern int			wad_numlumps;
extern lumpinfo_t	*wad_lumps;

 // =========================================================
 // Functions
 // =========================================================

void    W_LoadWadFile(char *filename);
void    W_CleanupName(char *in, char *out);
lumpinfo_t *W_GetLumpinfo(const char *name, qboolean doerror);
void    *W_GetLumpName(const char *name);
void    SwapPic(miptex_t *mt);

#endif // WAD_H
