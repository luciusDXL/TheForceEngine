#pragma once
//////////////////////////////////////////////////////////////////////
// Vector types for scripts.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/system.h>
#include <string>
#include "float2.h"
#include <angelscript.h>

namespace TFE_ForceScript
{
	struct float3
	{
		float3();
		float3(const float3& other);
		float3(f32 x, f32 y, f32 z);

		// Assignment operator
		float3 &operator=(const float3& other);

		// Compound assigment operators
		float3 &operator+=(const float3& other);
		float3 &operator-=(const float3& other);
		float3 &operator*=(const float3& other);
		float3 &operator/=(const float3& other);

		float3 &operator+=(const f32& other);
		float3 &operator-=(const f32& other);
		float3 &operator*=(const f32& other);
		float3 &operator/=(const f32& other);

		// Swizzle operators - float2
		float2 get_xx() const;
		float2 get_xy() const;
		float2 get_xz() const;
		float2 get_yx() const;
		float2 get_yy() const;
		float2 get_yz() const;
		float2 get_zx() const;
		float2 get_zy() const;
		float2 get_zz() const;

		// Swizzle operators - float3
		// x
		float3 get_xxx() const;
		float3 get_xxy() const;
		float3 get_xxz() const;
		float3 get_xyx() const;
		float3 get_xyy() const;
		float3 get_xyz() const;
		float3 get_xzx() const;
		float3 get_xzy() const;
		float3 get_xzz() const;
		void   set_xyz(const float3& in);
		void   set_xzy(const float3& in);
		// y
		float3 get_yxx() const;
		float3 get_yxy() const;
		float3 get_yxz() const;
		float3 get_yyx() const;
		float3 get_yyy() const;
		float3 get_yyz() const;
		float3 get_yzx() const;
		float3 get_yzy() const;
		float3 get_yzz() const;
		void   set_yxz(const float3& in);
		void   set_yzx(const float3& in);
		// z
		float3 get_zxx() const;
		float3 get_zxy() const;
		float3 get_zxz() const;
		float3 get_zyx() const;
		float3 get_zyy() const;
		float3 get_zyz() const;
		float3 get_zzx() const;
		float3 get_zzy() const;
		float3 get_zzz() const;
		void   set_zxy(const float3& in);
		void   set_zyx(const float3& in);

		// Indexing operator
		f32&   get_opIndex(s32 index);

		// Comparison
		bool operator==(const float3& other) const;
		bool operator!=(const float3& other) const;

		// Math operators
		float3 operator+(const float3& other) const;
		float3 operator-(const float3& other) const;
		float3 operator*(const float3& other) const;
		float3 operator/(const float3& other) const;

		float3 operator+(const f32& other) const;
		float3 operator-(const f32& other) const;
		float3 operator*(const f32& other) const;
		float3 operator/(const f32& other) const;

		union
		{
			struct { f32 x, y, z; };
			f32 m[3];
		};
	};

	void registerScriptMath_float3(asIScriptEngine *engine);
	s32 getFloat3ObjectId();
	std::string toStringF3(const float3& v);

	inline f32 dot(float3 a, float3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
}  // TFE_ForceScript
