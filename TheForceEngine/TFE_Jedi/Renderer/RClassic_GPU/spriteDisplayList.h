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
	struct SpriteDrawFrame
	{
		// Frame Data.
		void* basePtr;
		WaxFrame* frame;
		// Position Data.
		Vec2f v0, v1;		// base vertex positions
		Vec2f c0, c1;		// clipped vertex positions.
		f32   posY;
		// Sector.
		RSector* curSector;
		// Flags
		bool fullbright;
		// Portal data.
		u32 portalInfo;
	};

	void sprdisplayList_init(s32 startIndex);
	void sprdisplayList_destroy();

	void sprdisplayList_clear();
	void sprdisplayList_finish();

	void sprdisplayList_addFrame(const SpriteDrawFrame* const drawFrame);
	void sprdisplayList_draw();

	s32  sprdisplayList_getSize();
}  // TFE_Jedi
