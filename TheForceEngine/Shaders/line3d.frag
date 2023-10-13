in vec4 Frag_Uv;
in vec4 Frag_Color;
in float Frag_Width;
out vec4 Out_Color;

float distToLine(vec2 pt1, vec2 pt2, vec2 testPt)
{
  vec2 lineDir = pt2 - pt1;
  vec2 perpDir = vec2(lineDir.y, -lineDir.x);
  vec2 dirToPt1 = pt1 - testPt;
  return abs(dot(normalize(perpDir), dirToPt1));
}

void main()
{
	float dist = distToLine(Frag_Uv.xy, Frag_Uv.zw, gl_FragCoord.xy);

	float alpha = smoothstep(1.0, 0.0, dist*Frag_Width) * Frag_Color.a;
    Out_Color = vec4(Frag_Color.rgb * alpha, alpha);
}
