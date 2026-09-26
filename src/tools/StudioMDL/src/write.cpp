/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
****/

//
// write.cpp: writes a Half-Life Alpha (version 6) studio .mdl file
//

#pragma warning(disable : 4244)
#pragma warning(disable : 4237)
#pragma warning(disable : 4305)

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "cmdlib.h"
#include "mathlib.h"
#define Vector vec3_t
#include "studio.h"
#include "studiomdl.h"

int totalframes = 0;
float totalseconds = 0;

byte* pData;
byte* pStart;
studiohdr_t* phdr;

#define FILEBUFFER (64 * 1024 * 1024)
#define ALIGN4(a) a = (byte*)(((uintptr_t)(a) + 3) & ~(uintptr_t)3)

// differences below these store a single key frame for the bone
#define POS_EPSILON 0.0001f
#define ROT_EPSILON 0.00001f

/*
============
StringCopy

Copies a string, truncated and terminated to fit dest
============
*/
static void StringCopy(char* dest, size_t size, const char* src)
{
	strncpy(dest, src, size - 1);
	dest[size - 1] = '\0';
}

/*
============
AngleToKey

Converts a rotation in radians to hundredths of a degree
============
*/
static short AngleToKey(float radians)
{
	float value = radians * (180.0f / Q_PI) * ROTKEY_SCALE;

	if (value > SHRT_MAX)
		value = SHRT_MAX;
	if (value < SHRT_MIN)
		value = SHRT_MIN;

	return (short)Q_rint(value);
}

