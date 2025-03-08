#pragma once
//////////////////////////////////////////////////////////////////////
// Matrix types for scripts.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/system.h>
#include <string>
#include "float3.h"
#include <angelscript.h>

namespace TFE_ForceScript
{
	struct float3x3
	{
		float3x3();
		float3x3(const float3x3& other);
		float3x3(float3 r0, float3 r1, float3 r2);
		float3x3(f32 m00, f32 m01, f32 m02, f32 m10, f32 m11, f32 m12, f32 m20, f32 m21, f32 m22);

		// Assignment operator
		float3x3 &operator=(const float3x3& other);

		// Compound assigment operators
		float3x3 &operator*=(const float3x3& other);
		float3x3 &operator*=(const f32& other);

		// Indexing operator
		float3& get_opIndex(s32 index);

		// Comparison
		bool operator==(const float3x3& other) const;
		bool operator!=(const float3x3& other) const;

		// Math operators
		float3x3 operator*(const float3x3& other) const;
		float3x3 operator*(const f32& other) const;
		float3 operator*(const float3& other) const;

		union
		{
			float3 r[3];
			f32 m[9];
		};
	};

	void registerScriptMath_float3x3(asIScriptEngine *engine);
	s32 getFloat3x3ObjectId();
	std::string toStringF3x3(const float3x3& v);
}  // TFE_ForceScript
