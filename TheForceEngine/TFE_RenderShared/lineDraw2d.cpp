#include "lineDraw2d.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/system.h>
#include <vector>

#define MAX_LINES 65536

namespace TFE_RenderShared
{
	// Vertex Definition
	struct LineVertex
	{
		Vec3f pos;		// 2D position + width.
		Vec4f uv;		// Line relative position in pixels.
		u32   color;	// Line color + opacity.
	};
	struct ShaderSettings
	{
		bool bloom = false;
	};
	static const AttributeMapping c_lineAttrMapping[]=
	{
		{ATTR_POS,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 4, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_lineAttrCount = TFE_ARRAYSIZE(c_lineAttrMapping);

	struct Line2dShaderParam
	{
		Shader shader;
		s32 scaleOffset = -1;
	};
	static Line2dShaderParam s_shaderParam[LINE_DRAW_COUNT];
		
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static LineVertex* s_vertices = nullptr;
	static u32 s_lineCount;

	static ShaderSettings s_shaderSettings = {};
	static bool s_allowBloom = true;
	static bool s_line2d_init = false;
	static LineDrawMode s_lineDrawMode2d = LINE_DRAW_SOLID;

	static u32 s_width, s_height;
	   
	bool loadShaders()
	{
		ShaderDefine defines[2] = {};
		u32 defineCount = 0;
		if (s_shaderSettings.bloom)
		{
			defines[0].name = "OPT_BLOOM";
			defines[0].value = "1";
			defineCount++;
		}
		
		if (!s_shaderParam[LINE_DRAW_SOLID].shader.load("Shaders/line2d.vert", "Shaders/line2d.frag", defineCount, defines, SHADER_VER_STD))
		{
			return false;
		}
		s_shaderParam[LINE_DRAW_SOLID].scaleOffset = s_shaderParam[LINE_DRAW_SOLID].shader.getVariableId("ScaleOffset");
		if (s_shaderParam[LINE_DRAW_SOLID].scaleOffset < 0)
		{
			return false;
		}

		// Dashed lines.
		defines[defineCount].name = "OPT_DASHED_LINE";
		defines[defineCount].value = "1";
		defineCount++;
		if (!s_shaderParam[LINE_DRAW_DASHED].shader.load("Shaders/line2d.vert", "Shaders/line2d.frag", defineCount, defines, SHADER_VER_STD))
		{
			return false;
		}
		s_shaderParam[LINE_DRAW_DASHED].scaleOffset = s_shaderParam[LINE_DRAW_DASHED].shader.getVariableId("ScaleOffset");
		if (s_shaderParam[LINE_DRAW_DASHED].scaleOffset < 0)
		{
			return false;
		}

		return true;
	}

	bool init(bool allowBloom)
	{
		s_allowBloom = allowBloom;
		if (s_line2d_init) { return true; }

		s_shaderSettings.bloom = s_allowBloom && TFE_Settings::getGraphicsSettings()->bloomEnabled;
		if (!loadShaders())
		{
			return false;
		}
		s_line2d_init = true;

		// Create buffers
		// Create vertex and index buffers.
		u32* indices = new u32[6 * MAX_LINES];
		u32* outIndices = indices;
		for (u32 i = 0; i < MAX_LINES; i++, outIndices += 6)
		{
			outIndices[0] = i * 4 + 0;
			outIndices[1] = i * 4 + 1;
			outIndices[2] = i * 4 + 2;

			outIndices[3] = i * 4 + 0;
			outIndices[4] = i * 4 + 2;
			outIndices[5] = i * 4 + 3;
		}
		s_vertices = new LineVertex[4 * MAX_LINES];

		s_vertexBuffer.create(4 * MAX_LINES, sizeof(LineVertex), c_lineAttrCount, c_lineAttrMapping, true);
		s_indexBuffer.create(6 * MAX_LINES, sizeof(u32), false, indices);

		delete[] indices;
		s_lineCount = 0;

		return true;
	}

	void destroy()
	{
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
		delete[] s_vertices;
		s_vertices = nullptr;

		for (s32 i = 0; i < LINE_DRAW_COUNT; i++)
		{
			s_shaderParam[i].shader.destroy();
		}
		s_shaderSettings = {};

		s_line2d_init = false;
	}

	void lineDraw2d_setLineDrawMode(LineDrawMode mode/* = LINE_DRAW_SOLID*/)
	{
		s_lineDrawMode2d = mode;
	}
	
	void lineDraw2d_begin(u32 width, u32 height)
	{
		s_width = width;
		s_height = height;
		s_lineCount = 0;
	}

	void lineDraw2d_addLines(u32 count, f32 width, const Vec2f* lines, const u32* lineColors)
	{
		LineVertex* vert = &s_vertices[s_lineCount * 4];
		for (u32 i = 0; i < count && s_lineCount < MAX_LINES; i++, lines += 2, lineColors += 2, vert += 4)
		{
			s_lineCount++;

			// Convert to pixels
			f32 x0 = lines[0].x;
			f32 x1 = lines[1].x;

			f32 y0 = lines[0].z;
			f32 y1 = lines[1].z;

			f32 dx = x1 - x0;
			f32 dy = y1 - y0;
			const f32 offset = width * 2.0f;
			f32 scale = offset / sqrtf(dx*dx + dy * dy);
			dx *= scale;
			dy *= scale;

			f32 nx =  dy;
			f32 ny = -dx;
			
			// Build a quad.
			f32 lx0 = x0 - dx;
			f32 lx1 = x1 + dx;
			f32 ly0 = y0 - dy;
			f32 ly1 = y1 + dy;

			vert[0].pos.x = lx0 + nx;
			vert[1].pos.x = lx1 + nx;
			vert[2].pos.x = lx1 - nx;
			vert[3].pos.x = lx0 - nx;

			vert[0].pos.y = ly0 + ny;
			vert[1].pos.y = ly1 + ny;
			vert[2].pos.y = ly1 - ny;
			vert[3].pos.y = ly0 - ny;

			vert[0].pos.z = width;
			vert[1].pos.z = width;
			vert[2].pos.z = width;
			vert[3].pos.z = width;

			// FIX with PLANE offsets (pos - (x0, y0)).DIR or (pos - (x0, y0)).NRM
			// Calculate the offset from the line.
			vert[0].uv.x = x0; vert[0].uv.z = x1;
			vert[1].uv.x = x0; vert[1].uv.z = x1;
			vert[2].uv.x = x0; vert[2].uv.z = x1;
			vert[3].uv.x = x0; vert[3].uv.z = x1;

			vert[0].uv.y = y0; vert[0].uv.w = y1;
			vert[1].uv.y = y0; vert[1].uv.w = y1;
			vert[2].uv.y = y0; vert[2].uv.w = y1;
			vert[3].uv.y = y0; vert[3].uv.w = y1;

			// Copy the colors.
			vert[0].color = lineColors[0];
			vert[1].color = lineColors[1];
			vert[2].color = lineColors[1];
			vert[3].color = lineColors[0];
		}
	}

	void lineDraw2d_addLine(f32 width, const Vec2f* vertices, const u32* colors)
	{
		lineDraw2d_addLines(1, width, vertices, colors);
	}

	void lineDraw2d_drawLines()
	{
		if (s_lineCount < 1) { return; }
		// First see if the shader needs to be updated.
		bool bloomEnabled = s_allowBloom && TFE_Settings::getGraphicsSettings()->bloomEnabled;
		if (bloomEnabled != s_shaderSettings.bloom)
		{
			s_shaderSettings.bloom = bloomEnabled;
			loadShaders();
		}

		s_vertexBuffer.update(s_vertices, s_lineCount * 4 * sizeof(LineVertex));

		const f32 scaleX = 2.0f / f32(s_width);
		const f32 scaleY = 2.0f / f32(s_height);
		const f32 offsetX = -1.0f;
		const f32 offsetY = -1.0f;

		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST | STATE_DEPTH_WRITE);

		// Enable blending.
		TFE_RenderState::setStateEnable(true, STATE_BLEND);
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		Line2dShaderParam& shaderParam = s_shaderParam[s_lineDrawMode2d];
		shaderParam.shader.bind();
		// Bind Uniforms & Textures.
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };
		shaderParam.shader.setVariable(shaderParam.scaleOffset, SVT_VEC4, scaleOffset);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		u32 indexSizeType = s_indexBuffer.bind();

		// Draw.
		TFE_RenderBackend::drawIndexedTriangles(s_lineCount * 2, sizeof(u32));

		// Cleanup.
		shaderParam.shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();

		// Clear
		s_lineCount = 0;
	}
}
