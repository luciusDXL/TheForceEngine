// Lighting

float sqr(float x)
{
	return x*x;
}

float saturate(float x)
{
	return max(0.0, min(1.0, x));
}

// t is between 0, 1; sample is between x1 and x2.
float cubicInterpolation(float x0, float x1, float x2, float x3, float t)
{
	float a = (3.0*x1 - 3.0*x2 + x3 - x0) * 0.5;
	float b = (2.0*x0 - 5.0*x1 + 4.0*x2 - x3) * 0.5;
	float c = (x2 - x0) * 0.5;
	float d = x1;

	float t2 = t * t;
	float t3 = t2 * t;
	return a*t3 + b*t2 + c*t + d;
}

// Compute the point-light attenuation and final color based on the current distance from the light.
// The model consists of the following components:
// Ri : inner radius - where attenuation begins. Default = 0.0, Range: [0, Ro)
// Ro : outer radius - the overall size of the light, attenuation = 0 when d = Ro. Default = 10.0, Range: (0, 256]
// D  : decay speed  - how sharp the decay is. Default = 1.0, Range: [0, 100]
// I  : intensity    - the maximum intensity of the light, when d <= Ri. Default = 1.0, Range: (0, 10]
// Ci : inner color  - light color when d <= Ri. Default: (1,1,1), Range: [-1, 1]
// Co : outer color  - light color when d == Ro. Default: (1,1,1), Range: [-1, 1]
vec3 computeAttenuationColor(float d, float Ri, float Ro, float D, float I, vec3 ci, vec3 co)
{
	// Clamp Ro > Ri on the CPU/data side.
	float s  = max(0.0, d - Ri) / (Ro - Ri);
	float s2 = sqr(s);
	// Gives similar behavior to 1/d^2 falloff but ends at exactly 0.0 at d == Ro with a zero-derivative (i.e. no visible edge)
	float atten = I * sqr(1.0 - s2) / (1.0 + sqr(D*s));
	// Blend the light colors based on how close 's' is to 0 or 1.
	return atten * mix(ci, co, s);
}

vec3 computeLightContrib(vec3 pos, vec3 nrml, vec3 lightPos, float Ri, float Ro, float D, float A, vec3 ci, vec3 co)
{
	vec3 offset = lightPos - pos;
	float d = length(offset);

	// Early-out or normalize.
	if (d >= Ro) { return vec3(0.0); }
	else if (d > 0.0) { offset *= vec3(1.0/d); }

	vec3 colorAtten = computeAttenuationColor(d, Ri, Ro, D, A, ci, co);
	return colorAtten * saturate(dot(offset, nrml));
}

// Maximum luminance approaches [2.61184 + Offset] (4.0 with current settings).
// Linear + Shoulder @ [1.738 + Offset] -> [2.61184 + Offset]
// Function tuned to maximize the linear portion but with a long shoulder to limit clamping.
vec3 tonemapLighting(vec3 light)
{
	float offset = 1.38816;
	float A = 2.61184 + offset;
	float B = 1.32;
	float C = 0.5;

	vec3 Lweights = vec3(0.299, 0.587, 0.114);
	float L = dot(light, Lweights);
	if (L <= 1.738 + offset) { return light; }

	float Lnew = A - B / (C * sqr(L - offset));
	return light * Lnew / L;
}

// Gamma correction is only handled for dynamic lighting -
// this is to avoid modifying the look of the original sector and Z-based lighting.
// albedo: true-color texture value.
// pos, nrml: world space position and normal.
// cameraPos: world space camera position.
// ambient:   current color value (sector ambient, headlamp, etc.).
vec3 handleLighting(vec3 albedo, vec3 pos, vec3 nrml, vec3 cameraPos, vec3 ambient)
{
	vec3 gamma = vec3(2.2);
	vec3 invGamma = vec3(1.0) / gamma;

	// gamma -> linear.
	albedo = pow(albedo, gamma);
	ambient = pow(ambient, gamma);

	vec3 lightPos0 = vec3(201.0, -8.0, 280.0);
	vec3 lightPos1 = cameraPos;//vec3(212.0, -8.0, 242.0);
	vec3 lightColor0 = vec3(1.0, 1.0, 1.0);
	vec3 lightColor1 = vec3(0.25, 1.0, 1.0);

	vec3 light = vec3(0.0);
	light += computeLightContrib(pos, nrml, lightPos0, 20.0, 40.0, 1.0, 2.0, lightColor1, lightColor1);
	light += computeLightContrib(pos, nrml, lightPos1, 0.0, 40.0, 1.0, 2.0, lightColor0, lightColor1);

	// final color.
	vec3 colorLinear = tonemapLighting(light) * albedo + ambient;

	// linear -> gamma.
	return pow(colorLinear, invGamma);
}
