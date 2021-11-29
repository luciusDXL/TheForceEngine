#include <cstring>

#include "chunkedArray.h"
#include <TFE_System/system.h>
#include <TFE_Memory/memoryRegion.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

// Uncomment the following line to verify memory on free.
#ifdef _DEBUG
#define _VERIFY_CHUNKED_ARR_FREE 1
#endif

struct ChunkedArray
{
	u32 elemSize;		// size of an element.
	u32 elemPerChunk;	// number of elements.

	u32 elemCount;	// number of elements.
	u32 chunkCount;	// capacity.

	u32 freeSlotCount;
	u32 freeSlotCapacity;

	u8** chunks;
	u8** freeSlots;
	MemoryRegion* region;
};

namespace TFE_Memory
{
	enum
	{
		FREE_SLOT_STEP = 32,
	};
	void addFreeSlot(ChunkedArray* arr, u8* ptr);

	void serialize(ChunkedArray* arr, FileStream* file)
	{
		assert(file);
		size_t size = size_t(&arr->chunks) - sizeof(arr);
		file->writeBuffer(arr, (u32)size);

		const u32 chunkAllocSize = arr->elemPerChunk * arr->elemSize;
		for (u32 i = 0; i < arr->chunkCount; i++)
		{
			file->write(arr->chunks[i], chunkAllocSize);
		}

		for (u32 i = 0; i < arr->freeSlotCount; i++)
		{
			s32 freeSlotIndex = getSlotIndex(arr, arr->freeSlots[i]);
			file->write(&freeSlotIndex);
		}
	}

	ChunkedArray* restore(FileStream* file, MemoryRegion* region)
	{
		assert(file && region);
		ChunkedArray* arr = (ChunkedArray*)region_alloc(region, sizeof(ChunkedArray));
		memset(arr, 0, sizeof(ChunkedArray));

		size_t size = size_t(&arr->chunks) - sizeof(arr);
		file->readBuffer(arr, (u32)size);

		arr->chunks = (u8**)region_realloc(region, arr->chunks, sizeof(u8*) * arr->chunkCount);
		const u32 chunkAllocSize = arr->elemPerChunk * arr->elemSize;
		for (u32 i = 0; i < arr->chunkCount; i++)
		{
			arr->chunks[i] = (u8*)region_alloc(region, chunkAllocSize);
			file->read(arr->chunks[i], chunkAllocSize);
		}

		arr->freeSlots = (u8**)region_realloc(region, arr->freeSlots, sizeof(u8**) * arr->freeSlotCapacity);
		for (u32 i = 0; i < arr->freeSlotCount; i++)
		{
			s32 freeSlotIndex;
			file->read(&freeSlotIndex);
			if (freeSlotIndex >= 0)
			{
				arr->freeSlots[i] = arr->chunks[freeSlotIndex];
			}
			else
			{
				arr->freeSlots[i] = nullptr;
			}
		}
		return arr;
	}

	ChunkedArray* createChunkedArray(u32 elemSize, u32 elemPerChunk, u32 initChunkCount, MemoryRegion* region)
	{
		ChunkedArray* arr = (ChunkedArray*)region_alloc(region, sizeof(ChunkedArray));
		memset(arr, 0, sizeof(ChunkedArray));
		
		arr->region = region;
		arr->elemSize = elemSize;
		arr->elemCount = 0;
				
		arr->elemPerChunk = elemPerChunk;
		arr->chunkCount = initChunkCount;
		arr->chunks = (u8**)region_realloc(region, arr->chunks, sizeof(u8*) * initChunkCount);
		
		arr->freeSlotCount = 0;
		arr->freeSlotCapacity = 0;
		arr->freeSlots = nullptr;

		const u32 chunkAllocSize = elemPerChunk * elemSize;
		for (u32 i = 0; i < initChunkCount; i++)
		{
			arr->chunks[i] = (u8*)region_alloc(region, chunkAllocSize);
		}

		return arr;
	}

