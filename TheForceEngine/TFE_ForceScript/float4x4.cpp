#include "float4x4.h"
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
	float4x4::float4x4()
	{
		m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
		m[4] = 0.0f; m[5] = 1.0f; m[6] = 0.0f; m[7] = 0.0f;
		m[8] = 0.0f; m[9] = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
		m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
	}

	float4x4::float4x4(float4 _r0, float4 _r1, float4 _r2, float4 _r3)
	{
		r[0] = _r0;
		r[1] = _r1;
		r[2] = _r2;
		r[3] = _r3;
	}

	float4x4::float4x4(const float4x4& other)
	{
		r[0] = other.r[0];
		r[1] = other.r[1];
		r[2] = other.r[2];
		r[3] = other.r[3];
	}

	float4x4::float4x4(f32 m00, f32 m01, f32 m02, f32 m03, f32 m10, f32 m11, f32 m12, f32 m13, f32 m20, f32 m21, f32 m22, f32 m23, f32 m30, f32 m31, f32 m32, f32 m33)
	{
		m[0] = m00; m[1] = m01; m[2] = m02; m[3] = m03;
		m[4] = m10; m[5] = m11; m[6] = m12; m[7] = m13;
		m[8] = m20; m[9] = m21; m[10] = m22; m[11] = m23;
		m[12] = m30; m[13] = m31; m[14] = m32; m[15] = m33;
	}

	bool float4x4::operator==(const float4x4& o) const
	{
		for (s32 i = 0; i < 16; i++)
		{
			if (m[i] != o.m[i]) { return false; }
		}
		return true;
	}
	bool float4x4::operator!=(const float4x4& o) const
	{
		return !(*this == o);
	}

	float4x4& float4x4::operator=(const float4x4& other)
	{
		r[0] = other.r[0];
		r[1] = other.r[1];
		r[2] = other.r[2];
		r[3] = other.r[3];
		return *this;
	}
	// Compound float2
	float4x4& float4x4::operator*=(const float4x4& other)
	{
		*this = *this * other;
		return *this;
	}
	float4x4& float4x4::operator*=(const f32& other)
	{
		*this = *this * other;
		return *this;
	}
		
	float4x4 matMul(const float4x4& a, const float4x4& b)
	{
		float4 r0 = a.r[0];
		float4 r1 = a.r[1];
		float4 r2 = a.r[2];
		float4 r3 = a.r[3];
		float4 c0(b.r[0].x, b.r[1].x, b.r[2].x, b.r[3].x);
		float4 c1(b.r[0].y, b.r[1].y, b.r[2].y, b.r[3].y);
		float4 c2(b.r[0].z, b.r[1].z, b.r[2].z, b.r[3].z);
		float4 c3(b.r[0].w, b.r[1].w, b.r[2].w, b.r[3].w);
		
		return float4x4(dot(r0, c0), dot(r0, c1), dot(r0, c2), dot(r0, c3),
			            dot(r1, c0), dot(r1, c1), dot(r1, c2), dot(r1, c3),
						dot(r2, c0), dot(r2, c1), dot(r2, c2), dot(r2, c3),
						dot(r3, c0), dot(r3, c1), dot(r3, c2), dot(r3, c3));
	}

	float4 matVecMul(const float4x4& a, const float4& b)
	{
		return float4(dot(a.r[0], b), dot(a.r[1], b), dot(a.r[2], b), dot(a.r[3], b));
	}

	// float4x4 x float4x4
	float4x4 float4x4::operator*(const float4x4& other) const { return matMul(*this, other); }
	// float4x4 x scalar
	float4x4 float4x4::operator*(const f32& other) const { return float4x4(r[0]*other, r[1]*other, r[2]*other, r[3]*other); }
	// float4x4 x float2
	float4 float4x4::operator*(const float4& other) const { return matVecMul(*this, other); }

	//-----------------------
	// Array operator
	//-----------------------
	float4& float4x4::get_opIndex(s32 index)
	{
		if (index < 0 || index >= 4)
		{
			// TODO: WARNING
			index = 0;
		}
		return r[index];
	}

	//-----------------------
	// AngelScript functions
	//-----------------------
	static void float4x4DefaultConstructor(float4x4* self) { new(self) float4x4(); }
	static void float4x4CopyConstructor(const float4x4& other, float4x4* self) { new(self) float4x4(other); }
	static void float4x4ConvConstructor(const float4& m0, const float4& m1, const float4& m2, const float4& m3, float4x4* self) { new(self) float4x4(m0, m1, m2, m3); }
	static void float4x4InitConstructor(f32 m0, f32 m1, f32 m2, f32 m3, f32 m4, f32 m5, f32 m6, f32 m7, f32 m8, f32 m9, f32 m10, f32 m11, 
		f32 m12, f32 m13, f32 m14, f32 m15, float2* self) { new(self) float4x4(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15); }
	static void float4x4ListConstructor(f32* list, float2* self) { new(self) float4x4(list[0], list[1], list[2], list[3], list[4], list[5], list[6], list[7], list[8],
		list[9], list[10], list[11], list[12], list[13], list[14], list[15]); }

	//-----------------------
	// Intrinsics
	//-----------------------
	std::string toStringF4x4(const float4x4& m)
	{
		char tmp[1024];
		sprintf(tmp, "[ (%g, %g, %g, %g), (%g, %g, %g, %g), (%g, %g, %g, %g), (%g, %g, %g, %g) ]",
			m.r[0].x, m.r[0].y, m.r[0].z, m.r[0].w, m.r[1].x, m.r[1].y, m.r[1].z, m.r[1].w,
			m.r[2].x, m.r[2].y, m.r[2].z, m.r[2].w, m.r[3].x, m.r[3].y, m.r[3].z, m.r[3].w);
		return std::string(tmp);
	}

	//-------------------------------------
	// Registration
	//-------------------------------------
	static s32 s_float4x4ObjId = -1;
	s32 getFloat4x4ObjectId()
	{
		return s_float4x4ObjId;
	}
	
	void registerScriptMath_float4x4(asIScriptEngine *engine)
	{
		s32 r;
		r = engine->RegisterObjectType("float4x4", sizeof(float4x4), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<float4x4>() | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
		s_float4x4ObjId = r;

		// Register the constructors
		r = engine->RegisterObjectBehaviour("float4x4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(float4x4DefaultConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float4x4", asBEHAVE_CONSTRUCT, "void f(const float4x4 &in)", asFUNCTION(float4x4CopyConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float4x4", asBEHAVE_CONSTRUCT, "void f(const float4 &in, const float4 &in, const float4 &in, const float4 &in)", asFUNCTION(float4x4ConvConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float4x4", asBEHAVE_CONSTRUCT, "void f(float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float)", asFUNCTION(float4x4InitConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float4x4", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in) {float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float}", asFUNCTION(float4x4ListConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);

		// Register the operator overloads
		r = engine->RegisterObjectMethod("float4x4", "float4x4 &opMulAssign(const float4x4 &in)", asMETHODPR(float4x4, operator*=, (const float4x4 &), float4x4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4x4", "float4x4 &opMulAssign(const float &in)", asMETHODPR(float4x4, operator*=, (const float &), float4x4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4x4", "bool opEquals(const float4x4 &in) const", asMETHODPR(float4x4, operator==, (const float4x4 &) const, bool), asCALL_THISCALL); assert(r >= 0);

		// Indexing operator.
		r = engine->RegisterObjectMethod("float4x4", "float4& opIndex(int)", asMETHOD(float4x4, get_opIndex), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterObjectMethod("float4x4", "float4x4 opMul(const float4x4 &in) const", asMETHODPR(float4x4, operator*, (const float4x4 &) const, float4x4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4x4", "float4x4 opMul(const float &in) const", asMETHODPR(float4x4, operator*, (const float &) const, float4x4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4x4", "float4 opMul(const float4 &in) const", asMETHODPR(float4x4, operator*, (const float4 &) const, float4), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterGlobalFunction("string toString(float4x4 m)", asFUNCTION(toStringF4x4), asCALL_CDECL); assert(r >= 0);
	}
}  // TFE_ForceScript
