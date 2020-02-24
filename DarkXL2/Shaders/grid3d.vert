uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;
in vec3 vtx_pos;
out vec2 Frag_UV;
out vec3 Frag_Pos;
void main()
{
    Frag_UV.xy = vtx_pos.xz + CameraPos.xz;
	Frag_Pos = vtx_pos - vec3(0.0, CameraPos.y, 0.0);
	vec3 viewPos = Frag_Pos * CameraView;
    gl_Position = vec4(viewPos, 1.0) * CameraProj;
}
