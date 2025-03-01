#pragma once
//////////////////////////////////////////////////////////////////////
// Vector types for scripts.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/system.h>
#include <string>
#include <angelscript.h>

namespace TFE_ForceScript
{
	struct float2
	{
		float2();
		float2(const float2& other);
		float2(f32 x, f32 y);

		// Assignment operator
		float2 &operator=(const float2& other);

		// Compound assigment operators
		float2 &operator+=(const float2& other);
		float2 &operator-=(const float2& other);
		float2 &operator*=(const float2& other);
		float2 &operator/=(const float2& other);

		float2 &operator+=(const f32& other);
		float2 &operator-=(const f32& other);
		float2 &operator*=(const f32& other);
		float2 &operator/=(const f32& other);

		// Swizzle operators
		float2 get_xy() const;
		float2 get_xx() const;
		float2 get_yx() const;
		float2 get_yy() const;
		void   set_xy(const float2& in);
		void   set_yx(const float2& in);
		// Indexing operator
		f32&   get_opIndex(s32 index);

		// Comparison
		bool operator==(const float2& other) const;
		bool operator!=(const float2& other) const;

		// Math operators
		float2 operator+(const float2& other) const;
		float2 operator-(const float2& other) const;
		float2 operator*(const float2& other) const;
		float2 operator/(const float2& other) const;

		float2 operator+(const f32& other) const;
		float2 operator-(const f32& other) const;
		float2 operator*(const f32& other) const;
		float2 operator/(const f32& other) const;

		union
		{
			struct { f32 x, y; };
			f32 m[2];
		};
	};

	void registerScriptMath_float2(asIScriptEngine *engine);
	s32 getFloat2ObjectId();
	std::string toString(const float2& v);

	inline f32 dot(float2 a, float2 b) { return a.x*b.x + a.y*b.y; }
}  // TFE_ForceScript
