uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;
uniform vec2 SkyParallax;

uniform sampler2D Colormap;	// The color map has full RGB pre-backed in.
uniform sampler2D Palette;
uniform sampler2D Textures;

uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;
in vec3 Frag_Pos;

flat in int Frag_TextureId;
flat in vec4 Texture_Data;
out vec4 Out_Color;

vec3 getAttenuatedColor(int baseColor, int light)
{
	int color = baseColor;
	if (light < 31)
	{
		ivec2 uv = ivec2(color, light);
		color = int(texelFetch(Colormap, uv, 0).r * 255.0);
	}
	return texelFetch(Palette, ivec2(color, 0), 0).rgb;
}

float sampleTextureClamp(int id, vec2 uv)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec2 iuv = ivec2(floor(uv * vec2(sampleData.zw)));
	if ( any(lessThan(iuv, ivec2(0))) || any(greaterThan(iuv, sampleData.zw-1)) )
	{
		return 0.0;
	}
	iuv += sampleData.xy;
	return texelFetch(Textures, iuv, 0).r * 255.0;
}

void main()
{
    vec3 cameraRelativePos = Frag_Pos;

	float light = 0.0;
	float baseColor = 0.0;

	float z = dot(cameraRelativePos, CameraDir);
	float sectorAmbient = float(Texture_Data.y);

	// Camera light and world ambient.
	float worldAmbient = floor(LightData.x + 0.5);
	float cameraLightSource = LightData.y;

	if (worldAmbient < 31.0 || cameraLightSource != 0.0)
	{
		float depthScaled = min(floor(z * 4.0), 127.0);
		float lightSource = 31.0 - texture(Colormap, vec2(depthScaled/256.0, 0.0)).g*255.0 + worldAmbient;
		if (lightSource > 0)
		{
			light += lightSource;
		}
	}
	light = max(light, sectorAmbient);

	float minAmbient = sectorAmbient * 7.0 / 8.0;
	float depthAtten = floor(z / 16.0f) + floor(z / 32.0f);
	light = max(light - depthAtten, minAmbient);
	light = clamp(light, 0.0, 31.0);

	// Sample the texture.
	baseColor = sampleTextureClamp(Frag_TextureId, Frag_Uv);
	if (baseColor < 0.5)
	{
		discard;
	}

	Out_Color.rgb = getAttenuatedColor(int(baseColor), int(light));
	Out_Color.a = 1.0;
}