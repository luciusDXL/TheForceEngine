uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;
uniform float GridHeight;

in vec3 vtx_pos;
in vec2 vtx_uv;
in vec4 vtx_color;
out vec4 Frag_Color;
out vec2 Frag_Uv;
out vec3 Frag_Pos;
out float Frag_GridHeight;
void main()
{
    vec3 vpos = (vtx_pos - CameraPos) * CameraView;
	Frag_GridHeight = GridHeight - vtx_pos.y;

	gl_Position = vec4(vpos, 1.0) * CameraProj;
	Frag_Color = vtx_color;
	Frag_Uv = vtx_uv;
	Frag_Pos = vpos;
}
