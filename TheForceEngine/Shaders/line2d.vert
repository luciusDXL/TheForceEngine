uniform vec4 ScaleOffset;
in vec3 vtx_pos;
in vec4 vtx_uv;
in vec4 vtx_color;
out float Frag_Width;
out vec4 Frag_UV;
out vec2 Frag_Pos;
out vec4 Frag_Color;
void main()
{
    vec2 pos = vtx_pos.xy * ScaleOffset.xy + ScaleOffset.zw;
    Frag_Width = vtx_pos.z;
    Frag_Pos = vtx_pos.xy;
    Frag_UV = vtx_uv;
    Frag_Color = vtx_color;
    gl_Position = vec4(pos.xy, 0, 1);
}
