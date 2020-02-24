in float Frag_Width;
in vec4 Frag_UV;
in vec2 Frag_Pos;
in vec4 Frag_Color;
out vec4 Out_Color;
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

    Out_Color.rgb = Frag_Color.rgb * alpha;
    Out_Color.a = alpha;
}
