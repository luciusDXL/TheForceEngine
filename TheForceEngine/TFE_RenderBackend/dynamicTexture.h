#pragma once
//////////////////////////////////////////////////////////////////////
// Dynamic multi-buffered texture.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <vector>

enum DynamicTexFormat
{
	DTEX_RGBA8 = 0,
	DTEX_R8,
	DTEX_COUNT
};

class DynamicTexture
{
public:
	DynamicTexture() : m_bufferCount(0), m_readBuffer(0), m_writeBuffer(0), m_format(DTEX_RGBA8), m_textures(nullptr), m_stagingBuffers(nullptr) {}
	~DynamicTexture();

	bool create(u32 width, u32 height, u32 bufferCount, DynamicTexFormat format = DTEX_RGBA8);
	void resize(u32 newWidth, u32 newHeight);
	bool changeBufferCount(u32 newBufferCount, bool forceRealloc=false);

	void update(const void* imageData, size_t size);

	inline const TextureGpu* getTexture() const { return m_textures[m_readBuffer]; }
	inline u32 getWidth() const { return m_width; }
	inline u32 getHeight() const { return m_height; }

private:
	void freeBuffers();

	u32 m_bufferCount;
	u32 m_readBuffer;
	u32 m_writeBuffer;
	u32 m_width;
	u32 m_height;
	DynamicTexFormat m_format;

	TextureGpu** m_textures;
	u32* m_stagingBuffers;

	static std::vector<u8> s_tempBuffer;
};
