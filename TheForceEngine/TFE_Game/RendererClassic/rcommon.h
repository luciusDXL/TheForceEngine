#pragma once
#include <TFE_System/types.h>
#include "rwall.h"

namespace RendererClassic
{
	// Resolution
	extern s32 s_width;
	extern s32 s_height;
	extern s32 s_halfWidth;
	extern s32 s_halfHeight;

	// Projection
	extern s32 s_focalLength;

	// WallSegments
	extern RWallSegment s_wallSegListDst[MAX_SEG];
	extern RWallSegment s_wallSegListSrc[MAX_SEG];
}
