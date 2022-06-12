uniform vec3 CameraPos;
uniform vec3 CameraRight;
uniform vec3 CameraDir;
uniform mat3 CameraView;
uniform mat4 CameraProj;
uniform vec4 LightData;

uniform mat3 ModelMtx;
uniform vec3 ModelPos;

uniform sampler2D Colormap;

// Vertex Data
in vec3 vtx_pos;
in vec3 vtx_nrm;
in vec2 vtx_uv;
in vec4 vtx_color;

out vec2 Frag_Uv;
out vec3 Frag_WorldPos;
noperspective out float Frag_Light;
flat out int Frag_Color;
flat out int Frag_TextureId;
flat out int Frag_TextureMode;

float directionalLighting(vec3 nrm, float scale)
{
	float lighting = 0.0;
	for (int i = 0; i < 3; i++)
	{
		vec3 lightDir = vec3((i == 0) ? -1.0 : 0.0, (i == 1) ? -1.0 : 0.0, (i==2) ? -1.0 : 0.0);
		float L = max(0.0, dot(nrm, lightDir));
		lighting += L * 31.0;
	}
	return floor(lighting * scale);
}

void main()
{
	// Transform by the model matrix.
	vec3 worldPos = vtx_pos * ModelMtx + ModelPos;

	// Transform from world to view space.
    vec3 vpos = (worldPos - CameraPos) * CameraView;
	gl_Position = vec4(vpos, 1.0) * CameraProj;

	// UV Coordinates.
	Frag_Uv = vtx_uv;

	// Lighting
	float ambient = LightData.z;
	float light = 0.0;
	int textureMode = int(vtx_color.w * 255.0 + 0.5);
	bool vertexLighting = (textureMode == 0);
	if (vertexLighting)
	{
		// Directional lights.
		vec3 nrm = vtx_nrm * ModelMtx;
		float ambientScale = floor(ambient * 2048.0) / 65536.0;	// ambient / 32, but quantizing the same way the software renderer does.

		float dirLighting = directionalLighting(nrm, ambientScale);
		light += dirLighting;
		
		// Calculate Z value and scaled ambient.
		float scaledAmbient = ambient * 7.0 / 8.0;
		float z = max(0.0, dot((worldPos - CameraPos), CameraDir));

		// Camera Light
		float worldAmbient = LightData.x;
		float cameraLightSource = LightData.y;
		if (worldAmbient < 31.0 || cameraLightSource > 0.0)
		{
			float depthScaled = min(floor(z * 4.0), 127.0);
			float lightSource = 31.0 - texture(Colormap, vec2(depthScaled/256.0, 0.0)).g*255.0 + worldAmbient;
			if (lightSource > 0)
			{
				light += lightSource;
			}
		}
		light = max(light, ambient);

		// Falloff
		float falloff = floor(z / 16.0) + floor(z / 32.0);
		light = max(light - falloff, scaledAmbient);
	}
	light = clamp(light, 0.0, 31.0);
	
	// Write out the per-vertex uv and color.
	Frag_WorldPos = worldPos;
	Frag_Color = int(vtx_color.x * 255.0 + 0.5);
	Frag_Light = vertexLighting ? light : ambient;
	Frag_TextureId = int(floor(vtx_color.y * 255.0 + 0.5) + floor(vtx_color.z * 255.0 + 0.5)*256.0 + 0.5);
	Frag_TextureMode = textureMode;
}
