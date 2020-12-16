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
	namespace RClassic_Float
	{
		struct CameraLight
		{
			vec3_float lightWS;
			vec3_float lightVS;
			f32 brightness;
		};
		extern CameraLight s_cameraLight[];

		const u8* computeLighting(f32 depth, s32 lightOffset);
	}
}