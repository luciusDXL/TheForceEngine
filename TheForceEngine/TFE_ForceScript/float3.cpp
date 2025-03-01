#include "float3.h"
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
	float3::float3()
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}

	float3::float3(const float3& other)
	{
		x = other.x;
		y = other.y;
		z = other.z;
	}

	float3::float3(f32 _x, f32 _y, f32 _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}

	bool float3::operator==(const float3& o) const
	{
		return (x == o.x) && (y == o.y) && (z == o.z);
	}
	bool float3::operator!=(const float3& o) const
	{
		return !(*this == o);
	}

	float3& float3::operator=(const float3& other)
	{
		x = other.x;
		y = other.y;
		z = other.z;
		return *this;
	}
	// Compound float3
	float3& float3::operator+=(const float3& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}
	float3& float3::operator-=(const float3& other)
	{
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}
	float3& float3::operator*=(const float3& other)
	{
		*this = *this * other;
		return *this;
	}
	float3& float3::operator/=(const float3& other)
	{
		*this = *this / other;
		return *this;
	}
	// Compound Scalar
	float3& float3::operator+=(const f32& other)
	{
		x += other;
		y += other;
		z += other;
		return *this;
	}
	float3& float3::operator-=(const f32& other)
	{
		x -= other;
		y -= other;
		z -= other;
		return *this;
	}
	float3& float3::operator*=(const f32& other)
	{
		*this = *this * other;
		return *this;
	}
	float3& float3::operator/=(const f32& other)
	{
		*this = *this / other;
		return *this;
	}

	// float3 x float3
	float3 float3::operator+(const float3& other) const { return float3(x + other.x, y + other.y, z + other.z); }
	float3 float3::operator-(const float3& other) const { return float3(x - other.x, y - other.y, z - other.z); }
	float3 float3::operator*(const float3& other) const { return float3(x * other.x, y * other.y, z * other.z); }
	float3 float3::operator/(const float3& other) const { return float3(x / other.x, y / other.y, z / other.z); }
	// float3 x scalar
	float3 float3::operator+(const f32& other) const { return float3(x + other, y + other, z + other); }
	float3 float3::operator-(const f32& other) const { return float3(x - other, y - other, z - other); }
	float3 float3::operator*(const f32& other) const { return float3(x * other, y * other, z * other); }
	float3 float3::operator/(const f32& other) const { return float3(x / other, y / other, z / other); }

	//-----------------------
	// Swizzle operators
	//-----------------------
	// float2
	float2 float3::get_xx() const { return float2(x, x); }
	float2 float3::get_xy() const { return float2(x, y); }
	float2 float3::get_xz() const { return float2(x, z); }
	float2 float3::get_yx() const { return float2(y, x); }
	float2 float3::get_yy() const { return float2(y, y); }
	float2 float3::get_yz() const { return float2(y, z); }
	float2 float3::get_zx() const { return float2(z, x); }
	float2 float3::get_zy() const { return float2(z, y); }
	float2 float3::get_zz() const { return float2(z, z); }
	// float3 - x
	float3 float3::get_xxx() const { return float3(x, x, x); }
	float3 float3::get_xxy() const { return float3(x, x, y); }
	float3 float3::get_xxz() const { return float3(x, x, z); }
	float3 float3::get_xyx() const { return float3(x, y, x); }
	float3 float3::get_xyy() const { return float3(x, y, y); }
	float3 float3::get_xyz() const { return float3(x, y, z); }
	float3 float3::get_xzx() const { return float3(x, z, x); }
	float3 float3::get_xzy() const { return float3(x, z, y); }
	float3 float3::get_xzz() const { return float3(x, z, z); }
	void   float3::set_xyz(const float3& o) { *this = o; }
	void   float3::set_xzy(const float3& o) { x = o.x; z = o.y; y = o.z; }
	// float3 - y
	float3 float3::get_yxx() const { return float3(y, x, x); }
	float3 float3::get_yxy() const { return float3(y, x, y); }
	float3 float3::get_yxz() const { return float3(y, x, z); }
	float3 float3::get_yyx() const { return float3(y, y, x); }
	float3 float3::get_yyy() const { return float3(y, y, y); }
	float3 float3::get_yyz() const { return float3(y, y, z); }
	float3 float3::get_yzx() const { return float3(y, z, x); }
	float3 float3::get_yzy() const { return float3(y, z, y); }
	float3 float3::get_yzz() const { return float3(y, z, z); }
	void   float3::set_yxz(const float3& o) { y = o.x; x = o.y; z = o.z; }
	void   float3::set_yzx(const float3& o) { y = o.x; z = o.y; x = o.z; }
	// float3 - z
	float3 float3::get_zxx() const { return float3(z, x, x); }
	float3 float3::get_zxy() const { return float3(z, x, y); }
	float3 float3::get_zxz() const { return float3(z, x, z); }
	float3 float3::get_zyx() const { return float3(z, y, x); }
	float3 float3::get_zyy() const { return float3(z, y, y); }
	float3 float3::get_zyz() const { return float3(z, y, z); }
	float3 float3::get_zzx() const { return float3(z, z, x); }
	float3 float3::get_zzy() const { return float3(z, z, y); }
	float3 float3::get_zzz() const { return float3(z, z, z); }
	void   float3::set_zxy(const float3& o) { z = o.x; x = o.y; y = o.z; }
	void   float3::set_zyx(const float3& o) { z = o.x; y = o.y; x = o.z; }

	//-----------------------
	// Array operator
	//-----------------------
	f32& float3::get_opIndex(s32 index)
	{
		if (index < 0 || index >= 3)
		{
			// TODO: WARNING
			index = 0;
		}
		return m[index];
	}

	//-----------------------
	// AngelScript functions
	//-----------------------
	static void float3DefaultConstructor(float3* self) { new(self) float3(); }
	static void float3CopyConstructor(const float3& other, float3* self) { new(self) float3(other); }
	static void float3ConvConstructor(f32 x, float3* self) { new(self) float3(x, x, x); }
	static void float3InitConstructor(f32 x, f32 y, f32 z, float3* self) { new(self) float3(x, y, z); }
	static void float3ListConstructor(f32* list, float3* self) { new(self) float3(list[0], list[1], list[2]); }

	//-----------------------
	// Intrinsics
	//-----------------------
	std::string toStringF3(const float3& v)
	{
		char tmp[1024];
		sprintf(tmp, "(%g, %g, %g)", v.x, v.y, v.z);
		return std::string(tmp);
	}

	//-------------------------------------
	// Registration
	//-------------------------------------
	static s32 s_float3ObjId = -1;
	s32 getFloat3ObjectId()
	{
		return s_float3ObjId;
	}

	void registerScriptMath_float3(asIScriptEngine *engine)
	{
		s32 r;
		r = engine->RegisterObjectType("float3", sizeof(float3), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<float3>() | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
		s_float3ObjId = r;

		// Register the object properties
		r = engine->RegisterObjectProperty("float3", "float x", asOFFSET(float3, x)); assert(r >= 0);
		r = engine->RegisterObjectProperty("float3", "float y", asOFFSET(float3, y)); assert(r >= 0);
		r = engine->RegisterObjectProperty("float3", "float z", asOFFSET(float3, z)); assert(r >= 0);

		// Register the constructors
		r = engine->RegisterObjectBehaviour("float3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(float3DefaultConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float3", asBEHAVE_CONSTRUCT, "void f(const float3 &in)", asFUNCTION(float3CopyConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float3", asBEHAVE_CONSTRUCT, "void f(float)", asFUNCTION(float3ConvConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float3", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(float3InitConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float3", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in) {float, float, float}", asFUNCTION(float3ListConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);

		// Register the operator overloads
		r = engine->RegisterObjectMethod("float3", "float3 &opAddAssign(const float3 &in)", asMETHODPR(float3, operator+=, (const float3 &), float3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 &opSubAssign(const float3 &in)", asMETHODPR(float3, operator-=, (const float3 &), float3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 &opMulAssign(const float3 &in)", asMETHODPR(float3, operator*=, (const float3 &), float3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 &opDivAssign(const float3 &in)", asMETHODPR(float3, operator/=, (const float3 &), float3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 &opAddAssign(const float &in)", asMETHODPR(float3, operator+=, (const float &), float3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 &opSubAssign(const float &in)", asMETHODPR(float3, operator-=, (const float &), float3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 &opMulAssign(const float &in)", asMETHODPR(float3, operator*=, (const float &), float3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 &opDivAssign(const float &in)", asMETHODPR(float3, operator/=, (const float &), float3&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "bool opEquals(const float3 &in) const", asMETHODPR(float3, operator==, (const float3 &) const, bool), asCALL_THISCALL); assert(r >= 0);

		// Indexing operator.
		r = engine->RegisterObjectMethod("float3", "float& opIndex(int)", asMETHOD(float3, get_opIndex), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterObjectMethod("float3", "float3 opAdd(const float3 &in) const", asMETHODPR(float3, operator+, (const float3 &) const, float3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 opSub(const float3 &in) const", asMETHODPR(float3, operator-, (const float3 &) const, float3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 opMul(const float3 &in) const", asMETHODPR(float3, operator*, (const float3 &) const, float3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 opDiv(const float3 &in) const", asMETHODPR(float3, operator/, (const float3 &) const, float3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 opAdd(const float &in) const", asMETHODPR(float3, operator+, (const float &) const, float3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 opSub(const float &in) const", asMETHODPR(float3, operator-, (const float &) const, float3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 opMul(const float &in) const", asMETHODPR(float3, operator*, (const float &) const, float3), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 opDiv(const float &in) const", asMETHODPR(float3, operator/, (const float &) const, float3), asCALL_THISCALL); assert(r >= 0);

		// Register the swizzle operators
		r = engine->RegisterObjectMethod("float3", "float2 get_xx() const property", asMETHOD(float3, get_xx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float2 get_xy() const property", asMETHOD(float3, get_xy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float2 get_xz() const property", asMETHOD(float3, get_xz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float2 get_yx() const property", asMETHOD(float3, get_yx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float2 get_yy() const property", asMETHOD(float3, get_yy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float2 get_yz() const property", asMETHOD(float3, get_yz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float2 get_zx() const property", asMETHOD(float3, get_zx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float2 get_zy() const property", asMETHOD(float3, get_zy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float2 get_zz() const property", asMETHOD(float3, get_zz), asCALL_THISCALL); assert(r >= 0);
		// Swizzle operators - float3
		// x
		r = engine->RegisterObjectMethod("float3", "float3 get_xxx() const property", asMETHOD(float3, get_xxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_xxy() const property", asMETHOD(float3, get_xxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_xxz() const property", asMETHOD(float3, get_xxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_xyx() const property", asMETHOD(float3, get_xyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_xyy() const property", asMETHOD(float3, get_xyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_xyz() const property", asMETHOD(float3, get_xyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_xzx() const property", asMETHOD(float3, get_xzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_xzy() const property", asMETHOD(float3, get_xzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_xzz() const property", asMETHOD(float3, get_xzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "void   set_xyz(const float3 &in) property", asMETHOD(float3, set_xyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "void   set_xzy(const float3 &in) property", asMETHOD(float3, set_xzy), asCALL_THISCALL); assert(r >= 0);
		// y
		r = engine->RegisterObjectMethod("float3", "float3 get_yxx() const property", asMETHOD(float3, get_yxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_yxy() const property", asMETHOD(float3, get_yxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_yxz() const property", asMETHOD(float3, get_yxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_yyx() const property", asMETHOD(float3, get_yyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_yyy() const property", asMETHOD(float3, get_yyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_yyz() const property", asMETHOD(float3, get_yyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_yzx() const property", asMETHOD(float3, get_yzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_yzy() const property", asMETHOD(float3, get_yzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_yzz() const property", asMETHOD(float3, get_yzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "void   set_yxz(const float3 &in) property", asMETHOD(float3, set_yxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "void   set_yzx(const float3 &in) property", asMETHOD(float3, set_yzx), asCALL_THISCALL); assert(r >= 0);
		// z
		r = engine->RegisterObjectMethod("float3", "float3 get_zxx() const property", asMETHOD(float3, get_zxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_zxy() const property", asMETHOD(float3, get_zxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_zxz() const property", asMETHOD(float3, get_zxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_zyx() const property", asMETHOD(float3, get_zyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_zyy() const property", asMETHOD(float3, get_zyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_zyz() const property", asMETHOD(float3, get_zyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_zzx() const property", asMETHOD(float3, get_zzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_zzy() const property", asMETHOD(float3, get_zzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "float3 get_zzz() const property", asMETHOD(float3, get_zzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "void   set_zxy(const float3 &in) property", asMETHOD(float3, set_zxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float3", "void   set_zyx(const float3 &in) property", asMETHOD(float3, set_zyx), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterGlobalFunction("string toString(float3 v)", asFUNCTION(toStringF3), asCALL_CDECL); assert(r >= 0);
	}
}  // TFE_ForceScript
