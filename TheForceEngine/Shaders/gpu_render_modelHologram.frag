uniform sampler2D Palette;
#ifdef OPT_TRUE_COLOR
flat in vec3 Frag_Color;
#else
flat in int Frag_Color;
#endif

#ifdef OPT_BLOOM
layout(location = 0) out vec4 Out_Color;
layout(location = 1) out vec4 Out_Material;
#else
out vec4 Out_Color;
#endif

void main()
{
	#ifdef OPT_TRUE_COLOR
		Out_Color.rgb = Frag_Color;
	#else
		Out_Color.rgb = texelFetch(Palette, ivec2(Frag_Color, 0), 0).rgb;
	#endif
	Out_Color.a = 1.0;

#ifdef OPT_BLOOM
	// Material (just emissive for now)
	Out_Material = vec4(0.0);
	Out_Material.x = 1.0;
#endif
}
