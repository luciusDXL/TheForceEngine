#pragma once
//////////////////////////////////////////////////////////////////////
// Renderer Debugging
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "../rsectorRender.h"

namespace TFE_Jedi
{
	void renderDebug_enable(bool enable);
	void renderDebug_addLine(Vec3f v0, Vec3f v1, const u8* color);
	void renderDebug_addPolygon(s32 count, Vec3f* vtx, Vec4f color);
	void renderDebug_draw();
}  // TFE_Jedi
