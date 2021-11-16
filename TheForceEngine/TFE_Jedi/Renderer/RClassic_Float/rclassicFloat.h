#pragma once
#include <TFE_System/types.h>

struct ColorMap;

namespace TFE_Jedi
{
	struct EdgePairFixed;

	namespace RClassic_Float
	{
		void resetState();
		void setupInitCameraAndLights(s32 width, s32 height);
		void changeResolution(s32 width, s32 height);

		void computeCameraTransform(RSector* sector, f32 pitch, f32 yaw, f32 camX, f32 camY, f32 camZ);
		void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint);
		void blitTextureToScreen(TextureData* texture, s32 x0, s32 y0);
		void clear3DView(u8* framebuffer);
		void setVisionEffect(s32 effect);
		void computeSkyOffsets();
	}  // RClassic_Float
}  // TFE_Jedi
