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
// gl_mesh.c: triangle model functions

#include "quakedef.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

#define MAX_ALIAS_TRIS	8192
#define MAX_COMMANDS	8192
#define MAX_STRIP		1024

model_t		*g_aliasmodel;
aliashdr_t	*g_paliashdr;

int			g_commands[MAX_COMMANDS];
int			g_numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int			g_vertexorder[MAX_COMMANDS];
int			g_numorder;

int			g_stverts[3 * MAXALIASVERTS];	// onseam, s, t
mtriangle_t	g_triangles[MAX_ALIAS_TRIS];

int			g_used[MAX_ALIAS_TRIS];

int			g_stripverts[MAX_STRIP];
int			g_striptris[MAX_STRIP];
int			g_stripcount;

int			g_numverts;
int			g_numtris;

/*
================
FanLength
================
*/
static int FanLength(int starttri, int startv)
{
	int		m1, m2;
	int		j;
	int		k;

	g_used[starttri] = 2;

	m1 = g_triangles[starttri].vertindex[startv % 3];
	g_stripverts[0] = m1;
	g_stripverts[1] = g_triangles[starttri].vertindex[(startv + 1) % 3];
	m2 = g_triangles[starttri].vertindex[(startv + 2) % 3];
	g_stripverts[2] = m2;

	g_striptris[0] = starttri;
	g_stripcount = 1;

	// look for a matching triangle
	for (int i = 1;; i++)
	{
		if (g_paliashdr->numtris <= starttri + 1)
			break;

		for (j = starttri + 1; j < g_paliashdr->numtris; j++)
		{
			mtriangle_t	*check = &g_triangles[j];

			if (check->facesfront != g_triangles[starttri].facesfront)
				continue;

			for (k = 0; k < 3; k++)
			{
				if (check->vertindex[k] != m1)
					continue;
				if (check->vertindex[(k + 1) % 3] != m2)
					continue;

				// this is the next part of the fan or strip

				// if we can't use this triangle, this tristrip is done
				if (g_used[j])
					goto done;

				m2 = check->vertindex[(k + 2) % 3];
				g_stripverts[i + 2] = m2;
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

	// clear the temp used flags
	for (j = starttri + 1; j < g_paliashdr->numtris; j++)
	{
		if (g_used[j] == 2)
			g_used[j] = 0;
	}

	return g_stripcount;
}

/*
================
StripLength
================
*/
static int StripLength(int starttri, int startv)
{
	int		m1, m2;
	int		j;
	int		k;

	g_used[starttri] = 2;

	g_stripverts[0] = g_triangles[starttri].vertindex[startv % 3];
	g_stripverts[1] = g_triangles[starttri].vertindex[(startv + 1) % 3];
	g_stripverts[2] = g_triangles[starttri].vertindex[(startv + 2) % 3];

	g_striptris[0] = starttri;
	g_stripcount = 1;

	m1 = g_stripverts[2];
	m2 = g_stripverts[1];

	// look for a matching triangle
	for (int i = 1;; i++)
	{
		if (g_paliashdr->numtris <= starttri + 1)
			break;

		for (j = starttri + 1; j < g_paliashdr->numtris; j++)
		{
			mtriangle_t	*check = &g_triangles[j];

			if (check->facesfront != g_triangles[starttri].facesfront)
				continue;

			for (k = 0; k < 3; k++)
			{
				if (check->vertindex[k] != m1)
					continue;
				if (check->vertindex[(k + 1) % 3] != m2)
					continue;

				// this is the next part of the fan or strip

				// if we can't use this triangle, this tristrip is done
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

	// clear the temp used flags
	for (j = starttri + 1; j < g_paliashdr->numtris; j++)
	{
		if (g_used[j] == 2)
			g_used[j] = 0;
	}

	return g_stripcount;
}

/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
static void BuildTris(void)
{
	int		i;
	int		bestlen;
	int		besttype;
	int		bestverts[MAX_STRIP];
	int		besttris[MAX_STRIP];

	//
	// build tristrips
	//
	g_numorder = 0;
	g_numcommands = 0;
	memset(g_used, 0, sizeof(g_used));

	for (i = 0; i < g_paliashdr->numtris; i++)
	{
		// pick an unused triangle and start the trifan
		if (g_used[i])
			continue;

		bestlen = 0;
		besttype = 0;

		for (int type = 0; type < 2; type++)
		{
			for (int startv = 0; startv < 3; startv++)
			{
				int len = (type == 1) ? StripLength(i, startv) : FanLength(i, startv);

				if (bestlen < len)
				{
					bestlen = len;
					besttype = type;

					memcpy(bestverts, g_stripverts, sizeof(int) * (len + 2));
					memcpy(besttris, g_striptris, sizeof(int) * len);
				}
			}
		}

		// mark the tris on the best strip as used
		for (int j = 0; j < bestlen; j++)
			g_used[besttris[j]] = 1;

		if (besttype == 1)
			g_commands[g_numcommands++] = bestlen + 2;
		else
			g_commands[g_numcommands++] = -2 - bestlen;

		{
			int facesfront = g_triangles[besttris[0]].facesfront;
			int count = bestlen + 2;

			for (int j = 0; j < count; j++)
			{
				int		k = bestverts[j];
				float	s = (float)g_stverts[3 * k + 1];
				float	t = (float)g_stverts[3 * k + 2];

				// emit a vertex into the reorder buffer
				g_vertexorder[g_numorder++] = k;

				// emit s/t coords into the commands stream
				if (!facesfront && g_stverts[3 * k + 0])
					s += (float)(g_paliashdr->skinwidth / 2);	// on back side

				s = (s + 0.5f) / (float)g_paliashdr->skinwidth;
				t = (t + 0.5f) / (float)g_paliashdr->skinheight;

				*(float *)&g_commands[g_numcommands++] = s;
				*(float *)&g_commands[g_numcommands++] = t;
			}
		}
	}

	g_commands[g_numcommands++] = 0;		// end of list marker

	Con_Printf("%3i tri %3i vert %3i cmd\n", g_paliashdr->numtris, g_numorder, g_numcommands);

	g_numverts += g_numorder;
	g_numtris += g_paliashdr->numtris;
}

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *hdr)
{
	int			i;
	int			*cmds;
	trivertx_t	*verts;
	char		cache[64], fullpath[128];
	FILE		*f;

	g_aliasmodel = m;
	g_paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);

	//
	// look for a cached version
	//
	strcpy(cache, "glquake/");
	COM_StripExtension(m->name + strlen("progs/"), cache + strlen("glquake/"));
	strcat(cache, ".ms2");

	COM_FOpenFile(cache, &f);
	if (f)
	{
		fread(&g_numcommands, sizeof(g_numcommands), 1, f);
		fread(&g_numorder, sizeof(g_numorder), 1, f);
		fread(g_commands, g_numcommands * sizeof(g_commands[0]), 1, f);
		fread(g_vertexorder, g_numorder * sizeof(g_vertexorder[0]), 1, f);
		fclose(f);
	}
	else
	{
		//
		// build it from scratch
		//
		Con_Printf("meshing %s...\n", m->name);

		BuildTris();		// trifans or lists

		//
		// save out the cached version
		//
		sprintf(fullpath, "%s/%s", com_gamedir, cache);
		f = fopen(fullpath, "wb");
		if (f)
		{
			fwrite(&g_numcommands, sizeof(g_numcommands), 1, f);
			fwrite(&g_numorder, sizeof(g_numorder), 1, f);
			fwrite(g_commands, g_numcommands * sizeof(g_commands[0]), 1, f);
			fwrite(g_vertexorder, g_numorder * sizeof(g_vertexorder[0]), 1, f);
			fclose(f);
		}
	}

	// save the data out

	hdr->poseverts = g_numorder;

	cmds = Hunk_Alloc(g_numcommands * sizeof(int));
	hdr->commands = (byte *)cmds - (byte *)hdr;
	memcpy(cmds, g_commands, g_numcommands * sizeof(int));

	verts = Hunk_Alloc(hdr->numposes * hdr->poseverts * sizeof(trivertx_t));
	hdr->posedata = (byte *)verts - (byte *)hdr;

	for (i = 0; i < hdr->numposes; i++)
	{
		for (int j = 0; j < g_numorder; j++)
		{
			int k = g_vertexorder[j];
			*verts++ = g_poseverts[i][k];
		}
	}
}
