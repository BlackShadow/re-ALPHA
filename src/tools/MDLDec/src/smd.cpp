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
// smd.cpp: Half-Life Alpha MDL Decompiler.
//

#include "mdldec.h"

static float DegToRad(float deg)
{
    return deg * (Q_PI / 180.0f);
}

void CMDLDecompiler::SMD_GenerateReferences()
{
    if (!m_pstudiohdr)
        return;

    for (int i = 0; i < m_pstudiohdr->numbodyparts; ++i)
    {
        mstudiobodyparts_t *pbodyparts =
            (mstudiobodyparts_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bodypartindex + sizeof(mstudiobodyparts_t) * i);

        for (int j = 0; j < pbodyparts->nummodels; ++j)
        {
            mstudiomodel_t *pmodel =
                (mstudiomodel_t *)((byte *)m_pstudiohdr + pbodyparts->modelindex + sizeof(mstudiomodel_t) * j);

            if (_strnicmp(pmodel->name, "blank", 5) == 0)
                continue;

            char modelBase[64];
            strncpy(modelBase, pmodel->name, sizeof(modelBase) - 1);
            modelBase[sizeof(modelBase) - 1] = 0;
            MyExtractFileBase(modelBase, modelBase);

            char smdfilename[_MAX_PATH];
            sprintf(smdfilename, "%s%s.smd", DestPath, modelBase);

            FILE *smdfile = fopen(smdfilename, "w");
            if (!smdfile)
            {
                LogMessage(MDLDEC_MSG_ERROR, "ERROR: Can't write %s\r\n", smdfilename);
                return;
            }

            fprintf(smdfile, "version 1\n");
            SMD_WriteNodes(smdfile);

            fprintf(smdfile, "skeleton\n");
            fprintf(smdfile, "time 0\n");
            for (int bone = 0; bone < m_pstudiohdr->numbones; ++bone)
            {
                const AlphaBonePose &p = m_referencePose[(size_t)bone];
                fprintf(smdfile,
                        "%3i %f %f %f %f %f %f\n",
                        bone,
                        p.pos[0],
                        p.pos[1],
                        p.pos[2],
                        DegToRad(p.ang[0]),
                        DegToRad(p.ang[1]),
                        DegToRad(p.ang[2]));
            }
            fprintf(smdfile, "end\n");

            fprintf(smdfile, "triangles\n");

            if (pmodel->modeldataindex <= 0 || (size_t)pmodel->modeldataindex + sizeof(mstudiomodeldata_t) > CoreFile.size())
            {
                fclose(smdfile);
                LogMessage(MDLDEC_MSG_WARNING, "WARNING: Model %s has invalid modeldataindex.\r\n", pmodel->name);
                continue;
            }

            const mstudiomodeldata_t *pmodeldata =
                (const mstudiomodeldata_t *)((byte *)m_pstudiohdr + pmodel->modeldataindex);

            mstudiotexture_t *ptextures = (mstudiotexture_t *)((byte *)m_ptexturehdr + m_ptexturehdr->textureindex);

            for (int k = 0; k < pmodel->nummesh; ++k)
            {
                const mstudiomesh_t *pmesh =
                    (const mstudiomesh_t *)((byte *)m_pstudiohdr + pmodel->meshindex + sizeof(mstudiomesh_t) * k);

                const mstudiotexture_t *ptexture = &ptextures[pmesh->skinref];
                const short *tri = (const short *)((byte *)m_pstudiohdr + pmesh->triindex);

                for (int t = 0; t < pmesh->numtris; ++t)
                {
                    SMD_WriteTriangle(smdfile, pmodel, pmodeldata, ptexture, tri);
                    tri += 12;
                }
            }

            fprintf(smdfile, "end\n");
            fclose(smdfile);
            LogMessage(MDLDEC_MSG_INFO, "Reference: %s\r\n", smdfilename);
        }
    }
}

void CMDLDecompiler::SMD_WriteTriangle(FILE *smdfile,
                                       const mstudiomodel_t *pmodel,
                                       const mstudiomodeldata_t *pmodeldata,
                                       const mstudiotexture_t *ptexture,
                                       const short *tri)
{
    const vec3_t *pverts = (const vec3_t *)((byte *)m_pstudiohdr + pmodeldata->vertindex);
    const vec3_t *pnorms = (const vec3_t *)((byte *)m_pstudiohdr + pmodeldata->normindex);
    const byte *pvertbone = (const byte *)m_pstudiohdr + pmodel->vertinfoindex;
    const byte *pnormbone = (const byte *)m_pstudiohdr + pmodel->norminfoindex;

    float s = 1.0f / (float)ptexture->width;
    float t = 1.0f / (float)ptexture->height;

    fprintf(smdfile, "%s\n", ptexture->name);

    for (int k = 0; k < 3; ++k)
    {
        int vindex = tri[k * 4 + 0];
        int nindex = tri[k * 4 + 1];
        if (vindex < 0 || vindex >= pmodel->numverts || vindex >= MAXSTUDIOVERTS)
            vindex = 0;
        if (nindex < 0 || nindex >= MAXSTUDIOVERTS)
            nindex = 0;

        int boneV = (int)pvertbone[vindex];
        int boneN = (int)pnormbone[nindex];

        if (boneV < 0 || boneV >= MAXSTUDIOBONES)
            boneV = 0;
        if (boneN < 0 || boneN >= MAXSTUDIOBONES)
            boneN = 0;

        vec3_t vertex;
        vec3_t normal;
        VectorTransform((float *)pverts[vindex], g_bonetransform[boneV], vertex);
        VectorRotate((float *)pnorms[nindex], g_normtransform[boneN], normal);
        VectorNormalize(normal);

        fprintf(smdfile,
                "%3i %f %f %f %f %f %f %f %f\n",
                boneV,
                vertex[0],
                vertex[1],
                vertex[2],
                normal[0],
                normal[1],
                normal[2],
                tri[k * 4 + 2] * s,
                1.0f - (tri[k * 4 + 3] * t));
    }
}

