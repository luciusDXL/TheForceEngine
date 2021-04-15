#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Memory/allocator.h>

struct BM_Header
{
	s16 width;
	s16 height;
	s16 uvWidth;
	s16 uvHeight;
	u8  flags;
	u8  logSizeY;
	s16 compressed;
	s32 dataSize;
	u8  pad[12];
};

// was BM_SubHeader
struct TextureData
{
	s16 width;		// if = 1 then multiple BM in the file
	s16 height;		// EXCEPT if SizeY also = 1, in which case
					// it is a 1x1 BM
	s16 uvWidth;	// portion of texture actually used
	s16 uvHeight;	// portion of texture actually used

	u32 dataSize;	// Data size for compressed BM
					// excluding header and columns starts table
					// If not compressed, DataSize is unused

	u8 logSizeY;	// logSizeY = log2(SizeY)
					// logSizeY = 0 for weapons
	u8  pad1[3];

	u8* image;		// Image data.
	u32* columns;	// columns will be NULL except when compressed.

	// 4 bytes
	u8 flags;
	u8 compressed; // 0 = not compressed, 1 = compressed (RLE), 2 = compressed (RLE0)
	u8 pad3[2];
};

// Animated texture object.
struct AnimatedTexture
{
	s32 count;					// the number of things to iterate through.
	s32 frame;					// the current iteration index.
	s32 delay;					// the delay between iterations, in ticks.
	u32 nextTime;				// the next time this iterates.
	TextureData** frameList;	// the list of things to cycle through.
	TextureData** texPtr;		// iterates through a list every N seconds/ticks.
	TextureData* baseFrame;		// 
	u8* baseData;				// 
};

enum
{
	BM_ANIMATED_TEXTURE = -2,
};

namespace TFE_Level
{
	void bitmap_createAnimatedTextureAllocator();

	TextureData* bitmap_load(const char* filepath, u32 decompress);
	void bitmap_setupAnimatedTexture(TextureData** texture);

	// Per frame animated texture update.
	void update_animatedTextures(u32 curTick);
}
