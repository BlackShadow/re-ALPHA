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

extern model_t *loadmodel;
extern double cl_time;
extern double realtime;
extern vec3_t r_refdef_vieworg;
extern entity_t *currententity;
extern int texture_extension_number;
extern vec3_t r_origin;
extern model_t *currentmodel;
extern cvar_t gl_wateramp;

void GL_Bind(int texnum);
int COM_CheckParm(const char *parm);
int COM_FOpenFile(const char *filename, FILE **file);
void LoadTGA(FILE *fin);
extern byte *targa_rgba;
extern int gl_alpha_format;

float turbsin[256] =
{
	0.000000f, 0.196330f, 0.392541f, 0.588517f, 0.784137f, 0.979285f, 1.173840f, 1.367700f,
	1.560720f, 1.752810f, 1.943840f, 2.133700f, 2.322280f, 2.509450f, 2.695120f, 2.879160f,
	3.061470f, 3.241930f, 3.420440f, 3.596890f, 3.771170f, 3.943190f, 4.112820f, 4.279980f,
	4.444560f, 4.606470f, 4.765590f, 4.921850f, 5.075150f, 5.225380f, 5.372470f, 5.516320f,
	5.656850f, 5.793980f, 5.927610f, 6.057670f, 6.184080f, 6.306770f, 6.425660f, 6.540680f,
	6.651760f, 6.758830f, 6.861830f, 6.960700f, 7.055370f, 7.145790f, 7.231910f, 7.313680f,
	7.391040f, 7.463940f, 7.532350f, 7.596230f, 7.655520f, 7.710210f, 7.760250f, 7.805620f,
	7.846280f, 7.882220f, 7.913410f, 7.939840f, 7.961480f, 7.978320f, 7.990360f, 7.997590f,
	8.000000f, 7.997590f, 7.990360f, 7.978320f, 7.961480f, 7.939840f, 7.913410f, 7.882220f,
	7.846280f, 7.805620f, 7.760250f, 7.710210f, 7.655520f, 7.596230f, 7.532350f, 7.463940f,
	7.391040f, 7.313680f, 7.231910f, 7.145790f, 7.055370f, 6.960700f, 6.861830f, 6.758830f,
	6.651760f, 6.540680f, 6.425660f, 6.306770f, 6.184080f, 6.057670f, 5.927610f, 5.793980f,
	5.656850f, 5.516320f, 5.372470f, 5.225380f, 5.075150f, 4.921850f, 4.765590f, 4.606470f,
	4.444560f, 4.279980f, 4.112820f, 3.943190f, 3.771170f, 3.596890f, 3.420440f, 3.241930f,
	3.061470f, 2.879160f, 2.695120f, 2.509450f, 2.322280f, 2.133700f, 1.943840f, 1.752810f,
	1.560720f, 1.367700f, 1.173840f, 0.979285f, 0.784137f, 0.588517f, 0.392541f, 0.196330f,
	9.79717e-16f, -0.196330f, -0.392541f, -0.588517f, -0.784137f, -0.979285f, -1.173840f, -1.367700f,
	-1.560720f, -1.752810f, -1.943840f, -2.133700f, -2.322280f, -2.509450f, -2.695120f, -2.879160f,
	-3.061470f, -3.241930f, -3.420440f, -3.596890f, -3.771170f, -3.943190f, -4.112820f, -4.279980f,
	-4.444560f, -4.606470f, -4.765590f, -4.921850f, -5.075150f, -5.225380f, -5.372470f, -5.516320f,
	-5.656850f, -5.793980f, -5.927610f, -6.057670f, -6.184080f, -6.306770f, -6.425660f, -6.540680f,
	-6.651760f, -6.758830f, -6.861830f, -6.960700f, -7.055370f, -7.145790f, -7.231910f, -7.313680f,
	-7.391040f, -7.463940f, -7.532350f, -7.596230f, -7.655520f, -7.710210f, -7.760250f, -7.805620f,
	-7.846280f, -7.882220f, -7.913410f, -7.939840f, -7.961480f, -7.978320f, -7.990360f, -7.997590f,
	-8.000000f, -7.997590f, -7.990360f, -7.978320f, -7.961480f, -7.939840f, -7.913410f, -7.882220f,
	-7.846280f, -7.805620f, -7.760250f, -7.710210f, -7.655520f, -7.596230f, -7.532350f, -7.463940f,
	-7.391040f, -7.313680f, -7.231910f, -7.145790f, -7.055370f, -6.960700f, -6.861830f, -6.758830f,
	-6.651760f, -6.540680f, -6.425660f, -6.306770f, -6.184080f, -6.057670f, -5.927610f, -5.793980f,
	-5.656850f, -5.516320f, -5.372470f, -5.225380f, -5.075150f, -4.921850f, -4.765590f, -4.606470f,
	-4.444560f, -4.279980f, -4.112820f, -3.943190f, -3.771170f, -3.596890f, -3.420440f, -3.241930f,
	-3.061470f, -2.879160f, -2.695120f, -2.509450f, -2.322280f, -2.133700f, -1.943840f, -1.752810f,
	-1.560720f, -1.367700f, -1.173840f, -0.979285f, -0.784137f, -0.588517f, -0.392541f, -0.196330f
};

