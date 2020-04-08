#include "blit.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <vector>

namespace BlitOpenGL
{
	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer s_indexBuffer;
	static s32 s_attribLocationScaleOffset = -1;

	struct BlitVert
	{
		f32 x, y;
		f32 u, v;
	};
	static const AttributeMapping c_blitAttrMapping[] =
	{
		{ATTR_POS,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 2, 0, false},
	};
	static const u32 c_blitAttrCount = TFE_ARRAYSIZE(c_blitAttrMapping);

	bool init()
	{
		if (!s_shader.load("Shaders/blit.vert", "Shaders/blit.frag"))
		{
			return false;
		}

		s_shader.bindTextureNameToSlot("VirtualDisplay", 0);
		s_attribLocationScaleOffset = s_shader.getVariableId("ScaleOffset");
		if (s_attribLocationScaleOffset < 0)
		{
			return false;
		}

		// Upload vertex/index buffers
		const BlitVert vertices[]=
		{
			{0.0f, 0.0f, 0.0f, 1.0f},
			{1.0f, 0.0f, 1.0f, 1.0f},
			{1.0f, 1.0f, 1.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
		};
		const u16 indices[]=
		{
			0, 1, 2,
			0, 2, 3
		};
		s_vertexBuffer.create(4, sizeof(BlitVert), c_blitAttrCount, c_blitAttrMapping, false, (void*)vertices);
		s_indexBuffer.create(6, sizeof(u16), false, (void*)indices);

		return true;
	}

	void destroy()
	{
		s_shader.destroy();
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
	}
				
	void blitToScreen(TextureGpu* texture, s32 x, s32 y, s32 width, s32 height)
	{
		DisplayInfo display;
		TFE_RenderBackend::getDisplayInfo(&display);
		const f32 scaleX = 2.0f * f32(width) / f32(display.width);
		const f32 scaleY = 2.0f * f32(height) / f32(display.height);
		const f32 offsetX = 2.0f * f32(x) / f32(display.width) - 1.0f;
		const f32 offsetY = 2.0f * f32(y) / f32(display.height) - 1.0f;
				
		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);
		
		s_shader.bind();
		// Bind Uniforms & Textures.
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };
		s_shader.setVariable(s_attribLocationScaleOffset, SVT_VEC4, scaleOffset);
		texture->bind();

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		// Draw.
		TFE_RenderBackend::drawIndexedTriangles(2, sizeof(u16));

		// Cleanup.
		s_shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();
		texture->clear();
	}
}
