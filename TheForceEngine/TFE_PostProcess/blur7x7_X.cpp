#include "blur7x7_X.h"
#include "postprocess.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/profiler.h>

bool Blur7x7_X::init()
{
	return buildShaders();
}

void Blur7x7_X::destroy()
{
	m_featureShader.destroy();
}

bool Blur7x7_X::buildShaders()
{
	ShaderDefine defines[2];
	// Blur in X Direction.
	defines[0] = { "BLUR_DIR_X", "1" };
	if (!m_featureShader.load("Shaders/blit.vert", "Shaders/blur7x7.frag", 1, defines))
	{
		return false;
	}
	m_featureShader.bindTextureNameToSlot("VirtualDisplay", 0);

	setupShader();
	return true;
}

void Blur7x7_X::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);
	setupShader();
}

void Blur7x7_X::setupShader()
{
	m_shader = &m_featureShader;
}
