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
// mdldec.cpp: Half-Life Alpha MDL Decompiler.
//

#include "mdldec.h"

#include <direct.h>
#include <errno.h>
#include <ctype.h>
#include <string>
#include <unordered_map>

#pragma pack(push, 1)
struct alpha_pos_key_t
{
    short frame;
    short pad;
    float x;
    float y;
    float z;
};
#pragma pack(pop)

CMDLDecompiler::CMDLDecompiler()
{
    memset(ModelName, 0, sizeof(ModelName));
    memset(SourcePath, 0, sizeof(SourcePath));
    memset(DestPath, 0, sizeof(DestPath));

    ModelLoaded = false;
    m_pstudiohdr = NULL;
    m_ptexturehdr = NULL;
    qcfile = NULL;

    memset(g_bonetransform, 0, sizeof(g_bonetransform));
    memset(g_normtransform, 0, sizeof(g_normtransform));
}

CMDLDecompiler::~CMDLDecompiler()
{
}

bool CMDLDecompiler::ReadFileToBuffer(const char *path, std::vector<byte> &buffer)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return false;

    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (length <= 0)
    {
        fclose(fp);
        return false;
    }

    buffer.resize((size_t)length);
    size_t read = fread(buffer.data(), 1, (size_t)length, fp);
    fclose(fp);
    return read == (size_t)length;
}

bool CMDLDecompiler::EnsureDestPath(const char *modelname)
{
    if (DestPath[0] == 0)
    {
        char tmp[_MAX_PATH];
        strcpy(tmp, (char *)modelname);
        MyExtractFilePath(tmp, DestPath);
        if (DestPath[0] == 0)
            strcpy(DestPath, ".\\");
    }

    size_t len = strlen(DestPath);
    if (len > 0 && !PATHSEPARATOR(DestPath[len - 1]))
    {
        if (len + 1 >= sizeof(DestPath))
            return false;
        DestPath[len] = '\\';
        DestPath[len + 1] = 0;
    }

    char path[_MAX_PATH];
    strcpy(path, DestPath);

    for (char *ofs = path; *ofs; ++ofs)
    {
        if (PATHSEPARATOR(*ofs))
        {
            char c = *ofs;
            *ofs = 0;
            size_t partLen = strlen(path);
            if (partLen > 0 && path[partLen - 1] != ':')
            {
                if (_mkdir(path) == -1 && errno != EEXIST && errno != 17)
                {
                    LogMessage(MDLDEC_MSG_ERROR, "ERROR: Couldn't create %s\r\n", path);
                    return false;
                }
            }
            *ofs = c;
        }
    }

    if (_mkdir(path) == -1 && errno != EEXIST && errno != 17)
    {
        LogMessage(MDLDEC_MSG_ERROR, "ERROR: Couldn't create %s\r\n", path);
        return false;
    }

    return true;
}

bool CMDLDecompiler::LoadModel(char *modelname)
{
    strcpy(SourcePath, modelname);

    if (!EnsureDestPath(modelname))
        return false;

    CoreFile.clear();
    TextureFile.clear();
    m_referencePose.clear();
    m_pstudiohdr = NULL;
    m_ptexturehdr = NULL;
    ModelLoaded = false;

    if (!ReadFileToBuffer(modelname, CoreFile))
    {
        LogMessage(MDLDEC_MSG_ERROR, "ERROR: Couldn't open %s\r\n", modelname);
        return false;
    }

    if (CoreFile.size() < sizeof(studiohdr_t))
    {
        LogMessage(MDLDEC_MSG_ERROR, "ERROR: %s is too small to be a model file.\r\n", modelname);
        return false;
    }

    if (memcmp(CoreFile.data(), "IDST", 4) != 0)
    {
        LogMessage(MDLDEC_MSG_ERROR, "ERROR: %s isn't a valid IDST studio model.\r\n", modelname);
        return false;
    }

    m_pstudiohdr = reinterpret_cast<studiohdr_t *>(CoreFile.data());
    if (m_pstudiohdr->version != STUDIO_VERSION_ALPHA)
    {
        LogMessage(MDLDEC_MSG_ERROR,
                   "ERROR: Unsupported studio version %d in %s (expected %d for HL Alpha).\r\n",
                   m_pstudiohdr->version,
                   modelname,
                   STUDIO_VERSION_ALPHA);
        return false;
    }

    if (m_pstudiohdr->length <= 0 || (size_t)m_pstudiohdr->length > CoreFile.size())
    {
        LogMessage(MDLDEC_MSG_WARNING,
                   "WARNING: Header length (%d) looks invalid for %s, using raw file size.\r\n",
                   m_pstudiohdr->length,
                   modelname);
    }

    m_ptexturehdr = m_pstudiohdr;

    if (m_pstudiohdr->numtextures == 0)
    {
        char textureName[_MAX_PATH];
        strcpy(textureName, modelname);

        size_t nameLen = strlen(textureName);
        if (nameLen < 4)
        {
            LogMessage(MDLDEC_MSG_ERROR, "ERROR: Invalid model filename: %s\r\n", modelname);
            return false;
        }
        strcpy(&textureName[nameLen - 4], "T.mdl");

        if (!ReadFileToBuffer(textureName, TextureFile))
        {
            LogMessage(MDLDEC_MSG_ERROR, "ERROR: Missing textures file %s\r\n", textureName);
            return false;
        }

        if (TextureFile.size() < sizeof(studiohdr_t))
        {
            LogMessage(MDLDEC_MSG_ERROR, "ERROR: Invalid texture file %s\r\n", textureName);
            return false;
        }

        m_ptexturehdr = reinterpret_cast<studiohdr_t *>(TextureFile.data());
    }

    GetModelName();
    BuildReferencePose();
    ModelLoaded = true;
    return true;
}

