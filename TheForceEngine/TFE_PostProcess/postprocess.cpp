#include <cstring>

#include "postprocess.h"
#include "postprocesseffect.h"
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/profiler.h>
#include <assert.h>
#include <vector>

namespace TFE_PostProcess
{
	enum EffectFlags
	{
		EFLAG_NONE = 0,
		EFLAG_USE_DYNAMIC_INPUT = (1 << 0),
	};

	struct PostEffectInstance
	{
		PostProcessEffect* effect;
		u32 flags;
		u32 inputCount;
		PostEffectInput* inputs;
		RenderTargetHandle output;
		s32 x;
		s32 y;
		s32 width;
		s32 height;
	};

	struct EffectVertex
	{
		f32 x, y;
		f32 u, v;
	};
	static const AttributeMapping c_effectAttrMapping[] =
	{
		{ATTR_POS, ATYPE_FLOAT, 2, 0, false},
		{ATTR_UV,  ATYPE_FLOAT, 2, 0, false},
	};
	static const u32 c_effectAttrCount = TFE_ARRAYSIZE(c_effectAttrMapping);

	static VertexBuffer s_vertexBuffer;
	static IndexBuffer s_indexBuffer;
	static f32 s_screenScale[2];
	static std::vector<PostEffectInstance> s_effects;
		
	////////////////////////////////////////////////
	// Forward Declarations
	////////////////////////////////////////////////
	void updateRenderTargetScale(RenderTargetHandle renderTarget);
	void setScaleOffset(Shader* shader, s32 variableId, s32 x, s32 y, s32 width, s32 height);

	////////////////////////////////////////////////
	// Implementation
	////////////////////////////////////////////////
	bool init()
	{
		// Upload vertex/index buffers
		const EffectVertex vertices[] =
		{
			{0.0f, 0.0f, 0.0f, 1.0f},
			{1.0f, 0.0f, 1.0f, 1.0f},
			{1.0f, 1.0f, 1.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
		};
		const u16 indices[] =
		{
			0, 1, 2,
			0, 2, 3
		};
		s_vertexBuffer.create(4, sizeof(EffectVertex), c_effectAttrCount, c_effectAttrMapping, false, (void*)vertices);
		s_indexBuffer.create(6, sizeof(u16), false, (void*)indices);

		return true;
	}

	void destroy()
	{
		clearEffectStack();
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
	}

	void clearEffectStack()
	{
		s_effects.clear();
	}

	void appendEffect(PostProcessEffect* effect, u32 inputCount, const PostEffectInput* inputs, RenderTargetHandle output, s32 x, s32 y, s32 width, s32 height)
	{
		PostEffectInstance instance =
		{
			effect,
			0,
			inputCount,
			nullptr,
			output,
			x, y,
			width, height
		};
		instance.inputs = new PostEffectInput[inputCount];
		memcpy(instance.inputs, inputs, inputCount * sizeof(PostEffectInput));

		s_effects.push_back(instance);
	}

	void execute()
	{
		const size_t count = s_effects.size();
		if (!count) { return; }
		TFE_ZONE("Post Process");

		RenderTargetHandle curRenderTarget = nullptr;
		updateRenderTargetScale(curRenderTarget);

		s_vertexBuffer.bind();
		s_indexBuffer.bind();

		PostEffectInstance* effectInst = s_effects.data();
		for (size_t i = 0; i < count; i++, effectInst++)
		{
			PostProcessEffect* effect = effectInst->effect;
			Shader* shader = effect->m_shader;
			shader->bind();

			if (curRenderTarget != effectInst->output)
			{
				curRenderTarget = effectInst->output;
				updateRenderTargetScale(curRenderTarget);
			}
			setScaleOffset(shader, effect->m_scaleOffsetId, effectInst->x, effectInst->y, effectInst->width, effectInst->height);

			// Set effect output.
			if (effectInst->output)
				TFE_RenderBackend::bindRenderTarget(effectInst->output);
			else
				TFE_RenderBackend::unbindRenderTarget();

			// Bind inputs.
			for (u32 i = 0; i < effectInst->inputCount; i++)
			{
				if (!effectInst->inputs[i].ptr) { continue; }

				if (effectInst->inputs[i].type == PTYPE_TEXTURE) { effectInst->inputs[i].tex->bind(i); }
				else if (effectInst->inputs[i].type == PTYPE_DYNAMIC_TEX) { effectInst->inputs[i].dyntex->bind(i); }
			}

			// Setup any effect specific state.
			effect->setEffectState();

			// Draw the rectangle.
			drawRectangle();

			// Cleanup
			for (u32 i = 0; i < effectInst->inputCount; i++)
			{
				TextureGpu::clear(i);
			}
		}

		Shader::unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();
		TFE_RenderBackend::unbindRenderTarget();
	}
	
	void drawRectangle()
	{
		TFE_RenderBackend::drawIndexedTriangles(2, sizeof(u16));
	}

	////////////////////////////////////////////////
	// Internal
	////////////////////////////////////////////////
	void updateRenderTargetScale(RenderTargetHandle renderTarget)
	{
		if (renderTarget)
		{
			u32 w, h;
			TFE_RenderBackend::getRenderTargetDim(renderTarget, &w, &h);
			s_screenScale[0] = 1.0f / f32(w);
			s_screenScale[1] = 1.0f / f32(h);
		}
		else
		{
			DisplayInfo display;
			TFE_RenderBackend::getDisplayInfo(&display);
			s_screenScale[0] = 1.0f / f32(display.width);
			s_screenScale[1] = 1.0f / f32(display.height);
		}
	}

	void setScaleOffset(Shader* shader, s32 variableId, s32 x, s32 y, s32 width, s32 height)
	{
		const f32 scaleX = 2.0f * f32(width)  * s_screenScale[0];
		const f32 scaleY = 2.0f * f32(height) * s_screenScale[1];
		const f32 offsetX = 2.0f * f32(x) * s_screenScale[0] - 1.0f;
		const f32 offsetY = 2.0f * f32(y) * s_screenScale[1] - 1.0f;
		const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };

		shader->setVariable(variableId, SVT_VEC4, scaleOffset);
	}
}