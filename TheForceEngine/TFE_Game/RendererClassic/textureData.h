#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

struct TextureData
{
	u16 width;		// 0x00
	u16 height;		// 0x02
	u16 uvWidth;	// 0x04	-- guesses based on BM format.
	u16 uvHeight;   // 0x06

	u32 dataSize;	// 0x08
	u8 log2Height;	// 0x0C
	u8 u2[3];		// { 0x0D, 0x0E, 0x0F }
	u8* pixelData;	// 0x10
	// ...
	s32 t0;			// 0x3c
};
