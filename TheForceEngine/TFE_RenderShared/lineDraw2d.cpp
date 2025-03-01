#include "lineDraw2d.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/system.h>
#include <vector>
#include <algorithm>

#define MAX_LINES 65536
#define MAX_CURVES 4096

namespace TFE_RenderShared
{
	// Vertex Definition
	struct LineVertex
	{
		Vec4f pos;		// 2D position + width + pad -or- 2D pos + 2D center
		Vec4f uv;		// Line relative position in pixels -or- 2D a, b control points.
		Vec4f uv1;		// Curve offsets.
		u32   color;	// Line color + opacity.
	};
	struct ShaderSettings
	{
		bool bloom = false;
	};
	static const AttributeMapping c_lineAttrMapping[]=
	{
		{ATTR_POS,   ATYPE_FLOAT, 4, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 4, 0, false},
	    {ATTR_UV1,   ATYPE_FLOAT, 4, 0, false},
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
	static VertexBuffer s_curveVertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static LineVertex* s_vertices = nullptr;
	static LineVertex* s_curveVertices = nullptr;
	static u32 s_lineCount2d;
	static u32 s_curveCount2d;

	static ShaderSettings s_shaderSettings = {};
	static bool s_allowBloom = true;
	static bool s_line2d_init = false;
	static LineDrawMode s_lineDrawMode2d = LINE_DRAW_SOLID;

	static u32 s_width, s_height;
	   
	bool loadShaders()
	{
		ShaderDefine defines[3] = {};
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

		// Curve + Dashed Lines
		defines[defineCount].name = "OPT_CURVE";
		defines[defineCount].value = "1";
		defineCount++;
		if (!s_shaderParam[LINE_DRAW_CURVE_DASHED].shader.load("Shaders/line2d.vert", "Shaders/line2d.frag", defineCount, defines, SHADER_VER_STD))
		{
			return false;
		}
		s_shaderParam[LINE_DRAW_CURVE_DASHED].scaleOffset = s_shaderParam[LINE_DRAW_CURVE_DASHED].shader.getVariableId("ScaleOffset");
		if (s_shaderParam[LINE_DRAW_CURVE_DASHED].scaleOffset < 0)
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
		s_curveVertices = new LineVertex[4 * MAX_CURVES];

		s_vertexBuffer.create(4 * MAX_LINES, sizeof(LineVertex), c_lineAttrCount, c_lineAttrMapping, true);
		s_curveVertexBuffer.create(4 * MAX_CURVES, sizeof(LineVertex), c_lineAttrCount, c_lineAttrMapping, true);
		s_indexBuffer.create(6 * MAX_LINES, sizeof(u32), false, indices);

		delete[] indices;
		s_lineCount2d = 0;

		return true;
	}

	void destroy()
	{
		s_vertexBuffer.destroy();
		s_curveVertexBuffer.destroy();
		s_indexBuffer.destroy();
		delete[] s_vertices;
		delete[] s_curveVertices;
		s_vertices = nullptr;
		s_curveVertices = nullptr;

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
	
	void lineDraw2d_begin(u32 width, u32 height, LineDrawMode mode/* = LINE_DRAW_SOLID*/)
	{
		s_width = width;
		s_height = height;
		s_lineCount2d = 0;
		s_lineDrawCount = 0;
		s_lineDrawMode2d = mode;
	}

	void lineDraw2d_addCurve(const Vec2f* vertices, const u32 color, u32 offsetCount, const f32* offsets)
	{
		if (s_curveCount2d >= MAX_CURVES)
		{
			return;
		}

		LineVertex* vert = &s_curveVertices[s_curveCount2d * 4];
		s_curveCount2d++;
		// bounding box.
		f32 maxOffset = 0.0f;
		for (u32 i = 0; i < offsetCount; i++)
		{
			maxOffset = std::max(maxOffset, fabsf(offsets[i]));
		}
		const f32 padding = 2.0f + maxOffset;

		Vec2f boundsMin = vertices[0];
		Vec2f boundsMax = vertices[0];
		for (s32 i = 1; i < 3; i++)
		{
			boundsMin.x = std::min(boundsMin.x, vertices[i].x);
			boundsMin.z = std::min(boundsMin.z, vertices[i].z);
			boundsMax.x = std::max(boundsMax.x, vertices[i].x);
			boundsMax.z = std::max(boundsMax.z, vertices[i].z);
		}

		// bounding box.
		vert[0].pos = { boundsMin.x - padding, boundsMin.z - padding, vertices[2].x, vertices[2].z };
		vert[1].pos = { boundsMax.x + padding, boundsMin.z - padding, vertices[2].x, vertices[2].z };
		vert[2].pos = { boundsMax.x + padding, boundsMax.z + padding, vertices[2].x, vertices[2].z };
		vert[3].pos = { boundsMin.x - padding, boundsMax.z + padding, vertices[2].x, vertices[2].z };

		// Offsets
		Vec4f offsetVec = { 0 };
		for (u32 i = 0; i < offsetCount && i < 4; i++)
		{
			offsetVec.m[i] = offsets[i];
		}

		// store constant data in vertices to avoid draw calls.
		for (s32 i = 0; i < 4; i++)
		{
			vert[i].uv.x  = vertices[0].x;
			vert[i].uv.y  = vertices[0].z;
			vert[i].uv.z  = vertices[1].x;
			vert[i].uv.w  = vertices[1].z;
			vert[i].uv1   = offsetVec;
			vert[i].color = color;
		}
	}	

	void lineDraw2d_addLines(u32 count, f32 width, const Vec2f* lines, const u32* lineColors)
	{
		if (!s_vertices)
		{
			return;
		}

		LineDraw* draw = s_lineDrawCount > 0 ? &s_lineDraw[s_lineDrawCount - 1] : nullptr;
		if (!draw || draw->mode != s_lineDrawMode2d)
		{
			if (s_lineDrawCount >= MAX_LINE_DRAW_CALLS)
			{
				return;
			}

			// New draw call.
			s_lineDraw[s_lineDrawCount].mode = s_lineDrawMode2d;
			s_lineDraw[s_lineDrawCount].count = count;
			s_lineDraw[s_lineDrawCount].offset = s_lineCount2d;
			s_lineDrawCount++;
		}
		else
		{
			draw->count += count;
		}

		LineVertex* vert = &s_vertices[s_lineCount2d * 4];
		for (u32 i = 0; i < count && s_lineCount2d < MAX_LINES; i++, lines += 2, lineColors += 2, vert += 4)
		{
			s_lineCount2d++;

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

			vert[0].uv1 = { 0 };
			vert[1].uv1 = { 0 };
			vert[2].uv1 = { 0 };
			vert[3].uv1 = { 0 };

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
		if (s_lineCount2d < 1 && s_curveCount2d < 1) { return; }
		// First see if the shader needs to be updated.
		bool bloomEnabled = s_allowBloom && TFE_Settings::getGraphicsSettings()->bloomEnabled;
		if (bloomEnabled != s_shaderSettings.bloom)
		{
			s_shaderSettings.bloom = bloomEnabled;
			loadShaders();
		}

		if (s_lineCount2d)
		{
			s_vertexBuffer.update(s_vertices, s_lineCount2d * 4 * sizeof(LineVertex));
		}
		if (s_curveCount2d)
		{
			s_curveVertexBuffer.update(s_curveVertices, s_curveCount2d * 4 * sizeof(LineVertex));
		}

		const f32 scaleX = 2.0f / f32(s_width);
		const f32 scaleY = 2.0f / f32(s_height);
		const f32 offsetX = -1.0f;
		const f32 offsetY = -1.0f;
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };

		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST | STATE_DEPTH_WRITE);

		// Enable blending.
		TFE_RenderState::setStateEnable(true, STATE_BLEND);
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		// Bind vertex and index buffers.
		u32 indexSizeType = s_indexBuffer.bind();
		if (s_lineCount2d > 0)
		{
			s_vertexBuffer.bind();

			LineDraw* draw = s_lineDraw;
			for (s32 i = 0; i < s_lineDrawCount; i++, draw++)
			{
				Line2dShaderParam& shaderParam = s_shaderParam[draw->mode];
				shaderParam.shader.bind();
				shaderParam.shader.setVariable(shaderParam.scaleOffset, SVT_VEC4, scaleOffset);
				// Draw.
				TFE_RenderBackend::drawIndexedTriangles(draw->count * 2, sizeof(u32), draw->offset * 6);
			}
			s_vertexBuffer.unbind();
		}
		if (s_curveCount2d)
		{
			s_curveVertexBuffer.bind();

			Line2dShaderParam& shaderParam = s_shaderParam[LINE_DRAW_CURVE_DASHED];
			shaderParam.shader.bind();
			shaderParam.shader.setVariable(shaderParam.scaleOffset, SVT_VEC4, scaleOffset);
			// Draw.
			TFE_RenderBackend::drawIndexedTriangles(s_curveCount2d * 2, sizeof(u32));
		}

		// Cleanup.
		Shader::unbind();
		s_indexBuffer.unbind();

		// Clear
		s_lineCount2d = 0;
		s_curveCount2d = 0;
		s_lineDrawCount = 0;
	}
}
