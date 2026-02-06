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
// write.cpp: Writes a Half-Life Alpha v6 studio .mdl file.
//

#pragma warning(disable : 4244)
#pragma warning(disable : 4237)
#pragma warning(disable : 4305)

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "cmdlib.h"
#define Vector vec3_t
#include "studio.h"
#include "studiomdl.h"

int totalframes = 0;
float totalseconds = 0;

byte* pData;
byte* pStart;
studiohdr_t* phdr;

#define FILEBUFFER (64 * 1024 * 1024)
#define ALIGN4(a) a = reinterpret_cast<byte*>((reinterpret_cast<uintptr_t>(a) + 3u) & ~uintptr_t(3u))

#pragma pack(push, 1)
typedef struct
{
	short frame;
	short pad;
	float x;
	float y;
	float z;
} alpha_pos_key_t;

typedef struct
{
	short frame;
	short roll;
	short pitch;
	short yaw;
} alpha_rot_key_t;

typedef struct
{
	int frame;
	int event;
	int type;
	char options[64];
} alpha_event_t;

typedef struct
{
	short frame;
	unsigned char event;
	unsigned char type;
} alpha_event_legacy_t;

typedef struct
{
	int bone;
	vec3_t org;
} alpha_attachment_t;
#pragma pack(pop)

static inline void WriteSeqInt(mstudioseqdesc_t* seq, size_t offset, int value)
{
	*reinterpret_cast<int*>(reinterpret_cast<byte*>(seq) + offset) = value;
}

static inline void WriteSeqFloat(mstudioseqdesc_t* seq, size_t offset, float value)
{
	*reinterpret_cast<float*>(reinterpret_cast<byte*>(seq) + offset) = value;
}

static short PackAngle100(float radians)
{
	float value = radians * (180.0f / Q_PI) * 100.0f;
	if (value > 32767.0f)
		value = 32767.0f;
	if (value < -32768.0f)
		value = -32768.0f;
	return static_cast<short>(Q_rint(value));
}

static void CopyStringTrunc(char* dst, size_t dstSize, const char* src)
{
	if (!dst || dstSize == 0)
		return;

	if (!src)
	{
		dst[0] = 0;
		return;
	}

	strncpy(dst, src, dstSize - 1);
	dst[dstSize - 1] = 0;
}

static qboolean IsConstantPosTrack(vec3_t* track, int frameCount)
{
	int i;
	if (!track || frameCount <= 1)
		return true;

	for (i = 1; i < frameCount; ++i)
	{
		if (fabs(track[i][0] - track[0][0]) > 0.0001f ||
			fabs(track[i][1] - track[0][1]) > 0.0001f ||
			fabs(track[i][2] - track[0][2]) > 0.0001f)
		{
			return false;
		}
	}
	return true;
}

static qboolean IsConstantRotTrack(vec3_t* track, int frameCount)
{
	int i;
	if (!track || frameCount <= 1)
		return true;

	for (i = 1; i < frameCount; ++i)
	{
		if (fabs(track[i][0] - track[0][0]) > 0.00001f ||
			fabs(track[i][1] - track[0][1]) > 0.00001f ||
			fabs(track[i][2] - track[0][2]) > 0.00001f)
		{
			return false;
		}
	}
	return true;
}

