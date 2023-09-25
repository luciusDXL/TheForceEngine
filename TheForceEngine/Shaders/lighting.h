float getDepthAttenuation(float z, float ambient, float baseLight, float lightOffset)
{
#ifdef OPT_COLORMAP_INTERP // Smooth out the attenuation.
	float depthAtten = z * 0.09375;
#else
	float depthAtten = floor(z / 16.0) + floor(z / 32.0);
#endif

	float minAmbient = ambient * 0.875;
	float light = max(baseLight - depthAtten, minAmbient) + lightOffset;
	return clamp(light, 0.0, 31.0);
}

float getLightRampValue(float z, float worldAmbient)
{
#ifdef OPT_SMOOTH_LIGHTRAMP // Smooth out light ramp.
	float depthScaled = min(z * 4.0, 127.0);
	float base = floor(depthScaled);

	float d0 = base;
	float d1 = min(127.0, base + 1.0);
	float blendFactor = fract(depthScaled);
	float lightSource0 = 31.0 - (texture(Colormap, vec2(d0 / 256.0, 0.0)).g*255.0 / 8.23 + worldAmbient);
	float lightSource1 = 31.0 - (texture(Colormap, vec2(d1 / 256.0, 0.0)).g*255.0 / 8.23 + worldAmbient);
	float lightSource = mix(lightSource0, lightSource1, blendFactor);
#else // Vanilla style light ramp.
	float depthScaled = min(floor(z * 4.0), 127.0);
	float lightSource = 31.0 - (texture(Colormap, vec2(depthScaled / 256.0, 0.0)).g*255.0 + worldAmbient);
#endif
	return lightSource;
}
