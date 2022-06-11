uniform sampler2D Palette;
uniform sampler2D Colormap;
uniform sampler2D Textures;

uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;
in vec3 Frag_WorldPos;
flat in int Frag_Color;
flat in int Frag_TextureId;
flat in int Frag_TextureMode;

out vec4 Out_Color;

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
	ivec2 palColor = ivec2(Frag_Color, 0);
	if (Frag_TextureId < 65535) // 0xffff = no texture
	{
		vec2 uv = Frag_Uv;
		if (Frag_TextureMode > 0)
		{
			// Sector flat style projection.
			uv.x = Frag_WorldPos.x * 8.0;
			uv.y = Frag_WorldPos.z * 8.0;
		}

		palColor.x = int(sampleTexture(Frag_TextureId, uv));

		#ifdef MODEL_TRANSPARENT_PASS
		if (palColor.x == 0)
		{
			discard;
		}
		#endif
	}

	Out_Color.rgb = texelFetch(Palette, palColor, 0).rgb;
	Out_Color.a = 1.0;
}
