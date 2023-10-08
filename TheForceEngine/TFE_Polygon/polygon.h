#pragma once
#include <TFE_System/types.h>
#include <vector>

// Handling complex polygons.
struct Edge
{
	s32 i0, i1;
};

// Complex polygon and cached triangle list.
struct Polygon
{
	std::vector<Vec2f> vtx;
	std::vector<Edge> edge;
	Vec2f bounds[2];

	// Cached triangles - every 3 indices = 1 triangle.
	std::vector<Vec2f> triVtx;
	std::vector<s32> indices;
};

namespace TFE_Polygon
{
	bool computeTriangulation(Polygon* poly);
}
