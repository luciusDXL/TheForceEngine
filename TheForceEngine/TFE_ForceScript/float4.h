#pragma once
//////////////////////////////////////////////////////////////////////
// Vector types for scripts.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/system.h>
#include <string>
#include "float2.h"
#include "float3.h"
#include <angelscript.h>

namespace TFE_ForceScript
{
	struct float4
	{
		float4();
		float4(const float4& other);
		float4(f32 x, f32 y, f32 z, f32 w);

		// Assignment operator
		float4 &operator=(const float4& other);

		// Compound assigment operators
		float4 &operator+=(const float4& other);
		float4 &operator-=(const float4& other);
		float4 &operator*=(const float4& other);
		float4 &operator/=(const float4& other);

		float4 &operator+=(const f32& other);
		float4 &operator-=(const f32& other);
		float4 &operator*=(const f32& other);
		float4 &operator/=(const f32& other);

		// Swizzle operators - float2
		// x
		float2 get_xx() const;
		float2 get_xy() const;
		float2 get_xz() const;
		float2 get_xw() const;
		// y
		float2 get_yx() const;
		float2 get_yy() const;
		float2 get_yz() const;
		float2 get_yw() const;
		// z
		float2 get_zx() const;
		float2 get_zy() const;
		float2 get_zz() const;
		float2 get_zw() const;
		// w
		float2 get_wx() const;
		float2 get_wy() const;
		float2 get_wz() const;
		float2 get_ww() const;

		// Swizzle operators - float3
		// x
		float3 get_xxx() const;
		float3 get_xxy() const;
		float3 get_xxz() const;
		float3 get_xxw() const;
		float3 get_xyx() const;
		float3 get_xyy() const;
		float3 get_xyz() const;
		float3 get_xyw() const;
		float3 get_xzx() const;
		float3 get_xzy() const;
		float3 get_xzz() const;
		float3 get_xzw() const;
		float3 get_xwx() const;
		float3 get_xwy() const;
		float3 get_xwz() const;
		float3 get_xww() const;
		// y
		float3 get_yxx() const;
		float3 get_yxy() const;
		float3 get_yxz() const;
		float3 get_yxw() const;
		float3 get_yyx() const;
		float3 get_yyy() const;
		float3 get_yyz() const;
		float3 get_yyw() const;
		float3 get_yzx() const;
		float3 get_yzy() const;
		float3 get_yzz() const;
		float3 get_yzw() const;
		float3 get_ywx() const;
		float3 get_ywy() const;
		float3 get_ywz() const;
		float3 get_yww() const;
		// z
		float3 get_zxx() const;
		float3 get_zxy() const;
		float3 get_zxz() const;
		float3 get_zxw() const;
		float3 get_zyx() const;
		float3 get_zyy() const;
		float3 get_zyz() const;
		float3 get_zyw() const;
		float3 get_zzx() const;
		float3 get_zzy() const;
		float3 get_zzz() const;
		float3 get_zzw() const;
		float3 get_zwx() const;
		float3 get_zwy() const;
		float3 get_zwz() const;
		float3 get_zww() const;
		// w
		float3 get_wxx() const;
		float3 get_wxy() const;
		float3 get_wxz() const;
		float3 get_wxw() const;
		float3 get_wyx() const;
		float3 get_wyy() const;
		float3 get_wyz() const;
		float3 get_wyw() const;
		float3 get_wzx() const;
		float3 get_wzy() const;
		float3 get_wzz() const;
		float3 get_wzw() const;
		float3 get_wwx() const;
		float3 get_wwy() const;
		float3 get_wwz() const;
		float3 get_www() const;
		
