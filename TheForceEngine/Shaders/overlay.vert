uniform vec4 ScaleOffset;
uniform vec4 UvOffsetSize;
in vec2 vtx_pos;
in vec2 vtx_uv;

out vec2 Frag_UV;

void main()
{
    Frag_UV = vtx_uv * UvOffsetSize.xy + UvOffsetSize.zw;
    gl_Position = vec4(vtx_pos.xy * ScaleOffset.xy + ScaleOffset.zw, 0, 1);
}
