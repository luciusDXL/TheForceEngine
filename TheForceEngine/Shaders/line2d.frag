#ifdef OPT_CURVE
#include "Shaders/curve.h"
#endif

in vec2 Frag_Pos;
in vec4 Frag_Color;

#ifdef OPT_CURVE
	flat in vec4 Frag_ControlAB;
	flat in vec2 Frag_ControlC;
	flat in vec4 Frag_Offsets;
#else
	in float Frag_Width;
	in vec4 Frag_UV;
#endif

#ifdef OPT_BLOOM
	layout(location = 0) out vec4 Out_Color;
	layout(location = 1) out vec4 Out_Material;
#else
	out vec4 Out_Color;
#endif

#ifdef OPT_CURVE
float calculateCurveAlpha()
{
	// Calculate the distance from the current position to the curve.
	float t;
#ifdef OPT_CURVE_OFFSETS
	float sd = signedDistQuadraticBezierExact(Frag_Pos, Frag_ControlAB.xy, Frag_ControlC, Frag_ControlAB.zw, t);
	
	// Signed distance to alpha value based on desired width, alpha, and partial derivatives.
	float width = 2.0;
	float dFx = abs(dFdx(Frag_Pos.x));
    float dFy = abs(dFdy(Frag_Pos.y));
	float dFxy = 1.5 * max(dFx, dFy);
	float alpha = smoothstep(0.0, -dFxy, abs(sd)-width) * Frag_Color.a;

	// Apply up to 4 offsets, 0 = end of offset list.
	float endWidth = 0.001;
	float offsetAlpha = smoothstep(-endWidth, 0.0, t) * smoothstep(1.0, 1.0-endWidth, t) * 0.5 * Frag_Color.a;
	for (int i = 0; i < 4; i++)
	{
		float offset = Frag_Offsets[i];
		if (offset == 0.0) { break; }

		float curOffsetAlpha = offsetAlpha * smoothstep(0.0, -dFxy, abs(sd-offset)-width);
		// See which alpha we should use.
		alpha = max(alpha, curOffsetAlpha);
	}
#else
	float sd = signedDistQuadraticBezier(Frag_Pos, Frag_ControlAB.xy, Frag_ControlC, Frag_ControlAB.zw, t);

	// Signed distance to alpha value based on desired width, alpha, and partial derivatives.
	float width = 2.0;
	float dFx = abs(dFdx(Frag_Pos.x));
    float dFy = abs(dFdy(Frag_Pos.y));
	float dFxy = 1.5 * max(dFx, dFy);
	float alpha = smoothstep(0.0, -dFxy, abs(sd)-width) * Frag_Color.a;
#endif

#ifdef OPT_DASHED_LINE
	float L = approximateArcLength(Frag_ControlAB.xy, Frag_ControlAB.zw, Frag_ControlC, t);
	float fade = smoothstep(0.45, 0.5, fract(L * 0.1));
	alpha *= fade;
#endif

	return alpha;
}
#else
float calculateLineAlpha()
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

	return alpha;
}
#endif

void main()
{
#ifdef OPT_CURVE
	float alpha = calculateCurveAlpha();
#else
	float alpha = calculateLineAlpha();
#endif

    Out_Color.rgb = Frag_Color.rgb * alpha;
    Out_Color.a = alpha;

#ifdef OPT_BLOOM
	Out_Material = vec4(0.0);
#endif
}

