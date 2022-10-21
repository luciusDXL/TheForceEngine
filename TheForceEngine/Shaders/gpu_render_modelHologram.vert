uniform vec3 CameraPos;
uniform vec3 CameraRight;
uniform mat3 CameraView;
uniform mat4 CameraProj;

uniform mat3 ModelMtx;
uniform vec3 ModelPos;
uniform uvec2 PortalInfo;

uniform samplerBuffer DrawListPlanes;

// Vertex Data
in vec3 vtx_pos;
in vec2 vtx_uv;
in vec4 vtx_color;

void unpackPortalInfo(uint portalInfo, out uint portalOffset, out uint portalCount)
{
	portalCount  = (portalInfo >> 13u) & 7u;
	portalOffset = portalInfo & 8191u;
}

flat out int Frag_Color;
void main()
{
	// Transform by the model matrix.
	vec3 worldPos = vtx_pos * ModelMtx + ModelPos;

	vec3 centerPos = (worldPos - CameraPos) * CameraView;
	float scale = abs(0.5/200.0 * centerPos.z);
	
	// Expand the quad, aligned to the right vector and world up.
	worldPos.xz -= vec2(scale) * CameraRight.xz;
	worldPos.xz += vec2(2.0*scale) * CameraRight.xz * vtx_uv.xx;
	worldPos.y  += scale - 2.0*scale * vtx_uv.y;

	// Clipping.
	uint portalOffset, portalCount;
	unpackPortalInfo(PortalInfo.x, portalOffset, portalCount);
	for (int i = 0; i < int(portalCount) && i < 8; i++)
	{
		vec4 plane = texelFetch(DrawListPlanes, int(portalOffset) + i);
		gl_ClipDistance[i] = dot(vec4(worldPos.xyz, 1.0), plane);
	}
	for (int i = int(portalCount); i < 8; i++)
	{
		gl_ClipDistance[i] = 1.0;
	}

	// Transform from world to view space.
    vec3 vpos = (worldPos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;
	
	// Write out the per-vertex uv and color.
	Frag_Color  = int(floor(vtx_color.x * 255.0 + 0.5));
}
