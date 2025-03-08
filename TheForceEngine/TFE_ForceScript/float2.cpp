#include "float2.h"
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
	float2::float2()
	{
		x = 0.0f;
		y = 0.0f;
	}

	float2::float2(const float2& other)
	{
		x = other.x;
		y = other.y;
	}

	float2::float2(f32 _x, f32 _y)
	{
		x = _x;
		y = _y;
	}

	bool float2::operator==(const float2& o) const
	{
		return (x == o.x) && (y == o.y);
	}
	bool float2::operator!=(const float2& o) const
	{
		return !(*this == o);
	}

	float2& float2::operator=(const float2& other)
	{
		x = other.x;
		y = other.y;
		return *this;
	}
	// Compound float2
	float2& float2::operator+=(const float2& other)
	{
		x += other.x;
		y += other.y;
		return *this;
	}
	float2& float2::operator-=(const float2& other)
	{
		x -= other.x;
		y -= other.y;
		return *this;
	}
	float2& float2::operator*=(const float2& other)
	{
		*this = *this * other;
		return *this;
	}
	float2& float2::operator/=(const float2& other)
	{
		*this = *this / other;
		return *this;
	}
	// Compound Scalar
	float2& float2::operator+=(const f32& other)
	{
		x += other;
		y += other;
		return *this;
	}
	float2& float2::operator-=(const f32& other)
	{
		x -= other;
		y -= other;
		return *this;
	}
	float2& float2::operator*=(const f32& other)
	{
		*this = *this * other;
		return *this;
	}
	float2& float2::operator/=(const f32& other)
	{
		*this = *this / other;
		return *this;
	}

	// float2 x float2
	float2 float2::operator+(const float2& other) const { return float2(x + other.x, y + other.y); }
	float2 float2::operator-(const float2& other) const { return float2(x - other.x, y - other.y); }
	float2 float2::operator*(const float2& other) const { return float2(x * other.x, y * other.y); }
	float2 float2::operator/(const float2& other) const { return float2(x / other.x, y / other.y); }
	// float2 x scalar
	float2 float2::operator+(const f32& other) const { return float2(x + other, y + other); }
	float2 float2::operator-(const f32& other) const { return float2(x - other, y - other); }
	float2 float2::operator*(const f32& other) const { return float2(x * other, y * other); }
	float2 float2::operator/(const f32& other) const { return float2(x / other, y / other); }

	//-----------------------
	// Swizzle operators
	//-----------------------
	float2 float2::get_xy() const { return *this; }
	float2 float2::get_yx() const { return float2(y, x); }
	float2 float2::get_xx() const { return float2(x, x); }
	float2 float2::get_yy() const { return float2(y, y); }
	void float2::set_xy(const float2& o) { *this = o; }
	void float2::set_yx(const float2& o) { y = o.x; x = o.y; }

	//-----------------------
	// Array operator
	//-----------------------
	f32& float2::get_opIndex(s32 index)
	{
		if (index < 0 || index >= 2)
		{
			// TODO: WARNING
			index = 0;
		}
		return m[index];
	}

	//-----------------------
	// AngelScript functions
	//-----------------------
	static void float2DefaultConstructor(float2* self) { new(self) float2(); }
	static void float2CopyConstructor(const float2& other, float2* self) { new(self) float2(other); }
	static void float2ConvConstructor(f32 x, float2* self) { new(self) float2(x, x); }
	static void float2InitConstructor(f32 x, f32 y, float2* self) { new(self) float2(x, y); }
	static void float2ListConstructor(f32* list, float2* self) { new(self) float2(list[0], list[1]); }

	//-----------------------
	// Intrinsics
	//-----------------------
	std::string toString(const float2& v)
	{
		char tmp[1024];
		sprintf(tmp, "(%g, %g)", v.x, v.y);
		return std::string(tmp);
	}

	//-------------------------------------
	// Registration
	//-------------------------------------
	static s32 s_float2ObjId = -1;
	s32 getFloat2ObjectId()
	{
		return s_float2ObjId;
	}

	void registerScriptMath_float2(asIScriptEngine *engine)
	{
		s32 r;
		r = engine->RegisterObjectType("float2", sizeof(float2), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<float2>() | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
		s_float2ObjId = r;

		// Register the object properties
		r = engine->RegisterObjectProperty("float2", "float x", asOFFSET(float2, x)); assert(r >= 0);
		r = engine->RegisterObjectProperty("float2", "float y", asOFFSET(float2, y)); assert(r >= 0);

		// Register the constructors
		r = engine->RegisterObjectBehaviour("float2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(float2DefaultConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float2", asBEHAVE_CONSTRUCT, "void f(const float2 &in)", asFUNCTION(float2CopyConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float2", asBEHAVE_CONSTRUCT, "void f(float)", asFUNCTION(float2ConvConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float2", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(float2InitConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float2", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in) {float, float}", asFUNCTION(float2ListConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);

		// Register the operator overloads
		r = engine->RegisterObjectMethod("float2", "float2 &opAddAssign(const float2 &in)", asMETHODPR(float2, operator+=, (const float2 &), float2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 &opSubAssign(const float2 &in)", asMETHODPR(float2, operator-=, (const float2 &), float2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 &opMulAssign(const float2 &in)", asMETHODPR(float2, operator*=, (const float2 &), float2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 &opDivAssign(const float2 &in)", asMETHODPR(float2, operator/=, (const float2 &), float2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 &opAddAssign(const float &in)", asMETHODPR(float2, operator+=, (const float &), float2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 &opSubAssign(const float &in)", asMETHODPR(float2, operator-=, (const float &), float2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 &opMulAssign(const float &in)", asMETHODPR(float2, operator*=, (const float &), float2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 &opDivAssign(const float &in)", asMETHODPR(float2, operator/=, (const float &), float2&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "bool opEquals(const float2 &in) const", asMETHODPR(float2, operator==, (const float2 &) const, bool), asCALL_THISCALL); assert(r >= 0);

		// Indexing operator.
		r = engine->RegisterObjectMethod("float2", "float& opIndex(int)", asMETHOD(float2, get_opIndex), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterObjectMethod("float2", "float2 opAdd(const float2 &in) const", asMETHODPR(float2, operator+, (const float2 &) const, float2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 opSub(const float2 &in) const", asMETHODPR(float2, operator-, (const float2 &) const, float2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 opMul(const float2 &in) const", asMETHODPR(float2, operator*, (const float2 &) const, float2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 opDiv(const float2 &in) const", asMETHODPR(float2, operator/, (const float2 &) const, float2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 opAdd(const float &in) const", asMETHODPR(float2, operator+, (const float &) const, float2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 opSub(const float &in) const", asMETHODPR(float2, operator-, (const float &) const, float2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 opMul(const float &in) const", asMETHODPR(float2, operator*, (const float &) const, float2), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 opDiv(const float &in) const", asMETHODPR(float2, operator/, (const float &) const, float2), asCALL_THISCALL); assert(r >= 0);

		// Register the swizzle operators
		r = engine->RegisterObjectMethod("float2", "float2 get_xy() const property", asMETHOD(float2, get_xy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 get_yx() const property", asMETHOD(float2, get_yx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 get_xx() const property", asMETHOD(float2, get_xx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "float2 get_yy() const property", asMETHOD(float2, get_yy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "void set_xy(const float2 &in) property", asMETHOD(float2, set_xy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float2", "void set_yx(const float2 &in) property", asMETHOD(float2, set_yx), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterGlobalFunction("string toString(float2 v)", asFUNCTION(toString), asCALL_CDECL); assert(r >= 0);
	}
}  // TFE_ForceScript
