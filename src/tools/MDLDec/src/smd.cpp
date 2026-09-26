/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
****/

// smd.cpp: writes the reference and sequence .smd files

#include "mdldec.h"

static float DegToRad(float deg)
{
	return deg * (Q_PI / 180.0f);
}

void CMDLDecompiler::SMD_GenerateReferences()
{
	if (!m_pstudiohdr)
		return;

	mstudiobodyparts_t *pbodyparts = (mstudiobodyparts_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bodypartindex);
	mstudiotexture_t *ptextures = (mstudiotexture_t *)((byte *)m_ptexturehdr + m_ptexturehdr->textureindex);

	for (int i = 0; i < m_pstudiohdr->numbodyparts; i++)
	{
		mstudiomodel_t *pmodels = (mstudiomodel_t *)((byte *)m_pstudiohdr + pbodyparts[i].modelindex);

		for (int j = 0; j < pbodyparts[i].nummodels; j++)
		{
			mstudiomodel_t *pmodel = &pmodels[j];

			if (_strnicmp(pmodel->name, "blank", strlen("blank")) == 0)
				continue;

			char modelBase[sizeof(pmodel->name)];
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
			for (int bone = 0; bone < m_pstudiohdr->numbones; bone++)
			{
				const AlphaBonePose &pose = m_referencePose[(size_t)bone];
				fprintf(smdfile, "%3i %f %f %f %f %f %f\n", bone, pose.pos[0], pose.pos[1], pose.pos[2], DegToRad(pose.ang[0]), DegToRad(pose.ang[1]), DegToRad(pose.ang[2]));
			}
			fprintf(smdfile, "end\n");

			fprintf(smdfile, "triangles\n");

			if (pmodel->modeldataindex <= 0 || (size_t)pmodel->modeldataindex + sizeof(mstudiomodeldata_t) > CoreFile.size())
			{
				fclose(smdfile);
				LogMessage(MDLDEC_MSG_WARNING, "WARNING: Model %s has invalid modeldataindex.\r\n", pmodel->name);
				continue;
			}

			const mstudiomodeldata_t *pmodeldata = (const mstudiomodeldata_t *)((byte *)m_pstudiohdr + pmodel->modeldataindex);
			const mstudiomesh_t *pmeshes = (const mstudiomesh_t *)((byte *)m_pstudiohdr + pmodel->meshindex);

			for (int k = 0; k < pmodel->nummesh; k++)
			{
				const mstudiotrivert_t *ptrivert = (const mstudiotrivert_t *)((byte *)m_pstudiohdr + pmeshes[k].triindex);

				for (int t = 0; t < pmeshes[k].numtris; t++)
				{
					SMD_WriteTriangle(smdfile, pmodel, pmodeldata, &ptextures[pmeshes[k].skinref], ptrivert);
					ptrivert += 3;
				}
			}

			fprintf(smdfile, "end\n");
			fclose(smdfile);
			LogMessage(MDLDEC_MSG_INFO, "Reference: %s\r\n", smdfilename);
		}
	}
}

