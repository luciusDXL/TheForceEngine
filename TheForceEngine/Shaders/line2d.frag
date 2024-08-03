in vec2 Frag_Pos;
in vec4 Frag_Color;

#ifdef OPT_CURVE
	flat in vec4 Frag_ControlAB;
	flat in vec2 Frag_ControlC;
	flat in vec4 Frag_Offsets;
#else
	in float Frag_Width;
	in vec4 Frag_UV;
#endif

#ifdef OPT_BLOOM
	layout(location = 0) out vec4 Out_Color;
	layout(location = 1) out vec4 Out_Material;
#else
	out vec4 Out_Color;
#endif

#ifdef OPT_CURVE
float cross2d(vec2 a, vec2 b) { return a.x*b.y-a.y*b.x; }

float cos_acos_3(float x)
{
	x=sqrt(0.5+0.5*x);
	return x*(x*(x*(x*-0.008972+0.039071)-0.107074)+0.576975)+0.5;
}

// Exact signed distance, it is more expensive than the approximation below.
float signedDistQuadraticBezierExact(vec2 pos, vec2 v0, vec2 v1, vec2 v2, out float t)
{
	vec2 a = v1 - v0;
	vec2 b = v0 - 2.0*v1 + v2;
	vec2 c = a * 2.0;
	vec2 d = v0 - pos;

	// cubic to be solved (kx*=3 and ky*=3)
	float kk = 1.0/dot(b,b);
	float kx = kk * dot(a,b);
	float ky = kk * (2.0*dot(a,a)+dot(d,b))/3.0;
	float kz = kk * dot(d,a);      

	float res = 0.0;
	float sgn = 0.0;

	float p  = ky - kx*kx;
	float q  = kx*(2.0*kx*kx - 3.0*ky) + kz;
	float p3 = p*p*p;
	float q2 = q*q;
	float h  = q2 + 4.0*p3;

	if( h>=0.0 ) 
	{   // 1 root
		h = sqrt(h);
		vec2 x = (vec2(h,-h)-q)/2.0;

		vec2 uv = sign(x)*pow(abs(x), vec2(1.0/3.0));
		t = uv.x + uv.y;

		// from NinjaKoala - single newton iteration to account for cancellation
		t -= (t*(t*t+3.0*p)+q)/(3.0*t*t+3.0*p);
        
		t = clamp( t-kx, 0.0, 1.0 );
		vec2  w = d+(c+b*t)*t;
		res = dot(w, w);
		sgn = cross2d(c+2.0*b*t,w);
	}
	else 
	{   // 3 roots
		float z = sqrt(-p);
		float m = cos_acos_3( q/(p*z*2.0) );
		float n = sqrt(1.0-m*m);
		n *= sqrt(3.0);
		vec3  t3 = clamp( vec3(m+m,-n-m,n-m)*z-kx, 0.0, 1.0 );
		vec2  qx=d+(c+b*t3.x)*t3.x; float dx=dot(qx, qx), sx=cross2d(a+b*t3.x,qx);
		vec2  qy=d+(c+b*t3.y)*t3.y; float dy=dot(qy, qy), sy=cross2d(a+b*t3.y,qy);
		if( dx<dy ) {res=dx;sgn=sx;t=t3.x;} else {res=dy;sgn=sy;t=t3.y;}
	}
	return sqrt( res )*sign(sgn);
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

	vec2 s = 2.0*(y*j+z*k)-x*i;

    float r = (y*z-x*x*0.25)/dot(s, s);
    t = clamp((0.5*x+y+r*dot(s,w))/(x+y+z), 0.0, 1.0);
	return length( v0+t*(k+k+t*w) );
}

vec2 evaluateCurve(float t)
{
	vec2 a = mix(Frag_ControlC, Frag_ControlAB.xy, 1.0 - t);
	vec2 b = mix(Frag_ControlC, Frag_ControlAB.zw, t);
	return mix(a, b, t);
}

