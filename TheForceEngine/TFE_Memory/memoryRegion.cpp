#include "memoryRegion.h"
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

struct AllocHeader
{
	u32 size;
	u32 free;
};

struct MemoryBlock
{
	u32 sizeFree;
	u32 count;
	u32 pad[2];			// Pad to 16 bytes.
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

namespace TFE_Memory
{
	enum
	{
		MIN_SPLIT_SIZE = 32,
		BLOCK_ARR_STEP = 16,
		ALIGNMENT = 8,
	};

	void freeSlot(AllocHeader* alloc, AllocHeader* prev, AllocHeader* next, MemoryBlock* block);
	size_t alloc_align(size_t baseSize);

	bool allocateNewBlock(MemoryRegion* region)
	{
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

		AllocHeader* header = (AllocHeader*)((u8*)block + sizeof(MemoryBlock));
		header->free = 1;
		header->size = block->sizeFree;

		return true;
	}

	MemoryRegion* region_create(const char* name, size_t blockSize, size_t maxSize)
	{
		assert(name);
		if (!name || !blockSize) { return nullptr; }

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

			AllocHeader* header = (AllocHeader*)((u8*)block + sizeof(MemoryBlock));
			header->free = 1;
			header->size = block->sizeFree;
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
				
	void* region_alloc(MemoryRegion* region, size_t size)
	{
		assert(region);
		if (size == 0) { return nullptr; }

		size = alloc_align(size + sizeof(AllocHeader));
		if (size > region->blockSize) { return nullptr; }
		
		for (s32 i = 0; i < region->blockCount; i++)
		{
			MemoryBlock* block = region->memBlocks[i];
			if (block->sizeFree < size)
			{
				continue;
			}

			u32 count = block->count;
			u8* regPtr = (u8*)block + sizeof(MemoryBlock);
			for (u32 allocIdx = 0; allocIdx < count; allocIdx++)
			{
				AllocHeader* header = (AllocHeader*)regPtr;
				if (header->free && header->size >= size)
				{
					if (header->size - size >= MIN_SPLIT_SIZE)
					{
						// Split.
						size_t split0 = size;
						size_t split1 = header->size - split0;
						AllocHeader* next = (AllocHeader*)((u8*)header + split0);

						header->size = u32(split0);
						header->free = 0;

						next->size = u32(split1);
						next->free = 1;
						block->count++;
					}
					else
					{
						// Consume the whole block.
						header->free = 0;
					}
					block->sizeFree -= header->size;
					return (u8*)header + sizeof(AllocHeader);
				}
				regPtr += header->size;
			}
		}

		if (!region->maxBlocks || region->blockCount < region->maxBlocks)
		{
			if (allocateNewBlock(region))
			{
				return region_alloc(region, size);
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

		size = alloc_align(size + sizeof(AllocHeader));
		if (size > region->blockSize) { return nullptr; }

		// First try to reallocate in the same region.
		u32 prevSize = 0;
		for (s32 i = (s32)region->blockCount - 1; i >= 0; i--)
		{
			MemoryBlock* block = region->memBlocks[i];
			if (ptr >= block)
			{
				u32 count = block->count;
				u8* regPtr = (u8*)block + sizeof(MemoryBlock);
				for (u32 allocIdx = 0; allocIdx < count; allocIdx++)
				{
					AllocHeader* header = (AllocHeader*)regPtr;
					if (ptr == (u8*)header + sizeof(AllocHeader))
					{
						// If it is big enough, just stick to the same memory.
						if (header->size >= size)
						{
							return ptr;
						}
						// Otherwise break, we have to reallocate.
						prevSize = header->size;
						break;
					}
					regPtr += header->size;
				}
				break;
			}
		}

		// Allocate a new block of memory.
		void* newMem = region_alloc(region, size);
		if (!newMem) { return nullptr; }
		// Copy over the contents from the previous block.
		if (prevSize)
		{
			memcpy(newMem, ptr, std::min((u32)size, prevSize) - sizeof(AllocHeader));
		}
		// Free the previous block
		region_free(region, ptr);
		// Then return the new block.
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
				AllocHeader* prevHeader = nullptr;
				AllocHeader* nextHeader = nullptr;

				u32 count = block->count;
				u8* regPtr = (u8*)block + sizeof(MemoryBlock);
				for (u32 allocIdx = 0; allocIdx < count; allocIdx++)
				{
					AllocHeader* header = (AllocHeader*)regPtr;
					if (ptr == (u8*)header + sizeof(AllocHeader))
					{
						assert(!header->free);
						if (header->free)
						{
							TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Attempted to double free pointer %x in region '%s'.", ptr, region->name);
							return;
						}
						nextHeader = (allocIdx < count - 1) ? (AllocHeader*)(regPtr + header->size) : nullptr;
						freeSlot(header, prevHeader, nextHeader, block);
						return;
					}
					prevHeader = header;
					regPtr += header->size;
				}
				TFE_System::logWrite(LOG_ERROR, "MemoryRegion", "Attempted to free pointer %x which is not part of region '%s'.", ptr, region->name);
				break;
			}
		}
	}

	void freeSlot(AllocHeader* alloc, AllocHeader* prev, AllocHeader* next, MemoryBlock* block)
	{
		block->sizeFree += alloc->size;

		// Merge all 3 allocations.
		if (prev && prev->free && next && next->free)
		{
			prev->size += alloc->size + next->size;
			block->count -= 2;
		}
		else if (prev && prev->free)  // Then try merging the previous and current.
		{
			prev->size += alloc->size;
			block->count--;
		}
		else if (next && next->free)  // Then try merging the current and next.
		{
			alloc->size += next->size;
			alloc->free = 1;
			block->count--;
		}
		else
		{
			alloc->free = 1;
		}
	}

	size_t alloc_align(size_t baseSize)
	{
		return (baseSize + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
	}
}