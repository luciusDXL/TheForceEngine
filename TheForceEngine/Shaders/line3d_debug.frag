in vec4 Frag_Color;
out vec4 Out_Color;
void main()
{
	Out_Color.rgb = Frag_Color.rgb * Frag_Color.a;
	Out_Color.a = Frag_Color.a;
}
