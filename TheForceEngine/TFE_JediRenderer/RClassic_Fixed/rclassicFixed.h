#pragma once
#include <TFE_System/types.h>

struct ColorMap;

namespace TFE_JediRenderer
{
	struct EdgePair;

	namespace RClassic_Fixed
	{
		void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId);
		void setResolution(s32 width, s32 height);
	}  // RClassic_Fixed
}  // TFE_JediRenderer