void CMDLDecompiler::SMD_WriteTriangle(FILE *smdfile, const mstudiomodel_t *pmodel, const mstudiomodeldata_t *pmodeldata, const mstudiotexture_t *ptexture, const mstudiotrivert_t *ptrivert)
{
	const vec3_t *pverts = (const vec3_t *)((byte *)m_pstudiohdr + pmodeldata->vertindex);
	const vec3_t *pnorms = (const vec3_t *)((byte *)m_pstudiohdr + pmodeldata->normindex);
	const byte *pvertbone = (const byte *)m_pstudiohdr + pmodel->vertinfoindex;
	const byte *pnormbone = (const byte *)m_pstudiohdr + pmodel->norminfoindex;

	float s = 1.0f / (float)ptexture->width;
	float t = 1.0f / (float)ptexture->height;

	fprintf(smdfile, "%s\n", ptexture->name);

	for (int k = 0; k < 3; k++)
	{
		int vindex = ptrivert[k].vertindex;
		int nindex = ptrivert[k].normindex;

		if (vindex < 0 || vindex >= pmodel->numverts || vindex >= MAXSTUDIOVERTS)
			vindex = 0;
		if (nindex < 0 || nindex >= MAXSTUDIOVERTS)
			nindex = 0;

		int vertbone = pvertbone[vindex];
		int normbone = pnormbone[nindex];

		if (vertbone >= MAXSTUDIOBONES)
			vertbone = 0;
		if (normbone >= MAXSTUDIOBONES)
			normbone = 0;

		vec3_t vertex;
		vec3_t normal;
		VectorTransform(pverts[vindex], g_bonetransform[vertbone], vertex);
		VectorRotate(pnorms[nindex], g_normtransform[normbone], normal);
		VectorNormalize(normal);

		// the smd's t runs bottom up
		fprintf(smdfile, "%3i %f %f %f %f %f %f %f %f\n", vertbone, vertex[0], vertex[1], vertex[2], normal[0], normal[1], normal[2], ptrivert[k].s * s, 1.0f - (ptrivert[k].t * t));
	}
}

void CMDLDecompiler::SMD_GenerateSequences()
{
	if (!m_pstudiohdr)
		return;

	for (int i = 0; i < m_pstudiohdr->numseq; i++)
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

		for (int frame = 0; frame < pseqdesc->numframes; frame++)
		{
			SMD_WriteFrame(smdfile, frame, pseqdesc);
		}

		fprintf(smdfile, "end\n");
		fclose(smdfile);
		LogMessage(MDLDEC_MSG_INFO, "Sequence: %s\r\n", smdfilename);
	}
}

void CMDLDecompiler::SMD_WriteFrame(FILE *smdfile, int frame, const mstudioseqdesc_t *pseqdesc)
{
	std::vector<AlphaBonePose> pose;
	if (!CalcSequencePose(pseqdesc, (float)frame, pose))
		return;

	// put the extracted linear movement back on the motion bone,
	// so StudioMDL can extract it again
	if (pseqdesc->numframes > 1 && pseqdesc->motionbone >= 0 && pseqdesc->motionbone < m_pstudiohdr->numbones)
	{
		float t = (float)frame / (float)(pseqdesc->numframes - 1);
		AlphaBonePose &motionPose = pose[(size_t)pseqdesc->motionbone];

		if (pseqdesc->motiontype & STUDIO_LX)
			motionPose.pos[0] += pseqdesc->linearmovement[0] * t;
		if (pseqdesc->motiontype & STUDIO_LY)
			motionPose.pos[1] += pseqdesc->linearmovement[1] * t;
		if (pseqdesc->motiontype & STUDIO_LZ)
			motionPose.pos[2] += pseqdesc->linearmovement[2] * t;
	}

	fprintf(smdfile, "time %i\n", frame);
	for (int i = 0; i < m_pstudiohdr->numbones; i++)
	{
		const AlphaBonePose &p = pose[(size_t)i];
		fprintf(smdfile, "%3i %f %f %f %f %f %f\n", i, p.pos[0], p.pos[1], p.pos[2], DegToRad(p.ang[0]), DegToRad(p.ang[1]), DegToRad(p.ang[2]));
	}
}

void CMDLDecompiler::SMD_WriteNodes(FILE *smdfile)
{
	if (!m_pstudiohdr)
		return;

	const mstudiobone_t *pbones = (const mstudiobone_t *)((byte *)m_pstudiohdr + m_pstudiohdr->boneindex);

	fprintf(smdfile, "nodes\n");
	for (int i = 0; i < m_pstudiohdr->numbones; i++)
	{
		char boneName[64];
		GetBoneName(i, boneName, sizeof(boneName));
		fprintf(smdfile, "%3i \"%s\" %i\n", i, boneName, pbones[i].parent);
	}
	fprintf(smdfile, "end\n");
}
