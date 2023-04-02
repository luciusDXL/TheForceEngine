vec2 bilinearSharpness(vec2 uv, float sharpness)
{
	if (sharpness == 0.0)
	{
		return uv;
	}

	float ex = max(1.0, (sharpness - 0.5) * 32.0);
	vec2 uvNew = pow(uv*uv*(3.0 - 2.0*uv), vec2(ex));
	uv = mix(uv, uvNew, min(1.0, sharpness*2.0));

	return uv;
}

vec2 bilinearDither(vec2 uv)
{
	// Hack: fake bilinear...
	vec2 bilinearOffset[4] = vec2[4](
		vec2(0.25, 0.00), vec2(0.50, 0.75),
		vec2(0.75, 0.50), vec2(0.00, 0.25));

	vec2 st = bilinearSharpness(fract(uv), 0.0);
	int ix = int(gl_FragCoord.x) & 1;
	int iy = int(gl_FragCoord.y) & 1;
	uv = floor(uv) + st + bilinearOffset[iy * 2 + ix];

	return uv;
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