#include "float3x3.h"
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
	float3x3::float3x3()
	{
		m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f;
		m[3] = 0.0f; m[4] = 1.0f; m[5] = 0.0f;
		m[6] = 0.0f; m[7] = 0.0f; m[8] = 1.0f;
	}

	float3x3::float3x3(float3 _r0, float3 _r1, float3 _r2)
	{
		r[0] = _r0;
		r[1] = _r1;
		r[2] = _r2;
	}

	float3x3::float3x3(const float3x3& other)
	{
		r[0] = other.r[0];
		r[1] = other.r[1];
		r[2] = other.r[2];
	}

	float3x3::float3x3(f32 m00, f32 m01, f32 m02, f32 m10, f32 m11, f32 m12, f32 m20, f32 m21, f32 m22)
	{
		m[0] = m00; m[1] = m01; m[2] = m02;
		m[3] = m10; m[4] = m11; m[5] = m12;
		m[6] = m20; m[7] = m21; m[8] = m22;
	}

	bool float3x3::operator==(const float3x3& o) const
	{
		for (s32 i = 0; i < 9; i++)
		{
			if (m[i] != o.m[i]) { return false; }
		}
		return true;
	}
	bool float3x3::operator!=(const float3x3& o) const
	{
		return !(*this == o);
	}

	float3x3& float3x3::operator=(const float3x3& other)
	{
		r[0] = other.r[0];
		r[1] = other.r[1];
		r[2] = other.r[2];
		return *this;
	}
	// Compound float2
	float3x3& float3x3::operator*=(const float3x3& other)
	{
		*this = *this * other;
		return *this;
	}
	float3x3& float3x3::operator*=(const f32& other)
	{
		*this = *this * other;
		return *this;
	}
		
	float3x3 matMul(const float3x3& a, const float3x3& b)
	{
		float3 r0 = a.r[0];
		float3 r1 = a.r[1];
		float3 r2 = a.r[2];
		float3 c0(b.r[0].x, b.r[1].x, b.r[2].x);
		float3 c1(b.r[0].y, b.r[1].y, b.r[2].y);
		float3 c2(b.r[0].z, b.r[1].z, b.r[2].z);
		
		return float3x3(dot(r0, c0), dot(r0, c1), dot(r0, c2),
			            dot(r1, c0), dot(r1, c1), dot(r1, c2),
						dot(r2, c0), dot(r2, c1), dot(r2, c2));
	}

	float3 matVecMul(const float3x3& a, const float3& b)
	{
		return float3(dot(a.r[0], b), dot(a.r[1], b), dot(a.r[2], b));
	}

	// float3x3 x float3x3
	float3x3 float3x3::operator*(const float3x3& other) const { return matMul(*this, other); }
	// float3x3 x scalar
	float3x3 float3x3::operator*(const f32& other) const { return float3x3(r[0]*other, r[1]*other, r[2]*other); }
	// float3x3 x float2
	float3 float3x3::operator*(const float3& other) const { return matVecMul(*this, other); }

	//-----------------------
	// Array operator
	//-----------------------
	float3& float3x3::get_opIndex(s32 index)
	{
		if (index < 0 || index >= 3)
		{
			// TODO: WARNING
			index = 0;
		}
		return r[index];
	}

	//-----------------------
	// AngelScript functions
	//-----------------------
	static void float3x3DefaultConstructor(float3x3* self) { new(self) float3x3(); }
	static void float3x3CopyConstructor(const float3x3& other, float3x3* self) { new(self) float3x3(other); }
	static void float3x3ConvConstructor(const float3& m0, const float3& m1, const float3& m2, float3x3* self) { new(self) float3x3(m0, m1, m2); }
	static void float3x3InitConstructor(f32 m0, f32 m1, f32 m2, f32 m3, f32 m4, f32 m5, f32 m6, f32 m7, f32 m8, float2* self) { new(self) float3x3(m0, m1, m2, m3, m4, m5, m6, m7, m8); }
	static void float3x3ListConstructor(f32* list, float2* self) { new(self) float3x3(list[0], list[1], list[2], list[3], list[4], list[5], list[6], list[7], list[8]); }

	//-----------------------
	// Intrinsics
	//-----------------------
	std::string toStringF3x3(const float3x3& m)
	{
		char tmp[1024];
		sprintf(tmp, "[ (%g, %g, %g), (%g, %g, %g), (%g, %g, %g) ]", m.r[0].x, m.r[0].y, m.r[0].z, m.r[1].x, m.r[1].y, m.r[1].z, m.r[2].x, m.r[2].y, m.r[2].z);
		return std::string(tmp);
	}

	//-------------------------------------
	// Registration
	//-------------------------------------
	static s32 s_float3x3ObjId = -1;
	s32 getFloat3x3ObjectId()
	{
		return s_float3x3ObjId;
	}
	
	void registerScriptMath_float3x3(asIScriptEngine *engine)
	{
		s32 r;
		r = engine->RegisterObjectType("float3x3", sizeof(float3x3), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<float3x3>() | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
		s_float3x3ObjId = r;

		// Register the constructors
		r = engine->RegisterObjectBehaviour("float3x3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(float3x3DefaultConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float3x3", asBEHAVE_CONSTRUCT, "void f(const float3x3 &in)", asFUNCTION(float3x3CopyConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float3x3", asBEHAVE_CONSTRUCT, "void f(const float3 &in, const float3 &in, const float3 &in)", asFUNCTION(float3x3ConvConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float3x3", asBEHAVE_CONSTRUCT, "void f(float, float, float, float, float, float, float, float, float)", asFUNCTION(float3x3InitConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float3x3", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in) {float, float, float, float, float, float, float, float, float}", asFUNCTION(float3x3ListConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);

		// Register the operator overloads
		r = engine->RegisterObjectMethod("float3x3", "float3x3 &opMulAssign(const float3x3 &in)", asMETHODPR(float3x3, operator*=, (const float3x3 &), float3x3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3x3", "float3x3 &opMulAssign(const float &in)", asMETHODPR(float3x3, operator*=, (const float &), float3x3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3x3", "bool opEquals(const float3x3 &in) const", asMETHODPR(float3x3, operator==, (const float3x3 &) const, bool), asCALL_THISCALL); assert(r >= 0);

		// Indexing operator.
		r = engine->RegisterObjectMethod("float3x3", "float3& opIndex(int)", asMETHOD(float3x3, get_opIndex), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterObjectMethod("float3x3", "float3x3 opMul(const float3x3 &in) const", asMETHODPR(float3x3, operator*, (const float3x3 &) const, float3x3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3x3", "float3x3 opMul(const float &in) const", asMETHODPR(float3x3, operator*, (const float &) const, float3x3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3x3", "float3 opMul(const float3 &in) const", asMETHODPR(float3x3, operator*, (const float3 &) const, float3), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterGlobalFunction("string toString(float3x3 m)", asFUNCTION(toStringF3x3), asCALL_CDECL); assert(r >= 0);
	}
}  // TFE_ForceScript
