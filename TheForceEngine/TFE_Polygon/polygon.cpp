#include "polygon.h"
#include "math.h"
#include "clipper.hpp"
#include <TFE_System/math.h>
#include <TFE_System/system.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#define USE_POLY_ASSERT 0

namespace TFE_Polygon
{
#if USE_POLY_ASSERT == 1
	#define PolyAssert(x) assert(x)
#else
	#define PolyAssert(x)
#endif

	struct TriEdge
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
		Vec2f centroid;
		Vec2f circle;
		f32 radiusSq;
	};

	const f32 eps = 1e-3f;

	static bool s_init = false;
	static std::vector<Vec2f> s_vertices;
	static std::vector<Triangle> s_triangles;
	static std::vector<s32> s_freeList;
	static std::vector<TriEdge> s_edges;
	static std::vector<Edge> s_constraints;
	static Vec2f s_coordCenter;
	   
	static ClipperLib::Clipper* s_clipper = nullptr;

	void deleteTriangle(Triangle* tri);

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
				adj = (s32)t;
				tri->adj[0] = id;
				break;
			}
			else if ((tri->idx[1] == i1 && tri->idx[2] == i0) || (tri->idx[1] == i0 && tri->idx[2] == i1))
			{
				adj = (s32)t;
				tri->adj[1] = id;
				break;
			}
			else if ((tri->idx[2] == i1 && tri->idx[0] == i0) || (tri->idx[2] == i0 && tri->idx[0] == i1))
			{
				adj = (s32)t;
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
			PolyAssert(tri->id == id);
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

		// Compute the centroid.
		tri->centroid.x = (v0.x + v1.x + v2.x) / 3.0f;
		tri->centroid.z = (v0.z + v1.z + v2.z) / 3.0f;

		// Compute the circumcircle.
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

		// Compute the centroid.
		tri->centroid.x = (v0.x + v1.x + v2.x) / 3.0f;
		tri->centroid.z = (v0.z + v1.z + v2.z) / 3.0f;

		// Compute the cirumcircle.
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
		s_coordCenter.x = floorf(centroid.x);
		s_coordCenter.z = floorf(centroid.z);
		centroid.x -= s_coordCenter.x;
		centroid.z -= s_coordCenter.z;

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
		TriEdge* edge = s_edges.data();
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

	void deleteTriangle(Triangle* tri)
	{
		tri->allocated = false;
		tri->adj[0] = -1;
		tri->adj[1] = -1;
		tri->adj[2] = -1;
		s_freeList.push_back(tri->id);
	}

	void addPoint(Vec2f vtx)
	{
		s32 vtxIndex = (s32)s_vertices.size();
		s_vertices.push_back(vtx);

		const size_t count = s_triangles.size();
		Triangle* tri = s_triangles.data();
		s_edges.clear();

		f32 smallestDiff = FLT_MAX;
		for (size_t t = 0; t < count; t++, tri++)
		{
			if (!tri->allocated) { continue; }

			// Is the point inside the circumcircle?
			const Vec2f offset = { vtx.x - tri->circle.x, vtx.z - tri->circle.z };
			const f32 distSq = offset.x*offset.x + offset.z*offset.z;

			const f32 diff = fabsf(distSq - tri->radiusSq);
			smallestDiff = std::min(smallestDiff, diff);

			if (distSq <= tri->radiusSq + eps)
			{
				// Add triangle edges for later processing.
				addEdge(tri->idx[0], tri->idx[1]);
				addEdge(tri->idx[1], tri->idx[2]);
				addEdge(tri->idx[2], tri->idx[0]);
				// Delete the parent triangle.
				deleteTriangle(tri);
			}
		}
		PolyAssert(!s_edges.empty());

		// Fix up triangle adjacency.
		tri = s_triangles.data();
		for (size_t t = 0; t < count; t++, tri++)
		{
			if (!tri->allocated) { continue; }
			if (tri->adj[0] >= 0 && !s_triangles[tri->adj[0]].allocated) { tri->adj[0] = -1; }
			if (tri->adj[1] >= 0 && !s_triangles[tri->adj[1]].allocated) { tri->adj[1] = -1; }
			if (tri->adj[2] >= 0 && !s_triangles[tri->adj[2]].allocated) { tri->adj[2] = -1; }
		}

		// Form a new triangle between the vertex and any edge with adjacency.
		const size_t edgeCount = s_edges.size();
		const TriEdge* edge = s_edges.data();
		for (size_t e = 0; e < edgeCount; e++, edge++)
		{
			if (edge->refCount) { continue; }

			// Form a triangle from vtxIndex -> edge indices.
			addTriangle(vtxIndex, edge->idx[0], edge->idx[1]);
		}
	}

	// The original DF algorithm.
	enum PointSegSide
	{
		PS_INSIDE = -1,
		PS_ON_LINE = 0,
		PS_OUTSIDE = 1
	};

	PointSegSide lineSegmentSide(Vec2f p, Vec2f p0, Vec2f p1)
	{
		f32 dx = p0.x - p1.x;
		f32 dz = p0.z - p1.z;
		if (dx == 0)
		{
			if (dz > 0)
			{
				if (p.z < p1.z || p.z > p0.z || p.x > p0.x) { return PS_INSIDE; }
			}
			else
			{
				if (p.z < p0.z || p.z > p1.z || p.x > p0.x) { return PS_INSIDE; }
			}
			return (p.x == p0.x) ? PS_ON_LINE : PS_OUTSIDE;
		}
		else if (dz == 0)
		{
			if (p.z != p0.z)
			{
				// I believe this should be -1 or +1 depending on if z is less than or greater than z0.
				// Otherwise flat lines always give the same answer.
				return PS_INSIDE;
			}
			if (dx > 0)
			{
				return (p.x > p0.x) ? PS_INSIDE : (p.x < p1.x) ? PS_OUTSIDE : PS_ON_LINE;
			}
			return (p.x > p1.x) ? PS_INSIDE : (p.x < p0.x) ? PS_OUTSIDE : PS_ON_LINE;
		}
		else if (dx > 0)
		{
			if (p.x > p0.x) { return PS_INSIDE; }

			p.x -= p1.x;
			if (dz > 0)
			{
				if (p.z < p1.z || p.z > p0.z) { return PS_INSIDE; }
				p.z -= p1.z;
			}
			else
			{
				if (p.z < p0.z || p.z > p1.z) { return PS_INSIDE; }
				dz = -dz;
				p1.z -= p.z;
				p.z = p1.z;
			}
		}
		else // dx <= 0
		{
			if (p.x > p1.x) { return PS_INSIDE; }

			p.x -= p0.x;
			dx = -dx;
			if (dz > 0)
			{
				if (p.z < p1.z || p.z > p0.z) { return PS_INSIDE; }
				p0.z -= p.z;
				p.z = p0.z;
			}
			else  // dz <= 0
			{
				if (p.z < p0.z || p.z > p1.z) { return PS_INSIDE; }
				dz = -dz;
				p.z -= p0.z;
			}
		}
		const f32 zDx = p.z * dx;
		const f32 xDz = p.x * dz;
		if (xDz == zDx)
		{
			return PS_ON_LINE;
		}
		return (xDz > zDx) ? PS_INSIDE : PS_OUTSIDE;
	}

	f32 sign(f32 x)
	{
		return x < 0.0f ? -1.0f : 1.0f;
	}

	bool pointInsidePolygon(const Polygon* poly, Vec2f p)
	{
		const s32 edgeCount = (s32)poly->edge.size();
		const Edge* edge = poly->edge.data();
		const Edge* last = &edge[edgeCount - 1];
		const Vec2f* vtx = poly->vtx.data();

		const Vec2f* w0 = &vtx[last->i0];
		const Vec2f* w1 = &vtx[last->i1];
		f32 dzLast = w1->z - w0->z;
		s32 crossings = 0;

		for (s32 e = 0; e < edgeCount; e++, edge++)
		{
			w0 = &vtx[edge->i0];
			w1 = &vtx[edge->i1];

			const f32 x0 = w0->x;
			const f32 x1 = w1->x;
			const f32 z0 = w0->z;
			const f32 z1 = w1->z;
			const f32 dz = z1 - z0;

			if (dz != 0)
			{
				if (p.z == z0 && p.x == x0)
				{
					return true;
				}
				else if (p.z != z0)
				{
					if (p.z != z1)
					{
						PointSegSide side = lineSegmentSide(p, { x0, z0 }, { x1, z1 });
						if (side == PS_OUTSIDE)
						{
							crossings++;
						}
						else if (side == PS_ON_LINE)
						{
							return true;
						}
					}
					dzLast = dz;
				}
				else if (p.x != x0)
				{
					if (p.x < x0)
					{
						//f32 dzSignMatches = ((dz >= 0.0f && dzLast >= 0.0f) || (dz <= 0.0f && dzLast <= 0.0f)) ? 1.0f : -1.0f;
						bool dzSignMatches = sign(dz) == sign(dzLast);
						if (dzSignMatches || dzLast == 0)  // the signs match OR dz or dz0 are positive OR dz0 EQUALS 0.
						{
							crossings++;
						}
					}
					dzLast = dz;
				}
			}
			else if (lineSegmentSide(p, { x0, z0 }, { x1, z1 }) == PS_ON_LINE)
			{
				return true;
			}
		}

		return (crossings & 1) != 0;
	}
		
	void constraintSplit(s32 i0, s32 i1, s32 newVtx, Triangle* tri, Vec2f it, const Vec2f* c0, const Vec2f* c1, f32 prevConstIt)
	{
		// Find the matching edge.
		s32 startIndex = -1;
		s32 t0, t1;
		for (s32 i = 0; i < 3; i++)
		{
			s32 a = i, b = (i + 1) % 3;
			if ((tri->idx[a] == i0 && tri->idx[b] == i1) || (tri->idx[a] == i1 && tri->idx[b] == i0))
			{
				startIndex = i;
				t0 = tri->idx[a];
				t1 = tri->idx[b];
				break;
			}
		}
		if (startIndex < 0) { return; }

		// Now see if the line intersects either of the other two edges.
		for (s32 i = 1; i < 3; i++)
		{
			s32 a = (i + startIndex) % 3;
			s32 b = (a + 1) % 3;

			s32 A = tri->idx[a];
			s32 B = tri->idx[b];

			const Vec2f* v0 = &s_vertices[A];
			const Vec2f* v1 = &s_vertices[B];

			// Compute the intersection between line segment c0->c1 and v0->v1
			f32 constInter, triEdgeInter;
			if (!TFE_Math::lineSegmentIntersect(c0, c1, v0, v1, &constInter, &triEdgeInter))
			{
				continue;
			}

			if (constInter - prevConstIt < FLT_EPSILON)
			{
				// We aren't making any progress...
				// Abort to avoid a long running or infinite loop.
				PolyAssert(0);
				return;
			}

			// Constraint ends at vertex 'v0' or 'v1'
			const bool atV0 = triEdgeInter <= eps;
			const bool atV1 = triEdgeInter >= 1.0f - eps;
			if (atV0 || atV1)
			{
				PolyAssert(i != 1 || !atV0);
				PolyAssert(i != 2 || !atV1);
				s32 endVertex = atV0 ? A : B;
				// Triangles are:
				// T(newVtx, endVertex, t0)
				// T(newVtx, t1, endVertex)

				deleteTriangle(tri);
				addTriangle(newVtx, endVertex, t0);
				addTriangle(newVtx, t1, endVertex);
			}
			// Constraint hits the edge.
			else
			{
				s32 adj = tri->adj[a];
				s32 N = newVtx;
				s32 P = (s32)s_vertices.size();
				Vec2f newIt = { v0->x + triEdgeInter * (v1->x - v0->x), v0->z + triEdgeInter * (v1->z - v0->z) };
				s_vertices.push_back(newIt);

				deleteTriangle(tri);
				if (i == 1)  // Next adjacent edge
				{
					PolyAssert(A == t1);
					PolyAssert(B != t0);
					addTriangle(N, t1, P);
					addTriangle(N, P, B);
					addTriangle(N, B, t0);
				}
				else
				{
					PolyAssert(B == t0);
					PolyAssert(A != t1);
					addTriangle(N, t1, A);
					addTriangle(N, A, P);
					addTriangle(N, P, t0);
				}

				// Continue to the next triangle.
				if (adj >= 0 && s_triangles[adj].allocated && constInter < 1.0f - eps)
				{
					constraintSplit(A, B, P, &s_triangles[adj], newIt, c0, c1, constInter);
				}
			}
			break;
		}
	}

	bool insertConstraint(Polygon* poly, const Edge* constraint, Triangle* tri, s32 startIndex)
	{
		// *If* the constraint intersects an edge of *this* triangle, it must
		// be the opposite edge.
		s32 e0 = (startIndex + 1) % 3;
		s32 e1 = (e0 + 1) % 3;
		s32 i0 = tri->idx[e0];
		s32 i1 = tri->idx[e1];
		const Vec2f* c0 = &s_vertices[constraint->i0];
		const Vec2f* c1 = &s_vertices[constraint->i1];

		const Vec2f* v0 = &s_vertices[i0];
		const Vec2f* v1 = &s_vertices[i1];

		// Compute the intersection between line segment c0->c1 and v0->v1
		f32 constInter, triEdgeInter;
		if (!TFE_Math::lineSegmentIntersect(c0, c1, v0, v1, &constInter, &triEdgeInter))
		{
			return false;
		}
		// Make sure it is worth the hassle...
		if (constInter <= FLT_EPSILON || constInter >= 1.0f - FLT_EPSILON)
		{
			return false;
		}

		// *If* the intersection exists, *AND* it is before the end of the segment, recurse into the adjoining triangle.
		// Store the adjacency and edge indices for later.
		const s32 adj = tri->adj[e0];
		const s32 S = tri->idx[startIndex];
		
		// Delete triangle.
		deleteTriangle(tri);

		// Add two new triangles:
		// startIndex -> iEdge -> newVtx
		// startIndex -> newVtx -> iEdge + 1
		s32 N = (s32)s_vertices.size();
		Vec2f it = { v0->x + triEdgeInter * (v1->x - v0->x), v0->z + triEdgeInter * (v1->z - v0->z) };
		s_vertices.push_back(it);
		addTriangle(S, i0, N);
		addTriangle(S, N, i1);

		// Move on to the next triangle.
		if (adj >= 0 && s_triangles[adj].allocated && constInter <= 1.0f - eps)
		{
			constraintSplit(i0, i1, N, &s_triangles[adj], it, c0, c1, constInter);
		}
		return true;
	}

	// Compute a valid triangulation for the polygon.
	// Polygons may be complex, self-intersecting, and even be incomplete.
	// The goal is a *robust* triangulation system that will always produce
	// a plausible result even with malformed data.
	bool computeTriangulation(Polygon* poly, u32 debug)
	{
		if (!s_init)
		{
			s_init = true;
			s_freeList.reserve(256);
			s_constraints.reserve(256);
			s_triangles.reserve(1024);
			s_vertices.reserve(1024);
		}

		s_freeList.clear();
		s_triangles.clear();
		s_vertices.clear();
		s_constraints.clear();

		poly->triVtx.clear();
		poly->triIdx.clear();

		const size_t edgeCount = poly->edge.size();
		if (edgeCount < 3)
		{
			// ERROR!
			return false;
		}

		// 0. If the polygon has 3 edges, assume it is already convex.
		// Note: this assumption *cannot* be made for 4 edges, that generates failures in one of the original levels.
		if (edgeCount == 3)
		{
			poly->triVtx.resize(3);
			poly->triIdx.resize(3);

			for (s32 i = 0; i < 3; i++)
			{
				poly->triVtx[i] = poly->vtx[poly->edge[i].i0];
			}
			poly->triIdx[0] = 0;
			poly->triIdx[1] = 1;
			poly->triIdx[2] = 2;
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
			addPoint({ vtx->x - s_coordCenter.x, vtx->z - s_coordCenter.z });
		}

		// 2. Insert edges, splitting triangles as needed (note: new vertices may be added, but polygons should be re-triangulated instead).
		size_t triCount = s_triangles.size();
		const Edge* edge = poly->edge.data();
		for (size_t e = 0; e < edgeCount; e++, edge++)
		{
			// First check to see if the edge already exists in the set.
			s32 i0 = edge->i0 + 3;  // offset due to the super-triangle created at the beginning.
			s32 i1 = edge->i1 + 3;

			bool edgeFound = false;
			Triangle* tri = tri = s_triangles.data();
			for (size_t t = 0; t < triCount; t++, tri++)
			{
				if (!tri->allocated) { continue; }
				if ((tri->idx[0] == i0 && tri->idx[1] == i1) || (tri->idx[0] == i1 && tri->idx[1] == i0) ||
					(tri->idx[1] == i0 && tri->idx[2] == i1) || (tri->idx[1] == i1 && tri->idx[2] == i0) ||
					(tri->idx[2] == i0 && tri->idx[0] == i1) || (tri->idx[2] == i1 && tri->idx[0] == i0))
				{
					// Make sure this isn't part of the super triangle!
					if (tri->idx[0] >= 3 && tri->idx[1] >= 3 && tri->idx[2] >= 3)
					{
						edgeFound = true;
						break;
					}
				}
			}

			// Other add the edge for insertion.
			if (!edgeFound)
			{
				s_constraints.push_back({ i0, i1 });
			}
		}
		const size_t constraintCount = s_constraints.size();
		const Edge* constraint = s_constraints.data();
		for (size_t e = 0; e < constraintCount; e++, constraint++)
		{
			// Find all triangles that share a vertex with the constraint.
			Triangle* triList = s_triangles.data();
			size_t curTriCount = s_triangles.size(); // Update the count since triangles can change.
			bool constraintHasMatch = false;
			for (size_t t = 0; t < curTriCount; t++)
			{
				Triangle* tri = &triList[t];
				if (!tri->allocated) { continue; }
				s32 startIndex = -1;
				if (tri->idx[0] == constraint->i0) { startIndex = 0; }
				else if (tri->idx[1] == constraint->i0) { startIndex = 1; }
				else if (tri->idx[2] == constraint->i0) { startIndex = 2; }

				if (startIndex >= 0)
				{
					if (insertConstraint(poly, constraint, tri, startIndex))
					{
						constraintHasMatch = true;
					}
					// The triangle count may change, but triangle order will not.
					// The memory address may change if the array is resized.
					triList = s_triangles.data();
				}
			}
			//PolyAssert(constraintHasMatch);
		}

		// 3. Remove triangles that contain a super triangle vertex.
		triCount = s_triangles.size();
		Triangle* tri = s_triangles.data();
		if (!(debug & PDBG_SHOW_SUPERTRI))
		{
			for (size_t t = 0; t < triCount; t++, tri++)
			{
				if (!tri->allocated) { continue; }
				if (tri->idx[0] < 3 || tri->idx[1] < 3 || tri->idx[2] < 3)
				{
					deleteTriangle(tri);
					continue;
				}
			}
		}

		// 4. Given the final resulting triangles, determine which are *inside* of the complex polygon, discard the rest.
		tri = s_triangles.data();
		triCount = s_triangles.size();
		for (size_t t = 0; t < triCount; t++, tri++)
		{
			if (!tri->allocated) { continue; }
			// Determine if the circumcenter is inside the polygon.
			if (!(debug & PDBG_SHOW_SUPERTRI) && !(debug & PDBG_SKIP_INSIDE_TEST))
			{
				// Always jitter the centroid z slightly so that it is less likely to be exactly the same as a vertex z,
				// which can cause detection issues.
				tri->centroid.z += 0.01f;
				if (!pointInsidePolygon(poly, { tri->centroid.x + s_coordCenter.x, tri->centroid.z + s_coordCenter.z }))
				{
					deleteTriangle(tri);
					continue;
				}
			}

			// For now just add it here.
			poly->triIdx.push_back(tri->idx[0]);
			poly->triIdx.push_back(tri->idx[1]);
			poly->triIdx.push_back(tri->idx[2]);
		}
		// TODO: Remove unused vertices.
		const size_t finalVtxCount = s_vertices.size();
		poly->triVtx.resize(finalVtxCount);

		const Vec2f* srcVtx = s_vertices.data();
		Vec2f* dstVtx = poly->triVtx.data();
		for (size_t v = 0; v < finalVtxCount; v++, srcVtx++)
		{
			dstVtx[v] = { srcVtx->x + s_coordCenter.x, srcVtx->z + s_coordCenter.z };
		}

		return true;
	}

	bool addEdgeToBPoly(Vec2f v0, Vec2f v1, f32 uOffset, s32 wallIndex, BPolygon* poly)
	{
		// Discard degenerate edges.
		if (vtxEqual(&v0, &v1))
		{
			return false;
		}

		BEdge newEdge = {};
		newEdge.v0 = v0;
		newEdge.v1 = v1;
		newEdge.u0 = uOffset;
		newEdge.u1 = uOffset + TFE_Math::distance(&v0, &v1) * 8.0f;
		newEdge.srcWallIndex = wallIndex;

		const s32 newEdgeIndex = (s32)poly->edges.size();
		const s32 edgeCount = (s32)poly->edges.size();
		BEdge* edge = poly->edges.data();
		for (s32 e = 0; e < edgeCount; e++, edge++)
		{
			if (vtxEqual(&v1, &edge->v0))
			{
				newEdge.nextEdge = e;
				edge->prevEdge = newEdgeIndex;
			}
			if (vtxEqual(&v0, &edge->v1))
			{
				newEdge.prevEdge = e;
				edge->nextEdge = newEdgeIndex;
			}
		}

		poly->edges.push_back(newEdge);
		return true;
	}

	struct Intersection
	{
		Vec2f vI;
		f32 u;
		f32 v;
		s32 eS;
		s32 eC;
		s32 id;
	};

	struct IntersectionClip
	{
		s32 eC;
		std::vector<Intersection> it;
	};

	bool sortByU(Intersection& a, Intersection& b)
	{
		return a.u < b.u;
	}

	struct Contour
	{
		bool clockwise = true;
		bool outsideClip = false;
		std::vector<Vec2f> vtx;
	};

	bool buildContour(const BEdge* startEdge, const std::vector<BEdge>& edgesList, std::vector<s32>& remEdges, Contour* outContour)
	{
		if (!outContour) { return false; }
		outContour->vtx.clear();

		Vec2f startPt = startEdge->v0;
		s32 nextEdge = startEdge->nextEdge;
		outContour->vtx.push_back(startPt);
		// Loop until the end or we are back at the start.
		while (!remEdges.empty())
		{
			const BEdge* edge = &edgesList[nextEdge];
			if (vtxEqual(&edge->v0, &startPt))
			{
				// Vertex with 3+ edges intersecting.
				break;
			}
			outContour->vtx.push_back(edge->v0);

			// Erase the remaining edge.
			const s32 remEdgeCount = (s32)remEdges.size();
			const s32* remIdx = remEdges.data();
			for (s32 i = 0; i < remEdgeCount; i++)
			{
				if (remIdx[i] == nextEdge)
				{
					remEdges.erase(remEdges.begin() + i);
					break;
				}
			}
			nextEdge = edge->nextEdge;

			// Reached the end.
			if (vtxEqual(&edge->v1, &startPt))
			{
				break;
			}
		}

		// No valid contour.
		if (outContour->vtx.empty())
		{
			return false;
		}

		// Next determine the winding order.
		const f32 area = TFE_Polygon::signedArea((s32)outContour->vtx.size(), outContour->vtx.data());
		outContour->clockwise = (area >= 0.0f);
		outContour->outsideClip = false;
		return true;
	}

	void buildPathsFromContours(const std::vector<Contour>& contourList, ClipperLib::Paths* paths)
	{
		const f64 precision = 65536.0;
		const s32 contourCount = (s32)contourList.size();
		const Contour* contour = contourList.data();

		paths->resize(contourCount);
		ClipperLib::Path* path = paths->data();
		for (s32 i = 0; i < contourCount; i++, contour++, path++)
		{
			const s32 vtxCount = (s32)contour->vtx.size();
			const Vec2f* vtx = contour->vtx.data();

			path->resize(vtxCount);
			ClipperLib::IntPoint* pOut = path->data();
			for (s32 v = 0; v < vtxCount; v++, vtx++, pOut++)
			{
				pOut->X = ClipperLib::cInt((f64)vtx->x * precision);
				pOut->Y = ClipperLib::cInt((f64)vtx->z * precision);
			}
		}
	}

	void buildContoursFromPaths(const ClipperLib::Paths& paths, s32 outsideClipCount, std::vector<Contour>* contourList)
	{
		const f64 precision = 1.0 / 65536.0;
		const s32 contourCount = (s32)paths.size();
		const ClipperLib::Path* path = paths.data();

		contourList->resize(contourCount);
		Contour* contour = contourList->data();
		for (s32 i = 0; i < contourCount; i++, contour++, path++)
		{
			const s32 vtxCount = (s32)path->size();
			const ClipperLib::IntPoint* srcVtx = path->data();

			contour->vtx.resize(vtxCount);
			Vec2f* dstVtx = contour->vtx.data();

			for (s32 v = 0; v < vtxCount; v++, srcVtx++, dstVtx++)
			{
				dstVtx->x = f32((f64)srcVtx->X * precision);
				dstVtx->z = f32((f64)srcVtx->Y * precision);
			}

			// Next determine the winding order.
			const f32 area = TFE_Polygon::signedArea(vtxCount, contour->vtx.data());
			contour->clockwise = (area >= 0.0f);
			contour->outsideClip = i < outsideClipCount;
		}
	}

	void buildContoursFromPolygon(const BPolygon* poly, std::vector<Contour>* contours)
	{
		// Assume clockwise for the exterior contour, counter-clockwise for holes.
		// Copy the edges, so they can be removed after processing.
		const std::vector<BEdge>& edges = poly->edges;
		std::vector<s32> remEdges;

		const s32 edgeCount = (s32)edges.size();
		remEdges.resize(edgeCount);
		for (s32 i = 0; i < edgeCount; i++) { remEdges[i] = i; }

		while (!remEdges.empty())
		{
			Contour contour = {};
			s32 startEdgeIndex = remEdges.front();
			remEdges.erase(remEdges.begin());

			if (!buildContour(&edges[startEdgeIndex], edges, remEdges, &contour))
			{
				break;
			}
			const size_t index = contours->size();
			contours->push_back(contour);
			// Make sure the exterior edge is first, followed by holes.
			if (contour.clockwise && index != 0)
			{
				std::swap(contours->front(), contours->back());
			}
		}
	}
		
	void clipInit()
	{
		s_clipper = new ClipperLib::Clipper(ClipperLib::ioPreserveCollinear | ClipperLib::ioReverseSolution | ClipperLib::ioStrictlySimple);
	}

	void clipDestroy()
	{
		delete s_clipper;
		s_clipper = nullptr;
	}

	bool addEdgeIntersectionsToPoly(BPolygon* subject, const BPolygon* clip)
	{
		BPolygon tmp = {};

		const s32 subjEdgeCount = (s32)subject->edges.size();
		const BEdge* sEdge = subject->edges.data();
		std::vector<Intersection> interSrc;

		bool hasIntersections = false;
		for (s32 eS = 0; eS < subjEdgeCount; eS++, sEdge++)
		{
			interSrc.clear();
			s32 itrId = 0;

			const s32 clipEdgeCount = (s32)clip->edges.size();
			const BEdge* cEdge = clip->edges.data();
			for (s32 eC = 0; eC < clipEdgeCount; eC++, cEdge++)
			{
				Vec2f vI;
				f32 u, v;
				if (lineSegmentsIntersect(sEdge->v0, sEdge->v1, cEdge->v0, cEdge->v1, &vI, &u, &v))
				{
					// Handle floating point precision - treat intersection values close enough to 0 or 1 as if it is *exactly* 0 or 1.
					if (u < FLT_EPSILON) { u = 0.0f; vI = sEdge->v0; }
					else if (u > 1.0f - FLT_EPSILON) { u = 1.0f; vI = sEdge->v1; }
					assert(std::isfinite(u));

					if (v < FLT_EPSILON) { v = 0.0f; }
					else if (v > 1.0f - FLT_EPSILON) { v = 1.0f; }
					assert(std::isfinite(v));

					// Make sure the intersection is on the interior of the *subject* edge.
					if (u > 0.0f && u < 1.0f)
					{
						// Store intersections affecting 'eS'.
						interSrc.push_back({ vI, u, v, eS, eC, itrId });
						itrId++;
					}
				}
			}

			const s32 iCount = (s32)interSrc.size();
			if (iCount)
			{
				// Subject edge is clipped, put clipped segments into the new subject polygon in
				// the correct sorted order.
				std::sort(interSrc.begin(), interSrc.end(), sortByU);
				Intersection* inter = interSrc.data();
				Vec2f v0 = sEdge->v0;
				f32 u0 = 0.0f;
				for (s32 i = 0; i < iCount; i++, inter++)
				{
					addEdgeToBPoly(v0, inter->vI, u0, sEdge->srcWallIndex, &tmp);
					v0 = inter->vI;
					u0 = sEdge->u0 + inter->u*(sEdge->u1 - sEdge->u0);
				}
				// Insert the final edge.
				addEdgeToBPoly(v0, sEdge->v1, u0, sEdge->srcWallIndex, &tmp);
				hasIntersections = true;
			}
			else
			{
				// Subject edge is not clipped, put the full segment into the subject polygon.
				addEdgeToBPoly(sEdge->v0, sEdge->v1, sEdge->u0, sEdge->srcWallIndex, &tmp);
			}
		}

		if (hasIntersections)
		{
			*subject = tmp;
		}
		return hasIntersections;
	}
		
	// Returns the number of polygons outside of the clip area.
	void clipPolygons(const BPolygon* subject, const BPolygon* clip, std::vector<BPolygon>& outPoly, BoolMode boolMode)
	{
		s32 polysOutsideOfClip = 0;

		std::vector<Contour> subjContours;
		std::vector<Contour> clipContours;
		buildContoursFromPolygon(subject, &subjContours);
		buildContoursFromPolygon(clip, &clipContours);

		ClipperLib::Paths subjPaths, clipPaths;
		buildPathsFromContours(subjContours, &subjPaths);
		buildPathsFromContours(clipContours, &clipPaths);

		s_clipper->Clear();
		s_clipper->AddPaths(subjPaths, ClipperLib::ptSubject, true);
		s_clipper->AddPaths(clipPaths, ClipperLib::ptClip, true);

		outPoly.clear();
		std::vector<BPolygon> firstPassPoly;
		ClipperLib::Paths solution;

		// We always want the difference in order to get the new edges.
		bool result = s_clipper->Execute(ClipperLib::ctDifference, solution, ClipperLib::pftNonZero);
		if (!result) { return; }
		polysOutsideOfClip = (s32)solution.size();

		if (boolMode == BMODE_MERGE)
		{
			s_clipper->Clear();
			s_clipper->AddPaths(subjPaths, ClipperLib::ptSubject, true);
			s_clipper->AddPaths(clipPaths, ClipperLib::ptClip, true);

			// Intersection - intersection part between clip polygon and original polygon.
			ClipperLib::Paths solution2;
			result = s_clipper->Execute(ClipperLib::ctIntersection, solution2, ClipperLib::pftNonZero);
			if (!result) { return; }

			// Merge solutions.
			solution.insert(solution.end(), solution2.begin(), solution2.end());
		}
		if (solution.empty()) { return; }

		// Process the solution.
		// Build contours and determine winding.
		std::vector<Contour> results;
		buildContoursFromPaths(solution, polysOutsideOfClip, &results);
		if (results.empty()) { return; }

		// Outer polygons
		const s32 outCount = (s32)results.size();
		Contour* contour = results.data();
		std::vector<s32> outPolyIndex;
		for (s32 i = 0; i < outCount; i++, contour++)
		{
			// Skip holes for the first pass.
			if (!contour->clockwise) { continue; }
			outPolyIndex.push_back(i);

			BPolygon poly = {};

			const s32 vtxCount = (s32)contour->vtx.size();
			const Vec2f* vtx = contour->vtx.data();

			poly.edges.resize(vtxCount);
			BEdge* edge = poly.edges.data();

			for (s32 v = 0; v < vtxCount; v++, edge++)
			{
				s32 next = (v + 1) % vtxCount;

				*edge = {};
				edge->v0 = vtx[v];
				edge->v1 = vtx[next];
				// For now.
				edge->srcWallIndex = 0;
				edge->u0 = 0.0f;
			}
			poly.outsideClipRegion = contour->outsideClip;

			firstPassPoly.push_back(poly);
		}

		// Insert holes.
		const s32 outerPolyCount = (s32)firstPassPoly.size();

		// Create polygon for point in poly tests.
		// TODO: Remove BPolygon entirely?
		std::vector<Polygon> polys;
		polys.resize(outerPolyCount);
		Polygon* polyData = polys.data();
		for (s32 p = 0; p < outerPolyCount; p++, polyData++)
		{
			Contour& contour = results[outPolyIndex[p]];
			const s32 vtxCount = (s32)contour.vtx.size();
			polyData->vtx = contour.vtx;
			polyData->edge.resize(vtxCount);
			for (s32 e = 0; e < vtxCount; e++)
			{
				polyData->edge[e].i0 = e;
				polyData->edge[e].i1 = (e + 1) % vtxCount;
			}
		}

		contour = results.data();
		for (s32 i = 0; i < outCount; i++, contour++)
		{
			// Skip outer polygons for the second pass...
			if (contour->clockwise) { continue; }

			const s32 vtxCount = (s32)contour->vtx.size();
			const Vec2f* vtx = contour->vtx.data();

			// Which parent does the hole belong to?
			BPolygon* parent = nullptr;
			BPolygon* outerPoly = firstPassPoly.data();
			for (s32 v = 0; v < vtxCount && !parent; v++)
			{
				for (s32 p = 0; p < outerPolyCount; p++)
				{
					if (TFE_Polygon::pointInsidePolygon(&polys[p], vtx[v]))
					{
						parent = &outerPoly[p];
						break;
					}
				}
			}
			assert(parent);

			s32 startIndex = (s32)parent->edges.size();
			parent->edges.resize(vtxCount + startIndex);
			BEdge* edge = parent->edges.data() + startIndex;

			for (s32 v = 0; v < vtxCount; v++, edge++)
			{
				s32 next = (v + 1) % vtxCount;

				*edge = {};
				edge->v0 = vtx[v];
				edge->v1 = vtx[next];
				// For now.
				edge->srcWallIndex = 0;
				edge->u0 = 0.0f;
			}
		}

		// Insert intersections with the clip polygon.
		const BPolygon* srcPoly = firstPassPoly.data();
		outPoly.resize(outerPolyCount);
		BPolygon* dstPoly = outPoly.data();
		std::vector<Intersection> interSrc;
		for (s32 i = 0; i < outerPolyCount; i++, srcPoly++, dstPoly++)
		{
			const s32 subjEdgeCount = (s32)srcPoly->edges.size();
			const BEdge* sEdge = srcPoly->edges.data();

			*dstPoly = {};
			dstPoly->outsideClipRegion = srcPoly->outsideClipRegion;

			for (s32 eS = 0; eS < subjEdgeCount; eS++, sEdge++)
			{
				interSrc.clear();
				s32 itrId = 0;

				const s32 clipEdgeCount = (s32)clip->edges.size();
				const BEdge* cEdge = clip->edges.data();
				for (s32 eC = 0; eC < clipEdgeCount; eC++, cEdge++)
				{
					Vec2f vI;
					f32 u, v;
					if (lineSegmentsIntersect(sEdge->v0, sEdge->v1, cEdge->v0, cEdge->v1, &vI, &u, &v))
					{
						// Handle floating point precision - treat intersection values close enough to 0 or 1 as if it is *exactly* 0 or 1.
						if (u < FLT_EPSILON) { u = 0.0f; vI = sEdge->v0; }
						else if (u > 1.0f - FLT_EPSILON) { u = 1.0f; vI = sEdge->v1; }
						assert(std::isfinite(u));

						if (v < FLT_EPSILON) { v = 0.0f; }
						else if (v > 1.0f - FLT_EPSILON) { v = 1.0f; }
						assert(std::isfinite(v));

						// Make sure the intersection is on the interior of the *subject* edge.
						if (u > 0.0f && u < 1.0f)
						{
							// Store intersections affecting 'eS'.
							interSrc.push_back({ vI, u, v, eS, eC, itrId });
							itrId++;
						}
					}
				}

				const s32 iCount = (s32)interSrc.size();
				if (iCount)
				{
					// Subject edge is clipped, put clipped segments into the new subject polygon in
					// the correct sorted order.
					std::sort(interSrc.begin(), interSrc.end(), sortByU);
					Intersection* inter = interSrc.data();
					Vec2f v0 = sEdge->v0;
					f32 u0 = 0.0f;
					for (s32 i = 0; i < iCount; i++, inter++)
					{
						addEdgeToBPoly(v0, inter->vI, u0, sEdge->srcWallIndex, dstPoly);
						v0 = inter->vI;
						u0 = sEdge->u0 + inter->u*(sEdge->u1 - sEdge->u0);
					}
					// Insert the final edge.
					addEdgeToBPoly(v0, sEdge->v1, u0, sEdge->srcWallIndex, dstPoly);
				}
				else
				{
					// Subject edge is not clipped, put the full segment into the subject polygon.
					addEdgeToBPoly(sEdge->v0, sEdge->v1, sEdge->u0, sEdge->srcWallIndex, dstPoly);
				}
			}
		}

		if (boolMode == BMODE_MERGE)
		{
			// Clip polygons *inside* the original clip polygon with those *outside*
			BPolygon* srcPoly = outPoly.data();
			for (s32 i = 0; i < outerPolyCount; i++)
			{
				if (srcPoly[i].outsideClipRegion) { continue; }
				for (s32 j = 0; j < outerPolyCount; j++)
				{
					if (i == j || !srcPoly[j].outsideClipRegion) { continue; }

					BPolygon tmpPoly = {};
					tmpPoly.outsideClipRegion = srcPoly[i].outsideClipRegion;

					const s32 subjEdgeCount = (s32)srcPoly[i].edges.size();
					const BEdge* sEdge = srcPoly[i].edges.data();
					for (s32 eS = 0; eS < subjEdgeCount; eS++, sEdge++)
					{
						interSrc.clear();
						s32 itrId = 0;

						const s32 clipEdgeCount = (s32)srcPoly[j].edges.size();
						const BEdge* cEdge = srcPoly[j].edges.data();
						for (s32 eC = 0; eC < clipEdgeCount; eC++, cEdge++)
						{
							Vec2f vI;
							f32 u, v;
							if (lineSegmentsIntersect(sEdge->v0, sEdge->v1, cEdge->v0, cEdge->v1, &vI, &u, &v))
							{
								// Handle floating point precision - treat intersection values close enough to 0 or 1 as if it is *exactly* 0 or 1.
								if (u < FLT_EPSILON) { u = 0.0f; vI = sEdge->v0; }
								else if (u > 1.0f - FLT_EPSILON) { u = 1.0f; vI = sEdge->v1; }
								assert(std::isfinite(u));

								if (v < FLT_EPSILON) { v = 0.0f; }
								else if (v > 1.0f - FLT_EPSILON) { v = 1.0f; }
								assert(std::isfinite(v));

								// Make sure the intersection is on the interior of the *subject* edge.
								if (u > 0.0f && u < 1.0f)
								{
									// Store intersections affecting 'eS'.
									interSrc.push_back({ vI, u, v, eS, eC, itrId });
									itrId++;
								}
							}
						}

						const s32 iCount = (s32)interSrc.size();
						if (iCount)
						{
							// Subject edge is clipped, put clipped segments into the new subject polygon in
							// the correct sorted order.
							std::sort(interSrc.begin(), interSrc.end(), sortByU);
							Intersection* inter = interSrc.data();
							Vec2f v0 = sEdge->v0;
							f32 u0 = 0.0f;
							for (s32 i = 0; i < iCount; i++, inter++)
							{
								addEdgeToBPoly(v0, inter->vI, u0, sEdge->srcWallIndex, &tmpPoly);
								v0 = inter->vI;
								u0 = sEdge->u0 + inter->u*(sEdge->u1 - sEdge->u0);
							}
							// Insert the final edge.
							addEdgeToBPoly(v0, sEdge->v1, u0, sEdge->srcWallIndex, &tmpPoly);
						}
						else
						{
							// Subject edge is not clipped, put the full segment into the subject polygon.
							addEdgeToBPoly(sEdge->v0, sEdge->v1, sEdge->u0, sEdge->srcWallIndex, &tmpPoly);
						}
					}

					srcPoly[i] = tmpPoly;
				}
			}
		}
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

	bool lineSegmentsIntersect(Vec2f a0, Vec2f a1, Vec2f b0, Vec2f b1, Vec2f* vI, f32* u, f32* v)
	{
		const Vec2f b = { a1.x - a0.x, a1.z - a0.z };
		const Vec2f d = { b1.x - b0.x, b1.z - b0.z };
		const f32 denom = b.x*d.z - b.z*d.x;	// cross product.
		// Lines are parallel if denom == 0.0
		if (fabsf(denom) < FLT_EPSILON) { return false; }
		const f32 scale = 1.0f / denom;

		Vec2f c = { b0.x - a0.x, b0.z - a0.z };
		f32 s = (c.x*d.z - c.z*d.x) * scale;
		if (s < 0.0f || s > 1.0f) { return false; }

		f32 t = (c.x*b.z - c.z*b.x) * scale;
		if (t < 0.0f || t > 1.0f) { return false; }

		if (vI)
		{
			vI->x = a0.x + s * b.x;
			vI->z = a0.z + s * b.z;
		}
		if (u) { *u = s; }
		if (v) { *v = t; }

		// Intersection = a0 + s*b;
		return true;
	}
		
	bool vtxEqual(const Vec2f* a, const Vec2f* b)
	{
		//const f32 eps = 0.00001f;
		return fabsf(a->x - b->x) < eps && fabsf(a->z - b->z) < eps;
	}

	bool vtxEqual(const Vec3f* a, const Vec3f* b)
	{
		//const f32 eps = 0.00001f;
		return fabsf(a->x - b->x) < eps && fabsf(a->y - b->y) < eps && fabsf(a->z - b->z) < eps;
	}
}
