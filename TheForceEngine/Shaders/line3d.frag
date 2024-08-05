#ifdef OPT_CURVE
#include "Shaders/curve.h"
#endif

#ifdef OPT_CURVE
	flat in vec4 Frag_ControlAB;
	flat in vec2 Frag_ControlC;
	flat in vec4 Frag_Offsets;
	in vec2 Frag_Pos;
	in vec3 Frag_CameraVec;
#else
	in vec4 Frag_Uv;
	in float Frag_Width;
#endif
in vec4 Frag_Color;

out vec4 Out_Color;

#ifdef OPT_CURVE
	float calculateCurveAlpha()
	{
		// Calculate the distance from the current position to the curve.
	float t;
	#ifdef OPT_CURVE_OFFSETS
		float fade = 1.0; //smoothstep(256.0, 128.0, length(Frag_CameraVec.xz)) * Frag_Color.a;
		float edgeScale = smoothstep(0.0, 1.0, abs(normalize(Frag_CameraVec).y)) * 0.5 + 0.5;

		float sd = signedDistQuadraticBezierExact(Frag_Pos, Frag_ControlAB.xy, Frag_ControlC, Frag_ControlAB.zw, t);
	
		// Signed distance to alpha value based on desired width, alpha, and partial derivatives.
		float width = 2.0 * 0.1;
		vec2 dFx = abs(dFdx(Frag_Pos));
		vec2 dFy = abs(dFdy(Frag_Pos));
		vec2 filterWidth = vec2(sqrt(dFx.x*dFx.x + dFy.x*dFy.x), sqrt(dFx.y*dFx.y + dFy.y*dFy.y));
		float dFxy = 1.5 * edgeScale * length(filterWidth);
		float alpha = smoothstep(dFxy, 0.0, abs(sd)) * fade;

		// Apply up to 4 offsets, 0 = end of offset list.
		float endWidth = 0.001;
		float offsetAlpha = smoothstep(0.0, endWidth, t) * smoothstep(1.0, 1.0-endWidth, t) * 0.5 * fade;
		for (int i = 0; i < 4; i++)
		{
			float offset = Frag_Offsets[i];
			if (offset == 0.0) { break; }

			float curOffsetAlpha = offsetAlpha * smoothstep(dFxy, 0.0, abs(sd-offset));//smoothstep(0.0, -dFxy, abs(sd-offset)-width);
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

		/*
	#ifdef OPT_DASHED_LINE
		float L = approximateArcLength(Frag_ControlAB.xy, Frag_ControlAB.zw, Frag_ControlC, t) / (dFxy / 1.5);
		float fade = smoothstep(0.45, 0.5, fract(L * 0.1));
		alpha *= fade;
	#endif
		*/

		return alpha;
	}
#else
	float distToLine(vec2 pt1, vec2 pt2, vec2 testPt)
	{
	  vec2 lineDir = pt2 - pt1;
	  vec2 perpDir = vec2(lineDir.y, -lineDir.x);
	  vec2 dirToPt1 = pt1 - testPt;
	  return abs(dot(normalize(perpDir), dirToPt1));
	}

	float calculateLineAlpha()
	{
		float dist = distToLine(Frag_Uv.xy, Frag_Uv.zw, gl_FragCoord.xy);
		float alpha = smoothstep(1.0, 0.0, dist*Frag_Width) * Frag_Color.a;

	#ifdef OPT_DASHED_LINE
		float L = length(gl_FragCoord.xy - Frag_Uv.xy);
		alpha *= smoothstep(0.45, 0.5, fract(L*0.1));
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
    Out_Color = vec4(Frag_Color.rgb * alpha, alpha);
}
