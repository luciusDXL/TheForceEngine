// Adjust the output 
vec2 bilinearSharpness(vec2 uv, float sharpness)
{
	// Sharpness == 0 is the same as standard bilinear.
	if (sharpness == 0.0) { return uv; }

	// Sharpness == 1 adjust the filter width based on the per-pixel texel size in order
	// to approximate "antialised point-sampling".
	if (sharpness == 1.0)
	{
		vec2 w = fwidth(uv);
		float scale = 0.5;
		float mag = clamp(-log2(dot(w, w))*scale, 0.0, 1.0);
		sharpness = mag * 0.5 + 0.5;
	}

	// Adjust the sub-texel sample position by mapping the linear change to an exponentiated S-Curve.
	float ex = max(1.0, (sharpness - 0.5) * 32.0);
	vec2 st = fract(uv);
	vec2 stAdj = pow(uv*uv*(3.0 - 2.0*st), vec2(ex));
	st = mix(st, stAdj, min(1.0, sharpness*2.0));

	// The final texture coordinate is the integer position + adjusted sub-texel position.
	return floor(uv) + st;
}

float computeMipLevel(vec2 uv)
{
	vec2 dx = dFdx(uv);
	vec2 dy = dFdy(uv);
	float maxSq = max(dot(dx, dx), dot(dy, dy));
	return 0.5 * log2(maxSq);	// same as log2(maxSq^0.5)
}
