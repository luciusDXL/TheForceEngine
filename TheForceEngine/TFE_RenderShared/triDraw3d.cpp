#include "triDraw3d.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <assert.h>
#include <vector>

#define TRI3D_MAX_DRAW_COUNT 65536
#define TRI3D_DRAW_COUNT_RES 4096
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
		Vec2f uv2;
		u32   color;	// color + opacity.
	};
	struct Tri3dDraw
	{
		TextureGpu* texture;
		DrawMode mode;
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
		{ATTR_UV2,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_tri3dAttrCount = TFE_ARRAYSIZE(c_tri3dAttrMapping);

	struct ShaderState
	{
		s32 svCameraPos = -1;
		s32 svCameraView = -1;
		s32 svCameraProj = -1;
		s32 svGridScaleOpacity = -1;
		s32 svIsTextured = -1;
	};
	static ShaderState s_shaderState[TRIMODE_COUNT];

	static Shader s_shader[TRIMODE_COUNT];
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static Tri3dVertex* s_vertices = nullptr;
	static s32* s_indices = nullptr;
	
	static u32 s_vtxCount;
	static u32 s_idxCount;

	static Tri3dDraw* s_triDraw[TRIMODE_COUNT] = { nullptr };
	static u32 s_triDrawCount[TRIMODE_COUNT];
	static u32 s_triDrawCapacity[TRIMODE_COUNT];

	static DrawMode s_lastDrawMode = TRIMODE_COUNT;

	bool tri3d_loadTextureVariant(DrawMode mode, u32 defineCount, ShaderDefine* defines)
	{
		// Transparent
		if (!s_shader[mode].load("Shaders/tri3dWall.vert", "Shaders/tri3dWall.frag", defineCount, defines))
		{
			return false;
		}
		s_shader[mode].bindTextureNameToSlot("Image", 0);

		s_shaderState[mode].svCameraPos = s_shader[mode].getVariableId("CameraPos");
		s_shaderState[mode].svCameraView = s_shader[mode].getVariableId("CameraView");
		s_shaderState[mode].svCameraProj = s_shader[mode].getVariableId("CameraProj");
		s_shaderState[mode].svGridScaleOpacity = s_shader[mode].getVariableId("GridScaleOpacity");
		s_shaderState[mode].svIsTextured = s_shader[mode].getVariableId("isTextured");
		if (s_shaderState[mode].svCameraPos < 0 || s_shaderState[mode].svCameraView < 0 || s_shaderState[mode].svCameraProj < 0)
		{
			return false;
		}
		return true;
	}
	
	bool tri3d_init()
	{
		ShaderDefine defines[] = { { "TRANS", "1" }, { "TEX_CLAMP", "1" } };
		tri3d_loadTextureVariant(TRIMODE_OPAQUE, 0, nullptr);
		tri3d_loadTextureVariant(TRIMODE_BLEND, 1, defines);
		tri3d_loadTextureVariant(TRIMODE_CLAMP, 2, defines);

		// Create buffers
		// Create vertex and index buffers.
		s_vertices = new Tri3dVertex[VTX3D_MAX];
		s_indices = new s32[IDX3D_MAX];

		s_vertexBuffer.create(VTX3D_MAX, sizeof(Tri3dVertex), c_tri3dAttrCount, c_tri3dAttrMapping, true);
		s_indexBuffer.create(IDX3D_MAX, sizeof(u32), true);

		s_triDraw[TRIMODE_OPAQUE] = (Tri3dDraw*)malloc(sizeof(Tri3dDraw) * TRI3D_DRAW_COUNT_RES);
		s_triDrawCapacity[TRIMODE_OPAQUE] = TRI3D_DRAW_COUNT_RES;

		s_triDraw[TRIMODE_BLEND] = (Tri3dDraw*)malloc(sizeof(Tri3dDraw) * TRI3D_DRAW_COUNT_RES);
		s_triDrawCapacity[TRIMODE_BLEND] = TRI3D_DRAW_COUNT_RES;

		s_triDraw[TRIMODE_CLAMP] = (Tri3dDraw*)malloc(sizeof(Tri3dDraw) * TRI3D_DRAW_COUNT_RES);
		s_triDrawCapacity[TRIMODE_CLAMP] = TRI3D_DRAW_COUNT_RES;

		s_idxCount = 0;
		s_vtxCount = 0;
		s_triDrawCount[TRIMODE_OPAQUE] = 0;
		s_triDrawCount[TRIMODE_BLEND] = 0;
		s_triDrawCount[TRIMODE_CLAMP] = 0;

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
		for (s32 i = 0; i < TRIMODE_COUNT; i++)
		{
			free(s_triDraw[i]);
			s_triDraw[i] = nullptr;
		}
	}
	
	void triDraw3d_begin()
	{
		s_idxCount = 0;
		s_vtxCount = 0;
		s_lastDrawMode = TRIMODE_COUNT;
		for (s32 i = 0; i < TRIMODE_COUNT; i++)
		{
			s_triDrawCount[i] = 0;
		}
	}
		
	bool triDraw3d_expand(DrawMode pass)
	{
		if (s_triDrawCapacity[pass] >= TRI3D_MAX_DRAW_COUNT)
		{
			return false;
		}
		s_triDrawCapacity[pass] += TRI3D_DRAW_COUNT_RES;
		s_triDraw[pass] = (Tri3dDraw*)realloc(s_triDraw, sizeof(Tri3dDraw) * s_triDrawCapacity[pass]);
		return true;
	}

	Tri3dDraw* getTriDraw(DrawMode pass)
	{
		if (s_triDrawCount[pass] >= s_triDrawCapacity[pass])
		{
			if (!triDraw3d_expand(pass)) { return nullptr; }
		}
		Tri3dDraw* draw = &s_triDraw[pass][s_triDrawCount[pass]];
		s_triDrawCount[pass]++;
		return draw;
	}

	bool canMergeDraws(DrawMode mode, TextureGpu* texture)
	{
		bool canMerge = (s_triDrawCount[mode] > 0 && s_triDraw[mode][s_triDrawCount[mode] - 1].texture == texture && mode == s_lastDrawMode);
		s_lastDrawMode = mode;
		return canMerge;
	}
		
	void triDraw3d_addQuadTextured(DrawMode pass, Vec3f* corners, const Vec2f* uvCorners, const u32 color, TextureGpu* texture)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		const f32 dx = fabsf(corners[1].x - corners[0].x);
		const f32 dz = fabsf(corners[1].z - corners[0].z);

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + 4 > VTX3D_MAX || s_idxCount + 6 > IDX3D_MAX)
		{
			return;
		}

		// New draw call or add to the existing call?
		if (canMergeDraws(pass, texture))
		{
			// Append to the previous draw call.
			s_triDraw[pass][s_triDrawCount[pass] - 1].vtxCount += 4;
			s_triDraw[pass][s_triDrawCount[pass] - 1].idxCount += 6;
		}
		else
		{
			// Too many draw calls?
			Tri3dDraw* draw = getTriDraw(pass);
			// Add a new draw call.
			if (draw)
			{
				draw->texture = texture;
				draw->mode = pass;
				draw->vtxOffset = vtxOffset;
				draw->idxOffset = idxOffset;
				draw->vtxCount = 4;
				draw->idxCount = 6;
			}
			else
			{
				return;
			}
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		outVert[0].pos = { corners[0].x, corners[0].y, corners[0].z };
		outVert[0].uv = { 0.0f, 0.0f };
		outVert[0].uv1 = { (dx >= dz) ? corners[0].x : corners[0].z, corners[0].y };
		outVert[0].uv2 = { uvCorners[0].x, uvCorners[0].z };
		outVert[0].color = color;

		outVert[1].pos = { corners[1].x, corners[0].y, corners[1].z };
		outVert[1].uv = { 1.0f, 0.0f };
		outVert[1].uv1 = { (dx >= dz) ? corners[1].x : corners[1].z, corners[0].y };
		outVert[1].uv2 = { uvCorners[1].x, uvCorners[0].z };
		outVert[1].color = color;

		outVert[2].pos = { corners[1].x, corners[1].y, corners[1].z };
		outVert[2].uv = { 1.0f, 1.0f };
		outVert[2].uv1 = { (dx >= dz) ? corners[1].x : corners[1].z, corners[1].y };
		outVert[2].uv2 = { uvCorners[1].x, uvCorners[1].z };
		outVert[2].color = color;

		outVert[3].pos = { corners[0].x, corners[1].y, corners[0].z };
		outVert[3].uv = { 0.0f, 1.0f };
		outVert[3].uv1 = { (dx >= dz) ? corners[0].x : corners[0].z, corners[1].y };
		outVert[3].uv2 = { uvCorners[0].x, uvCorners[1].z };
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

	void triDraw3d_addTextured(DrawMode pass, u32 idxCount, u32 vtxCount, const Vec3f* vertices, const Vec2f* uv, const s32* indices, const u32 color, bool invSide, TextureGpu* texture)
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
		if (canMergeDraws(pass, texture))
		{
			// Append to the previous draw call.
			s_triDraw[pass][s_triDrawCount[pass] - 1].vtxCount += vtxCount;
			s_triDraw[pass][s_triDrawCount[pass] - 1].idxCount += idxCount;
		}
		else
		{
			// Too many draw calls?
			Tri3dDraw* draw = getTriDraw(pass);
			// Add a new draw call.
			if (draw)
			{
				draw->texture = texture;
				draw->mode = pass;
				draw->vtxOffset = vtxOffset;
				draw->idxOffset = idxOffset;
				draw->vtxCount = vtxCount;
				draw->idxCount = idxCount;
			}
			else
			{
				return;
			}
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		for (u32 v = 0; v < vtxCount; v++)
		{
			outVert[v].pos = vertices[v];
			outVert[v].uv = { 0.5f, 0.5f };
			outVert[v].uv1 = { outVert[v].pos.x, outVert[v].pos.z };
			outVert[v].uv2 = uv[v];
			outVert[v].color = color;
		}

		for (u32 i = 0; i < idxCount; i += 3)
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

	void triDraw3d_addQuadColored(DrawMode pass, Vec3f* corners, const u32 color)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		const f32 dx = fabsf(corners[1].x - corners[0].x);
		const f32 dz = fabsf(corners[1].z - corners[0].z);

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + 4 > VTX3D_MAX || s_idxCount + 6 > IDX3D_MAX)
		{
			return;
		}

		// New draw call or add to the existing call?
		if (canMergeDraws(pass, nullptr))
		{
			// Append to the previous draw call.
			s_triDraw[pass][s_triDrawCount[pass] - 1].vtxCount += 4;
			s_triDraw[pass][s_triDrawCount[pass] - 1].idxCount += 6;
		}
		else
		{
			// Too many draw calls?
			Tri3dDraw* draw = getTriDraw(pass);
			// Add a new draw call.
			if (draw)
			{
				draw->texture = nullptr;
				draw->mode = pass;
				draw->vtxOffset = vtxOffset;
				draw->idxOffset = idxOffset;
				draw->vtxCount = 4;
				draw->idxCount = 6;
			}
			else
			{
				return;
			}
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		outVert[0].pos = { corners[0].x, corners[0].y, corners[0].z };
		outVert[0].uv = { 0.0f, 0.0f };
		outVert[0].uv1 = { (dx >= dz) ? corners[0].x : corners[0].z, corners[0].y };
		outVert[0].color = color;

		outVert[1].pos = { corners[1].x, corners[0].y, corners[1].z };
		outVert[1].uv = { 1.0f, 0.0f };
		outVert[1].uv1 = { (dx >= dz) ? corners[1].x : corners[1].z, corners[0].y };
		outVert[1].color = color;

		outVert[2].pos = { corners[1].x, corners[1].y, corners[1].z };
		outVert[2].uv = { 1.0f, 1.0f };
		outVert[2].uv1 = { (dx >= dz) ? corners[1].x : corners[1].z, corners[1].y };
		outVert[2].color = color;

		outVert[3].pos = { corners[0].x, corners[1].y, corners[0].z };
		outVert[3].uv = { 0.0f, 1.0f };
		outVert[3].uv1 = { (dx >= dz) ? corners[0].x : corners[0].z, corners[1].y };
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

	void triDraw3d_addColored(DrawMode pass, u32 idxCount, u32 vtxCount, const Vec3f* vertices, const s32* indices, const u32 color, bool invSide)
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
		if (canMergeDraws(pass, nullptr))
		{
			// Append to the previous draw call.
			s_triDraw[pass][s_triDrawCount[pass] - 1].vtxCount += vtxCount;
			s_triDraw[pass][s_triDrawCount[pass] - 1].idxCount += idxCount;
		}
		else
		{
			// Too many draw calls?
			Tri3dDraw* draw = getTriDraw(pass);
			// Add a new draw call.
			if (draw)
			{
				draw->texture = nullptr;
				draw->mode = pass;
				draw->vtxOffset = vtxOffset;
				draw->idxOffset = idxOffset;
				draw->vtxCount = vtxCount;
				draw->idxCount = idxCount;
			}
			else
			{
				return;
			}
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		for (u32 v = 0; v < vtxCount; v++)
		{
			outVert[v].pos = vertices[v];
			outVert[v].uv = { 0.5f, 0.5f };
			outVert[v].uv1 = { outVert[v].pos.x, outVert[v].pos.z };
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

	void triDraw3d_draw(const Camera3d* camera, f32 gridScale, f32 gridOpacity)
	{
		if (s_vtxCount < 1 || s_idxCount < 1) { return; }

		s_vertexBuffer.update(s_vertices, s_vtxCount * sizeof(Tri3dVertex));
		s_indexBuffer.update(s_indices, s_idxCount * sizeof(s32));

		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE | STATE_CULLING);
		TFE_RenderState::setStateEnable(true, STATE_DEPTH_TEST);
		TFE_RenderState::setDepthFunction(CMP_LEQUAL);

		// Enable blending.
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		bool blendEnable[] = { false, true };
		for (s32 i = 0; i < TRIMODE_COUNT; i++)
		{
			if (s_triDrawCount[i] < 1) { continue; }

			s_shader[i].bind();
			// Bind Uniforms & Textures.
			s_shader[i].setVariable(s_shaderState[i].svCameraPos, SVT_VEC3, camera->pos.m);
			s_shader[i].setVariable(s_shaderState[i].svCameraView, SVT_MAT3x3, camera->viewMtx.data);
			s_shader[i].setVariable(s_shaderState[i].svCameraProj, SVT_MAT4x4, camera->projMtx.data);
			if (s_shaderState[i].svGridScaleOpacity >= 0)
			{
				f32 gridScaleOpacity[] = { gridScale, gridOpacity };
				s_shader[i].setVariable(s_shaderState[i].svGridScaleOpacity, SVT_VEC2, gridScaleOpacity);
			}

			// Bind vertex/index buffers and setup attributes for BlitVert
			s_vertexBuffer.bind();
			s_indexBuffer.bind();

			TFE_RenderState::setStateEnable(blendEnable[i], STATE_BLEND);

			// Draw.
			for (u32 t = 0; t < s_triDrawCount[i]; t++)
			{
				Tri3dDraw* draw = &s_triDraw[i][t];
				s32 isTextured = draw->texture ? 1 : 0;
				s_shader[i].setVariable(s_shaderState[i].svIsTextured, SVT_ISCALAR, &isTextured);
				if (isTextured)
				{
					draw->texture->bind(0);
				}
				TFE_RenderBackend::drawIndexedTriangles(draw->idxCount / 3, sizeof(u32), draw->idxOffset);
			}
		}

		// Cleanup.
		s_shader[0].unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();
		TFE_RenderState::setDepthBias();

		// Clear
		for (s32 i = 0; i < TRIMODE_COUNT; i++)
		{
			s_triDrawCount[i] = 0;
		}
	}
}
