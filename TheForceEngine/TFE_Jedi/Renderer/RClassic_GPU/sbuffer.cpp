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

using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	extern Vec3f s_cameraPos;
	const f32 c_sideEps = 0.0001f;

	SegmentClipped s_segClippedPool[1024];
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
		f32 axisScale = coord.m[axis & 1] != 0.0f ? 1.0f / fabsf(coord.m[axis & 1]) : 0.0f;
		f32 value = coord.m[(1 + axis) & 1] * axisScale * 0.5f + 0.5f;
		if (axis == 1 || axis == 2) { value = 1.0f - value; }

		// Switch the direction to match the wall direction.
		return 4.0f - fmodf(f32(axis) + value, 4.0f);	// this wraps around to 0.0 at 4.0
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
			else if (seg->x1 > range[r].z && seg->x0 < range[r].z)
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
		if (s_segClippedPoolCount >= 1024)
		{
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
