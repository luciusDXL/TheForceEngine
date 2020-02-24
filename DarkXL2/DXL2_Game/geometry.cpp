#include "geometry.h"
#include "player.h"
#include <algorithm>

namespace Geometry
{
	// Tests if a point (p2) is to the left, on or right of an infinite line (p0 -> p1).
	// Return: >0 p2 is on the left of the line.
	//         =0 p2 is on the line.
	//         <0 p2 is on the right of the line.
	f32 isLeft(Vec2f p0, Vec2f p1, Vec2f p2)
	{
		return (p1.x - p0.x) * (p2.z - p0.z) - (p2.x - p0.x) * (p1.z - p0.z);
	}

	// Returns true if p2 is in front of the infinite line scribed by p0 in the direction of dp (p1 - p0)
	bool edgeInFront(Vec2f p0, Vec2f dp, Vec2f p1, Vec2f p2)
	{
		const Vec2f u0 = { p1.x - p0.x, p1.z - p0.z };
		const Vec2f u1 = { p2.x - p0.x, p2.z - p0.z };
		return -u0.x*dp.z + u0.z*dp.x > FLT_EPSILON || -u1.x*dp.z + u1.z*dp.x > FLT_EPSILON;
	}

	// Uses the "Winding Number" test for a point in polygon.
	// The point is considered inside if the winding number is greater than 0.
	bool pointInSector(const Vec2f* point, u32 vertexCount, const Vec2f* vertices, u32 wallCount, const u8* wallData, u32 stride)
	{
		s32 wn = 0;

		for (s32 i = 0; i < (s32)wallCount; i++, wallData +=stride)
		{
			const u16* indices = (u16*)wallData;

			// Test the edge (i+1, i) - it is flipped due to the winding order that
			// Dark Forces uses.
			const s32 i0 = indices[1], i1 = indices[0];

			if (vertices[i0].z <= point->z)
			{
				// Upward crossing, if the point is left of the edge than it intersects.
				if (vertices[i1].z > point->z && isLeft(vertices[i0], vertices[i1], *point) > 0)
				{
					wn++;
				}
			}
			else
			{
				// Downward crossing, if point is right of the edge it intersects.
				if (vertices[i1].z <= point->z && isLeft(vertices[i0], vertices[i1], *point) < 0)
				{
					wn--;
				}
			}
		}

		// The point is only outside if the winding number is less than or equal to 0.
		return wn > 0;
	}

	bool pointInSector(const Vec2f* point, u32 vertexCount, const Vec2f* vertices, u32 wallCount, const SectorWall* walls)
	{
		return pointInSector(point, vertexCount, vertices, wallCount, (u8*)walls, sizeof(SectorWall));
	}

	// Returns true if the segment (a0, a1) intersects the line segment (b0, b1)
	// intersection is: I = a0 + s*(a1-a0) = b0 + t*(b1 - b0)
	// Returns false if the intersection occurs between the lines but not the segments.
	bool lineSegmentIntersect(const Vec2f* a0, const Vec2f* a1, const Vec2f* b0, const Vec2f* b1, f32* s, f32* t)
	{
		const Vec2f u = { a1->x - a0->x, a1->z - a0->z };
		const Vec2f v = { b1->x - b0->x, b1->z - b0->z };
		const Vec2f w = { a0->x - b0->x, a0->z - b0->z };

		f32 det = v.x*u.z - v.z*u.x;
		if (fabsf(det) < FLT_EPSILON) { return false; }
		det = 1.0f / det;

		*s =  (v.z*w.x - v.x*w.z) * det;
		*t = -(u.x*w.z - u.z*w.x) * det;

		return (*s) > -FLT_EPSILON && (*s) < 1.0f + FLT_EPSILON && (*t) > -FLT_EPSILON && (*t) < 1.0f + FLT_EPSILON;
	}

	// Returns true if the segment (a0, a1) intersects the line (b0, b1)
	// intersection is: I = a0 + s*(a1-a0) = b0 + t*(b1 - b0)
	// Returns false if the intersection occurs between the lines but not the segments.
	bool lineSegment_LineIntersect(const Vec2f* a0, const Vec2f* a1, const Vec2f* b0, const Vec2f* b1, f32* s)
	{
		const Vec2f u = { a1->x - a0->x, a1->z - a0->z };
		const Vec2f v = { b1->x - b0->x, b1->z - b0->z };
		const Vec2f w = { a0->x - b0->x, a0->z - b0->z };

		f32 det = v.x*u.z - v.z*u.x;
		if (fabsf(det) < FLT_EPSILON) { return false; }
		det = 1.0f / det;

		*s = (v.z*w.x - v.x*w.z) * det;
		return (*s) > -FLT_EPSILON && (*s) < 1.0f + FLT_EPSILON;
	}

	// Find the closest point to p2 on line segment p0 -> p1 as a parametric value on the segment.
	// Fills in point with the point itself.
	f32 closestPointOnLineSegment(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f* point)
	{
		const Vec2f r = { p2.x - p0.x, p2.z - p0.z };
		const Vec2f d = { p1.x - p0.x, p1.z - p0.z };
		const f32 denom = d.x * d.x + d.z * d.z;
		if (fabsf(denom) < FLT_EPSILON) { return 0.0f; }

		const f32 s = std::max(0.0f, std::min(1.0f, (r.x * d.x + r.z * d.z) / denom));
		point->x = p0.x + s * d.x;
		point->z = p0.z + s * d.z;
		return s;
	}

	// Find the closest point to p2 on line that passes through p0 -> p1 as a parametric value on the segment.
	// Fills in point with the point itself.
	f32 closestPointOnLine(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f* point)
	{
		const Vec2f r = { p2.x - p0.x, p2.z - p0.z };
		const Vec2f d = { p1.x - p0.x, p1.z - p0.z };
		const f32 denom = d.x * d.x + d.z * d.z;
		if (fabsf(denom) < FLT_EPSILON) { return 0.0f; }

		const f32 s = (r.x * d.x + r.z * d.z) / denom;
		point->x = p0.x + s * d.x;
		point->z = p0.z + s * d.z;
		return s;
	}

	// line: p0, p1; plane: planeHeight + planeDir (+/-Y)
	// returns true if the intersection occurs within the segment
	bool lineYPlaneIntersect(const Vec3f* p0, const Vec3f* p1, f32 planeHeight, Vec3f* hitPoint)
	{
		if (fabsf(p1->y - p0->y) < FLT_EPSILON) { return false; }

		const f32 s = (planeHeight - p0->y) / (p1->y - p0->y);
		if (s < -FLT_EPSILON || s > 1.0f + FLT_EPSILON) { return false; }

		*hitPoint = { p0->x + s*(p1->x - p0->x), p0->y + s*(p1->y - p0->y), p0->z + s*(p1->z - p0->z) };
		return true;
	}
}
