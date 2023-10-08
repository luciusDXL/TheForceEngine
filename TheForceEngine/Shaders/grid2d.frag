uniform vec2 GridOpacitySubGrid;
in vec2 Frag_UV;
out vec4 Out_Color;

float sqr(float x)
{
	return x*x;
}

float computeGrid(inout vec3 curColor, float scale, float lineScale, float gridOpacity)
{
	vec2 fUV = fract(Frag_UV.xy * scale);
    vec2 dFxy = lineScale * abs(vec2(dFdx(Frag_UV.x), dFdy(Frag_UV.y)));
    vec2 opacity = vec2(smoothstep(dFxy.x, 0.0, fUV.x) + smoothstep(1.0 - dFxy.x, 1.0, fUV.x), smoothstep(dFxy.y, 0.0, fUV.y) + smoothstep(1.0 - dFxy.y, 1.0, fUV.y));
    float alpha = sqr(max(opacity.x, opacity.y)) * gridOpacity;

    vec3 color = vec3(0.8, 0.9, 1.0);

	curColor = mix(curColor, color, alpha);
    return alpha;
}

float computeCoordAxis(inout vec3 curColor, float scale, float lineScale)
{
	vec2 fUV = Frag_UV.xy * scale;
	vec2 dFxy = lineScale * abs(vec2(dFdx(Frag_UV.x), dFdy(Frag_UV.y)));

	fUV = abs(fUV);
	vec2 opacity = vec2(smoothstep(dFxy.x, 0.0, fUV.x), smoothstep(dFxy.y, 0.0, fUV.y));

	curColor = mix(curColor, vec3(0.0, 0.0, 1.0), opacity.x);
	curColor = mix(curColor, vec3(1.0, 0.0, 0.0), opacity.y);
	return max(opacity.x, opacity.y);
}

void main()
{
	vec3 background = vec3(0.05, 0.06, 0.1);
    vec3 curColor = background;

	// Main Grid
	float alphaMain = computeGrid(curColor, 1.0, 2.0, GridOpacitySubGrid.x);
	float alphaSub  = computeGrid(curColor, 1.0/8.0, 2.0/7.0, GridOpacitySubGrid.y);
	float outAlpha  = max(alphaMain, alphaSub);

	// Coordinate Axis
	float coordAxis = computeCoordAxis(curColor, 1.0/8.0, 2.0/6.0);
	outAlpha = max(outAlpha, coordAxis);

	Out_Color.rgb = curColor * outAlpha;
	Out_Color.a = outAlpha;
}
