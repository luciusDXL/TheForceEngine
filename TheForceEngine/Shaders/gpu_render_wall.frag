uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;
uniform vec4 GlobalLightData;	// x = flat lighting, y = flat light value.
uniform vec2 SkyParallax;

uniform sampler2D Colormap;
uniform sampler2D Palette;
uniform sampler2DArray Textures;

uniform isamplerBuffer TextureTable;

#ifdef SKYMODE_VANILLA
uniform vec4 SkyParam;	// xOffset, yOffset, xScale, yScale
#endif

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

int wrapCoordScalar(int x, int edge)
{
	x = x - (x/edge)*edge;
	x += (x < 0) ? edge : 0;
	return x;
}

ivec2 wrapCoord(ivec2 uv, ivec2 edge)
{
	uv = imod(uv, edge);
	uv.x += (uv.x < 0) ? edge.x : 0;
	uv.y += (uv.y < 0) ? edge.y : 0;
	return uv;
}

// Approximate the distortion that happens in software when floor and ceiling textures are not 64x64.
ivec2 wrapCoordFlat(ivec2 uv, ivec2 edge)
{
	int coord = (uv.x & 63) * 64 + (uv.y & 63);
	uv = wrapCoord(ivec2(coord/edge.y, coord), edge);
	uv.x += (uv.x < 0) ? edge.x : 0;
	uv.y += (uv.y < 0) ? edge.y : 0;
	return uv;
}

float sampleTexture(int id, vec2 uv, bool sky, bool flip, bool applyFlatWarp)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec3 iuv;
	iuv.xy = ivec2(floor(uv));
	iuv.z = 0;

	if (sky)
	{
		if (abs(iuv.y) >= 9999)
		{
			// TODO: Single sample for the whole area.
			iuv.xy = ivec2(sampleData.z/2, sampleData.w/2);
		}
		else
		{
			iuv.x = wrapCoordScalar(iuv.x, sampleData.z);
			iuv.y = wrapCoordScalar(iuv.y, sampleData.w);
		}
	}
	else if (applyFlatWarp)
	{
		iuv.xy = wrapCoordFlat(iuv.xy, sampleData.zw);
		if (flip)
		{
			iuv.x = sampleData.z - iuv.x - 1;
		}
	}
	else
	{
		iuv.xy = wrapCoord(iuv.xy, sampleData.zw);
		if (flip)
		{
			iuv.x = sampleData.z - iuv.x - 1;
		}
	}
	iuv.xy += (sampleData.xy & ivec2(4095));
	iuv.z = sampleData.x >> 12;

	return texelFetch(Textures, iuv, 0).r * 255.0;
}

float sampleTextureClamp(int id, vec2 uv)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec3 iuv;
	iuv.xy = ivec2(floor(uv));
	iuv.z = 0;

	if ( any(lessThan(iuv.xy, ivec2(0))) || any(greaterThan(iuv.xy, sampleData.zw-1)) )
	{
		return 0.0;
	}
	iuv.xy += (sampleData.xy & ivec2(4095));
	iuv.z = sampleData.x >> 12;

	return texelFetch(Textures, iuv, 0).r * 255.0;
}

float sqr(float x)
{
	return x*x;
}

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
		// The camera direction and resolution parameters are used to derive
		// the SkyParam values on the CPU.
		// From there the screen pixel position is used in conjunction to compute the final coordinates.
		float dx = (gl_FragCoord.x * SkyParam.z - 1.0) * 0.8333333;	// -> (x/z - 1) / 1.2, refactored to remove per-pixel divisions.
		float dirX = SkyParam.x + atan(dx) * 256.0;
		float dirY = SkyParam.y + gl_FragCoord.y * SkyParam.w;

		uv.x =  dirX - texOffset.x;
		uv.y = -dirY - texOffset.y;
	}
#else  // SKYMODE_CYLINDER
	if (len > 0.0)
	{
		// Scale is derived from the XZ camera offset since we are looking for cylindrical distance
		// rather than spherical.
		float scale = 1.0 / len;
		
		// Projected Y position on cylinder infinitely far away.
		// +/-1.0 maps to the top of the cylinder and 0.0 maps to the horizon.
		float yCylinder = cameraVec.y*scale;

		// Scale from cylinder Y position to texels.
		float dirScale = 0.7071 * 256.0;
		// Offset to map from 0 -> 100 at the horizon line.
		float offsetY = texOffset.y + 100.0;	// Note 100 here maps the center line of the image (aka 200 / 2 = 100)

		// Hack! TODO: Figure out why this seems to be true.
		if (SkyParallax.y < 256.0)
		{
			offsetY += 50.0;
		}
		
		// Compute the X coordinate from the angle and scale by parallax.
		dir *= scale;
		uv.x = -(atan(dir.y, dir.x)/1.57 + 1.0) * SkyParallax.x - texOffset.x;
		// The Y Coordinate is derived from the cylinder Y position and constant scale factor.
		uv.y = -yCylinder * dirScale - offsetY;
		// The limit is the Y coordinate when yCylinder = +/-1.0, which is used to lookup the top/bottom cap color.
		yLimit = -sign(yCylinder) * dirScale - offsetY;
		if (abs(yCylinder) > 1.0)
		{
			uv.y = yLimit;
		}
		// Compute a fade out value so we can fade from the current sky texels to a single color.
		fade = smoothstep(0.95, 1.0, abs(yCylinder));
	}
#endif
	return uv;
}

void main()
{
    vec3 cameraRelativePos = Frag_Pos;
	vec2 uv = vec2(0.0);
	bool sky = Frag_Uv.y > 2.5;
	bool sign = false;
	bool flip = Frag_Color.a > 0.5;
	bool applyFlatWarp = false;
	float skyFade = 0.0;
	float yLimit = 0.0;
	if (sky) // Sky
	{
		uv = calculateSkyProjection(cameraRelativePos, Texture_Data.xy, skyFade, yLimit);
	}
	else if (Frag_Uv.y > 1.5) // Wall
	{
		uv.x = length((Frag_Pos.xz + CameraPos.xz) - Texture_Data.xy) * Texture_Data.z;
		uv.y = Frag_Uv.x - Frag_Pos.y - CameraPos.y;
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
	float baseColor = Frag_Color.g;
	if (!sky)
	{
		float z = dot(cameraRelativePos, CameraDir);
		float lightOffset   = Frag_Color.r;
		float sectorAmbient = Frag_Color.b;
		if (GlobalLightData.x != 0.0)
		{
			sectorAmbient = GlobalLightData.y;
		}

		if (sectorAmbient < 31.0)
		{
			// Camera light and world ambient.
			float worldAmbient = floor(LightData.x + 0.5);
			float cameraLightSource = LightData.y;

			light = 0.0;
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
		}
	}

	// Use define.
	#ifdef SECTOR_TRANSPARENT_PASS
	if (sign)
	{
		baseColor = sampleTextureClamp(Frag_TextureId, uv);
	}
	else
	#endif
	{
		baseColor = sampleTexture(Frag_TextureId, uv, sky, flip, applyFlatWarp);
	}
	// End

	// Support transparent textures.
	#ifdef SECTOR_TRANSPARENT_PASS
	if (baseColor < 0.5)
	{
		discard;
	}
	#endif

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
	Out_Color.rgb = LightData.w > 0.5 ? vec3(0.6, 0.7, 0.8) : getAttenuatedColor(int(baseColor), int(light));
	Out_Color.a = 1.0;
}