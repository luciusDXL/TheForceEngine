#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "rlightingFloat.h"
#include "rclassicFloat.h"
#include "../rcommon.h"
#include "../rlimits.h"

namespace TFE_Jedi
{

namespace RClassic_Float
{
	#define LIGHT_SCALE 14
	#define LIGHT_ATTEN0 20
	#define LIGHT_ATTEN1 21

	CameraLightFlt s_cameraLight[] =
	{
		{ {0, 0, 1.0f}, {0, 0, 0}, 1.0f },
		{ {0, 1.0f, 0}, {0, 0, 0}, 1.0f },
		{ {1.0f, 0, 0}, {0, 0, 0}, 1.0f },
	};

	void light_transformDirLights()
	{
		vec3_float pos = { 0 };
		vec3_float viewPos;
		transformPointByCamera(&pos, &viewPos);

		for (s32 i = 0; i < s_lightCount; i++)
		{
			vec3_float dir;
			transformPointByCamera(&s_cameraLight[i].lightWS, &dir);
			s_cameraLight[i].lightVS.x = -(dir.x - viewPos.x);
			s_cameraLight[i].lightVS.y = -(dir.y - viewPos.y);
			s_cameraLight[i].lightVS.z = -(dir.z - viewPos.z);
			normalizeVec3(&s_cameraLight[i].lightVS, &s_cameraLight[i].lightVS);
		}
	}

	const u8* computeLighting(f32 depth, s32 lightOffset)
	{
		if (s_sectorAmbient >= MAX_LIGHT_LEVEL)	{ return nullptr; }
		if (s_fullBright) {	return &s_colorMap[(MAX_LIGHT_LEVEL - 1) << 8]; } // TFE fullbright cheat (LABRIGHT)
		depth = max(depth, 0.0f);
		s32 light = 0;

		// handle camera lightsource
		if (s_worldAmbient < MAX_LIGHT_LEVEL || s_cameraLightSource)
		{
			s32 depthScaled = min(s32(depth * 4.0f), 127);
			s32 lightSource = MAX_LIGHT_LEVEL - (s_lightSourceRamp[depthScaled] + s_worldAmbient);
			if (lightSource > 0)
			{
				light += lightSource;
			}
		}

		s32 secAmb = s_sectorAmbient;
		if (light < secAmb) { light = secAmb; }

		s32 depthAtten = s32(depth / 16.0f) + s32(depth / 32.0f);		// depth * 3/32
		light = max(light - depthAtten, s_scaledAmbient);

		if (lightOffset != 0)
		{
			light += lightOffset;
		}
		if (light >= MAX_LIGHT_LEVEL) { return nullptr; }
		light = max(light, 0);

		return &s_colorMap[light << 8];
	}
}  // RLightingFixed

}  // TFE_Jedi