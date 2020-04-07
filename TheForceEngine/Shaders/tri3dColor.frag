uniform sampler2D filterMap;
in vec4 Frag_Color;
in vec2 Frag_Uv;
in vec3 Frag_Pos;
out vec4 Out_Color;

float anisotropicFilterWidth(vec2 uv)
{
	float log2Width = (65535.0/256.0) * texture(filterMap, uv).x;
	return exp2(-log2Width);
}

float computeGridAlpha(vec2 uv)
{
	vec2 fUV = fract(uv);
	float filterWidth = anisotropicFilterWidth(2.0*uv);

    vec2 opacity = vec2(smoothstep(filterWidth, 0.0, fUV.x) + smoothstep(1.0 - filterWidth, 1.0, fUV.x),
                        smoothstep(filterWidth, 0.0, fUV.y) + smoothstep(1.0 - filterWidth, 1.0, fUV.y));

	vec3 posScaled = Frag_Pos * 0.01;
	float distSq = dot(posScaled, posScaled);
	float falloff = 0.25 * max(1.0 - distSq, 0.0);

	return min(opacity.x + opacity.y, 1.0) * falloff;
}

void main()
{
	vec3 baseColor = Frag_Color.rgb;
	float gridAlpha = computeGridAlpha(Frag_Uv);
	vec3 gridColor = vec3(0.8, 0.9, 1.0);
	vec3 color = mix(baseColor, gridColor, gridAlpha);

    Out_Color = vec4(color * Frag_Color.a, Frag_Color.a);
}