float approximateArcLength(float t)
{
	// Approximate the total length to the current position.
	// The stepCount controls the accuracy, higher values = more accurate,
	// but slower.
	const int stepCount = 4;
	const float ds = 1.0 / float(stepCount);

	vec2 p0 = Frag_ControlAB.xy;
	float s = ds;
	float len = 0.0;

	vec2 p1;
	for (int i = 0; i < stepCount && s < t; i++)
	{
		p1 = evaluateCurve(s);
		len += length(p1 - p0);
		s += ds;
		p0 = p1;
	}
	// Take the remainder to get to the current position on the curve.
	p1 = evaluateCurve(t);
	len += length(p1 - p0);

	return len;
}

#define OPT_CURVE_OFFSETS 1

float calculateCurveAlpha()
{
	// Calculate the distance from the current position to the curve.
	float t;
#ifdef OPT_CURVE_OFFSETS
	float sd = signedDistQuadraticBezierExact(Frag_Pos, Frag_ControlAB.xy, Frag_ControlC, Frag_ControlAB.zw, t);
	
	// Signed distance to alpha value based on desired width, alpha, and partial derivatives.
	float width = 2.0;
	float dFx = abs(dFdx(Frag_Pos.x));
    float dFy = abs(dFdy(Frag_Pos.y));
	float dFxy = 1.5 * max(dFx, dFy);
	float alpha = smoothstep(0.0, -dFxy, abs(sd)-width) * Frag_Color.a;

	// Apply up to 4 offsets, 0 = end of offset list.
	float endWidth = 0.001;
	float offsetAlpha = smoothstep(-endWidth, 0.0, t) * smoothstep(1.0, 1.0-endWidth, t) * 0.5 * Frag_Color.a;
	for (int i = 0; i < 4; i++)
	{
		float offset = Frag_Offsets[i];
		if (offset == 0.0) { break; }

		float curOffsetAlpha = offsetAlpha * smoothstep(0.0, -dFxy, abs(sd-offset)-width);
		// See which alpha we should use.
		alpha = max(alpha, curOffsetAlpha);
	}
#else
	float sd = signedDistQuadraticBezier(Frag_Pos, Frag_ControlAB.xy, Frag_ControlC, Frag_ControlAB.zw, t);

	// Signed distance to alpha value based on desired width, alpha, and partial derivatives.
	float width = 2.0;
	float dFx = abs(dFdx(Frag_Pos.x));
    float dFy = abs(dFdy(Frag_Pos.y));
	float dFxy = 1.5 * max(dFx, dFy);
	float alpha = smoothstep(0.0, -dFxy, abs(sd)-width) * Frag_Color.a;
#endif

#ifdef OPT_DASHED_LINE
	float L = approximateArcLength(t);
	float fade = smoothstep(0.45, 0.5, fract(L * 0.1));
	alpha *= fade;
#endif

	return alpha;
}
#else
float calculateLineAlpha()
{
	vec2 dir = Frag_UV.zw - Frag_UV.xy;
    float l2 = dot(dir, dir);	// dist^2
    float t = max(0.0, min(1.0, dot(Frag_Pos - Frag_UV.xy, dir) / l2));
    vec2 proj = Frag_UV.xy + t * dir;
    float dist = length(Frag_Pos - proj);
    float dFx = abs(dFdx(Frag_Pos.x));
    float dFy = abs(dFdy(Frag_Pos.y));
    float dFxy = 1.5 * max(dFx, dFy);
    float alpha = smoothstep(0.0, -dFxy, dist-Frag_Width) * Frag_Color.a;

#ifdef OPT_DASHED_LINE
	float L = length(t * dir);
	float fade = smoothstep(0.45, 0.5, fract(L*0.1));
	alpha *= fade;
#endif

	return alpha;
}
#endif

void main()
{
#ifdef OPT_CURVE
	float alpha = calculateCurveAlpha();
#else
	float alpha = calculateLineAlpha();
#endif

    Out_Color.rgb = Frag_Color.rgb * alpha;
    Out_Color.a = alpha;

#ifdef OPT_BLOOM
	Out_Material = vec4(0.0);
#endif
}