void WriteBoneInfo()
{
	int i;
	mstudiobone_t* pbone;
	mstudiobonecontroller_t* pbonecontroller;

	pbone = reinterpret_cast<mstudiobone_t*>(pData);
	phdr->numbones = numbones;
	phdr->boneindex = (pData - pStart);

	for (i = 0; i < numbones; i++)
	{
		memset(&pbone[i], 0, sizeof(mstudiobone_t));
		CopyStringTrunc(pbone[i].name, sizeof(pbone[i].name), bonetable[i].name);
		pbone[i].parent = bonetable[i].parent;
	}
	pData += numbones * sizeof(mstudiobone_t);
	ALIGN4(pData);

	pbonecontroller = reinterpret_cast<mstudiobonecontroller_t*>(pData);
	phdr->numbonecontrollers = numbonecontrollers;
	phdr->bonecontrollerindex = (pData - pStart);

	for (i = 0; i < numbonecontrollers; i++)
	{
		pbonecontroller[i].bone = bonecontroller[i].bone;
		pbonecontroller[i].type = bonecontroller[i].type;
		pbonecontroller[i].start = bonecontroller[i].start;
		pbonecontroller[i].end = bonecontroller[i].end;
	}
	pData += numbonecontrollers * sizeof(mstudiobonecontroller_t);
	ALIGN4(pData);

	if (numattachments > 0)
	{
		alpha_attachment_t* pattachment = reinterpret_cast<alpha_attachment_t*>(pData);
		phdr->numattachments = numattachments;
		phdr->attachmentindex = (pData - pStart);

		for (i = 0; i < numattachments; i++)
		{
			pattachment[i].bone = attachment[i].bone;
			VectorCopy(attachment[i].org, pattachment[i].org);
		}
		pData += numattachments * sizeof(alpha_attachment_t);
		ALIGN4(pData);
	}
	else
	{
		phdr->numattachments = 0;
		phdr->attachmentindex = 0;
	}

	phdr->soundtable = 0;
	phdr->soundindex = 0;
	phdr->soundgroups = 0;
	phdr->soundgroupindex = 0;
}

