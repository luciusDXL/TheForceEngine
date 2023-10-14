#include "Shaders/grid.h"

in vec4 Frag_Color;
in vec2 Frag_Uv;
in vec2 Frag_Uv1;
in vec2 Frag_Uv2;
in vec3 Frag_Pos;
out vec4 Out_Color;

uniform sampler2D image;
uniform int isTextured;
uniform vec2 GridScaleOpacity;

void main()
{
	vec4 outColor = Frag_Color;
	if (int(isTextured) == int(1))
	{
		outColor *= texture(image, Frag_Uv2);
	}

	// Outline
	float lineScale = 2.0;

	vec2 fUV = Frag_Uv.xy;
	float du = lineScale * fwidth(Frag_Uv.x);
	float dv = lineScale * fwidth(Frag_Uv.y);
	vec2 opacity = vec2(smoothstep(du, 0.0, fUV.x) + smoothstep(1.0 - du, 1.0, fUV.x), smoothstep(dv, 0.0, fUV.y) + smoothstep(1.0 - dv, 1.0, fUV.y));

	float alpha = max(opacity.x, opacity.y);
	vec3 edgeColor = vec3(1.0, 1.0, 1.0);
	outColor.rgb = mix(outColor.rgb, edgeColor, alpha * 0.5);

	// Grid
	vec3 viewNormal = computeViewNormal(Frag_Pos);
	float viewFalloff = min(1.0, 2.0*computeViewFalloff(Frag_Pos, viewNormal));

	float outAlpha = 0.0;
	vec3 gridColor = outColor.rgb;
	drawFloorGridLevels(gridColor, outAlpha, GridScaleOpacity.x, Frag_Uv1.xy, viewFalloff, Frag_Pos);
	outColor.rgb = mix(outColor.rgb, gridColor, GridScaleOpacity.y);

#ifdef TRANS
	if (outColor.a < 0.01)
	{
		discard;
	}
#endif
#ifdef TEX_CLAMP
	if (Frag_Uv2.x < 0.0 || Frag_Uv2.x >= 1.0 || Frag_Uv2.y < 0.0 || Frag_Uv2.y >= 1.0)
	{
		discard;
	}
#endif
	
    Out_Color = vec4(outColor.rgb * outColor.a, outColor.a);
}
