#pragma once

#include "hl_types.h"

struct entvars_t;

BOOL GetTossTarget(float* out, entvars_t* pevOwner, const float* start, const float* target);
void ShootTimedGrenade(entvars_t* pevOwner, const float* start, const float* velocity);

