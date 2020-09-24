uniform sampler2D VirtualDisplay;

#ifdef ENABLE_GPU_COLOR_CONVERSION
uniform sampler2D Palette;
#endif

in vec2 Frag_UV;
out vec4 Out_Color;

void main()
{
#ifdef ENABLE_GPU_COLOR_CONVERSION
	// read the color index, it will be 0.0 - 255.0/256.0 range which maps to 0 - 255
	float index = texture(VirtualDisplay, Frag_UV).r;
	Out_Color.rgb = texture(Palette, vec2(index, 0.5));
#else
	Out_Color.rgb = texture(VirtualDisplay, Frag_UV).rgb;
#endif

	Out_Color.a = 1.0;
}
