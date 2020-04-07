uniform vec3 GridOpacitySubGrid;
uniform sampler2D filterMap;

in vec2 Frag_UV;
in vec3 Frag_Pos;
in vec2 Frag_Dir;
out vec4 Out_Color;

float anisotropicFilterWidth(vec2 uv)
{
	float log2Width = (65535.0/256.0) * texture(filterMap, uv).x;
	return exp2(-log2Width);
}

float sqr(float x)
{
	return x*x;
}

// TODO: Replace filter width calculate with an anisotropic filter
// Trick: use a texture with mipmaps to compute filter width (i.e. sample with anisotropic, convert from encoded color to scale).
void main()
{
	vec2 fUV = fract(Frag_UV.xy);
	float dist = length(Frag_Pos.xz) * 0.01;
	float filterWidth = anisotropicFilterWidth(3.0*Frag_UV);

	// if the filterwidth is less than 1 pixel, clamp it and instead adjust the opacity.
	float pixelSize = GridOpacitySubGrid.z;
	float opacityAdj = min(8.0 * pixelSize / filterWidth, 1.0) * sqr(1.0 - dist);

    vec2 opacity = vec2(smoothstep(filterWidth, 0.0, fUV.x) + smoothstep(1.0 - filterWidth, 1.0, fUV.x),
                        smoothstep(filterWidth, 0.0, fUV.y) + smoothstep(1.0 - filterWidth, 1.0, fUV.y));
	float alpha = min(opacity.x + opacity.y, 1.0) * opacityAdj;

	float outAlpha = alpha * GridOpacitySubGrid.x;
    Out_Color.rgb = vec3(0.8, 0.9, 1.0) * outAlpha;
	Out_Color.a = outAlpha;
}
