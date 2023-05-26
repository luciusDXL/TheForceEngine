#include "Shaders/config.h"
#include "Shaders/textureSampleFunc.h"

#ifdef OPT_DYNAMIC_LIGHTING
#include "Shaders/lighting.h"
#endif
#include "Shaders/filter.h"

uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;
uniform vec4 GlobalLightData;	// x = flat lighting, y = flat light value.
uniform vec2 SkyParallax;

#ifdef SKYMODE_VANILLA
uniform vec4 SkyParam0;	// xOffset, yOffset, xScale, yScale
uniform vec2 SkyParam1;	// atan(xScale, xOffset)
#endif

flat in vec4 Frag_Uv;
flat in vec4 Frag_Color;
flat in float Frag_Scale;
flat in int Frag_TextureId;
flat in int Frag_Flags;
#ifdef OPT_DYNAMIC_LIGHTING
flat in vec3 Frag_Normal;
#endif
in vec3 Frag_Pos;
in vec4 Texture_Data;
out vec4 Out_Color;

vec2 calculateSkyProjection(vec3 cameraVec, vec2 texOffset, out float fade, out float yLimit)
{
	// Cylindrical
	float len = length(cameraVec.xz);
	vec2 dir = cameraVec.xz;
	vec2 uv = vec2(0.0);
	fade = 0.0;
#ifdef SKYMODE_VANILLA
	if (len > 0.0)
	{
		// dir = offset + x|y * scale
		// where x = arcTan(offset + xPixel * scale)
		// and   y = yPixel
		vec2 xy  = vec2(atan(SkyParam1.x + gl_FragCoord.x*SkyParam1.y), gl_FragCoord.y);
		vec2 dir = SkyParam0.xy + xy*SkyParam0.zw;

		uv.x =  (dir.x + texOffset.x);
		uv.y = -(dir.y + texOffset.y);
	}
#else  // SKYMODE_CYLINDER
	if (len > 0.0)
	{
	    // Camera Yaw - this matches the vanilla case, but calculates it per-pixel.
		float oneOverTwoPi = 1.0 / 6.283185;
		float cameraYaw = fract(-atan(dir.y, dir.x) * oneOverTwoPi + 0.25);
		cameraYaw = cameraYaw < 0.0 ? cameraYaw + 1.0 : cameraYaw;
		uv.x = cameraYaw*SkyParallax.x + texOffset.x;
		
		// Handle vertical.
		float pitchScale = 0.785398;	// 45 degrees in radians.
		// Cylindrical projection, replaces pitch in the vanilla projection.
		float skyPitch = pitchScale * cameraVec.y / len;
		
		// Hack to try and get parallax < 1024 to look ok, despite the fact that there is no clean mapping to 3d.
		// Using the vanilla projection, it just moves up and down slower but stays the same size.
		if (SkyParallax.y < 1024.0)
		{
			skyPitch *= 1.5;
		}
		
		// Cylinder top and bottom coordinates.
		// These are based on the pitch scaling in the original projection + 200 units to the bottom.
		float yTop = -pitchScale * SkyParallax.y * oneOverTwoPi;
		float yBot = -yTop + 200.0;
		
		// Clamp just shy of the edges to avoid artifacts.
		float eps = 1.0 / 256.0;
		float v = clamp(skyPitch*0.5 + 0.5, eps, 1.0-eps);
		// Interpolate between the top and bottom of the cylinder based vertical cylindrical projection.
		float yCylinder = mix(yTop, yBot, v);
		// Calculate the final uv coordinate, this is the same as the vanilla projection.
		uv.y = -(yCylinder + texOffset.y);
		
		// yLimit is used for sampling to make sure the top and bottom are a solid color.
		// TODO: In the future, cap colors can be chosen per sky texture on the CPU and passed in.
		yLimit = mix(yTop, yBot, v < 0.5 ? eps : 1.0-eps);
		yLimit = -(yLimit + texOffset.y);
		// Compute a "fade" value, which is an ordered dither between the sampled color and cap color.
		// This is done since we are working in 8-bit color, so a proper blend isn't possible without lookup tables.
		fade = smoothstep(0.95, 1.0, abs(skyPitch));
	}
#endif
	return uv;
}

