uniform vec4 ScaleOffset;
uniform vec4 GridAxis;
in vec2 vtx_pos;
out vec2 Frag_UV;

void main()
{
	vec2 xAxis = GridAxis.xy;
	vec2 zAxis = GridAxis.zw;
	vec2 uv = vtx_pos.xy * ScaleOffset.xy + ScaleOffset.zw;
	Frag_UV.x = uv.x * xAxis.x + uv.y * zAxis.x;
	Frag_UV.y = uv.x * xAxis.y + uv.y * zAxis.y;

    gl_Position = vec4(vtx_pos.xy * vec2(2.0) - vec2(1.0), 0, 1);
}
