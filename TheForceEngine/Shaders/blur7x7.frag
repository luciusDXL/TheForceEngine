uniform sampler2D VirtualDisplay;

in vec2 Frag_UV;
out vec4 Out_Color;

vec3 blur7x7(sampler2D tex, vec2 uv, vec2 pixelOffset)
{
	vec3 outColor = vec3(0.0);
	const int stepCount = 2;
	const float weights[stepCount] = float[stepCount]( 0.44908, 0.05092 );
	const float offsets[stepCount] = float[stepCount]( 0.53805, 2.06278 );

	for (int i = 0; i < stepCount; i++)
	{
		vec2 texCoordOffset = offsets[i] * pixelOffset;
		vec3 color = texture(tex, uv + texCoordOffset).rgb + texture(tex, uv - texCoordOffset).rgb;
		outColor += color * weights[i];
	}

	return outColor;
}

void main()
{
	vec2 rcpTextureSize = vec2(1.0) / vec2(textureSize(VirtualDisplay, 0));
#ifdef BLUR_DIR_X
	vec2 pixelOffset = vec2(rcpTextureSize.x, 0.0);
#else
	vec2 pixelOffset = vec2(0.0, rcpTextureSize.y);
#endif

	Out_Color.rgb = blur7x7(VirtualDisplay, Frag_UV, pixelOffset);
	Out_Color.a   = 1.0;
}
