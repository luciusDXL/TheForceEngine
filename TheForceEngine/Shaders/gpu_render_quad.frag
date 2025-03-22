#include "Shaders/filter.h"

uniform sampler2D Colormap;
uniform sampler2D Palette;
uniform sampler2DArray Textures;
uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;		// base uv coordinates (0 - 1)
flat in vec4 Frag_TextureId_Color;
#ifdef OPT_BLOOM
	layout(location = 0) out vec4 Out_Color;
	layout(location = 1) out vec4 Out_Material;
#else
	out vec4 Out_Color;
#endif
#ifdef OPT_TRUE_COLOR
	uniform vec4 IndexedColors[8];
#endif

#ifdef OPT_TRUE_COLOR
	vec3 trueColorMapping(int lightLevel, vec3 baseColor)
	{
		vec4 mulFactor = texelFetch(Palette, ivec2(lightLevel, 0), 0);
		vec4 fogFactor = texelFetch(Palette, ivec2(lightLevel + 32, 0), 0);

		// Multiply
		baseColor *= mulFactor.rgb;

		// Fog
		return mix(baseColor, fogFactor.rgb, fogFactor.a);
	}

	vec3 generateColor(vec3 baseColor, float lightLevel)
	{
		int l0 = min(31, int(lightLevel));
		int l1 = min(31, l0 + 1);
		float blendFactor = fract(lightLevel);

		// TODO: Reduce texture fetches.
		vec3 mapping0 = trueColorMapping(l0, baseColor);
		vec3 mapping1 = trueColorMapping(l1, baseColor);
		
		return mix(mapping0, mapping1, blendFactor);
	}

	vec3 getAttenuatedColor(vec3 baseColor, float lightLevel)
	{
		vec3 color = baseColor;
		if (lightLevel < 31.0)
		{
			color.rgb = generateColor(baseColor.rgb, lightLevel);
		}
		return color;
	}

	vec2 scaleUv(vec2 uv, int data)
	{
		float scale = max(1.0, float(data >> 12));
		uv *= scale;
		return uv;
	}

	vec4 sampleTextureClamp(int id, vec2 uv)
	{
		ivec4 sampleData = texelFetch(TextureTable, id);
		ivec3 iuv;
		uv = scaleUv(uv, sampleData.y);
		iuv.xy = ivec2(uv);
		iuv.z = 0;

		if ( any(lessThan(iuv.xy, ivec2(0))) || any(greaterThan(iuv.xy, sampleData.zw-1)) )
		{
			return vec4(0.0);
		}

		iuv.xy += (sampleData.xy & ivec2(4095));
		iuv.z = sampleData.x >> 12;
	
		return texelFetch(Textures, iuv, 0);
	}
#else
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

	float sampleTextureClamp(int id, vec2 uv)
	{
		ivec4 sampleData = texelFetch(TextureTable, id);
		ivec3 iuv;
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
#endif

void main()
{
	// Unpack the texture and color information.
	int textureId  = int(floor(Frag_TextureId_Color.x * 255.0 + 0.5) + floor(Frag_TextureId_Color.y * 255.0 + 0.5)*256.0 + 0.5);
	int color      = int(Frag_TextureId_Color.z * 255.0 + 0.5);
	int lightLevel = int(Frag_TextureId_Color.w * 255.0 + 0.5);

	bool indexed = false;
	if (lightLevel > 31)
	{
		lightLevel = lightLevel&31;
		indexed = true;
	}

	// Sample the texture.
#ifdef OPT_TRUE_COLOR
	vec4 baseColor = vec4(float(color));
	float emissive = 0.0;
	if (textureId < 65535)
	{
		baseColor = sampleTextureClamp(textureId, Frag_Uv);
		if (indexed)
		{
			int alpha = int(baseColor.a * 255.0 + 0.5);
			// store 8 indices in alpha channel.
			if (alpha >= 128 && alpha < 255)
			{
				int index = clamp((alpha - 128) >> 4, 0, 7);
				baseColor = IndexedColors[index];
			}
		}
		else
		{
			// Get the emissive factor (0 = normal, 1 = 100% fullbright).
			emissive = clamp((baseColor.a - 0.5) * 2.0, 0.0, 1.0);
		}

		if (baseColor.a < 0.01)
		{
			discard;
		}
	}
#else
	int baseColor = color;
	if (textureId < 65535)
	{
		baseColor = int(sampleTextureClamp(textureId, Frag_Uv));
		if (baseColor < 1)
		{
			discard;
		}
	}
#endif

	// Get the final attenuated color.
#ifdef OPT_TRUE_COLOR
	Out_Color.rgb = mix(getAttenuatedColor(baseColor.rgb, lightLevel), baseColor.rgb, emissive);
	Out_Color.rgb = handlePaletteFx(Out_Color.rgb);
#else
	Out_Color.rgb = getAttenuatedColor(baseColor, lightLevel);
#endif
	Out_Color.a = 1.0;

#ifdef OPT_BLOOM
	Out_Material = vec4(0.0);
#endif
}