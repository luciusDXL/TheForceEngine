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
	struct Frustum
	{
		u32 planeCount;
		Vec4f planes[256];
	};

	struct Polygon
	{
		s32 vertexCount;
		Vec3f vtx[256];
	};

	void frustum_copy(const Frustum* src, Frustum* dst);
	void frustum_clearStack();
	void frustum_push(Frustum& frustum);

	Frustum* frustum_pop();
	Frustum* frustum_getBack();
	Frustum* frustum_getFront();
		
	// Returns true if the sphere defined by {pos, radius} is fully or partially inside of the current frustum on the stack.
	bool frustum_sphereInside(Vec3f pos, f32 radius);

	// This clips the quad formed from {corner0, corner1} against the current frustum on the stack.
	// Returns false if the quad is outside of the frustum, else true.
	bool frustum_clipQuadToFrustum(Vec3f corner0, Vec3f corner1, Polygon* output);

	// Builds a new frustum such that each polygon edge becomes a plane that passes through the camera and
	// the polygon plane defines the near plane.
	void frustum_buildFromPolygon(const Polygon* polygon, Frustum* newFrustum);

	// Builds a world-space frustum from the camera.
	void frustum_buildFromCamera();
}  // TFE_Jedi
