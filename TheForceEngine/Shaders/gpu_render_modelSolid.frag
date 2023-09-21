#include "Shaders/filter.h"
#include "Shaders/textureSampleFunc.h"
#include "Shaders/lighting.h"

uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec2 LightData;
uniform vec4 TextureOffsets;

in vec2 Frag_Uv;
in vec3 Frag_WorldPos;
noperspective in float Frag_Light;
flat in float Frag_ModelY;
#ifdef OPT_TRUE_COLOR
flat in vec4 Frag_Color;
#else
flat in int Frag_Color;
#endif
flat in int Frag_TextureId;
flat in int Frag_TextureMode;

#ifdef OPT_BLOOM
layout(location = 0) out vec4 Out_Color;
layout(location = 1) out vec4 Out_Material;
#else
out vec4 Out_Color;
#endif

void main()
{
	#ifdef OPT_TRUE_COLOR
		vec4 baseColor = Frag_Color;
	#else
		float baseColor = float(Frag_Color);
	#endif
	float light = 31.0;
	if (Frag_TextureId < 65535) // 0xffff = no texture
	{
		vec2 uv = Frag_Uv;
		if (Frag_TextureMode > 0) // PLANE shaded.
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
				light = 0.0;
				float z = max(0.0, dot((Frag_WorldPos - CameraPos), CameraDir));

				// Handle lighting in a similar way to sector floors and ceilings.
				// Camera Light
				float worldAmbient = LightData.x > 64.0 ? LightData.x - 128.0 : LightData.x;
				float cameraLightSource = LightData.y > 32.0 ? 1.0 : 0.0;
				if (worldAmbient < 31.0 || cameraLightSource > 0.0)
				{
					float lightSource = getLightRampValue(z, worldAmbient);
					if (lightSource > 0)
					{
						light += lightSource;
					}
				}
				light = max(light, ambient);
				light = getDepthAttenuation(z, ambient, light, 0.0);
			}
		}
		baseColor = sampleTexture(Frag_TextureId, uv);

		#ifdef MODEL_TRANSPARENT_PASS
			#ifdef OPT_TRUE_COLOR
				if (baseColor.a < 0.01) { discard; }
			#else
				if (baseColor < 0.5) { discard; }
			#endif
		#endif
	}
	// Vertex-lit.
	if (Frag_TextureMode == 0)
	{
		float dither = float(int(gl_FragCoord.x) + int(gl_FragCoord.y) & 1);
	#ifdef OPT_COLORMAP_INTERP // Smooth out the attenuation.
		light = Frag_Light;
	#else // Apply dithering like the original.
		light = floor(Frag_Light + 0.5*dither);
	#endif
	}

	float emissive;
	#ifdef OPT_TRUE_COLOR
		emissive = clamp((baseColor.a - 0.5) * 2.0, 0.0, 1.0);
		#ifdef MODEL_TRANSPARENT_PASS
			baseColor.a = min(baseColor.a * 2.008, 1.0);
			baseColor.rgb *= baseColor.a;
		#endif
	#else
		emissive = baseColor;
	#endif

	Out_Color = getFinalColor(baseColor, light, emissive);
	
	// Enable solid color rendering for wireframe.
	Out_Color.rgb = LightData.x > 64.0 ? vec3(0.3, 0.3, 0.8) : Out_Color.rgb;
	if (Frag_TextureId < 65535)
	{
		Out_Color.rgb = handlePaletteFx(Out_Color.rgb);
	}

#ifdef OPT_BLOOM
	// Material (just emissive for now)
	Out_Material = getMaterialColor(emissive);
	#ifdef OPT_TRUE_COLOR
		Out_Material.a = baseColor.a;
		Out_Material.rgb *= Out_Material.a;
	#endif
#endif
}