#include "scriptMath.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Level/levelData.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>

#ifdef ENABLE_FORCE_SCRIPT
#include <angelscript.h>

using namespace TFE_Jedi;

namespace TFE_ForceScript
{
	u32 countBits(u32 bits)
	{
		u32 count = 0;
		while (bits)
		{
			if (bits & 1) { count++; }
			bits >>= 1;
		}
		return count;
	}

	s32 countBits(s32 bits)
	{
		s32 count = 0;
		u32 ubits = *((u32*)&bits);
		while (ubits)
		{
			if (ubits & 1) { count++; }
			ubits >>= 1;
		}
		return count;
	}

	f32 mix(f32 x, f32 y, f32 a)
	{
		return x * (1.0f - a) + y * a;
	}

	f32 step(f32 edge, f32 x)
	{
		return edge < x ? 0.0f : 1.0f;
	}

	s32 step(s32 edge, s32 x)
	{
		return edge < x ? 0 : 1;
	}

	f32 smoothstep(f32 edge0, f32 edge1, f32 x)
	{
		x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		return x * x * (3.0f - 2.0f*x);
	}

	f32 distance(float2 a, float2 b)
	{
		f32 dx = a.x - b.x, dy = a.y - b.y;
		return sqrtf(dx*dx + dy*dy);
	}

	float2 normalize(float2 v)
	{
		const f32 lenSq = v.x*v.x + v.y*v.y;
		if (fabsf(lenSq) > FLT_EPSILON)
		{
			const f32 scale = 1.0f / sqrtf(lenSq);
			v.x *= scale;
			v.y *= scale;
		}
		return v;
	}

