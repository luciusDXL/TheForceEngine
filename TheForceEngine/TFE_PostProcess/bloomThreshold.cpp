#include "bloomThreshold.h"
#include "postprocess.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/profiler.h>

bool BloomThreshold::init()
{
	return buildShaders();
}

void BloomThreshold::destroy()
{
	m_featureShader.destroy();
}

bool BloomThreshold::buildShaders()
{
	// Base shader.
	if (!m_featureShader.load("Shaders/blit.vert", "Shaders/bloomDownsample1.frag"))
	{
		return false;
	}
	m_featureShader.bindTextureNameToSlot("VirtualDisplay", 0);

	setupShader();
	return true;
}

void BloomThreshold::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);
	setupShader();
}

void BloomThreshold::setupShader()
{
	m_shader = &m_featureShader;
}
