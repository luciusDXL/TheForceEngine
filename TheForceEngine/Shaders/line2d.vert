uniform vec4 ScaleOffset;

in vec4 vtx_pos;
in vec4 vtx_uv;
in vec4 vtx_color;
#ifdef OPT_CURVE
	flat out vec4 Frag_ControlAB;
	flat out vec2 Frag_ControlC;
#else
	out float Frag_Width;
	out vec4 Frag_UV;
#endif
out vec2 Frag_Pos;
out vec4 Frag_Color;

void main()
{
    vec2 pos = vtx_pos.xy * ScaleOffset.xy + ScaleOffset.zw;
#ifdef OPT_CURVE
	Frag_ControlAB = vtx_uv;
	Frag_ControlC = vtx_pos.zw;
#else  // Normal Line
    Frag_Width = vtx_pos.z;
    Frag_UV = vtx_uv;
#endif
	Frag_Pos = vtx_pos.xy;
	Frag_Color = vtx_color;
    gl_Position = vec4(pos.xy, 0, 1);
}
