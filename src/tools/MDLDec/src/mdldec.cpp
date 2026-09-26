/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
****/

// mdldec.cpp: loads the model and poses its skeleton

#include "mdldec.h"

#include <direct.h>
#include <errno.h>
#include <ctype.h>
#include <string>
#include <unordered_map>

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

//=========================================================
// EnsureDestPath - DestPath defaults to the model's folder,
// ends with a separator and exists
//=========================================================
bool CMDLDecompiler::EnsureDestPath(const char *modelname)
{
	if (DestPath[0] == 0)
	{
		char tmp[_MAX_PATH];
		strcpy(tmp, modelname);
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

	// create every folder along the way, but not drive letters
	for (char *ofs = path; *ofs; ofs++)
	{
		if (!PATHSEPARATOR(*ofs))
			continue;

		char c = *ofs;
		*ofs = 0;

		size_t partLen = strlen(path);
		if (partLen > 0 && path[partLen - 1] != ':')
		{
			if (_mkdir(path) == -1 && errno != EEXIST)
			{
				LogMessage(MDLDEC_MSG_ERROR, "ERROR: Couldn't create %s\r\n", path);
				return false;
			}
		}

		*ofs = c;
	}

	if (_mkdir(path) == -1 && errno != EEXIST)
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

	if (((studiohdr_t *)CoreFile.data())->id != IDSTUDIOHEADER)
	{
		LogMessage(MDLDEC_MSG_ERROR, "ERROR: %s isn't a valid IDST studio model.\r\n", modelname);
		return false;
	}

	m_pstudiohdr = (studiohdr_t *)CoreFile.data();
	if (m_pstudiohdr->version != STUDIO_VERSION)
	{
		LogMessage(MDLDEC_MSG_ERROR, "ERROR: Unsupported studio version %d in %s (expected %d for HL Alpha).\r\n", m_pstudiohdr->version, modelname, STUDIO_VERSION);
		return false;
	}

	if (m_pstudiohdr->length <= 0 || (size_t)m_pstudiohdr->length > CoreFile.size())
	{
		LogMessage(MDLDEC_MSG_WARNING, "WARNING: Header length (%d) looks invalid for %s, using raw file size.\r\n", m_pstudiohdr->length, modelname);
	}

	m_ptexturehdr = m_pstudiohdr;

	// textures in a separate <name>T.mdl
	if (m_pstudiohdr->numtextures == 0)
	{
		char textureName[_MAX_PATH];
		strcpy(textureName, modelname);

		size_t nameLen = strlen(textureName);
		if (nameLen < strlen(".mdl"))
		{
			LogMessage(MDLDEC_MSG_ERROR, "ERROR: Invalid model filename: %s\r\n", modelname);
			return false;
		}
		strcpy(&textureName[nameLen - strlen(".mdl")], "T.mdl");

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

		m_ptexturehdr = (studiohdr_t *)TextureFile.data();
	}

	GetModelName();
	BuildReferencePose();
	ModelLoaded = true;
	return true;
}

//=========================================================
// GetModelName - the base name stored in the model, else
// the file's
//=========================================================
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

	return (const mstudioseqdesc_t *)(CoreFile.data() + offset);
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

//=========================================================
// FixRepeatedSequenceNames - numbers the sequences that
// share a name, so their .smd files don't overwrite
//=========================================================
void CMDLDecompiler::FixRepeatedSequenceNames()
{
	if (!m_pstudiohdr)
		return;

	std::unordered_map<std::string, int> totals;
	std::unordered_map<std::string, int> seen;
	std::vector<std::string> original((size_t)m_pstudiohdr->numseq);

	for (int i = 0; i < m_pstudiohdr->numseq; i++)
	{
		const mstudioseqdesc_t *pseqdesc = GetSequenceDesc(i);
		if (!pseqdesc)
			continue;

		original[(size_t)i] = pseqdesc->label;
		totals[original[(size_t)i]]++;
	}

	for (int i = 0; i < m_pstudiohdr->numseq; i++)
	{
		mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)(CoreFile.data() + m_pstudiohdr->seqindex + i * sizeof(mstudioseqdesc_t));

		const std::string &base = original[(size_t)i];
		int idx = ++seen[base];
		if (idx > 1)
		{
			char label[sizeof(pseqdesc->label)];
			_snprintf(label, sizeof(label), "%s_%d", base.c_str(), idx);
			label[sizeof(label) - 1] = 0;
			strncpy(pseqdesc->label, label, sizeof(pseqdesc->label) - 1);
			pseqdesc->label[sizeof(pseqdesc->label) - 1] = 0;
		}
	}

	for (std::unordered_map<std::string, int>::const_iterator it = totals.begin(); it != totals.end(); ++it)
	{
		if (it->second > 1)
			LogMessage(MDLDEC_MSG_WARNING, "WARNING: Sequence name \"%s\" is repeated %d times. Added numeric suffixes.\r\n", it->first.c_str(), it->second);
	}
}

