#pragma once
//////////////////////////////////////////////////////////////////////
// TFE Post Process System
// Post Process effects are appended and executing in order.
// Effects can have an input image and output render target and can
// be chained together.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/dynamicTexture.h>

class PostProcessEffect;

namespace TFE_PostProcess
{
	bool init();
	void destroy();

	void clearEffectStack();
	void appendEffect(PostProcessEffect* effect, TextureGpu* input, RenderTargetHandle output = nullptr, s32 x = 0, s32 y = 0, s32 width = 0, s32 height = 0);
	void appendEffect(PostProcessEffect* effect, DynamicTexture* input, RenderTargetHandle output = nullptr, s32 x = 0, s32 y = 0, s32 width = 0, s32 height = 0);

	void execute();

	// Internal
	void drawRectangle();
}
