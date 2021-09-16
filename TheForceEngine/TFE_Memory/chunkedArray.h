#pragma once
//////////////////////////////////////////////////////////////////////
// Chunked array allocator
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>
#include <string>

struct ChunkedArray;
struct MemoryRegion;

namespace TFE_Memory
{
	ChunkedArray* createChunkedArray(u32 elemSize, u32 elemPerChunk, u32 initChunkCount, MemoryRegion* allocator);
	// Free everything.
	void freeChunkedArray(ChunkedArray* arr);
	// Empty the array but do not free the memory.
	void chunkedArrayClear(ChunkedArray* arr);

	void* allocFromChunkedArray(ChunkedArray* arr);
	void freeToChunkedArray(ChunkedArray* arr, void* ptr);

	u32 chunkedArraySize(ChunkedArray* arr);
	void* chunkedArrayGet(ChunkedArray* arr, u32 index);

	// TODO: Serialize/Restore
}