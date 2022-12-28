#pragma once
#include "memorystream.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

//Work buffers for handling special cases like std::string without allocating memory (beyond what the strings needs itself).
u32  s_workBufferU32[1024];		//4k buffer.
char s_workBufferChar[32768];	//32k buffer.

enum MemStreamConst : u32
{
	MS_PAGE_SHIFT = 18u,
	MS_PAGE_SIZE  = 1u << MS_PAGE_SHIFT,	// 256Kb page size to avoid excessive allocations with large streams.
};

MemoryStream::MemoryStream() : Stream()
{
	m_memory = nullptr;
	m_size = 0u;
	m_capacity = 0u;
	m_addr = 0u;
	m_mode = MODE_INVALID;
}

MemoryStream::~MemoryStream()
{
	close();
	free(m_memory);
}

bool MemoryStream::open(AccessMode mode)
{
	m_mode = mode;
	m_addr = 0u;

	// Pre-allocate if writing.
	if ((mode == MODE_WRITE || mode == MODE_READWRITE) && m_capacity == 0u)
	{
		m_memory = (u8*)malloc(MS_PAGE_SIZE);
		m_capacity = MS_PAGE_SIZE;
	}
	return m_memory != nullptr;
}

bool MemoryStream::load(size_t size, const void* const mem)
{
	if (!size || !mem) { return false; }

	clear();
	resizeBuffer(size);
	if (!m_memory) { return false; }

	memcpy(m_memory, mem, size);
	return true;
}

bool MemoryStream::allocate(size_t size)
{
	if (!size) { return false; }

	clear();
	resizeBuffer(size);
	return m_memory != nullptr;
}

const void* const MemoryStream::data() const
{
	return m_memory;
}

void* MemoryStream::data()
{
	return m_memory;
}

void MemoryStream::close()
{
	m_mode = MODE_INVALID;
}

void MemoryStream::clear()
{
	close();
	m_addr = 0u;
	m_size = 0u;
}

//derived from Stream
bool MemoryStream::seek(s32 offset, Origin origin/*=ORIGIN_START*/)
{
	if (m_memory)
	{
		if (origin == ORIGIN_START)
		{
			m_addr = offset;
		}
		else if (origin == ORIGIN_END)
		{
			m_addr = m_size + offset;
		}
		else
		{
			m_addr += offset;
		}
		return (m_addr <= m_size);
	}
	return false;
}

size_t MemoryStream::getLoc()
{
	return m_addr;
}

size_t MemoryStream::getSize()
{
	return m_size;
}

bool MemoryStream::isOpen() const
{
	return m_mode != MODE_INVALID;
}

u32 MemoryStream::readBuffer(void* ptr, u32 size, u32 count)
{
	assert(m_mode == MODE_READ || m_mode == MODE_READWRITE);
	if (m_memory && (m_mode == MODE_READ || m_mode == MODE_READWRITE))
	{
		size_t totalSize = size * count;
		if (m_addr + totalSize > m_size)
		{
			totalSize = m_addr < m_size ? (m_size - m_addr) : (0u);
		}
		memcpy(ptr, m_memory + m_addr, totalSize);
		// Step
		m_addr += totalSize;

		return (u32)totalSize;
	}
	return 0;
}

void MemoryStream::writeBuffer(const void* ptr, u32 size, u32 count)
{
	assert(m_mode == MODE_WRITE || m_mode == MODE_READWRITE);
	if (m_memory && (m_mode == MODE_WRITE || m_mode == MODE_READWRITE))
	{
		const u32 totalSize = size * count;
		resizeBuffer(m_addr + totalSize);
		memcpy(m_memory + m_addr, ptr, totalSize);
		// Step
		m_addr += totalSize;
	}
}

void MemoryStream::writeString(const char* fmt, ...)
{
	static char tmpStr[4096];
	va_list arg;
	va_start(arg, fmt);
	vsprintf(tmpStr, fmt, arg);
	va_end(arg);

	const size_t len = strlen(tmpStr);
	writeBuffer(tmpStr, (u32)len);
}

//internal
void MemoryStream::readTypeStr(std::string* ptr, u32 count)
{
	assert(count <= 256);
	//first read the length.
	readBuffer(s_workBufferU32, sizeof(u32), count);

	//then read the string data.
	for (u32 s=0; s<count; s++)
	{
		assert(s_workBufferU32[s] <= 32768);
		readBuffer(s_workBufferChar, 1, s_workBufferU32[s]);
		s_workBufferChar[ s_workBufferU32[s] ] = 0;

		ptr[s] = s_workBufferChar;
	}
}

template <typename T>
void MemoryStream::readType(T* ptr, u32 count)
{
	readBuffer(ptr, sizeof(T), count);
}

void MemoryStream::writeTypeStr(const std::string* ptr, u32 count)
{
	assert(m_memory);
	assert(count <= 256);
	//first read the length.
	for (u32 s=0; s<count; s++)
	{
		s_workBufferU32[s] = (u32)ptr[s].length();
	}
	writeBuffer(s_workBufferU32, sizeof(u32), count);

	//then write the string data.
	for (u32 s=0; s<count; s++)
	{
		writeBuffer(ptr[s].data(), 1, s_workBufferU32[s]);
	}
}

template <typename T>
void MemoryStream::writeType(const T* ptr, u32 count)
{
	assert(m_memory);
	writeBuffer(ptr, sizeof(T), count);
}

void MemoryStream::resizeBuffer(size_t newSize)
{
	if (newSize <= m_size) { return; }

	if (newSize > m_capacity)
	{
		const size_t newPageCount = (newSize + MS_PAGE_SIZE - 1) >> MS_PAGE_SHIFT;
		const size_t newCapacity = newPageCount << MS_PAGE_SHIFT;
		m_memory = (u8*)realloc(m_memory, newCapacity);
		m_capacity = newCapacity;
	}
	m_size = newSize;
}
