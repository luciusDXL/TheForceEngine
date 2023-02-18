// Lighting
// 28 bytes per light.
uniform samplerBuffer  lightPosition;	// vec3  x max lights = 12 * L; L = 8192, size = 96kB
uniform usamplerBuffer lightData;		// uvec4 x max lights = 16 * L; L = 8192, size = 128kB
uniform usamplerBuffer lightClusters;	// uvec4 x max clusters = 16 * 1024 = 16kB.

vec3 unpackColor(uint packedClr)
{
	float scale = 1.0 / 255.0;

	vec3 color;
	color.x = float(packedClr & 255u) * scale;
	color.y = float((packedClr >> 8u) & 255u) * scale;
	color.z = float((packedClr >> 16u) & 255u) * scale;
	return color;
}

vec2 unpackFixed10_6(uint packedV2)
{
	float scale = 1.0 / 63.0;

	vec2 v2;
	v2.x = float(packedV2 & 65535u) * scale;
	v2.y = float((packedV2 >> 16u) & 65535u) * scale;
	return v2;
}

vec2 unpackFixed4_12(uint packedV2)
{
	float scale = 1.0 / 4095.0;

	vec2 v2;
	v2.x = float(packedV2 & 65535u) * scale;
	v2.y = float((packedV2 >> 16u) & 65535u) * scale;
	return v2;
}

void getLightData(int index, out vec3 pos, out vec3 c0, out vec3 c1, out vec2 radius, out vec2 decayAmp)
{
	pos = texelFetch(lightPosition, index).xyz;
	uvec4 packedData = texelFetch(lightData, index);

	c0 = unpackColor(packedData.x);
	c1 = unpackColor(packedData.y);
	radius = unpackFixed10_6(packedData.z);
	decayAmp = unpackFixed4_12(packedData.w);
}

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

int getLightClusterId(vec3 posWS, vec3 cameraPos)
{
	vec2 offset = abs(posWS.xz - cameraPos.xz);

	float d = max(offset.x, offset.y) / 16.0;
	float mip = log2(max(1.0, d));
	float scale = pow(2.0, floor(mip) + 3.0);

	// This is the minimum corner for the current mip-level.
	vec2 clusterOffset = vec2(scale * 4.0);
	// This generates a clusterAddr such that x : [0, 7] and z : [0, 7]
	ivec2 clusterAddr = ivec2((posWS.xz - cameraPos.xz + clusterOffset) / scale);
	// This generates a final ID ranging from [0, 63]
	int clusterId = clusterAddr.x + clusterAddr.y * 8;

	int mipIndex = int(mip);
	if (mipIndex >= 1)
	{
		// In larger mips, the center 4x4 is the previous mip.
		// So those Ids need to be adjusted.
		if (clusterAddr.y >= 6)
		{
			clusterId = 32 + (clusterAddr.y - 6) * 8 + clusterAddr.x;
		}
		else if (clusterAddr.y >= 2)
		{
			clusterId = 16 + (clusterAddr.y - 2) * 4 + ((clusterAddr.x < 2) ? clusterAddr.x : clusterAddr.x - 4);
		}
		// Finally linearlize the ID factoring in the current mip.
		clusterId += 64 + (mipIndex - 1) * 48;
	}

	return clusterId;
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

	vec3 light = vec3(0.0);
	int clusterId = getLightClusterId(pos, cameraPos);
	uvec4 lightIndices = texelFetch(lightClusters, clusterId);
	for (int i = 0; i < 4; i++)
	{
		uint data = lightIndices[i];
		if (data == 0u) { break; }
		int index0 = int(data & 65535u);
		int index1 = int((data >> 16u) & 65535u);
		
		if (index0 != 0)
		{
			vec3 lightPos;
			vec3 c0, c1;
			vec2 radii, decayAmp;
			getLightData(index0 - 1, lightPos, c0, c1, radii, decayAmp);

			light += computeLightContrib(pos, nrml, lightPos, radii.x, radii.y, decayAmp.x, decayAmp.y, c0, c1);
		}
		if (index1 != 0)
		{
			vec3 lightPos;
			vec3 c0, c1;
			vec2 radii, decayAmp;
			getLightData(index1 - 1, lightPos, c0, c1, radii, decayAmp);

			light += computeLightContrib(pos, nrml, lightPos, radii.x, radii.y, decayAmp.x, decayAmp.y, c0, c1);
		}
	}

	// final color.
	vec3 colorLinear = tonemapLighting(light) * albedo + ambient;

	// linear -> gamma.
	return pow(colorLinear, invGamma);
}
