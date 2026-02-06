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
// mdldec.h: Half-Life Alpha MDL Decompiler adapted from the original MDLDec codebase.
//

#pragma once

#ifndef _MDLDECOMPILER_H
#define _MDLDECOMPILER_H

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "..\hlsdk\mathlib.h"
#include "..\hlsdk\studio_alpha.h"

#define MDLDEC_VERSION "2.00a"
#define MDLDEC_MSG_INFO 0
#define MDLDEC_MSG_WARNING 1
#define MDLDEC_MSG_ERROR 2

#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
void MyExtractFileBase(char *path, char *dest);
void MyExtractFilePath(char *path, char *dest);

struct AlphaBonePose
{
    vec3_t pos;
    vec3_t ang;
    vec4_t quat;
};

class CMDLDecompiler
{
public:
    CMDLDecompiler();
    virtual ~CMDLDecompiler();

    bool LoadModel(char *modelname);
    void DumpInfo();
    void QC_GenerateScript();
    void SMD_GenerateReferences();
    void SMD_GenerateSequences();
    void BMP_GenerateTextures();
    void FixRepeatedSequenceNames();
    void LogMessage(int type, const char *msg, ...);

    char DestPath[_MAX_PATH];

private:
    bool ModelLoaded;
    char ModelName[64];
    char SourcePath[_MAX_PATH];

    FILE *qcfile;

    std::vector<byte> CoreFile;
    std::vector<byte> TextureFile;
    studiohdr_t *m_pstudiohdr;
    studiohdr_t *m_ptexturehdr;

    std::vector<AlphaBonePose> m_referencePose;

    float g_bonetransform[MAXSTUDIOBONES][3][4];
    float g_normtransform[MAXSTUDIOBONES][3][4];

    bool EnsureDestPath(const char *modelname);
    bool ReadFileToBuffer(const char *path, std::vector<byte> &buffer);
    const mstudioseqdesc_t *GetSequenceDesc(int i) const;

    void QC_WriteBodyModels();
    void QC_WriteTextureGroups();
    void QC_WriteBoneControllers();
    void QC_WriteSequences();
    void QC_TranslateMotionFlag(int mflag, char *string, bool addtostring);

    void SMD_WriteNodes(FILE *smdfile);
    void SMD_WriteTriangle(FILE *smd,
                           const mstudiomodel_t *pmodel,
                           const mstudiomodeldata_t *pmodeldata,
                           const mstudiotexture_t *ptexture,
                           const short *tri);
    void SMD_WriteFrame(FILE *smd, int frame, const mstudioseqdesc_t *pseqdesc);

    bool CalcSequencePose(const mstudioseqdesc_t *pseqdesc, float frame, std::vector<AlphaBonePose> &outPose);
    void BuildBoneMatrices(const std::vector<AlphaBonePose> &pose, float outTransforms[MAXSTUDIOBONES][3][4]);
    void BuildReferencePose();
    void GetBoneName(int boneIndex, char *out, size_t outSize) const;

    void GetModelName();
    void BMP_WriteTexture(byte *pBitData,
                          byte *pColorData,
                          const char *TextureName,
                          const mstudiotexture_t *pTexture,
                          const char *textureDestPath);
};

#endif
