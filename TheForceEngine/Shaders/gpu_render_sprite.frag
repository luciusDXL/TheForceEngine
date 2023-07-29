#include "Shaders/textureSampleFunc.h"
#include "Shaders/filter.h"
#include "Shaders/lighting.h"

uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;
uniform vec4 GlobalLightData;	// x = flat lighting, y = flat light value.
uniform vec2 SkyParallax;

in vec2 Frag_Uv;
in vec3 Frag_Pos;
flat in vec3 Frag_Lighting;

flat in int Frag_TextureId;
flat in vec4 Texture_Data;
out vec4 Out_Color;

void main()
{
    vec3 cameraRelativePos = Frag_Pos;

	float light = 31.0;
	float baseColor = 0.0;

	if (GlobalLightData.x == 0.0)
	{
		float z = dot(cameraRelativePos, CameraDir);
		float sectorAmbient = float(Texture_Data.y);

		if (sectorAmbient < 31.0)
		{
			// Camera light and world ambient.
			float worldAmbient = floor(LightData.x + 0.5);
			float cameraLightSource = LightData.y;

			light = 0.0;
			if (worldAmbient < 31.0 || cameraLightSource != 0.0)
			{
				float lightSource = getLightRampValue(z, worldAmbient);
				if (lightSource > 0)
				{
					light += lightSource;
				}
			}
			light = max(light, sectorAmbient);
			light = getDepthAttenuation(z, sectorAmbient, light, 0.0);
		}
	}

	// Sample the texture.
	baseColor = sampleTextureClamp(Frag_TextureId, Frag_Uv);
	if (baseColor < 0.5 && LightData.w < 1.0) { discard; }

	Out_Color = getFinalColor(baseColor, light);
	// Enable solid color rendering for wireframe.
	Out_Color.rgb = LightData.w > 0.5 ? vec3(0.6, 0.8, 0.6) : Out_Color.rgb;
}