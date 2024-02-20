in float Frag_Width;
in vec4 Frag_UV;
in vec2 Frag_Pos;
in vec4 Frag_Color;

#ifdef OPT_BLOOM
	layout(location = 0) out vec4 Out_Color;
	layout(location = 1) out vec4 Out_Material;
#else
	out vec4 Out_Color;
#endif

void main()
{
    vec2 dir = Frag_UV.zw - Frag_UV.xy;
    float l2 = dot(dir, dir);	// dist^2
    float t = max(0.0, min(1.0, dot(Frag_Pos - Frag_UV.xy, dir) / l2));
    vec2 proj = Frag_UV.xy + t * dir;
    float dist = length(Frag_Pos - proj);
    float dFx = abs(dFdx(Frag_Pos.x));
    float dFy = abs(dFdy(Frag_Pos.y));
    float dFxy = 1.5 * max(dFx, dFy);
    float alpha = smoothstep(0.0, -dFxy, dist-Frag_Width) * Frag_Color.a;

#ifdef OPT_DASHED_LINE
	float L = length(t * dir);
	float fade = smoothstep(0.45, 0.5, fract(L*0.1));
	alpha *= fade;
#endif

    Out_Color.rgb = Frag_Color.rgb * alpha;
    Out_Color.a = alpha;

#ifdef OPT_BLOOM
	Out_Material = vec4(0.0);
#endif
}
