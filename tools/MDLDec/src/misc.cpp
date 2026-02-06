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
// misc.cpp: Half-Life Alpha MDL Decompiler.
//

#include "mdldec.h"

#include <stdarg.h>

void CMDLDecompiler::LogMessage(int type, const char *msg, ...)
{
    (void)type;

    va_list argptr;
    va_start(argptr, msg);
    vprintf(msg, argptr);
    va_end(argptr);
}

void MyExtractFileBase(char *path, char *dest)
{
    char *src = path + strlen(path) - 1;

    while (src != path && !PATHSEPARATOR(*(src - 1)))
        src--;

    while (*src && *src != '.')
    {
        *dest++ = *src++;
    }
    *dest = 0;
}

void MyExtractFilePath(char *path, char *dest)
{
    char *src = path + strlen(path) - 1;

    while (src != path && !PATHSEPARATOR(*(src - 1)))
        src--;

    memcpy(dest, path, src - path);
    dest[src - path] = 0;
}