void main()
{
    vec3 cameraRelativePos = Frag_Pos;
	vec3 lightPos = Frag_Pos + CameraPos.xyz;
	vec2 uv = vec2(0.0);
	bool sky = Frag_Uv.y > 2.5;
	bool sign = false;
	bool flip = Frag_Color.a > 0.5;
	bool applyFlatWarp = false;
	float skyFade = 0.0;
	float yLimit = 0.0;

	bool fullbright = (Frag_Flags & 1) != 0;
	bool opaque = (Frag_Flags & 2) != 0;

	if (sky) // Sky
	{
		uv = calculateSkyProjection(cameraRelativePos, Texture_Data.xy, skyFade, yLimit);
	}
	else if (Frag_Uv.y > 1.5) // Wall
	{
		uv.x = length((Frag_Pos.xz + CameraPos.xz) - Texture_Data.xy) * Texture_Data.z;
		uv.y = (Frag_Uv.x - Frag_Pos.y - CameraPos.y) * Frag_Scale;
		uv *= 8.0;

		// Texture Offset
		uv += Frag_Uv.zw;
	}
	#ifdef SECTOR_TRANSPARENT_PASS
	else // Sign
	{
		uv.x = length((Frag_Pos.xz + CameraPos.xz) - Texture_Data.xy) * Texture_Data.z;
		uv.y = Frag_Uv.x - Frag_Pos.y - CameraPos.y;
		uv *= 8.0;

		// Texture Offset
		uv += Frag_Uv.zw;
		sign = true;
	}
	#else
	else if (Frag_Uv.y > 0.0) // Flat
	{
		// Project onto the floor or ceiling plane.
		float t = Frag_Uv.x / Frag_Pos.y;
		// Camera relative position on the plane, add CameraPos to get world space position.
		cameraRelativePos = t*Frag_Pos;
		lightPos = cameraRelativePos + CameraPos.xyz;

		// Warp texture uvs for non-64x64 tiles.
		applyFlatWarp = true;
		uv = (cameraRelativePos.xz + CameraPos.xz - Texture_Data.xy)*vec2(-8.0, 8.0);
	}
	else // Cap
	{
		// Warp texture uvs for non-64x64 tiles.
		applyFlatWarp = true;
		uv = (cameraRelativePos.xz + CameraPos.xz - Texture_Data.xy)*vec2(-8.0, 8.0);
	}
	#endif

	float light = 31.0;
	float ambient = 31.0;
	if (!sky && !fullbright)
	{
		float z = dot(cameraRelativePos, CameraDir);
		float lightOffset   = Frag_Color.r;
		ambient = Frag_Color.b;
		if (GlobalLightData.x != 0.0)
		{
			ambient = GlobalLightData.y;
		}

		if (ambient < 31.0)
		{
			// Camera light and world ambient.
			float worldAmbient = floor(LightData.x + 0.5);
			float cameraLightSource = LightData.y;

			light = 0.0;
			if (worldAmbient < 31.0 || cameraLightSource != 0.0)
			{
			#ifdef OPT_SMOOTH_LIGHTRAMP // Smooth out light ramp.
				float depthScaled = min(z * 4.0, 127.0);
				float base = floor(depthScaled);

				// Cubic
				float d0 = max(0.0, base - 1.0);
				float d1 = base;
				float d2 = min(127.0, base + 1.0);
				float d3 = min(127.0, base + 2.0);
				float blendFactor = fract(depthScaled);
				
				float lightSource0 = 31.0 - (texture(Colormap, vec2(d0/256.0, 0.0)).g*255.0/8.23 + worldAmbient);
				float lightSource1 = 31.0 - (texture(Colormap, vec2(d1/256.0, 0.0)).g*255.0/8.23 + worldAmbient);
				float lightSource2 = 31.0 - (texture(Colormap, vec2(d2/256.0, 0.0)).g*255.0/8.23 + worldAmbient);
				float lightSource3 = 31.0 - (texture(Colormap, vec2(d3/256.0, 0.0)).g*255.0/8.23 + worldAmbient);

				float lightSource = cubicInterpolation(lightSource0, lightSource1, lightSource2, lightSource3, blendFactor);
			#else // Vanilla style light ramp.
				float depthScaled = min(floor(z * 4.0), 127.0);
				float lightSource = 31.0 - (texture(Colormap, vec2(depthScaled/256.0, 0.0)).g*255.0 + worldAmbient);
			#endif

				if (lightSource > 0)
				{
					light += lightSource;
				}
			}
			light = max(light, ambient);

			float minAmbient = ambient * 0.875;
		#ifdef OPT_COLORMAP_INTERP // Smooth out the attenuation.
			float depthAtten = z * 0.09375;
		#else
			float depthAtten = floor(z / 16.0f) + floor(z / 32.0f);
		#endif
			light = max(light - depthAtten, minAmbient) + lightOffset;
			light = clamp(light, 0.0, 31.0);
		}
	}

	#ifdef OPT_TRUE_COLOR
	#else
		float baseColor;

		#ifdef SECTOR_TRANSPARENT_PASS
		if (sign)
		{
			baseColor = sampleTextureClamp(Frag_TextureId, uv, opaque);
		}
		else
		#endif
		{
			baseColor = sampleTexture(Frag_TextureId, uv, sky, flip, applyFlatWarp);
		}

		if (skyFade > 0.0)
		{
			// 4x4 Ordered Dither pattern.
			mat4 bayerIndex = mat4(
				vec4(00.0/16.0, 12.0/16.0, 03.0/16.0, 15.0/16.0),
				vec4(08.0/16.0, 04.0/16.0, 11.0/16.0, 07.0/16.0),
				vec4(02.0/16.0, 14.0/16.0, 01.0/16.0, 13.0/16.0),
				vec4(10.0/16.0, 06.0/16.0, 09.0/16.0, 05.0/16.0));

			ivec2 iuv = ivec2(uv * 2.0);
			float rnd = bayerIndex[iuv.x&3][iuv.y&3];
		
			if (rnd < skyFade)
			{
				baseColor = sampleTexture(Frag_TextureId, vec2(256.0, yLimit), sky, flip, false);
			}
		}

		// Enable solid color rendering for wireframe.
		#ifdef OPT_COLORMAP_INTERP
			Out_Color.rgb = LightData.w > 0.5 ? vec3(0.6, 0.7, 0.8) : getAttenuatedColorBlend(baseColor, light);
		#else
			Out_Color.rgb = LightData.w > 0.5 ? vec3(0.6, 0.7, 0.8) : getAttenuatedColor(int(baseColor), int(light));
		#endif

		Out_Color.a = baseColor < 0.5 ? 0.0 : 1.0;

		// Support transparent textures.
		#ifdef SECTOR_TRANSPARENT_PASS
		if (Out_Color.a < 0.5)
		{
			discard;
		}
		#endif

		// Optional dynamic lighting.
		#ifdef OPT_DYNAMIC_LIGHTING
		if (!sky && baseColor >= 32.0)	// do not light fullbright colors or sky.
		{
			vec3 albedo = texelFetch(Palette, ivec2(baseColor, 0), 0).rgb;
			Out_Color.rgb = handleLighting(albedo, lightPos, Frag_Normal, CameraPos, Out_Color.rgb);
		}
		#endif

		// Reduce the bloom intensity as ambient increases. This is to simulate a reduction in brightness delta, which translates to
		// less light bleed. In a modern HDR based renderer, this will be handled automatically by the tonemapper.
		float minBloom = 0.5;	// Minimum bloom intensity even in bright areas.
		float bloomIntensity = 1.0;//(1.0 - smoothstep(16.0, 31.0, ambient))*(1.0 - minBloom) + minBloom;
		//float zScale = clamp(dot(cameraRelativePos, CameraDir) * 0.02, 0.25, 1.0);
		//bloomIntensity *= zScale;
		Out_Color.a = writeBloomMask(int(baseColor), bloomIntensity);
		//Out_Color.rgb = Out_Color.rgb*0.25 + 0.75*((Out_Color.a - 0.5) * 2.0);
	#endif
}