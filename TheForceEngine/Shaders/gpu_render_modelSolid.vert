uniform vec3 CameraPos;
uniform vec3 CameraRight;
uniform mat3 CameraView;
uniform mat4 CameraProj;
uniform vec4 LightData;

uniform mat3 ModelMtx;
uniform vec3 ModelPos;

// Vertex Data
in vec3 vtx_pos;
in vec3 vtx_nrm;
in vec2 vtx_uv;
in vec4 vtx_color;

out vec2 Frag_Uv;
out vec3 Frag_WorldPos;
flat out ivec2 Frag_Color;
flat out int Frag_TextureId;
flat out int Frag_TextureMode;
void main()
{
	// Transform by the model matrix.
	vec3 worldPos = vtx_pos * ModelMtx + ModelPos;

	// Transform from world to view space.
    vec3 vpos = (worldPos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;

	// UV Coordinates.
	Frag_Uv = vtx_uv;

	// TODO: Lighting
	int ambient = int(LightData.z);
	
	// Write out the per-vertex uv and color.
	Frag_WorldPos = worldPos;
	Frag_Color.x = int(vtx_color.x * 255.0 + 0.5);
	Frag_Color.y = ambient;
	Frag_TextureId = int(floor(vtx_color.y * 255.0 + 0.5) + floor(vtx_color.z * 255.0 + 0.5)*256.0 + 0.5);
	Frag_TextureMode = int(vtx_color.w * 255.0 + 0.5);
}
