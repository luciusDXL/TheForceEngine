#pragma once
//////////////////////////////////////////////////////////////////////
// GPU Based blit.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "postprocesseffect.h"

enum BlitFeature
{
	BLIT_GPU_COLOR_CONVERSION = (1 << 0),

	BLIT_FEATURE_COUNT = 1
};

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

private:
	u32 m_features = 0;

	bool buildShader();
};
