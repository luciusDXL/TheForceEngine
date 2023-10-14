uniform vec3 CameraPos;
uniform mat3 CameraView;
uniform mat4 CameraProj;
uniform vec2 ScreenSize;

in vec3 vtx_pos;		// vertex position.
in vec3 vtx_uv;			// line pos 0.
in vec2 vtx_uv1;		// line pos 1.
in vec4 vtx_color;		// vertex color.
out vec4 Frag_Uv;
out float Frag_Width;
out vec4 Frag_Color;

void main()
{
	vec3 vpos0 = (vtx_pos - CameraPos) * CameraView;
	vec3 vpos1 = (vtx_uv - CameraPos) * CameraView;

	vec4 ppos0 = vec4(vpos0, 1.0) * CameraProj;
	vec4 ppos1 = vec4(vpos1, 1.0) * CameraProj;

	// Clip the line to the near plane.				
	//if (ppos0.w * ppos1.w < 0.0)
	if (ppos0.w < 0.0001 && ppos1.w < 0.0001)
	{
		ppos0 = vec4(0.0);
		ppos1 = vec4(0.0);
	}
	else if (ppos0.w < 0.0001 || ppos1.w < 0.0001)
	{
		if (ppos0.w < 0.0001)
		{
			ppos0 = mix(ppos0, ppos1, 0.0001-ppos0.w / (ppos1.w - ppos0.w));
		}
		else
		{
			ppos1 = mix(ppos1, ppos0, 0.0001-ppos1.w / (ppos0.w - ppos1.w));
		}
	}

	if (ppos0.w != 0.0) { ppos0.xyz /= ppos0.w; }
	if (ppos1.w != 0.0) { ppos1.xyz /= ppos1.w; }

	vec2 screenDir = normalize(ppos1.xy - ppos0.xy);
    vec2 screenNormal = vec2(-screenDir.y, screenDir.x);

	int vtxId = gl_VertexID & 3;
	int end = vtxId & 1;
	int top = vtxId / 2;

	gl_Position = (end == 0 ? ppos0 : ppos1);
	gl_Position.xy += (top == 0 ? -screenNormal * vtx_uv1.y : screenNormal * vtx_uv1.y);
	gl_Position.xyz *= gl_Position.w;

	// Remove the bias once the depth gets far enough away.
	float z = abs(end == 0 ? vpos0.z : vpos1.z);
	float biasScale = 1.0;
	// Reduce the bias as we get further away to avoid lines bleeding through walls.
	if (z > 100.0)
	{
		biasScale = clamp(1.0 - (z - 100.0) / 100.0, 0.0, 1.0);
	}
	float zbias = -0.0015 * biasScale;
	gl_Position.z += zbias;
	
	Frag_Uv.xy = (ppos0.xy * vec2(0.5,0.5) + 0.5) * ScreenSize;
	Frag_Uv.zw = (ppos1.xy * vec2(0.5,0.5) + 0.5) * ScreenSize;
	Frag_Width = vtx_uv1.x;

    Frag_Color = vtx_color;
}
