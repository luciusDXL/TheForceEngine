#include "Shaders/grid.h"

uniform sampler2D image;
in vec4 Frag_Color;
in vec2 Frag_Uv;
in vec2 Frag_Uv1;
in vec3 Frag_Pos;
out vec4 Out_Color;

void main()
{
	vec3 viewNormal = computeViewNormal(Frag_Pos);
	float viewFalloff = computeViewFalloff(Frag_Pos, viewNormal);

	vec4 texelColor = texture(image, Frag_Uv1);
	if (texelColor.a < 0.5 || Frag_Uv1.x < 0.0 || Frag_Uv1.x >= 1.0 || Frag_Uv1.y < 0.0 || Frag_Uv1.y >= 1.0) { discard; }

	vec3 baseColor = Frag_Color.rgb * texelColor.rgb;
	vec3 outColor = vec3(0.0);
	float outAlpha = 0.0;
	drawFloorGridLevels(outColor, outAlpha, 0.5,  Frag_Uv.xy, viewFalloff, Frag_Pos);

	// Tweak to make it easier to read over textures.
	outAlpha = min(outAlpha*4.0, 1.0);
	outColor = baseColor + outColor*outAlpha;

    Out_Color = vec4(outColor * Frag_Color.a, Frag_Color.a);
}
