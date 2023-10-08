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
	struct Edge
	{
		s32 idx[2];
		s32 refCount;
	};

	struct Triangle
	{
		bool allocated;
		s32 id;
		s32 idx[3];
		s32 adj[3];
		Vec2f circle;
		f32 radiusSq;
	};

	static std::vector<Vec2f> s_vertices;
	static std::vector<Triangle> s_triangles;
	static std::vector<s32> s_freeList;
	static std::vector<Edge> s_edges;

	s32 computeAdjacency(s32 i0, s32 i1, s32 id)
	{
		s32 adj = -1;

		const size_t count = s_triangles.size();
		Triangle* tri = s_triangles.data();
		for (size_t t = 0; t < count; t++, tri++)
		{
			if (t == id || !tri->allocated) { continue; }
			if ((tri->idx[0] == i1 && tri->idx[1] == i0) || (tri->idx[0] == i0 && tri->idx[1] == i1))
			{
				//assert(tri->adj[0] < 0 || tri->adj[0] == id);
				adj = t;
				tri->adj[0] = id;
				break;
			}
			else if ((tri->idx[1] == i1 && tri->idx[2] == i0) || (tri->idx[1] == i0 && tri->idx[2] == i1))
			{
				//assert(tri->adj[1] < 0 || tri->adj[1] == id);
				adj = t;
				tri->adj[1] = id;
				break;
			}
			else if ((tri->idx[2] == i1 && tri->idx[0] == i0) || (tri->idx[2] == i0 && tri->idx[0] == i1))
			{
				//assert(tri->adj[2] < 0 || tri->adj[2] == id);
				adj = t;
				tri->adj[2] = id;
				break;
			}
		}
		return adj;
	}

	void addTriangle(s32 i0, s32 i1, s32 i2)
	{
		Triangle* tri = nullptr;
		s32 id = -1;
		if (!s_freeList.empty())
		{
			id = s_freeList.back();
			tri = &s_triangles[id];
			assert(tri->id == id);
			s_freeList.pop_back();
		}
		else
		{
			id = (s32)s_triangles.size();
			s_triangles.push_back({});
			tri = &s_triangles[id];
			tri->id = id;
		}

		tri->allocated = true;

		tri->idx[0] = i0;
		tri->idx[1] = i1;
		tri->idx[2] = i2;

		tri->adj[0] = computeAdjacency(i0, i1, id);
		tri->adj[1] = computeAdjacency(i1, i2, id);
		tri->adj[2] = computeAdjacency(i2, i0, id);

		Vec2f v0 = s_vertices[i0];
		Vec2f v1 = s_vertices[i1];
		Vec2f v2 = s_vertices[i2];

		Vec2f a = { v1.x - v0.x, v1.z - v0.z };
		Vec2f b = { v2.x - v0.x, v2.z - v0.z };
		f32 m = v1.x*v1.x - v0.x*v0.x + v1.z*v1.z - v0.z*v0.z;
		f32 u = v2.x*v2.x - v0.x*v0.x + v2.z*v2.z - v0.z*v0.z;
		f32 s = 1.0f / (2.0f * (a.x*b.z - a.z*b.x));

		tri->circle.x = ((v2.z - v0.z)*m + (v0.z - v1.z)*u) * s;
		tri->circle.z = ((v0.x - v2.x)*m + (v1.x - v0.x)*u) * s;

		Vec2f offset = { v0.x - tri->circle.x, v0.z - tri->circle.z };
		tri->radiusSq = offset.x*offset.x + offset.z*offset.z;

		// Deal with small errors.
		// TODO: Use an epsilon instead?
		const Vec2f offset1 = { v1.x - tri->circle.x, v1.z - tri->circle.z };
		const Vec2f offset2 = { v2.x - tri->circle.x, v2.z - tri->circle.z };
		f32 r1 = offset1.x*offset1.x + offset1.z*offset1.z;
		f32 r2 = offset2.x*offset2.x + offset2.z*offset2.z;
		tri->radiusSq = std::max(tri->radiusSq, r1);
		tri->radiusSq = std::max(tri->radiusSq, r2);
	}

	void addTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2)
	{
		Triangle* tri = nullptr;
		s32 id = -1;
		if (!s_freeList.empty())
		{
			id = s_freeList.back();
			tri = &s_triangles[id];
			s_freeList.pop_back();
		}
		else
		{
			id = (s32)s_triangles.size();
			s_triangles.push_back({});
			tri = &s_triangles[id];
			tri->id = id;
		}

		tri->allocated = true;

		tri->idx[0] = 0;
		tri->idx[1] = 1;
		tri->idx[2] = 2;

		tri->adj[0] = -1;
		tri->adj[1] = -1;
		tri->adj[2] = -1;

		s_vertices.push_back(v0);
		s_vertices.push_back(v1);
		s_vertices.push_back(v2);

		Vec2f a = { v1.x - v0.x, v1.z - v0.z };
		Vec2f b = { v2.x - v0.x, v2.z - v0.z };
		f32 m = v1.x*v1.x - v0.x*v0.x + v1.z*v1.z - v0.z*v0.z;
		f32 u = v2.x*v2.x - v0.x*v0.x + v2.z*v2.z - v0.z*v0.z;
		f32 s = 1.0f / (2.0f * (a.x*b.z - a.z*b.x));

		tri->circle.x = ((v2.z - v0.z)*m + (v0.z - v1.z)*u) * s;
		tri->circle.z = ((v0.x - v2.x)*m + (v1.x - v0.x)*u) * s;

		Vec2f offset = { v0.x - tri->circle.x, v0.z - tri->circle.z };
		tri->radiusSq = offset.x*offset.x + offset.z*offset.z;
	}

	void createSuperTriangle(Polygon* poly)
	{
		Vec2f centroid = { (poly->bounds[0].x + poly->bounds[1].x) * 0.5f, (poly->bounds[0].z + poly->bounds[1].z) * 0.5f };
		Vec2f ext = { poly->bounds[1].x - poly->bounds[0].x, poly->bounds[1].z - poly->bounds[0].z };
		f32 maxExt = std::max(ext.x, ext.z);

		Vec2f v0 = { centroid.x - maxExt * 5.0f, centroid.z - maxExt };
		Vec2f v1 = { centroid.x, centroid.z + maxExt * 5.0f };
		Vec2f v2 = { centroid.x + maxExt * 5.0f, centroid.z - maxExt };
		addTriangle(v0, v1, v2);
	}

	void addEdge(s32 i0, s32 i1)
	{
		const size_t count = s_edges.size();
		Edge* edge = s_edges.data();
		for (size_t e = 0; e < count; e++, edge++)
		{
			if ((edge->idx[0] == i0 && edge->idx[1] == i1) || (edge->idx[0] == i1 && edge->idx[1] == i0))
			{
				edge->refCount++;
				return;
			}
		}
		s_edges.push_back({ i0, i1, 0 });
	}

	void addPoint(const Vec2f* vtx)
	{
		s32 vtxIndex = (s32)s_vertices.size();
		s_vertices.push_back(*vtx);

		const size_t count = s_triangles.size();
		Triangle* tri = s_triangles.data();
		s_edges.clear();

		f32 smallestDiff = FLT_MAX;
		const f32 eps = 1e-4;
		for (size_t t = 0; t < count; t++, tri++)
		{
			if (!tri->allocated) { continue; }

			// Is the point inside the circumcircle?
			const Vec2f offset = { vtx->x - tri->circle.x, vtx->z - tri->circle.z };
			const f32 distSq = offset.x*offset.x + offset.z*offset.z;

			const f32 diff = fabsf(distSq - tri->radiusSq);
			smallestDiff = std::min(smallestDiff, diff);

			if (distSq <= tri->radiusSq + eps)
			{
				// Inside, grab the outer edge.
				addEdge(tri->idx[0], tri->idx[1]);
				addEdge(tri->idx[1], tri->idx[2]);
				addEdge(tri->idx[2], tri->idx[0]);
				
				tri->allocated = false;
				tri->adj[0] = -1;
				tri->adj[1] = -1;
				tri->adj[2] = -1;
				s_freeList.push_back(tri->id);
			}
		}
		assert(!s_edges.empty());

		// Fix up triangle adjacency.
		tri = s_triangles.data();
		for (size_t t = 0; t < count; t++, tri++)
		{
			if (!tri->allocated) { continue; }

			if (tri->adj[0] >= 0 && !s_triangles[tri->adj[0]].allocated)
			{
				tri->adj[0] = -1;
			}
			if (tri->adj[1] >= 0 && !s_triangles[tri->adj[1]].allocated)
			{
				tri->adj[1] = -1;
			}
			if (tri->adj[2] >= 0 && !s_triangles[tri->adj[2]].allocated)
			{
				tri->adj[2] = -1;
			}
		}

		// Form a new triangle between the vertex and any edge with adjacency.
		const size_t edgeCount = s_edges.size();
		const Edge* edge = s_edges.data();
		for (size_t e = 0; e < edgeCount; e++, edge++)
		{
			if (edge->refCount) { continue; }

			// Form a triangle from vtxIndex -> edge indices.
			addTriangle(vtxIndex, edge->idx[0], edge->idx[1]);
		}
	}

	// Compute a valid triangulation for the polygon.
	// Polygons may be complex, self-intersecting, and even be incomplete.
	// The goal is a *robust* triangulation system that will always produce
	// a plausible result even with malformed data.
	bool computeTriangulation(Polygon* poly)
	{
		s_freeList.clear();
		s_triangles.clear();
		s_vertices.clear();

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
		createSuperTriangle(poly);

		// 1.b Add each point iteratively.
		const size_t vtxCount = poly->vtx.size();
		const Vec2f* vtx = poly->vtx.data();
		for (size_t v = 0; v < vtxCount; v++, vtx++)
		{
			addPoint(vtx);
		}

		// 2. Remove triangles that contain a super triangle vertex.
		const size_t triCount = s_triangles.size();
		Triangle* tri = s_triangles.data();
		for (size_t t = 0; t < triCount; t++, tri++)
		{
			if (!tri->allocated) { continue; }
			if (tri->idx[0] < 3 || tri->idx[1] < 3 || tri->idx[2] < 3)
			{
				tri->allocated = false;
				continue;
			}

			// For now just add it here.
			poly->indices.push_back(tri->idx[0]);
			poly->indices.push_back(tri->idx[1]);
			poly->indices.push_back(tri->idx[2]);
		}
		poly->triVtx = s_vertices;

		// 3. Given the final resulting triangles, determine which are *inside* of the complex complex, discard the rest.

		return true;
	}
}
