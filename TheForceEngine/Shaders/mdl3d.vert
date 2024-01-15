uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;

uniform vec3 ObjPos;
uniform mat3 ObjTransform;

in vec3 vtx_pos;
in vec2 vtx_uv;
in vec4 vtx_color;

out vec4 Frag_Color;
out vec2 Frag_Uv;
out vec3 Frag_Pos;

void main()
{
	vec3 wpos = vtx_pos * ObjTransform + ObjPos;
    vec3 vpos = (wpos - CameraPos) * CameraView;
	
	gl_Position = vec4(vpos, 1.0) * CameraProj;

	Frag_Color = vtx_color;
	Frag_Uv = vtx_uv;
	Frag_Pos = wpos;
}
