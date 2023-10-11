#pragma once
//////////////////////////////////////////////////////////////////////
// GPU 2D antialiased line drawing
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/renderBackend.h>

namespace TFE_RenderShared
{
	bool tri2d_init();
	void tri2d_destroy();

	void triDraw2d_begin(u32 width, u32 height);
	void triDraw2d_addColored(u32 idxCount, u32 vtxCount, const Vec2f* vertices, const s32* indices, const u32 color);
	void triDraw2D_addTextured(u32 idxCount, u32 vtxCount, const Vec2f* vertices, const Vec2f* uv, const s32* indices, const u32 color, TextureGpu* texture);
	void triDraw2d_draw();
}
