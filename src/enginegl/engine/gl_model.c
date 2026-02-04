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
#include "studio.h"

void Mod_LoadAliasModel(model_t *mod, void *buffer);
void Mod_LoadBrushModel(model_t *mod, void *buffer);
void Mod_LoadSpriteModel(model_t *mod, void *buffer);
void Mod_LoadStudioModel(model_t *mod, void *buffer);
void GL_SubdivideSurface(msurface_t *fa);

model_t *loadmodel;
char loadname[32];
byte mod_novis[MAX_MAP_LEAFS / 8];
byte decompressed[MAX_MAP_LEAFS / 8];
byte *mod_base;

#define MAX_MOD_KNOWN 512
model_t mod_known[MAX_MOD_KNOWN];
int mod_numknown;

#define MAX_ALIAS_FRAMES 256

texture_t *r_notexture_mip;

static int gl_texturemode;

static aliashdr_t *pheader;
static int pose_count;
trivertx_t *g_poseverts[MAX_ALIAS_FRAMES];

static byte *g_sprite_palette;

byte player_8bit_texels[64 * 1024];

extern int g_stverts[];
extern mtriangle_t g_triangles[];

texture_t *R_InitNoTexture(void)
{
	int mip_level;
	int size;
	byte *mip_data;
	int x;
	int y;

	r_notexture_mip =
		Hunk_AllocName(sizeof(texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2, "notexture");

	r_notexture_mip->width = 16;
	r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = 72;
	r_notexture_mip->offsets[1] = 328;
	r_notexture_mip->offsets[2] = 392;
	r_notexture_mip->offsets[3] = 408;

	for (mip_level = 0; mip_level < 4; ++mip_level)
	{
		size = 16 >> mip_level;
		mip_data = (byte *)r_notexture_mip + r_notexture_mip->offsets[mip_level];

		for (y = 0; y < size; ++y)
		{
			for (x = 0; x < size; ++x)
			{
				if ((x < (8 >> mip_level)) == (y < (8 >> mip_level)))
					*mip_data++ = 0xFF;
				else
					*mip_data++ = 0;
			}
		}
	}

	return r_notexture_mip;
}

void Mod_Init(void)
{
	memset(mod_novis, 0xFF, sizeof(mod_novis));
}

void *Mod_Extradata(model_t *mod)
{
	void *r;

	if (!mod)
		return NULL;

	r = Cache_Check(&mod->cache);
	if (r)
		return r;

	Mod_LoadModel(mod, true);

	if (!mod->cache.data)
		Sys_Error("Mod_Extradata: caching failed");

	return mod->cache.data;
}

mleaf_t *Mod_PointInLeaf(vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;

	if (!model || !model->nodes)
		Sys_Error("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (node->contents >= 0)
	{
		plane = node->plane;
		d = DotProduct(p, plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return (mleaf_t *)node;
}

byte *Mod_DecompressVis(byte *in, model_t *model)
{
	byte	*out;
	int		row;
	int		c;

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

	if (in)
	{
		do
		{
			if (*in)
			{
				*out++ = *in++;
			}
			else
			{
				c = in[1];
				in += 2;
				if (c)
				{
					memset(out, 0, c);
					out += c;
				}
			}
		} while (out - decompressed < row);
	}
	else if (row)
	{
		memset(decompressed, 0xFF, row);
	}

	return decompressed;
}

byte *Mod_LeafPVS(mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;

	return Mod_DecompressVis(leaf->compressed_vis, model);
}

void Mod_ClearAll(void)
{
	int			i;
	model_t		*mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (mod->type != mod_alias)
			mod->needload = true;
	}
}

model_t *Mod_FindName(char *name)
{
	int			i;
	model_t		*mod;

	if (!name[0])
		Sys_Error("Mod_ForName: NULL name");

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!strcmp(mod->name, name))
			break;
	}

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error("mod_numknown == MAX_MOD_KNOWN");

		strcpy(mod->name, name);
		mod->needload = true;
		mod_numknown++;
	}

	return mod;
}

void Mod_TouchModel(char *name)
{
	model_t *mod;

	mod = Mod_FindName(name);

	if (!mod->needload)
	{
		if (mod->type == mod_alias || mod->type == mod_studio)
			Cache_Check(&mod->cache);
	}
}

