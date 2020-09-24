#include "blit.h"
#include "postprocess.h"
#include <TFE_System/profiler.h>

bool Blit::init()
{
	bool res = buildShaders();
	enableFeatures(0);
	return res;
}

void Blit::destroy()
{
	for (u32 i = 0; i < BLIT_FEATURE_COMBO_COUNT; i++)
	{
		m_featureShaders[i].destroy();
	}
}

bool Blit::buildShaders()
{
	// Base shader.
	m_featureShaders[0].load("Shaders/blit.vert", "Shaders/blit.frag");
	m_featureShaders[0].bindTextureNameToSlot("VirtualDisplay", 0);

	ShaderDefine defines[BLIT_FEATURE_COUNT];
	defines[0] = { "ENABLE_GPU_COLOR_CONVERSION", "1" };
	// BLIT_GPU_COLOR_CONVERSION feature
	m_featureShaders[1].load("Shaders/blit.vert", "Shaders/blit.frag", 1, defines);
	m_featureShaders[1].bindTextureNameToSlot("VirtualDisplay", 0);
	m_featureShaders[1].bindTextureNameToSlot("Palette", 1);

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
	
	m_shader = &m_featureShaders[0];
	if (m_features & BLIT_GPU_COLOR_CONVERSION)
	{
		m_shader = &m_featureShaders[1];
	}
	m_scaleOffsetId = m_shader->getVariableId("ScaleOffset");
}

void Blit::disableFeatures(u32 features)
{
	if (!features) { return; }
	m_features &= ~features;

	m_shader = &m_featureShaders[0];
	if (m_features & BLIT_GPU_COLOR_CONVERSION)
	{
		m_shader = &m_featureShaders[1];
	}
	m_scaleOffsetId = m_shader->getVariableId("ScaleOffset");
}
