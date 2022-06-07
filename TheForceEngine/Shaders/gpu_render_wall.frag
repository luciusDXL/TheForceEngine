uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;

uniform sampler2D Colormap;	// The color map has full RGB pre-backed in.
uniform sampler2D Palette;
uniform sampler2D Textures;

uniform isamplerBuffer TextureTable;

flat in vec4 Frag_Uv;
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
		color = int(texelFetch(Colormap, uv, 0).r * 255.0);
	}
	return texelFetch(Palette, ivec2(color, 0), 0).rgb;
}

ivec2 imod(ivec2 x, ivec2 y)
{
	return x - (x/y)*y;
}

ivec2 wrapCoord(ivec2 uv, ivec2 edge)
{
	uv = imod(uv, edge);
	uv.x += (uv.x < 0) ? edge.x : 0;
	uv.y += (uv.y < 0) ? edge.y : 0;
	return uv;
}

float sampleTexture(int id, vec2 uv)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec2 iuv = ivec2(floor(uv));

	iuv = wrapCoord(iuv, sampleData.zw);
	iuv = iuv + sampleData.xy;

	return texelFetch(Textures, iuv, 0).r * 255.0;
}

void main()
{
    vec3 cameraRelativePos = Frag_Pos;
	vec2 uv = vec2(0.0);
	if (Frag_Uv.y > 1.5) // Wall
	{
		uv.x = length((Frag_Pos.xz + CameraPos.xz) - Texture_Data.xy) * Texture_Data.z;
		uv.y = Frag_Uv.x - Frag_Pos.y - CameraPos.y;
		uv *= 8.0;

		// Texture Offset
		uv += Frag_Uv.zw;
	}
	else if (Frag_Uv.y > 0.0) // Flat
	{
		// Project onto the floor or ceiling plane.
		float t = Frag_Uv.x / Frag_Pos.y;
		// Camera relative position on the plane, add CameraPos to get world space position.
		cameraRelativePos = t*Frag_Pos;

		uv = (cameraRelativePos.xz + CameraPos.xz - Texture_Data.xy)*vec2(-8.0, 8.0);
	}
	else // Cap
	{
		uv = (cameraRelativePos.xz + CameraPos.xz - Texture_Data.xy)*vec2(-8.0, 8.0);
	}

	float z = dot(cameraRelativePos, CameraDir);
	float lightOffset   = Frag_Color.r;
	float baseColor     = Frag_Color.g;
	float sectorAmbient = Frag_Color.b;

	// Camera light and world ambient.
	float worldAmbient = floor(LightData.x + 0.5);
	float cameraLightSource = LightData.y;

	float light = 0.0;
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
	light = max(light - depthAtten, minAmbient) + lightOffset;
	light = clamp(light, 0.0, 31.0);

	// Use define.
	baseColor = sampleTexture(Frag_TextureId, uv);
	// End

	Out_Color.rgb = getAttenuatedColor(int(baseColor), int(light));
	Out_Color.a = 1.0;
}