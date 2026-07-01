#pragma once

#include <stddef.h>

struct edict_t;

struct KeyValueData
{
  const char *szClassName;
  const char *szKeyName;
  const char *szValue;
  int fHandled;
};

struct TraceResult
{
  int fAllSolid;
  int fStartSolid;
  int fInOpen;
  int fInWater;
  float flFraction;
  float vecEndPos[3];
  float flPlaneDist;
  float vecPlaneNormal[3];
  int pHit;
};

typedef char TraceResult_hit_offset_must_be_48[(offsetof(TraceResult, pHit) == 48) ? 1 : -1];
typedef char TraceResult_size_must_be_52[(sizeof(TraceResult) == 52) ? 1 : -1];

struct SAVERESTOREDATA
{
  int dummy;
};
