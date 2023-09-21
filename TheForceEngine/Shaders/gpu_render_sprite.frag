#include "Shaders/filter.h"
#include "Shaders/textureSampleFunc.h"
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
#ifdef OPT_BLOOM
	layout(location = 0) out vec4 Out_Color;
	layout(location = 1) out vec4 Out_Material;
#else
	out vec4 Out_Color;
#endif

void main()
{
    vec3 cameraRelativePos = Frag_Pos;
	float light = 31.0;

#ifdef OPT_TRUE_COLOR
	vec4 baseColor = vec4(0.0);
#else
	float baseColor = 0.0;
#endif

	// Handle shading.
	if (GlobalLightData.x == 0.0)
	{
		float z = dot(cameraRelativePos, CameraDir);
		float sectorAmbient = float(Texture_Data.y);

		// Only apply shading is sector ambient is less than max.
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
	if (discardPixel(baseColor, LightData.w)) { discard; }
	// Either discard very close to the iso-value or
	// do a two-pass filter - close cut with depth-write + alpha blend without depth-write.
	// if (baseColor.a < 0.48) { discard; }

	// Get the emissive factor (0 = normal, 1 = 100% fullbright).
	#ifdef OPT_TRUE_COLOR
		float emissive = clamp((baseColor.a - 0.5) * 2.0, 0.0, 1.0);
	#else
		float emissive = baseColor;
	#endif

	// Get the final lit color, enable solid color for wireframe.
	Out_Color = getFinalColor(baseColor, light, emissive);
	Out_Color.rgb = LightData.w > 0.5 ? vec3(0.6, 0.8, 0.6) : handlePaletteFx(Out_Color.rgb);

	// Handle pre-multiplied alpha blending.
	#ifdef OPT_TRUE_COLOR
	Out_Color.a = min(Out_Color.a * 2.008, 1.0);
	Out_Color.rgb *= Out_Color.a;
	#endif

	#ifdef OPT_BLOOM
		// Material (just emissive for now)
		Out_Material = getMaterialColor(emissive);
		#ifdef OPT_TRUE_COLOR
			Out_Material.a = Out_Color.a;
			Out_Material.rgb *= Out_Material.a;
		#endif
	#endif
}
