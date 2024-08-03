#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <algorithm>

namespace TFE_Editor
{
	// min(a, b, c)
	inline f32 min3(f32 a, f32 b, f32 c)
	{
		return std::min(a, std::min(b, c));
	}
	// max(a, b, c)
	inline f32 max3(f32 a, f32 b, f32 c)
	{
		return std::max(a, std::max(b, c));
	}
	// Return true if the 1D intervals [a0, a1] and [b0, b1] overlap.
	inline bool intervalOverlap(f32 a0, f32 a1, f32 b0, f32 b1)
	{
		return a0 <= b1 && b0 <= a1;
	}
	// Return true if the 2D bounds (packed as Vec4f) overlap.
	inline bool boundsOverlap(Vec4f bounds0, Vec4f bounds1, f32 padding = 0.0f)
	{
		// We only need to apply padding to one set of bounds, so pick the first one.
		return intervalOverlap(bounds0.x - padding, bounds0.z + padding, bounds1.x, bounds1.z) && // x intervals overlap
			   intervalOverlap(bounds0.y - padding, bounds0.w + padding, bounds1.y, bounds1.w);   // y intervals overlap
	}
}
