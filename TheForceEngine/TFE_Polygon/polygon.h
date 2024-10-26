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
	s32 nextEdge = -1;
	s32 prevEdge = -1;
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
	// Return edge index or -1 if point not on an edge.
	s32  pointOnPolygonEdge(const Polygon* poly, Vec2f p);

	void clipInit();
	void clipDestroy();
	void clipPolygons(const BPolygon* subject, const BPolygon* clip, std::vector<BPolygon>& outPoly, BoolMode boolMode);
	void insertPointsIntoPolygons(const std::vector<Vec2f>& insertionPt, std::vector<BPolygon>* poly);
	bool addEdgeIntersectionsToPoly(BPolygon* subject, const BPolygon* clip);
	void cleanUpShape(std::vector<Vec2f>& shape);
	bool addEdgeToBPoly(Vec2f v0, Vec2f v1, BPolygon* poly);
	void buildInsertionPointList(const BPolygon* poly, std::vector<Vec2f>* insertionPt);

	bool lineSegmentsIntersect(Vec2f a0, Vec2f a1, Vec2f b0, Vec2f b1, Vec2f* vI = nullptr, f32* u = nullptr, f32* v = nullptr, f32 sEps = 0.0f);
	f32 closestPointOnLineSegment(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f* point);

	bool vtxEqual(const Vec2f* a, const Vec2f* b);
	bool vtxEqual(const Vec3f* a, const Vec3f* b);

	inline f32 signedArea(s32 count, const Vec2f* vtx)
	{
		f32 twiceArea = 0.0f;
		for (s32 v = 0; v < count; v++)
		{
			s32 a = v, b = (v + 1) % count;
			twiceArea += (vtx[b].x - vtx[a].x) * (vtx[b].z + vtx[a].z);
		}
		return twiceArea * 0.5f;
	}

	inline f32 signedArea(const Polygon& poly)
	{
		const s32 edgeCount = (s32)poly.edge.size();
		const Edge* edge = poly.edge.data();
		const Vec2f* vtx = poly.vtx.data();
		f32 twiceArea = 0.0f;
		for (s32 e = 0; e < edgeCount; e++, edge++)
		{
			const Vec2f v0 = vtx[edge->i0];
			const Vec2f v1 = vtx[edge->i1];
			twiceArea += (v1.x - v0.x) * (v1.z + v0.z);
		}
		return twiceArea * 0.5f;
	}
}