void WriteSequenceInfo()
{
	int i, j;
	mstudioseqdesc_t* pseqdesc;

	pseqdesc = reinterpret_cast<mstudioseqdesc_t*>(pData);
	phdr->numseq = numseq;
	phdr->seqindex = (pData - pStart);
	pData += numseq * sizeof(mstudioseqdesc_t);
	ALIGN4(pData);

	for (i = 0; i < numseq; i++)
	{
		int legacyEventCount = sequence[i].numevents;
		int legacyEventIndex = sequence[i].animindex;
		int modernEventCount = 0;
		int modernEventIndex = 0;

		memset(&pseqdesc[i], 0, sizeof(mstudioseqdesc_t));
		CopyStringTrunc(pseqdesc[i].label, sizeof(pseqdesc[i].label), sequence[i].name);
		pseqdesc[i].numframes = sequence[i].numframes;
		pseqdesc[i].fps = sequence[i].fps;
		pseqdesc[i].flags = sequence[i].flags;
		pseqdesc[i].activity = sequence[i].activity;
		pseqdesc[i].actweight = sequence[i].actweight;
		pseqdesc[i].motiontype = sequence[i].motiontype;
		pseqdesc[i].motionbone = 0;
		pseqdesc[i].animindex = sequence[i].animindex;

		totalframes += sequence[i].numframes;
		totalseconds += sequence[i].numframes / sequence[i].fps;

		if (sequence[i].numblends > 1)
		{
			printf("WARNING: sequence \"%s\" has %d blends; HL Alpha keeps only the first blend.\n",
				sequence[i].name, sequence[i].numblends);
		}

		if (sequence[i].numevents > 0)
		{
			alpha_event_legacy_t* legacyEvents = reinterpret_cast<alpha_event_legacy_t*>(pData);
			legacyEventIndex = (pData - pStart);

			for (j = 0; j < sequence[i].numevents; j++)
			{
				int legacyFrame = sequence[i].event[j].frame - sequence[i].frameoffset;
				int legacyEvent = sequence[i].event[j].event;

				if (legacyFrame > 32767)
					legacyFrame = 32767;
				else if (legacyFrame < -32768)
					legacyFrame = -32768;

				if (legacyEvent < 0)
					legacyEvent = 0;
				else if (legacyEvent > 255)
				{
					printf("WARNING: sequence \"%s\" event id %d truncated to 255 for legacy event stream.\n",
						sequence[i].name,
						sequence[i].event[j].event);
					legacyEvent = 255;
				}

				legacyEvents[j].frame = static_cast<short>(legacyFrame);
				legacyEvents[j].event = static_cast<unsigned char>(legacyEvent);
				legacyEvents[j].type = 0;
			}

			pData += sequence[i].numevents * sizeof(alpha_event_legacy_t);
			ALIGN4(pData);

			alpha_event_t* pevent = reinterpret_cast<alpha_event_t*>(pData);
			modernEventCount = sequence[i].numevents;
			modernEventIndex = (pData - pStart);

			for (j = 0; j < sequence[i].numevents; j++)
			{
				pevent[j].frame = sequence[i].event[j].frame - sequence[i].frameoffset;
				pevent[j].event = sequence[i].event[j].event;
				pevent[j].type = 0;
				memset(pevent[j].options, 0, sizeof(pevent[j].options));
				CopyStringTrunc(pevent[j].options, sizeof(pevent[j].options), sequence[i].event[j].options);
			}

			pData += modernEventCount * sizeof(alpha_event_t);
			ALIGN4(pData);

			pseqdesc[i].numevents = modernEventCount;
			pseqdesc[i].eventindex = modernEventIndex;
		}

		// Legacy Alpha engines read sequence events/anim offsets from hardcoded fields.
		// Keep modern fields while mirroring the old layout for compatibility.
		WriteSeqInt(&pseqdesc[i], 40, legacyEventCount); // legacy numevents
		WriteSeqInt(&pseqdesc[i], 44, legacyEventIndex); // legacy eventindex (4-byte events)
		WriteSeqInt(&pseqdesc[i], 60, sequence[i].animindex); // legacy animindex
		WriteSeqFloat(&pseqdesc[i], 76, sequence[i].linearmovement[0]); // legacy linear movement X
		WriteSeqFloat(&pseqdesc[i], 80, sequence[i].linearmovement[1]); // legacy linear movement Y
		WriteSeqFloat(&pseqdesc[i], 84, sequence[i].linearmovement[2]); // legacy linear movement Z
		WriteSeqInt(&pseqdesc[i], 88, sequence[i].numblends > 0 ? sequence[i].numblends : 1); // legacy numblends
		WriteSeqInt(&pseqdesc[i], 92, sequence[i].animindex); // modern animindex
	}

	if (numxnodes > 0)
	{
		int row, col;
		byte* ptransition = reinterpret_cast<byte*>(pData);
		phdr->numtransitions = numxnodes;
		phdr->transitionindex = (pData - pStart);

		for (row = 0; row < numxnodes; ++row)
		{
			for (col = 0; col < numxnodes; ++col)
			{
				*ptransition++ = static_cast<byte>(xnode[row][col]);
			}
		}
		pData = ptransition;
		ALIGN4(pData);
	}
	else
	{
		phdr->numtransitions = 0;
		phdr->transitionindex = 0;
	}
}

