uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;

uniform sampler2D Colormap;	// The color map has full RGB pre-backed in.
uniform sampler2D Palette;
uniform sampler2D Textures;

uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;
flat in vec4 Frag_Color;
flat in int Frag_TextureId;
in vec3 Frag_Pos;
in vec4 Texture_Data;
out vec4 Out_Color;

vec3 getAttenuatedColor(int baseColor, int light)
{
	int color = baseColor;
	if (light < 31)
	{
		ivec2 uv = ivec2(color, light);
		color = int(texelFetch(Colormap, uv, 0).r * 256.0);
	}
	return texelFetch(Palette, ivec2(color, 0), 0).rgb;
}

int imod(int x, int y)
{
	return x - (x/y)*y;
}

float sampleTexture(int id, vec2 uv)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec2 iuv = ivec2(floor(uv));
	iuv.x = imod(iuv.x, sampleData.z);
	iuv.y = imod(iuv.y, sampleData.w);
	if (iuv.x < 0) iuv.x += sampleData.z;
	if (iuv.y < 0) iuv.y += sampleData.w;

	iuv = iuv + sampleData.xy;

	return floor(texelFetch(Textures, iuv, 0).r * 256.0);
}

void main()
{
    vec3 cameraRelativePos = Frag_Pos;
	vec2 uv = vec2(0.0);
	if (Frag_Uv.y > 1.5)
	{
		float s = length((Frag_Pos.xz + CameraPos.xz) - Texture_Data.xy) * Texture_Data.z;
		float t = Frag_Uv.x - Frag_Pos.y - CameraPos.y;
		uv.x = s * 8.0;
		uv.y = t * 8.0;
	}
	else if (Frag_Uv.y > 0.0)
	{
		// Project onto the floor or ceiling plane.
		float t = Frag_Uv.x / Frag_Pos.y;
		// Camera relative position on the plane, add CameraPos to get world space position.
		cameraRelativePos = t*Frag_Pos;

		uv = (cameraRelativePos.xz + CameraPos.xz - Texture_Data.xy) * 8.0;
	}
	else
	{
		uv = (cameraRelativePos.xz + CameraPos.xz - Texture_Data.xy) * 8.0;
	}

	float z = dot(cameraRelativePos, CameraDir);
	float ambient   = Frag_Color.r;
	float baseColor = Frag_Color.g;

	// Camera light and world ambient.
	float worldAmbient = floor(LightData.x + 0.5);
	float cameraLightSource = LightData.y;

	float light = 0.0;
	if (worldAmbient < 31.0 || cameraLightSource != 0.0)
	{
		float depthScaled = min(floor(z * 4.0), 127.0);
		float lightSource = 31.0 - texture(Colormap, vec2(depthScaled/256.0, 0.0)).g*256.0 + worldAmbient;
		if (lightSource > 0)
		{
			light += lightSource;
		}
	}
	light = max(light, ambient);

	float scaledAmbient = ambient * 7.0 / 8.0;	// approx, should be sector ambient.
	float depthAtten = floor(z / 16.0f) + floor(z / 32.0f);
	light = clamp(light - depthAtten, scaledAmbient, 31.0);

	// Use define.
	baseColor = sampleTexture(Frag_TextureId, uv);
	// End

	Out_Color.rgb = getAttenuatedColor(int(baseColor), int(light));
	Out_Color.a = 1.0;
}