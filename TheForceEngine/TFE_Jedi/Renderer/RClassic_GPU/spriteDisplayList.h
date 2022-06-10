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

namespace TFE_Jedi
{
	void sprdisplayList_init(s32 posTextureIndex, s32 scaleOffsetIndex);
	void sprdisplayList_destroy();

	void sprdisplayList_clear();
	void sprdisplayList_finish();

	void sprdisplayList_addFrame(void* basePtr, WaxFrame* frame, Vec3f posWS, RSector* curSector);
	void sprdisplayList_draw();

	s32  sprdisplayList_getSize();
}  // TFE_Jedi
