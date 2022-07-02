uniform vec3 CameraPos;
uniform vec3 CameraRight;
uniform mat3 CameraView;
uniform mat4 CameraProj;

uniform isamplerBuffer TextureTable;
#ifdef SPRITE_GPU_CLIPPING
	uniform samplerBuffer DrawListPosTexture;
	uniform samplerBuffer DrawListScaleOffset;
#else
	uniform samplerBuffer DrawListPosXZ_Texture;
	uniform samplerBuffer DrawListPosYU_Texture;
	uniform isamplerBuffer DrawListTexId_Texture;
#endif

// in int gl_VertexID;
out vec2 Frag_Uv; // base uv coordinates (0 - 1)
out vec3 Frag_Pos;     // camera relative position for lighting.
flat out vec4 Texture_Data; // not much here yet.
flat out int Frag_TextureId;
void main()
{
	// We do our own vertex fetching and geometry expansion, so calculate the relevent values from the vertex ID.
	int spriteIndex = gl_VertexID / 4;
	int vertexId  = gl_VertexID & 3;
	
#ifdef SPRITE_GPU_CLIPPING
	// Read part position and data.
	vec4 posTexture  = texelFetch(DrawListPosTexture,  spriteIndex);
	vec4 scaleOffset = texelFetch(DrawListScaleOffset, spriteIndex);

	// Unpack sprite data.
	uint tex_flags = uint(floor(posTexture.w + 0.5));
	Frag_TextureId = int(tex_flags & 32767u);
	vec2 texWidth = vec2(texelFetch(TextureTable, Frag_TextureId).zw);

	vec4 texture_data = vec4(0.0);
	vec2 vtx_uv = vec2(float(vertexId&1), float(1-(vertexId/2))) * texWidth;
	if ((tex_flags & 2097152u) != 0u)
	{
		vtx_uv.x = texWidth.x - vtx_uv.x - 1.0;
	}
	texture_data.y = float((tex_flags >> 16u) & 31u);

	// Expand the quad, aligned to the right vector and world up.
	vec3 vtx_pos = posTexture.xyz;
	vtx_pos.xz -= vec2(scaleOffset.x) * CameraRight.xz;
	vtx_pos.xz += vec2(scaleOffset.z) * CameraRight.xz * float(vertexId&1);
	vtx_pos.y = vtx_pos.y + scaleOffset.y - scaleOffset.w * float(1-(vertexId/2));
#else
	vec4 posTextureXZ = texelFetch(DrawListPosXZ_Texture, spriteIndex);
	vec4 posTextureYU = texelFetch(DrawListPosYU_Texture, spriteIndex);
	uint tex_flags = uint(texelFetch(DrawListTexId_Texture, spriteIndex).r);
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
#endif

	// Relative position
	Frag_Pos = vtx_pos - CameraPos;
	
	// Transform from world to view space.
    vec3 vpos = (vtx_pos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;
	
	// Write out the per-vertex uv and color.
	Frag_Uv = vtx_uv;
	Texture_Data = texture_data;
}
