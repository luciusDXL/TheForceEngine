#pragma once
#include <TFE_System/types.h>

namespace TFE_JediRenderer
{
	namespace RClassic_GPU
	{
		void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId);
		void setResolution(s32 width, s32 height);
	}  // RClassic_GPU
}  // TFE_JediRenderer
