#include "blit.h"
#include "postprocess.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <TFE_System/profiler.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <vector>

bool Blit::init()
{
	if (!m_shader.load("Shaders/blit.vert", "Shaders/blit.frag"))
	{
		return false;
	}

	m_shader.bindTextureNameToSlot("VirtualDisplay", 0);
	m_scaleOffsetId = m_shader.getVariableId("ScaleOffset");
	if (m_scaleOffsetId < 0)
	{
		return false;
	}

	return true;
}

void Blit::destroy()
{
	m_shader.destroy();
}

void Blit::execute(TextureGpu* input)
{
	// The vertex and input buffer will already be set at this point.
	TFE_ZONE("Blit To Screen (CPU)");
		
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);

	// Bind Uniforms & Textures.
	input->bind();

	// Draw.
	TFE_PostProcess::drawRectangle();

	// Cleanup.
	input->clear();
}
