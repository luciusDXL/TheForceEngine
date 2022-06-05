#pragma once
//////////////////////////////////////////////////////////////////////
// S-Buffer (S = span or segment) used to sort and clip walls in
// world space based on the camera position.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

namespace TFE_Jedi
{
	struct Segment
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

	struct SegmentClipped
	{
		SegmentClipped* prev;
		SegmentClipped* next;
		Segment* seg;

		// Adjusted values when segments need to be split.
		f32 x0, x1;
		Vec2f v0, v1;
	};

	f32   sbuffer_projectToUnitSquare(Vec2f coord);
	void  sbuffer_handleEdgeWrapping(f32& x0, f32& x1);
	bool  sbuffer_splitByRange(Segment* seg, Vec2f* range, Vec2f* points, s32 rangeCount);
	Vec2f sbuffer_clip(Vec2f v0, Vec2f v1, Vec2f pointOnPlane);

	void sbuffer_clear();
	void sbuffer_mergeSegments();
	void sbuffer_insertSegment(Segment* seg);
	SegmentClipped* sbuffer_get();

	void sbuffer_debugDisplay();
}  // TFE_Jedi
