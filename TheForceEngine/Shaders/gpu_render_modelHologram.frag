uniform sampler2D Palette;
flat in int Frag_Color;
#ifdef OPT_BLOOM
layout(location = 0) out vec4 Out_Color;
layout(location = 1) out vec4 Out_Material;
#else
out vec4 Out_Color;
#endif

void main()
{
	ivec2 uv = ivec2(Frag_Color, 0);

	Out_Color.rgb = texelFetch(Palette, uv, 0).rgb;
	Out_Color.a = 1.0;

#ifdef OPT_BLOOM
	// Material (just emissive for now)
	Out_Material = vec4(0.0);
	Out_Material.x = 1.0;
#endif
}
