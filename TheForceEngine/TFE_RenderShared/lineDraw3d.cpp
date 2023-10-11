#include "lineDraw3d.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/system.h>
#include <vector>
#include <algorithm>

#define LINE3D_MAX 65536

namespace TFE_RenderShared
{
	// Vertex Definition
	struct Line3dVertex
	{
		//Vec3f pos;
		Vec3f linePos0;
		Vec3f linePos1;
		Vec2f width;
		u32   color;
	};
	static const AttributeMapping c_line3dAttrMapping[]=
	{
		{ATTR_POS,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV1,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_line3dAttrCount = TFE_ARRAYSIZE(c_line3dAttrMapping);

	static s32 s_svCameraPos = -1;
	static s32 s_svCameraView = -1;
	static s32 s_svCameraProj = -1;
	static s32 s_svScreenSize = -1;

	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static Line3dVertex* s_vertices = nullptr;
	static u32 s_lineCount;

	static bool s_line3d_init = false;
	static Vec2f s_screenSize;
	static f32 s_pixelScale;
	   
	bool line3d_loadShader()
	{
		if (!s_shader.load("Shaders/line3d.vert", "Shaders/line3d.frag"))
		{
			return false;
		}
		s_svCameraPos  = s_shader.getVariableId("CameraPos");
		s_svCameraView = s_shader.getVariableId("CameraView");
		s_svCameraProj = s_shader.getVariableId("CameraProj");
		s_svScreenSize = s_shader.getVariableId("ScreenSize");
		if (s_svCameraPos < 0 || s_svCameraView < 0 || s_svCameraProj < 0 || s_svScreenSize < 0)
		{
			return false;
		}
		return true;
	}

	bool line3d_init()
	{
		if (s_line3d_init) { return true; }
		if (!line3d_loadShader())
		{
			return false;
		}
		s_line3d_init = true;

		// Create buffers
		// Create vertex and index buffers.
		u32* indices = new u32[6 * LINE3D_MAX];
		u32* outIndices = indices;
		for (u32 i = 0; i < LINE3D_MAX; i++, outIndices += 6)
		{
			outIndices[0] = i * 4 + 0;
			outIndices[1] = i * 4 + 1;
			outIndices[2] = i * 4 + 3;

			outIndices[3] = i * 4 + 0;
			outIndices[4] = i * 4 + 3;
			outIndices[5] = i * 4 + 2;
		}
		s_vertices = new Line3dVertex[4 * LINE3D_MAX];

		s_vertexBuffer.create(4 * LINE3D_MAX, sizeof(Line3dVertex), c_line3dAttrCount, c_line3dAttrMapping, true);
		s_indexBuffer.create(6 * LINE3D_MAX, sizeof(u32), false, indices);

		delete[] indices;
		s_lineCount = 0;

		return true;
	}

	void line3d_destroy()
	{
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
		delete[] s_vertices;
		s_vertices = nullptr;

		s_shader.destroy();
		s_line3d_init = false;
	}
	
	void lineDraw3d_begin(s32 width, s32 height)
	{
		s_screenSize = { f32(width), f32(height) };

		s_lineCount = 0;
		s_pixelScale = 1.0f / f32(std::min(width,height));
	}

	void lineDraw3d_addLines(u32 count, f32 width, const Vec3f* lines, const u32* lineColors)
	{
		Line3dVertex* vert = &s_vertices[s_lineCount * 4];
		f32 pixelWidth = width * s_pixelScale;
		for (u32 i = 0; i < count && s_lineCount < LINE3D_MAX; i++, lines += 2, lineColors++, vert += 4)
		{
			s_lineCount++;
		
			for (u32 v = 0; v < 4; v++)
			{
				vert[v].color = *lineColors;
				vert[v].width = { 1.5f/width, pixelWidth };
				vert[v].linePos0 = lines[0];
				vert[v].linePos1 = lines[1];
			}
		}
	}

	void lineDraw3d_addLine(f32 width, const Vec3f* vertices, const u32* colors)
	{
		lineDraw3d_addLines(1, width, vertices, colors);
	}
		
	void lineDraw3d_drawLines(const Camera3d* camera, bool depthTest, bool useBias)
	{
		if (s_lineCount < 1) { return; }
		s_vertexBuffer.update(s_vertices, s_lineCount * 4 * sizeof(Line3dVertex));
		TFE_RenderState::setDepthBias(0.0f, useBias ? -4.0f : 0.0f);

		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_WRITE);
		TFE_RenderState::setStateEnable(depthTest, STATE_DEPTH_TEST);
		TFE_RenderState::setDepthFunction(CMP_EQUAL);

		// Enable blending.
		TFE_RenderState::setStateEnable(true, STATE_BLEND);
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		s_shader.setVariable(s_svCameraPos, SVT_VEC3, camera->pos.m);
		s_shader.setVariable(s_svCameraView, SVT_MAT3x3, camera->viewMtx.data);
		s_shader.setVariable(s_svCameraProj, SVT_MAT4x4, camera->projMtx.data);
		s_shader.setVariable(s_svScreenSize, SVT_VEC2, s_screenSize.m);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		// Draw.
		TFE_RenderBackend::drawIndexedTriangles(s_lineCount * 2, sizeof(u32));

		// Cleanup.
		s_shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();
		TFE_RenderState::setDepthBias();

		// Clear
		s_lineCount = 0;
	}
}