#define SKY_TEX_START   2000

int solidskytexture;
int alphaskytexture;
float speedscale;

byte g_WaterColor[4];

static int skyfacecount;
static vec3_t sky_color;

static float skymins[2][6];
static float skymaxs[2][6];

extern char *cl_skyname_string;

static int st_to_vec[6][3] =
{
    {3, -1, 2},
    {-3, 1, 2},
    {1, 3, 2},
    {-1, -3, 2},
    {-3, -1, -2},
    {-3, 1, -2}
};

static int vec_to_st[6] = {3, -3, 1, -1, -3, -3};
static int vec_to_t[6] = {-1, 1, 3, -3, -1, 1};

static char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

void EmitWaterPolys(msurface_t *fa, int direction)
{
	glpoly_t *p;
	GLfloat *v;
	int i;
	float os;
	float ot;
	GLfloat vertex[3];
	float warpS;
	float warpT;
	float z;

	g_WaterColor[0] = currententity->rendercolor[0];
	g_WaterColor[1] = currententity->rendercolor[1];
	g_WaterColor[2] = currententity->rendercolor[2];
	g_WaterColor[3] = 128;

	for (p = fa->polys; p; p = p->next)
	{
		if (direction)
			v = &p->verts[p->numverts - 1][0];
		else
			v = &p->verts[0][0];

		glBegin(GL_POLYGON);

		for (i = 0; i < p->numverts; i++)
		{
			os = v[3];
			ot = v[4];

			warpS = turbsin[(unsigned char)(__int64)((ot * 0.125 + realtime) * 40.74366543152521)] + os;
			warpS *= 0.015625f;

			warpT = turbsin[(unsigned char)(__int64)((os * 0.125 + realtime) * 40.74366543152521)] + ot;
			warpT *= 0.015625f;

			glTexCoord2f(warpS, warpT);

			vertex[0] = v[0];
			vertex[1] = v[1];
			vertex[2] = v[2];

			z = turbsin[(unsigned char)(__int64)(cl_time * 160.0 + vertex[0] + vertex[1])] * gl_wateramp.value + vertex[2];
			z = turbsin[(unsigned char)(__int64)(vertex[0] * 5.0 + cl_time * 171.0 - vertex[1])] * gl_wateramp.value * 0.8f + z;
			vertex[2] = z;

			glVertex3fv(vertex);

			if (direction)
				v -= VERTEXSIZE;
			else
				v += VERTEXSIZE;
		}

		glEnd();
	}
}

