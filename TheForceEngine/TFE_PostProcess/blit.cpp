#include "blit.h"
#include "postprocess.h"
#include <TFE_System/profiler.h>

bool Blit::init()
{
	return buildShader();
}

void Blit::destroy()
{
	m_shader.destroy();
}

bool Blit::buildShader()
{
	ShaderDefine defines[BLIT_FEATURE_COUNT];
	u32 defineCount = 0;
	if (m_features & BLIT_GPU_COLOR_CONVERSION)
	{
		defines[defineCount++] = {"ENABLE_GPU_COLOR_CONVERSION", "1"};
	}

	if (!m_shader.load("Shaders/blit.vert", "Shaders/blit.frag", defineCount, defines))
	{
		return false;
	}

	m_shader.bindTextureNameToSlot("VirtualDisplay", 0);
	m_scaleOffsetId = m_shader.getVariableId("ScaleOffset");
	if (m_scaleOffsetId < 0)
	{
		return false;
	}

	if (m_features & BLIT_GPU_COLOR_CONVERSION)
	{
		m_shader.bindTextureNameToSlot("Palette", 1);
	}

	return true;
}

void Blit::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);
}

void Blit::enableFeatures(u32 features)
{
	if (!features) { return; }
	m_features |= features;
	buildShader();
}

void Blit::disableFeatures(u32 features)
{
	if (!features) { return; }
	m_features &= ~features;
	buildShader();
}
