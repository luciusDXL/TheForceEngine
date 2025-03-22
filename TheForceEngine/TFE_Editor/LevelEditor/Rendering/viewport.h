#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_Editor/LevelEditor/sharedState.h>

namespace LevelEditor
{
	enum SectorDrawMode
	{
		SDM_WIREFRAME = 0,
		SDM_LIGHTING,
		SDM_TEXTURED_FLOOR,
		SDM_TEXTURED_CEIL,
		SDM_GROUP_COLOR,
		SDM_COUNT
	};

	enum ViewportRenderFlags
	{
		VRF_NONE = 0,
		VRF_CURVE_MOD = FLAG_BIT(0),
	};

	void viewport_init();
	void viewport_destroy();

	void viewport_render(EditorView view, u32 flags = VRF_NONE);
	const TextureGpu* viewport_getTexture();
	void viewport_update(s32 resvWidth, s32 resvHeight);
	void viewport_setNoteIcon3dImage(TextureGpu* image);

	void viewport_clearRail();
	void viewport_setRail(const Vec3f* rail, s32 dirCount = 1, Vec3f* moveDir = nullptr);

	// Compute the bounding planes for an object based on the viewport and object transform.
	void viewport_computeEntityBoundingPlanes(const EditorSector* sector, const EditorObject* obj, Vec4f* boundingPlanes);

	extern SectorDrawMode s_sectorDrawMode;
	extern Vec2i s_viewportSize;
	extern Vec3f s_viewportPos;
	extern Vec4f s_viewportTrans2d;
	extern f32 s_gridOpacity;
	extern f32 s_zoom2d;
	extern f32 s_viewDepth[];
}
