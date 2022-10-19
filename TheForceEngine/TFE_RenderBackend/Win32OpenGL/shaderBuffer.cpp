#include <TFE_RenderBackend/shaderBuffer.h>
#include <GL/glew.h>
#include <memory.h>

GLenum getFormat(const ShaderBufferDef& bufferDef);

ShaderBuffer::~ShaderBuffer()
{
	destroy();
}

bool ShaderBuffer::create(u32 count, const ShaderBufferDef& bufferDef, bool dynamic, void* initData)
{
	if (!count) { return false; }
	GLenum internalFormat = getFormat(bufferDef);
	if (internalFormat == GL_INVALID_ENUM)
	{
		return false;
	}
	m_initialized = true;

	// Track shader buffer attributes.
	m_bufferDef = bufferDef;
	m_stride  = m_bufferDef.channelCount * m_bufferDef.channelSize;
	m_count   = count;
	m_size    = m_stride * m_count;
	m_dynamic = dynamic;

	// Build the GPU buffer and copy the initial data.
	glGenBuffers(1, &m_gpuHandle[0]);
	glBindBuffer(GL_TEXTURE_BUFFER, m_gpuHandle[0]);
	glBufferData(GL_TEXTURE_BUFFER, m_size, initData, dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);

	glGenTextures(1, &m_gpuHandle[1]);
	glBindTexture(GL_TEXTURE_BUFFER, m_gpuHandle[1]);
	glTexBuffer(GL_TEXTURE_BUFFER, internalFormat, m_gpuHandle[0]);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
	
	return true;
}

void ShaderBuffer::destroy()
{
	if (m_initialized)
	{
		glDeleteBuffers(2, m_gpuHandle);
	}
}

void ShaderBuffer::update(const void* buffer, size_t size)
{
	glBindBuffer(GL_TEXTURE_BUFFER, m_gpuHandle[0]);
	glBufferData(GL_TEXTURE_BUFFER, size, buffer, m_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
}

void ShaderBuffer::bind(s32 bindPoint) const
{
	if (bindPoint < 0) { return; }
	glActiveTexture(GL_TEXTURE0 + bindPoint);
	glBindTexture(GL_TEXTURE_BUFFER, m_gpuHandle[1]);
}

void ShaderBuffer::unbind(s32 bindPoint) const
{
	if (bindPoint < 0) { return; }
	glActiveTexture(GL_TEXTURE0 + bindPoint);
	glBindTexture(GL_TEXTURE_BUFFER, 0);
}

GLenum getFormat(const ShaderBufferDef& bufferDef)
{
	if (bufferDef.channelCount == 1)
	{
		if (bufferDef.channelSize == 1)
		{
			return GL_R8;
		}
		else if (bufferDef.channelSize == 2)
		{
			return GL_R16;
		}
		else if (bufferDef.channelSize == 4)
		{
			if (bufferDef.channelType == BUF_CHANNEL_UINT)
			{
				return GL_R32UI;
			}
			else if (bufferDef.channelType == BUF_CHANNEL_INT)
			{
				return GL_R32I;
			}
			else if (bufferDef.channelType == BUF_CHANNEL_FLOAT)
			{
				return GL_R32F;
			}
		}
	}
	else if (bufferDef.channelCount == 2)
	{
		if (bufferDef.channelSize == 1)
		{
			return GL_RG8;
		}
		else if (bufferDef.channelSize == 2)
		{
			return GL_RG16;
		}
		else if (bufferDef.channelSize == 4)
		{
			if (bufferDef.channelType == BUF_CHANNEL_UINT)
			{
				return GL_RG32UI;
			}
			else if (bufferDef.channelType == BUF_CHANNEL_INT)
			{
				return GL_RG32I;
			}
			else if (bufferDef.channelType == BUF_CHANNEL_FLOAT)
			{
				return GL_RG32F;
			}
		}
	}
	else if (bufferDef.channelCount == 4)
	{
		if (bufferDef.channelSize == 1)
		{
			return GL_RGBA8;
		}
		else if (bufferDef.channelSize == 2)
		{
			return GL_RGBA16;
		}
		else if (bufferDef.channelSize == 4)
		{
			if (bufferDef.channelType == BUF_CHANNEL_UINT)
			{
				return GL_RGBA32UI;
			}
			else if (bufferDef.channelType == BUF_CHANNEL_INT)
			{
				return GL_RGBA32I;
			}
			else if (bufferDef.channelType == BUF_CHANNEL_FLOAT)
			{
				return GL_RGBA32F;
			}
		}
	}
	return GL_INVALID_ENUM;
}