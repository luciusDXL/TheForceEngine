#pragma once

#include "types.h"
#include <SDL_endian.h>

namespace TFE_Endian
{
	inline u16 le16_to_cpu(u16 x)
	{
		return SDL_SwapLE16(x);
	}

	inline u32 le32_to_cpu(u32 x)
	{
		return SDL_SwapLE32(x);
	}

	inline u64 le64_to_cpu(u64 x)
	{
		return SDL_SwapLE64(x);
	}

	inline f32 lef32_to_cpu(f32 x)
	{
		return SDL_SwapFloatLE(x);
	}

	inline u16 be16_to_cpu(u16 x)
	{
		return SDL_SwapBE16(x);
	}

	inline u32 be32_to_cpu(u32 x)
	{
		return SDL_SwapBE32(x);
	}

	inline u64 be64_to_cpu(u64 x)
	{
		return SDL_SwapBE64(x);
	}

	inline f32 bef32_to_cpu(f32 x)
	{
		return SDL_SwapFloatBE(x);
	}
}
