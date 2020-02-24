#include "polygon.h"
#include "clipper.hpp"
#include "math.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#define MPE_POLY2TRI_IMPLEMENTATION
#include "MPE_fastpoly2tri.h"

namespace DXL2_Polygon
{
	// Container for resulting convex polygons.
	static Triangle s_outPolys[MAX_CONVEX_POLYGONS];

	// Stack and temporary polygon pool.
	static Polygon s_polyPool[MAX_CONVEX_POLYGONS];

	static void* s_memoryPool = nullptr;
	static const u32 c_maxPointCount = 1024u;
	static u32 s_memoryPoolSize;

	static ClipperLib::Clipper s_clipper;
		
	bool init()
	{
		s_memoryPoolSize = (u32)MPE_PolyMemoryRequired(c_maxPointCount);
		s_memoryPool = malloc(s_memoryPoolSize);

		return s_memoryPool && s_memoryPoolSize;
	}

	void shutdown()
	{
		free(s_memoryPool);
		s_memoryPool = nullptr;
		s_memoryPoolSize = 0;
	}

	void copyPolygon(Polygon& dst, const Polygon& src)
	{
		dst.vtxCount = src.vtxCount;
		for (s32 i = 0; i < dst.vtxCount; i++) { dst.vtx[i] = src.vtx[i]; }
	}

