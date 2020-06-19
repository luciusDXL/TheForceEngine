#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"

struct RSector;
struct EdgePair;

namespace RClassicFlat
{
	void flat_addEdges(s32 length, s32 x0, fixed16 dyFloor_dx, fixed16 yFloor, fixed16 dyCeil_dx, fixed16 yCeil);

	void flat_drawCeiling(RSector* sector, EdgePair* edges, s32 count);
	void flat_drawFloor(RSector* sector, EdgePair* edges, s32 count);
}