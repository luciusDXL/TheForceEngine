#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "../fixedPoint.h"
#include "../rmath.h"

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