	bool ScriptMath::scriptRegister(ScriptAPI api)
	{
		ScriptClassBegin("Math", "math", api);
		{
			// Constants
			ScriptLambdaPropertyGet("float get_pi()", f32, { return PI; });
			ScriptLambdaPropertyGet("float get_twoPi()", f32, { return TWO_PI; });
			ScriptLambdaPropertyGet("float get_e()", f32, { return 2.718281828459045f; });

			// Basic math functions - include overrides for different vector types.
			ScriptLambdaMethod("uint countBits(uint)", (u32 x), u32, { return countBits(x); });
			ScriptLambdaMethod("int countBits(int)", (s32 x), s32, { return countBits(x); });
			ScriptLambdaMethod("int abs(int)", (s32 x), s32, { return x < 0 ? -x : x; });
			ScriptLambdaMethod("float abs(float)", (f32 x), f32, { return fabsf(x); });
			ScriptLambdaMethod("float2 abs(float2)", (float2 v), float2, { return float2(fabsf(v.x), fabsf(v.y)); });
			ScriptLambdaMethod("bool all(float)", (f32 x), bool, { return x != 0.0f; });
			ScriptLambdaMethod("bool all(float2)", (float2 v), bool, { return v.x != 0.0f && v.y != 0.0f; });
			ScriptLambdaMethod("bool any(float)", (f32 x), bool, { return x != 0.0f; });
			ScriptLambdaMethod("bool any(float2)", (float2 v), bool, { return v.x != 0.0f || v.y != 0.0f; });
			ScriptLambdaMethod("float acos(float)", (f32 x), f32, { return acosf(x); });
			ScriptLambdaMethod("float2 acos(float2)", (float2 v), float2, { return float2(acosf(v.x), acosf(v.y)); });
			ScriptLambdaMethod("float asin(float)", (f32 x), f32, { return asinf(x); });
			ScriptLambdaMethod("float2 asin(float2)", (float2 v), float2, { return float2(asinf(v.x), asinf(v.y)); });
			ScriptLambdaMethod("float atan(float)", (f32 x), f32, { return atanf(x); });
			ScriptLambdaMethod("float2 atan(float2)", (float2 v), float2, { return float2(atanf(v.x), atanf(v.y)); });
			ScriptLambdaMethod("float atan2(float, float)", (f32 y, f32 x), f32, { return atan2f(y, x); });
			ScriptLambdaMethod("float2 atan2(float2, float2)", (float2 t, float2 s), float2, { return float2(atan2f(t.x, s.x), atan2f(t.y, s.y)); });
			ScriptLambdaMethod("float degrees(float)", (f32 r), f32, { return r * 180.0f / PI; });
			ScriptLambdaMethod("float2 degrees(float2)", (float2 r), float2, { return float2(r.x * 180.0f / PI, r.y * 180.0f / PI); });
			ScriptLambdaMethod("float radians(float)", (f32 d), f32, { return d * PI / 180.0f; });
			ScriptLambdaMethod("float2 radians(float2)", (float2 d), float2, { return float2(d.x * PI / 180.0f, d.y * PI / 180.0f); });
			ScriptLambdaMethod("float exp(float)", (f32 v), f32, { return expf(v); });
			ScriptLambdaMethod("float2 exp(float2)", (float2 v), float2, { return float2(expf(v.x), expf(v.y)); });
			ScriptLambdaMethod("float exp2(float)", (f32 v), f32, { return exp2f(v); });
			ScriptLambdaMethod("float2 exp2(float2)", (float2 v), float2, { return float2(exp2f(v.x), exp2f(v.y)); });
			ScriptLambdaMethod("float log(float)", (f32 v), f32, { return logf(v); });
			ScriptLambdaMethod("float2 log(float2)", (float2 v), float2, { return float2(logf(v.x), logf(v.y)); });
			ScriptLambdaMethod("float log2(float)", (f32 v), f32, { return log2f(v); });
			ScriptLambdaMethod("float2 log2(float2)", (float2 v), float2, { return float2(log2f(v.x), log2f(v.y)); });
			ScriptLambdaMethod("float log10(float)", (f32 v), f32, { return log10f(v); });
			ScriptLambdaMethod("float2 log10(float2)", (float2 v), float2, { return float2(log10f(v.x), log10f(v.y)); });
			ScriptLambdaMethod("float pow(float, float)", (f32 s, f32 t), f32, { return powf(s, t); });
			ScriptLambdaMethod("float2 pow(float2, float)", (float2 s, f32 t), float2, { return float2(powf(s.x, t), powf(s.y, t)); });
			ScriptLambdaMethod("float2 pow(float2, float2)", (float2 s, float2 t), float2, { return float2(powf(s.x, t.x), powf(s.y, t.y)); });
			ScriptLambdaMethod("float cos(float)", (f32 x), f32, { return cosf(x); });
			ScriptLambdaMethod("float2 cos(float2)", (float2 v), float2, { return float2(cosf(v.x),cosf(v.y)); });
			ScriptLambdaMethod("float sin(float)", (f32 x), f32, { return sinf(x); });
			ScriptLambdaMethod("float2 sin(float2)", (float2 v), float2, { return float2(sinf(v.x),sinf(v.x)); });
			ScriptLambdaMethod("float tan(float)", (f32 x), f32, { return tanf(x); });
			ScriptLambdaMethod("float2 tan(float2)", (float2 v), float2, { return float2(tanf(v.x),tanf(v.x)); });
			ScriptLambdaMethod("float2 sincos(float)", (f32 v), float2, { return float2(sinf(v),cosf(v)); });
			ScriptLambdaMethod("float floor(float)", (f32 x), f32, { return floorf(x); });
			ScriptLambdaMethod("float2 floor(float2)", (float2 v), float2, { return float2(floorf(v.x),floorf(v.y)); });
			ScriptLambdaMethod("float ceil(float)", (f32 x), f32, { return ceilf(x); });
			ScriptLambdaMethod("float2 ceil(float2)", (float2 v), float2, { return float2(ceilf(v.x),ceilf(v.y)); });
			ScriptLambdaMethod("float fract(float)", (f32 x), f32, { return x - floorf(x); });
			ScriptLambdaMethod("float2 fract(float2)", (float2 v), float2, { return float2(v.x - floorf(v.x), v.y - floorf(v.y)); });
			ScriptLambdaMethod("int max(int, int)", (s32 a, s32 b), s32, { return a > b ? a : b; });
			ScriptLambdaMethod("float max(float, float)", (f32 a, f32 b), f32, { return a > b ? a : b; });
			ScriptLambdaMethod("float2 max(float2, float2)", (float2 a, float2 b), float2, { return float2(a.x > b.x ? a.x : b.x,a.y > b.y ? a.y : b.y); });
			ScriptLambdaMethod("float2 max(float2, float)", (float2 a, f32 b), float2, { return float2(a.x > b ? a.x : b,a.y > b ? a.y : b); });
			ScriptLambdaMethod("int min(int, int)", (s32 a, s32 b), s32, { return a < b ? a : b; });
			ScriptLambdaMethod("float min(float, float)", (f32 a, f32 b), f32, { return a < b ? a : b; });
			ScriptLambdaMethod("float2 min(float2, float2)", (float2 a, float2 b), float2, { return float2(a.x < b.x ? a.x : b.x,a.y < b.y ? a.y : b.y); });
			ScriptLambdaMethod("float2 min(float2, float)", (float2 a, f32 b), float2, { return float2(a.x < b ? a.x : b,a.y < b ? a.y : b); });
			ScriptLambdaMethod("int clamp(int, int, int)", (s32 x, s32 a, s32 b), s32, { return clamp(x, a, b); });
			ScriptLambdaMethod("float clamp(float, float, float)", (f32 x, f32 a, f32 b), f32, { return clamp(x, a, b); });
			ScriptLambdaMethod("float2 clamp(float2, float2, float2)", (float2 v, float2 a, float2 b), float2, { return float2(clamp(v.x, a.x, b.x), clamp(v.y, a.y, b.y)); });
			ScriptLambdaMethod("float2 clamp(float2, float, float)", (float2 v, f32 a, f32 b), float2, { return float2(clamp(v.x, a, b), clamp(v.y, a, b)); });
			ScriptLambdaMethod("float saturate(float)", (f32 x), f32, { return clamp(x, 0.0f, 1.0f); });
			ScriptLambdaMethod("float2 saturate(float2)", (float2 v), float2, { return float2(clamp(v.x, 0.0f, 1.0f), clamp(v.y, 0.0f, 1.0f)); });
			ScriptLambdaMethod("float mix(float, float, float)", (f32 x, f32 y, f32 a), f32, { return mix(x, y, a); });
			ScriptLambdaMethod("float2 mix(float2, float2, float2)", (float2 s, float2 t, float2 a), float2, { return float2(mix(s.x, t.x, a.x), mix(s.y, t.y, a.y)); });
			ScriptLambdaMethod("float2 mix(float2, float2, float)", (float2 s, float2 t, f32 a), float2, { return float2(mix(s.x, t.x, a), mix(s.y, t.y, a)); });
			ScriptLambdaMethod("int step(int, int)", (s32 edge, s32 x), s32, { return step(edge, x); });
			ScriptLambdaMethod("float step(float, float)", (f32 edge, f32 x), f32, { return step(edge, x); });
			ScriptLambdaMethod("float2 step(float2, float2)", (float2 edge, float2 v), float2, { return float2(step(edge.x, v.x), step(edge.y, v.y)); });
			ScriptLambdaMethod("float2 step(float, float2)", (f32 edge, float2 v), float2, { return float2(step(edge, v.x), step(edge, v.y)); });
			ScriptLambdaMethod("int sign(int)", (s32 v), s32, { return (v == 0) ? 0 : (v < 0) ? -1 : 1; });
			ScriptLambdaMethod("float sign(float)", (f32 v), f32, { return (v == 0.0f) ? 0.0f : (v < 0.0f) ? -1.0f : 1.0f; });
			ScriptLambdaMethod("float2 sign(float2)", (float2 v), float2, { return float2((v.x == 0.0f) ? 0.0f : (v.x < 0.0f) ? -1.0f : 1.0f, (v.y == 0.0f) ? 0.0f : (v.y < 0.0f) ? -1.0f : 1.0f); });
			ScriptLambdaMethod("float sqrt(float)", (f32 v), f32, { return sqrtf(v); });
			ScriptLambdaMethod("float2 sqrt(float2)", (float2 v), float2, { return float2(sqrtf(v.x), sqrtf(v.y)); });
			ScriptLambdaMethod("float smoothstep(float, float, float)", (f32 edge0, f32 edge1, f32 x), f32, { return smoothstep(edge0, edge1, x); });
			ScriptLambdaMethod("float2 smoothstep(float2, float2, float2)", (float2 edge0, float2 edge1, float2 v), float2, { return float2(smoothstep(edge0.x, edge1.x, v.x), smoothstep(edge0.y, edge1.y, v.y)); });
			ScriptLambdaMethod("float2 smoothstep(float, float, float2)", (f32 edge0, f32 edge1, float2 v), float2, { return float2(smoothstep(edge0, edge1, v.x), smoothstep(edge0, edge1, v.y)); });
			ScriptLambdaMethod("float2 smoothstep(float2, float2, float)", (float2 edge0, float2 edge1, f32 v), float2, { return float2(smoothstep(edge0.x, edge1.x, v), smoothstep(edge0.y, edge1.y, v)); });
			ScriptLambdaMethod("float mod(float, float)", (f32 s, f32 t), f32, { return fmodf(s, t); });
			ScriptLambdaMethod("float2 mod(float2, float2)", (float2 s, float2 t), float2, { return float2(fmodf(s.x, t.x),fmodf(s.y, t.y)); });
			ScriptLambdaMethod("float2 mod(float2, float)", (float2 s, f32 t), float2, { return float2(fmodf(s.x, t),fmodf(s.y, t)); });
			ScriptLambdaMethod("float distance(float, float)", (f32 a, f32 b), f32, { return fabsf(a - b); });
			ScriptLambdaMethod("float distance(float2, float2)", (float2 a, float2 b), f32, { return distance(a, b); });
			ScriptLambdaMethod("float dot(float, float)", (f32 a, f32 b), f32, { return a*b; });
			ScriptLambdaMethod("float dot(float2, float2)", (float2 a, float2 b), f32, { return a.x*b.x + a.y*b.y; });
			ScriptLambdaMethod("float length(float)", (f32 a), f32, { return fabsf(a); });
			ScriptLambdaMethod("float length(float2)", (float2 a), f32, { return sqrtf(a.x*a.x + a.y*a.y); });
			ScriptLambdaMethod("float normalize(float)", (f32 a), f32, { return a < 0.0f ? -1.0f : 1.0f; });
			ScriptLambdaMethod("float2 normalize(float2)", (float2 a), float2, { return normalize(a); });
		}
		ScriptClassEnd();
	}
}
#endif