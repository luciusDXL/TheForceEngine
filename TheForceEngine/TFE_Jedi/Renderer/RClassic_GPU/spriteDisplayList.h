#pragma once
//////////////////////////////////////////////////////////////////////
// Display list for rendering sprites (WAX & FME).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include "sbuffer.h"

//#define SPRITE_CPU_CLIPPING 1

namespace TFE_Jedi
{
	void sprdisplayList_init(s32 startIndex);
	void sprdisplayList_destroy();

	void sprdisplayList_clear();
	void sprdisplayList_finish();

#ifdef SPRITE_CPU_CLIPPING
	void sprdisplayList_addFrame(void* basePtr, WaxFrame* frame, Vec2f v0, Vec2f v1, Vec2f c0, Vec2f c1, f32 posY, RSector* curSector, bool fullbright);
#else
	void sprdisplayList_addFrame(void* basePtr, WaxFrame* frame, Vec3f posWS, RSector* curSector, bool fullbright);
#endif
	void sprdisplayList_draw();

	s32  sprdisplayList_getSize();
}  // TFE_Jedi