byte* WriteAnimations(byte* pAnimData, byte* pAnimStart)
{
	int i, j, n;

	for (i = 0; i < numseq; i++)
	{
		int frameCount;
		int* pBoneTable;
		s_animation_t* panim;

		panim = sequence[i].panim[0];
		if (!panim)
		{
			Error("sequence \"%s\" has no animation data", sequence[i].name);
		}

		frameCount = sequence[i].numframes;
		if (frameCount <= 0)
		{
			frameCount = panim->endframe - panim->startframe + 1;
			if (frameCount <= 0)
				frameCount = 1;
		}

		sequence[i].animindex = (pAnimData - pAnimStart);

		pBoneTable = reinterpret_cast<int*>(pAnimData);
		pAnimData += numbones * 4 * sizeof(int);
		ALIGN4(pAnimData);

		for (j = 0; j < numbones; ++j)
		{
			int sourceBone;
			int posCount;
			int rotCount;
			vec3_t* posTrack = NULL;
			vec3_t* rotTrack = NULL;
			vec3_t* defaultPos = &bonetable[j].pos;
			vec3_t* defaultRot = &bonetable[j].rot;

			sourceBone = panim->boneimap[j];
			if (sourceBone >= 0 && sourceBone < panim->numbones)
			{
				posTrack = panim->pos[sourceBone];
				rotTrack = panim->rot[sourceBone];
			}

			posCount = IsConstantPosTrack(posTrack, frameCount) ? 1 : frameCount;
			pBoneTable[j * 4 + 0] = posCount;
			pBoneTable[j * 4 + 1] = (pAnimData - pAnimStart);

			for (n = 0; n < posCount; ++n)
			{
				int sourceFrame = (posCount == 1) ? 0 : n;
				vec3_t* source = posTrack ? &posTrack[sourceFrame] : defaultPos;
				alpha_pos_key_t* key = reinterpret_cast<alpha_pos_key_t*>(pAnimData);
				key->frame = static_cast<short>(sourceFrame);
				key->pad = 0;
				key->x = (*source)[0];
				key->y = (*source)[1];
				key->z = (*source)[2];
				pAnimData += sizeof(alpha_pos_key_t);
			}
			ALIGN4(pAnimData);

			rotCount = IsConstantRotTrack(rotTrack, frameCount) ? 1 : frameCount;
			pBoneTable[j * 4 + 2] = rotCount;
			pBoneTable[j * 4 + 3] = (pAnimData - pAnimStart);

			for (n = 0; n < rotCount; ++n)
			{
				int sourceFrame = (rotCount == 1) ? 0 : n;
				vec3_t* source = rotTrack ? &rotTrack[sourceFrame] : defaultRot;
				alpha_rot_key_t* key = reinterpret_cast<alpha_rot_key_t*>(pAnimData);

				// Decoder mapping in HL Alpha:
				// angle[0] <- key.pitch, angle[1] <- key.yaw, angle[2] <- key.roll
				key->frame = static_cast<short>(sourceFrame);
				key->roll = PackAngle100((*source)[2]);
				key->pitch = PackAngle100((*source)[0]);
				key->yaw = PackAngle100((*source)[1]);

				pAnimData += sizeof(alpha_rot_key_t);
			}
			ALIGN4(pAnimData);
		}
	}

	return pAnimData;
}

void WriteTextures()
{
	int i, j;
	mstudiotexture_t* ptexture;
	short* pref;

	ptexture = reinterpret_cast<mstudiotexture_t*>(pData);
	phdr->numtextures = numtextures;
	phdr->textureindex = (pData - pStart);
	pData += numtextures * sizeof(mstudiotexture_t);
	ALIGN4(pData);

	phdr->skinindex = (pData - pStart);
	phdr->numskinref = numskinref;
	phdr->numskinfamilies = numskinfamilies;
	pref = reinterpret_cast<short*>(pData);

	for (i = 0; i < phdr->numskinfamilies; i++)
	{
		for (j = 0; j < phdr->numskinref; j++)
		{
			*pref = static_cast<short>(skinref[i][j]);
			pref++;
		}
	}
	pData = reinterpret_cast<byte*>(pref);
	ALIGN4(pData);

	phdr->texturedataindex = (pData - pStart);

	for (i = 0; i < numtextures; i++)
	{
		memset(&ptexture[i], 0, sizeof(mstudiotexture_t));
		CopyStringTrunc(ptexture[i].name, sizeof(ptexture[i].name), texture[i].name);
		ptexture[i].flags = texture[i].flags;
		ptexture[i].width = texture[i].skinwidth;
		ptexture[i].height = texture[i].skinheight;
		ptexture[i].index = (pData - pStart);
		memcpy(pData, texture[i].pdata, texture[i].size);
		pData += texture[i].size;
	}
	ALIGN4(pData);
}

