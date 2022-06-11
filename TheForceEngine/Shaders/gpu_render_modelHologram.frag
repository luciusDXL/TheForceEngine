uniform sampler2D Palette;
flat in int Frag_Color;
out vec4 Out_Color;

void main()
{
	ivec2 uv = ivec2(Frag_Color, 0);

	Out_Color.rgb = texelFetch(Palette, uv, 0).rgb;
	Out_Color.a = 1.0;
}
