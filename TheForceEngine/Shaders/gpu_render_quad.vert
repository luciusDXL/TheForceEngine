uniform vec4 ScaleOffset;

in vec4 vtx_pos;
in vec4 vtx_color;

out vec2 Frag_Uv;		// base uv coordinates (0 - 1)
flat out vec4 Frag_TextureId_Color;

void main()
{
	Frag_Uv = vtx_pos.zw;
	Frag_TextureId_Color = vtx_color;

	vec2 pos = vtx_pos.xy*ScaleOffset.xy + ScaleOffset.zw;
	gl_Position = vec4(pos.xy, 0, 1);
}