void WriteModel()
{
	int i, j, k;
	int modelIndex = 0;

	mstudiobodyparts_t* pbodypart;
	mstudiomodel_t* pmodel;
	mstudiomodeldata_t* pmodeldata;

	pbodypart = reinterpret_cast<mstudiobodyparts_t*>(pData);
	phdr->numbodyparts = numbodyparts;
	phdr->bodypartindex = (pData - pStart);
	pData += numbodyparts * sizeof(mstudiobodyparts_t);

	pmodel = reinterpret_cast<mstudiomodel_t*>(pData);
	pData += nummodels * sizeof(mstudiomodel_t);

	pmodeldata = reinterpret_cast<mstudiomodeldata_t*>(pData);
	pData += nummodels * sizeof(mstudiomodeldata_t);
	ALIGN4(pData);

	for (i = 0; i < numbodyparts; ++i)
	{
		memset(&pbodypart[i], 0, sizeof(mstudiobodyparts_t));
		CopyStringTrunc(pbodypart[i].name, sizeof(pbodypart[i].name), bodypart[i].name);
		pbodypart[i].nummodels = bodypart[i].nummodels;
		pbodypart[i].base = bodypart[i].base;
		pbodypart[i].modelindex = reinterpret_cast<byte*>(&pmodel[modelIndex]) - pStart;
		modelIndex += bodypart[i].nummodels;
	}

	for (i = 0; i < nummodels; i++)
	{
		int n = 0;
		int totalTris = 0;
		int normmap[MAXSTUDIOVERTS];
		int normimap[MAXSTUDIOVERTS];
		int meshNormStart[MAXSTUDIOMESHES];
		byte* pbone;
		vec3_t* pvert;
		vec3_t* pnorm;
		mstudiomesh_t* pmesh;

		memset(normmap, 0, sizeof(normmap));
		memset(normimap, 0, sizeof(normimap));
		memset(meshNormStart, 0, sizeof(meshNormStart));

		memset(&pmodel[i], 0, sizeof(mstudiomodel_t));
		memset(&pmodeldata[i], 0, sizeof(mstudiomodeldata_t));

		CopyStringTrunc(pmodel[i].name, sizeof(pmodel[i].name), model[i]->name);
		pmodel[i].type = STUDIO_HAS_NORMALS | STUDIO_HAS_VERTICES;
		pmodel[i].boundingradius = model[i]->boundingradius;
		pmodel[i].nummesh = model[i]->nummesh;
		pmodel[i].numverts = model[i]->numverts;
		pmodel[i].modeldataindex = reinterpret_cast<byte*>(&pmodeldata[i]) - pStart;

		for (j = 0; j < model[i]->nummesh; j++)
		{
			model[i]->pmesh[j]->numnorms = 0;
		}

		for (j = 0; j < model[i]->nummesh; j++)
		{
			meshNormStart[j] = n;
			for (k = 0; k < model[i]->numnorms; k++)
			{
				if (model[i]->normal[k].skinref == model[i]->pmesh[j]->skinref)
				{
					normmap[k] = n;
					normimap[n] = k;
					n++;
					model[i]->pmesh[j]->numnorms++;
				}
			}
		}

		pbone = pData;
		pmodel[i].vertinfoindex = (pData - pStart);
		for (j = 0; j < pmodel[i].numverts; j++)
		{
			*pbone++ = static_cast<byte>(model[i]->vert[j].bone);
		}
		ALIGN4(pbone);

		pmodel[i].norminfoindex = reinterpret_cast<byte*>(pbone) - pStart;
		for (j = 0; j < n; j++)
		{
			*pbone++ = static_cast<byte>(model[i]->normal[normimap[j]].bone);
		}
		ALIGN4(pbone);

		pData = reinterpret_cast<byte*>(pbone);

		pvert = reinterpret_cast<vec3_t*>(pData);
		pmodeldata[i].vertindex = (pData - pStart);
		pData += model[i]->numverts * sizeof(vec3_t);
		ALIGN4(pData);

		pnorm = reinterpret_cast<vec3_t*>(pData);
		pmodeldata[i].normindex = (pData - pStart);
		pData += n * sizeof(vec3_t);
		ALIGN4(pData);

		for (j = 0; j < model[i]->numverts; j++)
		{
			VectorCopy(model[i]->vert[j].org, pvert[j]);
		}

		for (j = 0; j < n; j++)
		{
			VectorCopy(model[i]->normal[normimap[j]].org, pnorm[j]);
		}

		pmesh = reinterpret_cast<mstudiomesh_t*>(pData);
		pmodel[i].meshindex = (pData - pStart);
		pData += pmodel[i].nummesh * sizeof(mstudiomesh_t);
		ALIGN4(pData);

		for (j = 0; j < model[i]->nummesh; j++)
		{
			int tri;
			short* triOut;
			s_trianglevert_t(*triangles)[3];

			memset(&pmesh[j], 0, sizeof(mstudiomesh_t));
			pmesh[j].numtris = model[i]->pmesh[j]->numtris;
			pmesh[j].skinref = model[i]->pmesh[j]->skinref;
			pmesh[j].numnorms = model[i]->pmesh[j]->numnorms;
			pmesh[j].normindex = meshNormStart[j];
			pmesh[j].triindex = (pData - pStart);

			triOut = reinterpret_cast<short*>(pData);
			triangles = model[i]->pmesh[j]->triangle;

			for (tri = 0; tri < pmesh[j].numtris; ++tri)
			{
				for (k = 0; k < 3; ++k)
				{
					const s_trianglevert_t* tv = &triangles[tri][k];
					int mappedNorm = normmap[tv->normindex];

					*triOut++ = static_cast<short>(tv->vertindex);
					*triOut++ = static_cast<short>(mappedNorm);
					*triOut++ = static_cast<short>(tv->s);
					*triOut++ = static_cast<short>(tv->t);
				}
			}

			pData = reinterpret_cast<byte*>(triOut);
			ALIGN4(pData);
			totalTris += pmesh[j].numtris;
		}

		printf("model %-16s %6d tris\n", pmodel[i].name, totalTris);
	}
}

