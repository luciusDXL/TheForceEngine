#include "Shaders/config.h"
#include "Shaders/textureSampleFunc.h"
#ifdef OPT_DYNAMIC_LIGHTING
#include "Shaders/lighting.h"
#endif
#include "Shaders/filter.h"

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
			#ifdef OPT_SMOOTH_LIGHTRAMP // Smooth out light ramp.
				float depthScaled = min(z * 4.0, 127.0);
				float base = floor(depthScaled);

				// Cubic
				float d0 = max(0.0, base - 1.0);
				float d1 = base;
				float d2 = min(127.0, base + 1.0);
				float d3 = min(127.0, base + 2.0);
				float blendFactor = fract(depthScaled);
				
				float lightSource0 = 31.0 - (texture(Colormap, vec2(d0/256.0, 0.0)).g*255.0/8.23 + worldAmbient);
				float lightSource1 = 31.0 - (texture(Colormap, vec2(d1/256.0, 0.0)).g*255.0/8.23 + worldAmbient);
				float lightSource2 = 31.0 - (texture(Colormap, vec2(d2/256.0, 0.0)).g*255.0/8.23 + worldAmbient);
				float lightSource3 = 31.0 - (texture(Colormap, vec2(d3/256.0, 0.0)).g*255.0/8.23 + worldAmbient);

				float lightSource = cubicInterpolation(lightSource0, lightSource1, lightSource2, lightSource3, blendFactor);
			#else // Vanilla style light ramp.
				float depthScaled = min(floor(z * 4.0), 127.0);
				float lightSource = 31.0 - (texture(Colormap, vec2(depthScaled/256.0, 0.0)).g*255.0 + worldAmbient);
			#endif
				if (lightSource > 0)
				{
					light += lightSource;
				}
			}
			light = max(light, sectorAmbient);

			float minAmbient = sectorAmbient * 7.0 / 8.0;
		#ifdef OPT_COLORMAP_INTERP // Smooth out the attenuation.
			float depthAtten = z * 0.09375;
		#else
			float depthAtten = floor(z / 16.0f) + floor(z / 32.0f);
		#endif
			light = max(light - depthAtten, minAmbient);
			light = clamp(light, 0.0, 31.0);
		}
	}

	// Sample the texture.
	baseColor = sampleTextureClamp(Frag_TextureId, Frag_Uv);
	if (baseColor < 0.5 && LightData.w < 1.0)
	{
		discard;
	}

	#ifdef OPT_COLORMAP_INTERP
		Out_Color.rgb = getAttenuatedColorBlend(baseColor, light);
	#else
		Out_Color.rgb = getAttenuatedColor(int(baseColor), int(light));
	#endif
	#ifdef OPT_DYNAMIC_LIGHTING
	if (baseColor >= 32.0)
	{
		vec3 albedo   = colorToLinear(texelFetch(Palette, ivec2(baseColor, 0), 0).rgb);
		vec3 ambient  = colorToLinear(Out_Color.rgb);
		Out_Color.rgb = linearToColor(Frag_Lighting * albedo + ambient);
	}
	#endif

	// Write out the bloom mask.
	// TODO: Replace baseColor with emissive factor, derived from an emissive texture.
	// TODO: Write to a secondary render target.
	Out_Color.a = writeBloomMask(int(baseColor), 1.0);
}