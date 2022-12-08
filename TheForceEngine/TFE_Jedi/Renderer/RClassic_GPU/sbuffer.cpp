#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_System/math.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include <TFE_Input/input.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include "sbuffer.h"
#include "debug.h"
#include "rclassicGPU.h"
#include "../rcommon.h"

#include <cmath>

using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	extern Vec3f s_cameraPos;
	const f32 c_sideEps = 0.0001f;

	enum
	{
		SEG_CLIP_POOL_SIZE = 8192
	};

	SegmentClipped s_segClippedPool[SEG_CLIP_POOL_SIZE];
	SegmentClipped* s_segClippedHead;
	SegmentClipped* s_segClippedTail;
	s32 s_segClippedPoolCount = 0;

	SegmentClipped* sbuffer_getClippedSeg(Segment* seg);
	void insertSegmentBefore(SegmentClipped* cur, SegmentClipped* seg);
	void insertSegmentAfter(SegmentClipped* cur, SegmentClipped* seg);
	void deleteSegment(SegmentClipped* cur);
	void swapSegments(SegmentClipped* a, SegmentClipped* b);
	bool segmentInFront(Vec2f w0left, Vec2f w0right, Vec3f w0nrml, Vec2f w1left, Vec2f w1right, Vec3f w1nrml);
	bool segmentsOverlap(f32 ax0, f32 ax1, f32 bx0, f32 bx1);
	SegmentClipped* mergeSegments(SegmentClipped* a, SegmentClipped* b);

	///////////////////////////////////////////////////////////////
	// API
	///////////////////////////////////////////////////////////////
		
	// Project a 2D coordinate onto the unit square centered around the camera.
	// This returns back a single value where 0.5 = +x,0; 1.5 = 0,+z; 2.5 = -x,0; 3.5 = 0,-z
	// This is done for fast camera relative overlap tests for segments on the XZ axis in a way that
	// is independent of the camera orientation.
	// Once done, segments can be tested for overlap simply by comparing these values - note some care
	// must be taken to pitch the "shortest route" as 4.0 == 0.0
	f32 sbuffer_projectToUnitSquare(Vec2f coord)
	{
		coord.x -= s_cameraPos.x;
		coord.z -= s_cameraPos.z;
		// Handle the singularity.
		if (fabsf(coord.x) < FLT_EPSILON && fabsf(coord.z) < FLT_EPSILON)
		{
			return 0.0f;
		}

		// Find the largest axis.
		s32 axis = 0;
		if (fabsf(coord.x) > fabsf(coord.z))
		{
			axis = coord.x < 0.0f ? 2 : 0;
		}
		else
		{
			axis = coord.z < 0.0f ? 3 : 1;
		}
		const f32 axisScale = coord.m[axis & 1] != 0.0f ? 1.0f / fabsf(coord.m[axis & 1]) : 0.0f;
		f32 value = coord.m[(1 + axis) & 1] * axisScale * 0.5f + 0.5f;
		if (axis == 1 || axis == 2) { value = 1.0f - value; }

		// Switch the direction to match the wall direction.
		f32 projection = 4.0f - fmodf(f32(axis) + value, 4.0f);	// this wraps around to 0.0 at 4.0
		// This zero checks above should catch singularities, but since this causes crashes I want to be extra sure. :)
		if (!std::isfinite(projection))
		{
			projection = 0.0f;
		}
		return projection;
	}

	void sbuffer_handleEdgeWrapping(f32& x0, f32& x1)
	{
		// Make sure the segment is the correct length.
		if (fabsf(x1 - x0) > 2.0f)
		{
			if (x0 < 1.0f) { x0 += 4.0f; }
			else { x1 += 4.0f; }
		}
		// Handle wrapping.
		while (x0 < 0.0f || x1 < 0.0f)
		{
			x0 += 4.0f;
			x1 += 4.0f;
		}
		while (x0 >= 4.0f && x1 >= 4.0f)
		{
			x0 = fmodf(x0, 4.0f);
			x1 = fmodf(x1, 4.0f);
		}
	}

	bool sbuffer_splitByRange(Segment* seg, Vec2f* range, Vec2f* points, s32 rangeCount)
	{
		// Clip a single segment against one or two ranges...
		bool inside = false;
		for (s32 r = 0; r < rangeCount; r++)
		{
			if (seg->x1 <= range[r].x || seg->x0 >= range[r].z)
			{
				continue;
			}
			inside = true;

			if (seg->x0 < range[r].x && seg->x1 > range[r].x)
			{
				seg->v0 = sbuffer_clip(seg->v0, seg->v1, points[0]);
				seg->x0 = range[r].x;
			}
			if (seg->x1 > range[r].z && seg->x0 < range[r].z)
			{
				seg->v1 = sbuffer_clip(seg->v0, seg->v1, points[1]);
				seg->x1 = range[r].z;
			}
		}

		return inside;
	}
	
	Vec2f sbuffer_clip(Vec2f v0, Vec2f v1, Vec2f pointOnPlane)
	{
		Vec2f p1 = { pointOnPlane.x - s_cameraPos.x, pointOnPlane.z - s_cameraPos.z };
		Vec2f planeNormal = { -p1.z, p1.x };
		f32 lenSq = p1.x*p1.x + p1.z*p1.z;
		f32 scale = (lenSq > FLT_EPSILON) ? 1.0f / sqrtf(lenSq) : 0.0f;

		f32 d0 = ((v0.x - s_cameraPos.x)*planeNormal.x + (v0.z - s_cameraPos.z)*planeNormal.z) * scale;
		f32 d1 = ((v1.x - s_cameraPos.x)*planeNormal.x + (v1.z - s_cameraPos.z)*planeNormal.z) * scale;
		f32 t = -d0 / (d1 - d0);

		Vec2f intersection;
		intersection.x = (1.0f-t)*v0.x + t * v1.x;
		intersection.z = (1.0f-t)*v0.z + t * v1.z;
		return intersection;
	}

	void sbuffer_clear()
	{
		s_segClippedHead = nullptr;
		s_segClippedTail = nullptr;
		s_segClippedPoolCount = 0;
	}

	void sbuffer_mergeSegments()
	{
		SegmentClipped* cur = s_segClippedHead;
		while (cur)
		{
			SegmentClipped* curNext = cur->next;
			while (curNext && cur->x1 == curNext->x0 && cur->seg->id == curNext->seg->id)
			{
				cur = mergeSegments(cur, curNext);
				curNext = cur->next;
			}
			cur = cur->next;
		}

		// Try to merge the head and tail because they might have been split on the modulo line.
		/*
		if (s_segClippedHead != s_segClippedTail)
		{
			if (s_segClippedHead->x0 == 0.0f && s_segClippedTail->x1 == 4.0f && s_segClippedHead->seg->id == s_segClippedTail->seg->id)
			{
				s_segClippedTail->x1 = s_segClippedHead->x1 + 4.0f;
				s_segClippedTail->v1 = s_segClippedHead->v1;
				s_segClippedHead = s_segClippedHead->next;
				s_segClippedHead->prev = nullptr;
			}
		}
		*/
	}

	void sbuffer_insertSegment(Segment* seg)
	{
		if (!s_segClippedHead)
		{
			s_segClippedHead = sbuffer_getClippedSeg(seg);
			s_segClippedTail = s_segClippedHead;
			return;
		}

		// Go through and see if there is an overlap.
		SegmentClipped* cur = s_segClippedHead;
		while (cur)
		{
			// Do the segments overlap?
			if (segmentsOverlap(seg->x0, seg->x1, cur->x0, cur->x1))
			{
				bool curInFront = segmentInFront(seg->v0, seg->v1, seg->normal, cur->v0, cur->v1, cur->seg->normal);
				if (curInFront)
				{
					// Seg can be discarded, which means we are done here.
					if (seg->x0 >= cur->x0 && seg->x1 <= cur->x1) { return; }

					s32 clipFlags = 0;
					if (seg->x0 < cur->x0) { clipFlags |= 1; } // Seg sticks out of the left side and should be clipped.
					if (seg->x1 > cur->x1) { clipFlags |= 2; } // Seg sticks out of the right side and should be clipped.
					if (!clipFlags) { return; }

					// If the left side needs to be clipped, it can be added without continue with loop.
					if (clipFlags & 1)
					{
						SegmentClipped* newEntry = sbuffer_getClippedSeg(seg);
						newEntry->x1 = cur->x0;
						newEntry->v1 = sbuffer_clip(seg->v0, seg->v1, cur->v0);

						insertSegmentBefore(cur, newEntry);
					}
					// If the right side is clipped, then we must continue with the loop.
					if (clipFlags & 2)
					{
						seg->x0 = cur->x1;
						seg->v0 = sbuffer_clip(seg->v0, seg->v1, cur->v1);
					}
					else
					{
						return;
					}
				}
				else
				{
					s32 clipFlags = 0;
					if (cur->x0 < seg->x0) { clipFlags |= 1; } // Current segment is partially on the left.
					if (cur->x1 > seg->x1) { clipFlags |= 2; } // Current segment is partially on the right.
					if (seg->x1 > cur->x1) { clipFlags |= 4; } // Create a new segment on the right side of the current segment.

					// Save so the current segment can be modified.
					const f32 curX1 = cur->x1;
					const Vec2f curV1 = cur->v1;

					if (clipFlags & 1)
					{
						// [cur | segEntry ...]
						cur->x1 = seg->x0;
						cur->v1 = sbuffer_clip(cur->v0, cur->v1, seg->v0);
					}
					if (clipFlags & 2)
					{
						SegmentClipped* segEntry = sbuffer_getClippedSeg(seg);
						SegmentClipped* curRight = sbuffer_getClippedSeg(cur->seg);
						curRight->x0 = seg->x1;
						curRight->v0 = sbuffer_clip(cur->v0, curV1, seg->v1);
						curRight->x1 = curX1;
						curRight->v1 = curV1;

						insertSegmentAfter(cur, segEntry);
						insertSegmentAfter(segEntry, curRight);
						if (!(clipFlags & 1))
						{
							deleteSegment(cur);
						}
						return;
					}
					else if (clipFlags & 4)
					{
						// New segment that gets clipped by the edge of 'cur'.
						SegmentClipped* segEntry = sbuffer_getClippedSeg(seg);
						segEntry->x1 = curX1;
						segEntry->v1 = sbuffer_clip(seg->v0, seg->v1, curV1);

						// Left over part for the rest of the loop.
						seg->x0 = segEntry->x1;
						seg->v0 = segEntry->v1;

						insertSegmentAfter(cur, segEntry);
						if (!(clipFlags & 1))
						{
							deleteSegment(cur);
						}

						// continue with loop...
						cur = segEntry;
					}
					else  // Insert the full new segment.
					{
						insertSegmentAfter(cur, sbuffer_getClippedSeg(seg));
						if (!(clipFlags & 1))
						{
							deleteSegment(cur);
						}
						return;
					}
				}
			}
			else if (seg->x1 <= cur->x0) // Left
			{
				insertSegmentBefore(cur, sbuffer_getClippedSeg(seg));
				return;
			}

			cur = cur->next;
		}
		// The new segment is to the right of everything.
		insertSegmentAfter(s_segClippedTail, sbuffer_getClippedSeg(seg));
	}

	SegmentClipped* sbuffer_get()
	{
		return s_segClippedHead;
	}

	SegmentClipped* sbuffer_getClippedSeg(Segment* seg, SegmentClipped* dstSegs, s32 maxOutputSegs, s32& dstSegCount)
	{
		if (dstSegCount >= maxOutputSegs)
		{
			assert(0);
			TFE_System::logWrite(LOG_ERROR, "SegBuffer", "Too many clipped segs allocated - max is %d", maxOutputSegs);
			return nullptr;
		}
		SegmentClipped* segClipped = &dstSegs[dstSegCount];
		segClipped->prev = nullptr;
		segClipped->next = nullptr;
		segClipped->seg = seg;
		if (seg)
		{
			segClipped->x0 = seg->x0;
			segClipped->x1 = seg->x1;
			segClipped->v0 = seg->v0;
			segClipped->v1 = seg->v1;
		}
		dstSegCount++;
		return segClipped;
	}

	s32 sbuffer_convertToSegments(Vec2f v0, Vec2f v1, s32 id, s32 rangeCount, Vec2f* range, Vec2f* rangeSrc, Segment* segments)
	{
		s32 segCount = 1;
		segments[0].v0 = v0;
		segments[0].v1 = v1;
		segments[0].id = id;
		segments[0].normal = { -(v1.z - v0.z), 0.0f, v1.x - v0.x };
		segments[0].x0 = sbuffer_projectToUnitSquare(segments[0].v0);
		segments[0].x1 = sbuffer_projectToUnitSquare(segments[0].v1);

		// This means both vertices map to the same point on the unit square, in other words, the edge isn't actually visible.
		if (fabsf(segments[0].x0 - segments[0].x1) < FLT_EPSILON)
		{
			return 0;
		}
		
		// Project the edge.
		sbuffer_handleEdgeWrapping(segments[0].x0, segments[0].x1);
		// Check again for zero-length walls in case the fix-ups above caused it (for example, x0 = 0.0, x1 = 4.0).
		if (segments[0].x0 >= segments[0].x1 || segments[0].x1 - segments[0].x0 < FLT_EPSILON)
		{
			return 0;
		}
		assert(segments[0].x1 - segments[0].x0 > 0.0f);

		// Split segments that cross the modulo boundary.
		if (segments[0].x1 > 4.0f)
		{
			const f32 sx1 = segments[0].x1;
			const Vec2f sv1 = segments[0].v1;

			// Split the segment at the modulus border.
			segments[0].v1 = sbuffer_clip(segments[0].v0, segments[0].v1, { 1.0f + s_cameraPos.x, -1.0f + s_cameraPos.z });
			segments[0].x1 = 4.0f;
			Vec2f newV1 = segments[0].v1;

			s32 nextIndex = 1;
			if (rangeCount && !sbuffer_splitByRange(&segments[0], range, rangeSrc, rangeCount))
			{
				segCount = 1;
				nextIndex = 0;
			}
			else
			{
				segCount = 2;
				segments[1] = segments[0];
			}

			segments[nextIndex].x0 = 0.0f;
			segments[nextIndex].x1 = sx1 - 4.0f;
			segments[nextIndex].v0 = newV1;
			segments[nextIndex].v1 = sv1;
			if (rangeCount && !sbuffer_splitByRange(&segments[nextIndex], range, rangeSrc, rangeCount))
			{
				segCount--;
			}
		}
		else if (rangeCount)
		{
			if (!sbuffer_splitByRange(&segments[0], range, rangeSrc, rangeCount))
			{
				segCount = 0;
			}
		}

		return segCount;
	}

	// Returns the state of the overlap between 2 segments, whether they one is in front of the other or they are intersecting.
	// Handle 3 cases:
	// 1. Segment 0 is a seperating axis.
	// 2. Segment 1 is a seperating axis.
	// 3. Segment 0 and Segment 1 intersect.
	// Case 1 & 2 can result in the seg0 or seg1 in front state.
	// Case 3 always results in the intersect state.
	enum SegmentOverlapState
	{
		SOS_SEG0_FRONT = 0,
		SOS_SEG1_FRONT,
		SOS_SEG_INTERSECT,
	};

	SegmentOverlapState getSegmentOverlapState(Vec2f w0left, Vec2f w0right, Vec3f w0nrml, Vec2f w1left, Vec2f w1right, Vec3f w1nrml)
	{
		// Are w1left and w1right both on the same side of w0nrml as the camera position?
		const Vec2f left0 = { w1left.x - w0left.x, w1left.z - w0left.z };
		const Vec2f right0 = { w1right.x - w0left.x, w1right.z - w0left.z };
		const Vec2f camera0 = { s_cameraPos.x - w0left.x, s_cameraPos.z - w0left.z };
		f32 sideLeft0 = w0nrml.x*left0.x + w0nrml.z*left0.z;
		f32 sideRight0 = w0nrml.x*right0.x + w0nrml.z*right0.z;
		f32 cameraSide0 = w0nrml.x*camera0.x + w0nrml.z*camera0.z;

		// Handle floating point precision.
		if (fabsf(sideLeft0) < c_sideEps) { sideLeft0 = 0.0f; }
		if (fabsf(sideRight0) < c_sideEps) { sideRight0 = 0.0f; }
		if (fabsf(cameraSide0) < c_sideEps) { cameraSide0 = 0.0f; }

		// both vertices of 'w1' are on the same side of 'w0'
		// 'w1' is in front if the camera is on the same side.
		if (sideLeft0 <= 0.0f && sideRight0 <= 0.0f)
		{
			return cameraSide0 <= 0.0f ? SOS_SEG1_FRONT : SOS_SEG0_FRONT;
		}
		else if (sideLeft0 >= 0.0f && sideRight0 >= 0.0f)
		{
			return cameraSide0 >= 0.0f ? SOS_SEG1_FRONT : SOS_SEG0_FRONT;
		}

		// Are w0left and w0right both on the same side of w1nrml as the camera position?
		const Vec2f left1 = { w0left.x - w1left.x, w0left.z - w1left.z };
		const Vec2f right1 = { w0right.x - w1left.x, w0right.z - w1left.z };
		const Vec2f camera1 = { s_cameraPos.x - w1left.x, s_cameraPos.z - w1left.z };
		f32 sideLeft1 = w1nrml.x*left1.x + w1nrml.z*left1.z;
		f32 sideRight1 = w1nrml.x*right1.x + w1nrml.z*right1.z;
		f32 cameraSide1 = w1nrml.x*camera1.x + w1nrml.z*camera1.z;

		// Handle floating point precision.
		if (fabsf(sideLeft1) < c_sideEps) { sideLeft1 = 0.0f; }
		if (fabsf(sideRight1) < c_sideEps) { sideRight1 = 0.0f; }
		if (fabsf(cameraSide1) < c_sideEps) { cameraSide1 = 0.0f; }

		// both vertices of 'w0' are on the same side of 'w1'
		// 'w0' is in front if the camera is on the same side.
		if (sideLeft1 <= 0.0f && sideRight1 <= 0.0f)
		{
			return cameraSide1 <= 0.0f ? SOS_SEG0_FRONT : SOS_SEG1_FRONT;
		}
		else if (sideLeft1 >= 0.0f && sideRight1 >= 0.0f)
		{
			return cameraSide1 >= 0.0f ? SOS_SEG0_FRONT : SOS_SEG1_FRONT;
		}

		// If we still get here, just do a quick distance check.
		return SOS_SEG_INTERSECT;
	}

	// Clips a segment to the buffer but does *not* update the s-buffer itself.
	// The result will be zero or more output segments.
	// TODO: Handle unique cases -
	//   1. If clip rule returns false: check if either point of src segment is in front, if so than it *all* is otherwise *none* of it is.
	//	 2. If clip rule returns true or not an adjoin: if "in front" test is inconclusive, clip the src and cur segment lines to determine part in front.
	//      (This takes the place of the z-buffer test)
	s32 sbuffer_clipSegmentToBuffer(Vec2f v0, Vec2f v1, s32 rangeCount, Vec2f* range, Vec2f* rangeSrc, s32 maxOutputSegs, SegmentClipped* dstSegs, SBufferClipRule clipRule)
	{
		// Invalid state.
		if (!s_segClippedHead || !s_segClippedTail) { return 0; }

		// Convert from positions to segments.
		// Early return if no segments are generated.
		Segment srcSeg[8] = { 0 };
		const s32 srcSegCount = sbuffer_convertToSegments(v0, v1, 1, rangeCount, range, rangeSrc, srcSeg);
		if (!srcSegCount) { return 0; }

		s32 newSegCount = 0;
		for (s32 s = 0; s < srcSegCount; s++)
		{
			Segment* seg = &srcSeg[srcSegCount - s - 1];

			SegmentClipped* cur = s_segClippedHead;
			bool addSegEnd = true;
			while (cur)
			{
				// Do the segments overlap?
				if (segmentsOverlap(seg->x0, seg->x1, cur->x0, cur->x1))
				{
					SegmentOverlapState overlapState;

					// "Failing" the clip rule means we treat it as an adjoin instead of a wall.
					Segment* curSeg = cur->seg;
					if (curSeg->portal && clipRule && !clipRule(curSeg->id))
					{
						// Clip seg to cur for testing which is in front.
						f32   segX0 = seg->x0, segX1 = seg->x1;
						Vec2f segV0 = seg->v0, segV1 = seg->v1;
						if (seg->x0 < cur->x0)
						{
							segX0 = cur->x0;
							segV0 = sbuffer_clip(seg->v0, seg->v1, cur->v0);
						}
						if (seg->x1 > cur->x1)
						{
							segX1 = cur->x1;
							segV1 = sbuffer_clip(seg->v0, seg->v1, cur->v1);
						}

						// Determine if any part of the segment, within the portal range is in front.
						// If so then the *entire* segment is considered to be in front.
						const Vec2f left0 = { segV0.x - cur->v0.x, segV0.z - cur->v0.z };
						const Vec2f right0 = { segV1.x - cur->v0.x, segV1.z - cur->v0.z };
						const Vec2f camera0 = { s_cameraPos.x - cur->v0.x, s_cameraPos.z - cur->v0.z };
						const Vec3f& nrml = curSeg->normal;
						f32 sideLeft0 = nrml.x*left0.x + nrml.z*left0.z;
						f32 sideRight0 = nrml.x*right0.x + nrml.z*right0.z;
						f32 cameraSide0 = nrml.x*camera0.x + nrml.z*camera0.z;

						// Handle floating point precision.
						if (fabsf(sideLeft0) < c_sideEps) { sideLeft0 = 0.0f; }
						if (fabsf(sideRight0) < c_sideEps) { sideRight0 = 0.0f; }
						if (fabsf(cameraSide0) < c_sideEps) { cameraSide0 = 0.0f; }

						// seg is "in front" of cur.
						if (((sideLeft0 <= 0.0f || sideRight0 <= 0.0f) && cameraSide0 <= 0.0f) ||
							((sideLeft0 >= 0.0f || sideRight0 >= 0.0f) && cameraSide0 >= 0.0f))
						{
							overlapState = SOS_SEG0_FRONT;
						}
						else // seg is "behind" cur.
						{
							overlapState = SOS_SEG1_FRONT;
						}
					}
					else
					{
						overlapState = getSegmentOverlapState(seg->v0, seg->v1, seg->normal, cur->v0, cur->v1, cur->seg->normal);
					}

					// Otherwise clip the src segment as needed.
					if (overlapState == SOS_SEG1_FRONT)
					{
						// Seg can be discarded, which means we are done here.
						if (seg->x0 >= cur->x0 && seg->x1 <= cur->x1)
						{
							addSegEnd = false;
							break;
						}

						s32 clipFlags = 0;
						if (seg->x0 < cur->x0) { clipFlags |= 1; } // Seg sticks out of the left side and should be clipped.
						if (seg->x1 > cur->x1) { clipFlags |= 2; } // Seg sticks out of the right side and should be clipped.
						if (!clipFlags)
						{
							addSegEnd = false;
							break;
						}

						// If the left side needs to be clipped, it can be added without continue with loop.
						if (clipFlags & 1)
						{
							SegmentClipped* newEntry = sbuffer_getClippedSeg(seg, dstSegs, maxOutputSegs, newSegCount);
							newEntry->x1 = cur->x0;
							newEntry->v1 = sbuffer_clip(seg->v0, seg->v1, cur->v0);
						}
						// If the right side is clipped, then we must continue with the loop.
						if (clipFlags & 2)
						{
							seg->x0 = cur->x1;
							seg->v0 = sbuffer_clip(seg->v0, seg->v1, cur->v1);
						}
						else
						{
							addSegEnd = false;
							break;
						}
					}
					else if (overlapState == SOS_SEG0_FRONT)
					{
						if (seg->x1 > cur->x1)
						{
							// New segment that gets clipped by the edge of 'cur'.
							SegmentClipped* segEntry = sbuffer_getClippedSeg(seg, dstSegs, maxOutputSegs, newSegCount);
							segEntry->x1 = cur->x1;
							segEntry->v1 = sbuffer_clip(seg->v0, seg->v1, cur->v1);

							// Left over part for the rest of the loop.
							seg->x0 = segEntry->x1;
							seg->v0 = segEntry->v1;
						}
						else  // Insert the full new segment.
						{
							sbuffer_getClippedSeg(seg, dstSegs, maxOutputSegs, newSegCount);
							addSegEnd = false;
							break;
						}
					}
					else  // SOS_SEG_INTERSECT
					{
						// TODO: Determine where the intersect occurs and then split seg accordingly.
					}
				}
				else if (seg->x1 <= cur->x0) // Left
				{
					sbuffer_getClippedSeg(seg, dstSegs, maxOutputSegs, newSegCount);
					addSegEnd = false;
					break;
				}

				cur = cur->next;
			}
			// The new segment is to the right of everything.
			if (addSegEnd)
			{
				sbuffer_getClippedSeg(seg, dstSegs, maxOutputSegs, newSegCount);
			}
		}

		// Next attempt to merge the segments back together.
		s32 segToDelete[16];
		while (newSegCount > 1)
		{
			s32 segToDeleteCount = 0;

			for (s32 i = 0; i < newSegCount && segToDeleteCount < 16; i++)
			{
				const s32 i0 = i, i1 = (i0 + 1) % newSegCount;
				if (dstSegs[i0].x1 == dstSegs[i1].x0 || (dstSegs[i0].x1 == 4.0f && dstSegs[i1].x0 == 0.0f))
				{
					const f32 offset = (dstSegs[i0].x1 == 4.0f && dstSegs[i1].x0 == 0.0f) ? 4.0f : 0.0f;
					dstSegs[i0].x1 = dstSegs[i1].x1 + offset;
					dstSegs[i0].v1 = dstSegs[i1].v1;
					if (offset > 0.0f)
					{
						// This will add delete 0, which should obviously be at the beginning...
						for (s32 d = segToDeleteCount; d > 0; d--)
						{
							segToDelete[d] = segToDelete[d - 1];
						}
						segToDelete[0] = i1;
						segToDeleteCount++;
					}
					else
					{
						segToDelete[segToDeleteCount++] = i1;
					}
					// Skip the next segment since it was just merged.
					i++;
				}
			}

			if (!segToDeleteCount) { break; }
			for (s32 i = segToDeleteCount - 1; i >= 0; i--)
			{
				for (s32 s = segToDelete[i]; s < newSegCount - 1; s++)
				{
					dstSegs[s] = dstSegs[s + 1];
				}
				newSegCount--;
			}
		}

		return newSegCount;
	}

	void sbuffer_debugDisplay()
	{
		const SegmentClipped* wallSeg = s_segClippedHead;
		while (wallSeg)
		{
			const Segment* seg = wallSeg->seg;
			debug_addQuad(wallSeg->v0, wallSeg->v1, seg->y0, seg->y1, seg->portalY0, seg->portalY1, seg->portal);
			wallSeg = wallSeg->next;
		}
	}

	///////////////////////////////////////////////////////////////
	// Internal Implementation
	///////////////////////////////////////////////////////////////
	// Returns true if 'w1' is in front of 'w0'
	bool segmentInFront(Vec2f w0left, Vec2f w0right, Vec3f w0nrml, Vec2f w1left, Vec2f w1right, Vec3f w1nrml)
	{
		// Are w1left and w1right both on the same side of w0nrml as the camera position?
		const Vec2f left0 = { w1left.x - w0left.x, w1left.z - w0left.z };
		const Vec2f right0 = { w1right.x - w0left.x, w1right.z - w0left.z };
		const Vec2f camera0 = { s_cameraPos.x - w0left.x, s_cameraPos.z - w0left.z };
		f32 sideLeft0 = w0nrml.x*left0.x + w0nrml.z*left0.z;
		f32 sideRight0 = w0nrml.x*right0.x + w0nrml.z*right0.z;
		f32 cameraSide0 = w0nrml.x*camera0.x + w0nrml.z*camera0.z;

		// Handle floating point precision.
		if (fabsf(sideLeft0) < c_sideEps) { sideLeft0 = 0.0f; }
		if (fabsf(sideRight0) < c_sideEps) { sideRight0 = 0.0f; }
		if (fabsf(cameraSide0) < c_sideEps) { cameraSide0 = 0.0f; }

		// both vertices of 'w1' are on the same side of 'w0'
		// 'w1' is in front if the camera is on the same side.
		if (sideLeft0 <= 0.0f && sideRight0 <= 0.0f)
		{
			return cameraSide0 <= 0.0f;
		}
		else if (sideLeft0 >= 0.0f && sideRight0 >= 0.0f)
		{
			return cameraSide0 >= 0.0f;
		}

		// Are w0left and w0right both on the same side of w1nrml as the camera position?
		const Vec2f left1 = { w0left.x - w1left.x, w0left.z - w1left.z };
		const Vec2f right1 = { w0right.x - w1left.x, w0right.z - w1left.z };
		const Vec2f camera1 = { s_cameraPos.x - w1left.x, s_cameraPos.z - w1left.z };
		f32 sideLeft1 = w1nrml.x*left1.x + w1nrml.z*left1.z;
		f32 sideRight1 = w1nrml.x*right1.x + w1nrml.z*right1.z;
		f32 cameraSide1 = w1nrml.x*camera1.x + w1nrml.z*camera1.z;

		// Handle floating point precision.
		if (fabsf(sideLeft1) < c_sideEps) { sideLeft1 = 0.0f; }
		if (fabsf(sideRight1) < c_sideEps) { sideRight1 = 0.0f; }
		if (fabsf(cameraSide1) < c_sideEps) { cameraSide1 = 0.0f; }

		// both vertices of 'w0' are on the same side of 'w1'
		// 'w0' is in front if the camera is on the same side.
		if (sideLeft1 <= 0.0f && sideRight1 <= 0.0f)
		{
			return !(cameraSide1 <= 0.0f);
		}
		else if (sideLeft1 >= 0.0f && sideRight1 >= 0.0f)
		{
			return !(cameraSide1 >= 0.0f);
		}

		// If we still get here, just do a quick distance check.
		f32 distSq0 = camera0.x*camera0.x + camera0.z*camera0.z;
		f32 distSq1 = camera1.x*camera1.x + camera1.z*camera1.z;
		return distSq1 < distSq0;
	}

	SegmentClipped* sbuffer_getClippedSeg(Segment* seg)
	{
		if (s_segClippedPoolCount >= SEG_CLIP_POOL_SIZE)
		{
			assert(0);
			TFE_System::logWrite(LOG_ERROR, "SegBuffer", "Too many clipped segs allocated - max is %d", SEG_CLIP_POOL_SIZE);
			return nullptr;
		}
		SegmentClipped* segClipped = &s_segClippedPool[s_segClippedPoolCount];
		segClipped->prev = nullptr;
		segClipped->next = nullptr;
		segClipped->seg = seg;
		if (seg)
		{
			segClipped->x0 = seg->x0;
			segClipped->x1 = seg->x1;
			segClipped->v0 = seg->v0;
			segClipped->v1 = seg->v1;
		}
		s_segClippedPoolCount++;
		return segClipped;
	}

	void insertSegmentBefore(SegmentClipped* cur, SegmentClipped* seg)
	{
		SegmentClipped* curPrev = cur->prev;
		if (curPrev)
		{
			curPrev->next = seg;
		}
		else
		{
			s_segClippedHead = seg;
		}
		seg->prev = curPrev;
		seg->next = cur;
		cur->prev = seg;
	}

	void insertSegmentAfter(SegmentClipped* cur, SegmentClipped* seg)
	{
		SegmentClipped* curNext = cur->next;
		if (curNext)
		{
			curNext->prev = seg;
		}
		else
		{
			s_segClippedTail = seg;
		}
		seg->next = curNext;
		seg->prev = cur;
		cur->next = seg;
	}

	void deleteSegment(SegmentClipped* cur)
	{
		SegmentClipped* curPrev = cur->prev;
		SegmentClipped* curNext = cur->next;
		if (curPrev)
		{
			curPrev->next = curNext;
		}
		else
		{
			s_segClippedHead = curNext;
		}

		if (curNext)
		{
			curNext->prev = curPrev;
		}
		else
		{
			s_segClippedTail = curPrev;
		}
	}

	SegmentClipped* mergeSegments(SegmentClipped* a, SegmentClipped* b)
	{
		a->next = b->next;
		a->x1 = b->x1;
		a->v1 = b->v1;

		if (a->next)
		{
			a->next->prev = a;
		}
		else
		{
			s_segClippedTail = a;
		}
		return a;
	}

	void swapSegments(SegmentClipped* a, SegmentClipped* b)
	{
		SegmentClipped* prev = a->prev;
		SegmentClipped* next = b->next;
		b->prev = prev;
		if (prev)
		{
			prev->next = b;
		}
		else
		{
			s_segClippedHead = b;
		}
		b->next = a;
		a->prev = b;
		a->next = next;
		if (next)
		{
			next->prev = a;
		}
		else
		{
			s_segClippedTail = a;
		}
	}

	bool segmentsOverlap(f32 ax0, f32 ax1, f32 bx0, f32 bx1)
	{
		return (ax1 > bx0 && ax0 < bx1) || (bx1 > ax0 && bx0 < ax1);
	}
}
