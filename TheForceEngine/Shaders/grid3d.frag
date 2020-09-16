uniform vec3 GridOpacitySubGrid;
uniform sampler2D filterMap;

in vec2 Frag_UV;
in vec3 Frag_Pos;
in vec2 Frag_Dir;
in vec3 View_Pos;
in vec3 View_Up;
in float Plane_Dist;
out vec4 Out_Color;

// Number of grid levels
#define LEVELS 6
#define DIST_SCALE_BASE 0.01
#define LINE_WIDTH_SCALE 1.5
#define GRID_COLOR vec3(0.8, 0.9, 1.0)
#define GRID_COLOR_RED vec3(1.0, 0.0, 0.0)
#define GRID_COLOR_BLUE vec3(0.0, 0.0, 1.0)

float anisotropicFilterWidth(vec2 uv)
{
	float log2Width = (65535.0/256.0) * texture(filterMap, uv).x;
	return exp2(-log2Width);
}

float sqr(float x)
{
	return x*x;
}

float computeViewFalloff()
{
	vec3 V = normalize(View_Pos);
	float VdotUp = dot(V, View_Up);
	return min(1.0, abs(VdotUp));
}

float computeDistanceFalloff(float scale)
{
	scale = max(scale, 0.001);
	float dist = min(length(Frag_Pos.xyz) * scale, 1.0);
	return sqr(1.0 - dist);
}

vec2 computeLineFilter(vec2 uv)
{
	vec2 dx = abs(dFdx(uv));
	vec2 dy = abs(dFdy(uv));
	vec2 filterWidth = vec2(sqrt(dx.x*dx.x + dy.x*dy.x), sqrt(dx.y*dx.y + dy.y*dy.y));
	return filterWidth * LINE_WIDTH_SCALE;
}

float computeGridLineOpacity(vec2 uv, vec2 filterWidth)
{
	vec2 opacity = vec2(smoothstep(filterWidth.x, 0.0, uv.x) + smoothstep(1.0 - filterWidth.x, 1.0, uv.x),
                        smoothstep(filterWidth.y, 0.0, uv.y) + smoothstep(1.0 - filterWidth.y, 1.0, uv.y));
	// combine opacities for each direction.
	// multiply instead of add will give dots.
	return min(opacity.x + opacity.y, 1.0);
}

void main()
{
	float uvScale = 1.0;
	float distScale = DIST_SCALE_BASE;
	float viewFalloff = computeViewFalloff();
	vec3 outColor = vec3(0.0);
	float outAlpha = 0.0;

	// Draw the grid scales
	for (int i = 0; i < LEVELS; i++)
	{
		vec2 pUV = Frag_UV.xy * uvScale;
		vec2 fUV = fract(pUV);

		float falloff = computeDistanceFalloff(distScale) * viewFalloff;
		vec2 filterWidth = computeLineFilter(pUV);

		float alpha = computeGridLineOpacity(fUV, filterWidth) * falloff;
		outColor = GRID_COLOR*alpha + outColor*(1.0 - alpha);
		outAlpha = min(alpha + outAlpha*(1.0 - alpha), 1.0);

		uvScale   *= 0.5;
		distScale *= 0.5;
	}
	outColor *= GridOpacitySubGrid.x;
	outAlpha *= GridOpacitySubGrid.x;

	// Draw the X and Z axis.
	vec2 pUV = Frag_UV.xy;
	vec2 uv = fract(pUV);
	float falloff = computeDistanceFalloff(distScale) * viewFalloff;
	vec2 filterWidth = computeLineFilter(pUV);
	// X Axis (red)
	float alpha;
	if (abs(pUV.x) < 0.5)
	{
		alpha = min(smoothstep(filterWidth.x, 0.0, uv.x) + smoothstep(1.0 - filterWidth.x, 1.0, uv.x), 1.0) * falloff;
		outColor = GRID_COLOR_RED*alpha + outColor*(1.0 - alpha);
		outAlpha = min(alpha + outAlpha*(1.0 - alpha), 1.0);
	}
	// Z Axis (red)
	if (abs(pUV.y) < 0.5)
	{
		alpha = min(smoothstep(filterWidth.y, 0.0, uv.y) + smoothstep(1.0 - filterWidth.y, 1.0, uv.y), 1.0) * falloff;
		outColor = GRID_COLOR_BLUE*alpha + outColor*(1.0 - alpha);
		outAlpha = min(alpha + outAlpha*(1.0 - alpha), 1.0);
	}
	
    Out_Color.rgb = outColor;
	Out_Color.a   = outAlpha;
}
