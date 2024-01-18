uniform sampler2D image;
uniform int isTextured;
in vec2 Frag_Uv;
in vec4 Frag_Color;
out vec4 Out_Color;
void main()
{
    Out_Color = Frag_Color;
	if (int(isTextured) == int(1))
	{
		vec4 texColor = texture(image, Frag_Uv);
		Out_Color *= texColor;
		Out_Color.rgb *= texColor.a;
	}
}
