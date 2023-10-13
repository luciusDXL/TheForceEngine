#include "triDraw3d.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <assert.h>
#include <vector>

#define TRI3D_MAX_DRAW_COUNT 4096
#define TRI3D_MAX 65536
#define IDX3D_MAX TRI3D_MAX * 3
#define VTX3D_MAX TRI3D_MAX * 3

namespace TFE_RenderShared
{
	// Vertex Definition
	struct Tri3dVertex
	{
		Vec3f pos;		// 2D position.
		Vec2f uv;		// UV coordinates.
		Vec2f uv1;
		u32   color;	// color + opacity.
	};
	struct Tri3dDraw
	{
		TextureGpu* texture;
		s32 vtxOffset;
		s32 idxOffset;
		s32 vtxCount;
		s32 idxCount;
	};
	static const AttributeMapping c_tri3dAttrMapping[]=
	{
		{ATTR_POS,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 2, 0, false},
		{ATTR_UV1,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_tri3dAttrCount = TFE_ARRAYSIZE(c_tri3dAttrMapping);

	static s32 s_svCameraPos = -1;
	static s32 s_svCameraView = -1;
	static s32 s_svCameraProj = -1;

	static Shader s_shader;
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static Tri3dVertex* s_vertices = nullptr;
	static s32* s_indices = nullptr;
	static u32 s_triDrawCount;

	static u32 s_vtxCount;
	static u32 s_idxCount;

	static Tri3dDraw s_triDraw[TRI3D_MAX_DRAW_COUNT];
	
	bool tri3d_init()
	{
		if (!s_shader.load("Shaders/tri3dWall.vert", "Shaders/tri3dWall.frag"))
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
		s_vertices = new Tri3dVertex[VTX3D_MAX];
		s_indices = new s32[IDX3D_MAX];

		s_vertexBuffer.create(VTX3D_MAX, sizeof(Tri3dVertex), c_tri3dAttrCount, c_tri3dAttrMapping, true);
		s_indexBuffer.create(IDX3D_MAX, sizeof(u32), true);

		s_idxCount = 0;
		s_vtxCount = 0;
		s_triDrawCount = 0;

		return true;
	}

	void tri3d_destroy()
	{
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
		delete[] s_vertices;
		delete[] s_indices;
		s_vertices = nullptr;
		s_indices = nullptr;
	}
	
	void triDraw3d_begin()
	{
		s_idxCount = 0;
		s_vtxCount = 0;
		s_triDrawCount = 0;
	}

	void triDraw3d_addQuadColored(Vec3f* corners, const u32 color)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + 4 > VTX3D_MAX || s_idxCount + 6 > IDX3D_MAX)
		{
			return;
		}

		// New draw call or add to the existing call?
		if (s_triDrawCount > 0 && s_triDraw[s_triDrawCount - 1].texture == nullptr)
		{
			// Append to the previous draw call.
			s_triDraw[s_triDrawCount - 1].vtxCount += 4;
			s_triDraw[s_triDrawCount - 1].idxCount += 6;
		}
		else
		{
			// Too many draw calls?
			if (s_triDrawCount >= TRI3D_MAX_DRAW_COUNT) { return; }
			// Add a new draw call.
			s_triDraw[s_triDrawCount].texture = nullptr;
			s_triDraw[s_triDrawCount].vtxOffset = vtxOffset;
			s_triDraw[s_triDrawCount].idxOffset = idxOffset;
			s_triDraw[s_triDrawCount].vtxCount = 4;
			s_triDraw[s_triDrawCount].idxCount = 6;
			s_triDrawCount++;
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		outVert[0].pos = { corners[0].x, corners[0].y, corners[0].z };
		outVert[0].uv = { 0.0f, 0.0f };
		outVert[0].color = color;

		outVert[1].pos = { corners[1].x, corners[0].y, corners[1].z };
		outVert[1].uv = { 1.0f, 0.0f };
		outVert[1].color = color;

		outVert[2].pos = { corners[1].x, corners[1].y, corners[1].z };
		outVert[2].uv = { 1.0f, 1.0f };
		outVert[2].color = color;

		outVert[3].pos = { corners[0].x, corners[1].y, corners[0].z };
		outVert[3].uv = { 0.0f, 1.0f };
		outVert[3].color = color;

		outIdx[0] = vtxOffset + 0;
		outIdx[1] = vtxOffset + 1;
		outIdx[2] = vtxOffset + 2;

		outIdx[3] = vtxOffset + 0;
		outIdx[4] = vtxOffset + 2;
		outIdx[5] = vtxOffset + 3;

		s_vtxCount += 4;
		s_idxCount += 6;
	}

	void triDraw3d_addColored(u32 idxCount, u32 vtxCount, const Vec3f* vertices, const s32* indices, const u32 color, bool invSide)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + vtxCount > VTX3D_MAX || s_idxCount + idxCount > IDX3D_MAX)
		{
			return;
		}

