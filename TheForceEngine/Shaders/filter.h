#ifdef OPT_TRUE_COLOR
uniform vec3 PalFxLumMask;
uniform vec3 PalFxFlash;
#endif

// Adjust the output 
vec2 bilinearSharpness(vec2 uv, float sharpness)
{
	// Determine the edges
	if (sharpness > 0.5)
	{
		vec2 w = fwidth(uv);
		float negMip = -0.5 * log2(dot(w, w));	// ranges from 1 at 200% scale and 0 at 100% scale
		sharpness *= (clamp(negMip, 0.0, 1.0)*0.5 + 0.5);
	}
	// Sharpness == 0 is the same as standard bilinear.
	if (sharpness == 0.0) { return uv; }

	float e0 = 0.0, e1 = 1.0;
	if (sharpness > 0.5)
	{
		// Adjust the endpoints of the curve inward toward the center.
		e0 = 0.425 * (sharpness - 0.5) * 2.0;
		e1 = 1.0 - e0;
	}
	
	vec2 offset = vec2(0.5);

	// Deconstruct the uv coordinate into integer and fractional components.
	uv += offset;
	vec2 iuv = floor(uv);
	vec2 fuv = uv - iuv;

	// Adjusted uv using an s-curve where S(0.5) = 0.5
	vec2 uvAdj = smoothstep(e0, e1, fuv);
	// Blend between the standard linear and adjusted coordinates.
	float blendFactor = clamp(sharpness * 2.0, 0.0, 1.0); // 0 @ sharpness = 0, 1 @ sharpess >= 0.5
	fuv = mix(fuv, uvAdj, blendFactor);
	
	// Reconstruct the uv coordinate.
	return iuv + fuv - offset;
}

float computeMipLevel(vec2 uv)
{
	vec2 w = fwidth(uv);
	return 0.5 * log2(dot(w, w));	// same as log2(maxSq^0.5)
}

vec3 handlePaletteFx(vec3 inColor)
{
	vec3 outColor = inColor;
#ifdef OPT_TRUE_COLOR
	// Luminance mask.
	if (dot(PalFxLumMask, PalFxLumMask) > 0.1)
	{
		// Compute the approximate luminance (red/4 + green/2 + blue/4)
		vec3 lumMask = vec3(0.25, 0.5, 0.25);
		float L = dot(outColor, lumMask);
		// Then assign the luminance to the requested channels.
		outColor = PalFxLumMask * L;
	}
	// Flashes.
	if (PalFxFlash.x > 0.0)
	{
		float flashLevel = 1.0 - PalFxFlash.x;
		outColor.x   = 1.0 - abs(1.0 - outColor.x) * flashLevel;
		outColor.yz *= flashLevel;
	}
	else if (PalFxFlash.y > 0.0)
	{
		float flashLevel = 1.0 - PalFxFlash.y;
		outColor.y   = 1.0 - abs(1.0 - outColor.y) * flashLevel;
		outColor.xz *= flashLevel;
	}
	else if (PalFxFlash.z > 0.0)
	{
		float flashLevel = 1.0 - PalFxFlash.z;
		outColor.z   = 1.0 - abs(1.0 - outColor.z) * flashLevel;
		outColor.xy *= flashLevel;
	}
#endif
	return outColor;
}