/*
============
IsConstantTrack

True when no frame of the track moves further than epsilon from the first
============
*/
static bool IsConstantTrack(vec3_t* track, int numframes, float epsilon)
{
	if (!track || numframes <= 1)
		return true;

	for (int i = 1; i < numframes; i++)
	{
		if (fabs(track[i][0] - track[0][0]) > epsilon ||
			fabs(track[i][1] - track[0][1]) > epsilon ||
			fabs(track[i][2] - track[0][2]) > epsilon)
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
	mstudioattachment_t* pattachment;

	// save bone info
	pbone = (mstudiobone_t*)pData;
	phdr->numbones = numbones;
	phdr->boneindex = (pData - pStart);

	for (i = 0; i < numbones; i++)
	{
		memset(&pbone[i], 0, sizeof(mstudiobone_t));
		StringCopy(pbone[i].name, sizeof(pbone[i].name), bonetable[i].name);
		pbone[i].parent = bonetable[i].parent;
	}
	pData += numbones * sizeof(mstudiobone_t);
	ALIGN4(pData);

	// save bonecontroller info
	pbonecontroller = (mstudiobonecontroller_t*)pData;
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

	// save attachment info
	if (numattachments > 0)
	{
		pattachment = (mstudioattachment_t*)pData;
		phdr->numattachments = numattachments;
		phdr->attachmentindex = (pData - pStart);

		for (i = 0; i < numattachments; i++)
		{
			pattachment[i].bone = attachment[i].bone;
			VectorCopy(attachment[i].org, pattachment[i].org);
		}
		pData += numattachments * sizeof(mstudioattachment_t);
		ALIGN4(pData);
	}
}

void WriteSequenceInfo()
{
	int i, j;
	mstudioseqdesc_t* pseqdesc;
	mstudioevent_t* pevent;
	mstudiofullevent_t* pfullevent;

	// save sequence info
	pseqdesc = (mstudioseqdesc_t*)pData;
	phdr->numseq = numseq;
	phdr->seqindex = (pData - pStart);
	pData += numseq * sizeof(mstudioseqdesc_t);
	ALIGN4(pData);

	for (i = 0; i < numseq; i++)
	{
		memset(&pseqdesc[i], 0, sizeof(mstudioseqdesc_t));
		StringCopy(pseqdesc[i].label, sizeof(pseqdesc[i].label), sequence[i].name);
		pseqdesc[i].numframes = sequence[i].numframes;
		pseqdesc[i].fps = sequence[i].fps;
		pseqdesc[i].flags = sequence[i].flags;

		pseqdesc[i].motiontype = sequence[i].motiontype;
		VectorCopy(sequence[i].linearmovement, pseqdesc[i].linearmovement);

		pseqdesc[i].numblends = sequence[i].numblends;
		pseqdesc[i].animindex = sequence[i].animindex;
		pseqdesc[i].animindex2 = sequence[i].animindex;

		totalframes += sequence[i].numframes;
		totalseconds += sequence[i].numframes / sequence[i].fps;

		// save events
		pseqdesc[i].numevents = sequence[i].numevents;
		pseqdesc[i].eventindex = sequence[i].animindex;	 // left there when there are no events

		if (sequence[i].numevents == 0)
			continue;

		pevent = (mstudioevent_t*)pData;
		pseqdesc[i].eventindex = (pData - pStart);

		for (j = 0; j < sequence[i].numevents; j++)
		{
			int frame = sequence[i].event[j].frame - sequence[i].frameoffset;
			int event = sequence[i].event[j].event;

			if (frame > SHRT_MAX)
				frame = SHRT_MAX;
			else if (frame < SHRT_MIN)
				frame = SHRT_MIN;

			if (event < 0)
			{
				event = 0;
			}
			else if (event > UCHAR_MAX)
			{
				printf("WARNING: sequence \"%s\" event id %d truncated to 255 for legacy event stream.\n",
					sequence[i].name, sequence[i].event[j].event);
				event = UCHAR_MAX;
			}

			pevent[j].frame = frame;
			pevent[j].event = event;
			pevent[j].type = 0;
		}
		pData += sequence[i].numevents * sizeof(mstudioevent_t);
		ALIGN4(pData);

		// the same events with their options
		pfullevent = (mstudiofullevent_t*)pData;
		pseqdesc[i].numfullevents = sequence[i].numevents;
		pseqdesc[i].fulleventindex = (pData - pStart);

		for (j = 0; j < sequence[i].numevents; j++)
		{
			pfullevent[j].frame = sequence[i].event[j].frame - sequence[i].frameoffset;
			pfullevent[j].event = sequence[i].event[j].event;
			pfullevent[j].type = 0;
			StringCopy(pfullevent[j].options, sizeof(pfullevent[j].options), sequence[i].event[j].options);
		}
		pData += sequence[i].numevents * sizeof(mstudiofullevent_t);
		ALIGN4(pData);
	}

	// save transition graph
	if (numxnodes > 0)
	{
		byte* ptransition = pData;
		phdr->numtransitions = numxnodes;
		phdr->transitionindex = (pData - pStart);

		for (i = 0; i < numxnodes; i++)
		{
			for (j = 0; j < numxnodes; j++)
			{
				*ptransition++ = xnode[i][j];
			}
		}
		pData = ptransition;
		ALIGN4(pData);
	}
}

byte* WriteAnimations(byte* pData, byte* pStart)
{
	int i, j, n;
	int numframes;
	s_animation_t* panim;
	mstudioboneanim_t* pboneanim;
	mstudioposkey_t* pposkey;
	mstudiorotkey_t* protkey;

	for (i = 0; i < numseq; i++)
	{
		// the alpha format plays the first blend only
		panim = sequence[i].panim[0];
		if (!panim)
			Error("sequence \"%s\" has no animation data", sequence[i].name);

		numframes = sequence[i].numframes;
		if (numframes <= 0)
		{
			numframes = panim->endframe - panim->startframe + 1;
			if (numframes <= 0)
				numframes = 1;
		}

		sequence[i].animindex = (pData - pStart);

		pboneanim = (mstudioboneanim_t*)pData;
		pData += numbones * sizeof(mstudioboneanim_t);
		ALIGN4(pData);

		for (j = 0; j < numbones; j++)
		{
			vec3_t* ppos = NULL;
			vec3_t* prot = NULL;

			// bones the animation doesn't have keep their default position
			int k = panim->boneimap[j];
			if (k >= 0 && k < panim->numbones)
			{
				ppos = panim->pos[k];
				prot = panim->rot[k];
			}

			// position keys
			pboneanim[j].numposkeys = IsConstantTrack(ppos, numframes, POS_EPSILON) ? 1 : numframes;
			pboneanim[j].poskeyindex = (pData - pStart);

			pposkey = (mstudioposkey_t*)pData;
			for (n = 0; n < pboneanim[j].numposkeys; n++)
			{
				pposkey[n].frame = n;
				pposkey[n].unused = 0;
				VectorCopy(ppos ? ppos[n] : bonetable[j].pos, pposkey[n].pos);
			}
			pData += pboneanim[j].numposkeys * sizeof(mstudioposkey_t);
			ALIGN4(pData);

			// rotation keys
			pboneanim[j].numrotkeys = IsConstantTrack(prot, numframes, ROT_EPSILON) ? 1 : numframes;
			pboneanim[j].rotkeyindex = (pData - pStart);

			protkey = (mstudiorotkey_t*)pData;
			for (n = 0; n < pboneanim[j].numrotkeys; n++)
			{
				float* rot = prot ? prot[n] : bonetable[j].rot;

				protkey[n].frame = n;
				protkey[n].roll = AngleToKey(rot[2]);
				protkey[n].pitch = AngleToKey(rot[0]);
				protkey[n].yaw = AngleToKey(rot[1]);
			}
			pData += pboneanim[j].numrotkeys * sizeof(mstudiorotkey_t);
			ALIGN4(pData);
		}
	}

	return pData;
}

void WriteTextures()
{
	int i, j;
	mstudiotexture_t* ptexture;
	short* pref;

	// save bone info
	ptexture = (mstudiotexture_t*)pData;
	phdr->numtextures = numtextures;
	phdr->textureindex = (pData - pStart);
	pData += numtextures * sizeof(mstudiotexture_t);
	ALIGN4(pData);

	phdr->skinindex = (pData - pStart);
	phdr->numskinref = numskinref;
	phdr->numskinfamilies = numskinfamilies;
	pref = (short*)pData;

	for (i = 0; i < phdr->numskinfamilies; i++)
	{
		for (j = 0; j < phdr->numskinref; j++)
		{
			*pref = skinref[i][j];
			pref++;
		}
	}
	pData = (byte*)pref;
	ALIGN4(pData);

	phdr->texturedataindex = (pData - pStart);

	for (i = 0; i < numtextures; i++)
	{
		memset(&ptexture[i], 0, sizeof(mstudiotexture_t));
		StringCopy(ptexture[i].name, sizeof(ptexture[i].name), texture[i].name);
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
	int cur = 0;

	mstudiobodyparts_t* pbodypart;
	mstudiomodel_t* pmodel;
	mstudiomodeldata_t* pmodeldata;

	pbodypart = (mstudiobodyparts_t*)pData;
	phdr->numbodyparts = numbodyparts;
	phdr->bodypartindex = (pData - pStart);
	pData += numbodyparts * sizeof(mstudiobodyparts_t);

	pmodel = (mstudiomodel_t*)pData;
	pData += nummodels * sizeof(mstudiomodel_t);

	pmodeldata = (mstudiomodeldata_t*)pData;
	pData += nummodels * sizeof(mstudiomodeldata_t);
	ALIGN4(pData);

	for (i = 0; i < numbodyparts; i++)
	{
		memset(&pbodypart[i], 0, sizeof(mstudiobodyparts_t));
		StringCopy(pbodypart[i].name, sizeof(pbodypart[i].name), bodypart[i].name);
		pbodypart[i].nummodels = bodypart[i].nummodels;
		pbodypart[i].base = bodypart[i].base;
		pbodypart[i].modelindex = ((byte*)&pmodel[cur]) - pStart;
		cur += bodypart[i].nummodels;
	}

	for (i = 0; i < nummodels; i++)
	{
		int n = 0;
		int totaltris = 0;
		int normmap[MAXSTUDIOVERTS];
		int normimap[MAXSTUDIOVERTS];
		int meshnormindex[MAXSTUDIOMESHES];
		byte* pbone;
		vec3_t* pvert;
		vec3_t* pnorm;
		mstudiomesh_t* pmesh;

		memset(normmap, 0, sizeof(normmap));
		memset(normimap, 0, sizeof(normimap));
		memset(meshnormindex, 0, sizeof(meshnormindex));

		memset(&pmodel[i], 0, sizeof(mstudiomodel_t));
		memset(&pmodeldata[i], 0, sizeof(mstudiomodeldata_t));

		StringCopy(pmodel[i].name, sizeof(pmodel[i].name), model[i]->name);
		pmodel[i].type = STUDIO_HAS_NORMALS | STUDIO_HAS_VERTICES;
		pmodel[i].boundingradius = model[i]->boundingradius;
		pmodel[i].nummesh = model[i]->nummesh;
		pmodel[i].numverts = model[i]->numverts;
		pmodel[i].modeldataindex = ((byte*)&pmodeldata[i]) - pStart;

		// sort the normals by mesh
		for (j = 0; j < model[i]->nummesh; j++)
		{
			model[i]->pmesh[j]->numnorms = 0;
		}

		for (j = 0; j < model[i]->nummesh; j++)
		{
			meshnormindex[j] = n;
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

		// save vertice bones
		pbone = pData;
		pmodel[i].vertinfoindex = (pData - pStart);
		for (j = 0; j < pmodel[i].numverts; j++)
		{
			*pbone++ = model[i]->vert[j].bone;
		}
		ALIGN4(pbone);

		// save normal bones
		pmodel[i].norminfoindex = (pbone - pStart);
		for (j = 0; j < n; j++)
		{
			*pbone++ = model[i]->normal[normimap[j]].bone;
		}
		ALIGN4(pbone);

		pData = pbone;

		// save group info
		pvert = (vec3_t*)pData;
		pmodeldata[i].vertindex = (pData - pStart);
		pData += model[i]->numverts * sizeof(vec3_t);
		ALIGN4(pData);

		pnorm = (vec3_t*)pData;
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

		// save mesh info
		pmesh = (mstudiomesh_t*)pData;
		pmodel[i].meshindex = (pData - pStart);
		pData += pmodel[i].nummesh * sizeof(mstudiomesh_t);
		ALIGN4(pData);

		for (j = 0; j < model[i]->nummesh; j++)
		{
			s_mesh_t* psrcmesh = model[i]->pmesh[j];
			mstudiotrivert_t* ptrivert;

			memset(&pmesh[j], 0, sizeof(mstudiomesh_t));
			pmesh[j].numtris = psrcmesh->numtris;
			pmesh[j].skinref = psrcmesh->skinref;
			pmesh[j].numnorms = psrcmesh->numnorms;
			pmesh[j].normindex = meshnormindex[j];
			pmesh[j].triindex = (pData - pStart);

			ptrivert = (mstudiotrivert_t*)pData;
			for (k = 0; k < pmesh[j].numtris; k++)
			{
				for (int v = 0; v < 3; v++)
				{
					s_trianglevert_t* psrc = &psrcmesh->triangle[k][v];

					ptrivert->vertindex = psrc->vertindex;
					ptrivert->normindex = normmap[psrc->normindex];
					ptrivert->s = psrc->s;
					ptrivert->t = psrc->t;
					ptrivert++;
				}
			}
			pData = (byte*)ptrivert;
			ALIGN4(pData);

			totaltris += pmesh[j].numtris;
		}

		printf("model %-16s %6d tris\n", pmodel[i].name, totaltris);
	}
}

void WriteFile(void)
{
	FILE* modelouthandle;
	int total = 0;
	char filename[1024];

	pStart = (byte*)kalloc(1, FILEBUFFER);

	StripExtension(outname);
	sprintf(filename, "%s.mdl", outname);

	if (numseqgroups > 1)
		printf("WARNING: HL Alpha v6 does not use external sequence groups; embedding all sequences in %s.\n", filename);

	if (split_textures)
	{
		printf("WARNING: HL Alpha mode embeds textures in the main MDL; ignoring split texture output.\n");
		split_textures = 0;
	}

	printf("---------------------\n");
	printf("writing %s:\n", filename);
	modelouthandle = SafeOpenWrite(filename);

	phdr = (studiohdr_t*)pStart;
	memset(phdr, 0, sizeof(studiohdr_t));

	phdr->id = IDSTUDIOHEADER;
	phdr->version = STUDIO_VERSION;
	StringCopy(phdr->name, sizeof(phdr->name), filename);

	pData = (byte*)phdr + sizeof(studiohdr_t);

	WriteBoneInfo();
	printf("bones     %6d bytes (%d)\n", (int)(pData - pStart - total), numbones);
	total = pData - pStart;

	pData = WriteAnimations(pData, pStart);

	WriteSequenceInfo();
	printf("sequences %6d bytes (%d frames) [%d:%02d]\n", (int)(pData - pStart - total), totalframes, (int)totalseconds / 60, (int)totalseconds % 60);
	total = pData - pStart;

	WriteModel();
	printf("models    %6d bytes\n", (int)(pData - pStart - total));
	total = pData - pStart;

	WriteTextures();
	printf("textures  %6d bytes\n", (int)(pData - pStart - total));

	phdr->length = pData - pStart;
	if (phdr->length > FILEBUFFER)
		Error("model output exceeds FILEBUFFER (%d > %d)", phdr->length, FILEBUFFER);

	printf("total     %6d\n", phdr->length);

	SafeWrite(modelouthandle, pStart, phdr->length);

	fclose(modelouthandle);
}