	Triangle* decomposeComplexPolygon(u32 contourCount, const Polygon* contours, u32* outConvexPolyCount)
	{
		Polygon outerPoly[16];
		// If there is more than one contour then an outer contour must be found and inner contours
		// added while splitting the outer.
		s32 innerCount = 0;
		s32 outerCount = 1;
		Polygon* innerPoly = s_polyPool;
		if (contourCount > 1)
		{
			// First compute the AABB of the polygon.
			Vec2f aabb[2] = { {FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX} };
			for (u32 c = 0; c < contourCount; c++)
			{
				const s32 vtxCount = contours[c].vtxCount;
				const Vec2f* vtx = contours[c].vtx;
				for (s32 v = 0; v < vtxCount; v++)
				{
					aabb[0].x = std::min(aabb[0].x, vtx[v].x);
					aabb[0].z = std::min(aabb[0].z, vtx[v].z);

					aabb[1].x = std::max(aabb[1].x, vtx[v].x);
					aabb[1].z = std::max(aabb[1].z, vtx[v].z);
				}
			}

			// Next find the outer contour.
			// Find the contour with a vertex where x == max x
			s32 outer = 0;
			for (u32 c = 0; c < contourCount; c++)
			{
				const Polygon* curCon = &contours[c];
				const s32 vtxCount = curCon->vtxCount;
				const Vec2f* vtx = curCon->vtx;
				for (s32 v = 0; v < vtxCount; v++)
				{
					if (vtx[v].x == aabb[1].x)
					{
						outer = c;
						break;
					}
				}
			}

			// Convert the outer contour to a polygon.
			copyPolygon(outerPoly[0], contours[outer]);
						
			// If there is more than one hole, merge them together.
			// This fixes issues where holes share edges.
			if (contourCount > 2)
			{
				s_clipper.Clear();
				const ClipperLib::ClipType     ct = ClipperLib::ctUnion;
				const ClipperLib::PolyFillType pft = ClipperLib::pftEvenOdd;
				ClipperLib::Path hole;
				hole.reserve(1024);
				for (u32 c = 0; c < contourCount; c++)
				{
					if (c == outer) { continue; }
					hole.resize(contours[c].vtxCount);
					for (s32 v = 0; v < contours[c].vtxCount; v++)
					{
						const f32 sx = contours[c].vtx[v].x < 0.0f ? -1.0f : 1.0f;
						const f32 sz = contours[c].vtx[v].z < 0.0f ? -1.0f : 1.0f;

						hole[v].X = s32(contours[c].vtx[v].x * 100.0f + 0.5f*sx);
						hole[v].Y = s32(contours[c].vtx[v].z * 100.0f + 0.5f*sz);
					}
					s_clipper.AddPath(hole, ClipperLib::ptSubject, true);
				}
				ClipperLib::Paths solution;
				s_clipper.Execute(ct, solution, pft, pft);

				innerCount = (u32)solution.size();
				for (s32 c = 0; c < innerCount; c++)
				{
					ClipperLib::Path& path = solution[c];
					innerPoly[c].vtxCount = (u32)path.size();
					for (s32 v = 0; v < innerPoly[c].vtxCount; v++)
					{
						innerPoly[c].vtx[v].x = f32(path[v].X) * 0.01f;
						innerPoly[c].vtx[v].z = f32(path[v].Y) * 0.01f;
					}
				}
			}
			else
			{
				// Then convert and count all of the inner contours
				for (u32 c = 0; c < contourCount; c++)
				{
					if (c == outer) { continue; }
					copyPolygon(innerPoly[innerCount], contours[c]);
					innerCount++;
				}
			}
		}
		else
		{
			copyPolygon(outerPoly[0], contours[0]);

			// The outer polygon may self-intersect (such as in Jabba's ship, sector 348). So we have to clean it up just in case.
			// However this should only be done if it has more than 4 edges so that simple sectors are fast to triangulate.
			// Note this may miss cases where 4 vertices intersect - though I suspect that doesn't happen in existing data and can just be flagged as an error in new data.
			if (outerPoly[0].vtxCount > 4)
			{
				s_clipper.Clear();
				ClipperLib::Path outer(outerPoly[0].vtxCount);

				for (s32 v = 0; v < outerPoly[0].vtxCount; v++)
				{
					const f32 sx = outerPoly[0].vtx[v].x < 0.0f ? -1.0f : 1.0f;
					const f32 sz = outerPoly[0].vtx[v].z < 0.0f ? -1.0f : 1.0f;

					outer[v].X = s32(outerPoly[0].vtx[v].x * 100.0f + 0.5f*sx);
					outer[v].Y = s32(outerPoly[0].vtx[v].z * 100.0f + 0.5f*sz);
				}

				ClipperLib::Paths solution;
				s_clipper.StrictlySimple(true);
				s_clipper.AddPath(outer, ClipperLib::ptSubject, true);
				s_clipper.Execute(ClipperLib::ctUnion, solution, ClipperLib::pftEvenOdd, ClipperLib::pftEvenOdd);

				outerCount = (s32)solution.size();
				for (s32 i = 0; i < outerCount; i++)
				{
					ClipperLib::Path& path = solution[i];
					outerPoly[i].vtxCount = (u32)path.size();
					for (s32 v = 0; v < outerPoly[i].vtxCount; v++)
					{
						outerPoly[i].vtx[v].x = f32(path[v].X) * 0.01f;
						outerPoly[i].vtx[v].z = f32(path[v].Y) * 0.01f;
					}
				}
			}
		}
				
		u32 triOffset = 0;
		*outConvexPolyCount = 0;
		// Having more than one outer polygon is expensive but fortunately happens very rarely (only one case that I know of in vanilla data).
		for (s32 i = 0; i < outerCount; i++)
		{
			// Can we avoid clearing memory every time?
			memset(s_memoryPool, 0, s_memoryPoolSize);
		
			// Initialize the poly context by passing the memory pointer,
			// and max number of points from before
			MPEPolyContext PolyContext = { 0 };
			if (MPE_PolyInitContext(&PolyContext, s_memoryPool, c_maxPointCount))
			{
				// Add the outer edge.
				for (s32 v = 0; v < outerPoly[i].vtxCount; v++)
				{
					MPEPolyPoint* pt = MPE_PolyPushPoint(&PolyContext);
					pt->X = outerPoly[i].vtx[v].x;
					pt->Y = outerPoly[i].vtx[v].z;
				}
				MPE_PolyAddEdge(&PolyContext);

				// Add holes.
				for (s32 c = 0; c < innerCount; c++)
				{
					for (s32 v = 0; v < innerPoly[c].vtxCount; v++)
					{
						MPEPolyPoint* pt = MPE_PolyPushPoint(&PolyContext);
						pt->X = innerPoly[c].vtx[v].x;
						pt->Y = innerPoly[c].vtx[v].z;
					}
					MPE_PolyAddHole(&PolyContext);
				}

				// Triangulate
				MPE_PolyTriangulate(&PolyContext);

				// Get the triangles.
				for (uxx TriangleIndex = 0; TriangleIndex < PolyContext.TriangleCount; ++TriangleIndex)
				{
					MPEPolyTriangle* Triangle = PolyContext.Triangles[TriangleIndex];
					MPEPolyPoint* PointA = Triangle->Points[0];
					MPEPolyPoint* PointB = Triangle->Points[1];
					MPEPolyPoint* PointC = Triangle->Points[2];

					s_outPolys[TriangleIndex+triOffset].vtx[0] = { PointA->X, PointA->Y };
					s_outPolys[TriangleIndex+triOffset].vtx[1] = { PointB->X, PointB->Y };
					s_outPolys[TriangleIndex+triOffset].vtx[2] = { PointC->X, PointC->Y };
				}
				*outConvexPolyCount += PolyContext.TriangleCount;
				triOffset += PolyContext.TriangleCount;
			}
		}
		return s_outPolys;
	}
}