//=========================================================
// CalcSequencePose - poses every bone at the frame, the way
// the alpha engine does (R_StudioCalcRotations)
//=========================================================
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

	for (int bone = 0; bone < numBones; bone++)
	{
		AlphaBonePose &pose = outPose[(size_t)bone];
		pose.pos[0] = pose.pos[1] = pose.pos[2] = 0.0f;
		pose.ang[0] = pose.ang[1] = pose.ang[2] = 0.0f;
		pose.quat[0] = pose.quat[1] = pose.quat[2] = 0.0f;
		pose.quat[3] = 1.0f;

		size_t animOffset = (size_t)pseqdesc->animindex + (size_t)bone * sizeof(mstudioboneanim_t);
		if (animOffset + sizeof(mstudioboneanim_t) > CoreFile.size())
			continue;

		const mstudioboneanim_t *panim = (const mstudioboneanim_t *)(base + animOffset);

		// rotation, from the last key at or before the frame towards the next one
		if (panim->numrotkeys > 0 && panim->rotkeyindex > 0
			&& (size_t)panim->rotkeyindex + (size_t)panim->numrotkeys * sizeof(mstudiorotkey_t) <= CoreFile.size())
		{
			const mstudiorotkey_t *pkeys = (const mstudiorotkey_t *)(base + panim->rotkeyindex);

			int key = 1;
			while (key < panim->numrotkeys)
			{
				if (pkeys[key].frame > frameInt)
					break;
				key++;
			}

			vec3_t angle1;
			vec3_t angle2;
			float t = 0.0f;

			if (key >= panim->numrotkeys)
			{
				const mstudiorotkey_t *pkey = &pkeys[panim->numrotkeys - 1];
				angle1[0] = pkey->pitch / ROTKEY_SCALE;
				angle1[1] = pkey->yaw / ROTKEY_SCALE;
				angle1[2] = pkey->roll / ROTKEY_SCALE;
				VectorCopy(angle1, angle2);
			}
			else
			{
				const mstudiorotkey_t *pkey1 = &pkeys[key - 1];
				const mstudiorotkey_t *pkey2 = &pkeys[key];

				angle1[0] = pkey1->pitch / ROTKEY_SCALE;
				angle1[1] = pkey1->yaw / ROTKEY_SCALE;
				angle1[2] = pkey1->roll / ROTKEY_SCALE;

				angle2[0] = pkey2->pitch / ROTKEY_SCALE;
				angle2[1] = pkey2->yaw / ROTKEY_SCALE;
				angle2[2] = pkey2->roll / ROTKEY_SCALE;

				int delta = pkey2->frame - pkey1->frame;
				if (delta != 0)
					t = (frame - pkey1->frame) / (float)delta;
			}

			vec4_t q1, q2;
			AngleQuaternion(angle1, q1);
			AngleQuaternion(angle2, q2);
			QuaternionSlerp(q1, q2, t, pose.quat);
			QuaternionAngles(pose.quat, pose.ang);
		}

		// position, the same way
		if (panim->numposkeys > 0 && panim->poskeyindex > 0
			&& (size_t)panim->poskeyindex + (size_t)panim->numposkeys * sizeof(mstudioposkey_t) <= CoreFile.size())
		{
			const mstudioposkey_t *pkeys = (const mstudioposkey_t *)(base + panim->poskeyindex);

			int key = 1;
			while (key < panim->numposkeys)
			{
				if (pkeys[key].frame > frameInt)
					break;
				key++;
			}

			if (key >= panim->numposkeys)
			{
				VectorCopy(pkeys[panim->numposkeys - 1].pos, pose.pos);
			}
			else
			{
				const mstudioposkey_t *pkey1 = &pkeys[key - 1];
				const mstudioposkey_t *pkey2 = &pkeys[key];

				int delta = pkey2->frame - pkey1->frame;
				float t = 0.0f;
				if (delta != 0)
					t = (frame - pkey1->frame) / (float)delta;
				float it = 1.0f - t;

				pose.pos[0] = pkey1->pos[0] * it + pkey2->pos[0] * t;
				pose.pos[1] = pkey1->pos[1] * it + pkey2->pos[1] * t;
				pose.pos[2] = pkey1->pos[2] * it + pkey2->pos[2] * t;
			}
		}
	}

	// the motion bone stays in place
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

	const mstudiobone_t *pbones = (const mstudiobone_t *)(CoreFile.data() + m_pstudiohdr->boneindex);

	for (int i = 0; i < m_pstudiohdr->numbones; i++)
	{
		float bonematrix[3][4];
		memset(bonematrix, 0, sizeof(bonematrix));

		QuaternionMatrix(pose[(size_t)i].quat, bonematrix);
		bonematrix[0][3] = pose[(size_t)i].pos[0];
		bonematrix[1][3] = pose[(size_t)i].pos[1];
		bonematrix[2][3] = pose[(size_t)i].pos[2];

		if (pbones[i].parent == -1)
			memcpy(outTransforms[i], bonematrix, sizeof(bonematrix));
		else
			R_ConcatTransforms(outTransforms[pbones[i].parent], bonematrix, outTransforms[i]);
	}
}

