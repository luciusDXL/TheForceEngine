uniform sampler2D Palette;
uniform sampler2D Colormap;
uniform sampler2DArray Textures;

uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec2 LightData;
uniform vec4 TextureOffsets;

uniform isamplerBuffer TextureTable;

in vec2 Frag_Uv;
in vec3 Frag_WorldPos;
noperspective in float Frag_Light;
flat in int Frag_Color;
flat in int Frag_TextureId;
flat in int Frag_TextureMode;

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
	ivec3 iuv;
	iuv.xy = ivec2(uv);
	iuv.z = 0;

	iuv.xy = wrapCoord(iuv.xy, sampleData.zw);
	iuv.xy += (sampleData.xy & ivec2(4095));
	iuv.z = sampleData.x >> 12;

	return texelFetch(Textures, iuv, 0).r * 255.0;
}

void main()
{
	int baseColor = Frag_Color;
	int lightLevel = 31;
	if (Frag_TextureId < 65535) // 0xffff = no texture
	{
		vec2 uv = Frag_Uv;
		if (Frag_TextureMode > 0)
		{
			// Sector flat style projection.
			// TODO: Handle non-flat polygons with this projection...
			vec2 offset = TextureOffsets.xy;
			if (Frag_WorldPos.y < CameraPos.y)
			{
				offset = TextureOffsets.zw;
			}
			uv.xy = (Frag_WorldPos.xz - offset) * vec2(-8.0, 8.0);

			// Calculate Z value and scaled ambient.
			float ambient = max(0.0, LightData.y > 32.0 ? LightData.y - 64.0 : LightData.y);
			if (ambient < 31.0)
			{
				float light = 0.0;
				float scaledAmbient = ambient * 7.0 / 8.0;
				float z = max(0.0, dot((Frag_WorldPos - CameraPos), CameraDir));

				// Handle lighting in a similar way to sector floors and ceilings.
				// Camera Light
				float worldAmbient = LightData.x;
				float cameraLightSource = LightData.y > 63.0 ? 1.0 : 0.0;
				if (worldAmbient < 31.0 || cameraLightSource > 0.0)
				{
					float depthScaled = min(floor(z * 4.0), 127.0);
					float lightSource = 31.0 - (texture(Colormap, vec2(depthScaled/256.0, 0.0)).g*255.0 + worldAmbient);
					if (lightSource > 0)
					{
						light += lightSource;
					}
				}
				light = max(light, ambient);

				// Falloff
				float falloff = floor(z / 16.0) + floor(z / 32.0);
				lightLevel = int(clamp(light - falloff, scaledAmbient, 31.0));
			}
		}
		baseColor = int(sampleTexture(Frag_TextureId, uv));

		#ifdef MODEL_TRANSPARENT_PASS
		if (baseColor == 0)
		{
			discard;
		}
		#endif
	}
	if (Frag_TextureMode == 0)
	{
		float dither = float(int(gl_FragCoord.x) + int(gl_FragCoord.y) & 1);
		lightLevel = int(Frag_Light + 0.5*dither);
	}

	Out_Color.rgb = getAttenuatedColor(baseColor, lightLevel);
	Out_Color.a = 1.0;
}
