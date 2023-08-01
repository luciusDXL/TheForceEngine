uniform sampler2D ColorBuffer;
uniform sampler2D MaterialBuffer;
uniform float bloomIntensity;

in vec2 Frag_UV;
out vec4 Out_Color;

vec3 sampleWeighted(vec2 uv, vec2 step, float x, float y)
{
	vec2 sampleUv = uv + step*vec2(x, y);
	return texture(ColorBuffer, sampleUv).rgb * texture(MaterialBuffer, sampleUv).r;
}

vec3 smoothWeightedDownsample(vec2 baseUV, vec2 step)
{
	// Take 13 samples around current texel, using bilinear filtering to grab neighbors, where e is the center.
	// High Weights (0.5 / 4.0)
	vec3 h0 = sampleWeighted(baseUV, step, -1.0,  1.0);
	vec3 h1 = sampleWeighted(baseUV, step,  1.0,  1.0);
	vec3 h2 = sampleWeighted(baseUV, step, -1.0, -1.0);
	vec3 h3 = sampleWeighted(baseUV, step,  1.0, -1.0);

	// Low Weights (0.5 / 9.0)
	vec3 l0 = sampleWeighted(baseUV, step, -2.0,  2.0);
	vec3 l1 = sampleWeighted(baseUV, step,  0.0,  2.0);
	vec3 l2 = sampleWeighted(baseUV, step,  2.0,  2.0);

	vec3 l3 = sampleWeighted(baseUV, step, -2.0,  0.0);
	vec3 l4 = sampleWeighted(baseUV, step,  0.0,  0.0);
	vec3 l5 = sampleWeighted(baseUV, step,  2.0,  0.0);

	vec3 l6 = sampleWeighted(baseUV, step, -2.0, -2.0);
	vec3 l7 = sampleWeighted(baseUV, step,  0.0, -2.0);
	vec3 l8 = sampleWeighted(baseUV, step,  2.0, -2.0);
		
    // Apply weighted distribution:
	// 4 samples at +/-1 sum to 0.5, the rest of the samples also sum to 0.5
	return (h0 + h1 + h2 + h3) * 0.125 + (l0 + l1 + l2 + l3 + l4 + l5 + l6 + l7 + l8) * 0.0555555;
}

void main()
{
	vec2 step = vec2(1.0) / vec2(textureSize(ColorBuffer, 0));
	Out_Color.rgb = smoothWeightedDownsample(Frag_UV, step) * bloomIntensity;
	Out_Color.a = 1.0;
}
