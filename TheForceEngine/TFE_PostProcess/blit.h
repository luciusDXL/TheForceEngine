#pragma once
//////////////////////////////////////////////////////////////////////
// GPU Based blit.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "postprocesseffect.h"

enum BlitFeature
{
	BLIT_GPU_COLOR_CONVERSION = (1 << 0),
	BLIT_GPU_COLOR_CORRECTION = (1 << 1),

	BLIT_FEATURE_COUNT = 2,
	BLIT_FEATURE_COMBO_COUNT = 4,
};

struct ColorCorrection;

class Blit : public PostProcessEffect
{
public:
	// Load shaders, setup geometry.
	bool init() override;
	// Free GPU assets.
	void destroy() override;

	// Execute the post process.
	void setEffectState() override;

	// Enable various features.
	void enableFeatures(u32 features);
	void disableFeatures(u32 features);
	bool featureEnabled(u32 feature) const;

	void setColorCorrectionParameters(const ColorCorrection* parameters);

private:
	u32 m_features = 0;
	Shader m_featureShaders[BLIT_FEATURE_COMBO_COUNT];
	Vec4f m_colorCorrectParam;
	s32   m_colorCorrectVarId;

	void setupShader();
	bool buildShaders();
};
