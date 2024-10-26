uniform sampler2D MergeCur;		// Higher resolution source.
uniform sampler2D MergePrev;	// Lower resolution upscale.
uniform float bloomSpread;

in vec2 Frag_UV;
out vec4 Out_Color;

vec3 blur3x3(sampler2D tex, vec2 uv, vec2 scale)
{
	const float weights[9] = float[9](
		0.0625f, 0.125f, 0.0625f,	// Tent filter
        0.125f,  0.25f,  0.125f,
        0.0625f, 0.125f, 0.0625f
	);

	// 3x3 blur on upscale.
	vec2 sampleUV;
	vec3 sum = vec3(0.0);
	int i = 0;
	for (int y = -1; y <= 1; y++)
	{
		sampleUV.y = uv.y + float(y) * scale.y;
		for (int x = -1; x <= 1; x++, i++)
		{
			sampleUV.x = uv.x + float(x) * scale.x;
			sum += texture(tex, sampleUV).rgb * weights[i];
		}
	}
	return sum;
}

void main()
{
	float blurScale = 1.5;
	vec3 curSample = texture(MergeCur, Frag_UV).rgb;

	// 3x3 blur on upscale.
	vec2 scaleUpsampled = vec2(blurScale) / vec2(textureSize(MergePrev, 0));
	vec3 upsampled = blur3x3(MergePrev, Frag_UV, scaleUpsampled);

	Out_Color.rgb = mix(curSample, upsampled, bloomSpread);
	Out_Color.a = 1.0;
}
