// Number of grid levels
#define LEVELS 6
#define DIST_SCALE_BASE 0.004
#define LINE_WIDTH_SCALE 1.75
#define GRID_SCALE_MAX (1.0 / 512.0)
#define GRID_COLOR vec3(0.8, 0.9, 1.0)
#define GRID_COLOR_RED vec3(1.0, 0.0, 0.0)
#define GRID_COLOR_BLUE vec3(0.0, 0.0, 1.0)

float sqr(float x)
{
	return x*x;
}

// Use the screenspace partial derivatives of the viewspace position to compute the normal.
vec3 computeViewNormal(vec3 pos)
{
	vec3 dx = dFdx(pos);
	vec3 dy = dFdy(pos);
	return normalize(cross(dx, dy));
}

float computeViewFalloff(vec3 viewPos, vec3 viewUp)
{
	vec3 V = normalize(viewPos);
	float VdotUp = dot(V, viewUp);
	return min(1.0, abs(VdotUp));
}

float computeDistanceFalloff(float scale, vec3 inPos)
{
	float dist = min(length(inPos) * scale, 1.0);
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

void computeGrid(vec2 pUV, float falloff, inout vec3 outColor, inout float outAlpha)
{
	vec2 fUV = fract(pUV);
	vec2 filterWidth = computeLineFilter(pUV);

	float alpha = computeGridLineOpacity(fUV, filterWidth) * falloff;
	outColor = GRID_COLOR * alpha + outColor * (1.0 - alpha);
	outAlpha = min(alpha + outAlpha * (1.0 - alpha), 1.0);
}

void drawFloorGridLevels(inout vec3 baseColor, inout float baseAlpha, float gridOpacity, float gridSize, vec2 inUv, float viewFalloff, vec3 inPos)
{
	float uvScale = 1.0 / gridSize;
	float distScale = DIST_SCALE_BASE / gridSize;

	vec3 outColor = vec3(0.0);
	float outAlpha = 0.0;

	for (int i = 0; i < LEVELS; i++)
	{
		vec2 pUV = inUv * uvScale;
		float falloff = computeDistanceFalloff(distScale, inPos) * viewFalloff;
		computeGrid(pUV, falloff, outColor, outAlpha);

		uvScale /= 8.0;
		distScale /= 8.0;
	}

	// We have the final grid color and opacity, factor in the gridOpacity from the application.
	outColor *= gridOpacity;
	outAlpha *= gridOpacity;

	// Blend the incoming base color and alpha with the grid.
	baseColor = mix(baseColor, outColor, outAlpha);
	baseAlpha = min(baseAlpha + outAlpha * (1.0 - baseAlpha), 1.0);
}

// Draw the X and Z axis.
void drawXZAxis(inout vec3 outColor, inout float outAlpha, vec2 inUv, float viewFalloff, vec3 inPos)
{
	vec2 pUV = inUv.xy * GRID_SCALE_MAX;
	vec2 uv = fract(pUV);
	vec2 filterWidth = computeLineFilter(pUV) * 2.5;
	float alpha;

	// X Axis (red)
	if (abs(pUV.x) < 0.5)
	{
		alpha = min(smoothstep(filterWidth.x, 0.0, uv.x) + smoothstep(1.0 - filterWidth.x, 1.0, uv.x), 1.0) * viewFalloff;
		outColor = GRID_COLOR_RED * alpha + outColor * (1.0 - alpha);
		outAlpha = min(alpha + outAlpha * (1.0 - alpha), 1.0);
	}
	// Z Axis (red)
	if (abs(pUV.y) < 0.5)
	{
		alpha = min(smoothstep(filterWidth.y, 0.0, uv.y) + smoothstep(1.0 - filterWidth.y, 1.0, uv.y), 1.0) * viewFalloff;
		outColor = GRID_COLOR_BLUE * alpha + outColor * (1.0 - alpha);
		outAlpha = min(alpha + outAlpha * (1.0 - alpha), 1.0);
	}
}