model_t *Mod_LoadModel(model_t *mod, qboolean crash)
{
	unsigned	*buf;
	byte		stackbuf[1024];

	if (mod->type == mod_alias || mod->type == mod_studio)
	{
		if (Cache_Check(&mod->cache))
		{
			mod->needload = false;
			return mod;
		}
	}
	else if (!mod->needload)
	{
		return mod;
	}

	buf = (unsigned *)COM_LoadStackFile(mod->name, stackbuf, sizeof(stackbuf));
	if (!buf)
	{
		if (crash)
			Sys_Error("Mod_NumForName: %s not found", mod->name);
		return NULL;
	}

	COM_FileBase(mod->name, loadname);
	loadmodel = mod;
	mod->needload = false;

	switch (LittleLong(*buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel(mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel(mod, buf);
		break;

	case IDSTUDIOHEADER:
		Mod_LoadStudioModel(mod, buf);
		break;

	default:
		Mod_LoadBrushModel(mod, buf);
		break;
	}

	return mod;
}

model_t *Mod_ForName(char *name, qboolean crash)
{
	model_t *mod;

	mod = Mod_FindName(name);

	return Mod_LoadModel(mod, crash);
}

void Mod_LoadLighting(lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = Hunk_AllocName(l->filelen, loadname);
	memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

void Mod_LoadVisibility(lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}

	loadmodel->visdata = Hunk_AllocName(l->filelen, loadname);
	memcpy(loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

void Mod_LoadEntities(lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = Hunk_AllocName(l->filelen, loadname);
	memcpy(loadmodel->entities, mod_base + l->fileofs, l->filelen);
}

void Mod_LoadVertexes(lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (dvertex_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

void Mod_LoadSubmodels(lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (dmodel_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->mins[j] = LittleFloat(in->mins[j]) - 1.0f;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1.0f;
			out->origin[j] = LittleFloat(in->origin[j]);
		}

		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);

		out->visleafs = LittleLong(in->visleafs);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

void Mod_LoadEdges(lump_t *l)
{
	dedge_t		*in;
	medge_t		*out;
	int			i, count;

	in = (dedge_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName((count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

void Mod_LoadTexinfo(lump_t *l)
{
	texinfo_t	*in;
	mtexinfo_t	*out;
	int			i, j, count;
	int			miptex;
	float		len1, len2;

	in = (texinfo_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 8; j++)
			((float *)out->vecs)[j] = LittleFloat(((float *)in->vecs)[j]);

		len1 = Length(out->vecs[0]);
		len2 = Length(out->vecs[1]);
		len1 = (len1 + len2) / 2.0f;

		if (len1 < 0.32f)
			out->mipadjust = 4.0f;
		else if (len1 < 0.49f)
			out->mipadjust = 3.0f;
		else if (len1 < 0.99f)
			out->mipadjust = 2.0f;
		else
			out->mipadjust = 1.0f;

		miptex = LittleLong(in->miptex);
		out->flags = LittleLong(in->flags);

		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;
			out->flags = 0;
		}
		else if (miptex >= loadmodel->numtextures)
		{
			Sys_Error("miptex >= loadmodel->numtextures");
		}
		else
		{
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip;
				out->flags = 0;
			}
		}
	}
}

void Mod_LoadTextures(lump_t *l)
{
	dmiptexlump_t *m;
	int i;
	int j;
	int num;
	int max;
	int altmax;
	miptex_t *mt;
	texture_t *tx;
	texture_t *tx2;
	texture_t *anims[10];
	texture_t *altanims[10];
	int pixels;
	unsigned short palette_colors;
	int total_bytes;

	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	m->nummiptex = LittleLong(m->nummiptex);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName(4 * m->nummiptex, loadname);

	for (i = 0; i < m->nummiptex; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		mt->width = LittleLong(mt->width);
		mt->height = LittleLong(mt->height);
		for (j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong(mt->offsets[j]);

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error("Texture %s is not 16 aligned", mt->name);

		pixels = 85 * ((mt->width * mt->height) >> 6);
		palette_colors = *(unsigned short *)((byte *)mt + sizeof(miptex_t) + pixels);
		total_bytes = pixels + 2 + palette_colors * 3;

		tx = Hunk_AllocName(sizeof(texture_t) + total_bytes, loadname);
		loadmodel->textures[i] = tx;

		memcpy(tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;

		for (j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] = mt->offsets[j] + (sizeof(texture_t) - sizeof(miptex_t));

		tx->palette = (byte *)tx + sizeof(texture_t) + pixels + 2;
		memcpy((byte *)(tx + 1), (byte *)mt + sizeof(miptex_t), total_bytes);

		if (Q_strncmp(mt->name, "sky", 3))
		{
			gl_texturemode = GL_LINEAR_MIPMAP_NEAREST;
			tx->gl_texturenum =
				GL_LoadTexture(mt->name, tx->width, tx->height, (byte *)(tx + 1), 1, 0, tx->palette);
			gl_texturemode = GL_LINEAR;
		}
		else
		{
			R_InitSky(tx);
		}
	}

	for (i = 0; i < m->nummiptex; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx)
			continue;
		if (tx->name[0] != '+' && tx->name[0] != '-')
			continue;
		if (tx->anim_next)
			continue;

		memset(anims, 0, sizeof(anims));
		memset(altanims, 0, sizeof(altanims));

		max = 0;
		altmax = 0;

		num = tx->name[1];
		if (num >= 'a' && num <= 'z')
			num -= 'a' - 'A';

		if (num >= '0' && num <= '9')
		{
			num -= '0';
			anims[num] = tx;
			max = num + 1;
		}
		else if (num >= 'A' && num <= 'J')
		{
			num -= 'A';
			altanims[num] = tx;
			altmax = num + 1;
		}
		else
		{
			Sys_Error("Bad animating texture %s", tx->name);
		}

		for (j = i + 1; j < m->nummiptex; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2)
				continue;
			if (tx2->name[0] != '+' && tx2->name[0] != '-')
				continue;
			if (strcmp(tx2->name + 2, tx->name + 2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';

			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num -= 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num + 1;
			}
			else
			{
				Sys_Error("Bad animating texture %s", tx->name);
			}
		}

		for (j = 0; j < max; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max;
			tx2->anim_min = j;
			tx2->anim_max = j + 1;
			tx2->anim_next = anims[(j + 1) % max];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}

		for (j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax;
			tx2->anim_min = j;
			tx2->anim_max = j + 1;
			tx2->anim_next = altanims[(j + 1) % altmax];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

void CalcSurfaceExtents(msurface_t *s)
{
	float		mins[2], maxs[2], val;
	int			i, j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int			bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -999999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++)
	{
		e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j = 0; j < 2; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				  v->position[1] * tex->vecs[j][1] +
				  v->position[2] * tex->vecs[j][2] +
				  tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++)
	{
		bmins[i] = floor(mins[i] / 16);
		bmaxs[i] = ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 512)
			Sys_Error("Bad surface extents");
	}
}

void Mod_LoadFaces(lump_t *l)
{
	dface_t		*in;
	msurface_t	*out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (dface_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + LittleShort(in->texinfo);

		CalcSurfaceExtents(out);

		out->styles[0] = in->styles[0];
		out->styles[1] = in->styles[1];
		out->styles[2] = in->styles[2];
		out->styles[3] = in->styles[3];

		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + i;

		if (!Q_strncmp(out->texinfo->texture->name, "sky", 3))
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
		}
		else
		{
			const char *texname = out->texinfo->texture->name;

			if (texname[0] == '!' || !strncmp(texname, "laser", 5))
			{
				out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
				GL_SubdivideSurface(out);
			}
		}
	}
}

void Mod_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

void Mod_LoadNodes(lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t		*out;

	in = (dnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j = 0; j < 2; j++)
		{
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent(loadmodel->nodes, NULL);
}

void Mod_LoadLeafs(lump_t *l)
{
	dleaf_t		*in;
	mleaf_t		*out;
	int			i, j, count, p;

	in = (dleaf_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;

		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		if (out->contents != -1 && out->nummarksurfaces > 0)
		{
			msurface_t **mark = out->firstmarksurface;
			j = out->nummarksurfaces;
			do
			{
				(*mark)->flags |= SURF_UNDERWATER;
				mark++;
			} while (--j);
		}
	}
}

void Mod_LoadClipnodes(lump_t *l)
{
	dclipnode_t	*in, *out;
	int			i, count;
	hull_t		*hull;

	in = (dclipnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -36;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 36;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -32;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[3];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -18;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 18;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

void Mod_MakeHull0(void)
{
	mnode_t		*in, *child;
	dclipnode_t	*out;
	int			i, j, count;
	hull_t		*hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j = 0; j < 2; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

void Mod_LoadMarksurfaces(lump_t *l)
{
	int			i, j, count;
	short		*in;
	msurface_t	**out;

	in = (short *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

void Mod_LoadSurfedges(lump_t *l)
{
	int		i, count;
	int		*in, *out;

	in = (int *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

void Mod_LoadPlanes(lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t	*in;
	int			count;
	int			bits;

	in = (dplane_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		bits = 0;
		for (j = 0; j < 3; j++)
		{
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}

float RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	bounds;

	for (i = 0; i < 3; i++)
	{
		if (fabs(mins[i]) > fabs(maxs[i]))
			bounds[i] = fabs(mins[i]);
		else
			bounds[i] = fabs(maxs[i]);
	}

	return Length(bounds);
}

void Mod_LoadBrushModel(model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	dmodel_t	*bm;
	char		subname[10];

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	i = LittleLong(header->version);
	if (i != BSPVERSION)
		Sys_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

	mod_base = (byte *)header;

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	Mod_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures(&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes(&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities(&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0();

	mod->numframes = 2;
	mod->flags = 0;

	for (i = 0; i < mod->numsubmodels; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j = 1; j < MAX_MAP_HULLS; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);

		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1)
		{
			sprintf(subname, "*%i", i + 1);
			loadmodel = Mod_FindName(subname);
			*loadmodel = *mod;
			strcpy(loadmodel->name, subname);
			mod = loadmodel;
		}
	}
}

void *Mod_LoadAliasFrame(void *pin, maliasframedesc_t *frame)
{
	daliasframe_t *pdaliasframe;
	trivertx_t *pinframe;

	pdaliasframe = (daliasframe_t *)pin;

	strcpy(frame->name, pdaliasframe->name);
	frame->firstpose = pose_count;
	frame->numposes = 1;

	frame->bboxmin = pdaliasframe->bboxmin;
	frame->bboxmax = pdaliasframe->bboxmax;

	pinframe = (trivertx_t *)(pdaliasframe + 1);
	g_poseverts[pose_count] = pinframe;
	pose_count++;

	return (byte *)pinframe + sizeof(trivertx_t) * pheader->numverts;
}

void *Mod_LoadAliasGroup(void *pin, maliasframedesc_t *frame)
{
	daliasgroup_t *pingroup;
	daliasinterval_t *pin_intervals;
	int numframes;
	int i;
	byte *ptemp;

	pingroup = (daliasgroup_t *)pin;
	numframes = LittleLong(pingroup->numframes);

	frame->firstpose = pose_count;
	frame->numposes = numframes;
	frame->bboxmin = pingroup->bboxmin;
	frame->bboxmax = pingroup->bboxmax;

	pin_intervals = (daliasinterval_t *)(pingroup + 1);
	frame->interval = LittleFloat(pin_intervals[0].interval);

	ptemp = (byte *)(pin_intervals + numframes);

	for (i = 0; i < numframes; i++)
	{
		g_poseverts[pose_count + i] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		ptemp += sizeof(daliasframe_t) + pheader->numverts * sizeof(trivertx_t);
	}

	pose_count += numframes;
	return ptemp;
}

typedef struct floodfill_s
{
	short x, y;
} floodfill_t;

void Mod_FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte			fillcolor;
	floodfill_t		fifo[0x1000];
	int				inpt = 0;
	int				outpt = 0;
	int				filledcolor = -1;
	int				x, y;
	int				fdc;
	byte			*pos;

	fillcolor = *skin;

	if (filledcolor == -1)
		filledcolor = 0;

	if ((fillcolor == filledcolor) || (fillcolor == 255))
		return;

	fifo[inpt].x = 0;
	fifo[inpt].y = 0;
	inpt = (inpt + 1) & 0xFFF;

	while (outpt != inpt)
	{
		x = fifo[outpt].x;
		y = fifo[outpt].y;
		fdc = filledcolor;
		pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & 0xFFF;

		if (x > 0)
		{
			if (*(pos - 1) == fillcolor)
			{
				*(pos - 1) = 255;
				fifo[inpt].x = x - 1;
				fifo[inpt].y = y;
				inpt = (inpt + 1) & 0xFFF;
			}
			else if (*(pos - 1) != 255)
				fdc = *(pos - 1);
		}

		if (x < skinwidth - 1)
		{
			if (*(pos + 1) == fillcolor)
			{
				*(pos + 1) = 255;
				fifo[inpt].x = x + 1;
				fifo[inpt].y = y;
				inpt = (inpt + 1) & 0xFFF;
			}
			else if (*(pos + 1) != 255)
				fdc = *(pos + 1);
		}

		if (y > 0)
		{
			if (*(pos - skinwidth) == fillcolor)
			{
				*(pos - skinwidth) = 255;
				fifo[inpt].x = x;
				fifo[inpt].y = y - 1;
				inpt = (inpt + 1) & 0xFFF;
			}
			else if (*(pos - skinwidth) != 255)
				fdc = *(pos - skinwidth);
		}

		if (y < skinheight - 1)
		{
			if (*(pos + skinwidth) == fillcolor)
			{
				*(pos + skinwidth) = 255;
				fifo[inpt].x = x;
				fifo[inpt].y = y + 1;
				inpt = (inpt + 1) & 0xFFF;
			}
			else if (*(pos + skinwidth) != 255)
				fdc = *(pos + skinwidth);
		}

		*pos = fdc;
	}
}

void *Mod_LoadAllSkins(int numskins, void *pskintype)
{
	int i;
	int skinsize;
	byte *skin;
	byte *p;
	char texname[32];

	if (numskins < 1 || numskins > 32)
		Sys_Error("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	skinsize = pheader->skinwidth * pheader->skinheight;
	p = (byte *)pskintype;

	for (i = 0; i < numskins; i++)
	{
		skin = p + 4;

		Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);

		if (!strcmp(loadmodel->name, "progs/player.mdl"))
		{
			if ((unsigned)skinsize > 0xFA00u)
				Sys_Error("Player skin too large");
			memcpy(player_8bit_texels, skin, skinsize);
		}

		sprintf(texname, "%s_%i", loadmodel->name, i);
		pheader->gl_texturenum[i] =
			GL_LoadTexture(texname, pheader->skinwidth, pheader->skinheight, skin, 1, 0, NULL);

		p += skinsize + 4;
	}

	return p;
}

void Mod_LoadAliasModel(model_t *mod, void *buffer)
{
	mdl_t *pinmodel;
	byte *ptemp;
	stvert_t *pinstverts;
	dtriangle_t *pintriangles;
	daliasframetype_t *pframetype;
	int startMark;
	int version;
	int i;
	int j;
	int size;

	startMark = Hunk_LowMark();

	pinmodel = (mdl_t *)buffer;
	version = LittleLong(pinmodel->version);
	if (version != ALIASVERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, ALIASVERSION);

	pheader = Hunk_AllocName(
		sizeof(aliashdr_t) + (LittleLong(pinmodel->numframes) - 1) * sizeof(maliasframedesc_t), loadname);

	mod->flags = LittleLong(pinmodel->flags);
	mod->synctype = LittleLong(pinmodel->synctype);

	pheader->boundingradius = LittleFloat(pinmodel->boundingradius);
	pheader->numskins = LittleLong(pinmodel->numskins);
	pheader->skinwidth = LittleLong(pinmodel->skinwidth);
	pheader->skinheight = LittleLong(pinmodel->skinheight);
	pheader->numverts = LittleLong(pinmodel->numverts);
	pheader->numtris = LittleLong(pinmodel->numtris);
	pheader->numframes = LittleLong(pinmodel->numframes);
	pheader->size = LittleFloat(pinmodel->size) * 0.09090909090909091f;

	if (pheader->skinheight > 480)
		Sys_Error("model %s has a skin taller than %d", mod->name, 480);

	if (pheader->numverts <= 0)
		Sys_Error("model %s has no vertices", mod->name);
	if (pheader->numverts > 1024)
		Sys_Error("model %s has too many vertices", mod->name);

	if (pheader->numtris <= 0)
		Sys_Error("model %s has no triangles", mod->name);

	if (pheader->numframes < 1)
		Sys_Error("Mod_LoadAliasModel: Invalid # of frames: %d\n", pheader->numframes);

	mod->numframes = pheader->numframes;

	for (i = 0; i < 3; i++)
	{
		pheader->scale[i] = LittleFloat(pinmodel->scale[i]);
		pheader->translate[i] = LittleFloat(pinmodel->translate[i]);
		pheader->eyeposition[i] = LittleFloat(pinmodel->eyeposition[i]);
	}

	ptemp = Mod_LoadAllSkins(pheader->numskins, (void *)(pinmodel + 1));

	pinstverts = (stvert_t *)ptemp;
	for (i = 0; i < pheader->numverts; i++)
	{
		g_stverts[3 * i + 0] = LittleLong(pinstverts[i].onseam);
		g_stverts[3 * i + 1] = LittleLong(pinstverts[i].s);
		g_stverts[3 * i + 2] = LittleLong(pinstverts[i].t);
	}

	pintriangles = (dtriangle_t *)(pinstverts + pheader->numverts);
	for (i = 0; i < pheader->numtris; i++)
	{
		g_triangles[i].facesfront = LittleLong(pintriangles[i].facesfront);
		for (j = 0; j < 3; j++)
			g_triangles[i].vertindex[j] = LittleLong(pintriangles[i].vertindex[j]);
	}

	pframetype = (daliasframetype_t *)(pintriangles + pheader->numtris);

	pose_count = 0;
	for (i = 0; i < pheader->numframes; i++)
	{
		if (LittleLong(pframetype->type))
			pframetype = (daliasframetype_t *)Mod_LoadAliasGroup(pframetype + 1, &pheader->frames[i]);
		else
			pframetype = (daliasframetype_t *)Mod_LoadAliasFrame(pframetype + 1, &pheader->frames[i]);
	}

	mod->type = mod_alias;
	pheader->numposes = pose_count;

	mod->mins[0] = -16.0f;
	mod->mins[1] = -16.0f;
	mod->mins[2] = -16.0f;
	mod->maxs[0] = 16.0f;
	mod->maxs[1] = 16.0f;
	mod->maxs[2] = 16.0f;

	GL_MakeAliasModelDisplayLists(mod, pheader);

	size = Hunk_LowMark() - startMark;
	if (Cache_Alloc(&mod->cache, size, loadname))
	{
		memcpy(mod->cache.data, pheader, size);
		Hunk_FreeToLowMark(startMark);
	}
}

static void *Mod_LoadSpriteFrame(void *pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pframe;
	int				width, height;
	char			name[64];
	byte			palette[768];
	int				origin_x, origin_y;

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong(pinframe->width);
	height = LittleLong(pinframe->height);
	origin_x = LittleLong(pinframe->origin[0]);
	origin_y = LittleLong(pinframe->origin[1]);

	pframe = Hunk_AllocName(sizeof(mspriteframe_t), loadname);
	Q_memset(pframe, 0, sizeof(mspriteframe_t));

	*ppframe = pframe;

	pframe->width = width;
	pframe->height = height;

	pframe->up = (float)origin_y;
	pframe->down = (float)(origin_y - height);
	pframe->left = (float)origin_x;
	pframe->right = (float)(origin_x + width);

	memcpy(palette, g_sprite_palette, sizeof(palette));

	sprintf(name, "%s_%i", loadmodel->name, framenum);

	pframe->gl_texturenum = GL_LoadTexture(name, width, height, (byte *)(pinframe + 1), 1, 1, palette);

	return (void *)((byte *)(pinframe + 1) + width * height);
}

static void *Mod_LoadSpriteGroup(void *pin, mspriteframe_t **ppframe, int framenum)
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pgroup;
	int				numframes;
	int				i;
	dspriteinterval_t *pinintervals;
	float			*pintervals;
	void			*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong(pingroup->numframes);

	pgroup = Hunk_AllocName(sizeof(mspritegroup_t) + (numframes - 1) * sizeof(mspriteframe_t *), loadname);
	pgroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pgroup;

	pintervals = Hunk_AllocName(sizeof(float) * numframes, loadname);
	pgroup->intervals = pintervals;

	pinintervals = (dspriteinterval_t *)(pingroup + 1);

	for (i = 0; i < numframes; i++)
	{
		*pintervals = LittleFloat(pinintervals[i].interval);
		if (*pintervals <= 0.0)
			Sys_Error("Mod_LoadSpriteGroup: Invalid frame duration");
		pintervals++;
	}

	ptemp = (void *)(pinintervals + numframes);

	for (i = 0; i < numframes; i++)
	{
		ptemp = Mod_LoadSpriteFrame(ptemp, &pgroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}

void Mod_LoadSpriteModel(model_t *mod, void *buffer)
{
	dsprite_t		*pin;
	msprite_t		*psprite;
	int				i;
	byte			*ptemp;
	int				version;
	int				numframes;
	int				size;
	unsigned int	palSize;
	dspriteframetype_t *pframetype;
	short			numcolors;

	pin = (dsprite_t *)buffer;
	version = LittleLong(pin->version);

	if (version != 1)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, 1);

	numframes = LittleLong(pin->numframes);

	size = sizeof(msprite_t) + (numframes - 1) * sizeof(mspritegroupframe_t);

	numcolors = LittleShort(*(short *)((byte *)buffer + sizeof(dsprite_t)));
	palSize = 3 * (unsigned short)numcolors + 2;

	psprite = Hunk_AllocName(size + palSize, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong(pin->type);
	psprite->maxwidth = LittleLong(pin->width);
	psprite->maxheight = LittleLong(pin->height);
	psprite->beamlength = LittleFloat(pin->beamlength);
	psprite->numframes = numframes;
	psprite->paloffset = size + 2;

	mod->type = mod_sprite;
	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = psprite->maxheight / -2;
	mod->maxs[2] = psprite->maxheight / 2;

	mod->numframes = numframes;
	mod->synctype = LittleLong(pin->synctype);
	mod->flags = 0;

	g_sprite_palette = (byte *)psprite + size;
	memcpy((byte *)psprite + size, (byte *)buffer + sizeof(dsprite_t) + 2, palSize);

	ptemp = (byte *)buffer + sizeof(dsprite_t) + palSize;

	for (i = 0; i < numframes; i++)
	{
		pframetype = (dspriteframetype_t *)ptemp;
		psprite->frames[i].type = LittleLong(pframetype->type);

		if (psprite->frames[i].type == SPR_SINGLE)
		{
			ptemp = (byte *)Mod_LoadSpriteFrame(pframetype + 1, &psprite->frames[i].frameptr, i);
		}
		else
		{
			ptemp = (byte *)Mod_LoadSpriteGroup(pframetype + 1, &psprite->frames[i].frameptr, i);
		}
	}
}

void Mod_LoadStudioModel(model_t *mod, void *buffer)
{
	int startMark;
	studiohdr_t *phdr;
	unsigned int length;
	studiohdr_t *copy;
	int i;
	mstudiotexture_t *textures;
	char texname[256];
	byte *palette;
	void *cache;
	int size;

	startMark = Hunk_LowMark();
	phdr = (studiohdr_t *)buffer;

	if (LittleLong(phdr->version) != 6)
	{
		memset(phdr, 0, 188);
		strcpy(phdr->name, "bogus");
		phdr->length = 188;
	}

	length = phdr->length;

	mod->type = mod_studio;
	mod->flags = 0;

	copy = Hunk_AllocName(length, loadname);
	memcpy(copy, buffer, length);

	if (phdr->numtextures > 0)
	{
		textures = (mstudiotexture_t *)((byte *)copy + phdr->textureindex);
		for (i = 0; i < phdr->numtextures; i++)
		{
			palette = (byte *)copy + textures[i].index + textures[i].width * textures[i].height;

			strcpy(texname, mod->name);
			strcat(texname, textures[i].name);

			textures[i].index = GL_LoadTexture(
				texname,
				textures[i].width,
				textures[i].height,
				(byte *)copy + textures[i].index,
				0,
				0,
				palette);
		}
	}

	size = Hunk_LowMark() - startMark;
	cache = Cache_Alloc(&mod->cache, size, loadname);
	if (cache)
	{
		memcpy(cache, copy, size);
		Hunk_FreeToLowMark(startMark);
	}
}

void Mod_Print(void)
{
	int		i;
	model_t	*mod;

	Con_Printf("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		Con_Printf("%8p : %s\n", mod->cache.data, mod->name);
	}
}
