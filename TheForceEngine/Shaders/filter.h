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

// t is between 0, 1; sample is between x1 and x2.
float cubicInterpolation(float x0, float x1, float x2, float x3, float t)
{
	float a = (3.0*x1 - 3.0*x2 + x3 - x0) * 0.5;
	float b = (2.0*x0 - 5.0*x1 + 4.0*x2 - x3) * 0.5;
	float c = (x2 - x0) * 0.5;
	float d = x1;

	float t2 = t * t;
	float t3 = t2 * t;
	return a * t3 + b * t2 + c * t + d;
}