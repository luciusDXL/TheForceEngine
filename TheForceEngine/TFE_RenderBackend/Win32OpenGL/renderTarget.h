#pragma once
//////////////////////////////////////////////////////////////////////
// Wrapper for OpenGL textures that expose the limited set of 
// required functionality.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/textureGpu.h>

class RenderTarget
{
public:
	RenderTarget() : m_texture(nullptr), m_gpuHandle(0), m_depthBufferHandle(0) {}
	~RenderTarget();

	bool create(TextureGpu* texture, bool depthBuffer);
	void bind();
	void clear(const f32* color, f32 depth, u8 stencil = 0, bool clearColor = true);
	void clearDepth(f32 depth);
	void clearStencil(u8 stencil);
	static void copy(RenderTarget* dst, RenderTarget* src);
	static void unbind();

	inline const TextureGpu* getTexture() const { return m_texture; }

private:
	const TextureGpu* m_texture;
	u32 m_gpuHandle;
	u32 m_depthBufferHandle;
};
