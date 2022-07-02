uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;
uniform vec2 SkyParallax;

uniform sampler2D Colormap;
uniform sampler2D Palette;
uniform sampler2DArray Textures;

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

float sampleTexture(int id, vec2 uv, bool sky, bool flip)
{
	ivec4 sampleData = texelFetch(TextureTable, id);
	ivec3 iuv;
	iuv.xy = ivec2(floor(uv));
	iuv.z = 0;

	if (sky)
	{
		iuv.x = wrapCoordScalar(iuv.x, sampleData.z);
		iuv.y = clamp(iuv.y, 0, sampleData.w - 1);
		if (iuv.y == 0 || iuv.y == sampleData.w - 1)
		{
			// Single sample for the whole area.
			iuv.xy = ivec2(sampleData.z/2, iuv.y);
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

vec2 calculateSkyProjection(vec3 cameraVec, vec2 texOffset)
{
	// Cylindrical
	/*
	float len = length(cameraVec.xz);
	vec2 dir = cameraVec.xz;
	vec2 uv = vec2(0.0);
	if (len > 0.0)
	{
		float scale = 1.0 / len;
		dir *= scale;

		float dirY = cameraVec.y*scale;
		
		uv.x = -(atan(dir.y, dir.x)/1.57 + 1.0) * SkyParallax.x - texOffset.x;
		uv.y = -dirY*0.7071 * SkyParallax.y + texOffset.y*0.5;
	}
	*/
	// Spherical
	float len = length(cameraVec);
	vec3 dir = cameraVec;
	vec2 uv = vec2(0.0);
	if (len > 0.0)
	{
		float scale = 1.0 / len;
		dir *= scale;

		float horzAngle = atan(dir.z, dir.x) * 0.63662;
		float vertAngle = asin(-dir.y) * 0.73848;

		uv.x = -(horzAngle + 1.0) * SkyParallax.x - texOffset.x;
		uv.y = SkyParallax.y*vertAngle + texOffset.y*0.5;
	}
	return uv;
}

void main()
{
    vec3 cameraRelativePos = Frag_Pos;
	vec2 uv = vec2(0.0);
	bool sky = Frag_Uv.y > 2.5;
	bool sign = false;
	bool flip = Frag_Color.a > 0.5;
	if (sky) // Sky
	{
		uv = calculateSkyProjection(cameraRelativePos, Texture_Data.xy);
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

		uv = (cameraRelativePos.xz + CameraPos.xz - Texture_Data.xy)*vec2(-8.0, 8.0);
	}
	else // Cap
	{
		uv = (cameraRelativePos.xz + CameraPos.xz - Texture_Data.xy)*vec2(-8.0, 8.0);
	}
	#endif

	float light = 0.0;
	float baseColor = Frag_Color.g;
	if (!sky)
	{
		float z = dot(cameraRelativePos, CameraDir);
		float lightOffset   = Frag_Color.r;
		float sectorAmbient = Frag_Color.b;

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
		light = max(light - depthAtten, minAmbient) + lightOffset;
		light = clamp(light, 0.0, 31.0);
	}
	else
	{
		light = 31.0;
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
		baseColor = sampleTexture(Frag_TextureId, uv, sky, flip);
	}
	// End

	// Support transparent textures.
	#ifdef SECTOR_TRANSPARENT_PASS
	if (baseColor < 0.5)
	{
		discard;
	}
	#endif

	// Enable solid color rendering for wireframe.
	Out_Color.rgb = LightData.w > 0.5 ? vec3(0.6, 0.7, 0.8) : getAttenuatedColor(int(baseColor), int(light));
	Out_Color.a = 1.0;
}