#pragma once
//////////////////////////////////////////////////////////////////////
// Wrapper for OpenGL textures that expose the limited set of 
// required functionality.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

class IndexBuffer
{
public:
	IndexBuffer() : m_stride(0), m_count(0), m_dynamic(false), m_gpuHandle(0) {}
	~IndexBuffer();

	bool create(u32 count, u32 stride, bool dynamic, void* initData = nullptr);
	void destroy();

	void update(const void* buffer, size_t size);
	// bind() returns the size type that should be used in draw commands.
	u32 bind();
	void unbind();

	inline u32 getHandle() const { return m_gpuHandle; }

private:
	u32 m_stride;
	u32 m_count;
	u32 m_size;
	u32 m_gpuHandle;
	bool m_dynamic;
};
