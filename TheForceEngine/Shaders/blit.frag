uniform sampler2D VirtualDisplay;
in vec2 Frag_UV;
out vec4 Out_Color;
void main()
{
    Out_Color.rgb = texture(VirtualDisplay, Frag_UV).rgb;
	Out_Color.a = 1.0;
}
