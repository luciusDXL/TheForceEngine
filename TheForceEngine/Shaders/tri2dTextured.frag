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
		Out_Color *= texture(image, Frag_Uv);
	}
}
