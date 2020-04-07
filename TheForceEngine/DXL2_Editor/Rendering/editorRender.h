#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_RenderBackend/textureGpu.h>

struct Model;
struct EditorSector;

namespace DXL2_EditorRender
{
	bool init();
	void destroy();

	void drawModel2d(const EditorSector* sector, const Model* model, const Vec2f* pos, const Mat3* orientation, const u32* pal, u32 alpha);
	void drawModel2d_Bounds(const Model* model, const Vec3f* pos, const Mat3* orientation, u32 color, bool highlight);

	void drawModel3d(const EditorSector* sector, const Model* model, const Vec3f* pos, const Mat3* orientation, const u32* pal, u32 alpha);
	void drawModel3d_Bounds(const Model* model, const Vec3f* pos, const Mat3* orientation, f32 width, u32 color);

	TextureGpu* getFilterMap();
}
