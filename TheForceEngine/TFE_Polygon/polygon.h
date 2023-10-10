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
	std::vector<s32> triIdx;
};

enum PolyDebug
{
	PDBG_NONE = 0,
	PDBG_SHOW_SUPERTRI = FLAG_BIT(0),
	PDBG_SKIP_INSIDE_TEST = FLAG_BIT(1),
};

namespace TFE_Polygon
{
	bool computeTriangulation(Polygon* poly, u32 debug=PDBG_NONE);
	bool pointInsidePolygon(Polygon* poly, Vec2f p);
}
