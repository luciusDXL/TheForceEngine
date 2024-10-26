#include "bloomDownsample.h"
#include "postprocess.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/profiler.h>

bool BloomDownsample::init()
{
	return buildShaders();
}

void BloomDownsample::destroy()
{
	m_shaderInternal.destroy();
}

bool BloomDownsample::buildShaders()
{
	// Base shader.
	if (!m_shaderInternal.load("Shaders/bloom.vert", "Shaders/bloomDownsample.frag", 0u, nullptr, SHADER_VER_STD))
	{
		return false;
	}
	m_shaderInternal.bindTextureNameToSlot("ColorBuffer", 0);
	setupShader();
	return true;
}

void BloomDownsample::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);
}

void BloomDownsample::setupShader()
{
	m_shader = &m_shaderInternal;
	m_scaleOffsetId = m_shader->getVariableId("ScaleOffset");
}
