#pragma once
//////////////////////////////////////////////////////////////////////
// Wrapper for OpenGL textures that expose the limited set of 
// required functionality.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

enum MagFilter
{
	MAG_FILTER_NONE = 0,
	MAG_FILTER_LINEAR,
};

enum TexFormat
{
	TEX_RGBA8 = 0,
	TEX_R8,
	TEX_COUNT
};

class TextureGpu
{
public:
	TextureGpu() : m_width(0), m_height(0), m_channels(4), m_gpuHandle(0) {}
	~TextureGpu();

	bool create(u32 width, u32 height, u32 channels = 4);
	bool createWithData(u32 width, u32 height, const void* buffer, MagFilter magFilter = MAG_FILTER_NONE);
	bool update(const void* buffer, size_t size);
	void bind(u32 slot = 0) const;
	static void clear(u32 slot = 0);

	u32  getWidth() const { return m_width; }
	u32  getHeight() const { return m_height; }

	inline u32 getHandle() const { return m_gpuHandle; }

private:
	u32 m_width;
	u32 m_height;
	u32 m_channels;
	u32 m_gpuHandle;
};
