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

enum BoolMode
{
	BMODE_SET = 0,
	BMODE_MERGE,
	BMODE_SUBTRACT,
	BMODE_COUNT
};

struct BEdge
{
	Vec2f v0 = { 0 };
	Vec2f v1 = { 0 };
	f32 u0 = 0.0f;
	f32 u1 = 0.0f;
	s32 nextEdge = -1;
	s32 prevEdge = -1;
	s32 nextMirrorEdge = -1;
	s32 prevMirrorEdge = -1;
	s32 srcWallIndex = -1;
};

struct BPolygon
{
	std::vector<BEdge> edges;
	bool outsideClipRegion = false;
};

namespace TFE_Polygon
{
	bool computeTriangulation(Polygon* poly, u32 debug=PDBG_NONE);
	bool pointInsidePolygon(const Polygon* poly, Vec2f p);

	void clipInit();
	void clipDestroy();
	void clipPolygons(const BPolygon* subject, const BPolygon* clip, std::vector<BPolygon>& outPoly, BoolMode boolMode);
	bool addEdgeIntersectionsToPoly(BPolygon* subject, const BPolygon* clip);
	void cleanUpShape(std::vector<Vec2f>& shape);
	bool addEdgeToBPoly(Vec2f v0, Vec2f v1, f32 uOffset, s32 wallIndex, BPolygon* poly);

	bool lineSegmentsIntersect(Vec2f a0, Vec2f a1, Vec2f b0, Vec2f b1, Vec2f* vI = nullptr, f32* u = nullptr, f32* v = nullptr);
	f32 closestPointOnLineSegment(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f* point);

	bool vtxEqual(const Vec2f* a, const Vec2f* b);
	bool vtxEqual(const Vec3f* a, const Vec3f* b);

	inline f32 signedArea(s32 count, const Vec2f* vtx)
	{
		f32 sum = 0.0f;
		for (s32 v = 0; v < count; v++)
		{
			s32 a = v, b = (v + 1) % count;
			sum += (vtx[b].x - vtx[a].x) * (vtx[b].z + vtx[a].z);
		}
		return sum * 0.5f;
	}
}
