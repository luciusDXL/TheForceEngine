uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;
uniform float GridHeight;
in vec3 vtx_pos;
out vec2 Frag_UV;
out vec3 Frag_Pos;
out vec3 View_Pos;
out vec3 View_Up;
out float Plane_Dist;
void main()
{
    Frag_UV.xy = vtx_pos.xz + CameraPos.xz;
	Frag_Pos = vtx_pos + vec3(0.0, GridHeight - CameraPos.y, 0.0);
	Plane_Dist = abs(GridHeight - CameraPos.y);
	View_Pos = Frag_Pos * CameraView;
	View_Up = vec3(CameraView[0].y, CameraView[1].y, CameraView[2].y);
    gl_Position = vec4(View_Pos, 1.0) * CameraProj;
}
