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
// gl_texloader.c -- TGA image loading

#include "quakedef.h"

#define TGA_TYPE_RGB		2		// uncompressed true color
#define TGA_TYPE_RGB_RLE	10		// run-length encoded true color

#define TGA_RLE_PACKET		0x80	// packet header: run-length packet flag
#define TGA_PACKET_SIZE		0x7F	// packet header: pixel count - 1

typedef struct
{
	unsigned char	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

byte			*targa_rgba;
unsigned short	targa_width;
unsigned short	targa_height;

/*
=============
fgetLittleShort
=============
*/
static unsigned short fgetLittleShort(FILE *f)
{
	unsigned short	b1, b2;

	b1 = (byte)fgetc(f);
	b2 = (byte)fgetc(f);

	return (unsigned short)(b1 | (b2 << 8));
}

/*
=============
LoadTGA

Reads a 24 or 32 bit TGA into targa_rgba (RGBA, top row first) and closes the file.
=============
*/
void LoadTGA(FILE *fin)
{
	TargaHeader	header;
	int			row, column;
	byte		*pixbuf;

	header.id_length = fgetc(fin);
	header.colormap_type = fgetc(fin);
	header.image_type = fgetc(fin);

	header.colormap_index = fgetLittleShort(fin);
	header.colormap_length = fgetLittleShort(fin);
	header.colormap_size = fgetc(fin);
	header.x_origin = fgetLittleShort(fin);
	header.y_origin = fgetLittleShort(fin);
	targa_width = fgetLittleShort(fin);
	targa_height = fgetLittleShort(fin);
	header.pixel_size = fgetc(fin);
	header.attributes = fgetc(fin);

	if (header.image_type != TGA_TYPE_RGB && header.image_type != TGA_TYPE_RGB_RLE)
		Sys_Error("LoadTGA: Only type 2 and 10 supported\n");

	if (header.colormap_type != 0 || (header.pixel_size != 24 && header.pixel_size != 32))
		Sys_Error("Texture_LoadTGA: Only 24 or 32 bit images supported (no colormaps)\n");

	targa_rgba = malloc(4 * targa_width * targa_height);

	if (header.id_length)
		fseek(fin, header.id_length, SEEK_CUR);	// skip TARGA image comment

	if (header.image_type == TGA_TYPE_RGB)
	{
		for (row = targa_height - 1; row >= 0; row--)
		{
			pixbuf = targa_rgba + 4 * targa_width * row;
			for (column = 0; column < targa_width; column++)
			{
				byte	red, green, blue, alphabyte;

				blue = getc(fin);
				green = getc(fin);
				red = getc(fin);
				alphabyte = (header.pixel_size == 32) ? (byte)getc(fin) : 255;

				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alphabyte;
			}
		}

		fclose(fin);
		return;
	}

	for (row = targa_height - 1; row >= 0; row--)
	{
		column = 0;
		pixbuf = targa_rgba + 4 * targa_width * row;

		while (column < targa_width)
		{
			int		packetHeader, packetSize;
			int		j;

			packetHeader = getc(fin);
			packetSize = (packetHeader & TGA_PACKET_SIZE) + 1;

			if (packetHeader & TGA_RLE_PACKET)
			{
				// run-length packet
				byte	red, green, blue, alphabyte;

				blue = getc(fin);
				green = getc(fin);
				red = getc(fin);
				alphabyte = (header.pixel_size == 32) ? (byte)getc(fin) : 255;

				for (j = 0; j < packetSize; j++)
				{
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;

					// run spans across rows
					if (++column == targa_width && row > 0)
					{
						row--;
						column = 0;
						pixbuf = targa_rgba + 4 * targa_width * row;
					}
				}
			}
			else
			{
				// non run-length packet
				for (j = 0; j < packetSize; j++)
				{
					byte	red, green, blue, alphabyte;

					blue = getc(fin);
					green = getc(fin);
					red = getc(fin);
					alphabyte = (header.pixel_size == 32) ? (byte)getc(fin) : 255;

					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;

					if (++column == targa_width)
						break;
				}
			}
		}
	}

	fclose(fin);
}
