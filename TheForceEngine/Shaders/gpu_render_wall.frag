uniform vec3 CameraPos;
uniform vec3 CameraDir;
uniform vec4 LightData;

uniform sampler2D Colormap;	// The color map has full RGB pre-backed in.
uniform sampler2D Palette;

in vec2 Frag_Uv;
flat in vec4 Frag_Color;
in vec3 Frag_Pos;
out vec4 Out_Color;

vec3 getAttenuatedColor(float baseColor, float light)
{
	baseColor = floor(baseColor + 0.5);
	light = floor(light + 0.5);

	float color = baseColor/256.0;
	if (light < 31.0)
	{
		vec2 uv = vec2(color, light/32.0);
		color = texture(Colormap, uv).r;
	}
	return texture(Palette, vec2(color, 0.5)).rgb;
}

void main()
{
    vec3 cameraRelativePos = Frag_Pos;
	if (Frag_Uv.y > 0.0)
	{
		// Project onto the floor or ceiling plane.
		float t = Frag_Uv.x / Frag_Pos.y;
		// Camera relative position on the plane, add CameraPos to get world space position.
		cameraRelativePos = t*Frag_Pos;
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

	Out_Color.rgb = getAttenuatedColor(baseColor, light);
	Out_Color.a = 1.0;
}