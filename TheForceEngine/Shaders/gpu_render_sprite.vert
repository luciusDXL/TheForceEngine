uniform vec3 CameraPos;
uniform vec3 CameraRight;
uniform mat3 CameraView;
uniform mat4 CameraProj;

uniform isamplerBuffer TextureTable;
uniform samplerBuffer DrawListPosXZ_Texture;
uniform samplerBuffer DrawListPosYU_Texture;
uniform isamplerBuffer DrawListTexId_Texture;

uniform samplerBuffer DrawListPlanes;

// in int gl_VertexID;
out vec2 Frag_Uv; // base uv coordinates (0 - 1)
out vec3 Frag_Pos;     // camera relative position for lighting.
out float gl_ClipDistance[6];
flat out vec4 Texture_Data; // not much here yet.
flat out int Frag_TextureId;

void unpackPortalInfo(uint portalInfo, out uint portalOffset, out uint portalCount)
{
	portalCount  = (portalInfo >> 13u) & 7u;
	portalOffset = portalInfo & 8191u;
}

void main()
{
	// We do our own vertex fetching and geometry expansion, so calculate the relevent values from the vertex ID.
	int spriteIndex = gl_VertexID / 4;
	int vertexId  = gl_VertexID & 3;
	
	vec4 posTextureXZ = texelFetch(DrawListPosXZ_Texture, spriteIndex);
	vec4 posTextureYU = texelFetch(DrawListPosYU_Texture, spriteIndex);
	uvec2 texPortalData = uvec2(texelFetch(DrawListTexId_Texture, spriteIndex).rg);
	uint tex_flags = texPortalData.x;
	Frag_TextureId = int(tex_flags & 32767u);

	float u = float(vertexId&1);
	float v = float(1-(vertexId/2));

	vec3 vtx_pos;
	vtx_pos.xz = mix(posTextureXZ.xy, posTextureXZ.zw, u);
	vtx_pos.y  = mix(posTextureYU.x, posTextureYU.y, v);

	vec2 vtx_uv;
	vtx_uv.x = mix(posTextureYU.z, posTextureYU.w, u);
	vtx_uv.y = v * float(texelFetch(TextureTable, Frag_TextureId).w);

	vec4 texture_data = vec4(0.0);
	texture_data.y = float((tex_flags >> 16u) & 31u);

	// Calculate vertical clipping.
	uint topId = texPortalData.y & 65535u;
	uint botId = texPortalData.y >> 16u;

	// For now just use the bottom id.
	uint portalOffset, portalCount;
	unpackPortalInfo(botId, portalOffset, portalCount);

	// Clipping.
	// TODO: Handle different top and bottom IDs.
	for (int i = 0; i < int(portalCount) && i < 6; i++)
	{
		vec4 plane = texelFetch(DrawListPlanes, int(portalOffset) + i);
		gl_ClipDistance[i] = dot(vec4(vtx_pos.xyz, 1.0), plane);
	}
	for (int i = int(portalCount); i < 6; i++)
	{
		gl_ClipDistance[i] = 1.0;
	}

	// Relative position
	Frag_Pos = vtx_pos - CameraPos;
	
	// Transform from world to view space.
    vec3 vpos = (vtx_pos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;
	
	// Write out the per-vertex uv and color.
	Frag_Uv = vtx_uv;
	Texture_Data = texture_data;
}
