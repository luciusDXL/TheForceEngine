#pragma once
//////////////////////////////////////////////////////////////////////
// General purpose memory allocator which acts as a region of
// memory which can be quickly cleared.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>
#include <vector>
#include <string>

struct MemoryRegion;
typedef u32 RelativePointer;

#define NULL_RELATIVE_POINTER 0

namespace TFE_Memory
{
	MemoryRegion* region_create(const char* name, u64 blockSize, u64 maxSize = 0u);
	void region_clear(MemoryRegion* region);
	void region_destroy(MemoryRegion* region);

	void* region_alloc(MemoryRegion* region, u64 size);
	void* region_realloc(MemoryRegion* region, void* ptr, u64 size);
	void  region_free(MemoryRegion* region, void* ptr);

	u64 region_getMemoryUsed(MemoryRegion* region);
	u64 region_getMemoryCapacity(MemoryRegion* region);
	void region_getBlockInfo(MemoryRegion* region, u64* blockCount, u64* blockSize);

	RelativePointer region_getRelativePointer(MemoryRegion* region, void* ptr);
	void* region_getRealPointer(MemoryRegion* region, RelativePointer ptr);

	// TODO: Support writing to and restoring from streams in memory.
	bool region_serializeToDisk(MemoryRegion* region, FileStream* file);
	// Restore a region from disk. If 'region' is NULL then a new region is allocated,
	// otherwise it will attempt to reuse the existing region.
	MemoryRegion* region_restoreFromDisk(MemoryRegion* region, FileStream* file);

	void region_test();
}