#include "renderTarget.h"
#include <TFE_RenderBackend/renderState.h>
#include <GL/glew.h>
#include <assert.h>

namespace
{
	u32 createDepthBuffer(u32 width, u32 height)
	{
		u32 handle = 0;
		glGenRenderbuffers(1, &handle);
		glBindRenderbuffer(GL_RENDERBUFFER, handle);
		// GL_DEPTH_COMPONENT32 is not required to be supported
		// though GL_DEPTH_COMPONENT24 and GL_DEPTH_COMPONENT32F are.
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		return handle;
	}
}

RenderTarget::~RenderTarget()
{
	glDeleteFramebuffers(1, &m_gpuHandle);
	m_gpuHandle = 0;

	if (m_depthBufferHandle)
	{
		glDeleteRenderbuffers(1, &m_depthBufferHandle);
		m_depthBufferHandle = 0;
	}
}

bool RenderTarget::create(TextureGpu* texture, bool depthBuffer)
{
	if (!texture) { return false; }
	m_texture = texture;

	glGenFramebuffers(1, &m_gpuHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, m_gpuHandle);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_texture->getHandle(), 0);

	m_depthBufferHandle = 0;
	if (depthBuffer)
	{
		m_depthBufferHandle = createDepthBuffer(texture->getWidth(), texture->getHeight());
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBufferHandle);
	}

	// Set the list of draw buffers.
	GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers); // "1" is the size of DrawBuffers

	// check FBO status
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

void RenderTarget::bind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_gpuHandle);
	glViewport(0, 0, m_texture->getWidth(), m_texture->getHeight());
	glDepthRange(0.0f, 1.0f);
}

void RenderTarget::clear(const f32* color, f32 depth)
{
	if (color)
		glClearColor(color[0], color[1], color[2], color[3]);
	else
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	u32 clearFlags = GL_COLOR_BUFFER_BIT;
	if (m_depthBufferHandle)
	{
		clearFlags |= GL_DEPTH_BUFFER_BIT;
		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE);
		glClearDepth(depth);
	}

	glClear(clearFlags);
}

 void RenderTarget::clearDepth(f32 depth)
 {
	 if (m_depthBufferHandle)
	 {
		 TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE);
		 glClearDepth(depth);
		 glClear(GL_DEPTH_BUFFER_BIT);
	 }
 }

void RenderTarget::unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
