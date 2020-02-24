#include "trianglesColor2d.h"
#include <DXL2_RenderBackend/shader.h>
#include <DXL2_RenderBackend/vertexBuffer.h>
#include <DXL2_RenderBackend/indexBuffer.h>
#include <DXL2_System/system.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <vector>

#define TRIANGLE_MAX 65536

namespace TriColoredDraw2d
{
	struct TriangleVertex
	{
		Vec2f pos;	// 2D position.
		u32  color;	// vertex color + opacity.
	};
	static const AttributeMapping c_triAttrMapping[] =
	{
		{ATTR_POS,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_triAttrCount = DXL2_ARRAYSIZE(c_triAttrMapping);

	static s32 s_svScaleOffset = -1;
	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer s_indexBuffer;

	static TriangleVertex* s_vertices;
	static u32 s_triCount;

	bool init()
	{
		if (!s_shader.load("Shaders/tri2dColor.vert", "Shaders/tri2dColor.frag"))
		{
			return false;
		}

		s_svScaleOffset = s_shader.getVariableId("ScaleOffset");
		if (s_svScaleOffset < 0)
		{
			return false;
		}

		// Create vertex and index buffers.
		u32* indices = new u32[3 * TRIANGLE_MAX];
		u32* outIndices = indices;
		for (u32 i = 0; i < TRIANGLE_MAX; i++, outIndices += 3)
		{
			outIndices[0] = i * 3 + 0;
			outIndices[1] = i * 3 + 1;
			outIndices[2] = i * 3 + 2;
		}
		s_vertices = new TriangleVertex[3 * TRIANGLE_MAX];

		s_vertexBuffer.create(3 * TRIANGLE_MAX, sizeof(TriangleVertex), c_triAttrCount, c_triAttrMapping, true);
		s_indexBuffer.create(3 * TRIANGLE_MAX, sizeof(u32), false, indices);
		delete[] indices;
		s_triCount = 0;

		return true;
	}

	void destroy()
	{
		s_shader.destroy();
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();

		delete[] s_vertices;
	}

	static u32 s_width, s_height;
	static f32 s_pixelsToWorldUnits;
	static Vec2f s_worldUnitsToPixels;
	static Vec2f s_offsetWorldUnits;
	
	void begin(u32 width, u32 height, f32 pixelsToWorldUnits, f32 offsetX_WorldUnits, f32 offsetY_WorldUnits)
	{
		s_width = width;
		s_height = height;
		s_triCount = 0;

		s_pixelsToWorldUnits = pixelsToWorldUnits;
		s_worldUnitsToPixels.x = 1.0f / pixelsToWorldUnits;
		s_worldUnitsToPixels.z = -1.0f / pixelsToWorldUnits;
		s_offsetWorldUnits = { -offsetX_WorldUnits* s_worldUnitsToPixels.x, offsetY_WorldUnits*s_worldUnitsToPixels.z };
	}

	void addTriangles(u32 count, const Vec2f* triVtx, const u32* triColors)
	{
		TriangleVertex* vert = &s_vertices[s_triCount * 3];
		for (u32 i = 0; i < count && s_triCount < TRIANGLE_MAX; i++, triVtx += 3, triColors++, vert += 3)
		{
			s_triCount++;

			// Convert to pixels
			vert[0].pos.x = triVtx[0].x * s_worldUnitsToPixels.x + s_offsetWorldUnits.x;
			vert[1].pos.x = triVtx[1].x * s_worldUnitsToPixels.x + s_offsetWorldUnits.x;
			vert[2].pos.x = triVtx[2].x * s_worldUnitsToPixels.x + s_offsetWorldUnits.x;

			vert[0].pos.z = triVtx[0].z * s_worldUnitsToPixels.z + s_offsetWorldUnits.z;
			vert[1].pos.z = triVtx[1].z * s_worldUnitsToPixels.z + s_offsetWorldUnits.z;
			vert[2].pos.z = triVtx[2].z * s_worldUnitsToPixels.z + s_offsetWorldUnits.z;

			// Copy the colors.
			vert[0].color = *triColors;
			vert[1].color = *triColors;
			vert[2].color = *triColors;
		}
	}

	void addTriangle(const Vec2f* triVtx, u32 triColor)
	{
		addTriangles(1, triVtx, &triColor);
	}

	void drawTriangles()
	{
		if (s_triCount < 1) { return; }

		s_vertexBuffer.update(s_vertices, s_triCount * 3 * sizeof(TriangleVertex));

		const f32 scaleX = 2.0f / f32(s_width);
		const f32 scaleY = 2.0f / f32(s_height);
		const f32 offsetX = -1.0f;
		const f32 offsetY = -1.0f;

		DXL2_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST | STATE_DEPTH_WRITE);

		// Enable blending.
		DXL2_RenderState::setStateEnable(true, STATE_BLEND);
		DXL2_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };
		s_shader.setVariable(s_svScaleOffset, SVT_VEC4, scaleOffset);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		u32 indexSizeType = s_indexBuffer.bind();

		// Draw.
		DXL2_RenderBackend::drawIndexedTriangles(s_triCount, sizeof(u32));

		// Cleanup.
		s_shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();

		// Clear
		s_triCount = 0;
	}
}
