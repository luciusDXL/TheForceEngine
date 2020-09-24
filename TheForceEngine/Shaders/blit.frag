uniform sampler2D VirtualDisplay;

#ifdef ENABLE_GPU_COLOR_CONVERSION
uniform sampler2D Palette;
#endif

#ifdef ENABLE_GPU_COLOR_CORRECTION
uniform vec4 ColorCorrectParam;
#endif

in vec2 Frag_UV;
out vec4 Out_Color;

// All components are in the range [0…1], including hue.
vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// All components are in the range [0…1], including hue.
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 applyContrast(vec3 color, float contrast)
{
	return max((color - 0.5) * contrast + 0.5, vec3(0.0));
}

vec3 applyGamma(vec3 color, float gamma)
{
	vec3 outColor;
	// Square the gamma to give it more range.
	gamma = 2.0 - gamma;
	gamma *= gamma;

	outColor.x = pow(abs(color.x), gamma);
	outColor.y = pow(abs(color.y), gamma);
	outColor.z = pow(abs(color.z), gamma);
	return outColor;
}

void main()
{
#ifdef ENABLE_GPU_COLOR_CONVERSION
	// read the color index, it will be 0.0 - 255.0/256.0 range which maps to 0 - 255
	float index = texture(VirtualDisplay, Frag_UV).r;
	Out_Color.rgb = texture(Palette, vec2(index, 0.5)).rgb;
#else
	Out_Color.rgb = texture(VirtualDisplay, Frag_UV).rgb;
#endif

#ifdef ENABLE_GPU_COLOR_CORRECTION
	vec3 hsv = rgb2hsv(Out_Color.rgb);

	// Brightness & Saturation
	hsv.z = clamp(hsv.z * ColorCorrectParam.x, 0.0, 1.0);
	hsv.y = clamp(hsv.y * ColorCorrectParam.z, 0.0, 1.0);

	// Convert back to RGB
	Out_Color.rgb = hsv2rgb(hsv);
	
	// Contrast
	Out_Color.rgb = applyContrast(Out_Color.rgb, ColorCorrectParam.y);

	// Gamma
	Out_Color.rgb = applyGamma(Out_Color.rgb, ColorCorrectParam.w);
#endif

	Out_Color.a = 1.0;
}
