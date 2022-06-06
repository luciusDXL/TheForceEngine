uniform vec3 CameraPos;
uniform vec3 CameraDir;

in vec2 Frag_Uv;
in vec4 Frag_Color;
in vec3 Frag_Pos;
out vec4 Out_Color;

void main()
{
    Out_Color = Frag_Color;

	float z;
	if (Frag_Uv.y > 0.0)
	{
		// Project onto the floor or ceiling plane.
		float t = Frag_Uv.x / Frag_Pos.y;
		vec3 posOnPlane = CameraPos + t*Frag_Pos;
		// Get planar distance.
		z = dot(posOnPlane - CameraPos, CameraDir);
	}
	else
	{
		z = dot(Frag_Pos, CameraDir);
	}
	Out_Color.rgb *= clamp(1.0 - z*0.01, 0.5, 1.0);
}
