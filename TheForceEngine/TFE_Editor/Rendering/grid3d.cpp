#include "grid3d.h"
#include "editorRender.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <vector>

namespace Grid3d
{
	static const AttributeMapping c_gridAttrMapping[] =
	{
		{ATTR_POS, ATYPE_FLOAT, 3, 0, false},
	};
	static const u32 c_gridAttrCount = TFE_ARRAYSIZE(c_gridAttrMapping);

	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer s_indexBuffer;
	static s32 s_svCameraPos = -1;
	static s32 s_svCameraView = -1;
	static s32 s_svCameraProj = -1;
	static s32 s_svGridOpacity = -1;
	static s32 s_svGridHeight = -1;

	bool init()
	{
		if (!s_shader.load("Shaders/grid3d.vert", "Shaders/grid3d.frag"))
		{
			return false;
		}

		s_svCameraPos = s_shader.getVariableId("CameraPos");
		s_svCameraView = s_shader.getVariableId("CameraView");
		s_svCameraProj = s_shader.getVariableId("CameraProj");
		s_svGridOpacity = s_shader.getVariableId("GridOpacitySubGrid");
		s_svGridHeight = s_shader.getVariableId("GridHeight");
		s_shader.bindTextureNameToSlot("filterMap", 0);
		if (s_svCameraPos < 0 || s_svCameraView < 0 || s_svCameraProj < 0)
		{
			return false;
		}

		// Upload vertex/index buffers
		const Vec3f vertices[] =
		{
			{-10000.0f, 0.0f,  10000.0f},
			{ 10000.0f, 0.0f,  10000.0f},
			{ 10000.0f, 0.0f, -10000.0f},
			{-10000.0f, 0.0f, -10000.0f},
		};
		const u16 indices[] =
		{
			0, 1, 2,
			0, 2, 3
		};

		s_vertexBuffer.create(4, sizeof(Vec3f), c_gridAttrCount, c_gridAttrMapping, false, (void*)vertices);
		s_indexBuffer.create(6, sizeof(u16), false, (void*)indices);
		
		return true;
	}

	void destroy()
	{
		s_shader.destroy();
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
	}

	void draw(f32 gridScale, f32 height, f32 subGridSize, f32 gridOpacity, f32 pixelSize, const Vec3f* camPos, const Mat3* viewMtx, const Mat4* projMtx)
	{
		DisplayInfo display;
		TFE_RenderBackend::getDisplayInfo(&display);
		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_WRITE);
		TFE_RenderState::setStateEnable(true, STATE_BLEND | STATE_DEPTH_TEST);
		TFE_RenderState::setDepthBias();

		// Enable blending.
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		const f32 gridOpacitySubGrid[] = { gridOpacity*2.0f, subGridSize, pixelSize };
		s_shader.setVariable(s_svCameraPos, SVT_VEC3, camPos->m);
		s_shader.setVariable(s_svCameraView, SVT_MAT3x3, (f32*)viewMtx);
		s_shader.setVariable(s_svCameraProj, SVT_MAT4x4, (f32*)projMtx);
		s_shader.setVariable(s_svGridOpacity, SVT_VEC3, gridOpacitySubGrid);
		s_shader.setVariable(s_svGridHeight, SVT_SCALAR, &height);
		
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
