#pragma once
#include <TFE_System/types.h>

struct ColorMap;

namespace TFE_Jedi
{
	struct EdgePairFixed;

	namespace RClassic_Float
	{
		void setupInitCameraAndLights();

		//void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId);
		void computeCameraTransform(RSector* sector, angle14_32 pitch, angle14_32 yaw, fixed16_16 camX, fixed16_16 camY, fixed16_16 camZ);
		// void setResolution(s32 width, s32 height);
		void transformPointByCamera(vec3_fixed* worldPoint, vec3_fixed* viewPoint);
		void blitTextureToScreen(TextureData* texture, s32 x0, s32 y0);
		void clear3DView(u8* framebuffer);
		void setVisionEffect(s32 effect);
		void computeSkyOffsets();
	}  // RClassic_Float
}  // TFE_Jedi
