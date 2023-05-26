#include <TFE_RenderBackend/textureGpu.h>
#include <GL/glew.h>
#include <vector>
#include <assert.h>

static std::vector<u8> s_workBuffer;
const GLenum c_channelFormat[] = { 0, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, 0, GL_UNSIGNED_INT };

TextureGpu::~TextureGpu()
{
	if (m_gpuHandle)
	{
		glDeleteTextures(1, &m_gpuHandle);
		m_gpuHandle = 0;

		// Catch GL errors up to this point.
		glGetError();
	}
}

bool TextureGpu::create(u32 width, u32 height, u32 channels, bool hasMipmaps, MagFilter magFilter)
{
	m_width = width;
	m_height = height;
	m_channels = channels;
	m_bytesPerChannel = 1;
	m_layers = 1;

	glGenTextures(1, &m_gpuHandle);
	if (!m_gpuHandle) { return false; }

	glBindTexture(GL_TEXTURE_2D, m_gpuHandle);
	if (channels == 1)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, c_channelFormat[m_bytesPerChannel], nullptr);
	}
	else if (channels == 4)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, c_channelFormat[m_bytesPerChannel], nullptr);
	}
	assert(glGetError() == GL_NO_ERROR);
	if (hasMipmaps)
	{
		// Allocate memory for the mipmaps.
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, hasMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter == MAG_FILTER_LINEAR ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

bool TextureGpu::create(u32 width, u32 height, u32 channels, u32 bytesPerChannel)
{
	m_width = width;
	m_height = height;
	m_channels = channels;
	m_bytesPerChannel = bytesPerChannel;
	m_layers = 1;

	glGenTextures(1, &m_gpuHandle);
	if (!m_gpuHandle) { return false; }

	glBindTexture(GL_TEXTURE_2D, m_gpuHandle);
	if (channels == 1)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, c_channelFormat[bytesPerChannel], nullptr);
	}
	else if (channels == 4)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, c_channelFormat[bytesPerChannel], nullptr);
	}
	assert(glGetError() == GL_NO_ERROR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

bool TextureGpu::createArray(u32 width, u32 height, u32 layers, u32 channels)
{
	// No need to make this a texture array if the layer count == 1.
	if (layers <= 1)
	{
		return create(width, height, channels);
	}

	m_width = width;
	m_height = height;
	m_channels = channels;
	m_bytesPerChannel = 1;
	m_layers = layers;

	glGenTextures(1, &m_gpuHandle);
	if (!m_gpuHandle) { return false; }

	glBindTexture(GL_TEXTURE_2D_ARRAY, m_gpuHandle);
	if (channels == 1)
	{
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, width, height, layers, 0, GL_RED, c_channelFormat[m_bytesPerChannel], nullptr);
	}
	else if (channels == 4)
	{
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, layers, 0, GL_RGBA, c_channelFormat[m_bytesPerChannel], nullptr);
	}
	assert(glGetError() == GL_NO_ERROR);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	assert(glGetError() == GL_NO_ERROR);
	return true;
}

bool TextureGpu::createWithData(u32 width, u32 height, const void* buffer, MagFilter magFilter)
{
	m_width = width;
	m_height = height;
	m_channels = 4;
	m_bytesPerChannel = 1;
	m_layers = 1;

	glGenTextures(1, &m_gpuHandle);
	if (!m_gpuHandle) { return false; }

	glBindTexture(GL_TEXTURE_2D, m_gpuHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, c_channelFormat[m_bytesPerChannel], buffer);
	
	f32 maxAniso = 1.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter == MAG_FILTER_NONE ? GL_NEAREST : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);

	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

bool TextureGpu::update(const void* buffer, size_t size, s32 layer)
{
	s32 layerCount = layer < 0 ? m_layers : 1;
	s32 layerIndex = layer < 0 ? 0 : layer;
	if (size < m_width * m_height * layerCount) { return false; }

	if (m_layers == 1)
	{
		glBindTexture(GL_TEXTURE_2D, m_gpuHandle);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, m_channels == 4 ? GL_RGBA : GL_RED, c_channelFormat[m_bytesPerChannel], buffer);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_gpuHandle);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0/*level*/, 0/*xOffset*/, 0/*yOffset*/, layerIndex, m_width, m_height, layerCount,
			m_channels == 4 ? GL_RGBA : GL_RED, c_channelFormat[m_bytesPerChannel], buffer);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	assert(glGetError() == GL_NO_ERROR);
	return true;
}

void TextureGpu::bind(u32 slot/* = 0*/) const
{
	glActiveTexture(GL_TEXTURE0 + slot);
	if (m_layers == 1)
	{
		glBindTexture(GL_TEXTURE_2D, m_gpuHandle);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_gpuHandle);
	}
}

void TextureGpu::clear(u32 slot/* = 0*/)
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void TextureGpu::clearSlots(u32 count, u32 start/* = 0*/)
{
	for (u32 i = 0; i < count; i++)
	{
		glActiveTexture(GL_TEXTURE0 + i + start);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}


void TextureGpu::generateMipmaps()
{
	const GLenum target = (m_layers == 1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_ARRAY;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(target, m_gpuHandle);
	glGenerateMipmap(target);
	glBindTexture(target, 0);
}

