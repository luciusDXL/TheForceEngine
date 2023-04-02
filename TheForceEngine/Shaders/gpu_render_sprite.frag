#include "Shaders/config.h"
#ifdef OPT_DYNAMIC_LIGHTING
#include "Shaders/lighting.h"
#endif
#if defined(OPT_BILINEAR_DITHER) || defined(OPT_SMOOTH_LIGHTRAMP)
#include "Shaders/filter.h"
#endif

uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;
uniform vec4 GlobalLightData;	// x = flat lighting, y = flat light value.
uniform vec2 SkyParallax;

uniform sampler2D Colormap;	// The color map has full RGB pre-backed in.
uniform sampler2D Palette;
uniform sampler2DArray Textures;

uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;
in vec3 Frag_Pos;
flat in vec3 Frag_Lighting;

flat in int Frag_TextureId;
flat in vec4 Texture_Data;
out vec4 Out_Color;

vec3 getAttenuatedColor(int baseColor, int light)
{
	int color = baseColor;
	if (light < 31)
	{
		ivec2 uv = ivec2(color, light);
		color = int(texelFetch(Colormap, uv, 0).r * 255.0);
	}
	return texelFetch(Palette, ivec2(color, 0), 0).rgb;
}

vec3 getAttenuatedColorBlend(float baseColor, float light)
{
	int color = int(baseColor);
	if (light < 31)
	{
		int l0 = int(light);
		int l1 = min(31, l0 + 1);
		float blendFactor = fract(light);

		ivec4 uv = ivec4(color, l0, color, l1);
		int color0 = int(texelFetch(Colormap, uv.xy, 0).r * 255.0);
		int color1 = int(texelFetch(Colormap, uv.zw, 0).r * 255.0);

		vec3 value0 = texelFetch(Palette, ivec2(color0, 0), 0).rgb;
		vec3 value1 = texelFetch(Palette, ivec2(color1, 0), 0).rgb;
		return mix(value0, value1, blendFactor);
		//return vec3(light/31.0);
	}
	//return vec3(1.0);
	return texelFetch(Palette, ivec2(color, 0), 0).rgb;
}

float sampleTextureClamp(int id, vec2 uv)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec3 iuv;

	#ifdef OPT_BILINEAR_DITHER
	uv = bilinearDither(uv);
	#endif

	iuv.xy = ivec2(uv);
	iuv.z = 0;

	if ( any(lessThan(iuv.xy, ivec2(0))) || any(greaterThan(iuv.xy, sampleData.zw-1)) )
	{
		return 0.0;
	}

	iuv.xy += (sampleData.xy & ivec2(4095));
	iuv.z = sampleData.x >> 12;
	
	return texelFetch(Textures, iuv, 0).r * 255.0;
}

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
	if (baseColor >= 16.0)
	{
		vec3 albedo   = colorToLinear(texelFetch(Palette, ivec2(baseColor, 0), 0).rgb);
		vec3 ambient  = colorToLinear(Out_Color.rgb);
		Out_Color.rgb = linearToColor(Frag_Lighting * albedo + ambient);
	}
	#endif
	Out_Color.a = 1.0;
}