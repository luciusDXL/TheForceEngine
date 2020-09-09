#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "../rmath.h"

struct RSector;
struct EdgePair;

namespace TFE_JediRenderer
{
	namespace RClassic_Float
	{
		void flat_addEdges(s32 length, s32 x0, f32 dyFloor_dx, f32 yFloor, f32 dyCeil_dx, f32 yCeil);

		void flat_drawCeiling(RSector* sector, EdgePair* edges, s32 count);
		void flat_drawFloor(RSector* sector, EdgePair* edges, s32 count);
	}
}
