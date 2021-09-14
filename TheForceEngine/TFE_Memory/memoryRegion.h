#pragma once
//////////////////////////////////////////////////////////////////////
// General purpose memory allocator which acts as a region of
// memory which can be quickly cleared.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>
#include <string>

struct MemoryRegion;

namespace TFE_Memory
{
	MemoryRegion* region_create(const char* name, size_t blockSize, size_t maxSize = 0u);
	void region_clear(MemoryRegion* region);
	void region_destroy(MemoryRegion* region);

	void* region_alloc(MemoryRegion* region, size_t size);
	void* region_realloc(MemoryRegion* region, void* ptr, size_t size);
	void  region_free(MemoryRegion* region, void* ptr);

	void region_test();
}