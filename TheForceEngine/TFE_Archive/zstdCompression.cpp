#include "zstdCompression.h"
#include <assert.h>

// Avoid include/link nonesense...
extern "C"
{
	extern s32 mz_compress2(u8 *pDest, unsigned long *pDest_len, const u8 *pSource, unsigned long source_len, int level);
	extern s32 mz_uncompress(u8 *pDest, unsigned long *pDest_len, const u8 *pSource, unsigned long source_len);
	extern u32 mz_compressBound(unsigned long source_len);
};

bool zstd_compress(std::vector<u8>& outbuffer, const u8* srcBuffer, const u32 srcSize, s32 level)
{
	// Size for the worst case.
	unsigned long compressedSize = mz_compressBound(srcSize);
	outbuffer.resize(compressedSize);

	// Then compress.
	s32 result = mz_compress2(outbuffer.data(), &compressedSize, srcBuffer, srcSize, level);
	if (result == 0)
	{
		// And if successful, resize again to the actual size.
		outbuffer.resize(compressedSize);
		return true;
	}
	return false;
}

bool zstd_decompress(u8* dstBuffer, u32 uncompressedSize, const u8* srcBuffer, const u32 srcSize)
{
	//int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len,
	//	const unsigned char *pSource, mz_ulong source_len);
	unsigned long dstSize = uncompressedSize;
	s32 result = mz_uncompress(dstBuffer, &dstSize, srcBuffer, srcSize);
	if (result == 0)
	{
		assert(dstSize == uncompressedSize);
		return true;
	}
	return false;
}