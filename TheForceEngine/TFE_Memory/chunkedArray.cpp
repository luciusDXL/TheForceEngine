#include "chunkedArray.h"
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

// Uncomment the following line to verify memory on free.
// #define _VERIFY_CHUNKED_ARR_FREE 1

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
};

namespace TFE_Memory
{
	enum
	{
		FREE_SLOT_STEP = 32,
	};
	void addFreeSlot(ChunkedArray* arr, u8* ptr);

	ChunkedArray* createChunkedArray(u32 elemSize, u32 elemPerChunk, u32 initChunkCount)
	{
		ChunkedArray* arr = (ChunkedArray*)malloc(sizeof(ChunkedArray));
		memset(arr, 0, sizeof(ChunkedArray));

		arr->elemSize = elemSize;
		arr->elemCount = 0;
				
		arr->elemPerChunk = elemPerChunk;
		arr->chunkCount = initChunkCount;
		arr->chunks = (u8**)realloc(arr->chunks, sizeof(u8*) * initChunkCount);
		
		arr->freeSlotCount = 0;
		arr->freeSlotCapacity = 0;
		arr->freeSlots = nullptr;

		const u32 chunkAllocSize = elemPerChunk * elemSize;
		for (u32 i = 0; i < initChunkCount; i++)
		{
			arr->chunks[i] = (u8*)malloc(chunkAllocSize);
		}

		return arr;
	}

	void freeChunkedArray(ChunkedArray* arr)
	{
		if (!arr) { return; }

		for (u32 i = 0; i < arr->chunkCount; i++)
		{
			free(arr->chunks[i]);
		}
		free(arr->chunks);
		free(arr->freeSlots);
		free(arr);
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
			arr->chunks = (u8**)realloc(arr->chunks, sizeof(u8*) * newChunkCount);

			const u32 chunkAllocSize = arr->elemPerChunk * arr->elemSize;
			for (u32 i = arr->chunkCount; i < newChunkCount; i++)
			{
				arr->chunks[i] = (u8*)malloc(chunkAllocSize);
			}
			arr->chunkCount = newChunkCount;
		}

		const u32 index = elementIndex - newChunkIndex*arr->elemPerChunk;
		return arr->chunks[newChunkIndex] + index*arr->elemSize;
	}
		
	void freeToChunkedArray(ChunkedArray* arr, void* ptr)
	{
		if (!arr || !ptr) { return; }
		addFreeSlot(arr, (u8*)ptr);

#ifdef _VERIFY_CHUNKED_ARR_FREE
		// First search for the containing chunk.
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
#endif
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
		return arr->chunks[chunkId] + elemId*arr->elemSize;
	}

	void addFreeSlot(ChunkedArray* arr, u8* ptr)
	{
		if (arr->freeSlotCount + 1 >= arr->freeSlotCapacity)
		{
			arr->freeSlotCapacity += FREE_SLOT_STEP;
			arr->freeSlots = (u8**)realloc(arr->freeSlots, sizeof(u8*) * arr->freeSlotCapacity);
		}
		arr->freeSlots[arr->freeSlotCount] = ptr;
		arr->freeSlotCount++;
	}
}