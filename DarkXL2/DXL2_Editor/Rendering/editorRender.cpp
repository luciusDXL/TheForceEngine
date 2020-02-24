#include "grid2d.h"
#include <DXL2_System/system.h>
#include <DXL2_RenderBackend/renderBackend.h>
//Rendering 2d
#include <DXL2_Editor/Rendering/lineDraw2d.h>
#include <DXL2_Editor/Rendering/grid2d.h>
#include <DXL2_Editor/Rendering/trianglesColor2d.h>
#include <DXL2_Editor/Rendering/trianglesTextured2d.h>
//Rendering 3d
#include <DXL2_Editor/Rendering/grid3d.h>
#include <DXL2_Editor/Rendering/lineDraw3d.h>
#include <DXL2_Editor/Rendering/trianglesColor3d.h>

namespace DXL2_EditorRender
{
	static TextureGpu* s_filterMap = nullptr;

	bool init()
	{
		s_filterMap = DXL2_RenderBackend::createFilterTexture();

		// 2D
		LineDraw2d::init();
		Grid2d::init();
		TriColoredDraw2d::init();
		TriTexturedDraw2d::init();
		// 3D
		Grid3d::init();
		LineDraw3d::init();
		TrianglesColor3d::init();

		return s_filterMap != nullptr;
	}

	void destroy()
	{
		// delete s_filterMap;
		// 2D
		LineDraw2d::destroy();
		Grid2d::destroy();
		TriColoredDraw2d::destroy();
		TriTexturedDraw2d::destroy();
		// 3D
		Grid3d::destroy();
		LineDraw3d::destroy();
		TrianglesColor3d::destroy();
	}

	TextureGpu* getFilterMap()
	{
		return s_filterMap;
	}
}