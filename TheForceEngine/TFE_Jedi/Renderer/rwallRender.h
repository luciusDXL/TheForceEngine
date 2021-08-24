#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Level/rwall.h>

struct TextureFrame;
struct Allocator;

namespace TFE_JediRenderer
{
	enum WallOrient
	{
		WORIENT_DZ_DX = 0,
		WORIENT_DX_DZ = 1
	};

	enum WallDrawFlags
	{
		WDF_MIDDLE = 0,
		WDF_TOP = 1,
		WDF_BOT = 2,
		WDF_TOP_AND_BOT = 3,
	};
}
