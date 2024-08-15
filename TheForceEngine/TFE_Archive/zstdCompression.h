#pragma once
#include <TFE_System/types.h>
#include <vector>

bool zstd_compress(std::vector<u8>& outbuffer, const u8* srcBuffer, const u32 srcSize, s32 level);
bool zstd_decompress(u8* dstBuffer, u32 uncompressedSize, const u8* srcBuffer, const u32 srcSize);