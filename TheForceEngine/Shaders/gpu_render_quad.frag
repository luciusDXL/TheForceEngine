#include "Shaders/config.h"
#ifdef OPT_DYNAMIC_LIGHTING
#include "Shaders/lighting.h"
#endif
#include "Shaders/filter.h"

uniform sampler2D Colormap;
uniform sampler2D Palette;
uniform sampler2DArray Textures;
uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;		// base uv coordinates (0 - 1)
flat in vec4 Frag_TextureId_Color;
#ifdef OPT_DYNAMIC_LIGHTING
flat in vec3 Frag_Lighting;
#endif
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

void main()
{
	// Unpack the texture and color information.
	int textureId  = int(floor(Frag_TextureId_Color.x * 255.0 + 0.5) + floor(Frag_TextureId_Color.y * 255.0 + 0.5)*256.0 + 0.5);
	int color      = int(Frag_TextureId_Color.z * 255.0 + 0.5);
	int lightLevel = int(Frag_TextureId_Color.w * 255.0 + 0.5);

	// Sample the texture.
	int baseColor = color;
	if (textureId < 65535)
	{
		baseColor = int(sampleTextureClamp(textureId, Frag_Uv));
		if (baseColor < 1)
		{
			discard;
		}
	}

	// Get the final attenuated color.
	Out_Color.rgb = getAttenuatedColor(baseColor, lightLevel);
	Out_Color.a = 0.5;

#ifdef OPT_DYNAMIC_LIGHTING
	if (textureId < 65535 && color > 0 && baseColor >= 32)
	{
		vec3 albedo   = colorToLinear(texelFetch(Palette, ivec2(baseColor, 0), 0).rgb);
		vec3 ambient  = colorToLinear(Out_Color.rgb);
		Out_Color.rgb = linearToColor(Frag_Lighting * albedo + ambient);
	}

	// Write out the bloom mask.
	// TODO: Replace baseColor with emissive factor, derived from an emissive texture.
	// TODO: Write to a secondary render target.
	Out_Color.a = writeBloomMask(baseColor, 1.0);
#endif
}