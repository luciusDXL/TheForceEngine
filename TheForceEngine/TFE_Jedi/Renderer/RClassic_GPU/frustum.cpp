#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_System/math.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include "frustum.h"
#include "../rcommon.h"

namespace TFE_Jedi
{
	// A small epsilon value to make point vs. plane side determinations more robust to numerical error.
	const f32 c_planeEps = 0.0001f;

	// When building the camera frustum, we instead build a "super frustum" that is slightly set backward and slightly larger
	// in order to avoid precision issues when the camera is right on top of an adjoin.
	const f32 c_superSidePlaneNormalScale = 0.98f;
	const f32 c_superNearPlaneOffsetScale = 0.1f;

	static Frustum s_frustumStack[256];
	static u32 s_frustumStackPtr = 0;

	extern Mat3  s_cameraMtx;
	extern Mat4  s_cameraProj;
	extern Vec3f s_cameraPos;

	void frustum_createQuad(Vec3f corner0, Vec3f corner1, Polygon* poly);
	f32 frustum_planeDist(const Vec4f* plane, const Vec3f* pos);

	void frustum_clearStack()
	{
		s_frustumStackPtr = 0;
	}

	void frustum_copy(const Frustum* src, Frustum* dst)
	{
		dst->planeCount = src->planeCount;
		for (u32 i = 0; i < src->planeCount; i++)
		{
			dst->planes[i] = src->planes[i];
		}
	}

	void frustum_push(Frustum& frustum)
	{
		frustum_copy(&frustum, &s_frustumStack[s_frustumStackPtr]);
		s_frustumStackPtr++;
	}

	Frustum* frustum_pop()
	{
		s_frustumStackPtr--;
		return &s_frustumStack[s_frustumStackPtr];
	}

	Frustum* frustum_getBack()
	{
		return &s_frustumStack[s_frustumStackPtr - 1];
	}

	Frustum* frustum_getFront()
	{
		return &s_frustumStack[0];
	}

	bool frustum_sphereInside(Vec3f pos, f32 radius)
	{
		Frustum* frustum = frustum_getBack();

		Vec4f* plane = frustum->planes;
		for (u32 i = 0; i < frustum->planeCount; i++)
		{
			f32 dist = frustum_planeDist(&plane[i], &pos);
			if (dist + radius < 0.0f)
			{
				return false;
			}
		}
		return true;
	}

	bool frustum_quadInside(const Vec3f v0, const Vec3f v1)
	{
		const Frustum* frustum = frustum_getBack();
		if (!frustum || frustum->planeCount < 1) { return true; }

		Polygon poly;
		frustum_createQuad(v0, v1, &poly);

		const f32 nearPlaneEps = -c_planeEps * 10.0f;
		const u32 nearPlaneIdx = frustum->planeCount - 1;

		const Vec4f* plane = frustum->planes;
		for (u32 i = 0; i < frustum->planeCount; i++, plane++)
		{
			// Make the near plane test less aggressive.
			const f32 eps = (i == nearPlaneIdx) ? nearPlaneEps : c_planeEps;

			// Are all vertices outside of this plane?
			bool outside = true;
			const Vec3f* vtx = poly.vtx;
			for (u32 v = 0; v < 4; v++, vtx++)
			{
				const f32 dist = frustum_planeDist(plane, vtx);
				if (dist >= eps)
				{
					outside = false;
					break;
				}
			}

			if (outside)
			{
				return false;
			}
		}
		return true;
	}
						
	bool frustum_clipQuadToFrustum(Vec3f corner0, Vec3f corner1, Polygon* output)
	{
		Frustum* frustum = frustum_getBack();
		return frustum_clipQuadToPlanes(frustum->planeCount, frustum->planes, corner0, corner1, output);
	}

