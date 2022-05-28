#pragma once
#include <TFE_System/types.h>

struct ColorMap;
struct RSector;
struct TextureData;

namespace TFE_Jedi
{
	struct EdgePairFixed;

	namespace RClassic_Fixed
	{
		void resetState();
		void setupInitCameraAndLights();
		void changeResolution(s32 width, s32 height);

		void computeCameraTransform(RSector* sector, angle14_32 pitch, angle14_32 yaw, fixed16_16 camX, fixed16_16 camY, fixed16_16 camZ);
		void transformPointByCamera(vec3_fixed* worldPoint, vec3_fixed* viewPoint);
		void blitTextureToScreen(TextureData* texture, s32 x0, s32 y0);
		void clear3DView(u8* framebuffer);
		void setVisionEffect(s32 effect);
		void computeSkyOffsets();
	}  // RClassic_Fixed
}  // TFE_Jedi
