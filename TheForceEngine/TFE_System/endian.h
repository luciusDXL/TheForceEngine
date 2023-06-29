#pragma once

#include "types.h"
#include <SDL_endian.h>

namespace TFE_Endian
{
	inline u16 swapLE16(u16 x)
	{
		return SDL_SwapLE16(x);
	}

	inline u32 swapLE32(u32 x)
	{
		return SDL_SwapLE32(x);
	}

	inline u64 swap64LE(u64 x)
	{
		return SDL_SwapLE64(x);
	}

	inline f32 swapFloatLE(f32 x)
	{
		return SDL_SwapFloatLE(x);
	}

	inline u16 swapBE16(u16 x)
	{
		return SDL_SwapBE16(x);
	}

	inline u32 swapBE32(u32 x)
	{
		return SDL_SwapBE32(x);
	}

	inline u64 swap64BE(u64 x)
	{
		return SDL_SwapBE64(x);
	}

	inline f32 swapFloatBE(f32 x)
	{
		return SDL_SwapFloatBE(x);
	}
}
