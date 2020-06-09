#include "rlighting.h"
#include "fixedPoint.h"
#include "rmath.h"
#include "rcommon.h"

using namespace RendererClassic;
using namespace RMath;

namespace RClassicLighting
{
	const u8* computeLighting(s32 depth, s32 lightOffset)
	{
		if (s_sectorAmbient >= 31)
		{
			return nullptr;
		}
		depth = max(depth, 0);
		s32 light = 0;

		// handle camera lightsource
		if (s_worldAmbient < 31 && s_cameraLightSource != 0)
		{
			s32 depthScaled = min(depth >> 14, 127);
			s32 lightSource = 31 - (s_lightSourceRamp[depthScaled] + s_worldAmbient);
			if (lightSource > 0)
			{
				light += lightSource;
			}
		}

		s32 secAmb = s_sectorAmbient;
		if (light < secAmb) { light = secAmb; }

		// depthScale = 0.09375 (3/32)
		// light = max(light - depth * depthScale, secAmb*0.875)
		s32 depthAtten = (depth >> 20) + (depth >> 21);		// depth/16 + depth/32
		light = max(light - depthAtten, s_scaledAmbient);

		if (lightOffset != 0)
		{
			light += lightOffset;
		}
		if (light >= 31) { return nullptr; }

		return &s_colorMap[light << 8];
	}
}