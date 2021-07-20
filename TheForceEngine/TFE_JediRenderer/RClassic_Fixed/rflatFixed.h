#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Level/fixedPoint.h>
#include <TFE_Level/core_math.h>
#include <TFE_Level/fixedPoint.h>

struct RSector;
struct EdgePair;

using namespace TFE_CoreMath;

namespace TFE_JediRenderer
{
	namespace RClassic_Fixed
	{
		void flat_addEdges(s32 length, s32 x0, fixed16_16 dyFloor_dx, fixed16_16 yFloor, fixed16_16 dyCeil_dx, fixed16_16 yCeil);

		void flat_drawCeiling(RSector* sector, EdgePair* edges, s32 count);
		void flat_drawFloor(RSector* sector, EdgePair* edges, s32 count);

		// Set Parameters for 3D object rendering.
		void flat_preparePolygon(fixed16_16 heightOffset, fixed16_16 offsetX, fixed16_16 offsetZ, TextureData* texture);
		void flat_drawPolygonScanline(s32 x0, s32 x1, s32 y, bool trans);
	}
}