		// Swizzle operators - float4
		// x
		float4 get_xxxx() const;
		float4 get_xxxy() const;
		float4 get_xxxz() const;
		float4 get_xxxw() const;
		float4 get_xxyx() const;
		float4 get_xxyy() const;
		float4 get_xxyz() const;
		float4 get_xxyw() const;
		float4 get_xxzx() const;
		float4 get_xxzy() const;
		float4 get_xxzz() const;
		float4 get_xxzw() const;
		float4 get_xxwx() const;
		float4 get_xxwy() const;
		float4 get_xxwz() const;
		float4 get_xxww() const;
		float4 get_xyxx() const;
		float4 get_xyxy() const;
		float4 get_xyxz() const;
		float4 get_xyxw() const;
		float4 get_xyyx() const;
		float4 get_xyyy() const;
		float4 get_xyyz() const;
		float4 get_xyyw() const;
		float4 get_xyzx() const;
		float4 get_xyzy() const;
		float4 get_xyzz() const;
		float4 get_xyzw() const;
		float4 get_xywx() const;
		float4 get_xywy() const;
		float4 get_xywz() const;
		float4 get_xyww() const;
		float4 get_xzxx() const;
		float4 get_xzxy() const;
		float4 get_xzxz() const;
		float4 get_xzxw() const;
		float4 get_xzyx() const;
		float4 get_xzyy() const;
		float4 get_xzyz() const;
		float4 get_xzyw() const;
		float4 get_xzzx() const;
		float4 get_xzzy() const;
		float4 get_xzzz() const;
		float4 get_xzzw() const;
		float4 get_xzwx() const;
		float4 get_xzwy() const;
		float4 get_xzwz() const;
		float4 get_xzww() const;
		float4 get_xwxx() const;
		float4 get_xwxy() const;
		float4 get_xwxz() const;
		float4 get_xwxw() const;
		float4 get_xwyx() const;
		float4 get_xwyy() const;
		float4 get_xwyz() const;
		float4 get_xwyw() const;
		float4 get_xwzx() const;
		float4 get_xwzy() const;
		float4 get_xwzz() const;
		float4 get_xwzw() const;
		float4 get_xwwx() const;
		float4 get_xwwy() const;
		float4 get_xwwz() const;
		float4 get_xwww() const;
		void   set_xyzw(const float4& in);
		void   set_xywz(const float4& in);
		void   set_xzyw(const float4& in);
		void   set_xzwy(const float4& in);
		void   set_xwyz(const float4& in);
		void   set_xwzy(const float4& in);
		// y
		float4 get_yxxx() const;
		float4 get_yxxy() const;
		float4 get_yxxz() const;
		float4 get_yxxw() const;
		float4 get_yxyx() const;
		float4 get_yxyy() const;
		float4 get_yxyz() const;
		float4 get_yxyw() const;
		float4 get_yxzx() const;
		float4 get_yxzy() const;
		float4 get_yxzz() const;
		float4 get_yxzw() const;
		float4 get_yxwx() const;
		float4 get_yxwy() const;
		float4 get_yxwz() const;
		float4 get_yxww() const;
		float4 get_yyxx() const;
		float4 get_yyxy() const;
		float4 get_yyxz() const;
		float4 get_yyxw() const;
		float4 get_yyyx() const;
		float4 get_yyyy() const;
		float4 get_yyyz() const;
		float4 get_yyyw() const;
		float4 get_yyzx() const;
		float4 get_yyzy() const;
		float4 get_yyzz() const;
		float4 get_yyzw() const;
		float4 get_yywx() const;
		float4 get_yywy() const;
		float4 get_yywz() const;
		float4 get_yyww() const;
		float4 get_yzxx() const;
		float4 get_yzxy() const;
		float4 get_yzxz() const;
		float4 get_yzxw() const;
		float4 get_yzyx() const;
		float4 get_yzyy() const;
		float4 get_yzyz() const;
		float4 get_yzyw() const;
		float4 get_yzzx() const;
		float4 get_yzzy() const;
		float4 get_yzzz() const;
		float4 get_yzzw() const;
		float4 get_yzwx() const;
		float4 get_yzwy() const;
		float4 get_yzwz() const;
		float4 get_yzww() const;
		float4 get_ywxx() const;
		float4 get_ywxy() const;
		float4 get_ywxz() const;
		float4 get_ywxw() const;
		float4 get_ywyx() const;
		float4 get_ywyy() const;
		float4 get_ywyz() const;
		float4 get_ywyw() const;
		float4 get_ywzx() const;
		float4 get_ywzy() const;
		float4 get_ywzz() const;
		float4 get_ywzw() const;
		float4 get_ywwx() const;
		float4 get_ywwy() const;
		float4 get_ywwz() const;
		float4 get_ywww() const;
		void   set_yxzw(const float4& in);
		void   set_yxwz(const float4& in);
		void   set_yzxw(const float4& in);
		void   set_yzwx(const float4& in);
		void   set_ywxz(const float4& in);
		void   set_ywzx(const float4& in);
		// z
		float4 get_zxxx() const;
		float4 get_zxxy() const;
		float4 get_zxxz() const;
		float4 get_zxxw() const;
		float4 get_zxyx() const;
		float4 get_zxyy() const;
		float4 get_zxyz() const;
		float4 get_zxyw() const;
		float4 get_zxzx() const;
		float4 get_zxzy() const;
		float4 get_zxzz() const;
		float4 get_zxzw() const;
		float4 get_zxwx() const;
		float4 get_zxwy() const;
		float4 get_zxwz() const;
		float4 get_zxww() const;
		float4 get_zyxx() const;
		float4 get_zyxy() const;
		float4 get_zyxz() const;
		float4 get_zyxw() const;
		float4 get_zyyx() const;
		float4 get_zyyy() const;
		float4 get_zyyz() const;
		float4 get_zyyw() const;
		float4 get_zyzx() const;
		float4 get_zyzy() const;
		float4 get_zyzz() const;
		float4 get_zyzw() const;
		float4 get_zywx() const;
		float4 get_zywy() const;
		float4 get_zywz() const;
		float4 get_zyww() const;
		float4 get_zzxx() const;
		float4 get_zzxy() const;
		float4 get_zzxz() const;
		float4 get_zzxw() const;
		float4 get_zzyx() const;
		float4 get_zzyy() const;
		float4 get_zzyz() const;
		float4 get_zzyw() const;
		float4 get_zzzx() const;
		float4 get_zzzy() const;
		float4 get_zzzz() const;
		float4 get_zzzw() const;
		float4 get_zzwx() const;
		float4 get_zzwy() const;
		float4 get_zzwz() const;
		float4 get_zzww() const;
		float4 get_zwxx() const;
		float4 get_zwxy() const;
		float4 get_zwxz() const;
		float4 get_zwxw() const;
		float4 get_zwyx() const;
		float4 get_zwyy() const;
		float4 get_zwyz() const;
		float4 get_zwyw() const;
		float4 get_zwzx() const;
		float4 get_zwzy() const;
		float4 get_zwzz() const;
		float4 get_zwzw() const;
		float4 get_zwwx() const;
		float4 get_zwwy() const;
		float4 get_zwwz() const;
		float4 get_zwww() const;
		void   set_zyxw(const float4& in);
		void   set_zywx(const float4& in);
		void   set_zxyw(const float4& in);
		void   set_zxwy(const float4& in);
		void   set_zwyx(const float4& in);
		void   set_zwxy(const float4& in);
		// w
		float4 get_wxxx() const;
		float4 get_wxxy() const;
		float4 get_wxxz() const;
		float4 get_wxxw() const;
		float4 get_wxyx() const;
		float4 get_wxyy() const;
		float4 get_wxyz() const;
		float4 get_wxyw() const;
		float4 get_wxzx() const;
		float4 get_wxzy() const;
		float4 get_wxzz() const;
		float4 get_wxzw() const;
		float4 get_wxwx() const;
		float4 get_wxwy() const;
		float4 get_wxwz() const;
		float4 get_wxww() const;
		float4 get_wyxx() const;
		float4 get_wyxy() const;
		float4 get_wyxz() const;
		float4 get_wyxw() const;
		float4 get_wyyx() const;
		float4 get_wyyy() const;
		float4 get_wyyz() const;
		float4 get_wyyw() const;
		float4 get_wyzx() const;
		float4 get_wyzy() const;
		float4 get_wyzz() const;
		float4 get_wyzw() const;
		float4 get_wywx() const;
		float4 get_wywy() const;
		float4 get_wywz() const;
		float4 get_wyww() const;
		float4 get_wzxx() const;
		float4 get_wzxy() const;
		float4 get_wzxz() const;
		float4 get_wzxw() const;
		float4 get_wzyx() const;
		float4 get_wzyy() const;
		float4 get_wzyz() const;
		float4 get_wzyw() const;
		float4 get_wzzx() const;
		float4 get_wzzy() const;
		float4 get_wzzz() const;
		float4 get_wzzw() const;
		float4 get_wzwx() const;
		float4 get_wzwy() const;
		float4 get_wzwz() const;
		float4 get_wzww() const;
		float4 get_wwxx() const;
		float4 get_wwxy() const;
		float4 get_wwxz() const;
		float4 get_wwxw() const;
		float4 get_wwyx() const;
		float4 get_wwyy() const;
		float4 get_wwyz() const;
		float4 get_wwyw() const;
		float4 get_wwzx() const;
		float4 get_wwzy() const;
		float4 get_wwzz() const;
		float4 get_wwzw() const;
		float4 get_wwwx() const;
		float4 get_wwwy() const;
		float4 get_wwwz() const;
		float4 get_wwww() const;
		void   set_wyzx(const float4& in);
		void   set_wyxz(const float4& in);
		void   set_wzyx(const float4& in);
		void   set_wzxy(const float4& in);
		void   set_wxyz(const float4& in);
		void   set_wxzy(const float4& in);

		// Indexing operator
		f32&   get_opIndex(s32 index);

		// Comparison
		bool operator==(const float4& other) const;
		bool operator!=(const float4& other) const;

		// Math operators
		float4 operator+(const float4& other) const;
		float4 operator-(const float4& other) const;
		float4 operator*(const float4& other) const;
		float4 operator/(const float4& other) const;

		float4 operator+(const f32& other) const;
		float4 operator-(const f32& other) const;
		float4 operator*(const f32& other) const;
		float4 operator/(const f32& other) const;

		union
		{
			struct { f32 x, y, z, w; };
			f32 m[4];
		};
	};

	void registerScriptMath_float4(asIScriptEngine *engine);
	s32 getFloat4ObjectId();
	std::string toStringF4(const float4& v);

	inline f32 dot(float4 a, float4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
}  // TFE_ForceScript
