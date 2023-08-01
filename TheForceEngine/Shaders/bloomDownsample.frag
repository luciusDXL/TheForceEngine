uniform sampler2D ColorBuffer;

in vec2 Frag_UV;
out vec4 Out_Color;

vec3 sampleWithStep(vec2 uv, vec2 step, float x, float y)
{
	return texture(ColorBuffer, uv + step*vec2(x, y)).rgb;
}

vec3 smoothDownsample(vec2 baseUV, vec2 step)
{
	// Take 13 samples around current texel, using bilinear filtering to grab neighbors, where e is the center.
	// High Weights (0.5 / 4.0)
	vec3 h0 = sampleWithStep(baseUV, step, -1.0,  1.0);
	vec3 h1 = sampleWithStep(baseUV, step,  1.0,  1.0);
	vec3 h2 = sampleWithStep(baseUV, step, -1.0, -1.0);
	vec3 h3 = sampleWithStep(baseUV, step,  1.0, -1.0);

	// Low Weights (0.5 / 9.0)
	vec3 l0 = sampleWithStep(baseUV, step, -2.0,  2.0);
	vec3 l1 = sampleWithStep(baseUV, step,  0.0,  2.0);
	vec3 l2 = sampleWithStep(baseUV, step,  2.0,  2.0);

	vec3 l3 = sampleWithStep(baseUV, step, -2.0,  0.0);
	vec3 l4 = sampleWithStep(baseUV, step,  0.0,  0.0);
	vec3 l5 = sampleWithStep(baseUV, step,  2.0,  0.0);

	vec3 l6 = sampleWithStep(baseUV, step, -2.0, -2.0);
	vec3 l7 = sampleWithStep(baseUV, step,  0.0, -2.0);
	vec3 l8 = sampleWithStep(baseUV, step,  2.0, -2.0);

    // Apply weighted distribution:
	// 4 samples at +/-1 sum to 0.5, the rest of the samples also sum to 0.5
	return (h0 + h1 + h2 + h3) * 0.125 + (l0 + l1 + l2 + l3 + l4 + l5 + l6 + l7 + l8) * 0.0555555;
}

void main()
{
	vec2 step = vec2(1.0) / vec2(textureSize(ColorBuffer, 0));
	Out_Color.rgb = smoothDownsample(Frag_UV, step);
	Out_Color.a = 1.0;
}
