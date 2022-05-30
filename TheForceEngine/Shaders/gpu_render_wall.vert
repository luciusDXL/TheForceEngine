uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;

in vec3 vtx_pos;
in vec2 vtx_uv;
out vec2 Frag_Uv;
void main()
{
    vec3 vpos = (vtx_pos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;

	Frag_Uv = vtx_uv;
}
