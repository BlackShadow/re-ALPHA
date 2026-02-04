#ifndef EIFACE_H
#define EIFACE_H

#include "edict.h"
#include "mathlib.h"

 // Returned by TraceLine/ TraceToss for game DLL interface.
typedef struct TraceResult_s
{
	int     fAllSolid; // if true, plane is not valid
	int     fStartSolid; // if true, the initial point was in a solid area
	int     fInOpen;
	int     fInWater;
	float   flFraction; // time completed, 1.0 = didn't hit anything
	vec3_t  vecEndPos; // final position
	float   flPlaneDist;
	vec3_t  vecPlaneNormal; // surface normal at impact
	int     pHit; // edict offset (0 == world/ no hit)
} TraceResult;

void DispatchEntityCallback(int callbackIndex);

#endif // EIFACE_H
