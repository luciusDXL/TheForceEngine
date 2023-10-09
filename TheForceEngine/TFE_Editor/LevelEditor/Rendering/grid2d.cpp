#include "grid2d.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <algorithm>
#include <vector>

namespace LevelEditor
{
	static const AttributeMapping c_gridAttrMapping[] =
	{
		{ATTR_POS, ATYPE_FLOAT, 2, 0, false},
	};
	static const u32 c_gridAttrCount = TFE_ARRAYSIZE(c_gridAttrMapping);

	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer s_indexBuffer;
	static s32 s_svScaleOffset = -1;
	static s32 s_svGridOpacity = -1;

	bool grid2d_init()
	{
		if (!s_shader.load("Shaders/grid2d.vert", "Shaders/grid2d.frag"))
		{
			return false;
		}

		s_svScaleOffset = s_shader.getVariableId("ScaleOffset");
		s_svGridOpacity = s_shader.getVariableId("GridOpacitySubGrid");
		if (s_svScaleOffset < 0)
		{
			return false;
		}

		// Upload vertex/index buffers
		const Vec2f vertices[] =
		{
			{0.0f, 0.0f},
			{1.0f, 0.0f},
			{1.0f, 1.0f},
			{0.0f, 1.0f},
		};
		const u16 indices[] =
		{
			0, 1, 2,
			0, 2, 3
		};

		s_vertexBuffer.create(4, sizeof(Vec2f), c_gridAttrCount, c_gridAttrMapping, false, (void*)vertices);
		s_indexBuffer.create(6, sizeof(u16), false, (void*)indices);

		return true;
	}

	void grid2d_destroy()
	{
		s_shader.destroy();
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
	}

	struct GridScale
	{
		f32 gridScale;
		f32 baseGridScale;	// the size of the base *visible* grid (may be larger than the actual base grid).
		f32 snapGridScale;	// the grid scale to use for snapping.
		Vec2f scale;
		Vec2f offset;
	};
	static GridScale s_gridScale;
		
	void grid2d_computeScale(Vec2i viewportSize, f32 baseGridScale, f32 pixelsToWorldUnits, Vec3f viewPos)
	{
		// Finest grid.
		f32 gridScale = pixelsToWorldUnits / baseGridScale;

		// Zoom out until the finest grid is visible.
		const f32 eps = 0.01f;
		f32 fadeMain = std::min(1.0f, std::max(0.0f, 1.0f - gridScale * 5.0f));
		// Given the zoom limits, the maximum number of loop iterations is 4.
		while (fadeMain < eps)
		{
			baseGridScale *= 8.0f;
			gridScale = pixelsToWorldUnits / baseGridScale;
			fadeMain = std::min(1.0f, std::max(0.0f, 1.0f - gridScale * 5.0f));
		}
		// Adjust the grid scale.
		s_gridScale.gridScale = gridScale;
		s_gridScale.baseGridScale = baseGridScale;
		s_gridScale.snapGridScale = baseGridScale;
		s_gridScale.scale.x = f32(viewportSize.x) * gridScale;
		s_gridScale.scale.z = f32(viewportSize.z) * gridScale;
		s_gridScale.offset.x = viewPos.x / baseGridScale;
		s_gridScale.offset.z = viewPos.z / baseGridScale;
	}

	f32 grid2d_getGrid(s32 layer)
	{
		const f32 layerScale[] = { 1.0f, 8.0f, 64.0f };
		layer = std::max(0, std::min(2, layer));

		return s_gridScale.baseGridScale * layerScale[layer];
	}

	void grid2d_snap(const Vec2f& worldPos, s32 layer, Vec2f& snappedPos)
	{
		const f32 layerScale[] = { 1.0f, 8.0f, 64.0f };
		layer = std::max(0, std::min(2, layer));

		const f32 grid = s_gridScale.baseGridScale * layerScale[layer];
		snappedPos.x = floorf(worldPos.x / grid) * grid;
		snappedPos.z = floorf(worldPos.z / grid) * grid;
	}

	void grid2d_blitToScreen(f32 gridOpacity)
	{
		f32 fadeMain = std::min(1.0f, std::max(0.0f, 1.0f - s_gridScale.gridScale * 5.0f));
		f32 fadeSub  = std::min(1.0f, std::max(0.0f, 1.0f - s_gridScale.gridScale * 5.0f / 8.0f));
		f32 fadeSub2 = std::min(1.0f, std::max(0.0f, 1.0f - s_gridScale.gridScale * 5.0f / 64.0f));

		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST);
		TFE_RenderState::setStateEnable(true, STATE_BLEND);

		// Enable blending.
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		const f32 scaleOffset[] = { s_gridScale.scale.x, s_gridScale.scale.z, s_gridScale.offset.x, s_gridScale.offset.z };
		const f32 gridOpacitySubGrid[] = { gridOpacity * fadeMain, gridOpacity * fadeSub, std::max(0.02f, gridOpacity * fadeSub2) };
		s_shader.setVariable(s_svScaleOffset, SVT_VEC4, scaleOffset);
		s_shader.setVariable(s_svGridOpacity, SVT_VEC3, gridOpacitySubGrid);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		// Draw.
		TFE_RenderBackend::drawIndexedTriangles(2, sizeof(u16));

		// Cleanup.
		s_shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();
	}
}