	void freeChunkedArray(ChunkedArray* arr)
	{
		if (!arr) { return; }

		for (u32 i = 0; i < arr->chunkCount; i++)
		{
			region_free(arr->region, arr->chunks[i]);
		}
		region_free(arr->region, arr->chunks);
		region_free(arr->region, arr->freeSlots);
		region_free(arr->region, arr);
	}

	void* allocFromChunkedArray(ChunkedArray* arr)
	{
		if (arr->freeSlotCount)
		{
			arr->freeSlotCount--;
			return arr->freeSlots[arr->freeSlotCount];
		}
		s32 elementIndex = arr->elemCount;
		arr->elemCount++;

		const u32 newChunkIndex = elementIndex / arr->elemPerChunk;
		const u32 newChunkCount = newChunkIndex + 1;
		if (newChunkCount > arr->chunkCount)
		{
			arr->chunks = (u8**)region_realloc(arr->region, arr->chunks, sizeof(u8*) * newChunkCount);

			const u32 chunkAllocSize = arr->elemPerChunk * arr->elemSize;
			for (u32 i = arr->chunkCount; i < newChunkCount; i++)
			{
				arr->chunks[i] = (u8*)region_alloc(arr->region, chunkAllocSize);
			}
			arr->chunkCount = newChunkCount;
		}

		const u32 index = elementIndex - newChunkIndex*arr->elemPerChunk;
		assert(index < arr->elemPerChunk);
		return arr->chunks[newChunkIndex] + index*arr->elemSize;
	}
		
	void freeToChunkedArray(ChunkedArray* arr, void* ptr)
	{
		if (!arr || !ptr) { return; }
		
#ifdef _VERIFY_CHUNKED_ARR_FREE
		// First verify that the memory is contained within the chunked array.
		for (s32 i = arr->chunkCount - 1; i >= 0; i--)
		{
			if (ptr >= arr->chunks[i])
			{
				assert(ptr < arr->chunks[i] + arr->elemPerChunk * arr->elemSize);
				// Then find the index.
				u32 index = u32((u8*)ptr - arr->chunks[i]) / arr->elemSize;
				assert(index < arr->elemPerChunk);
				break;
			}
		}
		// Then verify that it hasn't already been freed.
		for (u32 i = 0; i < arr->freeSlotCount; i++)
		{
			assert(arr->freeSlots[i] != ptr);
		}
#endif

		addFreeSlot(arr, (u8*)ptr);
	}

	void chunkedArrayClear(ChunkedArray* arr)
	{
		arr->elemCount = 0;
		arr->freeSlotCount = 0;
		for (u32 i = 0; i < arr->chunkCount; i++)
		{
			memset(arr->chunks[i], 0, arr->elemPerChunk * arr->elemSize);
		}
	}

	u32 chunkedArraySize(ChunkedArray* arr)
	{
		if (!arr) { return 0; }
		return arr->elemCount;
	}

	void* chunkedArrayGet(ChunkedArray* arr, u32 index)
	{
		u32 chunkId = index / arr->elemPerChunk;
		u32 elemId = index - chunkId*arr->elemPerChunk;
		assert(chunkId < arr->chunkCount);
		return arr->chunks[chunkId] + elemId*arr->elemSize;
	}

	void addFreeSlot(ChunkedArray* arr, u8* ptr)
	{
		if (arr->freeSlotCount + 1 >= arr->freeSlotCapacity)
		{
			arr->freeSlotCapacity += FREE_SLOT_STEP;
			arr->freeSlots = (u8**)region_realloc(arr->region, arr->freeSlots, sizeof(u8*) * arr->freeSlotCapacity);
		}
		arr->freeSlots[arr->freeSlotCount] = ptr;
		arr->freeSlotCount++;
	}

	s32 getSlotIndex(ChunkedArray* arr, u8* ptr)
	{
		for (s32 i = 0; i < (s32)arr->chunkCount; i++)
		{
			if (arr->chunks[i] == ptr)
			{
				return i;
			}
		}
		return -1;
	}
}
