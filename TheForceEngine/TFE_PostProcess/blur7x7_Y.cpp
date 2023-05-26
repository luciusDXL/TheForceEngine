#include "blur7x7_Y.h"
#include "postprocess.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/profiler.h>

bool Blur7x7_Y::init()
{
	return buildShaders();
}

void Blur7x7_Y::destroy()
{
	m_featureShader.destroy();
}

bool Blur7x7_Y::buildShaders()
{
	// Blur in Y Direction.
	if (!m_featureShader.load("Shaders/blit.vert", "Shaders/blur7x7.frag"))
	{
		return false;
	}
	m_featureShader.bindTextureNameToSlot("VirtualDisplay", 0);

	setupShader();
	return true;
}

void Blur7x7_Y::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);
	setupShader();
}

void Blur7x7_Y::setupShader()
{
	m_shader = &m_featureShader;
}
