#pragma once
//////////////////////////////////////////////////////////////////////
// Matrix types for scripts.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/system.h>
#include <string>
#include "float4.h"
#include <angelscript.h>

namespace TFE_ForceScript
{
	struct float4x4
	{
		float4x4();
		float4x4(const float4x4& other);
		float4x4(float4 r0, float4 r1, float4 r2, float4 r3);
		float4x4(f32 m00, f32 m01, f32 m02, f32 m03, f32 m10, f32 m11, f32 m12, f32 m13, f32 m20, f32 m21, f32 m22, f32 m23, f32 m30, f32 m31, f32 m32, f32 m33);

		// Assignment operator
		float4x4 &operator=(const float4x4& other);

		// Compound assigment operators
		float4x4 &operator*=(const float4x4& other);
		float4x4 &operator*=(const f32& other);

		// Indexing operator
		float4& get_opIndex(s32 index);

		// Comparison
		bool operator==(const float4x4& other) const;
		bool operator!=(const float4x4& other) const;

		// Math operators
		float4x4 operator*(const float4x4& other) const;
		float4x4 operator*(const f32& other) const;
		float4 operator*(const float4& other) const;

		union
		{
			float4 r[4];
			f32 m[16];
		};
	};

	void registerScriptMath_float4x4(asIScriptEngine *engine);
	s32 getFloat4x4ObjectId();
	std::string toStringF4x4(const float4x4& v);
}  // TFE_ForceScript
