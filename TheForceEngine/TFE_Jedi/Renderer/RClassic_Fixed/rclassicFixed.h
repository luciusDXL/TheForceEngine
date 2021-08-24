#pragma once
#include <TFE_System/types.h>

struct ColorMap;

namespace TFE_JediRenderer
{
	struct EdgePair;

	namespace RClassic_Fixed
	{
		void setupInitCameraAndLights();

		void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId);
		void setResolution(s32 width, s32 height);
		void transformPointByCamera(vec3_fixed* worldPoint, vec3_fixed* viewPoint);
		void blitTextureToScreen(TextureData* texture, s32 x0, s32 y0);
	}  // RClassic_Fixed
}  // TFE_JediRenderer
