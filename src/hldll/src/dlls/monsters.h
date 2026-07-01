#pragma once

#include "hl_types.h"

struct entvars_t;

BOOL FVisible(entvars_t* pevLooker, entvars_t* pevTarget);
BOOL FInViewCone(entvars_t* pevLooker, entvars_t* pevTarget, float flDot);
BOOL CheckFriendlyFire(entvars_t* pevShooter, float dirX, float dirY, float dirZ, float flDistance);
