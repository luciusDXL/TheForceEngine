uniform sampler2D Palette;
uniform sampler2D Colormap;
uniform sampler2D Textures;

uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;
in vec3 Frag_WorldPos;
noperspective in float Frag_Light;
flat in int Frag_Color;
flat in int Frag_TextureId;
flat in int Frag_TextureMode;

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

ivec2 imod(ivec2 x, ivec2 y)
{
	return x - (x/y)*y;
}

ivec2 wrapCoord(ivec2 uv, ivec2 edge)
{
	uv = imod(uv, edge);
	uv.x += (uv.x < 0) ? edge.x : 0;
	uv.y += (uv.y < 0) ? edge.y : 0;
	return uv;
}

float sampleTexture(int id, vec2 uv)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec2 iuv = ivec2(uv);

	iuv = wrapCoord(iuv, sampleData.zw);
	iuv = iuv + sampleData.xy;

	return texelFetch(Textures, iuv, 0).r * 255.0;
}

void main()
{
	int baseColor = Frag_Color;
	if (Frag_TextureId < 65535) // 0xffff = no texture
	{
		vec2 uv = Frag_Uv;
		if (Frag_TextureMode > 0)
		{
			// Sector flat style projection.
			uv.x = Frag_WorldPos.x * 8.0;
			uv.y = Frag_WorldPos.z * 8.0;

			// Handle lighting in a similar way to sector floors and ceilings.

		}

		baseColor = int(sampleTexture(Frag_TextureId, uv));

		#ifdef MODEL_TRANSPARENT_PASS
		if (baseColor == 0)
		{
			discard;
		}
		#endif
	}
	float dither = float(int(gl_FragCoord.x) + int(gl_FragCoord.y) & 1);
	int light = int(Frag_Light + 0.5*dither);

	Out_Color.rgb = getAttenuatedColor(baseColor, light);
	Out_Color.a = 1.0;
}
