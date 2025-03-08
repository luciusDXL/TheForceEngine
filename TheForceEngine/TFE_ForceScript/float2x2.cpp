#include "float2x2.h"
#include "forceScript.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/frontEndUi.h>
#include <stdint.h>
#include <cstring>
#include <string>
#include <algorithm>
#include <assert.h>

// TFE_ForceScript wraps Anglescript, so these includes should only exist here.
#include <angelscript.h>

namespace TFE_ForceScript
{
	float2x2::float2x2()
	{
		m[0] = 1.0f; m[1] = 0.0f;
		m[2] = 0.0f; m[3] = 1.0f;
	}

	float2x2::float2x2(float2 _r0, float2 _r1)
	{
		r[0] = _r0;
		r[1] = _r1;
	}

	float2x2::float2x2(const float2x2& other)
	{
		r[0] = other.r[0];
		r[1] = other.r[1];
	}

	float2x2::float2x2(f32 m00, f32 m01, f32 m10, f32 m11)
	{
		m[0] = m00; m[1] = m01;
		m[2] = m10; m[3] = m11;
	}

	bool float2x2::operator==(const float2x2& o) const
	{
		return (m[0] == o.m[0]) && (m[1] == o.m[1]) && (m[2] == o.m[2]) && (m[3] == o.m[3]);
	}
	bool float2x2::operator!=(const float2x2& o) const
	{
		return !(*this == o);
	}

	float2x2& float2x2::operator=(const float2x2& other)
	{
		r[0] = other.r[0];
		r[1] = other.r[1];
		return *this;
	}
	// Compound float2
	float2x2& float2x2::operator*=(const float2x2& other)
	{
		*this = *this * other;
		return *this;
	}
	float2x2& float2x2::operator*=(const f32& other)
	{
		*this = *this * other;
		return *this;
	}
		
	float2x2 matMul(const float2x2& a, const float2x2& b)
	{
		float2 r0 = a.r[0];
		float2 r1 = a.r[1];
		float2 c0(b.r[0].x, b.r[1].x);
		float2 c1(b.r[0].y, b.r[1].y);
		
		return float2x2(dot(r0, c0), dot(r0, c1), dot(r1, c0), dot(r1, c1));
	}

	float2 matVecMul(const float2x2& a, const float2& b)
	{
		return float2(dot(a.r[0], b), dot(a.r[1], b));
	}

	// float2x2 x float2x2
	float2x2 float2x2::operator*(const float2x2& other) const { return matMul(*this, other); }
	// float2x2 x scalar
	float2x2 float2x2::operator*(const f32& other) const { return float2x2(r[0]*other, r[1]*other); }
	// float2x2 x float2
	float2 float2x2::operator*(const float2& other) const { return matVecMul(*this, other); }

	//-----------------------
	// Array operator
	//-----------------------
	float2& float2x2::get_opIndex(s32 index)
	{
		if (index < 0 || index >= 2)
		{
			// TODO: WARNING
			index = 0;
		}
		return r[index];
	}

	//-----------------------
	// AngelScript functions
	//-----------------------
	static void float2x2DefaultConstructor(float2x2* self) { new(self) float2x2(); }
	static void float2x2CopyConstructor(const float2x2& other, float2x2* self) { new(self) float2x2(other); }
	static void float2x2ConvConstructor(const float2& m0, const float2& m1, float2x2* self) { new(self) float2x2(m0, m1); }
	static void float2x2InitConstructor(f32 m0, f32 m1, f32 m2, f32 m3, float2* self) { new(self) float2x2(m0, m1, m2, m3); }
	static void float2x2ListConstructor(f32* list, float2* self) { new(self) float2x2(list[0], list[1], list[2], list[3]); }

	//-----------------------
	// Intrinsics
	//-----------------------
	std::string toStringF2x2(const float2x2& m)
	{
		char tmp[1024];
		sprintf(tmp, "[ (%g, %g), (%g, %g) ]", m.r[0].x, m.r[0].y, m.r[1].x, m.r[1].y);
		return std::string(tmp);
	}

	//-------------------------------------
	// Registration
	//-------------------------------------
	static s32 s_float2x2ObjId = -1;
	s32 getFloat2x2ObjectId()
	{
		return s_float2x2ObjId;
	}
	
	void registerScriptMath_float2x2(asIScriptEngine *engine)
	{
		s32 r;
		r = engine->RegisterObjectType("float2x2", sizeof(float2x2), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<float2x2>() | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
		s_float2x2ObjId = r;

		// Register the constructors
		r = engine->RegisterObjectBehaviour("float2x2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(float2x2DefaultConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float2x2", asBEHAVE_CONSTRUCT, "void f(const float2x2 &in)", asFUNCTION(float2x2CopyConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float2x2", asBEHAVE_CONSTRUCT, "void f(const float2 &in, const float2 &in)", asFUNCTION(float2x2ConvConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float2x2", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(float2x2InitConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float2x2", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in) {float, float, float, float}", asFUNCTION(float2x2ListConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);

		// Register the operator overloads
		r = engine->RegisterObjectMethod("float2x2", "float2x2 &opMulAssign(const float2x2 &in)", asMETHODPR(float2x2, operator*=, (const float2x2 &), float2x2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2x2", "float2x2 &opMulAssign(const float &in)", asMETHODPR(float2x2, operator*=, (const float &), float2x2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2x2", "bool opEquals(const float2x2 &in) const", asMETHODPR(float2x2, operator==, (const float2x2 &) const, bool), asCALL_THISCALL); assert(r >= 0);

		// Indexing operator.
		r = engine->RegisterObjectMethod("float2x2", "float2& opIndex(int)", asMETHOD(float2x2, get_opIndex), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterObjectMethod("float2x2", "float2x2 opMul(const float2x2 &in) const", asMETHODPR(float2x2, operator*, (const float2x2 &) const, float2x2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2x2", "float2x2 opMul(const float &in) const", asMETHODPR(float2x2, operator*, (const float &) const, float2x2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2x2", "float2 opMul(const float2 &in) const", asMETHODPR(float2x2, operator*, (const float2 &) const, float2), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterGlobalFunction("string toString(float2x2 m)", asFUNCTION(toStringF2x2), asCALL_CDECL); assert(r >= 0);
	}
}  // TFE_ForceScript
