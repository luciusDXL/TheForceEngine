#include "Shaders/config.h"
#include "Shaders/textureSampleFunc.h"
#if defined(OPT_BILINEAR_DITHER) || defined(OPT_SMOOTH_LIGHTRAMP)
#include "Shaders/filter.h"
#endif

uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec2 LightData;
uniform vec4 TextureOffsets;

in vec2 Frag_Uv;
in vec3 Frag_WorldPos;
noperspective in float Frag_Light;
flat in float Frag_ModelY;
flat in int Frag_Color;
flat in int Frag_TextureId;
flat in int Frag_TextureMode;

out vec4 Out_Color;

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
			float planeY = uv.x + Frag_ModelY;
			vec2 offset = TextureOffsets.xy;
			if (planeY < CameraPos.y)
			{
				offset = TextureOffsets.zw;
			}

			// Intersect the eye with the plane at planeY.
			float t = (planeY - CameraPos.y) / (Frag_WorldPos.y - CameraPos.y);
			vec2 posXZ = CameraPos.xz + t*(Frag_WorldPos.xz - CameraPos.xz);
			uv.xy = (posXZ - offset) * vec2(-8.0, 8.0);

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

	#ifdef OPT_COLORMAP_INTERP
		Out_Color.rgb = getAttenuatedColorBlend(baseColor, lightLevel);
	#else
		Out_Color.rgb = getAttenuatedColor(int(baseColor), int(lightLevel));
	#endif

	Out_Color.a = 1.0;
}
