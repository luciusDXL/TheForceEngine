uniform sampler2D Palette;
uniform sampler2D Colormap;
uniform sampler2D Textures;

uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;
flat in int Frag_Color;
flat in int Frag_TextureId;

out vec4 Out_Color;

ivec2 imod(ivec2 x, ivec2 y)
{
	return x - (x/y)*y;
}

int wrapCoordScalar(int x, int edge)
{
	x = x - (x/edge)*edge;
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

float sampleTexture(int id, vec2 uv)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec2 iuv = ivec2(floor(uv));

	iuv = wrapCoord(iuv, sampleData.zw);
	iuv = iuv + sampleData.xy;

	return texelFetch(Textures, iuv, 0).r * 255.0;
}

void main()
{
	ivec2 uv = ivec2(Frag_Color, 0);
	if (Frag_TextureId < 65535)
	{
		uv.x = int(sampleTexture(Frag_TextureId, Frag_Uv));

		#ifdef MODEL_TRANSPARENT_PASS
		if (uv.x == 0)
		{
			discard;
		}
		#endif
	}

	Out_Color.rgb = texelFetch(Palette, uv, 0).rgb;
	Out_Color.a = 1.0;
}
