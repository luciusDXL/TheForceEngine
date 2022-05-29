#pragma once
#include <TFE_System/types.h>

struct ColorMap;
struct RSector;
struct TextureData;

namespace TFE_Jedi
{
	namespace RClassic_GPU
	{
		void resetState();
		void setupInitCameraAndLights(s32 width, s32 height);
		void changeResolution(s32 width, s32 height);

		void computeCameraTransform(RSector* sector, f32 pitch, f32 yaw, f32 camX, f32 camY, f32 camZ);
		void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint);
		void computeSkyOffsets();
	}  // RClassic_GPU
}  // TFE_Jedi
