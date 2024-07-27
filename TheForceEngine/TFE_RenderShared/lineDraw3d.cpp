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

	struct Line3dShaderParam
	{
		Shader shader;
		s32 cameraPos = -1;
		s32 cameraView = -1;
		s32 cameraProj = -1;
		s32 screenSize = -1;
	};

	static Line3dShaderParam s_shaderParam[LINE_DRAW_COUNT];
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static Line3dVertex* s_vertices = nullptr;
	static LineDrawMode s_lineDrawMode3d = LINE_DRAW_SOLID;
	static u32 s_lineCount;

	static bool s_line3d_init = false;
	static Vec2f s_screenSize;
	static f32 s_pixelScale;
	   
	bool line3d_loadShaders()
	{
		if (!s_shaderParam[LINE_DRAW_SOLID].shader.load("Shaders/line3d.vert", "Shaders/line3d.frag"))
		{
			return false;
		}
		s_shaderParam[LINE_DRAW_SOLID].cameraPos  = s_shaderParam[LINE_DRAW_SOLID].shader.getVariableId("CameraPos");
		s_shaderParam[LINE_DRAW_SOLID].cameraView = s_shaderParam[LINE_DRAW_SOLID].shader.getVariableId("CameraView");
		s_shaderParam[LINE_DRAW_SOLID].cameraProj = s_shaderParam[LINE_DRAW_SOLID].shader.getVariableId("CameraProj");
		s_shaderParam[LINE_DRAW_SOLID].screenSize = s_shaderParam[LINE_DRAW_SOLID].shader.getVariableId("ScreenSize");
		if (s_shaderParam[LINE_DRAW_SOLID].cameraPos < 0 || s_shaderParam[LINE_DRAW_SOLID].cameraView < 0 ||
			s_shaderParam[LINE_DRAW_SOLID].cameraProj < 0 || s_shaderParam[LINE_DRAW_SOLID].screenSize < 0)
		{
			return false;
		}

		s32 defineCount = 1;
		ShaderDefine define = { "OPT_DASHED_LINE", "1" };
		if (!s_shaderParam[LINE_DRAW_DASHED].shader.load("Shaders/line3d.vert", "Shaders/line3d.frag", defineCount, &define))
		{
			return false;
		}
		s_shaderParam[LINE_DRAW_DASHED].cameraPos = s_shaderParam[LINE_DRAW_DASHED].shader.getVariableId("CameraPos");
		s_shaderParam[LINE_DRAW_DASHED].cameraView = s_shaderParam[LINE_DRAW_DASHED].shader.getVariableId("CameraView");
		s_shaderParam[LINE_DRAW_DASHED].cameraProj = s_shaderParam[LINE_DRAW_DASHED].shader.getVariableId("CameraProj");
		s_shaderParam[LINE_DRAW_DASHED].screenSize = s_shaderParam[LINE_DRAW_DASHED].shader.getVariableId("ScreenSize");
		if (s_shaderParam[LINE_DRAW_DASHED].cameraPos < 0 || s_shaderParam[LINE_DRAW_DASHED].cameraView < 0 ||
			s_shaderParam[LINE_DRAW_DASHED].cameraProj < 0 || s_shaderParam[LINE_DRAW_DASHED].screenSize < 0)
		{
			return false;
		}

		return true;
	}

	bool line3d_init()
	{
		if (s_line3d_init) { return true; }
		if (!line3d_loadShaders())
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

		for (s32 i = 0; i < LINE_DRAW_COUNT; i++)
		{
			s_shaderParam[i].shader.destroy();
		}
		s_line3d_init = false;
	}

	void lineDraw3d_setLineDrawMode(LineDrawMode mode/* = LINE_DRAW_SOLID*/)
	{
		s_lineDrawMode3d = mode;
	}
	
	void lineDraw3d_begin(s32 width, s32 height, LineDrawMode mode)
	{
		s_screenSize = { f32(width), f32(height) };

		s_lineCount = 0;
		s_lineDrawCount = 0;
		s_lineDrawMode3d = mode;
		s_pixelScale = 1.0f / f32(std::min(width,height));
	}

	void lineDraw3d_addLines(u32 count, f32 width, const Vec3f* lines, const u32* lineColors)
	{
		LineDraw* draw = s_lineDrawCount > 0 ? &s_lineDraw[s_lineDrawCount - 1] : nullptr;
		if (!draw || draw->mode != s_lineDrawMode3d)
		{
			if (s_lineDrawCount >= MAX_LINE_DRAW_CALLS)
			{
				return;
			}

			// New draw call.
			s_lineDraw[s_lineDrawCount].mode = s_lineDrawMode3d;
			s_lineDraw[s_lineDrawCount].count = count;
			s_lineDraw[s_lineDrawCount].offset = s_lineCount;
			s_lineDrawCount++;
		}
		else
		{
			draw->count += count;
		}

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
		TFE_RenderState::setDepthFunction(CMP_LEQUAL);

		// Enable blending.
		TFE_RenderState::setStateEnable(true, STATE_BLEND);
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		LineDraw* draw = s_lineDraw;
		for (s32 i = 0; i < s_lineDrawCount; i++, draw++)
		{
			Line3dShaderParam& shaderParam = s_shaderParam[draw->mode];
			shaderParam.shader.bind();
			// Bind Uniforms & Textures.
			shaderParam.shader.setVariable(shaderParam.cameraPos, SVT_VEC3, camera->pos.m);
			shaderParam.shader.setVariable(shaderParam.cameraView, SVT_MAT3x3, camera->viewMtx.data);
			shaderParam.shader.setVariable(shaderParam.cameraProj, SVT_MAT4x4, camera->projMtx.data);
			shaderParam.shader.setVariable(shaderParam.screenSize, SVT_VEC2, s_screenSize.m);

			// Draw.
			TFE_RenderBackend::drawIndexedTriangles(draw->count * 2, sizeof(u32), draw->offset * 6);
		}

		// Cleanup.
		Shader::unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();
		TFE_RenderState::setDepthBias();

		// Clear
		s_lineCount = 0;
		s_lineDrawCount = 0;
	}
}
