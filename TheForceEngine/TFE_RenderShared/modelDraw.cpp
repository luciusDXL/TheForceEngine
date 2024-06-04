#include "modelDraw.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>
#include <vector>

namespace TFE_RenderShared
{
	enum ModelDrawCmd : u32
	{
		MCMD_SET_MODEL = 0u,
		MCMD_DRAW_MTL,
		MCMD_COUNT
	};

	struct ModelInfo
	{
		ModelDrawCmd cmd;

		const VertexBuffer* vtx;
		const IndexBuffer* idx;
		Vec3f pos;
		Mat3 xform;
		Vec4f tint;
	};
	struct ModelDraw
	{
		ModelDrawCmd cmd;

		ModelDrawMode mode;
		u32 idxStart;
		u32 idxCount;
		TextureGpu* tex;
		TexProjection proj;
	};
	
	struct ShaderStateMDRAW
	{
		s32 svCameraPos = -1;
		s32 svCameraView = -1;
		s32 svCameraProj = -1;
		s32 svIsTextured = -1;
		s32 svPos = -1;
		s32 svLocalToWorld = -1;
		s32 svTint = -1;
		s32 svProj = -1;
		s32 svOffset = -1;
	};

	static std::vector<u8> s_cmdBuffer;
	static ShaderStateMDRAW s_shaderState;
	static Shader s_shader;

	bool modelDraw_loadShader(u32 defineCount, ShaderDefine* defines)
	{
		if (!s_shader.load("Shaders/mdl3d.vert", "Shaders/mdl3d.frag", defineCount, defines))
		{
			return false;
		}
		s_shader.bindTextureNameToSlot("Image", 0);

		s_shaderState.svCameraPos = s_shader.getVariableId("CameraPos");
		s_shaderState.svCameraView = s_shader.getVariableId("CameraView");
		s_shaderState.svCameraProj = s_shader.getVariableId("CameraProj");
		s_shaderState.svIsTextured = s_shader.getVariableId("isTexturedProj");

		s_shaderState.svPos = s_shader.getVariableId("ObjPos");
		s_shaderState.svLocalToWorld = s_shader.getVariableId("ObjTransform");
		s_shaderState.svTint = s_shader.getVariableId("tint");
		s_shaderState.svProj = s_shader.getVariableId("texProj");
		s_shaderState.svOffset = s_shader.getVariableId("texOffset");

		if (s_shaderState.svCameraPos < 0 || s_shaderState.svCameraView < 0 || s_shaderState.svCameraProj < 0 ||
			s_shaderState.svPos < 0 || s_shaderState.svLocalToWorld < 0)
		{
			return false;
		}
		return true;
	}
	
	bool modelDraw_init()
	{
		if (!modelDraw_loadShader(0, nullptr)) { return false; }
		// Rough size estimate for typical scenes,
		// the array will be expanded as needed.
		s_cmdBuffer.reserve((sizeof(ModelInfo) + sizeof(ModelDraw) * 10) * 100);
		return true;
	}

	void modelDraw_destroy()
	{
		s_cmdBuffer.clear();
		s_cmdBuffer.shrink_to_fit();
		s_shader.destroy();
	}
	
	void modelDraw_begin()
	{
		// This will set the size to 0, but not free the memory.
		// So once a maximal size is reached for a scene, addition memory
		// allocations will not be needed.
		s_cmdBuffer.clear();
	}

	void modelDraw_addModel(const VertexBuffer* vtx, const IndexBuffer* idx, const Vec3f& pos, const Mat3& xform, const Vec4f tint)
	{
		const size_t bufferPtr = s_cmdBuffer.size();
		s_cmdBuffer.resize(s_cmdBuffer.size() + sizeof(ModelInfo));
		ModelInfo* info = (ModelInfo*)(s_cmdBuffer.data() + bufferPtr);

		info->cmd = MCMD_SET_MODEL;
		info->vtx = vtx;
		info->idx = idx;
		info->pos = pos;
		info->xform = xform;
		info->tint = tint;
	}

