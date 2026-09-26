#pragma once

// Transitional: removed once every file includes extdll.h directly.

#include "extdll.h"

#define HL_UNUSED(x) (void)(x)

#ifndef HL_COMPILE_TIME_ASSERT
#define HL_COMPILE_TIME_ASSERT(condition, name) typedef char name[(condition) ? 1 : -1]
#endif
