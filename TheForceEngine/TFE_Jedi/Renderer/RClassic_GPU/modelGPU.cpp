#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_System/math.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include <TFE_Input/input.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include "modelGPU.h"
#include "frustum.h"
#include "../rcommon.h"

using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	enum ModelShader
	{
		MGPU_SHADER_SOLID = 0,
		MGPU_SHADER_HOLOGRAM,
		MGPU_SHADER_COUNT
	};

	struct ModelVertex
	{
		Vec3f pos;
		Vec3f nrm;
		Vec2f uv;
		u32 color;
	};

	struct ModelGPU
	{
		ModelShader shader;
		s32 indexStart;
		s32 polyCount;
	};

	struct ModelDraw
	{
		s32 modelId;
		Vec3f posWS;
		f32 transform[9];
	};

	static const AttributeMapping c_modelAttrMapping[] =
	{
		{ATTR_POS,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_NRM,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true},
	};
	static const u32 c_modelAttrCount = TFE_ARRAYSIZE(c_modelAttrMapping);

	static Shader s_modelShaders[MGPU_SHADER_COUNT];
	static VertexBuffer s_modelVertexBuffer;	// vertex buffer contains vertices for all loaded models.
	static IndexBuffer  s_modelIndexBuffer;		// index buffer for all loaded models.

	static std::vector<ModelVertex> s_vertexData;
	static std::vector<u32> s_indexData;

	static s32 s_mgpu_cameraPosId[MGPU_SHADER_COUNT];
	static s32 s_mgpu_cameraViewId[MGPU_SHADER_COUNT];
	static s32 s_mgpu_cameraProjId[MGPU_SHADER_COUNT];
	static s32 s_mgpu_cameraDirId[MGPU_SHADER_COUNT];
	static s32 s_mgpu_lightDataId[MGPU_SHADER_COUNT];
	static s32 s_mgpu_modelMtxId[MGPU_SHADER_COUNT];
	static s32 s_mgpu_modelPosId[MGPU_SHADER_COUNT];
	static s32 s_mgpu_cameraRightId;

	static ModelGPU s_models[1024];
	static ModelDraw s_modelDrawList[1024];
	static s32 s_modelDrawListCount;
	static s32 s_modelCount;

	extern Mat3  s_cameraMtx;
	extern Mat4  s_cameraProj;
	extern Vec3f s_cameraPos;
	extern Vec3f s_cameraDir;
	extern Vec3f s_cameraRight;

	bool model_init()
	{
		Shader* shader = &s_modelShaders[MGPU_SHADER_HOLOGRAM];
		if (!shader->load("Shaders/gpu_render_modelHologram.vert", "Shaders/gpu_render_modelHologram.frag", 0, nullptr, SHADER_VER_STD))
		{
			return false;
		}
		shader->getVariables();

		s_mgpu_cameraPosId[MGPU_SHADER_HOLOGRAM]  = shader->getVariableId("CameraPos");
		s_mgpu_cameraViewId[MGPU_SHADER_HOLOGRAM] = shader->getVariableId("CameraView");
		s_mgpu_cameraProjId[MGPU_SHADER_HOLOGRAM] = shader->getVariableId("CameraProj");
		s_mgpu_cameraDirId[MGPU_SHADER_HOLOGRAM]  = shader->getVariableId("CameraDir");
		s_mgpu_modelMtxId[MGPU_SHADER_HOLOGRAM]   = shader->getVariableId("ModelMtx");
		s_mgpu_modelPosId[MGPU_SHADER_HOLOGRAM]   = shader->getVariableId("ModelPos");
		s_mgpu_cameraRightId = shader->getVariableId("CameraRight");

		shader->bindTextureNameToSlot("Palette", 0);
		return true;
	}

	void model_destroy()
	{
		for (s32 i = 0; i < MGPU_SHADER_COUNT; i++)
		{
			s_modelShaders[i].destroy();
		}
	}

	void buildModelDrawVertices(JediModel* model, s32* indexStart, s32* vertexStart)
	{
		// In this version, we render one quad per vertex.
		// Store store 4 vertices per quad, but store corner in uv.
		u32 vcount = 4 * model->vertexCount;
		u32 icount = 6 * model->vertexCount;
		u32 vidx = *vertexStart;
		(*vertexStart) += vcount;
		(*indexStart) += icount;

		const size_t curVtxSize = s_vertexData.size();
		const size_t curIdxSize = s_indexData.size();
		s_vertexData.resize(curVtxSize + vcount);
		s_indexData.resize(curIdxSize + icount);

		const vec3* srcVtx = model->vertices;
		const u32 color = model->polygons[0].color;

		ModelVertex* outVtx = s_vertexData.data() + curVtxSize;
		u32* outIdx = s_indexData.data() + curIdxSize;
		for (s32 v = 0; v < model->vertexCount; v++, outVtx += 4, vidx += 4, outIdx += 6)
		{
			Vec3f pos = { fixed16ToFloat(srcVtx[v].x), fixed16ToFloat(srcVtx[v].y), fixed16ToFloat(srcVtx[v].z) };
			Vec3f nrm = { 0 };

			// Build a quad.
			outVtx[0].pos = pos;
			outVtx[0].nrm = nrm;
			outVtx[0].uv = { 0.0f, 1.0f };
			outVtx[0].color = color;

			outVtx[1].pos = pos;
			outVtx[1].nrm = nrm;
			outVtx[1].uv = { 1.0f, 1.0f };
			outVtx[1].color = color;

			outVtx[2].pos = pos;
			outVtx[2].nrm = nrm;
			outVtx[2].uv = { 1.0f, 0.0f };
			outVtx[2].color = color;

			outVtx[3].pos = pos;
			outVtx[3].nrm = nrm;
			outVtx[3].uv = { 0.0f, 0.0f };
			outVtx[3].color = color;

			// Indices.
			outIdx[0] = 0 + vidx;
			outIdx[1] = 1 + vidx;
			outIdx[2] = 2 + vidx;

			outIdx[3] = 0 + vidx;
			outIdx[4] = 2 + vidx;
			outIdx[5] = 3 + vidx;
		}

		s_models[s_modelCount].indexStart = curIdxSize;
		s_models[s_modelCount].polyCount = model->vertexCount * 2;
		s_models[s_modelCount].shader = MGPU_SHADER_HOLOGRAM;
		model->drawId = s_modelCount;
		s_modelCount++;
	}

	void startModel(JediModel* model, s32* indexStart, s32* vertexStart)
	{
	}

	void addFlatPolygon(JmPolygon* poly)
	{
	}

	void addGouraudPolygon(JmPolygon* poly)
	{
	}

	void addTexturePolygon(JmPolygon* poly)
	{
	}

	void addGouraudTexturePolygon(JmPolygon* poly)
	{
	}

	void addPlanePolygon(JmPolygon* poly)
	{
	}

	void model_loadLevelModels()
	{
		s32 indexStart  = 0;
		s32 vertexStart = 0;
		s_vertexData.clear();
		s_indexData.clear();
		s_modelCount = 0;

		std::vector<JediModel*> modelList;
		TFE_Model_Jedi::getModelList(modelList);
		const size_t modelCount = modelList.size();
		JediModel** model = modelList.data();
		for (size_t i = 0; i < modelCount; i++)
		{
			if (model[i]->flags & MFLAG_DRAW_VERTICES)
			{
				buildModelDrawVertices(model[i], &indexStart, &vertexStart);
				continue;
			}

			// This model has solid polygons.
			startModel(model[i], &indexStart, &vertexStart);
			for (s32 p = 0; p < model[i]->polygonCount; p++)
			{
				JmPolygon* poly = &model[i]->polygons[p];
				switch (poly->shading)
				{
					case PSHADE_FLAT:
						addFlatPolygon(poly);
						break;
					case PSHADE_GOURAUD:
						addGouraudPolygon(poly);
						break;
					case PSHADE_TEXTURE:
						addTexturePolygon(poly);
						break;
					case PSHADE_GOURAUD_TEXTURE:
						addGouraudTexturePolygon(poly);
						break;
					case PSHADE_PLANE:
						addPlanePolygon(poly);
						break;
				};
			}
		}

		s_modelVertexBuffer.create(s_vertexData.size(), sizeof(ModelVertex), c_modelAttrCount, c_modelAttrMapping, false, s_vertexData.data());
		s_modelIndexBuffer.create(s_indexData.size(), sizeof(u32), false, s_indexData.data());
	}

	void model_drawListClear()
	{
		s_modelDrawListCount = 0;
	}

	void model_drawListFinish()
	{
		if (!s_modelDrawListCount) { return; }
		// Nothing to do yet...
	}

	void model_add(JediModel* model, Vec3f posWS, fixed16_16* transform)
	{
		// Make sure the model has been assigned a GPU ID.
		if (model->drawId < 0)
		{
			return;
		}

		// TODO: Sort by shader.
		ModelDraw* drawItem = &s_modelDrawList[s_modelDrawListCount];
		s_modelDrawListCount++;

		drawItem->modelId = model->drawId;
		drawItem->posWS = posWS;
		for (s32 i = 0; i < 9; i++)
		{
			drawItem->transform[i] = fixed16ToFloat(transform[i]);
		}
	}

	void model_drawList()
	{
		if (!s_modelDrawListCount) { return; }

		// Setup shader and data, including vertex and index buffers.
		Shader* shader = &s_modelShaders[MGPU_SHADER_HOLOGRAM];
		shader->bind();

		s_modelVertexBuffer.bind();
		s_modelIndexBuffer.bind();

		const TextureGpu* palette = TFE_RenderBackend::getPaletteTexture();
		palette->bind(0);

		// Camera and lighting.
		Vec4f lightData = { f32(s_worldAmbient), s_cameraLightSource ? 1.0f : 0.0f, 0.0f, 0.0f };
		shader->setVariable(s_mgpu_cameraPosId[MGPU_SHADER_HOLOGRAM],  SVT_VEC3,   s_cameraPos.m);
		shader->setVariable(s_mgpu_cameraViewId[MGPU_SHADER_HOLOGRAM], SVT_MAT3x3, s_cameraMtx.data);
		shader->setVariable(s_mgpu_cameraProjId[MGPU_SHADER_HOLOGRAM], SVT_MAT4x4, s_cameraProj.data);
		shader->setVariable(s_mgpu_cameraDirId[MGPU_SHADER_HOLOGRAM],  SVT_VEC3,   s_cameraDir.m);
		shader->setVariable(s_mgpu_cameraRightId,                      SVT_VEC3,   s_cameraRight.m);

		ModelDraw* drawItem = s_modelDrawList;
		for (s32 i = 0; i < s_modelDrawListCount; i++, drawItem++)
		{
			const ModelGPU* model = &s_models[drawItem->modelId];

			shader->setVariable(s_mgpu_modelPosId[MGPU_SHADER_HOLOGRAM], SVT_VEC3,   drawItem->posWS.m);
			shader->setVariable(s_mgpu_modelMtxId[MGPU_SHADER_HOLOGRAM], SVT_MAT3x3, drawItem->transform);
			TFE_RenderBackend::drawIndexedTriangles(model->polyCount, sizeof(u32), model->indexStart);
		}

		// Cleanup
		shader->unbind();
		s_modelVertexBuffer.unbind();
		s_modelIndexBuffer.unbind();
		TextureGpu::clear(0);
	}

	s32 model_getDrawListSize()
	{
		return s_modelDrawListCount;
	}
}
