#include "bloomThreshold.h"
#include "postprocess.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/profiler.h>
#include <TFE_Settings/settings.h>

bool BloomThreshold::init()
{
	m_bloomIntensityId = -1;
	return buildShaders();
}

void BloomThreshold::destroy()
{
	m_shaderInternal.destroy();
}

bool BloomThreshold::buildShaders()
{
	// Base shader.
	if (!m_shaderInternal.load("Shaders/bloom.vert", "Shaders/bloomThreshold.frag", 0u, nullptr, SHADER_VER_STD))
	{
		return false;
	}
	m_shaderInternal.bindTextureNameToSlot("ColorBuffer", 0);
	m_shaderInternal.bindTextureNameToSlot("MaterialBuffer", 1);
	m_bloomIntensityId = m_shaderInternal.getVariableId("bloomIntensity");
	setupShader();
	return true;
}

void BloomThreshold::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);
	if (m_shader)
	{
		f32 bloomIntensity = TFE_Settings::getGraphicsSettings()->bloomStrength * 8.0f;
		m_shader->setVariable(m_bloomIntensityId, SVT_SCALAR, &bloomIntensity);
	}
}

void BloomThreshold::setupShader()
{
	m_shader = &m_shaderInternal;
	m_scaleOffsetId = m_shader->getVariableId("ScaleOffset");
}
