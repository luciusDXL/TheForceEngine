uniform vec4 ScaleOffset;
in vec2 vtx_pos;
out vec2 Frag_UV;
void main()
{
    Frag_UV = vtx_pos.xy * ScaleOffset.xy + ScaleOffset.zw;
    gl_Position = vec4(vtx_pos.xy * vec2(2.0) - vec2(1.0), 0, 1);
}
