#include "Shaders/grid.h"

in vec4 Frag_Color;
in vec2 Frag_Uv;
in vec2 Frag_Uv1;
in vec2 Frag_Uv2;
in vec3 Frag_Pos;
out vec4 Out_Color;

uniform sampler2D image;
uniform ivec2 isTexturedSky;
uniform vec2 GridScaleOpacity;
uniform vec4 SkyParam0;	// xOffset, yOffset, xScale, yScale
uniform vec4 SkyParam1;	// atan(xScale, xOffset), 1.0/texWidth, 1.0/texHeight

// For now just stick with the vanilla projection.
vec2 calculateSkyProjection(vec2 texOffset)
{
	vec2 uv = vec2(0.0);

	// dir = offset + x|y * scale
	// where x = arcTan(offset + xPixel * scale)
	// and   y = yPixel
	vec2 xy  = vec2(atan(SkyParam1.x + gl_FragCoord.x*SkyParam1.y), gl_FragCoord.y);
	vec2 dir = SkyParam0.xy + xy*SkyParam0.zw;
	uv.x =  (dir.x + texOffset.x);
	uv.y = -(dir.y + texOffset.y);

	return uv * SkyParam1.zw;
}

void main()
{
	vec4 outColor = Frag_Color;
	float gridOpacityMod = 1.0;
	if (isTexturedSky.x == int(1))
	{
		vec2 uv = Frag_Uv2;
		if (isTexturedSky.y == int(1))
		{
			uv = calculateSkyProjection(vec2(0.0)); 
		}
		vec4 texColor = texture(image, uv);
		if (isTexturedSky.y == int(1))	// Sky is fullbright.
		{
			outColor = texColor;
		}
		else
		{
		#ifdef TRANS
			outColor *= texColor;
		#else
			outColor.rgb *= texColor.rgb;
		#endif
		}

		// Make the grid more translucent on darker surfaces.
		// On bright surfaces it stays about the same.
		float L = dot(outColor.rgb, vec3(0.3333));
		gridOpacityMod = L * 0.75 + 0.25;
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
	outColor.rgb = mix(outColor.rgb, gridColor, GridScaleOpacity.y * gridOpacityMod);

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
