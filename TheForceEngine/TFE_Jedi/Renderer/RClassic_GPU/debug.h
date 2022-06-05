#pragma once
//////////////////////////////////////////////////////////////////////
// Renderer Debug 
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

namespace TFE_Jedi
{
	extern s32 s_maxPortals;
	extern s32 s_maxWallSeg;
	extern s32 s_portalsTraversed;
	extern s32 s_wallSegGenerated;

	void debug_update();
	void debug_addQuad(Vec2f v0, Vec2f v1, f32 y0, f32 y1, f32 portalY0, f32 portalY1, bool isPortal);
}  // TFE_Jedi
