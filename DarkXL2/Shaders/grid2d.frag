uniform vec2 GridOpacitySubGrid;
in vec2 Frag_UV;
out vec4 Out_Color;
void main()
{
    vec2 uvSub = Frag_UV.xy * GridOpacitySubGrid.y;
    vec2 fUVSub = fract(uvSub);
    vec2 dFxySub = 1.5 * abs(vec2(dFdx(uvSub.x), dFdy(uvSub.y)));
    vec2 opacitySub = vec2(smoothstep(dFxySub.x, 0.0, fUVSub.x) + smoothstep(1.0 - dFxySub.x, 1.0, fUVSub.x), smoothstep(dFxySub.y, 0.0, fUVSub.y) + smoothstep(1.0 - dFxySub.y, 1.0, fUVSub.y));
    float alphaSub = max(opacitySub.x, opacitySub.y);
    alphaSub *= alphaSub;
    if (GridOpacitySubGrid.y < 0.1) alphaSub = 0.0;

    vec3 subColor = vec3(0.3, 0.6, 0.6);
    vec3 background = vec3(0.05, 0.06, 0.1);
    vec3 curColor = mix(background, subColor, alphaSub);
    float outAlpha = alphaSub;

    vec2 fUV = fract(Frag_UV.xy);
    vec2 dFxy = 2.0 * abs(vec2(dFdx(Frag_UV.x), dFdy(Frag_UV.y)));
    vec2 opacity = vec2(smoothstep(dFxy.x, 0.0, fUV.x) + smoothstep(1.0 - dFxy.x, 1.0, fUV.x), smoothstep(dFxy.y, 0.0, fUV.y) + smoothstep(1.0 - dFxy.y, 1.0, fUV.y));
    float alpha = max(opacity.x, opacity.y);

    vec3 color = vec3(0.8, 0.9, 1.0);
    if (abs(Frag_UV.x) <= dFxy.x) color = vec3(0.0, 0.0, 1.0);
    else if (abs(Frag_UV.y) <= dFxy.y) color = vec3(1.0, 0.0, 0.0);
    else alpha *= alpha;
    outAlpha = GridOpacitySubGrid.x*max(outAlpha, alpha);

    Out_Color.rgb = mix(curColor, color, alpha) * outAlpha;
	Out_Color.a = outAlpha;
}
