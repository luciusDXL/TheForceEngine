#pragma once
//////////////////////////////////////////////////////////////////////
// Display list for rendering sector geometry
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

namespace TFE_Jedi
{
	struct WallSegment
	{
		Vec3f normal;
		Vec2f v0, v1;
		f32 y0, y1;
		f32 portalY0, portalY1;
		bool portal;
		s32 id;
		// positions projected onto the camera plane, used for sorting and overlap tests.
		f32 x0, x1;
	};

	struct WallSegBuffer
	{
		WallSegBuffer* prev;
		WallSegBuffer* next;
		WallSegment* seg;

		// Adjusted values when segments need to be split.
		f32 x0, x1;
		Vec2f v0, v1;
	};
}  // TFE_Jedi
