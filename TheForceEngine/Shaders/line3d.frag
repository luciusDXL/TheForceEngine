in vec2 Frag_Uv;
in vec4 Frag_Color;
out vec4 Out_Color;
void main()
{
	float alpha = smoothstep(1.0, 0.0, abs(Frag_Uv.y)) * Frag_Color.a;
    Out_Color = vec4(Frag_Color.rgb * alpha, alpha);
}
