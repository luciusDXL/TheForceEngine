#include "polygon.h"
#include "math.h"
#include <TFE_System/system.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

namespace TFE_Polygon
{
	struct Triangle
	{
		Vec2f v0, v1, v2;
	};

	static std::vector<Triangle> s_triangles;

	// Compute a valid triangulation for the polygon.
	// Polygons may be complex, self-intersecting, and even be incomplete.
	// The goal is a *robust* triangulation system that will always produce
	// a plausible result even with malformed data.
	bool computeTriangulation(Polygon* poly)
	{
		s_triangles.clear();
		const size_t edgeCount = poly->edge.size();
		if (edgeCount < 3)
		{
			// ERROR!
			return false;
		}

		// 0. If the polygon has 3 or 4 edges, assume it is already convex.
		// Many of the sectors in a level are actually 4-sided doorways, so optimize for the common case.
		if (edgeCount <= 4)
		{
			poly->triVtx.resize(edgeCount);
			poly->indices.resize((edgeCount - 2) * 3);

			for (s32 i = 0; i < edgeCount; i++)
			{
				poly->triVtx[i] = poly->vtx[poly->edge[i].i0];
			}
			if (edgeCount == 4)
			{
				poly->indices[0] = 0;
				poly->indices[1] = 1;
				poly->indices[2] = 2;

				poly->indices[3] = 0;
				poly->indices[4] = 2;
				poly->indices[5] = 3;
			}
			else
			{
				poly->indices[0] = 0;
				poly->indices[1] = 1;
				poly->indices[2] = 2;
			}
			return true;
		}

		// 1. Given the vertices that make up the polygon, perform a Delaunary triangulation.
		// 1.a Create a "super triangle" to hold all of the points.
		Vec2f centroid = { (poly->bounds[0].x + poly->bounds[1].x) * 0.5f, (poly->bounds[0].z + poly->bounds[1].z) * 0.5f };


		// 2. Apply each edge, splitting the triangles as needed.

		// 3. Given the final resulting triangles, determine which are *inside* of the complex complex, discard the rest.

		return false;
	}
}
