#include "Shaders/grid.h"

uniform sampler2D image;
in vec4 Frag_Color;
in vec2 Frag_Uv;
in vec2 Frag_Uv1;
in vec3 Frag_Pos;
in float Frag_GridHeight;
out vec4 Out_Color;

void main()
{
	vec3 viewNormal = computeViewNormal(Frag_Pos);
	float viewFalloff = computeViewFalloff(Frag_Pos, viewNormal);
	float heightScale = max(0.0, -Frag_GridHeight*8.0);
	if (heightScale > 0) { heightScale = min(1.0, 1.0 / heightScale); }
	else { heightScale = 1.0; }

	vec3 baseColor = Frag_Color.rgb * texture(image, Frag_Uv1).rgb;
	vec3 outColor = vec3(0.0);
	float outAlpha = 0.0;
	drawFloorGridLevels(outColor, outAlpha, 0.5,  Frag_Uv.xy, viewFalloff, Frag_Pos);
	outColor = baseColor + outColor * heightScale;

    Out_Color = vec4(outColor * Frag_Color.a, Frag_Color.a);
}
