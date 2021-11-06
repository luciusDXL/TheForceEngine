#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

struct RSector;
struct EdgePairFixed;

namespace TFE_Jedi
{
	namespace RClassic_Fixed
	{
		void flat_addEdges(s32 length, s32 x0, fixed16_16 dyFloor_dx, fixed16_16 yFloor, fixed16_16 dyCeil_dx, fixed16_16 yCeil);

		void flat_drawCeiling(RSector* sector, EdgePairFixed* edges, s32 count);
		void flat_drawFloor(RSector* sector, EdgePairFixed* edges, s32 count);

		// Set Parameters for 3D object rendering.
		void flat_preparePolygon(fixed16_16 heightOffset, fixed16_16 offsetX, fixed16_16 offsetZ, TextureData* texture);
		void flat_drawPolygonScanline(s32 x0, s32 x1, s32 y, bool trans);
	}
}
