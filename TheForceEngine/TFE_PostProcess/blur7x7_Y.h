#pragma once
//////////////////////////////////////////////////////////////////////
// GPU Based blit.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "postprocesseffect.h"

class Blur7x7_Y : public PostProcessEffect
{
public:
	// Load shaders, setup geometry.
	bool init() override;
	// Free GPU assets.
	void destroy() override;

	// Execute the post process.
	void setEffectState() override;

private:
	Shader m_featureShader;

	void setupShader();
	bool buildShaders();
};
