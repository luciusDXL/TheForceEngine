#include "overlay.h"
#include "postprocess.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/profiler.h>

bool Overlay::init()
{
	return buildShaders();
}

void Overlay::destroy()
{
	m_overlayShader.destroy();
}

bool Overlay::buildShaders()
{
	// Base shader.
	m_overlayShader.load("Shaders/overlay.vert", "Shaders/overlay.frag");
	m_overlayShader.bindTextureNameToSlot("Image", 0);
	m_scaleOffsetId = m_overlayShader.getVariableId("ScaleOffset");
	m_tintId = m_overlayShader.getVariableId("Tint");
	m_uvOffsetSizeId = m_overlayShader.getVariableId("UvOffsetSize");

	m_shader = &m_overlayShader;
	return true;
}

void Overlay::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST);
	TFE_RenderState::setStateEnable(true, STATE_BLEND);
	TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);
}
