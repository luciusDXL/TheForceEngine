#pragma once
//////////////////////////////////////////////////////////////////////
// GPU 2D antialiased line drawing
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/renderBackend.h>
#include "camera3d.h"
#include "gridDef.h"

namespace TFE_RenderShared
{
	enum DrawMode
	{
		TRIMODE_OPAQUE = 0,
		TRIMODE_BLEND,
		TRIMODE_CLAMP,
		TRIMODE_COUNT
	};
	enum DrawFlags
	{
		TFLAG_NONE = 0,
		TFLAG_NO_GRID = FLAG_BIT(0),
		TFLAG_SKY = FLAG_BIT(1),
	};

	bool tri3d_init();
	void tri3d_destroy();

	// Quads converted to triangles.
	void triDraw3d_begin(const Grid* gridDef = nullptr);
	void triDraw3d_addQuadColored(DrawMode pass, Vec3f* corners, const u32 color);
	void triDraw3d_addColored(DrawMode pass, u32 idxCount, u32 vtxCount, const Vec3f* vertices, const s32* indices, const u32 color, bool invSide, bool showGrid = true);

	void triDraw3d_addQuadTextured(DrawMode pass, Vec3f* corners, const Vec2f* uvCorners, const u32 color, TextureGpu* texture, bool sky = false);
	void triDraw3d_addTextured(DrawMode pass, u32 idxCount, u32 vtxCount, const Vec3f* vertices, const Vec2f* uv, const s32* indices, const u32 color, bool invSide, TextureGpu* texture, bool showGrid = true, bool sky = false);

	void triDraw3d_draw(const Camera3d* camera, f32 width, f32 height, f32 gridScale, f32 gridOpacity, bool depthTest = true, bool culling = true);
}