void EmitSkyPolys(msurface_t *fa)
{
    glpoly_t *p;
    float *v;
    int i;
    vec3_t dir;
    float length;
    float s, t;

    for (p = (glpoly_t *)*(int *)((int)fa + 36); p; p = (glpoly_t *)p->next)
    {
        glBegin(GL_POLYGON);

        v = &p->verts[0][0];

        for (i = 0; i < p->numverts; i++, v += 7)
        {
            dir[0] = v[0] - r_refdef_vieworg[0];
            dir[1] = v[1] - r_refdef_vieworg[1];
            dir[2] = v[2] - r_refdef_vieworg[2];

            dir[2] *= 3.0f;

            length = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
            length = sqrt(length);
            length = 378.0f / length;

            dir[0] *= length;
            dir[1] *= length;

            s = (dir[0] + speedscale) * 0.0078125f;
            t = (dir[1] + speedscale) * 0.0078125f;

            glTexCoord2f(s, t);
            glVertex3fv(v);
        }

        glEnd();
    }
}

void EmitBothSkyLayers(msurface_t *fa)
{
    float temp;
    int truncated;

    GL_Bind(solidskytexture);

    temp = cl_time * 8.0f;
    truncated = (int)temp;
    truncated &= 0x80;
    speedscale = temp - (float)truncated;

    EmitSkyPolys(fa);

    glEnable(GL_BLEND);

    GL_Bind(alphaskytexture);

    temp = cl_time * 16.0f;
    truncated = (int)temp;
    truncated &= 0x80;
    speedscale = temp - (float)truncated;

    EmitSkyPolys(fa);

    glDisable(GL_BLEND);
}

