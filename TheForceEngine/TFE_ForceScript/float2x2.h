#pragma once
//////////////////////////////////////////////////////////////////////
// Matrix types for scripts.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/system.h>
#include <string>
#include "float2.h"
#include <angelscript.h>

namespace TFE_ForceScript
{
	struct float2x2
	{
		float2x2();
		float2x2(const float2x2& other);
		float2x2(float2 r0, float2 r1);
		float2x2(f32 m00, f32 m01, f32 m10, f32 m11);

		// Assignment operator
		float2x2 &operator=(const float2x2& other);

		// Compound assigment operators
		float2x2 &operator*=(const float2x2& other);
		float2x2 &operator*=(const f32& other);

		// Indexing operator
		float2& get_opIndex(s32 index);

		// Comparison
		bool operator==(const float2x2& other) const;
		bool operator!=(const float2x2& other) const;

		// Math operators
		float2x2 operator*(const float2x2& other) const;
		float2x2 operator*(const f32& other) const;
		float2 operator*(const float2& other) const;

		union
		{
			float2 r[2];
			f32 m[4];
		};
	};

	void registerScriptMath_float2x2(asIScriptEngine *engine);
	s32 getFloat2x2ObjectId();
	std::string toStringF2x2(const float2x2& v);
}  // TFE_ForceScript
