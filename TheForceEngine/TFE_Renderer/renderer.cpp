#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Renderer/renderer.h>
#include <TFE_Renderer/TFE_SoftRenderCPU/renderer_softCPU.h>
#include <TFE_System/system.h>
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

TFE_Renderer* TFE_Renderer::create(RendererType type)
{
	TFE_System::logWrite(LOG_MSG, "Startup", "TFE_Renderer::create - type = %d", type);
	if (type == TFE_RENDERER_SOFTWARE_CPU)
	{
		return new TFE_SoftRenderCPU();
	}

	// No valid renderer found.
	assert(0);
	return nullptr;
}

void TFE_Renderer::destroy(TFE_Renderer* renderer)
{
	assert(renderer);

	delete renderer;
}