void R_LoadSkys(void)
{
    int i;
    char filename[64];
    FILE *f;

    for (i = 0; i < 6; i++)
    {
        GL_Bind(SKY_TEX_START + i);

        sprintf(filename, "gfx/env/bkgtst%s.tga", suf[i]);

        COM_FOpenFile(filename, &f);
        if (f)
        {
            LoadTGA(f);
            glTexImage2D(GL_TEXTURE_2D, 0, gl_alpha_format, 256, 256, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, targa_rgba);
            free(targa_rgba);
            targa_rgba = NULL;

            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    }
}

static void EmitSkyVertex(float s, float t, int facenum)
{
	vec3_t v, b;
	int j;
	static int st_to_vec[6][3] =
	{
		{3, -1, 2},
		{-3, 1, 2},
		{1, 3, 2},
		{-1, -3, 2},
		{-3, -1, -2},
		{-3, 1, -2}
	};

	b[0] = s * 2048.0f;
	b[1] = t * 2048.0f;
	b[2] = 2048.0f;

	for (j = 0; j < 3; j++)
	{
		int k = st_to_vec[facenum][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_refdef_vieworg[j];
	}

	glTexCoord2f((s + 1.0f) * 0.5f, (t + 1.0f) * 0.5f);
	glVertex3fv(v);
}

void R_DrawSkyBox(void)
{
	int face;

	for (face = 0; face < 6; face++)
	{
		GL_Bind(SKY_TEX_START + face);
		glBegin(GL_QUADS);
		EmitSkyVertex(-1, -1, face);
		EmitSkyVertex(-1, 1, face);
		EmitSkyVertex(1, 1, face);
		EmitSkyVertex(1, -1, face);
		glEnd();
	}
}

void MakeSkyVec(float s, float t, int axis)
{
	EmitSkyVertex(s, t, axis);
}

void R_InitSky(texture_t *mt)
{
    int x, y;
    int src_offset;
    byte *src;
    unsigned int *dest;
    unsigned int pixel;
    int red_sum, green_sum, blue_sum;
    unsigned int avg_color;
    unsigned int pixels[16384];

    red_sum = 0;
    green_sum = 0;
    blue_sum = 0;
    src_offset = mt->offsets[0];
    src = (byte *)mt + src_offset;
    dest = pixels;

    for (y = 0; y < 128; y++)
    {
        for (x = 0; x < 128; x++)
        {
            pixel = pixels[0];
            *dest++ = pixel;

            red_sum += pixel & 0xFF;
            green_sum += (pixel >> 8) & 0xFF;
            blue_sum += (pixel >> 16) & 0xFF;
        }
    }

    avg_color = (red_sum / 0x4000) |
                ((green_sum / 0x4000) << 8) |
                ((blue_sum / 0x4000) << 16) |
                0xFF000000;

    if (!solidskytexture)
        solidskytexture = texture_extension_number++;

    GL_Bind(solidskytexture);
    glTexImage2D(GL_TEXTURE_2D, 0, texture_extension_number, 128, 128, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    for (y = 0; y < 0x4000; y += 128)
    {
        for (x = 0; x < 128; x++)
        {
            if (!src[x])
                pixels[x + y] = avg_color;
        }
        src += 256;
    }

    if (!alphaskytexture)
        alphaskytexture = texture_extension_number++;

    GL_Bind(alphaskytexture);
    glTexImage2D(GL_TEXTURE_2D, 0, texture_extension_number, 128, 128, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void GL_CalcMinMaxBounds(int vertexCount, float *vertices, float *minBound, float *maxBound)
{
	int i, j;
	float *v;
	float *vmax;

	minBound[2] = 9999.0f;
	minBound[1] = 9999.0f;
	minBound[0] = 9999.0f;
	maxBound[2] = -9999.0f;
	maxBound[1] = -9999.0f;
	maxBound[0] = -9999.0f;

	if (vertexCount > 0)
	{
		for (i = 0; i < vertexCount; ++i)
		{
			vmax = maxBound;
			v = minBound;
			for (j = 0; j < 3; ++j)
			{
				if (*vertices < *v)
					*v = *vertices;
				if (*vertices > *vmax)
					*vmax = *vertices;
				++vmax;
				++v;
				++vertices;
			}
		}
	}
}

static msurface_t *g_WarpFace;

static void SubdividePolygon(int vertexCount, float *vertices)
{
	int axis;
	float mid;
	vec3_t mins;
	vec3_t maxs;
	float dist[64];
	float front[64][3];
	float back[64][3];
	int numFront;
	int numBack;
	int i;
	int j;
	float *v;

	if (vertexCount > 60)
		Sys_Error("numverts = %i", vertexCount);

	GL_CalcMinMaxBounds(vertexCount, vertices, mins, maxs);

	axis = 0;
	while (1)
	{
		mid = (maxs[axis] + mins[axis]) * 0.5f;
		mid = (float)(floor(mid / 64.0f + 0.5f) * 64.0f);
		if (maxs[axis] - mid >= 8.0f && mid - mins[axis] >= 8.0f)
			break;

		axis++;
		if (axis >= 3)
		{
			glpoly_t *poly;
			mtexinfo_t *tex;
			float texMinS;
			float texMinT;
			float lightS;
			float lightT;
			float *out;

			poly = Hunk_Alloc(16 + vertexCount * VERTEXSIZE * sizeof(float));
			poly->next = g_WarpFace->polys;
			g_WarpFace->polys = poly;

			poly->numverts = vertexCount;
			poly->flags = g_WarpFace->flags;

			tex = g_WarpFace->texinfo;
			texMinS = (float)g_WarpFace->texturemins[0];
			texMinT = (float)g_WarpFace->texturemins[1];
			lightS = (float)(16 * g_WarpFace->light_s);
			lightT = (float)(16 * g_WarpFace->light_t);

			out = &poly->verts[0][0];
			v = vertices;

			for (i = 0; i < vertexCount; i++, v += 3, out += VERTEXSIZE)
			{
				float s;
				float t;

				out[0] = v[0];
				out[1] = v[1];
				out[2] = v[2];

				s = tex->vecs[0][0] * v[0] + tex->vecs[0][1] * v[1] + tex->vecs[0][2] * v[2];
				t = tex->vecs[1][0] * v[0] + tex->vecs[1][1] * v[1] + tex->vecs[1][2] * v[2];

				out[3] = s;
				out[4] = t;

				out[5] = (s + tex->vecs[0][3] - texMinS + lightS + 8.0f) / 2048.0f;
				out[6] = (t + tex->vecs[1][3] - texMinT + lightT + 8.0f) / 2048.0f;
			}

			return;
		}
	}

	for (i = 0; i < vertexCount; i++)
		dist[i] = vertices[i * 3 + axis] - mid;
	dist[vertexCount] = dist[0];
	memcpy(&vertices[vertexCount * 3], vertices, sizeof(float) * 3);

	numFront = 0;
	numBack = 0;

	v = vertices;
	for (i = 0; i < vertexCount; i++, v += 3)
	{
		if (dist[i] >= 0.0f)
		{
			front[numFront][0] = v[0];
			front[numFront][1] = v[1];
			front[numFront][2] = v[2];
			numFront++;
		}

		if (dist[i] <= 0.0f)
		{
			back[numBack][0] = v[0];
			back[numBack][1] = v[1];
			back[numBack][2] = v[2];
			numBack++;
		}

		if (dist[i] != 0.0f && dist[i + 1] != 0.0f && (dist[i] > 0.0f) != (dist[i + 1] > 0.0f))
		{
			float frac;
			float *next;

			frac = dist[i] / (dist[i] - dist[i + 1]);
			next = v + 3;
			for (j = 0; j < 3; j++)
			{
				float val = (next[j] - v[j]) * frac + v[j];
				front[numFront][j] = val;
				back[numBack][j] = val;
			}
			numFront++;
			numBack++;
		}
	}

	SubdividePolygon(numFront, (float *)front);
	SubdividePolygon(numBack, (float *)back);
}

void GL_SubdivideSurface(msurface_t *fa)
{
	int i;
	int vertexCount;
	float verts[64][3];

	g_WarpFace = fa;

	vertexCount = fa->numedges;
	for (i = 0; i < vertexCount; i++)
	{
		int lindex;
		medge_t *edge;
		float *vec;

		lindex = loadmodel->surfedges[fa->firstedge + i];
		if (lindex <= 0)
		{
			edge = &loadmodel->edges[-lindex];
			vec = loadmodel->vertexes[edge->v[1]].position;
		}
		else
		{
			edge = &loadmodel->edges[lindex];
			vec = loadmodel->vertexes[edge->v[0]].position;
		}

		verts[i][0] = vec[0];
		verts[i][1] = vec[1];
		verts[i][2] = vec[2];
	}

	SubdividePolygon(vertexCount, (float *)verts);
}

int SetupSkyPolygonClipping(int param1, int param2, int param3)
{
	sky_color[0] = (float)param1;
	sky_color[1] = (float)param2;
	sky_color[2] = (float)param3;
	g_WaterColor[0] = (byte)param1;
	g_WaterColor[1] = (byte)param2;
	g_WaterColor[2] = (byte)param3;
	g_WaterColor[3] = 128;

	return param1;
}

int ReadWord(FILE *file)
{
	unsigned char byte1, byte2;

	byte1 = fgetc(file);
	byte2 = fgetc(file);
	return (short)(byte1 + (byte2 << 8));
}

void AccumulateSkySurface(int planeAxis, float *vertices)
{
	float minX, minY, minZ;
	float absX, absY, absZ;
	int axis;
	int i;
	int axis2;
	int axis3;

	++skyfacecount;

	minX = vertices[0];
	minY = vertices[1];
	minZ = vertices[2];

	if (planeAxis > 0)
	{
		for (i = 0; i < planeAxis; ++i)
		{
			minX = vertices[0] + minX;
			vertices += 3;
			minY = vertices[-2] + minY;
			minZ = vertices[-1] + minZ;
		}
	}

	absX = fabs(minX);
	absY = fabs(minY);
	absZ = fabs(minZ);

	if (absY < absX && absZ < absX)
	{
		axis = (minX < 0.0f) ? 1 : 0;
	}
	else if (absZ >= absY)
	{
		axis = (minZ < 0.0f) ? 5 : 4;
	}
	else
	{
		axis = (minY < 0.0f) ? 3 : 2;
	}

	if (planeAxis > 0)
	{
		axis2 = vec_to_st[axis];
		axis3 = vec_to_t[axis];

		for (i = 0; i < planeAxis; ++i)
		{
			float val;
			if (axis2 <= 0)
				val = -vertices[-axis2 - 1];
			else
				val = vertices[axis2 - 1];

			float ratio1 = (axis3 >= 0) ? vertices[axis3 - 1] / val : -(vertices[-axis3 - 1] / val);

			if (skymins[0][axis] > ratio1)
				skymins[0][axis] = ratio1;

			vertices += 3;
		}
	}
}

void ClipSkyPolygon(int vertexCount, float *vertices, int planeIndex)
{
	float *v;
	int i;
	double dotProduct;
	float clipVerts[192];
	float outVerts[192];
	int outCount = 0;
	int clipCount = 0;
	int edgeDots[64];

	if (vertexCount > 62)
		Sys_Error("ClipSkyPolygon: too many verts");

	v = (float *)&st_to_vec[0][0] + 3 * planeIndex;

	while (1)
	{
		if (planeIndex >= 6)
		{
			AccumulateSkySurface(vertexCount, vertices);
			return;
		}

		clipCount = 0;
		outCount = 0;

		for (i = 0; i < vertexCount; ++i)
		{
			dotProduct = vertices[0] * v[0] + vertices[1] * v[1] + vertices[2] * v[2];

			if (dotProduct <= 0.1)
			{
				if (dotProduct >= 0.1)
					edgeDots[i] = 2;
				else
				{
					clipCount = 1;
					edgeDots[i] = 1;
				}
			}
			else
			{
				outCount = 1;
				edgeDots[i] = 0;
			}

			vertices += 3;
		}

		if (outCount && clipCount)
			break;

		v += 3;
		++planeIndex;
	}

	ClipSkyPolygon(outCount, (float *)outVerts, planeIndex + 1);
	ClipSkyPolygon(clipCount, (float *)clipVerts, planeIndex + 1);
}

void R_DrawSkyChain(msurface_t *surface)
{
	glpoly_t *poly;
	int vertexCount;
	float *vertexData;
	float tmpVerts[192];
	float *outVerts;
	int vertexIdx;

	skyfacecount = 0;
	GL_Bind(solidskytexture);

	for (; surface; surface = surface->texturechain)
	{
		for (poly = surface->polys; poly; poly = poly->next)
		{
			vertexCount = poly->numverts;
			if (vertexCount > 0)
			{
				vertexData = &poly->verts[0][0];
				outVerts = tmpVerts;

				for (vertexIdx = 0; vertexIdx < vertexCount; ++vertexIdx)
				{
					outVerts[0] = vertexData[0] - r_origin[0];
					outVerts[1] = vertexData[1] - r_origin[1];
					outVerts[2] = vertexData[2] - r_origin[2];
					outVerts += 3;
					vertexData += 7;
				}
			}

			ClipSkyPolygon(vertexCount, tmpVerts, 0);
		}
	}
}

int InitSkyPolygonBounds(void)
{
	int i;

	for (i = 0; i < 6; ++i)
	{
		skymins[1][i] = 9999.0f;
		skymaxs[0][i] = -9999.0f;
		skymins[0][i] = 9999.0f;
		skymaxs[1][i] = -9999.0f;
	}

	return i * 4;
}
