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

byte *targa_rgba;
unsigned short targa_width;
unsigned short targa_height;

static unsigned short TGA_ReadLittleShort(FILE *stream)
{
	const unsigned short lo = (unsigned char)fgetc(stream);
	const unsigned short hi = (unsigned char)fgetc(stream);
	return (unsigned short)(lo | (hi << 8));
}

void LoadTGA(FILE *stream)
{
	unsigned char id_length;
	unsigned char colormap_type;
	unsigned char image_type;
	unsigned short colormap_index;
	unsigned short colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin;
	unsigned short y_origin;
	unsigned char pixel_depth;
	unsigned char image_desc;

	int x;
	int y;

	id_length = (unsigned char)fgetc(stream);
	colormap_type = (unsigned char)fgetc(stream);
	image_type = (unsigned char)fgetc(stream);
	colormap_index = TGA_ReadLittleShort(stream);
	colormap_length = TGA_ReadLittleShort(stream);
	colormap_size = (unsigned char)fgetc(stream);
	x_origin = TGA_ReadLittleShort(stream);
	y_origin = TGA_ReadLittleShort(stream);
	targa_width = TGA_ReadLittleShort(stream);
	targa_height = TGA_ReadLittleShort(stream);
	pixel_depth = (unsigned char)fgetc(stream);
	image_desc = (unsigned char)fgetc(stream);

	(void)colormap_index;
	(void)colormap_length;
	(void)colormap_size;
	(void)x_origin;
	(void)y_origin;
	(void)image_desc;

	if (image_type != 2 && image_type != 10)
		Sys_Error("LoadTGA: Only type 2 and 10 supported\n");

	if (colormap_type != 0 || (pixel_depth != 24 && pixel_depth != 32))
		Sys_Error("Texture_LoadTGA: Only 24 or 32 bit images supported (no colormaps)\n");

	targa_rgba = (byte *)malloc(4 * (int)targa_width * (int)targa_height);

	if (id_length)
		fseek(stream, id_length, SEEK_CUR);

	if (image_type == 2)
	{
		for (y = (int)targa_height - 1; y >= 0; --y)
		{
			byte *dst = targa_rgba + 4 * (int)targa_width * y;
			for (x = 0; x < (int)targa_width; ++x)
			{
				const unsigned char b = (unsigned char)getc(stream);
				const unsigned char g = (unsigned char)getc(stream);
				const unsigned char r = (unsigned char)getc(stream);
				const unsigned char a = (pixel_depth == 32) ? (unsigned char)getc(stream) : 0xFF;

				*dst++ = r;
				*dst++ = g;
				*dst++ = b;
				*dst++ = a;
			}
		}

		fclose(stream);
		return;
	}

	for (y = (int)targa_height - 1; y >= 0; --y)
	{
		int cur_x = 0;
		byte *dst = targa_rgba + 4 * (int)targa_width * y;

		while (cur_x < (int)targa_width)
		{
			const int packet_header = getc(stream);
			const int count = (packet_header & 0x7F) + 1;
			int i;

			if (packet_header & 0x80)
			{
				const unsigned char b = (unsigned char)getc(stream);
				const unsigned char g = (unsigned char)getc(stream);
				const unsigned char r = (unsigned char)getc(stream);
				const unsigned char a = (pixel_depth == 32) ? (unsigned char)getc(stream) : 0xFF;

				for (i = 0; i < count; ++i)
				{
					*dst++ = r;
					*dst++ = g;
					*dst++ = b;
					*dst++ = a;

					if (++cur_x == (int)targa_width && y > 0)
					{
						--y;
						cur_x = 0;
						dst = targa_rgba + 4 * (int)targa_width * y;
					}
				}
			}
			else
			{
				for (i = 0; i < count; ++i)
				{
					const unsigned char b = (unsigned char)getc(stream);
					const unsigned char g = (unsigned char)getc(stream);
					const unsigned char r = (unsigned char)getc(stream);
					const unsigned char a = (pixel_depth == 32) ? (unsigned char)getc(stream) : 0xFF;

					*dst++ = r;
					*dst++ = g;
					*dst++ = b;
					*dst++ = a;

					if (++cur_x == (int)targa_width)
						break;
				}
			}
		}
	}

	fclose(stream);
}
