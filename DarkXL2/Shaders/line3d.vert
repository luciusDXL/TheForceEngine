uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;

in vec3 vtx_pos;		// vertex position.
in vec3 vtx_uv;			// line pos 0.
in vec3 vtx_uv1;		// line pos 1.
in vec3 vtx_uv2;		// vertex dir (+/-1,+/-1) + line width.
in vec4 vtx_color;		// vertex color.
out vec2 Frag_Uv;
out vec4 Frag_Color;

void main()
{
	vec3 vpos0 = (vtx_uv  - CameraPos) * CameraView;
	vec3 vpos1 = (vtx_uv1 - CameraPos) * CameraView;
	vec3 vpos  = (vtx_pos - CameraPos) * CameraView;

	vec4 ppos0 = vec4(vpos0, 1.0) * CameraProj;
	vec4 ppos1 = vec4(vpos1, 1.0) * CameraProj;
	vec4 ppos  = vec4(vpos,  1.0) * CameraProj;

	vec2 sp0 = ppos0.xy / ppos0.w;
	vec2 sp1 = ppos1.xy / ppos1.w;
	vec2 dir = normalize(sp1 - sp0);
	vec2 nrm = vec2(-dir.y, dir.x);

	// multiply the offset by 'w' so it gets canceled out during perspective division.
	vec4 offset = vec4((nrm*vtx_uv2.y) * vtx_uv2.z * ppos.w, 0.0, 0.0);
    gl_Position = ppos + offset;

    Frag_Color = vtx_color;
	Frag_Uv = vtx_uv2.xy;
}
