uniform sampler2D ColorBuffer;
uniform sampler2D MaterialBuffer;
uniform float bloomIntensity;

in vec2 Frag_UV;
out vec4 Out_Color;

vec3 sampleWeighted(vec2 uv, vec2 step, float x, float y)
{
	vec2 subOffs = vec2(0.5, -0.5);
	return texture(ColorBuffer, vec2(uv.x + step.x*(x+subOffs.x), uv.y + step.y*(y+subOffs.y))).rgb *
	       texture(MaterialBuffer, vec2(uv.x + step.x*(x+subOffs.x), uv.y + step.y*(y+subOffs.y))).r;
}

vec3 smoothWeightedDownsample(vec2 baseUV, vec2 step)
{
	// Take 13 samples around current texel, using bilinear filtering to grab neighbors, where e is the center.
	vec3 a = sampleWeighted(baseUV, step, -2.0,  2.0);
	vec3 b = sampleWeighted(baseUV, step,  0.0,  2.0);
	vec3 c = sampleWeighted(baseUV, step,  2.0,  2.0);

	vec3 d = sampleWeighted(baseUV, step, -2.0,  0.0);
	vec3 e = sampleWeighted(baseUV, step,  0.0,  0.0);
	vec3 f = sampleWeighted(baseUV, step,  2.0,  0.0);

	vec3 g = sampleWeighted(baseUV, step, -2.0, -2.0);
	vec3 h = sampleWeighted(baseUV, step,  0.0, -2.0);
	vec3 i = sampleWeighted(baseUV, step,  2.0, -2.0);

	vec3 j = sampleWeighted(baseUV, step, -1.0,  1.0);
	vec3 k = sampleWeighted(baseUV, step,  1.0,  1.0);
	vec3 l = sampleWeighted(baseUV, step, -1.0, -1.0);
	vec3 m = sampleWeighted(baseUV, step,  1.0, -1.0);

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    return e*0.125 + (a+c+g+i)*0.03125 + (b+d+f+h)*0.0625 + (j+k+l+m)*0.125;
}

void main()
{
	vec2 step = vec2(1.0) / vec2(textureSize(ColorBuffer, 0));
	vec2 baseUV = vec2(Frag_UV.x, 1.0 - Frag_UV.y);

	Out_Color.rgb = smoothWeightedDownsample(baseUV, step) * bloomIntensity;
	Out_Color.a = 1.0;
}
