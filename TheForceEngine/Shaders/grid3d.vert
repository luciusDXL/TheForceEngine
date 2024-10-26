uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;
uniform float GridHeight;
uniform vec4 GridAxis;
uniform vec2 GridOrigin;
in vec3 vtx_pos;
out vec2 Frag_UV;
out vec3 Frag_Pos;
out vec3 View_Pos;
out vec3 View_Up;

vec2 posToGrid(vec2 pos)
{
	vec2 offset = pos - GridOrigin;
	vec2 gridPos;
	gridPos.x = offset.x * GridAxis.x + offset.y * GridAxis.y;
	gridPos.y = offset.x * GridAxis.z + offset.y * GridAxis.w;
	return gridPos;
}

void main()
{
    Frag_UV.xy = posToGrid(vtx_pos.xz + CameraPos.xz);
	
	Frag_Pos = vtx_pos + vec3(0.0, GridHeight - CameraPos.y, 0.0);
	View_Pos = Frag_Pos * CameraView;
	View_Up = vec3(CameraView[0].y, CameraView[1].y, CameraView[2].y);
    gl_Position = vec4(View_Pos, 1.0) * CameraProj;
	// Z-Bias to show over geometry.
	gl_Position.z -= 0.0005;
}
