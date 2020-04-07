#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Geometry
// Functions to handle geometry, such as point in polygon tests.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>

namespace Geometry
{
	// Tests if a point (p2) is to the left, on or right of an infinite line (p0 -> p1).
	// Return: >0 p2 is on the left of the line.
	//         =0 p2 is on the line.
	//         <0 p2 is on the right of the line.
	f32 isLeft(Vec2f p0, Vec2f p1, Vec2f p2);

	// Returns true if EITHER p1 or p2 is in front of the plane formed by p0 and the direction formed by its line segment.
	bool edgeInFront(Vec2f p0, Vec2f dp, Vec2f p1, Vec2f p2);

	// Directly uses the SectorWall structure
	bool pointInSector(const Vec2f* point, u32 vertexCount, const Vec2f* vertices, u32 wallCount, const SectorWall* walls);
	// Handles any wall structure as long as the u16 vertex indices start at offset 0.
	bool pointInSector(const Vec2f* point, u32 vertexCount, const Vec2f* vertices, u32 wallCount, const u8* wallData, u32 stride);

	// Returns true if the segment (a0, a1) intersects the line segment (b0, b1)
	// intersection is: I = a0 + s*(a1-a0) = b0 + t*(b1 - b0)
	// Returns false if the intersection occurs between the lines but not the segments.
	bool lineSegmentIntersect(const Vec2f* a0, const Vec2f* a1, const Vec2f* b0, const Vec2f* b1, f32* s, f32* t);

	// Returns true if the segment (a0, a1) intersects the line (b0, b1)
	// intersection is: I = a0 + s*(a1-a0) = b0 + t*(b1 - b0)
	// Returns false if the intersection occurs between the lines but not the segments.
	bool lineSegment_LineIntersect(const Vec2f* a0, const Vec2f* a1, const Vec2f* b0, const Vec2f* b1, f32* s);

	// Find the closest point to p2 on line segment p0 -> p1 as a parametric value on the segment.
	// Fills in point with the point itself.
	f32 closestPointOnLineSegment(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f* point);

	// Find the closest point to p2 on line that passes through p0 -> p1 as a parametric value on the segment.
	// Fills in point with the point itself.
	f32 closestPointOnLine(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f* point);

	bool lineYPlaneIntersect(const Vec3f* p0, const Vec3f* p1, f32 planeHeight, Vec3f* hitPoint);
}