void CMDLDecompiler::SMD_GenerateSequences()
{
    if (!m_pstudiohdr)
        return;

    for (int i = 0; i < m_pstudiohdr->numseq; ++i)
    {
        const mstudioseqdesc_t *pseqdesc = GetSequenceDesc(i);
        if (!pseqdesc || pseqdesc->numframes <= 0)
            continue;

        char smdfilename[_MAX_PATH];
        sprintf(smdfilename, "%s%s.smd", DestPath, pseqdesc->label);

        FILE *smdfile = fopen(smdfilename, "w");
        if (!smdfile)
        {
            LogMessage(MDLDEC_MSG_ERROR, "ERROR: Can't write %s\r\n", smdfilename);
            return;
        }

        fprintf(smdfile, "version 1\n");
        SMD_WriteNodes(smdfile);
        fprintf(smdfile, "skeleton\n");

        for (int frame = 0; frame < pseqdesc->numframes; ++frame)
        {
            SMD_WriteFrame(smdfile, frame, pseqdesc);
        }

        fprintf(smdfile, "end\n");
        fclose(smdfile);
        LogMessage(MDLDEC_MSG_INFO, "Sequence: %s\r\n", smdfilename);
    }
}

void CMDLDecompiler::SMD_WriteFrame(FILE *smd, int frame, const mstudioseqdesc_t *pseqdesc)
{
    std::vector<AlphaBonePose> pose;
    if (!CalcSequencePose(pseqdesc, (float)frame, pose))
        return;

    // Legacy Alpha v6 stores extracted sequence movement as linear deltas in the
    // seqdesc payload (offsets 76/80/84). When exporting SMD, re-apply that
    // movement on the motion bone so StudioMDL can extract it back on recompile.
    if (pseqdesc->numframes > 1 && pseqdesc->motionbone >= 0 && pseqdesc->motionbone < m_pstudiohdr->numbones)
    {
        const byte *seqRaw = reinterpret_cast<const byte *>(pseqdesc);
        const float moveX = *reinterpret_cast<const float *>(seqRaw + 76);
        const float moveY = *reinterpret_cast<const float *>(seqRaw + 80);
        const float moveZ = *reinterpret_cast<const float *>(seqRaw + 84);
        const float t = (float)frame / (float)(pseqdesc->numframes - 1);
        AlphaBonePose &motionPose = pose[(size_t)pseqdesc->motionbone];

        if (pseqdesc->motiontype & STUDIO_LX)
            motionPose.pos[0] += moveX * t;
        if (pseqdesc->motiontype & STUDIO_LY)
            motionPose.pos[1] += moveY * t;
        if (pseqdesc->motiontype & STUDIO_LZ)
            motionPose.pos[2] += moveZ * t;
    }

    fprintf(smd, "time %i\n", frame);
    for (int i = 0; i < m_pstudiohdr->numbones; ++i)
    {
        const AlphaBonePose &p = pose[(size_t)i];
        fprintf(smd,
                "%3i %f %f %f %f %f %f\n",
                i,
                p.pos[0],
                p.pos[1],
                p.pos[2],
                DegToRad(p.ang[0]),
                DegToRad(p.ang[1]),
                DegToRad(p.ang[2]));
    }
}

void CMDLDecompiler::SMD_WriteNodes(FILE *smd)
{
    if (!m_pstudiohdr)
        return;

    fprintf(smd, "nodes\n");
    for (int i = 0; i < m_pstudiohdr->numbones; ++i)
    {
        const mstudiobone_t *pbone =
            (const mstudiobone_t *)((byte *)m_pstudiohdr + m_pstudiohdr->boneindex + sizeof(mstudiobone_t) * i);
        char boneName[64];
        GetBoneName(i, boneName, sizeof(boneName));
        fprintf(smd, "%3i \"%s\" %i\n", i, boneName, pbone->parent);
    }
    fprintf(smd, "end\n");
}
