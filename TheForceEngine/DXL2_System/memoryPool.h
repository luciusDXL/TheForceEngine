#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Memory Pool
// A frame based memory allocator
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include <vector>
#include <string>

class MemoryPool
{
public:
	MemoryPool();

	void init(size_t poolSize, const char* name);

	void  clear();
	void* allocate(size_t size);
	// Allocates a new block of memory and copies the old memory into the new memory.
	// However this does not free the old memory since this is a frame based allocator - so this should be used sparingly.
	void* reallocate(void* ptr, size_t oldSize, size_t newSize);

	void  setWarningWatermark(size_t sizeToWarn) { m_waterMark = sizeToWarn; }

	size_t getMemoryUsed()  const { return m_ptr; }
	f32    getPercentUsed() const { return m_poolSize ? f32(m_ptr) / f32(m_poolSize) : 0.0f; }

private:
	std::vector<u8> m_memory;
	std::string m_name;
	size_t m_poolSize;
	size_t m_waterMark;
	size_t m_ptr;
};
