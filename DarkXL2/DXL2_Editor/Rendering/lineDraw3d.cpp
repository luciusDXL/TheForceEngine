#include "lineDraw2d.h"
#include <DXL2_RenderBackend/renderBackend.h>
#include <DXL2_RenderBackend/shader.h>
#include <DXL2_RenderBackend/vertexBuffer.h>
#include <DXL2_RenderBackend/indexBuffer.h>
#include <DXL2_System/system.h>
#include <vector>

#define LINE_MAX 65536

namespace LineDraw3d
{
	// Vertex Definition
	struct LineVertex
	{
		Vec3f pos;		
		Vec3f linePos0;	
		Vec3f linePos1;	
		Vec3f dirWidth;	
		u32   color;	
	};
	static const AttributeMapping c_lineAttrMapping[]=
	{
		{ATTR_POS,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV1,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV2,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_lineAttrCount = DXL2_ARRAYSIZE(c_lineAttrMapping);

	static s32 s_svCameraPos = -1;
	static s32 s_svCameraView = -1;
	static s32 s_svCameraProj = -1;

	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static LineVertex* s_vertices;
	static u32 s_lineCount;

	bool init()
	{
		if (!s_shader.load("Shaders/line3d.vert", "Shaders/line3d.frag"))
		{
			return false;
		}

		s_svCameraPos = s_shader.getVariableId("CameraPos");
		s_svCameraView = s_shader.getVariableId("CameraView");
		s_svCameraProj = s_shader.getVariableId("CameraProj");
		if (s_svCameraPos < 0 || s_svCameraView < 0 || s_svCameraProj < 0)
		{
			return false;
		}

		// Create buffers
		// Create vertex and index buffers.
		u32* indices = new u32[6 * LINE_MAX];
		u32* outIndices = indices;
		for (u32 i = 0; i < LINE_MAX; i++, outIndices += 6)
		{
			outIndices[0] = i * 4 + 0;
			outIndices[1] = i * 4 + 1;
			outIndices[2] = i * 4 + 2;

			outIndices[3] = i * 4 + 0;
			outIndices[4] = i * 4 + 2;
			outIndices[5] = i * 4 + 3;
		}
		s_vertices = new LineVertex[4 * LINE_MAX];

		s_vertexBuffer.create(4 * LINE_MAX, sizeof(LineVertex), c_lineAttrCount, c_lineAttrMapping, true);
		s_indexBuffer.create(6 * LINE_MAX, sizeof(u32), false, indices);

		delete[] indices;
		s_lineCount = 0;

		return true;
	}

	void destroy()
	{
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
		delete[] s_vertices;
	}
	
	void addLines(u32 count, f32 width, const Vec3f* lines, const u32* lineColors)
	{
		LineVertex* vert = &s_vertices[s_lineCount * 4];
		for (u32 i = 0; i < count && s_lineCount < LINE_MAX; i++, lines += 2, lineColors++, vert += 4)
		{
			s_lineCount++;

			// Build vertices - the positions will be updated in the shader.
			vert[0].pos = lines[0];
			vert[1].pos = lines[1];
			vert[2].pos = lines[1];
			vert[3].pos = lines[0];

			vert[0].dirWidth = { -1,  1, width };
			vert[1].dirWidth = {  1,  1, width };
			vert[2].dirWidth = {  1, -1, width };
			vert[3].dirWidth = { -1, -1, width };

			for (u32 v = 0; v < 4; v++)
			{
				vert[v].color = *lineColors;
				vert[v].linePos0 = lines[0];
				vert[v].linePos1 = lines[1];
			}
		}
	}

	void addLine(f32 width, const Vec3f* vertices, const u32* colors)
	{
		addLines(1, width, vertices, colors);
	}

	void draw(const Vec3f* camPos, const Mat3* viewMtx, const Mat4* projMtx, bool depthTest, bool useBias)
	{
		if (s_lineCount < 1) { return; }

		s_vertexBuffer.update(s_vertices, s_lineCount * 4 * sizeof(LineVertex));
		DXL2_RenderState::setDepthBias(0.0f, useBias ? -4.0f : 0.0f);

		DXL2_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_WRITE);
		DXL2_RenderState::setStateEnable(depthTest, STATE_DEPTH_TEST);
		DXL2_RenderState::setDepthFunction(CMP_LEQUAL);

		// Enable blending.
		DXL2_RenderState::setStateEnable(true, STATE_BLEND);
		DXL2_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		s_shader.setVariable(s_svCameraPos,  SVT_VEC3,   camPos->m);
		s_shader.setVariable(s_svCameraView, SVT_MAT3x3, (f32*)viewMtx);
		s_shader.setVariable(s_svCameraProj, SVT_MAT4x4, (f32*)projMtx);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		// Draw.
		DXL2_RenderBackend::drawIndexedTriangles(s_lineCount * 2, sizeof(u32));

		// Cleanup.
		s_shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();

		DXL2_RenderState::setDepthBias();

		// Clear
		s_lineCount = 0;
	}
}