	bool frustum_clipQuadToPlanes(s32 count, const Vec4f* plane, Vec3f corner0, Vec3f corner1, Polygon* output)
	{
		Polygon poly[2];
		Polygon* cur = &poly[0];
		Polygon* next = &poly[1];
		frustum_createQuad(corner0, corner1, cur);

		f32 vtxDist[256];
		for (s32 p = 0; p < count; p++, plane++)
		{
			s32 positive = 0, negative = 0;
			next->vertexCount = 0;
			assert(cur->vertexCount <= 256);

			// Process the vertices.
			for (s32 v = 0; v < cur->vertexCount; v++)
			{
				vtxDist[v] = frustum_planeDist(plane, &cur->vtx[v]);
				if (vtxDist[v] >= c_planeEps)
				{
					positive++;
				}
				else if (vtxDist[v] <= -c_planeEps)
				{
					negative++;
				}
				else
				{
					// Point is on the plane.
					vtxDist[v] = 0.0f;
				}
			}

			// Return early or skip plane.
			if (positive == cur->vertexCount)
			{
				// Nothing to clip, moving on.
				continue;
			}
			else if (negative == cur->vertexCount)
			{
				// The entire polygon is behind the plane, we are done here.
				return false;
			}

			// Process the edges.
			for (s32 v = 0; v < cur->vertexCount; v++)
			{
				const s32 a = v;
				const s32 b = (v + 1) % cur->vertexCount;

				f32 d0 = vtxDist[a], d1 = vtxDist[b];
				if (d0 < 0.0f && d1 < 0.0f)
				{
					// Cull the edge.
					continue;
				}
				if (d0 >= 0.0f && d1 >= 0.0f)
				{
					// The edge does not need clipping.
					next->vtx[next->vertexCount++] = cur->vtx[a];
					continue;
				}
				// Calculate the edge intersection with the plane.
				const f32 t = -d0 / (d1 - d0);
				Vec3f intersect = { (1.0f - t)*cur->vtx[a].x + t * cur->vtx[b].x, (1.0f - t)*cur->vtx[a].y + t * cur->vtx[b].y,
									(1.0f - t)*cur->vtx[a].z + t * cur->vtx[b].z };
				if (d0 > 0.0f)
				{
					// The first vertex of the edge is inside and should be included.
					next->vtx[next->vertexCount++] = cur->vtx[a];
				}
				// The intersection should be included.
				if (t < 1.0f)
				{
					next->vtx[next->vertexCount++] = intersect;
				}
			}
			// Swap polygons for the next plane.
			std::swap(cur, next);
			// If the new polygon has less than 3 vertices, than it has been culled.
			if (cur->vertexCount < 3)
			{
				return false;
			}
		}

		// Output the clipped polygon.
		assert(cur->vertexCount <= 256);
		output->vertexCount = cur->vertexCount;
		for (s32 i = 0; i < output->vertexCount; i++)
		{
			output->vtx[i] = cur->vtx[i];
		}
		return true;
	}
		
	void frustum_buildFromPolygon(const Polygon* polygon, Frustum* newFrustum)
	{
		s32 count = polygon->vertexCount;
		newFrustum->planeCount = 0;

		// Sides.
		for (s32 i = 0; i < count; i++)
		{
			s32 e0 = i, e1 = (i + 1) % count;
			Vec3f S = { polygon->vtx[e0].x - s_cameraPos.x, polygon->vtx[e0].y - s_cameraPos.y, polygon->vtx[e0].z - s_cameraPos.z };
			Vec3f T = { polygon->vtx[e1].x - s_cameraPos.x, polygon->vtx[e1].y - s_cameraPos.y, polygon->vtx[e1].z - s_cameraPos.z };
			Vec3f N = TFE_Math::cross(&S, &T);
			if (TFE_Math::dot(&N, &N) <= 0.0f)
			{
				continue;
			}
			N = TFE_Math::normalize(&N);
			f32 d = -TFE_Math::dot(&N, &s_cameraPos);
			newFrustum->planes[newFrustum->planeCount++] = { N.x, N.y, N.z, d };
		}

		// Near plane.
		Vec3f O = { polygon->vtx[0].x, polygon->vtx[0].y, polygon->vtx[0].z };
		Vec3f S = { polygon->vtx[1].x - polygon->vtx[0].x, polygon->vtx[1].y - polygon->vtx[0].y, polygon->vtx[1].z - polygon->vtx[0].z };
		Vec3f T = { polygon->vtx[2].x - polygon->vtx[0].x, polygon->vtx[2].y - polygon->vtx[0].y, polygon->vtx[2].z - polygon->vtx[0].z };
		Vec3f N = TFE_Math::cross(&S, &T);
		N = TFE_Math::normalize(&N);
		f32 d = -TFE_Math::dot(&N, &O);
		newFrustum->planes[newFrustum->planeCount++] = { N.x, N.y, N.z, d };
	}
		
