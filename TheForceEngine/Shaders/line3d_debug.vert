uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;

in vec3 vtx_pos;		// vertex position.
in vec4 vtx_color;		// vertex color.
out vec4 Frag_Color;

void main()
{
	// Transform from world to view space.
    vec3 vpos = (vtx_pos.xyz - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;
	Frag_Color = vtx_color;
}
