in vec4 Frag_Color;
in vec2 Frag_Uv;
in vec3 Frag_Pos;
out vec4 Out_Color;

void main()
{
	vec4 outColor = Frag_Color;

	// Outline
	float lineScale = 2.0;

	vec2 fUV = Frag_Uv.xy;
	float du = lineScale * fwidth(Frag_Uv.x);
	float dv = lineScale * fwidth(Frag_Uv.y);
	vec2 opacity = vec2(smoothstep(du, 0.0, fUV.x) + smoothstep(1.0 - du, 1.0, fUV.x), smoothstep(dv, 0.0, fUV.y) + smoothstep(1.0 - dv, 1.0, fUV.y));

	float alpha = max(opacity.x, opacity.y);
	vec3 edgeColor = vec3(1.0, 1.0, 1.0);
	outColor.rgb = mix(outColor.rgb, edgeColor, alpha * 0.5);

    Out_Color = vec4(outColor.rgb * outColor.a, outColor.a);
}
