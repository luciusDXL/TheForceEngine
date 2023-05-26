uniform sampler2D VirtualDisplay;

#ifdef ENABLE_GPU_COLOR_CONVERSION
uniform sampler2D Palette;
#endif

uniform sampler2D Bloom0;
uniform sampler2D Bloom1;
uniform sampler2D Bloom2;
uniform sampler2D Bloom3;
uniform sampler2D Bloom4;
uniform sampler2D Bloom5;
uniform sampler2D Bloom6;
uniform sampler2D Bloom7;

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

float sqr(float x)
{
	return x * x;
}

// Maximum luminance approaches [2.61184 + Offset] (4.0 with current settings).
// Linear + Shoulder @ [1.738 + Offset] -> [2.61184 + Offset]
// Function tuned to maximize the linear portion but with a long shoulder to limit clamping.
vec3 tonemapBloom(vec3 light)
{
	float offset = 1.38816;
	float A = 2.61184 + offset;
	float B = 1.32;
	float C = 0.5;
	
	vec3 Lweights = vec3(0.299, 0.587, 0.114);
	float L = dot(light, Lweights);
	if (L > 1.738 + offset)
	{
		float Lnew = A - B / (C * sqr(L - offset));
		light *= Lnew / L;
	}
	return light;
}

void main()
{
#ifdef ENABLE_GPU_COLOR_CONVERSION
	// read the color index, it will be 0.0 - 255.0/256.0 range which maps to 0 - 255
	float index = texture(VirtualDisplay, Frag_UV).r;
	Out_Color.rgb = texture(Palette, vec2(index, 0.5)).rgb;
#else
	Out_Color = texture(VirtualDisplay, Frag_UV);

	float scale = 4.0;
	vec3 A = scale * texture(Bloom0, vec2(Frag_UV.x, 1.0 - Frag_UV.y)).rgb;		// 540p
	vec3 B = scale * texture(Bloom1, vec2(Frag_UV.x, /*1.0 - */Frag_UV.y)).rgb; // 270p
	vec3 C = scale * texture(Bloom2, vec2(Frag_UV.x, 1.0 - Frag_UV.y)).rgb;		// 135p
	vec3 D = scale * texture(Bloom3, vec2(Frag_UV.x, /*1.0 - */Frag_UV.y)).rgb; //  67p
	vec3 E = scale * texture(Bloom4, vec2(Frag_UV.x, 1.0 - Frag_UV.y)).rgb;		//  33p
	vec3 F = scale * texture(Bloom5, vec2(Frag_UV.x, /*1.0 - */Frag_UV.y)).rgb; //  16p
	vec3 G = scale * texture(Bloom6, vec2(Frag_UV.x, 1.0 - Frag_UV.y)).rgb;     //   8p
	vec3 H = scale * texture(Bloom7, vec2(Frag_UV.x, /*1.0 - */Frag_UV.y)).rgb; //   4p

	float f = 0.85;
	vec3 Gp = mix(G, H, f);
	vec3 Fp = mix(F, Gp, f);
	vec3 Ep = mix(E, Fp, f);
	vec3 Dp = mix(D, Ep, f);
	vec3 Cp = mix(C, Dp, f);
	vec3 Bp = mix(B, Cp, f);
	vec3 bloom = mix(A, Bp, f);
	Out_Color.rgb += bloom;

	//float mask = clamp(2.0*(Out_Color.a-0.5), 0.0, 0.5);
	//Out_Color.rgb += (bloom * 6.0 / 36.0);// * (1.0-mask);
	//Out_Color.rgb = mix(Out_Color.rgb, bloom * 6.0 / 36.0, 1.0-clamp(2.0*(Out_Color.a-0.5),0.0, 1.0));//(bloom * 6.0 / 37.0);
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
