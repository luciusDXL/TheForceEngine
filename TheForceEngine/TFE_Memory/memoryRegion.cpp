#include <cstring>

#include "memoryRegion.h"
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <TFE_System/math.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

// #define _VERIFY_MEMORY

#ifdef _VERIFY_MEMORY
#define VERIFY_MEMORY() verifyMemory(region)
#else
#define VERIFY_MEMORY()
#endif

using namespace TFE_Jedi;

enum
{
	MIN_SPLIT_SIZE = 32,
	BLOCK_ARR_STEP = 16,
	ALIGNMENT = 8,
	ALLOC_BIN_COUNT = 6,
	ALLOC_BIN_LAST = 5,
	// No more then 256 blocks, and no more than 16MB per block for a total of 4GB.
	MAX_BLOCK_COUNT = 256,
	MAX_BLOCK_SIZE  = 16 * 1024 * 1024,
	RELATIVE_NON_NULL_BIT = 1u,
	SHARED_HEADER_SIZE = 8,	// 8 bytes are shared between RegionAllocHeader{} and AllocHeaderFree{}
};

struct RegionAllocHeader
{
	u32 size;
	u8  free;
	u8  bin;
	u8  pad8[2];
	u64 pad; // pad to 16 bytes.
};

// free structure is larger than header, because it fits within the
// alignment: align(8, sizeof(header)=16 + size), so at least 24 bytes is allocated.
struct AllocHeaderFree
{
	u32 size;
	u8  free;
	u8  bin;
	u8  pad8[2];
	AllocHeaderFree* binNext;
	AllocHeaderFree* binPrev;
};

struct MemoryBlock
{
	u32 sizeFree;
	u32 count;
	// Head pointer to each bin.
	// Bin index = clamp(log2(nextPow2(size)) - 5, 0, 5)
	// bin 0: [0,  32]
	//     1: [33, 64]
	//     2: [65, 128]
	//     3: [129, 256]
	//     4: [257, 512]
	//     5: [513+]
	AllocHeaderFree* freeListBins[ALLOC_BIN_COUNT];
};

struct MemoryRegion
{
	char name[32];
	MemoryBlock** memBlocks;
	size_t blockArrCapacity;

	size_t blockCount;
	size_t blockSize;
	size_t maxBlocks;
};

static_assert(sizeof(RegionAllocHeader) == 16, "RegionAllocHeader is the wrong size.");
static_assert(sizeof(AllocHeaderFree) == 24, "AllocHeaderFree is the wrong size.");

namespace TFE_Memory
{
	// These constants assume no more then 256 blocks, and no more than 16MB per block for a total of 4GB.
	// See MAX_BLOCK_COUNT and MAX_BLOCK_SIZE above.
	static const u32 c_relativeBlockShift = 24u;
	static const u32 c_relativeOffsetMask = (1u << c_relativeBlockShift) - 1u;

	void freeSlot(RegionAllocHeader* alloc, RegionAllocHeader* next, MemoryBlock* block);
	size_t alloc_align(size_t baseSize);
	s32  getBinFromSize(u32 size);
	bool allocateNewBlock(MemoryRegion* region);
	void removeHeaderFromFreelist(MemoryBlock* block, RegionAllocHeader* header);
	void insertBlockIntoFreelist(MemoryBlock* block, RegionAllocHeader* header);

	void verifyMemory(MemoryRegion* region)
	{
		for (s32 i = 0; i < region->blockCount; i++)
		{
			MemoryBlock* block = region->memBlocks[i];
			assert(block->sizeFree <= region->blockSize);
			u8* mem = (u8*)block + sizeof(MemoryBlock);
			RegionAllocHeader* prev = nullptr;
			for (u32 a = 0; a < block->count; a++)
			{
				RegionAllocHeader* header = (RegionAllocHeader*)mem;
				assert(header->free == 0 || header->free == 1);
				assert(header->size <= region->blockSize);
				mem += header->size;
				prev = header;
			}

			for (s32 b = 0; b < ALLOC_BIN_COUNT; b++)
			{
				if (block->freeListBins[b])
				{
					AllocHeaderFree* slot = block->freeListBins[b];
					while (slot)
					{
						assert(slot->free == 1 && slot->bin == b);
						assert(slot->size <= block->sizeFree);
						slot = slot->binNext;
					}
				}
			}
		}
	}

