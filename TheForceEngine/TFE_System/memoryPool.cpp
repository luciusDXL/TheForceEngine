#include <cstring>

#include "memoryPool.h"
#include <TFE_System/system.h>
#include <algorithm>

MemoryPool::MemoryPool() : m_poolSize(0), m_waterMark(0), m_ptr(0) {}

void MemoryPool::init(size_t poolSize, const char* name)
{
	if (poolSize > m_poolSize)
	{
		m_poolSize = poolSize;
		m_name = name;
		m_memory.resize(poolSize);

		TFE_System::logWrite(LOG_MSG, "MemoryPool", "Allocating memory pool \"%s\", size %u bytes.", name, poolSize);
	}
	clear();
}

void MemoryPool::clear()
{
	m_ptr = 0u;
}

void* MemoryPool::allocate(size_t size)
{
	if (size == 0) { return nullptr; }

	if (m_ptr + size > m_poolSize)
	{
		TFE_System::logWrite(LOG_ERROR, "MemoryPool", "Allocate of size %u bytes failed for memory pool \"%s\"", size, m_name.c_str());
		return nullptr;
	}
	else if (m_ptr + size >= m_waterMark && m_waterMark > 0u)
	{
		TFE_System::logWrite(LOG_WARNING, "MemoryPool", "Pool \"%s\" is at %0.2f%% capacity.", m_name.c_str(), 100.0f * f32(m_ptr + size) / f32(m_poolSize));
		// Change the watermark to be closer to full to avoid spamming the log.
		m_waterMark = (m_waterMark + m_poolSize) >> 1;
	}

	u8* memory = m_memory.data() + m_ptr;
	m_ptr += size;

	return memory;
}

void* MemoryPool::reallocate(void* ptr, size_t oldSize, size_t newSize)
{
	u8* newMem = (u8*)allocate(newSize);
	memcpy(newMem, ptr, oldSize);
	return newMem;
}
