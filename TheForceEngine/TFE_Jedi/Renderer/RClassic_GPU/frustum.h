#pragma once
//////////////////////////////////////////////////////////////////////
// Frustum management
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

namespace TFE_Jedi
{
	enum FrustumConstants
	{
		FRUSTUM_STACK_SIZE = 256,
		FRUSTUM_PLANE_MAX  = 256,
	};

	struct Frustum
	{
		u32 planeCount;
		Vec4f planes[FRUSTUM_PLANE_MAX];
	};

	struct Polygon
	{
		s32 vertexCount;
		Vec3f vtx[FRUSTUM_PLANE_MAX];
	};

	void frustum_copy(const Frustum* src, Frustum* dst);
	void frustum_clearStack();
	void frustum_push(Frustum& frustum);

	Frustum* frustum_pop();
	Frustum* frustum_getBack();
	Frustum* frustum_getFront();
		
	// Returns true if the sphere defined by {pos, radius} is fully or partially inside of the current frustum on the stack.
	bool frustum_sphereInside(Vec3f pos, f32 radius);

	// Returns true if the quad is in front of the near plane.
	bool frustum_quadInside(const Vec3f v0, const Vec3f v1);

	// This clips the quad formed from {corner0, corner1} against the current frustum on the stack.
	// Returns false if the quad is outside of the frustum, else true.
	bool frustum_clipQuadToFrustum(Vec3f corner0, Vec3f corner1, Polygon* output, bool ignoreNearPlane = false);
	// Clip quad to list of planes.
	bool frustum_clipQuadToPlanes(s32 count, const Vec4f* plane, Vec3f corner0, Vec3f corner1, Polygon* output);

	// Builds a new frustum such that each polygon edge becomes a plane that passes through the camera and
	// the polygon plane defines the near plane.
	void frustum_buildFromPolygon(const Polygon* polygon, Frustum* newFrustum);

	// Builds a world-space frustum from the camera.
	void frustum_buildFromCamera();

	// Build a single plane from an edge and the camera position.
	Vec4f frustum_calculatePlaneFromEdge(const Vec3f* edge);

	// Distance of pos from plane.
	f32 frustum_planeDist(const Vec4f* plane, const Vec3f* pos);
}  // TFE_Jedi
