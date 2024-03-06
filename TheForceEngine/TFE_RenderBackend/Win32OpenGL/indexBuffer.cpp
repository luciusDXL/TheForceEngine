#include <TFE_RenderBackend/indexBuffer.h>
#include <GL/glew.h>
#include <memory.h>

IndexBuffer::~IndexBuffer()
{
	destroy();
}

bool IndexBuffer::create(u32 count, u32 stride, bool dynamic, void* initData)
{
	if (!count || !stride) { return false; }

	// Track index buffer attributes.
	m_size = count * stride;
	m_count = count;
	m_stride = stride;
	m_dynamic = dynamic;

	// Build the GPU buffer and copy the initial data.
	glGenBuffers(1, &m_gpuHandle);
	if (!m_gpuHandle) { return false; }
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpuHandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_size, initData, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return true;
}

void IndexBuffer::destroy()
{
	if (m_gpuHandle) { glDeleteBuffers(1, &m_gpuHandle); }
	m_gpuHandle = 0;
}

void IndexBuffer::update(const void* buffer, size_t size)
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpuHandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)size, (const GLvoid*)buffer, m_dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

u32 IndexBuffer::bind() const
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpuHandle);
	return m_stride == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
}

void IndexBuffer::unbind() const
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
