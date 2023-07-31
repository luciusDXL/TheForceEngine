uniform sampler2D ColorBuffer;

in vec2 Frag_UV;
out vec4 Out_Color;

vec3 sampleWithStep(vec2 uv, vec2 step, float x, float y)
{
	vec2 subOffs = vec2(0.5, -0.5);
	return texture(ColorBuffer, vec2(uv.x + step.x*(x+subOffs.x), uv.y + step.y*(y+subOffs.y))).rgb;
}

vec3 smoothDownsample(vec2 baseUV, vec2 step)
{
	// Take 13 samples around current texel, using bilinear filtering to grab neighbors, where e is the center.
	vec3 a = sampleWithStep(baseUV, step, -2.0,  2.0);
	vec3 b = sampleWithStep(baseUV, step,  0.0,  2.0);
	vec3 c = sampleWithStep(baseUV, step,  2.0,  2.0);

	vec3 d = sampleWithStep(baseUV, step, -2.0,  0.0);
	vec3 e = sampleWithStep(baseUV, step,  0.0,  0.0);
	vec3 f = sampleWithStep(baseUV, step,  2.0,  0.0);

	vec3 g = sampleWithStep(baseUV, step, -2.0, -2.0);
	vec3 h = sampleWithStep(baseUV, step,  0.0, -2.0);
	vec3 i = sampleWithStep(baseUV, step,  2.0, -2.0);

	vec3 j = sampleWithStep(baseUV, step, -1.0,  1.0);
	vec3 k = sampleWithStep(baseUV, step,  1.0,  1.0);
	vec3 l = sampleWithStep(baseUV, step, -1.0, -1.0);
	vec3 m = sampleWithStep(baseUV, step,  1.0, -1.0);

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    return e*0.125 + (a+c+g+i)*0.03125 + (b+d+f+h)*0.0625 + (j+k+l+m)*0.125;
}

void main()
{
	vec2 step = vec2(1.0) / vec2(textureSize(ColorBuffer, 0));
	vec2 baseUV = vec2(Frag_UV.x, 1.0 - Frag_UV.y);

	Out_Color.rgb = smoothDownsample(baseUV, step);
	Out_Color.a = 1.0;
}