		// New draw call or add to the existing call?
		if (s_triDrawCount > 0 && s_triDraw[s_triDrawCount - 1].texture == nullptr)
		{
			// Append to the previous draw call.
			s_triDraw[s_triDrawCount - 1].vtxCount += vtxCount;
			s_triDraw[s_triDrawCount - 1].idxCount += idxCount;
		}
		else
		{
			// Too many draw calls?
			if (s_triDrawCount >= TRI3D_MAX_DRAW_COUNT) { return; }
			// Add a new draw call.
			s_triDraw[s_triDrawCount].texture = nullptr;
			s_triDraw[s_triDrawCount].vtxOffset = vtxOffset;
			s_triDraw[s_triDrawCount].idxOffset = idxOffset;
			s_triDraw[s_triDrawCount].vtxCount = vtxCount;
			s_triDraw[s_triDrawCount].idxCount = idxCount;
			s_triDrawCount++;
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		for (u32 v = 0; v < vtxCount; v++)
		{
			outVert[v].pos = vertices[v];
			outVert[v].uv = { 0.5f, 0.5f };
			outVert[v].color = color;
		}

		for (u32 i = 0; i < idxCount; i+=3)
		{
			if (invSide)
			{
				outIdx[i + 0] = indices[i + 0] + vtxOffset;
				outIdx[i + 1] = indices[i + 2] + vtxOffset;
				outIdx[i + 2] = indices[i + 1] + vtxOffset;
			}
			else
			{
				outIdx[i + 0] = indices[i + 0] + vtxOffset;
				outIdx[i + 1] = indices[i + 1] + vtxOffset;
				outIdx[i + 2] = indices[i + 2] + vtxOffset;
			}
		}

		s_vtxCount += vtxCount;
		s_idxCount += idxCount;
	}

	void triDraw3d_draw(const Camera3d* camera)
	{
		if (s_triDrawCount < 1) { return; }

		s_vertexBuffer.update(s_vertices, s_vtxCount * sizeof(Tri3dVertex));
		s_indexBuffer.update(s_indices, s_idxCount * sizeof(s32));

		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE | STATE_CULLING);
		TFE_RenderState::setStateEnable(true, STATE_DEPTH_TEST);
		TFE_RenderState::setDepthFunction(CMP_LEQUAL);

		// Enable blending.
		TFE_RenderState::setStateEnable(true, STATE_BLEND);
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		// Bind Uniforms & Textures.
		s_shader.setVariable(s_svCameraPos, SVT_VEC3, camera->pos.m);
		s_shader.setVariable(s_svCameraView, SVT_MAT3x3, camera->viewMtx.data);
		s_shader.setVariable(s_svCameraProj, SVT_MAT4x4, camera->projMtx.data);

		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		// Draw.
		for (u32 t = 0; t < s_triDrawCount; t++)
		{
			Tri3dDraw* draw = &s_triDraw[t];
			TFE_RenderBackend::drawIndexedTriangles(draw->idxCount / 3, sizeof(u32), draw->idxOffset);
		}

		// Cleanup.
		s_shader.unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();
		TFE_RenderState::setDepthBias();

		// Clear
		s_triDrawCount = 0;
	}
}
