in vec2 Frag_Uv;
out vec4 Out_Color;

void main()
{
    Out_Color = vec4(Frag_Uv, 0.0, 1.0);
}
