#pragma once
//////////////////////////////////////////////////////////////////////
// GPU 2D antialiased line drawing
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/renderBackend.h>
#include "camera3d.h"

namespace TFE_RenderShared
{
	bool tri3d_init();
	void tri3d_destroy();

	// Quads converted to triangles.
	void triDraw3d_begin();
	void triDraw3d_addQuadColored(Vec3f* corners, const u32 color);
	void triDraw3d_addColored(u32 idxCount, u32 vtxCount, const Vec3f* vertices, const s32* indices, const u32 color, bool invSide);

	void triDraw3d_addQuadTextured(Vec3f* corners, const Vec2f* uvCorners, const u32 color, TextureGpu* texture);
	void triDraw3d_addTextured(u32 idxCount, u32 vtxCount, const Vec3f* vertices, const Vec2f* uv, const s32* indices, const u32 color, bool invSide, TextureGpu* texture);

	void triDraw3d_draw(const Camera3d* camera, f32 gridScale, f32 gridOpacity);
}
