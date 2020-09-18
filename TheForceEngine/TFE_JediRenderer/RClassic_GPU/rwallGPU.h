#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "../rmath.h"
#include "../rlimits.h"
#include "../rwall.h"

struct RSector;
struct EdgePair;
struct TextureFrame;

namespace TFE_JediRenderer
{
	namespace RClassic_GPU
	{
		void wall_process(RWall* wall);
		s32  wall_mergeSort(RWallSegment* segOutList, s32 availSpace, s32 start, s32 count);

	#if 0
		void wall_drawSolid(RWallSegment* wallSegment);
		void wall_drawTransparent(RWallSegment* wallSegment, EdgePair* edge);
		void wall_drawMask(RWallSegment* wallSegment);
		void wall_drawBottom(RWallSegment* wallSegment);
		void wall_drawTop(RWallSegment* wallSegment);
		void wall_drawTopAndBottom(RWallSegment* wallSegment);

		void wall_drawSkyTop(RSector* sector);
		void wall_drawSkyTopNoWall(RSector* sector);
		void wall_drawSkyBottom(RSector* sector);
		void wall_drawSkyBottomNoWall(RSector* sector);
	#endif

		void wall_addAdjoinSegment(s32 length, s32 x0, f32 top_dydx, f32 y1, f32 bot_dydx, f32 y0, RWallSegment* wallSegment);

		void wall_setupAdjoinDrawFlags(RWall* wall);
		void wall_computeTexelHeights(RWall* wall);
	}
}
