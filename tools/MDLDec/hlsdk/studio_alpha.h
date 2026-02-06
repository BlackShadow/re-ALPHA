/*
==============================================================

    Half-Life Alpha studio model format (v6)
    Definitions derived from the Alpha engine source.

==============================================================
*/

#pragma once

#ifndef _STUDIO_ALPHA_H
#define _STUDIO_ALPHA_H

#include "mathlib.h"

typedef unsigned char byte;

#define STUDIO_VERSION_ALPHA 6
#define IDSTUDIOHEADER (('T' << 24) + ('S' << 16) + ('D' << 8) + 'I')

#define MAXSTUDIOTRIANGLES 65536
#define MAXSTUDIOVERTS 8192
#define MAXSTUDIOSEQUENCES 256
#define MAXSTUDIOSSKINS 100
#define MAXSTUDIOSRCBONES 512
#define MAXSTUDIOBONES 128
#define MAXSTUDIOMODELS 32
#define MAXSTUDIOBODYPARTS 32
#define MAXSTUDIOGROUPS 16
#define MAXSTUDIOANIMATIONS 2048
#define MAXSTUDIOMESHES 256
#define MAXSTUDIOEVENTS 1024
#define MAXSTUDIOPIVOTS 256
#define MAXSTUDIOCONTROLLERS 8

typedef struct
{
    int id;
    int version;

    char name[64];
    int length;

    int numbones;
    int boneindex;

    int numbonecontrollers;
    int bonecontrollerindex;

    int numseq;
    int seqindex;

    int numtextures;
    int textureindex;

    int texturedataindex;
    int numskinref;
    int numskinfamilies;
    int skinindex;

    int numbodyparts;
    int bodypartindex;

    int numattachments;
    int attachmentindex;

    int soundtable;
    int soundindex;
    int soundgroups;
    int soundgroupindex;

    int numtransitions;
    int transitionindex;
} studiohdr_t;

typedef struct
{
    char name[64];
    int nummodels;
    int base;
    int modelindex;
} mstudiobodyparts_t;

typedef struct
{
    char name[64];
    int flags;
    int width;
    int height;
    int index;
} mstudiotexture_t;

typedef struct
{
    int numtris;
    int triindex;
    int skinref;
    int numnorms;
    int normindex;
} mstudiomesh_t;

typedef struct
{
    int unused0[4];
    int vertindex;
    int unused1;
    int normindex;
} mstudiomodeldata_t;

typedef struct
{
    char name[64];
    int type;
    float boundingradius;
    int unused0;
    int nummesh;
    int meshindex;
    int numverts;
    int vertinfoindex;
    int unused1;
    int norminfoindex;
    int unused2;
    int modeldataindex;
} mstudiomodel_t;

typedef struct
{
    char label[32];
    float fps;
    int flags;
    int activity;
    int actweight;
    int numframes;
    int numevents;
    int eventindex;
    int unused0;
    int motiontype;
    int motionbone;
    int unused1[5];
    int animindex;
    int unused2[2];
} mstudioseqdesc_t;

typedef struct
{
    int frame;
    int event;
    int type;
    char options[64];
} mstudioevent_t;

typedef struct
{
    char name[32];
    int parent;
    int unused[6];
} mstudiobone_t;

typedef struct
{
    int bone;
    int type;
    float start;
    float end;
} mstudiobonecontroller_t;

// motion flags
#define STUDIO_X 0x0001
#define STUDIO_Y 0x0002
#define STUDIO_Z 0x0004
#define STUDIO_XR 0x0008
#define STUDIO_YR 0x0010
#define STUDIO_ZR 0x0020
#define STUDIO_LX 0x0040
#define STUDIO_LY 0x0080
#define STUDIO_LZ 0x0100
#define STUDIO_AX 0x0200
#define STUDIO_AY 0x0400
#define STUDIO_AZ 0x0800
#define STUDIO_XRT 0x1000
#define STUDIO_YRT 0x2000
#define STUDIO_ZRT 0x4000
#define STUDIO_QT 0x8000
#define STUDIO_RLOOP 0x8000

#define STUDIO_LOOPING 0x0001

#define STUDIO_HAS_NORMALS 0x0001
#define STUDIO_HAS_VERTICES 0x0002
#define STUDIO_HAS_BBOX 0x0004
#define STUDIO_HAS_CHROME 0x0008

#endif
