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

namespace LevelEditor
{
	enum EditorView
	{
		EDIT_VIEW_2D = 0,
		EDIT_VIEW_3D,
		EDIT_VIEW_3D_GAME,
		EDIT_VIEW_PLAY,
	};
	enum SectorDrawMode
	{
		SDM_WIREFRAME = 0,
		SDM_LIGHTING,
		SDM_TEXTURED_FLOOR,
		SDM_TEXTURED_CEIL,
		SDM_COUNT
	};

	void viewport_init();
	void viewport_destroy();

	void viewport_render(EditorView view);
	const TextureGpu* viewport_getTexture();
	void viewport_update(s32 resvWidth, s32 resvHeight);

	extern SectorDrawMode s_sectorDrawMode;
	extern Vec2i s_viewportSize;
	extern Vec3f s_viewportPos;
	extern Vec4f s_viewportTrans2d;
	extern f32 s_gridOpacity;
	extern f32 s_gridSize2d;
	extern f32 s_zoom2d;
	extern f32 s_gridHeight;
}
