#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

class TextureGpu;

namespace TrianglesColor3d
{
	enum Tri3dTrans
	{
		TRANS_NONE = 0,		// Opaque
		TRANS_CUTOUT,		// Cutout transparent texels
		TRANS_CLAMP,		// Cutout transparent texels AND cutout any texels outside of the 0 - 1 range.
		TRANS_BLEND,		// Alpha blend.
		TRANS_BLEND_CLAMP,	// Cutout transparent texels AND cutout any texels outside of the 0 - 1 range and blend.
		TRANS_COUNT,
	};

	bool init();
	void destroy();

	void addTriangles(u32 count, const Vec3f* vtx, const Vec2f* uv, const u32* triColors, bool blend=false);
	void addTriangle(const Vec3f* vertices, const Vec2f* uv, u32 triColor, bool blend = false);

	void addTexturedTriangles(u32 count, const Vec3f* vtx, const Vec2f* uv, const Vec2f* uv1, const u32* triColors, const TextureGpu* texture, Tri3dTrans trans=Tri3dTrans::TRANS_NONE);
	void addTexturedTriangle(const Vec3f* vertices, const Vec2f* uv, const Vec2f* uv1, u32 triColor, const TextureGpu* texture, Tri3dTrans trans=Tri3dTrans::TRANS_NONE);

	void draw(const Vec3f* camPos, const Mat3* viewMtx, const Mat4* projMtx, bool depthTest = true);
}
