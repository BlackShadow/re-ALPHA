/***
*
*	Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
#include "quakedef.h"

model_t *g_aliasmodel;
aliashdr_t *g_paliashdr;

int g_commands[8192];
int g_numcommands;

int g_vertexorder[8192];
int g_numorder;

int g_stverts[3 * 1024];

#define MAX_ALIAS_TRIS 8192
mtriangle_t g_triangles[MAX_ALIAS_TRIS];

int g_used[MAX_ALIAS_TRIS];
int g_stripverts[1024];
int g_striptris[1024];
int g_stripcount;

int g_numverts;
int g_numtris;

extern trivertx_t	*g_poseverts[256];

static int FanLength(int starttri, int startv);
static int StripLength(int starttri, int startv);
static int BuildGLPolyFromEdges(void);

static int FanLength(int starttri, int startv)
{
	int fixed;
	int last;
	int j;
	int k;

	g_used[starttri] = 2;

	fixed = g_triangles[starttri].vertindex[startv % 3];
	g_stripverts[0] = fixed;
	g_stripverts[1] = g_triangles[starttri].vertindex[(startv + 1) % 3];
	last = g_triangles[starttri].vertindex[(startv + 2) % 3];
	g_stripverts[2] = last;

	g_striptris[0] = starttri;
	g_stripcount = 1;

	for (int i = 1;; i++)
	{
		if (g_paliashdr->numtris <= starttri + 1)
			break;

		for (j = starttri + 1; j < g_paliashdr->numtris; j++)
		{
			mtriangle_t *check = &g_triangles[j];
			if (check->facesfront != g_triangles[starttri].facesfront)
				continue;

			for (k = 0; k < 3; k++)
			{
				if (check->vertindex[k] != fixed)
					continue;
				if (check->vertindex[(k + 1) % 3] != last)
					continue;

				if (g_used[j])
					goto done;

				last = check->vertindex[(k + 2) % 3];
				g_stripverts[i + 2] = last;
				g_striptris[i] = j;
				++g_stripcount;
				g_used[j] = 2;
				goto nexttri;
			}
		}

	done:
		break;

	nexttri:
		;
	}

	for (j = starttri + 1; j < g_paliashdr->numtris; j++)
	{
		if (g_used[j] == 2)
			g_used[j] = 0;
	}

	return g_stripcount;
}

static int StripLength(int starttri, int startv)
{
	int m1;
	int m2;
	int j;
	int k;

	g_used[starttri] = 2;

	g_stripverts[0] = g_triangles[starttri].vertindex[startv % 3];
	g_stripverts[1] = g_triangles[starttri].vertindex[(startv + 1) % 3];
	g_stripverts[2] = g_triangles[starttri].vertindex[(startv + 2) % 3];

	g_striptris[0] = starttri;
	g_stripcount = 1;

	m1 = g_stripverts[2];
	m2 = g_stripverts[1];

	for (int i = 1;; i++)
	{
		if (g_paliashdr->numtris <= starttri + 1)
			break;

		for (j = starttri + 1; j < g_paliashdr->numtris; j++)
		{
			mtriangle_t *check = &g_triangles[j];
			if (check->facesfront != g_triangles[starttri].facesfront)
				continue;

			for (k = 0; k < 3; k++)
			{
				if (check->vertindex[k] != m1)
					continue;
				if (check->vertindex[(k + 1) % 3] != m2)
					continue;

				if (g_used[j])
					goto done;

				if (g_stripcount & 1)
					m2 = check->vertindex[(k + 2) % 3];
				else
					m1 = check->vertindex[(k + 2) % 3];

				g_stripverts[i + 2] = check->vertindex[(k + 2) % 3];
				g_striptris[i] = j;
				++g_stripcount;
				g_used[j] = 2;
				goto nexttri;
			}
		}

	done:
		break;

	nexttri:
		;
	}

	for (j = starttri + 1; j < g_paliashdr->numtris; j++)
	{
		if (g_used[j] == 2)
			g_used[j] = 0;
	}

	return g_stripcount;
}

static int BuildGLPolyFromEdges(void)
{
	int best_len;
	int best_type;
	int best_verts[1024];
	int best_tris[1024];
	int i;

	g_numorder = 0;
	g_numcommands = 0;
	memset(g_used, 0, sizeof(g_used));

	for (i = 0; i < g_paliashdr->numtris; i++)
	{
		if (g_used[i])
			continue;

		best_len = 0;
		best_type = 0;

		for (int type = 0; type < 2; type++)
		{
			for (int startv = 0; startv < 3; startv++)
			{
				int len = (type == 1) ? StripLength(i, startv) : FanLength(i, startv);
				if (best_len < len)
				{
					best_len = len;
					best_type = type;

					if (len + 2 > 0)
						memcpy(best_verts, g_stripverts, sizeof(int) * (len + 2));
					if (len > 0)
						memcpy(best_tris, g_striptris, sizeof(int) * len);
				}
			}
		}

		for (int j = 0; j < best_len; j++)
			g_used[best_tris[j]] = 1;

		if (best_type == 1)
			g_commands[g_numcommands++] = best_len + 2;
		else
			g_commands[g_numcommands++] = -2 - best_len;

		{
			int facesfront = g_triangles[best_tris[0]].facesfront;
			int count = best_len + 2;

			for (int j = 0; j < count; j++)
			{
				int vertindex = best_verts[j];
				float s = (float)g_stverts[3 * vertindex + 1];
				float t = (float)g_stverts[3 * vertindex + 2];

				g_vertexorder[g_numorder++] = vertindex;

				if (!facesfront && g_stverts[3 * vertindex + 0])
					s += (float)(g_paliashdr->skinwidth / 2);

				s = (s + 0.5f) / (float)g_paliashdr->skinwidth;
				t = (t + 0.5f) / (float)g_paliashdr->skinheight;

				*(float *)&g_commands[g_numcommands++] = s;
				*(float *)&g_commands[g_numcommands++] = t;
			}
		}
	}

	*(float *)&g_commands[g_numcommands++] = 0.0f;

	Con_Printf("%3i tri %3i vert %3i cmd\n",
		g_paliashdr->numtris,
		g_numorder,
		g_numcommands);

	g_numverts += g_numorder;
	g_numtris += g_paliashdr->numtris;

	return g_paliashdr->numtris;
}

void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *hdr)
{
	FILE *f;
	char cache[64];
	char fullpath[128];
	int *cmds;
	trivertx_t *verts;
	int i;

	g_aliasmodel = m;
	g_paliashdr = hdr;

	strcpy(cache, "glquake/");
	COM_StripExtension(m->name + 6, &cache[8]);
	strcat(cache, ".ms2");

	COM_FOpenFile(cache, &f);
	if (f)
	{
		fread(&g_numcommands, 4u, 1u, f);
		fread(&g_numorder, 4u, 1u, f);
		fread(g_commands, 4 * g_numcommands, 1u, f);
		fread(g_vertexorder, 4 * g_numorder, 1u, f);
		fclose(f);
	}
	else
	{
		Con_Printf("meshing %s...\n", m->name);
		BuildGLPolyFromEdges();

		sprintf(fullpath, "%s/%s", com_gamedir, cache);
		f = fopen(fullpath, "wb");
		if (f)
		{
			fwrite(&g_numcommands, 4u, 1u, f);
			fwrite(&g_numorder, 4u, 1u, f);
			fwrite(g_commands, 4 * g_numcommands, 1u, f);
			fwrite(g_vertexorder, 4 * g_numorder, 1u, f);
			fclose(f);
		}
	}

	hdr->poseverts = g_numorder;

	cmds = (int *)Hunk_Alloc(4 * g_numcommands);
	hdr->commands = (int)((byte *)cmds - (byte *)hdr);
	memcpy(cmds, g_commands, 4 * g_numcommands);

	verts = (trivertx_t *)Hunk_Alloc(4 * hdr->numposes * hdr->poseverts);
	hdr->posedata = (int)((byte *)verts - (byte *)hdr);

	for (i = 0; i < hdr->numposes; i++)
	{
		for (int j = 0; j < g_numorder; j++)
		{
			int vertindex = g_vertexorder[j];
			*verts++ = g_poseverts[i][vertindex];
		}
	}
}

