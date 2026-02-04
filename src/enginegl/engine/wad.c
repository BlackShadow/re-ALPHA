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

byte		*wad_base;
int			wad_numlumps;
lumpinfo_t	*wad_lumps;

extern int (*LittleLong)(int l);

void W_CleanupName(char *in, char *out)
{
	int		i;
	int		c;

	for (i = 0; i < 16; i++)
	{
		c = in[i];
		if (!c)
			break;

		if (c >= 'A' && c <= 'Z')
			c = c + 32;

		out[i] = c;
	}

	if (i < 16)
		memset(&out[i], 0, 16 - i);
}

lumpinfo_t *W_GetLumpinfo(const char *name, qboolean doerror)
{
	lumpinfo_t	*lump_p;
	int			i;
	char		clean[16];

	W_CleanupName((char *)name, clean);

	lump_p = wad_lumps;
	i = 0;

	if (wad_numlumps <= 0)
	{
		if (doerror)
			Sys_Error("W_GetLumpinfo: %s not found", name);
		return NULL;
	}

	while (strcmp(clean, lump_p->name))
	{
		i++;
		lump_p++;
		if (i >= wad_numlumps)
		{
			if (doerror)
				Sys_Error("W_GetLumpinfo: %s not found", name);
			return NULL;
		}
	}

	return lump_p;
}

void *W_GetLumpName(const char *name)
{
	lumpinfo_t *lump;

	lump = W_GetLumpinfo(name, true);
	return (void *)(wad_base + lump->filepos);
}

void SwapPic(miptex_t *mt)
{
	mt->width = LittleLong(mt->width);
	mt->height = LittleLong(mt->height);
}

void W_LoadWadFile(char *filename)
{
	lumpinfo_t	*lump_p;
	wadinfo_t	*header;
	int			i;
	int			infotableofs;

	wad_base = COM_LoadHunkFile(filename);
	if (!wad_base)
		Sys_Error("W_LoadWadFile: couldn't load %s", filename);

	header = (wadinfo_t *)wad_base;

	if (header->identification[0] != 'W' ||
		header->identification[1] != 'A' ||
		header->identification[2] != 'D' ||
		header->identification[3] != '3')
	{
		Sys_Error("Wad file %s doesn't have WAD3 id\n", filename);
	}

	wad_numlumps = LittleLong(header->numlumps);
	infotableofs = LittleLong(header->infotableofs);
	wad_lumps = (lumpinfo_t *)(wad_base + infotableofs);

	for (i = 0; i < wad_numlumps; i++)
	{
		lump_p = &wad_lumps[i];

		lump_p->filepos = LittleLong(lump_p->filepos);
		lump_p->size = LittleLong(lump_p->size);

		W_CleanupName(lump_p->name, lump_p->name);

		if (lump_p->type == TYP_MIPTEX)
			SwapPic((miptex_t *)(wad_base + lump_p->filepos));
	}
}
