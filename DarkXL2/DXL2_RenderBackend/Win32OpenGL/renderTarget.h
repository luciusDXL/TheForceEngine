#pragma once
//////////////////////////////////////////////////////////////////////
// Wrapper for OpenGL textures that expose the limited set of 
// required functionality.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_RenderBackend/textureGpu.h>

class RenderTarget
{
public:
	RenderTarget() : m_texture(nullptr), m_gpuHandle(0), m_depthBufferHandle(0) {}
	~RenderTarget();

	bool create(TextureGpu* texture, bool depthBuffer);
	void bind();
	void clear(const f32* color, f32 depth);
	static void unbind();

	inline const TextureGpu* getTexture() const { return m_texture; }

private:
	const TextureGpu* m_texture;
	u32 m_gpuHandle;
	u32 m_depthBufferHandle;
};
