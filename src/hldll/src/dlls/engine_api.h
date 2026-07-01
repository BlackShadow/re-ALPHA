#pragma once

#include "../public/edict.h"

struct EngineApi
{
  edict_t *(*CreateEntity)();
  void *(*AllocPrivateData)(edict_t *pEdict, int size);
  void *(*GetPrivateData)(edict_t *pEdict);
  entvars_t *(*GetVarsOfEnt)(edict_t *pEdict);
  float (*CvarGetFloat)(const char *name);
  void (*EmitSound)(edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags);
  const char *(*StringFromIndex)(int index);
  void (*ServerCommand)(const char *command);
  float (*DropToFloor)(edict_t *pEdict);
  int (*EntIsOnFloor)(edict_t *pEdict);
};

extern EngineApi g_Engine;