	MemoryRegion* region_create(const char* name, size_t blockSize, size_t maxSize)
	{
		assert(name);
		if (!name || !blockSize) { return nullptr; }
		if (blockSize > MAX_BLOCK_SIZE)
		{
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "The block size for region '%s' of %u is too large, the maximum is %u.", name, blockSize, MAX_BLOCK_SIZE);
			return nullptr;
		}

		MemoryRegion* region = (MemoryRegion*)malloc(sizeof(MemoryRegion));
		if (!region)
		{
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Failed to allocate region '%s'.", region->name);
			return nullptr;
		}

		strcpy(region->name, name);
		region->memBlocks = nullptr;
		region->blockArrCapacity = 0;
		region->blockCount = 0;
		region->blockSize = blockSize;
		region->maxBlocks = maxSize ? (maxSize + blockSize - 1) / blockSize : 0;
		if (!allocateNewBlock(region))
		{
			free(region);
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Failed to memory block of size %u in region '%s'.", blockSize, region->name);
			return nullptr;
		}
		VERIFY_MEMORY();

		return region;
	}

	void region_clear(MemoryRegion* region)
	{
		assert(region);
		for (s32 i = 0; i < region->blockCount; i++)
		{
			MemoryBlock* block = region->memBlocks[i];
			block->sizeFree = u32(region->blockSize);
			block->count = 1;

			RegionAllocHeader* header = (RegionAllocHeader*)((u8*)block + sizeof(MemoryBlock));
			header->size = block->sizeFree;
			header->free = 0;
			memset(block->freeListBins, 0, sizeof(AllocHeaderFree*)*ALLOC_BIN_COUNT);
			insertBlockIntoFreelist(block, header);
			VERIFY_MEMORY();
		}
	}

	void region_destroy(MemoryRegion* region)
	{
		assert(region);
		for (s32 i = 0; i < region->blockCount; i++)
		{
			free(region->memBlocks[i]);
		}
		free(region->memBlocks);
		free(region);
	}
		
	void* allocFromHeader(MemoryBlock* block, RegionAllocHeader* header, u32 size)
	{
		assert(header->free == 1);
		if (header->size - size >= MIN_SPLIT_SIZE)
		{
			// Split.
			size_t split0 = size;
			size_t split1 = header->size - split0;
			RegionAllocHeader* next = (RegionAllocHeader*)((u8*)header + split0);

			// Cleanup the free list.
			removeHeaderFromFreelist(block, header);
			header->size = u32(split0);

			// Create a new free block.
			next->size = u32(split1);
			next->free = 0;
			block->count++;
						
			// Add the new block to the free list.
			insertBlockIntoFreelist(block, next);
		}
		else
		{
			// Consume the whole block.
			removeHeaderFromFreelist(block, header);
		}
		block->sizeFree -= header->size;
		return (u8*)header + sizeof(RegionAllocHeader);
	}

	void* region_alloc(MemoryRegion* region, size_t size)
	{
		assert(region);
		if (size == 0) { return nullptr; }

		size = alloc_align(size + sizeof(RegionAllocHeader));
		assert(size >= 24);	// at least 24 bytes is required to hold the free header.
		if (size > region->blockSize) { return nullptr; }
		
		for (s32 i = 0; i < region->blockCount; i++)
		{
			MemoryBlock* block = region->memBlocks[i];
			if (block->sizeFree < size)
			{
				continue;
			}

			// Try to allocate from the closest matching bin.
			s32 bin = getBinFromSize((u32)size);
			for (s32 b = bin; b < ALLOC_BIN_COUNT; b++)
			{
				AllocHeaderFree* header = block->freeListBins[b];
				while (header)
				{
					if (header->size >= size)
					{
						VERIFY_MEMORY();
						void* mem = allocFromHeader(block, (RegionAllocHeader*)header, (u32)size);
						VERIFY_MEMORY();
						return mem;
					}
					header = header->binNext;
				}
			}
		}

		if (!region->maxBlocks || region->blockCount < region->maxBlocks)
		{
			if (allocateNewBlock(region))
			{
				VERIFY_MEMORY();
				void* mem = region_alloc(region, size);
				VERIFY_MEMORY();
				return mem;
			}
		}
		
		// We are all out of memory...
		TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Failed to allocate %u bytes in region '%s'.", size, region->name);
		return nullptr;
	}

	void* region_realloc(MemoryRegion* region, void* ptr, size_t size)
	{
		assert(region);
		if (!ptr) { return region_alloc(region, size); }
		if (size == 0) { return nullptr; }

		size = alloc_align(size + sizeof(RegionAllocHeader));
		if (size > region->blockSize) { return nullptr; }

		// If the current block is already large enough, skip looping over the memory blocks.
		RegionAllocHeader* header = (RegionAllocHeader*)((u8*)ptr - sizeof(RegionAllocHeader));
		if (header->size >= size)
		{
			return ptr;
		}

		// First try to reallocate in the same region.
		u32 prevSize = 0;
		for (s32 i = (s32)region->blockCount - 1; i >= 0; i--)
		{
			MemoryBlock* block = region->memBlocks[i];
			if (ptr >= block)
			{
				RegionAllocHeader* header = (RegionAllocHeader*)((u8*)ptr - sizeof(RegionAllocHeader));
				RegionAllocHeader* nextHeader = (RegionAllocHeader*)((u8*)header + header->size);
				assert(header->free == 0);

				if ((u8*)nextHeader >= (u8*)block + region->blockSize)
				{
					nextHeader = nullptr;
				}
				// If it is big enough, just stick to the same memory.
				if (header->size >= size)
				{
					return ptr;
				}
				// If the next block is free, merge the two blocks and then allocate from that.
				if (nextHeader && nextHeader->free && header->size + nextHeader->size >= size)
				{
					VERIFY_MEMORY();
					// Remove the nextHeader from the freelist.
					assert(nextHeader->free == 1);
					removeHeaderFromFreelist(block, nextHeader);

					// Merge blocks.
					block->sizeFree += header->size;
					header->size += nextHeader->size;
					block->count--;
									
					// Allocate from the new header.
					if (header->size - size >= MIN_SPLIT_SIZE)
					{
						// Split.
						size_t split0 = size;
						size_t split1 = header->size - split0;
						RegionAllocHeader* next = (RegionAllocHeader*)((u8*)header + split0);

						// Reset the header.
						header->free = 0;
						header->size = u32(split0);

						// Create a new free block.
						next->size = u32(split1);
						next->free = 0;
						block->count++;

						// Add the new block to the free list.
						insertBlockIntoFreelist(block, next);
					}
					block->sizeFree -= header->size;
					VERIFY_MEMORY();
					return (u8*)header + sizeof(RegionAllocHeader);
				}
				// Otherwise break, we have to free and reallocate.
				prevSize = header->size;
				break;
			}
		}

		// Allocate a new block of memory.
		void* newMem = region_alloc(region, size);
		if (!newMem) { return nullptr; }
		// Copy over the contents from the previous block.
		if (prevSize > sizeof(RegionAllocHeader))
		{
			memcpy(newMem, ptr, std::min((u32)size, prevSize) - sizeof(RegionAllocHeader));
		}
		// Free the previous block
		region_free(region, ptr);
		// Then return the new block.
		VERIFY_MEMORY();
		return newMem;
	}
		
	void region_free(MemoryRegion* region, void* ptr)
	{
		if (!ptr || !region) { return; }

		for (s32 i = (s32)region->blockCount - 1; i >= 0; i--)
		{
			MemoryBlock* block = region->memBlocks[i];
			if (ptr >= block)
			{
				RegionAllocHeader* header = (RegionAllocHeader*)((u8*)ptr - sizeof(RegionAllocHeader));
				RegionAllocHeader* nextHeader = (RegionAllocHeader*)((u8*)header + header->size);
				if ((u8*)nextHeader >= (u8*)block + region->blockSize)
				{
					nextHeader = nullptr;
				}

				assert(!header->free);
				if (header->free)
				{
					TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Attempted to double free pointer %x in region '%s'.", ptr, region->name);
					return;
				}

				VERIFY_MEMORY();
				freeSlot(header, nextHeader, block);
				VERIFY_MEMORY();
				return;
			}
		}
	}
		
	size_t region_getMemoryUsed(MemoryRegion* region)
	{
		size_t used = 0;
		for (s32 i = 0; i < region->blockCount; i++)
		{
			used += (region->blockSize - region->memBlocks[i]->sizeFree);
		}
		return used;
	}

	void region_getBlockInfo(MemoryRegion* region, size_t* blockCount, size_t* blockSize)
	{
		*blockCount = region->blockCount;
		*blockSize = region->blockSize;
	}

	size_t region_getMemoryCapacity(MemoryRegion* region)
	{
		return region->blockCount * region->blockSize;
	}
		
	RelativePointer region_getRelativePointer(MemoryRegion* region, void* ptr)
	{
		RelativePointer rp = NULL_RELATIVE_POINTER;
		if (!ptr || !region) { return rp; }
		
		for (s32 i = (s32)region->blockCount - 1; i >= 0; i--)
		{
			MemoryBlock* block = region->memBlocks[i];
			if (ptr >= block)
			{
				rp = RelativePointer((u8*)ptr - (u8*)block - sizeof(MemoryBlock));
				rp |= (i << c_relativeBlockShift);
				assert(!(rp & RELATIVE_NON_NULL_BIT));

				// With 8 byte alignment, the lowest bit should always be 0, so we can use it as a "non-null" bit.
				rp |= RELATIVE_NON_NULL_BIT;
				break;
			}
		}
		// With alignment, the lower bit should always be zero.
		return rp;
	}

	void* region_getRealPointer(MemoryRegion* region, RelativePointer ptr)
	{
		if (ptr == NULL_RELATIVE_POINTER)
		{
			return nullptr;
		}
		assert(ptr & RELATIVE_NON_NULL_BIT);
		ptr &= ~RELATIVE_NON_NULL_BIT;

		const u32 blockIndex = ptr >> c_relativeBlockShift;
		assert(blockIndex < region->blockCount);
		if (blockIndex >= region->blockCount)
		{
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Relative pointer for region '%s' has an invalid block index: %u.", region->name, blockIndex);
			return nullptr;
		}
		MemoryBlock* block = region->memBlocks[blockIndex];
		return (u8*)block + (ptr & c_relativeOffsetMask) + sizeof(MemoryBlock);
	}

	bool region_serializeToDisk(MemoryRegion* region, FileStream* file)
	{
		if (!region || !file || !file->isOpen())
		{
			return false;
		}

		file->writeBuffer(region->name, 32);
		file->write(&region->blockArrCapacity);
		file->write(&region->blockCount);
		file->write(&region->blockSize);
		file->write(&region->maxBlocks);

		for (s32 b = 0; b < region->blockCount; b++)
		{
			MemoryBlock* block = region->memBlocks[b];
			file->write(&block->count);
			file->write(&block->sizeFree);
			for (s32 bin = 0; bin < ALLOC_BIN_COUNT; bin++)
			{
				RelativePointer ptr = region_getRelativePointer(region, block->freeListBins[bin]);
				file->write(&ptr);
			}

			u8* memPtr = (u8*)block + sizeof(MemoryBlock);
			for (u32 al = 0; al < block->count; al++)
			{
				RegionAllocHeader* header = (RegionAllocHeader*)memPtr;
				if (header->free)
				{
					AllocHeaderFree* freeHeader = (AllocHeaderFree*)memPtr;
					file->writeBuffer(freeHeader, SHARED_HEADER_SIZE);

					RelativePointer binNext = region_getRelativePointer(region, freeHeader->binNext);
					RelativePointer binPrev = region_getRelativePointer(region, freeHeader->binPrev);
					file->write(&binNext);
					file->write(&binPrev);
				}
				else
				{
					file->writeBuffer(header, header->size);
				}

				memPtr += header->size;
			}
		}

		return true;
	}

	MemoryRegion* region_restoreFromDisk(MemoryRegion* region, FileStream* file)
	{
		if (!file || !file->isOpen())
		{
			return nullptr;
		}
		if (!region)
		{
			region = (MemoryRegion*)malloc(sizeof(MemoryRegion));
			region->blockArrCapacity = 0;
		}
		if (!region)
		{
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Failed to allocate region.");
			return nullptr;
		}

		size_t blockAllocStart = 0;
		file->readBuffer(region->name, 32);
		if (region->blockArrCapacity == 0)
		{
			file->read(&region->blockArrCapacity);
			file->read(&region->blockCount);
			file->read(&region->blockSize);
			file->read(&region->maxBlocks);
			region->memBlocks = (MemoryBlock**)malloc(sizeof(MemoryBlock*)*region->blockArrCapacity);
		}
		else
		{
			size_t blockArrCapacity, blockCount, blockSize, maxBlocks;
			file->read(&blockArrCapacity);
			file->read(&blockCount);
			file->read(&blockSize);
			file->read(&maxBlocks);

			// The region is just too different, we have to start again.
			if (blockSize != region->blockSize)
			{
				// Free memory since we have to reallocate from scratch.
				for (s32 i = 0; i < region->blockCount; i++)
				{
					free(region->memBlocks[i]);
				}
				free(region->memBlocks);

				// Recreate.
				region->blockArrCapacity = blockArrCapacity;
				region->blockCount = blockCount;
				region->blockSize = blockSize;
				region->maxBlocks = maxBlocks;
				region->memBlocks = (MemoryBlock**)malloc(sizeof(MemoryBlock*)*region->blockArrCapacity);
			}
			else  // We don't need to allocate from scratch.
			{
				// Don't reallocate existing blocks, just reset them.
				blockAllocStart = region->blockCount;
				// Only reallocate if the capacity is lower.
				if (region->blockArrCapacity < blockArrCapacity)
				{
					region->blockArrCapacity = blockArrCapacity;
					region->memBlocks = (MemoryBlock**)realloc(region->memBlocks, sizeof(MemoryBlock*)*region->blockArrCapacity);
				}
				region->blockCount = blockCount;
				region->blockSize  = blockSize;
				region->maxBlocks  = maxBlocks;
			}
		}
		
		for (s32 b = 0; b < region->blockCount; b++)
		{
			// Only allocate the block if it was not part of the original region passed in.
			if (b >= blockAllocStart)
			{
				region->memBlocks[b] = (MemoryBlock*)malloc(sizeof(MemoryBlock) + region->blockSize);
			}

			MemoryBlock* block = region->memBlocks[b];
			file->read(&block->count);
			file->read(&block->sizeFree);
			for (s32 bin = 0; bin < ALLOC_BIN_COUNT; bin++)
			{
				RelativePointer ptr;
				file->read(&ptr);
				block->freeListBins[bin] = (AllocHeaderFree*)region_getRealPointer(region, ptr);
			}

			u8* memPtr = (u8*)block + sizeof(MemoryBlock);
			for (u32 al = 0; al < block->count; al++)
			{
				RegionAllocHeader* header = (RegionAllocHeader*)memPtr;
				file->readBuffer(header, SHARED_HEADER_SIZE);

				if (header->free)
				{
					AllocHeaderFree* freeHeader = (AllocHeaderFree*)memPtr;
					RelativePointer binNext, binPrev;
					file->read(&binNext);
					file->read(&binPrev);

					freeHeader->binNext = (AllocHeaderFree*)region_getRealPointer(region, binNext);
					freeHeader->binPrev = (AllocHeaderFree*)region_getRealPointer(region, binPrev);
				}
				else
				{
					file->readBuffer((u8*)header + SHARED_HEADER_SIZE, header->size - SHARED_HEADER_SIZE);
				}

				memPtr += header->size;
			}
		}

		return region;
	}

	void freeSlot(RegionAllocHeader* alloc, RegionAllocHeader* next, MemoryBlock* block)
	{
		block->sizeFree += alloc->size;

		assert(alloc->free == 0);
		if (next && next->free)  // Then try merging the current and next.
		{
			assert(next->free == 1);
			// Remove the next block from the freelist.
			removeHeaderFromFreelist(block, next);

			// Merge
			alloc->size += next->size;
			block->count--;
		}
		// Then add the new item to the free list.
		insertBlockIntoFreelist(block, alloc);
	}

	size_t alloc_align(size_t baseSize)
	{
		return (baseSize + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
	}
		
	s32 getBinFromSize(u32 size)
	{
		if (size < 32) { return 0; }
		else if (size <= 64) { return 1; }
		else if (size <= 128) { return 2; }
		else if (size <= 256) { return 3; }
		else if (size <= 512) { return 4; }
		return 5;
	}

	void removeHeaderFromFreelist(MemoryBlock* block, RegionAllocHeader* header)
	{
		AllocHeaderFree* freeHeader = (AllocHeaderFree*)header;
		assert(freeHeader->free == 1);
		assert(freeHeader->bin < ALLOC_BIN_COUNT);

		u8 bin = freeHeader->bin;
		freeHeader->free = 0;
		freeHeader->bin = 0;
		if (freeHeader->binPrev)
		{
			AllocHeaderFree* nextFree = freeHeader->binNext;
			AllocHeaderFree* prevFree = freeHeader->binPrev;
			prevFree->binNext = nextFree;
			if (nextFree)
			{
				nextFree->binPrev = prevFree;
			}
		}
		else if (freeHeader->binNext)
		{
			assert(freeHeader == block->freeListBins[bin]);
			if (freeHeader == block->freeListBins[bin])
			{
				block->freeListBins[bin] = freeHeader->binNext;
				block->freeListBins[bin]->binPrev = nullptr;
			}
		}
		else
		{
			assert(freeHeader->binPrev || freeHeader == block->freeListBins[bin]);
			if (freeHeader == block->freeListBins[bin])
			{
				block->freeListBins[bin] = nullptr;
			}
		}
	}

	void insertBlockIntoFreelist(MemoryBlock* block, RegionAllocHeader* header)
	{
		AllocHeaderFree* freeNext = (AllocHeaderFree*)header;
		assert(freeNext->free == 0);
		s32 bin = getBinFromSize(header->size);
		freeNext->free = 1;
		freeNext->bin = bin;
		freeNext->pad8[0] = 0;
		freeNext->pad8[1] = 0;
		if (!block->freeListBins[bin])
		{
			block->freeListBins[bin] = freeNext;
			freeNext->binPrev = nullptr;
			freeNext->binNext = nullptr;
		}
		else
		{
			block->freeListBins[bin]->binPrev = freeNext;
			freeNext->binNext = block->freeListBins[bin];
			freeNext->binPrev = nullptr;

			block->freeListBins[bin] = freeNext;
		}
	}

	bool allocateNewBlock(MemoryRegion* region)
	{
		if (region->blockCount >= MAX_BLOCK_COUNT)
		{
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Too many blocks allocated for region '%s' - the maximum is %u.", region->name, MAX_BLOCK_COUNT);
			return false;
		}

		if (!region->memBlocks)
		{
			region->blockArrCapacity = BLOCK_ARR_STEP;
			region->memBlocks = (MemoryBlock**)malloc(sizeof(MemoryBlock*)*region->blockArrCapacity);
		}
		else if (region->blockCount + 1 > region->blockArrCapacity)
		{
			region->blockArrCapacity += BLOCK_ARR_STEP;
			region->memBlocks = (MemoryBlock**)realloc(region->memBlocks, sizeof(MemoryBlock*)*region->blockArrCapacity);
		}
		if (!region->memBlocks)
		{
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Failed to resize memory block of array to %u in region '%s'.", region->blockArrCapacity, region->name);
			return false;
		}

		size_t blockIndex = region->blockCount;
		region->memBlocks[blockIndex] = (MemoryBlock*)malloc(sizeof(MemoryBlock) + region->blockSize);
		if (!region->memBlocks[blockIndex])
		{
			TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Failed to allocate block of size %u in region '%s'.", region->blockSize, region->name);
			return false;
		}
		region->blockCount++;
		TFE_System::logWrite(LOG_MSG, "MemoryRegion", "Allocated new memory block in region '%s' - new size is %u blocks, total size is '%u'", region->name, region->blockCount, region->blockSize * region->blockCount);

		MemoryBlock* block = region->memBlocks[blockIndex];
		block->sizeFree = u32(region->blockSize);
		block->count = 1;

		RegionAllocHeader* header = (RegionAllocHeader*)((u8*)block + sizeof(MemoryBlock));
		header->size = block->sizeFree;
		header->free = 0;
		memset(block->freeListBins, 0, sizeof(AllocHeaderFree*)*ALLOC_BIN_COUNT);
		insertBlockIntoFreelist(block, header);

		return true;
	}

	// 20k allocations and 1250 deallocations:
	// Malloc = 0.005514 sec.
	// Region = 0.000991 sec.
	#define ALLOC_COUNT 20000
	const size_t _testAllocSize[] = { 16, 32, 24, 100, 200, 500, 327, 537, 200, 17, 57, 387, 874, 204, 100, 22 };

	void region_test()
	{
		u64 start = TFE_System::getCurrentTimeInTicks();
		const size_t mask = TFE_ARRAYSIZE(_testAllocSize) - 1;
		void* alloc[ALLOC_COUNT];
		for (s32 i = 0; i < ALLOC_COUNT; i++)
		{
			alloc[i] = malloc(_testAllocSize[i&mask]);
			if ((i % 16) == 0)
			{
				free(alloc[i]);
				alloc[i] = nullptr;
			}
		}
		u64 mallocDelta = TFE_System::getCurrentTimeInTicks() - start;
		// Free memory.
		for (s32 i = 0; i < ALLOC_COUNT; i++)
		{
			free(alloc[i]);
		}

		MemoryRegion* region = region_create("Test", 32 * 1024 * 1024);
		start = TFE_System::getCurrentTimeInTicks();
		for (s32 i = 0; i < ALLOC_COUNT; i++)
		{
			alloc[i] = region_alloc(region, _testAllocSize[i&mask]);
			if ((i % 16) == 0)
			{
				region_free(region, alloc[i]);
				alloc[i] = nullptr;
			}
		}
		u64 regionDelta = TFE_System::getCurrentTimeInTicks() - start;
		// Free memory.
		region_destroy(region);

		TFE_System::logWrite(LOG_MSG, "MemoryRegion", "Malloc: %f, Region: %f", TFE_System::convertFromTicksToSeconds(mallocDelta), TFE_System::convertFromTicksToSeconds(regionDelta));
	}
}