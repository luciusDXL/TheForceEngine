uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;

in vec3 vtx_pos;
in vec2 vtx_uv;
in vec2 vtx_uv1;
in vec2 vtx_uv2;
in vec4 vtx_color;
out vec4 Frag_Color;
out vec2 Frag_Uv;
out vec2 Frag_Uv1;
out vec2 Frag_Uv2;
out vec3 Frag_Pos;
void main()
{
    vec3 vpos = (vtx_pos - CameraPos) * CameraView;
	
	gl_Position = vec4(vpos, 1.0) * CameraProj;
	Frag_Color = vtx_color;
	Frag_Uv = vtx_uv;
	Frag_Uv1 = vtx_uv1;
	Frag_Uv2 = vtx_uv2;
	Frag_Pos = vpos;
}
