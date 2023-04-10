uniform sampler2DArray Textures;
uniform isamplerBuffer TextureTable;

uniform sampler2D Colormap;	// The color map has full RGB pre-backed in.
uniform sampler2D Palette;

vec3 fmod(vec3 x, vec3 y)
{
	return x - floor(x / y)*y;
}

ivec2 imod(ivec2 x, ivec2 y)
{
	return x - (x / y)*y;
}

int wrapCoordScalar(int x, int edge)
{
	x = x - (x / edge)*edge;
	x += (x < 0) ? edge : 0;
	return x;
}

ivec2 wrapCoord(ivec2 uv, ivec2 edge)
{
	uv = imod(uv, edge);
	uv.x += (uv.x < 0) ? edge.x : 0;
	uv.y += (uv.y < 0) ? edge.y : 0;
	return uv;
}

// Approximate the distortion that happens in software when floor and ceiling textures are not 64x64.
ivec2 wrapCoordFlat(ivec2 uv, ivec2 edge)
{
	int coord = (uv.x & 63) * 64 + (uv.y & 63);
	uv = wrapCoord(ivec2(coord / edge.y, coord), edge);
	uv.x += (uv.x < 0) ? edge.x : 0;
	uv.y += (uv.y < 0) ? edge.y : 0;
	return uv;
}

// Approximate a bilinear filter using a screenspace dither
// This technique was first seen in Unreal 1 (as far as I know).
vec2 bilinearDither(vec2 uv)
{
	vec2 bilinearOffset[4] = vec2[4](
		vec2(0.25, 0.00), vec2(0.50, 0.75),
		vec2(0.75, 0.50), vec2(0.00, 0.25));

	int ix = int(gl_FragCoord.x) & 1;
	int iy = int(gl_FragCoord.y) & 1;
	uv += bilinearOffset[iy * 2 + ix];

	return uv;
}

float sampleTexture(int id, vec2 uv)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec3 iuv;

#ifdef OPT_BILINEAR_DITHER
	uv = bilinearDither(uv);
#endif

	iuv.xy = ivec2(uv);
	iuv.z = 0;

	iuv.xy = wrapCoord(iuv.xy, sampleData.zw);
	iuv.xy += (sampleData.xy & ivec2(4095));
	iuv.z = sampleData.x >> 12;

	return texelFetch(Textures, iuv, 0).r * 255.0;
}

float sampleTexture(int id, vec2 uv, bool sky, bool flip, bool applyFlatWarp)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec3 iuv;

#ifdef OPT_BILINEAR_DITHER
	uv = bilinearDither(uv);
#endif

	iuv.xy = ivec2(floor(uv));
	iuv.z = 0;

	if (sky)
	{
		if (abs(iuv.y) >= 9999)
		{
			// TODO: Single sample for the whole area.
			iuv.xy = ivec2(sampleData.z / 2, sampleData.w / 2);
		}
		else
		{
			iuv.x = wrapCoordScalar(iuv.x, sampleData.z);
			iuv.y = wrapCoordScalar(iuv.y, sampleData.w);
		}
	}
	else if (applyFlatWarp)
	{
		iuv.xy = wrapCoordFlat(iuv.xy, sampleData.zw);
		if (flip)
		{
			iuv.x = sampleData.z - iuv.x - 1;
		}
	}
	else
	{
		iuv.xy = wrapCoord(iuv.xy, sampleData.zw);
		if (flip)
		{
			iuv.x = sampleData.z - iuv.x - 1;
		}
	}
	iuv.xy += (sampleData.xy & ivec2(4095));
	iuv.z = sampleData.x >> 12;

	return texelFetch(Textures, iuv, 0).r * 255.0;
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

	if (any(lessThan(iuv.xy, ivec2(0))) || any(greaterThan(iuv.xy, sampleData.zw - 1)))
	{
		return 0.0;
	}

	iuv.xy += (sampleData.xy & ivec2(4095));
	iuv.z = sampleData.x >> 12;

	return texelFetch(Textures, iuv, 0).r * 255.0;
}

float sampleTextureClamp(int id, vec2 uv, bool opaque)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec3 iuv;

#ifdef OPT_BILINEAR_DITHER
	uv = bilinearDither(uv);
#endif

	iuv.xy = ivec2(floor(uv));
	iuv.z = 0;

	if (any(lessThan(iuv.xy, ivec2(0))) || any(greaterThan(iuv.xy, sampleData.zw - 1)))
	{
		return 0.0;
	}
	iuv.xy += (sampleData.xy & ivec2(4095));
	iuv.z = sampleData.x >> 12;

	float value = texelFetch(Textures, iuv, 0).r * 255.0;
	if (opaque) { value = max(0.75, value); }	// avoid clipping if opaque.
	return value;
}

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
	}
	return texelFetch(Palette, ivec2(color, 0), 0).rgb;
}
