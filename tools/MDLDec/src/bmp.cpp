/***
*
*    Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*    This product contains software technology licensed from Id
*    Software, Inc ("Id Technology"). Id Technology (c) 1996 Id Software, Inc.
*    All Rights Reserved.
*
****/
//
// bmp.cpp: Half-Life Alpha MDL Decompiler.
//

#include "mdldec.h"

#include <windows.h>
#include <direct.h>
#include <errno.h>

void CMDLDecompiler::BMP_GenerateTextures()
{
    if (!m_ptexturehdr)
        return;

    char textureDestPath[_MAX_PATH];
    _snprintf(textureDestPath, sizeof(textureDestPath), "%smaps_8bit\\", DestPath);
    textureDestPath[sizeof(textureDestPath) - 1] = 0;

    char textureDir[_MAX_PATH];
    strcpy(textureDir, textureDestPath);
    size_t dirLen = strlen(textureDir);
    if (dirLen > 0 && PATHSEPARATOR(textureDir[dirLen - 1]))
        textureDir[dirLen - 1] = 0;

    if (_mkdir(textureDir) == -1 && errno != EEXIST && errno != 17)
    {
        LogMessage(MDLDEC_MSG_ERROR, "ERROR: Couldn't create %s\r\n", textureDir);
        return;
    }

    studiohdr_t *pHdr = m_ptexturehdr;
    for (int i = 0; i < pHdr->numtextures; ++i)
    {
        mstudiotexture_t *pTexture = (mstudiotexture_t *)((byte *)pHdr + pHdr->textureindex + sizeof(mstudiotexture_t) * i);
        byte *pBitData = (byte *)pHdr + pTexture->index;
        byte *pColorData = (byte *)pHdr + pTexture->index + (pTexture->width * pTexture->height);
        BMP_WriteTexture(pBitData, pColorData, pTexture->name, pTexture, textureDestPath);
    }
}

void CMDLDecompiler::BMP_WriteTexture(byte *pBitData,
                                      byte *pColorData,
                                      const char *TextureName,
                                      const mstudiotexture_t *pTexture,
                                      const char *textureDestPath)
{
    char Filename[_MAX_PATH];
    const char *ext = strrchr(TextureName, '.');
    bool hasExt = (ext != NULL && ext != TextureName);
    if (hasExt)
        _snprintf(Filename, sizeof(Filename), "%s%s", textureDestPath, TextureName);
    else
        _snprintf(Filename, sizeof(Filename), "%s%s.bmp", textureDestPath, TextureName);
    Filename[sizeof(Filename) - 1] = 0;

    FILE *fp = fopen(Filename, "wb");
    if (!fp)
    {
        LogMessage(MDLDEC_MSG_ERROR, "ERROR: Couldn't write texture file %s\r\n", Filename);
        return;
    }

    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
    RGBQUAD rgbPalette[256];

    ULONG biTrueWidth = (ULONG)pTexture->width;
    ULONG cbBmpBits = biTrueWidth * (ULONG)pTexture->height;
    ULONG cbPalBytes = 256 * sizeof(RGBQUAD);

    bmfh.bfType = MAKEWORD('B', 'M');
    bmfh.bfSize = sizeof(bmfh) + sizeof(bmih) + cbBmpBits + cbPalBytes;
    bmfh.bfReserved1 = 0;
    bmfh.bfReserved2 = 0;
    bmfh.bfOffBits = sizeof(bmfh) + sizeof(bmih) + cbPalBytes;
    fwrite(&bmfh, sizeof(bmfh), 1, fp);

    bmih.biSize = sizeof(bmih);
    bmih.biWidth = biTrueWidth;
    bmih.biHeight = pTexture->height;
    bmih.biPlanes = 1;
    bmih.biBitCount = 8;
    bmih.biCompression = BI_RGB;
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 256;
    bmih.biClrImportant = 0;
    fwrite(&bmih, sizeof(bmih), 1, fp);

    byte *pb = pColorData;
    for (int i = 0; i < (int)bmih.biClrUsed; ++i)
    {
        rgbPalette[i].rgbRed = *pb++;
        rgbPalette[i].rgbGreen = *pb++;
        rgbPalette[i].rgbBlue = *pb++;
        rgbPalette[i].rgbReserved = 0;
    }
    fwrite(rgbPalette, cbPalBytes, 1, fp);

    byte *pbBmpBits = (byte *)malloc(cbBmpBits);
    if (!pbBmpBits)
    {
        fclose(fp);
        LogMessage(MDLDEC_MSG_ERROR, "ERROR: Out of memory while writing %s\r\n", Filename);
        return;
    }

    pb = pBitData + (pTexture->height - 1) * pTexture->width;
    for (int i = 0; i < bmih.biHeight; ++i)
    {
        memmove(&pbBmpBits[biTrueWidth * i], pb, pTexture->width);
        pb -= pTexture->width;
    }

    fwrite(pbBmpBits, cbBmpBits, 1, fp);
    free(pbBmpBits);
    fclose(fp);

    LogMessage(MDLDEC_MSG_INFO, "Texture: %s\r\n", Filename);
}
