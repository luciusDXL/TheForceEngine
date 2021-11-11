#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

namespace TFE_Jedi
{
	namespace RClassic_Float
	{
		struct CameraLightFlt
		{
			vec3_float lightWS;
			vec3_float lightVS;
			f32 brightness;
		};
		extern CameraLightFlt s_cameraLight[];

		const u8* computeLighting(f32 depth, s32 lightOffset);
	}
}