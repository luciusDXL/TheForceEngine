#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#define LIGHT_SCALE 14
#define LIGHT_ATTEN0 20
#define LIGHT_ATTEN1 21

namespace TFE_Jedi
{
	namespace RClassic_Fixed
	{
		struct CameraLight
		{
			vec3_fixed lightWS;
			vec3_fixed lightVS;
			fixed16_16 brightness;
		};
		extern CameraLight s_cameraLight[];

		void light_transformDirLights();
		u8 getLightLevelFromAtten(const u8* atten);
		const u8* computeLighting(fixed16_16 depth, s32 lightOffset);
	}
}