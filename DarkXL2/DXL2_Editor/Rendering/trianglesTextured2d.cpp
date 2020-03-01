#include "trianglesTextured2d.h"
#include <DXL2_RenderBackend/shader.h>
#include <DXL2_RenderBackend/textureGpu.h>
#include <DXL2_RenderBackend/vertexBuffer.h>
#include <DXL2_RenderBackend/indexBuffer.h>
#include <DXL2_System/system.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <vector>

#define TRIANGLE_MAX 65536

namespace TriTexturedDraw2d
{
	struct TriangleVertex
	{
		Vec2f pos;	// 2D position.
		Vec2f uv;	// Texture coordinate.
		u32  color;	// vertex color + opacity.
	};
	static const AttributeMapping c_triAttrMapping[] =
	{
		{ATTR_POS,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_triAttrCount = DXL2_ARRAYSIZE(c_triAttrMapping);

	static s32 s_svScaleOffset = -1;

	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer s_indexBuffer;

	static TriangleVertex* s_vertices;
	static const TextureGpu* s_curTexture;
	static u32 s_triCount;

	bool init()
	{
		if (!s_shader.load("Shaders/tri2dTextured.vert", "Shaders/tri2dTextured.frag"))
		{
			return false;
		}

		s_shader.bindTextureNameToSlot("image", 0);
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
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();

		delete[] s_vertices;
	}

	static u32 s_width, s_height;
	static f32 s_pixelsToWorldUnits;
	static Vec2f s_worldUnitsToPixels;
	static Vec2f s_offsetWorldUnits;

	#define DRAW_CALL_MAX 2048
	struct DrawCall
	{
		const TextureGpu* texture;
		u32 start;
		u32 count;
	};
	static DrawCall s_drawCalls[DRAW_CALL_MAX];
	static DrawCall* s_curDraw;
	static u32 s_drawCount;
	
	void begin(u32 width, u32 height, f32 pixelsToWorldUnits, f32 offsetX_WorldUnits, f32 offsetY_WorldUnits)
	{
		s_width = width;
		s_height = height;
		s_triCount = 0;
		s_curTexture = nullptr;

		s_drawCount = 0;
		s_curDraw = &s_drawCalls[s_drawCount];
		s_curDraw->start = 0;
		s_curDraw->count = 0;

		s_pixelsToWorldUnits = pixelsToWorldUnits;
		s_worldUnitsToPixels.x = 1.0f / pixelsToWorldUnits;
		s_worldUnitsToPixels.z = -1.0f / pixelsToWorldUnits;
		s_offsetWorldUnits = { -offsetX_WorldUnits* s_worldUnitsToPixels.x, offsetY_WorldUnits*s_worldUnitsToPixels.z };
	}

	void addTriangles(u32 count, const Vec2f* triVtx, const Vec2f* triUv, const u32* triColors, const TextureGpu* texture)
	{
		if (texture != s_curTexture)
		{
			if (s_curDraw->count) { s_drawCount++; }

			s_curDraw = &s_drawCalls[s_drawCount];
			s_curDraw->start = s_triCount * 3;
			s_curDraw->count = 0;
			s_curDraw->texture = texture;

			s_curTexture = texture;
		}
		s_curDraw->count += count;

		TriangleVertex* vert = &s_vertices[s_triCount * 3];
		for (u32 i = 0; i < count && s_triCount < TRIANGLE_MAX; i++, triVtx += 3, triUv += 3, triColors++, vert += 3)
		{
			s_triCount++;

			// Convert to pixels
			vert[0].pos.x = triVtx[0].x * s_worldUnitsToPixels.x + s_offsetWorldUnits.x;
			vert[1].pos.x = triVtx[1].x * s_worldUnitsToPixels.x + s_offsetWorldUnits.x;
			vert[2].pos.x = triVtx[2].x * s_worldUnitsToPixels.x + s_offsetWorldUnits.x;

			vert[0].pos.z = triVtx[0].z * s_worldUnitsToPixels.z + s_offsetWorldUnits.z;
			vert[1].pos.z = triVtx[1].z * s_worldUnitsToPixels.z + s_offsetWorldUnits.z;
			vert[2].pos.z = triVtx[2].z * s_worldUnitsToPixels.z + s_offsetWorldUnits.z;

			// Copy uvs.
			vert[0].uv = triUv[0];
			vert[1].uv = triUv[1];
			vert[2].uv = triUv[2];

			// Copy the colors.
			vert[0].color = *triColors;
			vert[1].color = *triColors;
			vert[2].color = *triColors;
		}
	}

	void addTriangle(const Vec2f* triVtx, const Vec2f* triUv, u32 triColor, const TextureGpu* texture)
	{
		addTriangles(1, triVtx, triUv, &triColor, texture);
	}

	void drawTriangles()
	{
		if (s_triCount < 1) { return; }
		if (s_curDraw->count)
		{
			s_drawCount++;
		}

		s_vertexBuffer.update(s_vertices, s_triCount * 3 * sizeof(TriangleVertex));

		const f32 scaleX = 2.0f / f32(s_width);
		const f32 scaleY = 2.0f / f32(s_height);
		const f32 offsetX = -1.0f;
		const f32 offsetY = -1.0f;

		DXL2_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST | STATE_DEPTH_WRITE);

		// Enable blending.
		DXL2_RenderState::setStateEnable(true, STATE_BLEND);
		DXL2_RenderState::setBlendMode(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };
		s_shader.setVariable(s_svScaleOffset, SVT_VEC4, scaleOffset);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		// Draw.
		for (u32 i = 0; i < s_drawCount; i++)
		{
			const DrawCall* draw = &s_drawCalls[i];
			if (draw->texture) { draw->texture->bind(); }
			DXL2_RenderBackend::drawIndexedTriangles(draw->count, sizeof(u32), draw->start);
		}

		// Cleanup.
		s_shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();

		// Clear
		s_triCount = 0;
		s_drawCount = 0;
	}
}
