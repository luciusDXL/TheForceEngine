#include "Shaders/grid.h"

uniform vec3 GridOpacitySubGrid;

in vec2 Frag_UV;
in vec3 Frag_Pos;
in vec2 Frag_Dir;
in vec3 View_Pos;
in vec3 View_Up;
out vec4 Out_Color;

void main()
{
	vec3 outColor = vec3(0.0);
	float outAlpha = 0.0;

	float viewFalloff = computeViewFalloff(View_Pos, View_Up);
	drawFloorGridLevels(outColor, outAlpha, GridOpacitySubGrid.x, GridOpacitySubGrid.y, Frag_UV.xy, viewFalloff, Frag_Pos);
	
    Out_Color.rgb = outColor;
	Out_Color.a   = outAlpha;
}
