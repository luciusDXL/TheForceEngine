#include "../dynamicTexture.h"
#include <GL/glew.h>
#include <assert.h>

std::vector<u8> DynamicTexture::s_tempBuffer;

DynamicTexture::~DynamicTexture()
{
	freeBuffers();
}

bool DynamicTexture::create(u32 width, u32 height, u32 bufferCount, DynamicTexFormat format/* = DTEX_RGBA8*/)
{
	m_width = width;
	m_height = height;
	m_format = format;
	return changeBufferCount(bufferCount);
}

void DynamicTexture::resize(u32 newWidth, u32 newHeight)
{
	if (newWidth == m_width && newHeight == m_height) { return; }

	m_width = newWidth;
	m_height = newHeight;
	changeBufferCount(m_bufferCount, true);
}

bool DynamicTexture::changeBufferCount(u32 newBufferCount, bool forceRealloc/* = false*/)
{
	if (newBufferCount == m_bufferCount && !forceRealloc) { return false; }
	freeBuffers();

	m_bufferCount = newBufferCount;
	m_readBuffer  = 0;
	m_writeBuffer = m_bufferCount - 1;

	const size_t bufferSize = m_width * m_height * (m_format == DTEX_RGBA8 ? 4 : 1);
	s_tempBuffer.resize(bufferSize);
	memset(s_tempBuffer.data(), 0, bufferSize);

	m_textures = new TextureGpu*[m_bufferCount];
	for (u32 i = 0; i < m_bufferCount; i++)
	{
		m_textures[i] = new TextureGpu();
		// Create the texture itself.
		m_textures[i]->create(m_width, m_height);
		// Clear each buffer to avoid garbage as the buffers are used.
		m_textures[i]->update(s_tempBuffer.data(), bufferSize);
	}

	return m_textures && m_bufferCount;
}

void DynamicTexture::update(const void* imageData, size_t size)
{
	// copy imageData to [m_writeBuffer]
	m_textures[m_writeBuffer]->update(imageData, size);

	// update buffer indices.
	m_writeBuffer = (m_writeBuffer + 1) % m_bufferCount;
	m_readBuffer = (m_readBuffer + 1) % m_bufferCount;
}

void DynamicTexture::freeBuffers()
{
	for (u32 i = 0; i < m_bufferCount; i++)
	{
		delete m_textures[i];
	}
	delete[] m_textures;
	m_bufferCount = 0;
}
