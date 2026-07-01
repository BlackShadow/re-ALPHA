#pragma once

#include <stdint.h>

typedef unsigned int uint;

typedef int BOOL;
typedef uint32_t DWORD;
typedef void *HINSTANCE;
typedef void *LPVOID;

struct edict_t;
struct entvars_t;
struct globalvars_t;
struct enginefuncs_t;
struct client_t;
struct KeyValueData;
struct SAVERESTOREDATA;

#ifndef DLLEXPORT
#define DLLEXPORT
#endif

#ifndef EXPORT
#define EXPORT
#endif

#define HL_UNUSED(x) (void)(x)

#ifndef HL_COMPILE_TIME_ASSERT
#define HL_COMPILE_TIME_ASSERT(condition, name) typedef char name[(condition) ? 1 : -1]
#endif
