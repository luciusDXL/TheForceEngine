#pragma once
//////////////////////////////////////////////////////////////////////
// GPU Based blit.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/shader.h>

class TextureGpu;

class PostProcessEffect
{
public:
	// Load shaders, setup geometry.
	virtual bool init() = 0;
	// Free GPU assets.
	virtual void destroy() = 0;

	// Execute the post process.
	// input: Input image.
	virtual void execute(const TextureGpu* input) = 0;

public:
	Shader m_shader;
	s32 m_scaleOffsetId;
};
