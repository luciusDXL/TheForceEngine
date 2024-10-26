#include "blit.h"
#include "postprocess.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/profiler.h>

bool Blit::init()
{
	bool res = buildShaders();
	enableFeatures(0);

	m_colorCorrectParam = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_colorCorrectVarId = -1;

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
	// BLIT_GPU_COLOR_CONVERSION feature
	defines[0] = { "ENABLE_GPU_COLOR_CONVERSION", "1" };
	m_featureShaders[1].load("Shaders/blit.vert", "Shaders/blit.frag", 1, defines);
	m_featureShaders[1].bindTextureNameToSlot("VirtualDisplay", 0);
	m_featureShaders[1].bindTextureNameToSlot("Palette", 1);

	// BLIT_GPU_COLOR_CORRECTION feature
	defines[0] = { "ENABLE_GPU_COLOR_CORRECTION", "1" };
	m_featureShaders[2].load("Shaders/blit.vert", "Shaders/blit.frag", 1, defines);
	m_featureShaders[2].bindTextureNameToSlot("VirtualDisplay", 0);

	// BLIT_GPU_COLOR_CONVERSION + BLIT_GPU_COLOR_CORRECTION features
	defines[0] = { "ENABLE_GPU_COLOR_CONVERSION", "1" };
	defines[1] = { "ENABLE_GPU_COLOR_CORRECTION", "1" };
	m_featureShaders[3].load("Shaders/blit.vert", "Shaders/blit.frag", 2, defines);
	m_featureShaders[3].bindTextureNameToSlot("VirtualDisplay", 0);
	m_featureShaders[3].bindTextureNameToSlot("Palette", 1);


	// BLIT_BLOOM feature
	defines[0] = { "ENABLE_BLOOM", "1" };
	m_featureShaders[4].load("Shaders/blit.vert", "Shaders/blit.frag", 1, defines);
	m_featureShaders[4].bindTextureNameToSlot("VirtualDisplay", 0);
	m_featureShaders[4].bindTextureNameToSlot("Bloom", 1);

	// BLIT_BLOOM + BLIT_GPU_COLOR_CORRECTION feature
	defines[0] = { "ENABLE_BLOOM", "1" };
	defines[1] = { "ENABLE_GPU_COLOR_CORRECTION", "1" };
	m_featureShaders[5].load("Shaders/blit.vert", "Shaders/blit.frag", 2, defines);
	m_featureShaders[5].bindTextureNameToSlot("VirtualDisplay", 0);
	m_featureShaders[5].bindTextureNameToSlot("Bloom", 1);

	return true;
}

void Blit::setEffectState()
{
	TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_BLEND | STATE_DEPTH_TEST);

	if (m_features & BLIT_GPU_COLOR_CORRECTION)
	{
		m_shader->setVariable(m_colorCorrectVarId, SVT_VEC4, m_colorCorrectParam.m);
	}
}

void Blit::setupShader()
{
	m_shader = &m_featureShaders[0];
	if ((m_features & BLIT_GPU_COLOR_CONVERSION) && (m_features & BLIT_GPU_COLOR_CORRECTION))
	{
		m_shader = &m_featureShaders[3];
	}
	else if (m_features & BLIT_GPU_COLOR_CONVERSION)
	{
		m_shader = &m_featureShaders[1];
	}
	else if ((m_features & BLIT_GPU_COLOR_CORRECTION) && (m_features & BLIT_BLOOM))
	{
		m_shader = &m_featureShaders[5];
	}
	else if (m_features & BLIT_GPU_COLOR_CORRECTION)
	{
		m_shader = &m_featureShaders[2];
	}
	else if (m_features & BLIT_BLOOM)
	{
		m_shader = &m_featureShaders[4];
	}
	m_scaleOffsetId = m_shader->getVariableId("ScaleOffset");

	if (m_features & BLIT_GPU_COLOR_CORRECTION)
	{
		m_colorCorrectVarId = m_shader->getVariableId("ColorCorrectParam");
	}
}

void Blit::enableFeatures(u32 features)
{
	m_features |= features;
	setupShader();
}

void Blit::disableFeatures(u32 features)
{
	m_features &= ~features;
	setupShader();
}

bool Blit::featureEnabled(u32 feature) const
{
	return (m_features & feature) != 0;
}

void Blit::setColorCorrectionParameters(const ColorCorrection* parameters)
{
	m_colorCorrectParam.x = parameters->brightness;
	m_colorCorrectParam.y = parameters->contrast;
	m_colorCorrectParam.z = parameters->saturation;
	m_colorCorrectParam.w = parameters->gamma;
}
