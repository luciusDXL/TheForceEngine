#include "triDraw2d.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <assert.h>
#include <vector>

#define TRI_MAX_DRAW_COUNT 4096
#define TRI_MAX 65536
#define IDX_MAX TRI_MAX * 3
#define VTX_MAX TRI_MAX * 3

#define SHOW_WIREFRAME 0
#define SHOW_RAINBOW 0

namespace TFE_RenderShared
{
	// Vertex Definition
	struct TriVertex
	{
		Vec2f pos;		// 2D position.
		Vec2f uv;		// UV coordinates.
		u32   color;	// color + opacity.
	};
	struct TriDraw
	{
		TextureGpu* texture;
		s32 vtxOffset;
		s32 idxOffset;
		s32 vtxCount;
		s32 idxCount;
	};
	static const AttributeMapping c_triAttrMapping[]=
	{
		{ATTR_POS,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_triAttrCount = TFE_ARRAYSIZE(c_triAttrMapping);

	static s32 s_svScaleOffset = -1;
	static s32 s_isTexturedId = -1;

	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static TriVertex* s_vertices = nullptr;
	static s32* s_indices = nullptr;
	static u32 s_triDrawCount;

	static u32 s_vtxCount;
	static u32 s_idxCount;

	static TriDraw s_triDraw[TRI_MAX_DRAW_COUNT];
	static u32 s_width, s_height;

	bool tri2d_init()
	{
		if (!s_shader.load("Shaders/tri2dTextured.vert", "Shaders/tri2dTextured.frag"))
		{
			return false;
		}
		s_shader.bindTextureNameToSlot("Image", 0);

		s_svScaleOffset = s_shader.getVariableId("ScaleOffset");
		if (s_svScaleOffset < 0)
		{
			return false;
		}
		s_isTexturedId = s_shader.getVariableId("isTextured");
		if (s_isTexturedId < 0)
		{
			return false;
		}

		// Create buffers
		// Create vertex and index buffers.
		s_vertices = new TriVertex[VTX_MAX];
		s_indices = new s32[IDX_MAX];

		s_vertexBuffer.create(VTX_MAX, sizeof(TriVertex), c_triAttrCount, c_triAttrMapping, true);
		s_indexBuffer.create(IDX_MAX, sizeof(u32), true);

		s_idxCount = 0;
		s_vtxCount = 0;
		s_triDrawCount = 0;

		return true;
	}

	void tri2d_destroy()
	{
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
		delete[] s_vertices;
		delete[] s_indices;
		s_vertices = nullptr;
		s_indices = nullptr;
	}
	
	void triDraw2d_begin(u32 width, u32 height)
	{
		s_width = width;
		s_height = height;
		s_idxCount = 0;
		s_vtxCount = 0;
		s_triDrawCount = 0;
	}

	void triDraw2d_addColored(u32 idxCount, u32 vtxCount, const Vec2f* vertices, const s32* indices, const u32 color)
	{
		triDraw2D_addTextured(idxCount, vtxCount, vertices, nullptr, indices, color, nullptr);
	}

	void triDraw2D_addTextured(u32 idxCount, u32 vtxCount, const Vec2f* vertices, const Vec2f* uv, const s32* indices, const u32 color, TextureGpu* texture)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + vtxCount > VTX_MAX || s_idxCount + idxCount > IDX_MAX)
		{
			return;
		}

		// New draw call or add to the existing call?
		if (s_triDrawCount > 0 && s_triDraw[s_triDrawCount - 1].texture == texture)
		{
			// Append to the previous draw call.
			s_triDraw[s_triDrawCount - 1].vtxCount += vtxCount;
			s_triDraw[s_triDrawCount - 1].idxCount += idxCount;
		}
		else
		{
			// Too many draw calls?
			if (s_triDrawCount >= TRI_MAX_DRAW_COUNT) { return; }
			// Add a new draw call.
			s_triDraw[s_triDrawCount].texture = texture;
			s_triDraw[s_triDrawCount].vtxOffset = vtxOffset;
			s_triDraw[s_triDrawCount].idxOffset = idxOffset;
			s_triDraw[s_triDrawCount].vtxCount = vtxCount;
			s_triDraw[s_triDrawCount].idxCount = idxCount;
			s_triDrawCount++;
		}

		TriVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		const Vec2f zero = { 0 };
		for (u32 i = 0; i < vtxCount; i++, vertices++)
		{
			outVert[i].pos = *vertices;
			outVert[i].uv = uv ? uv[i] : zero;
		#if SHOW_RAINBOW == 1
			outVert[i].color = (color + 0x114020*i) | 0xff000000;
		#else
			outVert[i].color = color;
		#endif
		}

		for (u32 i = 0; i < idxCount; i++)
		{
			outIdx[i] = indices[i] + vtxOffset;
		}

		s_vtxCount += vtxCount;
		s_idxCount += idxCount;
	}

	void triDraw2d_draw()
	{
		if (s_triDrawCount < 1) { return; }

		s_vertexBuffer.update(s_vertices, s_vtxCount * sizeof(TriVertex));
		s_indexBuffer.update(s_indices, s_idxCount * sizeof(s32));

		const f32 scaleX = 2.0f / f32(s_width);
		const f32 scaleY = 2.0f / f32(s_height);
		const f32 offsetX = -1.0f;
		const f32 offsetY = -1.0f;

		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST | STATE_DEPTH_WRITE);

		// Enable blending.
		#if SHOW_WIREFRAME == 1
			TFE_RenderState::setStateEnable(true, STATE_BLEND | STATE_WIREFRAME);
		#else
			TFE_RenderState::setStateEnable(true, STATE_BLEND);
		#endif
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };
		s_shader.setVariable(s_svScaleOffset, SVT_VEC4, scaleOffset);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		u32 indexSizeType = s_indexBuffer.bind();

		for (u32 i = 0; i < s_triDrawCount; i++)
		{
			TriDraw* draw = &s_triDraw[i];
			s32 isTextured = s_triDraw[i].texture != nullptr ? 1 : 0;
			s_shader.setVariable(s_isTexturedId, SVT_ISCALAR, &isTextured);

			if (isTextured)
			{
				s_triDraw[i].texture->bind();
			}
			TFE_RenderBackend::drawIndexedTriangles(draw->idxCount/3, sizeof(u32), draw->idxOffset);
		}

		// Cleanup.
		s_shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();

		#if SHOW_WIREFRAME == 1
			TFE_RenderState::setStateEnable(false, STATE_WIREFRAME);
		#endif

		// Clear
		s_triDrawCount = 0;
		s_vtxCount = 0;
		s_idxCount = 0;
	}
}