//=========================================================
// BuildReferencePose - the reference meshes are posed with
// the first frame of the first sequence
//=========================================================
void CMDLDecompiler::BuildReferencePose()
{
	int numBones = m_pstudiohdr ? m_pstudiohdr->numbones : 0;
	if (numBones <= 0)
		return;

	if (m_pstudiohdr->numseq > 0)
	{
		const mstudioseqdesc_t *pseqdesc = GetSequenceDesc(0);
		if (!CalcSequencePose(pseqdesc, 0.0f, m_referencePose))
			m_referencePose.assign((size_t)numBones, AlphaBonePose());
	}
	else
	{
		m_referencePose.assign((size_t)numBones, AlphaBonePose());
	}

	// bones without a pose get the identity rotation
	for (int i = 0; i < numBones; i++)
	{
		AlphaBonePose &pose = m_referencePose[(size_t)i];

		if (pose.quat[3] == 0.0f && pose.quat[0] == 0.0f && pose.quat[1] == 0.0f && pose.quat[2] == 0.0f)
			pose.quat[3] = 1.0f;
	}

	BuildBoneMatrices(m_referencePose, g_bonetransform);
	memcpy(g_normtransform, g_bonetransform, sizeof(g_normtransform));
}

//=========================================================
// GetBoneName - bone_<index> for bones without a usable name
//=========================================================
void CMDLDecompiler::GetBoneName(int boneIndex, char *out, size_t outSize) const
{
	if (!out || outSize == 0)
		return;

	out[0] = 0;

	const mstudiobone_t *pbone = NULL;

	if (m_pstudiohdr && boneIndex >= 0 && boneIndex < m_pstudiohdr->numbones)
	{
		size_t offset = (size_t)m_pstudiohdr->boneindex + (size_t)boneIndex * sizeof(mstudiobone_t);
		if (offset + sizeof(mstudiobone_t) <= CoreFile.size())
			pbone = (const mstudiobone_t *)(CoreFile.data() + offset);
	}

	bool hasVisible = false;
	if (pbone)
	{
		for (size_t i = 0; i < sizeof(pbone->name) && pbone->name[i]; i++)
		{
			if (!isspace((unsigned char)pbone->name[i]))
			{
				hasVisible = true;
				break;
			}
		}
	}

	if (!hasVisible)
	{
		_snprintf(out, outSize, "bone_%02d", boneIndex);
		out[outSize - 1] = 0;
		return;
	}

	strncpy(out, pbone->name, outSize - 1);
	out[outSize - 1] = 0;
}
