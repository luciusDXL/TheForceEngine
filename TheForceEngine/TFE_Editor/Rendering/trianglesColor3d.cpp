#include "trianglesColor3d.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <vector>

#define TRI_MAX 65536
#define DRAW_CALL_MAX 2048

namespace TrianglesColor3d
{
	// Vertex Definition
	struct TriVertex
	{
		Vec3f pos;
		Vec2f uv;
		Vec2f uv1;
		u32   color;
	};
	static const AttributeMapping c_triAttrMapping[]=
	{
		{ATTR_POS,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 2, 0, false},
		{ATTR_UV1,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_triAttrCount = TFE_ARRAYSIZE(c_triAttrMapping);

	struct DrawCall
	{
		const TextureGpu* texture;
		u32 start;
		u32 count;
		Tri3dTrans trans;
	};
	static DrawCall s_drawCalls[DRAW_CALL_MAX];
	static DrawCall* s_curDraw;
	static u32 s_drawCount;

	struct ShaderInfo
	{
		Shader shader;
		s32 svCameraPos = -1;
		s32 svCameraView = -1;
		s32 svCameraProj = -1;
		s32 svGridHeight = -1;
	};
	static ShaderInfo s_shaderInfo[1 + TRANS_COUNT];

	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static TriVertex* s_vertices = nullptr;
	static u32 s_triCount;

	static const TextureGpu* s_curTexture;
	static Tri3dTrans s_curTrans;

	bool loadShaderInfo(u32 index, const char* vertFile, const char* fragFile)
	{
		ShaderInfo* sinfo = &s_shaderInfo[index];
		if (!sinfo->shader.load(vertFile, fragFile))
		{
			return false;
		}

		sinfo->svCameraPos = sinfo->shader.getVariableId("CameraPos");
		sinfo->svCameraView = sinfo->shader.getVariableId("CameraView");
		sinfo->svCameraProj = sinfo->shader.getVariableId("CameraProj");
		sinfo->svGridHeight = sinfo->shader.getVariableId("GridHeight");
		sinfo->shader.bindTextureNameToSlot("filterMap", 0);
		sinfo->shader.bindTextureNameToSlot("image", 1);
		if (sinfo->svCameraPos < 0 || sinfo->svCameraView < 0 || sinfo->svCameraProj < 0)
		{
			return false;
		}

		return true;
	}

	bool init()
	{
		if (!loadShaderInfo(0, "Shaders/tri3dColor.vert", "Shaders/tri3dColor.frag"))
		{
			return false;
		}
		if (!loadShaderInfo(1, "Shaders/tri3dTexture.vert", "Shaders/tri3dTexture.frag"))
		{
			return false;
		}
		if (!loadShaderInfo(2, "Shaders/tri3dTexture.vert", "Shaders/tri3dTextureTrans.frag"))
		{
			return false;
		}
		if (!loadShaderInfo(3, "Shaders/tri3dTexture.vert", "Shaders/tri3dTextureClamp.frag"))
		{
			return false;
		}
		s_shaderInfo[4] = s_shaderInfo[1];
		s_shaderInfo[5] = s_shaderInfo[3];
		
		// Create buffers
		// Create vertex and index buffers.
		u32* indices = new u32[3 * TRI_MAX];
		u32* outIndices = indices;
		for (u32 i = 0; i < TRI_MAX; i++, outIndices += 3)
		{
			outIndices[0] = i * 3 + 0;
			outIndices[1] = i * 3 + 1;
			outIndices[2] = i * 3 + 2;
		}
		s_vertices = new TriVertex[3 * TRI_MAX];

		s_vertexBuffer.create(3 * TRI_MAX, sizeof(TriVertex), c_triAttrCount, c_triAttrMapping, true);
		s_indexBuffer.create(3 * TRI_MAX, sizeof(u32), false, indices);

		delete[] indices;
		s_triCount = 0;
		s_drawCount = 0;
		s_curTexture = nullptr;
		s_curTrans = TRANS_NONE;

		return true;
	}

	void destroy()
	{
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
		delete[] s_vertices;
		s_vertices = nullptr;
	}
	
	void addTriangles(u32 count, const Vec3f* vtx, const Vec2f* uv, const u32* triColors, bool blend)
	{
		const Tri3dTrans trans = blend ? TRANS_BLEND : TRANS_NONE;
		if (s_curTexture != nullptr || s_drawCount == 0u || s_curTrans != trans)
		{
			s_curDraw = &s_drawCalls[s_drawCount];
			s_curDraw->start = s_triCount * 3;
			s_curDraw->count = 0;
			s_curDraw->trans = trans;
			s_curDraw->texture = nullptr;
			s_curTexture = nullptr;
			s_curTrans = trans;

			s_drawCount++;
		}
		s_curDraw->count += count;

		Vec2f defUv = { 0.5f, 0.5f };

		TriVertex* vert = &s_vertices[s_triCount * 3];
		for (u32 i = 0; i < count && s_triCount < TRI_MAX; i++, vtx += 3, triColors++, vert += 3)
		{
			s_triCount++;

			// Build vertices - the positions will be updated in the shader.
			vert[0].pos = vtx[0];
			vert[1].pos = vtx[1];
			vert[2].pos = vtx[2];

			if (uv)
			{
				vert[0].uv = uv[0];
				vert[1].uv = uv[1];
				vert[2].uv = uv[2];

				vert[0].uv1 = uv[0];
				vert[1].uv1 = uv[0];
				vert[2].uv1 = uv[0];
			}
			else
			{
				vert[0].uv = defUv;
				vert[1].uv = defUv;
				vert[2].uv = defUv;

				vert[0].uv1 = defUv;
				vert[1].uv1 = defUv;
				vert[2].uv1 = defUv;
			}

			for (u32 v = 0; v < 3; v++)
			{
				vert[v].color = *triColors;
			}

			if (uv) { uv += 3; }
		}
	}

	void addTriangle(const Vec3f* vertices, const Vec2f* uv, u32 triColor, bool blend)
	{
		addTriangles(1, vertices, uv, &triColor, blend);
	}

	void addTexturedTriangles(u32 count, const Vec3f* vtx, const Vec2f* uv, const Vec2f* uv1, const u32* triColors, const TextureGpu* texture, Tri3dTrans trans)
	{
		if (s_curTexture != texture || s_curTrans != trans || s_drawCount == 0u)
		{
			s_curDraw = &s_drawCalls[s_drawCount];
			s_curDraw->start = s_triCount * 3;
			s_curDraw->count = 0;
			s_curDraw->trans = trans;
			s_curDraw->texture = texture;

			s_curTexture = texture;
			s_curTrans = trans;
			s_drawCount++;
		}
		s_curDraw->count += count;

		TriVertex* vert = &s_vertices[s_triCount * 3];
		for (u32 i = 0; i < count && s_triCount < TRI_MAX; i++, vtx += 3, uv += 3, triColors++, vert += 3)
		{
			s_triCount++;

			// Build vertices - the positions will be updated in the shader.
			vert[0].pos = vtx[0];
			vert[1].pos = vtx[1];
			vert[2].pos = vtx[2];

			vert[0].uv = uv[0];
			vert[1].uv = uv[1];
			vert[2].uv = uv[2];

			if (uv1)
			{
				vert[0].uv1 = uv1[0];
				vert[1].uv1 = uv1[1];
				vert[2].uv1 = uv1[2];
				uv1 += 3;
			}
			else
			{
				vert[0].uv1 = { 0.5f, 0.5f };
				vert[1].uv1 = { 0.5f, 0.5f };
				vert[2].uv1 = { 0.5f, 0.5f };
			}

			for (u32 v = 0; v < 3; v++)
			{
				vert[v].color = *triColors;
			}
		}
	}

	void addTexturedTriangle(const Vec3f* vertices, const Vec2f* uv, const Vec2f* uv1, u32 triColor, const TextureGpu* texture, Tri3dTrans trans)
	{
		addTexturedTriangles(1, vertices, uv, uv1, &triColor, texture, trans);
	}

	void draw(const Vec3f* camPos, const Mat3* viewMtx, const Mat4* projMtx, bool depthTest, f32 gridHeight)
	{
		if (s_triCount < 1) { return; }

		s_vertexBuffer.update(s_vertices, s_triCount * 3 * sizeof(TriVertex));
		TFE_RenderState::setDepthBias();
		if (depthTest)
		{
			TFE_RenderState::setStateEnable(true, STATE_DEPTH_TEST | STATE_DEPTH_WRITE | STATE_CULLING);
		}
		else
		{
			TFE_RenderState::setStateEnable(true, STATE_CULLING);
			TFE_RenderState::setStateEnable(false, STATE_DEPTH_TEST | STATE_DEPTH_WRITE);
		}
		
		// Bind vertex/index buffers and setup attributes for BlitVert
		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		for (u32 i = 0; i < s_drawCount; i++)
		{
			u32 shaderIndex = 0;
			if (s_drawCalls[i].texture)
			{
				shaderIndex = 1 + s_drawCalls[i].trans;
			}
			ShaderInfo* sinfo = &s_shaderInfo[shaderIndex];

			// Enable blending.
			if (s_drawCalls[i].trans == TRANS_BLEND || s_drawCalls[i].trans == TRANS_BLEND_CLAMP)
			{
				TFE_RenderState::setStateEnable(true, STATE_BLEND);
				TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);
			}
			else
			{
				TFE_RenderState::setStateEnable(false, STATE_BLEND);
			}
			
			sinfo->shader.bind();
			// Bind Uniforms & Textures.
			sinfo->shader.setVariable(sinfo->svCameraPos,  SVT_VEC3, camPos->m);
			sinfo->shader.setVariable(sinfo->svCameraView, SVT_MAT3x3, (f32*)viewMtx);
			sinfo->shader.setVariable(sinfo->svCameraProj, SVT_MAT4x4, (f32*)projMtx);
			sinfo->shader.setVariable(sinfo->svGridHeight, SVT_SCALAR, &gridHeight);
			if (s_drawCalls[i].texture)
			{
				s_drawCalls[i].texture->bind(1);
			}
									
			// Draw.
			TFE_RenderBackend::drawIndexedTriangles(s_drawCalls[i].count, sizeof(u32), s_drawCalls[i].start);

			sinfo->shader.unbind();
		}

		// Cleanup.
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();

		TextureGpu::clear(1);

		// Clear
		s_triCount = 0;
		s_drawCount = 0;
		s_curTexture = nullptr;
		s_curTrans = TRANS_NONE;
	}
}
