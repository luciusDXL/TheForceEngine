#pragma once
//////////////////////////////////////////////////////////////////////
// Chunked array allocator
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/filestream.h>
#include <vector>
#include <string>

struct ChunkedArray;
struct MemoryRegion;

namespace TFE_Memory
{
	ChunkedArray* createChunkedArray(u32 elemSize, u32 elemPerChunk, u32 initChunkCount, MemoryRegion* region);
	// Free everything.
	void freeChunkedArray(ChunkedArray* arr);
	// Empty the array but do not free the memory.
	void chunkedArrayClear(ChunkedArray* arr);

	void* allocFromChunkedArray(ChunkedArray* arr);
	void freeToChunkedArray(ChunkedArray* arr, void* ptr);

	u32 chunkedArraySize(ChunkedArray* arr);
	void* chunkedArrayGet(ChunkedArray* arr, u32 index);

	void serialize(ChunkedArray* arr, FileStream* file);
	ChunkedArray* restore(FileStream* file, MemoryRegion* region);

	s32 getSlotIndex(ChunkedArray* arr, u8* ptr);
}