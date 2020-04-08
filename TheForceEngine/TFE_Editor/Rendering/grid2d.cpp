#include "grid2d.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <vector>

namespace Grid2d
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

	bool init()
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

	void destroy()
	{
		s_shader.destroy();
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
	}

	void blitToScreen(s32 width, s32 height, f32 gridScale, f32 subGridSize, f32 pixelsToWorldUnits, f32 offsetX_WorldUnits, f32 offsetY_WorldUnits, f32 gridOpacity)
	{
		DisplayInfo display;
		TFE_RenderBackend::getDisplayInfo(&display);
		const f32 scaleX = f32(width) * pixelsToWorldUnits / gridScale;
		const f32 scaleY = f32(height) * pixelsToWorldUnits / gridScale;
		const f32 offsetX = offsetX_WorldUnits / gridScale;
		const f32 offsetY = offsetY_WorldUnits / gridScale;

		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST);
		TFE_RenderState::setStateEnable(true, STATE_BLEND);

		// Enable blending.
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };
		const f32 gridOpacitySubGrid[] = { gridOpacity, subGridSize };
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