void WriteFile(void)
{
	FILE* modelouthandle;
	int total = 0;
	char finalName[1024];

	pStart = reinterpret_cast<byte*>(kalloc(1, FILEBUFFER));

	StripExtension(outname);
	sprintf(finalName, "%s.mdl", outname);

	if (numseqgroups > 1)
	{
		printf("WARNING: HL Alpha v6 does not use external sequence groups; embedding all sequences in %s.\n",
			finalName);
	}

	if (split_textures)
	{
		printf("WARNING: HL Alpha mode embeds textures in the main MDL; ignoring split texture output.\n");
		split_textures = 0;
	}

	printf("---------------------\n");
	printf("writing %s:\n", finalName);
	modelouthandle = SafeOpenWrite(finalName);

	phdr = reinterpret_cast<studiohdr_t*>(pStart);
	memset(phdr, 0, sizeof(studiohdr_t));

	phdr->id = IDSTUDIOHEADER;
	phdr->version = STUDIO_VERSION;
	CopyStringTrunc(phdr->name, sizeof(phdr->name), finalName);

	pData = reinterpret_cast<byte*>(phdr) + sizeof(studiohdr_t);

	WriteBoneInfo();
	printf("bones     %6d bytes (%d)\n", static_cast<int>(pData - pStart - total), numbones);
	total = static_cast<int>(pData - pStart);

	pData = WriteAnimations(pData, pStart);

	WriteSequenceInfo();
	printf("sequences %6d bytes (%d frames) [%d:%02d]\n",
		static_cast<int>(pData - pStart - total),
		totalframes,
		(int)totalseconds / 60,
		(int)totalseconds % 60);
	total = static_cast<int>(pData - pStart);

	WriteModel();
	printf("models    %6d bytes\n", static_cast<int>(pData - pStart - total));
	total = static_cast<int>(pData - pStart);

	WriteTextures();
	printf("textures  %6d bytes\n", static_cast<int>(pData - pStart - total));

	phdr->length = pData - pStart;
	if (phdr->length > FILEBUFFER)
	{
		Error("model output exceeds FILEBUFFER (%d > %d)", phdr->length, FILEBUFFER);
	}

	printf("total     %6d\n", phdr->length);

	SafeWrite(modelouthandle, pStart, phdr->length);

	fclose(modelouthandle);
}
