#pragma once
//////////////////////////////////////////////////////////////////////
// GPU Based blit.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "postprocesseffect.h"

class Blit : public PostProcessEffect
{
public:
	// Load shaders, setup geometry.
	bool init() override;
	// Free GPU assets.
	void destroy() override;

	// Execute the post process.
	// input: Input image.
	void execute(const TextureGpu* input) override;
};
