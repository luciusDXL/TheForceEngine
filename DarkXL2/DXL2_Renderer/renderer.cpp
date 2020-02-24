#include <DXL2_RenderBackend/renderBackend.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_Renderer/DXL2_SoftRenderCPU/renderer_softCPU.h>
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

DXL2_Renderer* DXL2_Renderer::create(RendererType type)
{
	if (type == DXL2_RENDERER_SOFTWARE_CPU)
	{
		return new DXL2_SoftRenderCPU();
	}

	// No valid renderer found.
	assert(0);
	return nullptr;
}

void DXL2_Renderer::destroy(DXL2_Renderer* renderer)
{
	assert(renderer);

	delete renderer;
}
