#include "bloomMerge.h"
#include "postprocess.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/profiler.h>
#include <TFE_Settings/settings.h>

static const f32 c_minSpread = 0.50f;
static const f32 c_maxSpread = 0.875f;

bool BloomMerge::init()
{
	m_bloomSpreadId = -1;
	return buildShaders();
}

void BloomMerge::destroy()
{
	m_shaderInternal.destroy();
}

bool BloomMerge::buildShaders()
{
	// Base shader.
	if (!m_shaderInternal.load("Shaders/bloom.vert", "Shaders/bloomMerge.frag", 0u, nullptr, SHADER_VER_STD))
	{
		return false;
	}
	m_shaderInternal.bindTextureNameToSlot("MergeCur", 0);
	m_shaderInternal.bindTextureNameToSlot("MergePrev", 1);
	m_bloomSpreadId = m_shaderInternal.getVariableId("bloomSpread");
	setupShader();
	return true;
}

void BloomMerge::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);
	if (m_shader)
	{
		const f32 blend = TFE_Settings::getGraphicsSettings()->bloomSpread;
		const f32 bloomSpread = c_minSpread + blend * (c_maxSpread - c_minSpread);
		m_shader->setVariable(m_bloomSpreadId, SVT_SCALAR, &bloomSpread);
	}
}

void BloomMerge::setupShader()
{
	m_shader = &m_shaderInternal;
	m_scaleOffsetId = m_shader->getVariableId("ScaleOffset");
}