void CMDLDecompiler::GetModelName()
{
    memset(ModelName, 0, sizeof(ModelName));

    if (m_pstudiohdr && m_pstudiohdr->name[0])
    {
        char tmp[128];
        strncpy(tmp, m_pstudiohdr->name, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        MyExtractFileBase(tmp, ModelName);
    }

    if (!ModelName[0] && SourcePath[0])
    {
        char tmp[_MAX_PATH];
        strcpy(tmp, SourcePath);
        MyExtractFileBase(tmp, ModelName);
    }

    if (!ModelName[0])
        strcpy(ModelName, "model");
}

const mstudioseqdesc_t *CMDLDecompiler::GetSequenceDesc(int i) const
{
    if (!m_pstudiohdr || i < 0 || i >= m_pstudiohdr->numseq)
        return NULL;

    size_t offset = (size_t)m_pstudiohdr->seqindex + (size_t)i * sizeof(mstudioseqdesc_t);
    if (offset + sizeof(mstudioseqdesc_t) > CoreFile.size())
        return NULL;

    return reinterpret_cast<const mstudioseqdesc_t *>(CoreFile.data() + offset);
}

void CMDLDecompiler::DumpInfo()
{
    if (!m_pstudiohdr)
        return;

    char infoPath[_MAX_PATH];
    sprintf(infoPath, "%sunmdl_alpha.txt", DestPath);

    FILE *modelinfo = fopen(infoPath, "w");
    if (!modelinfo)
        return;

    fprintf(modelinfo, "name: %s\n\n", m_pstudiohdr->name);
    fprintf(modelinfo, "version: %d\n", m_pstudiohdr->version);
    fprintf(modelinfo, "length: %d\n", m_pstudiohdr->length);
    fprintf(modelinfo, "numbones: %d\n", m_pstudiohdr->numbones);
    fprintf(modelinfo, "numbonecontrollers: %d\n", m_pstudiohdr->numbonecontrollers);
    fprintf(modelinfo, "numseq: %d\n", m_pstudiohdr->numseq);
    fprintf(modelinfo, "numtextures: %d\n", m_pstudiohdr->numtextures);
    fprintf(modelinfo, "numbodyparts: %d\n", m_pstudiohdr->numbodyparts);
    fprintf(modelinfo, "numattachments: %d\n", m_pstudiohdr->numattachments);
    fprintf(modelinfo, "numtransitions: %d\n", m_pstudiohdr->numtransitions);

    fclose(modelinfo);
}

void CMDLDecompiler::FixRepeatedSequenceNames()
{
    if (!m_pstudiohdr)
        return;

    std::unordered_map<std::string, int> totals;
    std::unordered_map<std::string, int> seen;
    std::vector<std::string> original((size_t)m_pstudiohdr->numseq);

    for (int i = 0; i < m_pstudiohdr->numseq; ++i)
    {
        const mstudioseqdesc_t *seq = GetSequenceDesc(i);
        if (!seq)
            continue;
        original[(size_t)i] = seq->label;
        totals[original[(size_t)i]]++;
    }

    for (int i = 0; i < m_pstudiohdr->numseq; ++i)
    {
        mstudioseqdesc_t *seq = reinterpret_cast<mstudioseqdesc_t *>(
            CoreFile.data() + m_pstudiohdr->seqindex + i * sizeof(mstudioseqdesc_t));

        const std::string &base = original[(size_t)i];
        int idx = ++seen[base];
        if (idx > 1)
        {
            char label[32];
            _snprintf(label, sizeof(label), "%s_%d", base.c_str(), idx);
            label[sizeof(label) - 1] = 0;
            strncpy(seq->label, label, sizeof(seq->label) - 1);
            seq->label[sizeof(seq->label) - 1] = 0;
        }
    }

    for (std::unordered_map<std::string, int>::const_iterator it = totals.begin(); it != totals.end(); ++it)
    {
        if (it->second > 1)
        {
            LogMessage(MDLDEC_MSG_WARNING,
                       "WARNING: Sequence name \"%s\" is repeated %d times. Added numeric suffixes.\r\n",
                       it->first.c_str(),
                       it->second);
        }
    }
}

bool CMDLDecompiler::CalcSequencePose(const mstudioseqdesc_t *pseqdesc, float frame, std::vector<AlphaBonePose> &outPose)
{
    if (!m_pstudiohdr || !pseqdesc)
        return false;

    int numBones = m_pstudiohdr->numbones;
    if (numBones <= 0 || numBones > MAXSTUDIOBONES)
        return false;

    outPose.assign((size_t)numBones, AlphaBonePose());
    const byte *base = CoreFile.data();
    int frameInt = (int)frame;

    for (int bone = 0; bone < numBones; ++bone)
    {
        AlphaBonePose &pose = outPose[(size_t)bone];
        pose.pos[0] = pose.pos[1] = pose.pos[2] = 0.0f;
        pose.ang[0] = pose.ang[1] = pose.ang[2] = 0.0f;
        pose.quat[0] = pose.quat[1] = pose.quat[2] = 0.0f;
        pose.quat[3] = 1.0f;

        size_t entryOffset = (size_t)pseqdesc->animindex + (size_t)bone * 16;
        if (entryOffset + 16 > CoreFile.size())
            continue;

        const int *entry = reinterpret_cast<const int *>(base + entryOffset);
        int posCount = entry[0];
        int posOffset = entry[1];
        int rotCount = entry[2];
        int rotOffset = entry[3];

        float adjX = 0.0f;
        float adjY = 0.0f;
        float adjZ = 0.0f;

        if (rotCount > 0 && rotOffset > 0)
        {
            size_t needed = (size_t)rotOffset + (size_t)rotCount * 8;
            if (needed <= CoreFile.size())
            {
                const short *keys = reinterpret_cast<const short *>(base + rotOffset);
                int key = 1;
                while (key < rotCount)
                {
                    const short *k = keys + key * 4;
                    if (k[0] > frameInt)
                        break;
                    ++key;
                }

                float angle1[3];
                float angle2[3];
                float t = 0.0f;

                if (key >= rotCount)
                {
                    const short *k = keys + (rotCount - 1) * 4;
                    angle1[0] = (float)k[2] / 100.0f + adjY;
                    angle1[1] = (float)k[3] / 100.0f + adjZ;
                    angle1[2] = (float)k[1] / 100.0f + adjX;
                    VectorCopy(angle1, angle2);
                }
                else
                {
                    const short *k1 = keys + (key - 1) * 4;
                    const short *k2 = keys + key * 4;

                    angle1[0] = (float)k1[2] / 100.0f + adjY;
                    angle1[1] = (float)k1[3] / 100.0f + adjZ;
                    angle1[2] = (float)k1[1] / 100.0f + adjX;

                    angle2[0] = (float)k2[2] / 100.0f + adjY;
                    angle2[1] = (float)k2[3] / 100.0f + adjZ;
                    angle2[2] = (float)k2[1] / 100.0f + adjX;

                    int delta = (int)k2[0] - (int)k1[0];
                    if (delta != 0)
                        t = (frame - (float)k1[0]) / (float)delta;
                }

                float q1[4], q2[4];
                AngleQuaternion(angle1, q1);
                AngleQuaternion(angle2, q2);
                QuaternionSlerp(q1, q2, t, pose.quat);
                QuaternionAngles(pose.quat, pose.ang);
            }
        }

        if (posCount > 0 && posOffset > 0)
        {
            size_t needed = (size_t)posOffset + (size_t)posCount * sizeof(alpha_pos_key_t);
            if (needed <= CoreFile.size())
            {
                const alpha_pos_key_t *keys = reinterpret_cast<const alpha_pos_key_t *>(base + posOffset);
                int key = 1;
                while (key < posCount)
                {
                    if (keys[key].frame > frameInt)
                        break;
                    ++key;
                }

                if (key >= posCount)
                {
                    const alpha_pos_key_t &k = keys[posCount - 1];
                    pose.pos[0] = k.x;
                    pose.pos[1] = k.y;
                    pose.pos[2] = k.z;
                }
                else
                {
                    const alpha_pos_key_t &k1 = keys[key - 1];
                    const alpha_pos_key_t &k2 = keys[key];
                    int delta = (int)k2.frame - (int)k1.frame;
                    float t = 0.0f;
                    if (delta != 0)
                        t = (frame - (float)k1.frame) / (float)delta;
                    float it = 1.0f - t;

                    pose.pos[0] = k1.x * it + k2.x * t;
                    pose.pos[1] = k1.y * it + k2.y * t;
                    pose.pos[2] = k1.z * it + k2.z * t;
                }
            }
        }
    }

    if (pseqdesc->motionbone >= 0 && pseqdesc->motionbone < numBones)
    {
        AlphaBonePose &motionBone = outPose[(size_t)pseqdesc->motionbone];
        if (pseqdesc->motiontype & STUDIO_X)
            motionBone.pos[0] = 0.0f;
        if (pseqdesc->motiontype & STUDIO_Y)
            motionBone.pos[1] = 0.0f;
        if (pseqdesc->motiontype & STUDIO_Z)
            motionBone.pos[2] = 0.0f;
    }

    return true;
}

void CMDLDecompiler::BuildBoneMatrices(const std::vector<AlphaBonePose> &pose, float outTransforms[MAXSTUDIOBONES][3][4])
{
    if (!m_pstudiohdr)
        return;

    const mstudiobone_t *bones = reinterpret_cast<const mstudiobone_t *>(CoreFile.data() + m_pstudiohdr->boneindex);

    for (int i = 0; i < m_pstudiohdr->numbones; ++i)
    {
        float boneMatrix[3][4];
        memset(boneMatrix, 0, sizeof(boneMatrix));

        QuaternionMatrix((float *)pose[(size_t)i].quat, (float *)boneMatrix);
        boneMatrix[0][3] = pose[(size_t)i].pos[0];
        boneMatrix[1][3] = pose[(size_t)i].pos[1];
        boneMatrix[2][3] = pose[(size_t)i].pos[2];

        if (bones[i].parent == -1)
        {
            memcpy(outTransforms[i], boneMatrix, sizeof(boneMatrix));
        }
        else
        {
            R_ConcatTransforms(outTransforms[bones[i].parent], boneMatrix, outTransforms[i]);
        }
    }
}

void CMDLDecompiler::BuildReferencePose()
{
    int numBones = m_pstudiohdr ? m_pstudiohdr->numbones : 0;
    if (numBones <= 0)
        return;

    if (m_pstudiohdr->numseq > 0)
    {
        const mstudioseqdesc_t *seq = GetSequenceDesc(0);
        if (!CalcSequencePose(seq, 0.0f, m_referencePose))
            m_referencePose.assign((size_t)numBones, AlphaBonePose());
    }
    else
    {
        m_referencePose.assign((size_t)numBones, AlphaBonePose());
    }

    for (int i = 0; i < numBones; ++i)
    {
        if (m_referencePose[(size_t)i].quat[3] == 0.0f &&
            m_referencePose[(size_t)i].quat[0] == 0.0f &&
            m_referencePose[(size_t)i].quat[1] == 0.0f &&
            m_referencePose[(size_t)i].quat[2] == 0.0f)
        {
            m_referencePose[(size_t)i].quat[3] = 1.0f;
        }
    }

    BuildBoneMatrices(m_referencePose, g_bonetransform);
    memcpy(g_normtransform, g_bonetransform, sizeof(g_normtransform));
}

void CMDLDecompiler::GetBoneName(int boneIndex, char *out, size_t outSize) const
{
    if (!out || outSize == 0)
        return;

    out[0] = 0;

    if (!m_pstudiohdr || boneIndex < 0 || boneIndex >= m_pstudiohdr->numbones)
    {
        _snprintf(out, outSize, "bone_%02d", boneIndex);
        out[outSize - 1] = 0;
        return;
    }

    size_t offset = (size_t)m_pstudiohdr->boneindex + (size_t)boneIndex * sizeof(mstudiobone_t);
    if (offset + sizeof(mstudiobone_t) > CoreFile.size())
    {
        _snprintf(out, outSize, "bone_%02d", boneIndex);
        out[outSize - 1] = 0;
        return;
    }

    const mstudiobone_t *bone = reinterpret_cast<const mstudiobone_t *>(CoreFile.data() + offset);
    bool hasVisible = false;
    for (size_t i = 0; i < sizeof(bone->name); ++i)
    {
        unsigned char c = (unsigned char)bone->name[i];
        if (c == 0)
            break;
        if (!isspace(c))
        {
            hasVisible = true;
            break;
        }
    }

    if (!hasVisible || bone->name[0] == 0)
    {
        _snprintf(out, outSize, "bone_%02d", boneIndex);
        out[outSize - 1] = 0;
        return;
    }

    strncpy(out, bone->name, outSize - 1);
    out[outSize - 1] = 0;
}
