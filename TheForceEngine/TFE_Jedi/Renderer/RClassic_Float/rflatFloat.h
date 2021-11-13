#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

struct RSector;

namespace TFE_Jedi
{
	struct EdgePairFloat;

	namespace RClassic_Float
	{
		void flat_addEdges(s32 length, s32 x0, f32 dyFloor_dx, f32 yFloor, f32 dyCeil_dx, f32 yCeil);

		void flat_drawCeiling(RSector* sector, EdgePairFloat* edges, s32 count);
		void flat_drawFloor(RSector* sector, EdgePairFloat* edges, s32 count);

		// Set Parameters for 3D object rendering.
		void flat_preparePolygon(f32 heightOffset, f32 offsetX, f32 offsetZ, TextureData* texture);
		void flat_drawPolygonScanline(s32 x0, s32 x1, s32 y, bool trans);
	}
}
