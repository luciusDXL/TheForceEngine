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

	f32 distance(float3 a, float3 b)
	{
		f32 dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
		return sqrtf(dx*dx + dy*dy + dz*dz);
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

	float3 normalize(float3 v)
	{
		const f32 lenSq = v.x*v.x + v.y*v.y + v.z*v.z;
		if (fabsf(lenSq) > FLT_EPSILON)
		{
			const f32 scale = 1.0f / sqrtf(lenSq);
			v.x *= scale;
			v.y *= scale;
			v.z *= scale;
		}
		return v;
	}

	float3 cross(float3 s, float3 t)
	{
		return float3(s.y*t.z - s.z*t.y, s.z*t.x - s.x*t.z, s.x*t.y - s.y*t.x);
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
			ScriptLambdaMethod("float3 abs(float3)", (float3 v), float3, { return float3(fabsf(v.x), fabsf(v.y), fabsf(v.z)); });
			ScriptLambdaMethod("bool all(float)", (f32 x), bool, { return x != 0.0f; });
			ScriptLambdaMethod("bool all(float2)", (float2 v), bool, { return v.x != 0.0f && v.y != 0.0f; });
			ScriptLambdaMethod("bool all(float3)", (float3 v), bool, { return v.x != 0.0f && v.y != 0.0f && v.z != 0.0f; });
			ScriptLambdaMethod("bool any(float)", (f32 x), bool, { return x != 0.0f; });
			ScriptLambdaMethod("bool any(float2)", (float2 v), bool, { return v.x != 0.0f || v.y != 0.0f; });
			ScriptLambdaMethod("bool any(float3)", (float3 v), bool, { return v.x != 0.0f || v.y != 0.0f || v.z != 0.0f; });
			ScriptLambdaMethod("float acos(float)", (f32 x), f32, { return acosf(x); });
			ScriptLambdaMethod("float2 acos(float2)", (float2 v), float2, { return float2(acosf(v.x), acosf(v.y)); });
			ScriptLambdaMethod("float3 acos(float3)", (float3 v), float3, { return float3(acosf(v.x), acosf(v.y), acosf(v.z)); });
			ScriptLambdaMethod("float asin(float)", (f32 x), f32, { return asinf(x); });
			ScriptLambdaMethod("float2 asin(float2)", (float2 v), float2, { return float2(asinf(v.x), asinf(v.y)); });
			ScriptLambdaMethod("float3 asin(float3)", (float3 v), float3, { return float3(asinf(v.x), asinf(v.y), asinf(v.z)); });
			ScriptLambdaMethod("float atan(float)", (f32 x), f32, { return atanf(x); });
			ScriptLambdaMethod("float2 atan(float2)", (float2 v), float2, { return float2(atanf(v.x), atanf(v.y)); });
			ScriptLambdaMethod("float3 atan(float3)", (float3 v), float3, { return float3(atanf(v.x), atanf(v.y), atanf(v.z)); });
			ScriptLambdaMethod("float atan2(float, float)", (f32 y, f32 x), f32, { return atan2f(y, x); });
			ScriptLambdaMethod("float2 atan2(float2, float2)", (float2 t, float2 s), float2, { return float2(atan2f(t.x, s.x), atan2f(t.y, s.y)); });
			ScriptLambdaMethod("float3 atan2(float3, float3)", (float3 t, float3 s), float3, { return float3(atan2f(t.x, s.x), atan2f(t.y, s.y), atan2f(t.z, s.z)); });
			ScriptLambdaMethod("float degrees(float)", (f32 r), f32, { return r * 180.0f / PI; });
			ScriptLambdaMethod("float2 degrees(float2)", (float2 r), float2, { return float2(r.x * 180.0f / PI, r.y * 180.0f / PI); });
			ScriptLambdaMethod("float3 degrees(float3)", (float3 r), float3, { return float3(r.x * 180.0f / PI, r.y * 180.0f / PI, r.z * 180.0f / PI); });
			ScriptLambdaMethod("float radians(float)", (f32 d), f32, { return d * PI / 180.0f; });
			ScriptLambdaMethod("float2 radians(float2)", (float2 d), float2, { return float2(d.x * PI / 180.0f, d.y * PI / 180.0f); });
			ScriptLambdaMethod("float3 radians(float3)", (float3 d), float3, { return float3(d.x * PI / 180.0f, d.y * PI / 180.0f, d.z * PI / 180.0f); });
			ScriptLambdaMethod("float exp(float)", (f32 v), f32, { return expf(v); });
			ScriptLambdaMethod("float2 exp(float2)", (float2 v), float2, { return float2(expf(v.x), expf(v.y)); });
			ScriptLambdaMethod("float3 exp(float3)", (float3 v), float3, { return float3(expf(v.x), expf(v.y), expf(v.z)); });
			ScriptLambdaMethod("float exp2(float)", (f32 v), f32, { return exp2f(v); });
			ScriptLambdaMethod("float2 exp2(float2)", (float2 v), float2, { return float2(exp2f(v.x), exp2f(v.y)); });
			ScriptLambdaMethod("float3 exp2(float3)", (float3 v), float3, { return float3(exp2f(v.x), exp2f(v.y), exp2f(v.z)); });
			ScriptLambdaMethod("float log(float)", (f32 v), f32, { return logf(v); });
			ScriptLambdaMethod("float2 log(float2)", (float2 v), float2, { return float2(logf(v.x), logf(v.y)); });
			ScriptLambdaMethod("float3 log(float3)", (float3 v), float3, { return float3(logf(v.x), logf(v.y), logf(v.z)); });
			ScriptLambdaMethod("float log2(float)", (f32 v), f32, { return log2f(v); });
			ScriptLambdaMethod("float2 log2(float2)", (float2 v), float2, { return float2(log2f(v.x), log2f(v.y)); });
			ScriptLambdaMethod("float3 log2(float3)", (float3 v), float3, { return float3(log2f(v.x), log2f(v.y), log2f(v.z)); });
			ScriptLambdaMethod("float log10(float)", (f32 v), f32, { return log10f(v); });
			ScriptLambdaMethod("float2 log10(float2)", (float2 v), float2, { return float2(log10f(v.x), log10f(v.y)); });
			ScriptLambdaMethod("float3 log10(float3)", (float3 v), float3, { return float3(log10f(v.x), log10f(v.y), log10f(v.z)); });
			ScriptLambdaMethod("float pow(float, float)", (f32 s, f32 t), f32, { return powf(s, t); });
			ScriptLambdaMethod("float2 pow(float2, float)", (float2 s, f32 t), float2, { return float2(powf(s.x, t), powf(s.y, t)); });
			ScriptLambdaMethod("float2 pow(float2, float2)", (float2 s, float2 t), float2, { return float2(powf(s.x, t.x), powf(s.y, t.y)); });
			ScriptLambdaMethod("float3 pow(float3, float)", (float3 s, f32 t), float3, { return float3(powf(s.x, t), powf(s.y, t), powf(s.z, t)); });
			ScriptLambdaMethod("float3 pow(float3, float3)", (float3 s, float3 t), float3, { return float3(powf(s.x, t.x), powf(s.y, t.y), powf(s.z, t.z)); });
			ScriptLambdaMethod("float cos(float)", (f32 x), f32, { return cosf(x); });
			ScriptLambdaMethod("float2 cos(float2)", (float2 v), float2, { return float2(cosf(v.x),cosf(v.y)); });
			ScriptLambdaMethod("float3 cos(float3)", (float3 v), float3, { return float3(cosf(v.x),cosf(v.y),cosf(v.z)); });
			ScriptLambdaMethod("float sin(float)", (f32 x), f32, { return sinf(x); });
			ScriptLambdaMethod("float2 sin(float2)", (float2 v), float2, { return float2(sinf(v.x),sinf(v.y)); });
			ScriptLambdaMethod("float3 sin(float3)", (float3 v), float3, { return float3(sinf(v.x),sinf(v.y),sinf(v.z)); });
			ScriptLambdaMethod("float tan(float)", (f32 x), f32, { return tanf(x); });
			ScriptLambdaMethod("float2 tan(float2)", (float2 v), float2, { return float2(tanf(v.x),tanf(v.y)); });
			ScriptLambdaMethod("float3 tan(float3)", (float3 v), float3, { return float3(tanf(v.x),tanf(v.y),tanf(v.z)); });
			ScriptLambdaMethod("float2 sincos(float)", (f32 v), float2, { return float2(sinf(v),cosf(v)); });
			ScriptLambdaMethod("float floor(float)", (f32 x), f32, { return floorf(x); });
			ScriptLambdaMethod("float2 floor(float2)", (float2 v), float2, { return float2(floorf(v.x),floorf(v.y)); });
			ScriptLambdaMethod("float3 floor(float3)", (float3 v), float3, { return float3(floorf(v.x),floorf(v.y),floorf(v.z)); });
			ScriptLambdaMethod("float ceil(float)", (f32 x), f32, { return ceilf(x); });
			ScriptLambdaMethod("float2 ceil(float2)", (float2 v), float2, { return float2(ceilf(v.x),ceilf(v.y)); });
			ScriptLambdaMethod("float3 ceil(float3)", (float3 v), float3, { return float3(ceilf(v.x),ceilf(v.y),ceilf(v.z)); });
			ScriptLambdaMethod("float fract(float)", (f32 x), f32, { return x - floorf(x); });
			ScriptLambdaMethod("float2 fract(float2)", (float2 v), float2, { return float2(v.x - floorf(v.x), v.y - floorf(v.y)); });
			ScriptLambdaMethod("float3 fract(float3)", (float3 v), float3, { return float3(v.x - floorf(v.x), v.y - floorf(v.y), v.z - floorf(v.z)); });
			ScriptLambdaMethod("int max(int, int)", (s32 a, s32 b), s32, { return a > b ? a : b; });
			ScriptLambdaMethod("float max(float, float)", (f32 a, f32 b), f32, { return a > b ? a : b; });
			ScriptLambdaMethod("float2 max(float2, float2)", (float2 a, float2 b), float2, { return float2(a.x > b.x ? a.x : b.x,a.y > b.y ? a.y : b.y); });
			ScriptLambdaMethod("float2 max(float2, float)", (float2 a, f32 b), float2, { return float2(a.x > b ? a.x : b,a.y > b ? a.y : b); });
			ScriptLambdaMethod("float3 max(float3, float3)", (float3 a, float3 b), float3, { return float3(a.x > b.x ? a.x : b.x,a.y > b.y ? a.y : b.y,a.z > b.z ? a.z : b.z); });
			ScriptLambdaMethod("float3 max(float3, float)", (float3 a, f32 b), float3, { return float3(a.x > b ? a.x : b,a.y > b ? a.y : b,a.z > b ? a.z : b); });
			ScriptLambdaMethod("int min(int, int)", (s32 a, s32 b), s32, { return a < b ? a : b; });
			ScriptLambdaMethod("float min(float, float)", (f32 a, f32 b), f32, { return a < b ? a : b; });
			ScriptLambdaMethod("float2 min(float2, float2)", (float2 a, float2 b), float2, { return float2(a.x < b.x ? a.x : b.x,a.y < b.y ? a.y : b.y); });
			ScriptLambdaMethod("float2 min(float2, float)", (float2 a, f32 b), float2, { return float2(a.x < b ? a.x : b,a.y < b ? a.y : b); });
			ScriptLambdaMethod("float3 min(float3, float3)", (float3 a, float3 b), float3, { return float3(a.x < b.x ? a.x : b.x,a.y < b.y ? a.y : b.y,a.z < b.z ? a.z : b.z); });
			ScriptLambdaMethod("float3 min(float3, float)", (float3 a, f32 b), float3, { return float3(a.x < b ? a.x : b,a.y < b ? a.y : b,a.z < b ? a.z : b); });
			ScriptLambdaMethod("int clamp(int, int, int)", (s32 x, s32 a, s32 b), s32, { return clamp(x, a, b); });
			ScriptLambdaMethod("float clamp(float, float, float)", (f32 x, f32 a, f32 b), f32, { return clamp(x, a, b); });
			ScriptLambdaMethod("float2 clamp(float2, float2, float2)", (float2 v, float2 a, float2 b), float2, { return float2(clamp(v.x, a.x, b.x), clamp(v.y, a.y, b.y)); });
			ScriptLambdaMethod("float2 clamp(float2, float, float)", (float2 v, f32 a, f32 b), float2, { return float2(clamp(v.x, a, b), clamp(v.y, a, b)); });
			ScriptLambdaMethod("float3 clamp(float3, float3, float3)", (float3 v, float3 a, float3 b), float3, { return float3(clamp(v.x, a.x, b.x), clamp(v.y, a.y, b.y), clamp(v.z, a.z, b.z)); });
			ScriptLambdaMethod("float3 clamp(float3, float, float)", (float3 v, f32 a, f32 b), float3, { return float3(clamp(v.x, a, b), clamp(v.y, a, b), clamp(v.z, a, b)); });
			ScriptLambdaMethod("float saturate(float)", (f32 x), f32, { return clamp(x, 0.0f, 1.0f); });
			ScriptLambdaMethod("float2 saturate(float2)", (float2 v), float2, { return float2(clamp(v.x, 0.0f, 1.0f), clamp(v.y, 0.0f, 1.0f)); });
			ScriptLambdaMethod("float3 saturate(float3)", (float3 v), float3, { return float3(clamp(v.x, 0.0f, 1.0f), clamp(v.y, 0.0f, 1.0f), clamp(v.z, 0.0f, 1.0f)); });
			ScriptLambdaMethod("float mix(float, float, float)", (f32 x, f32 y, f32 a), f32, { return mix(x, y, a); });
			ScriptLambdaMethod("float2 mix(float2, float2, float2)", (float2 s, float2 t, float2 a), float2, { return float2(mix(s.x, t.x, a.x), mix(s.y, t.y, a.y)); });
			ScriptLambdaMethod("float2 mix(float2, float2, float)", (float2 s, float2 t, f32 a), float2, { return float2(mix(s.x, t.x, a), mix(s.y, t.y, a)); });
			ScriptLambdaMethod("float3 mix(float3, float3, float3)", (float3 s, float3 t, float3 a), float3, { return float3(mix(s.x, t.x, a.x), mix(s.y, t.y, a.y), mix(s.z, t.z, a.z)); });
			ScriptLambdaMethod("float3 mix(float3, float3, float)", (float3 s, float3 t, f32 a), float3, { return float3(mix(s.x, t.x, a), mix(s.y, t.y, a), mix(s.z, t.z, a)); });
			ScriptLambdaMethod("int step(int, int)", (s32 edge, s32 x), s32, { return step(edge, x); });
			ScriptLambdaMethod("float step(float, float)", (f32 edge, f32 x), f32, { return step(edge, x); });
			ScriptLambdaMethod("float2 step(float2, float2)", (float2 edge, float2 v), float2, { return float2(step(edge.x, v.x), step(edge.y, v.y)); });
			ScriptLambdaMethod("float2 step(float, float2)", (f32 edge, float2 v), float2, { return float2(step(edge, v.x), step(edge, v.y)); });
			ScriptLambdaMethod("float3 step(float3, float3)", (float3 edge, float3 v), float3, { return float3(step(edge.x, v.x), step(edge.y, v.y), step(edge.z, v.z)); });
			ScriptLambdaMethod("float3 step(float, float3)", (f32 edge, float3 v), float3, { return float3(step(edge, v.x), step(edge, v.y), step(edge, v.z)); });
			ScriptLambdaMethod("int sign(int)", (s32 v), s32, { return (v == 0) ? 0 : (v < 0) ? -1 : 1; });
			ScriptLambdaMethod("float sign(float)", (f32 v), f32, { return (v == 0.0f) ? 0.0f : (v < 0.0f) ? -1.0f : 1.0f; });
			ScriptLambdaMethod("float2 sign(float2)", (float2 v), float2, { return float2((v.x == 0.0f) ? 0.0f : (v.x < 0.0f) ? -1.0f : 1.0f, (v.y == 0.0f) ? 0.0f : (v.y < 0.0f) ? -1.0f : 1.0f); });
			ScriptLambdaMethod("float3 sign(float3)", (float3 v), float3, { return float3((v.x == 0.0f) ? 0.0f : (v.x < 0.0f) ? -1.0f : 1.0f, (v.y == 0.0f) ? 0.0f : (v.y < 0.0f) ? -1.0f : 1.0f, (v.z == 0.0f) ? 0.0f : (v.z < 0.0f) ? -1.0f : 1.0f); });
			ScriptLambdaMethod("float sqrt(float)", (f32 v), f32, { return sqrtf(v); });
			ScriptLambdaMethod("float2 sqrt(float2)", (float2 v), float2, { return float2(sqrtf(v.x), sqrtf(v.y)); });
			ScriptLambdaMethod("float3 sqrt(float3)", (float3 v), float3, { return float3(sqrtf(v.x), sqrtf(v.y), sqrtf(v.z)); });
			ScriptLambdaMethod("float smoothstep(float, float, float)", (f32 edge0, f32 edge1, f32 x), f32, { return smoothstep(edge0, edge1, x); });
			ScriptLambdaMethod("float2 smoothstep(float2, float2, float2)", (float2 edge0, float2 edge1, float2 v), float2, { return float2(smoothstep(edge0.x, edge1.x, v.x), smoothstep(edge0.y, edge1.y, v.y)); });
			ScriptLambdaMethod("float2 smoothstep(float, float, float2)", (f32 edge0, f32 edge1, float2 v), float2, { return float2(smoothstep(edge0, edge1, v.x), smoothstep(edge0, edge1, v.y)); });
			ScriptLambdaMethod("float2 smoothstep(float2, float2, float)", (float2 edge0, float2 edge1, f32 v), float2, { return float2(smoothstep(edge0.x, edge1.x, v), smoothstep(edge0.y, edge1.y, v)); });
			ScriptLambdaMethod("float3 smoothstep(float3, float3, float3)", (float3 edge0, float3 edge1, float3 v), float3, { return float3(smoothstep(edge0.x, edge1.x, v.x), smoothstep(edge0.y, edge1.y, v.y), smoothstep(edge0.z, edge1.z, v.z)); });
			ScriptLambdaMethod("float3 smoothstep(float, float, float3)", (f32 edge0, f32 edge1, float3 v), float3, { return float3(smoothstep(edge0, edge1, v.x), smoothstep(edge0, edge1, v.y), smoothstep(edge0, edge1, v.z)); });
			ScriptLambdaMethod("float3 smoothstep(float3, float3, float)", (float3 edge0, float3 edge1, f32 v), float3, { return float3(smoothstep(edge0.x, edge1.x, v), smoothstep(edge0.y, edge1.y, v), smoothstep(edge0.z, edge1.z, v)); });
			ScriptLambdaMethod("float mod(float, float)", (f32 s, f32 t), f32, { return fmodf(s, t); });
			ScriptLambdaMethod("float2 mod(float2, float2)", (float2 s, float2 t), float2, { return float2(fmodf(s.x, t.x),fmodf(s.y, t.y)); });
			ScriptLambdaMethod("float2 mod(float2, float)", (float2 s, f32 t), float2, { return float2(fmodf(s.x, t),fmodf(s.y, t)); });
			ScriptLambdaMethod("float3 mod(float3, float3)", (float3 s, float3 t), float3, { return float3(fmodf(s.x, t.x),fmodf(s.y, t.y),fmodf(s.z, t.z)); });
			ScriptLambdaMethod("float3 mod(float3, float)", (float3 s, f32 t), float3, { return float3(fmodf(s.x, t),fmodf(s.y, t),fmodf(s.z, t)); });
			ScriptLambdaMethod("float distance(float, float)", (f32 a, f32 b), f32, { return fabsf(a - b); });
			ScriptLambdaMethod("float distance(float2, float2)", (float2 a, float2 b), f32, { return distance(a, b); });
			ScriptLambdaMethod("float distance(float3, float3)", (float3 a, float3 b), f32, { return distance(a, b); });
			ScriptLambdaMethod("float dot(float, float)", (f32 a, f32 b), f32, { return a*b; });
			ScriptLambdaMethod("float dot(float2, float2)", (float2 a, float2 b), f32, { return a.x*b.x + a.y*b.y; });
			ScriptLambdaMethod("float dot(float3, float3)", (float3 a, float3 b), f32, { return a.x*b.x + a.y*b.y + a.z*b.z; });
			ScriptLambdaMethod("float length(float)", (f32 a), f32, { return fabsf(a); });
			ScriptLambdaMethod("float length(float2)", (float2 a), f32, { return sqrtf(a.x*a.x + a.y*a.y); });
			ScriptLambdaMethod("float length(float3)", (float3 a), f32, { return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z); });
			ScriptLambdaMethod("float normalize(float)", (f32 a), f32, { return a < 0.0f ? -1.0f : 1.0f; });
			ScriptLambdaMethod("float2 normalize(float2)", (float2 a), float2, { return normalize(a); });
			ScriptLambdaMethod("float3 normalize(float3)", (float3 a), float3, { return normalize(a); });
			ScriptLambdaMethod("float  perp(float2, float2)", (float2 a, float2 b), f32, { return a.x*b.y - a.y*b.x; });
			ScriptLambdaMethod("float3 cross(float3, float3)", (float3 a, float3 b), float3, { return cross(a, b); });
		}
		ScriptClassEnd();
	}
}
#endif