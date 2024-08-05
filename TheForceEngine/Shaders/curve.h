#define OPT_CURVE_OFFSETS 1

float cross2d(vec2 a, vec2 b) { return a.x*b.y - a.y*b.x; }

float cos_acos_3(float x)
{
	x = sqrt(0.5 + 0.5*x);
	return x * (x*(x*(x*-0.008972 + 0.039071) - 0.107074) + 0.576975) + 0.5;
}

// Exact signed distance, it is more expensive than the approximation below.
float signedDistQuadraticBezierExact(vec2 pos, vec2 v0, vec2 v1, vec2 v2, out float t)
{
	vec2 a = v1 - v0;
	vec2 b = v0 - 2.0*v1 + v2;
	vec2 c = a * 2.0;
	vec2 d = v0 - pos;

	// cubic to be solved (kx*=3 and ky*=3)
	float kk = 1.0 / dot(b, b);
	float kx = kk * dot(a, b);
	float ky = kk * (2.0*dot(a, a) + dot(d, b)) / 3.0;
	float kz = kk * dot(d, a);

	float res = 0.0;
	float sgn = 0.0;

	float p = ky - kx * kx;
	float q = kx * (2.0*kx*kx - 3.0*ky) + kz;
	float p3 = p * p*p;
	float q2 = q * q;
	float h = q2 + 4.0*p3;

	if (h >= 0.0)
	{   // 1 root
		h = sqrt(h);
		vec2 x = (vec2(h, -h) - q) / 2.0;

		vec2 uv = sign(x)*pow(abs(x), vec2(1.0 / 3.0));
		t = uv.x + uv.y;

		// from NinjaKoala - single newton iteration to account for cancellation
		t -= (t*(t*t + 3.0*p) + q) / (3.0*t*t + 3.0*p);

		t = clamp(t - kx, 0.0, 1.0);
		vec2  w = d + (c + b * t)*t;
		res = dot(w, w);
		sgn = cross2d(c + 2.0*b*t, w);
	}
	else
	{   // 3 roots
		float z = sqrt(-p);
		float m = cos_acos_3(q / (p*z*2.0));
		float n = sqrt(1.0 - m * m);
		n *= sqrt(3.0);
		vec3  t3 = clamp(vec3(m + m, -n - m, n - m)*z - kx, 0.0, 1.0);
		vec2  qx = d + (c + b * t3.x)*t3.x; float dx = dot(qx, qx), sx = cross2d(a + b * t3.x, qx);
		vec2  qy = d + (c + b * t3.y)*t3.y; float dy = dot(qy, qy), sy = cross2d(a + b * t3.y, qy);
		if (dx < dy) { res = dx; sgn = sx; t = t3.x; }
		else { res = dy; sgn = sy; t = t3.y; }
	}
	return sqrt(res)*sign(sgn);
}

// This method provides just an approximation, and is only usable in
// the very close neighborhood of the curve. Taken and adapted from
// http://research.microsoft.com/en-us/um/people/hoppe/ravg.pdf
// See https://www.shadertoy.com/view/MlKcDD
float signedDistQuadraticBezier(vec2 p, vec2 v0, vec2 v1, vec2 v2, out float t)
{
	vec2 i = v0 - v2;
	vec2 j = v2 - v1;
	vec2 k = v1 - v0;
	vec2 w = j - k;

	v0 -= p;
	v1 -= p;
	v2 -= p;

	float x = cross2d(v0, v2);
	float y = cross2d(v1, v0);
	float z = cross2d(v2, v1);

	vec2 s = 2.0*(y*j + z * k) - x * i;

	float r = (y*z - x * x*0.25) / dot(s, s);
	t = clamp((0.5*x + y + r * dot(s, w)) / (x + y + z), 0.0, 1.0);
	return length(v0 + t * (k + k + t * w));
}

vec2 evaluateCurve(vec2 v0, vec2 v1, vec2 c, float t)
{
	vec2 a = mix(c, v0, 1.0 - t);
	vec2 b = mix(c, v1, t);
	return mix(a, b, t);
}

float approximateArcLength(vec2 v0, vec2 v1, vec2 c, float t)
{
	// Approximate the total length to the current position.
	// The stepCount controls the accuracy, higher values = more accurate,
	// but slower.
	const int stepCount = 4;
	const float ds = 1.0 / float(stepCount);

	vec2 p0 = v0;
	float s = ds;
	float len = 0.0;

	vec2 p1;
	for (int i = 0; i < stepCount && s < t; i++)
	{
		p1 = evaluateCurve(v0, v1, c, s);
		len += length(p1 - p0);
		s += ds;
		p0 = p1;
	}
	// Take the remainder to get to the current position on the curve.
	p1 = evaluateCurve(v0, v1, c, t);
	len += length(p1 - p0);

	return len;
}