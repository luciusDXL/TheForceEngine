#pragma once
//////////////////////////////////////////////////////////////////////
// Allocator API as found in the Jedi Engine
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <vector>
#include <string>

struct ChunkedArray;

namespace TFE_Memory
{
	ChunkedArray* createChunkedArray(u32 elemSize, u32 chunkSize, u32 initChunkCount);
	void freeChunkedArray(ChunkedArray* arr);

	void* allocFromChunkedArray(ChunkedArray* arr);
	void freeToChunkedArray(ChunkedArray* arr, void* ptr);

	u32 chunkedArraySize(ChunkedArray* arr);
	void* chunkedArrayGet(ChunkedArray* arr, u32 index);
}