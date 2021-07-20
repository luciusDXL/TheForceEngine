#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Level/fixedPoint.h>
#include <TFE_Level/core_math.h>

using namespace TFE_CoreMath;

namespace TFE_JediRenderer
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

		const u8* computeLighting(fixed16_16 depth, s32 lightOffset);
	}
}