	void modelDraw_addMaterial(ModelDrawMode mode, u32 idxStart, u32 idxCount, TextureGpu* tex, const TexProjection proj)
	{
		const size_t bufferPtr = s_cmdBuffer.size();
		s_cmdBuffer.resize(s_cmdBuffer.size() + sizeof(ModelDraw));
		ModelDraw* draw = (ModelDraw*)(s_cmdBuffer.data() + bufferPtr);
		draw->cmd = MCMD_DRAW_MTL;
		draw->mode = mode;
		draw->idxStart = idxStart;
		draw->idxCount = idxCount;
		draw->tex = tex;
		draw->proj = proj;
	}

	bool modelDraw_draw(const Camera3d* camera, f32 width, f32 height, bool enableCulling)
	{
		if (s_cmdBuffer.empty()) { return true; }

		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE | STATE_DEPTH_TEST);
		TFE_RenderState::setStateEnable(enableCulling, STATE_CULLING);
		TFE_RenderState::setDepthFunction(CMP_LEQUAL);
		TFE_RenderState::setBlendMode(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA);

		s_shader.bind();
		s_shader.setVariable(s_shaderState.svCameraPos, SVT_VEC3, camera->pos.m);
		s_shader.setVariable(s_shaderState.svCameraView, SVT_MAT3x3, camera->viewMtx.data);
		s_shader.setVariable(s_shaderState.svCameraProj, SVT_MAT4x4, camera->projMtx.data);

		const size_t cmdBufferSize = s_cmdBuffer.size();
		const u8* cmdData = s_cmdBuffer.data();
		size_t cmdBufferIdx = 0;
		bool renderSuccess = true;
		for (; cmdBufferIdx < cmdBufferSize;)
		{
			// read the command.
			u32 cmd = *((u32*)(cmdData + cmdBufferIdx));
			switch (cmd)
			{
				case MCMD_SET_MODEL:
				{
					ModelInfo* info = (ModelInfo*)(cmdData + cmdBufferIdx);
					cmdBufferIdx += sizeof(ModelInfo);

					info->vtx->bind();
					info->idx->bind();
					s_shader.setVariable(s_shaderState.svPos, SVT_VEC3, info->pos.m);
					s_shader.setVariable(s_shaderState.svLocalToWorld, SVT_MAT3x3, info->xform.data);
					s_shader.setVariable(s_shaderState.svTint, SVT_VEC4, info->tint.m);

					// Enable alpha blending if the model has opacity set.
					TFE_RenderState::setStateEnable(info->tint.w < 1.0f, STATE_BLEND);
				} break;
				case MCMD_DRAW_MTL:
				{
					const ModelDraw* draw = (ModelDraw*)(cmdData + cmdBufferIdx);
					cmdBufferIdx += sizeof(ModelDraw);

					const s32 isTexturedProj[] = { draw->tex && draw->mode != MDLMODE_COLORED ? 1 : 0, draw->tex && draw->mode == MDLMODE_TEX_PROJ ? 1 : 0 };
					s_shader.setVariable(s_shaderState.svIsTextured, SVT_IVEC2, isTexturedProj);
					if (isTexturedProj[1])
					{
						s_shader.setVariable(s_shaderState.svProj, SVT_VEC4, draw->proj.plane.m);
						s_shader.setVariable(s_shaderState.svOffset, SVT_VEC2, draw->proj.offset.m);
					}

					if (draw->tex)
					{
						draw->tex->bind(0);
					}

					TFE_RenderBackend::drawIndexedTriangles(draw->idxCount / 3, sizeof(u32), draw->idxStart);
				} break;
				default:
				{
					// This should never happen, if it does "panic" and stop trying to render to avoid getting stuck
					// in an infinite loop.
					assert(0);	// assert to catch in the debugger.
					// Return failure to the client so it can be logged. This should not crash or block in release.
					renderSuccess = false;
					break;
				}
			}
		}

		// Cleanup.
		s_shader.unbind();
		TFE_RenderState::setDepthBias();
		return renderSuccess;
	}
}