	void frustum_buildFromCamera()
	{
		Mat4 view4;
		view4.m0 = { s_cameraMtx.m0.x, s_cameraMtx.m0.y, s_cameraMtx.m0.z, 0.0f };
		view4.m1 = { s_cameraMtx.m1.x, s_cameraMtx.m1.y, s_cameraMtx.m1.z, 0.0f };
		view4.m2 = { s_cameraMtx.m2.x, s_cameraMtx.m2.y, s_cameraMtx.m2.z, 0.0f };
		view4.m3 = { 0.0f,             0.0f,             0.0f,             1.0f };

		// Scale the projection slightly, so the frustum is slightly bigger than needed (i.e. something like a guard band).
		// This is done because exact frustum clipping isn't actually necessary for rendering, clipping is only used for portal testing.
		Mat4 proj = s_cameraProj;
		proj.m0.x *= c_superSidePlaneNormalScale;
		proj.m1.y *= c_superSidePlaneNormalScale;

		// Move the near plane to 0, which is allowed since clipping is done in world space.
		proj.m2.z = -1.0f;
		proj.m2.w = 0.0f;

		Mat4 viewProj = TFE_Math::transpose4(TFE_Math::mulMatrix4(proj, view4));
		Vec4f planes[] =
		{
			// Left
			{
				viewProj.m0.w + viewProj.m0.x,
				viewProj.m1.w + viewProj.m1.x,
				viewProj.m2.w + viewProj.m2.x,
				viewProj.m3.w + viewProj.m3.x
			},
			// Right
			{
				viewProj.m0.w - viewProj.m0.x,
				viewProj.m1.w - viewProj.m1.x,
				viewProj.m2.w - viewProj.m2.x,
				viewProj.m3.w - viewProj.m3.x
			},
			// Bottom
			{
				viewProj.m0.w + viewProj.m0.y,
				viewProj.m1.w + viewProj.m1.y,
				viewProj.m2.w + viewProj.m2.y,
				viewProj.m3.w + viewProj.m3.y
			},
			// Top
			{
				viewProj.m0.w - viewProj.m0.y,
				viewProj.m1.w - viewProj.m1.y,
				viewProj.m2.w - viewProj.m2.y,
				viewProj.m3.w - viewProj.m3.y
			},
			// Near
			{
				viewProj.m0.w + viewProj.m0.z,
				viewProj.m1.w + viewProj.m1.z,
				viewProj.m2.w + viewProj.m2.z,
				viewProj.m3.w + viewProj.m3.z
			},
		};

		// Move the camera back slightly...
		const f32 nearPlaneLen = sqrtf(planes[4].x*planes[4].x + planes[4].y*planes[4].y + planes[4].z*planes[4].z);
		f32 nearPlaneOffsetScale = c_superNearPlaneOffsetScale;
		if (nearPlaneLen > FLT_EPSILON)
		{
			nearPlaneOffsetScale /= nearPlaneLen;
		}
		Vec3f cameraPos = s_cameraPos;
		cameraPos.x -= planes[4].x * nearPlaneOffsetScale;
		cameraPos.y -= planes[4].y * nearPlaneOffsetScale;
		cameraPos.z -= planes[4].z * nearPlaneOffsetScale;

		Frustum frustum;
		frustum.planeCount = TFE_ARRAYSIZE(planes);
		for (u32 i = 0; i < frustum.planeCount; i++)
		{
			frustum.planes[i] = planes[i];

			f32 scale = 1.0f;
			if (i < frustum.planeCount - 1)
			{
				const f32 len = sqrtf(frustum.planes[i].x*frustum.planes[i].x + frustum.planes[i].y*frustum.planes[i].y + frustum.planes[i].z*frustum.planes[i].z);
				if (len > FLT_EPSILON)
				{
					scale = 1.0f / len;
				}
			}
			else if (nearPlaneLen > FLT_EPSILON)
			{
				scale = 1.0f / nearPlaneLen;
			}
			frustum.planes[i].x *= scale;
			frustum.planes[i].y *= scale;
			frustum.planes[i].z *= scale;
			frustum.planes[i].w *= scale;

			assert(frustum.planes[i].w == 0.0f);
			f32 d = -TFE_Math::dot((Vec3f*)frustum.planes[i].m, &cameraPos);
			frustum.planes[i].w = d;
		}
		frustum_push(frustum);
	}

	Vec4f frustum_calculatePlaneFromEdge(const Vec3f* edge)
	{
		Vec3f S = { edge[1].x - s_cameraPos.x, edge[1].y - s_cameraPos.y, edge[1].z - s_cameraPos.z };
		Vec3f T = { edge[0].x - s_cameraPos.x, edge[0].y - s_cameraPos.y, edge[0].z - s_cameraPos.z };
		Vec3f N = TFE_Math::cross(&S, &T);

		N = TFE_Math::normalize(&N);
		Vec4f plane = { N.x, N.y, N.z, -TFE_Math::dot(&N, &s_cameraPos) };

		return plane;
	}

	f32 frustum_planeDist(const Vec4f* plane, const Vec3f* pos)
	{
		return plane->x*pos->x + plane->y*pos->y + plane->z*pos->z + plane->w;
	}

	////////////////////////////////////////////////
	// Internal
	////////////////////////////////////////////////
	void frustum_createQuad(Vec3f corner0, Vec3f corner1, Polygon* poly)
	{
		for (s32 v = 0; v < 4; v++)
		{
			Vec3f pos;
			pos.x = (v == 0 || v == 3) ? corner0.x : corner1.x;
			pos.y = (v < 2) ? corner0.y : corner1.y;
			pos.z = (v == 0 || v == 3) ? corner0.z : corner1.z;

			poly->vtx[v] = pos;
		}
		poly->vertexCount = 4;
	}
}