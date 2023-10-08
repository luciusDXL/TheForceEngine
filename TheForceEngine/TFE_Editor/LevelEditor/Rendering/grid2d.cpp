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

	void grid2d_blitToScreen(Vec2i viewportSize, f32 baseGridScale, f32 pixelsToWorldUnits, Vec3f viewPos, f32 gridOpacity)
	{
		DisplayInfo display;
		TFE_RenderBackend::getDisplayInfo(&display);
		const f32 gridScale = pixelsToWorldUnits / baseGridScale;
		const f32 scaleX = f32(viewportSize.x) * gridScale;
		const f32 scaleY = f32(viewportSize.z) * gridScale;
		const f32 offsetX = viewPos.x / baseGridScale;
		const f32 offsetY = viewPos.z / baseGridScale;

		f32 fadeMain = std::min(1.0f, std::max(0.0f, 1.0f - gridScale * 5.0f));
		f32 fadeSub  = std::min(1.0f, std::max(0.0f, 1.0f - gridScale * 5.0f / 8.0f));

		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST);
		TFE_RenderState::setStateEnable(true, STATE_BLEND);

		// Enable blending.
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };
		const f32 gridOpacitySubGrid[] = { gridOpacity * fadeMain, std::max(0.02f, gridOpacity * fadeSub) };
		s_shader.setVariable(s_svScaleOffset, SVT_VEC4, scaleOffset);
		s_shader.setVariable(s_svGridOpacity, SVT_VEC2, gridOpacitySubGrid);

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
