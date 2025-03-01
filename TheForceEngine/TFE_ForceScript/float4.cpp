#include "float4.h"
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
	float4::float4()
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
		w = 0.0f;
	}

	float4::float4(const float4& other)
	{
		x = other.x;
		y = other.y;
		z = other.z;
		w = other.w;
	}

	float4::float4(f32 _x, f32 _y, f32 _z, f32 _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	bool float4::operator==(const float4& o) const
	{
		return (x == o.x) && (y == o.y) && (z == o.z) && (w == o.w);
	}
	bool float4::operator!=(const float4& o) const
	{
		return !(*this == o);
	}

	float4& float4::operator=(const float4& other)
	{
		x = other.x;
		y = other.y;
		z = other.z;
		w = other.w;
		return *this;
	}
	// Compound float4
	float4& float4::operator+=(const float4& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
		w += other.w;
		return *this;
	}
	float4& float4::operator-=(const float4& other)
	{
		x -= other.x;
		y -= other.y;
		z -= other.z;
		w -= other.w;
		return *this;
	}
	float4& float4::operator*=(const float4& other)
	{
		*this = *this * other;
		return *this;
	}
	float4& float4::operator/=(const float4& other)
	{
		*this = *this / other;
		return *this;
	}
	// Compound Scalar
	float4& float4::operator+=(const f32& other)
	{
		x += other;
		y += other;
		z += other;
		w += other;
		return *this;
	}
	float4& float4::operator-=(const f32& other)
	{
		x -= other;
		y -= other;
		z -= other;
		w -= other;
		return *this;
	}
	float4& float4::operator*=(const f32& other)
	{
		*this = *this * other;
		return *this;
	}
	float4& float4::operator/=(const f32& other)
	{
		*this = *this / other;
		return *this;
	}

	// float4 x float4
	float4 float4::operator+(const float4& other) const { return float4(x + other.x, y + other.y, z + other.z, w + other.w); }
	float4 float4::operator*(const float4& other) const { return float4(x * other.x, y * other.y, z * other.z, w * other.w); }
	float4 float4::operator-(const float4& other) const { return float4(x - other.x, y - other.y, z - other.z, w - other.w); }
	float4 float4::operator/(const float4& other) const { return float4(x / other.x, y / other.y, z / other.z, w / other.w); }
	// float4 x scalar
	float4 float4::operator+(const f32& other) const { return float4(x + other, y + other, z + other, w + other); }
	float4 float4::operator-(const f32& other) const { return float4(x - other, y - other, z - other, w - other); }
	float4 float4::operator*(const f32& other) const { return float4(x * other, y * other, z * other, w * other); }
	float4 float4::operator/(const f32& other) const { return float4(x / other, y / other, z / other, w / other); }

	//-----------------------
	// Swizzle operators
	//-----------------------
	// float2
	float2 float4::get_xx() const { return float2(x, x); }
	float2 float4::get_xy() const { return float2(x, y); }
	float2 float4::get_xz() const { return float2(x, z); }
	float2 float4::get_xw() const { return float2(x, w); }
	float2 float4::get_yx() const { return float2(y, x); }
	float2 float4::get_yy() const { return float2(y, y); }
	float2 float4::get_yz() const { return float2(y, z); }
	float2 float4::get_yw() const { return float2(y, w); }
	float2 float4::get_zx() const { return float2(z, x); }
	float2 float4::get_zy() const { return float2(z, y); }
	float2 float4::get_zz() const { return float2(z, z); }
	float2 float4::get_zw() const { return float2(z, w); }
	float2 float4::get_wx() const { return float2(w, x); }
	float2 float4::get_wy() const { return float2(w, y); }
	float2 float4::get_wz() const { return float2(w, z); }
	float2 float4::get_ww() const { return float2(w, w); }
	// float3 - x
	float3 float4::get_xxx() const { return float3(x, x, x); }
	float3 float4::get_xxy() const { return float3(x, x, y); }
	float3 float4::get_xxz() const { return float3(x, x, z); }
	float3 float4::get_xxw() const { return float3(x, x, w); }
	float3 float4::get_xyx() const { return float3(x, y, x); }
	float3 float4::get_xyy() const { return float3(x, y, y); }
	float3 float4::get_xyz() const { return float3(x, y, z); }
	float3 float4::get_xyw() const { return float3(x, y, w); }
	float3 float4::get_xzx() const { return float3(x, z, x); }
	float3 float4::get_xzy() const { return float3(x, z, y); }
	float3 float4::get_xzz() const { return float3(x, z, z); }
	float3 float4::get_xzw() const { return float3(x, z, w); }
	float3 float4::get_xwx() const { return float3(x, w, x); }
	float3 float4::get_xwy() const { return float3(x, w, y); }
	float3 float4::get_xwz() const { return float3(x, w, z); }
	float3 float4::get_xww() const { return float3(x, w, w); }
	// float3 - y
	float3 float4::get_yxx() const { return float3(y, x, x); }
	float3 float4::get_yxy() const { return float3(y, x, y); }
	float3 float4::get_yxz() const { return float3(y, x, z); }
	float3 float4::get_yxw() const { return float3(y, x, w); }
	float3 float4::get_yyx() const { return float3(y, y, x); }
	float3 float4::get_yyy() const { return float3(y, y, y); }
	float3 float4::get_yyz() const { return float3(y, y, z); }
	float3 float4::get_yyw() const { return float3(y, y, w); }
	float3 float4::get_yzx() const { return float3(y, z, x); }
	float3 float4::get_yzy() const { return float3(y, z, y); }
	float3 float4::get_yzz() const { return float3(y, z, z); }
	float3 float4::get_yzw() const { return float3(y, z, w); }
	float3 float4::get_ywx() const { return float3(y, w, x); }
	float3 float4::get_ywy() const { return float3(y, w, y); }
	float3 float4::get_ywz() const { return float3(y, w, z); }
	float3 float4::get_yww() const { return float3(y, w, w); }
	// float3 - z
	float3 float4::get_zxx() const { return float3(z, x, x); }
	float3 float4::get_zxy() const { return float3(z, x, y); }
	float3 float4::get_zxz() const { return float3(z, x, z); }
	float3 float4::get_zxw() const { return float3(z, x, w); }
	float3 float4::get_zyx() const { return float3(z, y, x); }
	float3 float4::get_zyy() const { return float3(z, y, y); }
	float3 float4::get_zyz() const { return float3(z, y, z); }
	float3 float4::get_zyw() const { return float3(z, y, w); }
	float3 float4::get_zzx() const { return float3(z, z, x); }
	float3 float4::get_zzy() const { return float3(z, z, y); }
	float3 float4::get_zzz() const { return float3(z, z, z); }
	float3 float4::get_zzw() const { return float3(z, z, w); }
	float3 float4::get_zwx() const { return float3(z, w, x); }
	float3 float4::get_zwy() const { return float3(z, w, y); }
	float3 float4::get_zwz() const { return float3(z, w, z); }
	float3 float4::get_zww() const { return float3(z, w, w); }
	// float3 - w
	float3 float4::get_wxx() const { return float3(w, x, x); }
	float3 float4::get_wxy() const { return float3(w, x, y); }
	float3 float4::get_wxz() const { return float3(w, x, z); }
	float3 float4::get_wxw() const { return float3(w, x, w); }
	float3 float4::get_wyx() const { return float3(w, y, x); }
	float3 float4::get_wyy() const { return float3(w, y, y); }
	float3 float4::get_wyz() const { return float3(w, y, z); }
	float3 float4::get_wyw() const { return float3(w, y, w); }
	float3 float4::get_wzx() const { return float3(w, z, x); }
	float3 float4::get_wzy() const { return float3(w, z, y); }
	float3 float4::get_wzz() const { return float3(w, z, z); }
	float3 float4::get_wzw() const { return float3(w, z, w); }
	float3 float4::get_wwx() const { return float3(w, w, x); }
	float3 float4::get_wwy() const { return float3(w, w, y); }
	float3 float4::get_wwz() const { return float3(w, w, z); }
	float3 float4::get_www() const { return float3(w, w, w); }
	// Swizzle operators - float4
	// x
	float4 float4::get_xxxx() const { return float4(x, x, x, x); }
	float4 float4::get_xxxy() const { return float4(x, x, x, y); }
	float4 float4::get_xxxz() const { return float4(x, x, x, z); }
	float4 float4::get_xxxw() const { return float4(x, x, x, w); }
	float4 float4::get_xxyx() const { return float4(x, x, y, x); }
	float4 float4::get_xxyy() const { return float4(x, x, y, y); }
	float4 float4::get_xxyz() const { return float4(x, x, y, z); }
	float4 float4::get_xxyw() const { return float4(x, x, y, w); }
	float4 float4::get_xxzx() const { return float4(x, x, z, x); }
	float4 float4::get_xxzy() const { return float4(x, x, z, y); }
	float4 float4::get_xxzz() const { return float4(x, x, z, z); }
	float4 float4::get_xxzw() const { return float4(x, x, z, w); }
	float4 float4::get_xxwx() const { return float4(x, x, w, x); }
	float4 float4::get_xxwy() const { return float4(x, x, w, y); }
	float4 float4::get_xxwz() const { return float4(x, x, w, z); }
	float4 float4::get_xxww() const { return float4(x, x, w, w); }
	float4 float4::get_xyxx() const { return float4(x, y, x, x); }
	float4 float4::get_xyxy() const { return float4(x, y, x, y); }
	float4 float4::get_xyxz() const { return float4(x, y, x, z); }
	float4 float4::get_xyxw() const { return float4(x, y, x, w); }
	float4 float4::get_xyyx() const { return float4(x, y, y, x); }
	float4 float4::get_xyyy() const { return float4(x, y, y, y); }
	float4 float4::get_xyyz() const { return float4(x, y, y, z); }
	float4 float4::get_xyyw() const { return float4(x, y, y, w); }
	float4 float4::get_xyzx() const { return float4(x, y, z, x); }
	float4 float4::get_xyzy() const { return float4(x, y, z, y); }
	float4 float4::get_xyzz() const { return float4(x, y, z, z); }
	float4 float4::get_xyzw() const { return float4(x, y, z, w); }
	float4 float4::get_xywx() const { return float4(x, y, w, x); }
	float4 float4::get_xywy() const { return float4(x, y, w, y); }
	float4 float4::get_xywz() const { return float4(x, y, w, z); }
	float4 float4::get_xyww() const { return float4(x, y, w, w); }
	float4 float4::get_xzxx() const { return float4(x, z, x, x); }
	float4 float4::get_xzxy() const { return float4(x, z, x, y); }
	float4 float4::get_xzxz() const { return float4(x, z, x, z); }
	float4 float4::get_xzxw() const { return float4(x, z, x, w); }
	float4 float4::get_xzyx() const { return float4(x, z, y, x); }
	float4 float4::get_xzyy() const { return float4(x, z, y, y); }
	float4 float4::get_xzyz() const { return float4(x, z, y, z); }
	float4 float4::get_xzyw() const { return float4(x, z, y, w); }
	float4 float4::get_xzzx() const { return float4(x, z, z, x); }
	float4 float4::get_xzzy() const { return float4(x, z, z, y); }
	float4 float4::get_xzzz() const { return float4(x, z, z, z); }
	float4 float4::get_xzzw() const { return float4(x, z, z, w); }
	float4 float4::get_xzwx() const { return float4(x, z, w, x); }
	float4 float4::get_xzwy() const { return float4(x, z, w, y); }
	float4 float4::get_xzwz() const { return float4(x, z, w, z); }
	float4 float4::get_xzww() const { return float4(x, z, w, w); }
	float4 float4::get_xwxx() const { return float4(x, w, x, x); }
	float4 float4::get_xwxy() const { return float4(x, w, x, y); }
	float4 float4::get_xwxz() const { return float4(x, w, x, z); }
	float4 float4::get_xwxw() const { return float4(x, w, x, w); }
	float4 float4::get_xwyx() const { return float4(x, w, y, x); }
	float4 float4::get_xwyy() const { return float4(x, w, y, y); }
	float4 float4::get_xwyz() const { return float4(x, w, y, z); }
	float4 float4::get_xwyw() const { return float4(x, w, y, w); }
	float4 float4::get_xwzx() const { return float4(x, w, z, x); }
	float4 float4::get_xwzy() const { return float4(x, w, z, y); }
	float4 float4::get_xwzz() const { return float4(x, w, z, z); }
	float4 float4::get_xwzw() const { return float4(x, w, z, w); }
	float4 float4::get_xwwx() const { return float4(x, w, w, x); }
	float4 float4::get_xwwy() const { return float4(x, w, w, y); }
	float4 float4::get_xwwz() const { return float4(x, w, w, z); }
	float4 float4::get_xwww() const { return float4(x, w, w, w); }
	void   float4::set_xyzw(const float4& o) { *this = o; }
	void   float4::set_xywz(const float4& o) { x = o.x; y = o.y; w = o.z; z = o.w; }
	void   float4::set_xzyw(const float4& o) { x = o.x; z = o.y; y = o.z; w = o.w; }
	void   float4::set_xzwy(const float4& o) { x = o.x; z = o.y; w = o.z; y = o.w; }
	void   float4::set_xwyz(const float4& o) { x = o.x; w = o.y; y = o.z; z = o.w; }
	void   float4::set_xwzy(const float4& o) { x = o.x; w = o.y; z = o.z; y = o.w; }
	// y
	float4 float4::get_yxxx() const { return float4(y, x, x, x); }
	float4 float4::get_yxxy() const { return float4(y, x, x, y); }
	float4 float4::get_yxxz() const { return float4(y, x, x, z); }
	float4 float4::get_yxxw() const { return float4(y, x, x, w); }
	float4 float4::get_yxyx() const { return float4(y, x, y, x); }
	float4 float4::get_yxyy() const { return float4(y, x, y, y); }
	float4 float4::get_yxyz() const { return float4(y, x, y, z); }
	float4 float4::get_yxyw() const { return float4(y, x, y, w); }
	float4 float4::get_yxzx() const { return float4(y, x, z, x); }
	float4 float4::get_yxzy() const { return float4(y, x, z, y); }
	float4 float4::get_yxzz() const { return float4(y, x, z, z); }
	float4 float4::get_yxzw() const { return float4(y, x, z, w); }
	float4 float4::get_yxwx() const { return float4(y, x, w, x); }
	float4 float4::get_yxwy() const { return float4(y, x, w, y); }
	float4 float4::get_yxwz() const { return float4(y, x, w, z); }
	float4 float4::get_yxww() const { return float4(y, x, w, w); }
	float4 float4::get_yyxx() const { return float4(y, y, x, x); }
	float4 float4::get_yyxy() const { return float4(y, y, x, y); }
	float4 float4::get_yyxz() const { return float4(y, y, x, z); }
	float4 float4::get_yyxw() const { return float4(y, y, x, w); }
	float4 float4::get_yyyx() const { return float4(y, y, y, x); }
	float4 float4::get_yyyy() const { return float4(y, y, y, y); }
	float4 float4::get_yyyz() const { return float4(y, y, y, z); }
	float4 float4::get_yyyw() const { return float4(y, y, y, w); }
	float4 float4::get_yyzx() const { return float4(y, y, z, x); }
	float4 float4::get_yyzy() const { return float4(y, y, z, y); }
	float4 float4::get_yyzz() const { return float4(y, y, z, z); }
	float4 float4::get_yyzw() const { return float4(y, y, z, w); }
	float4 float4::get_yywx() const { return float4(y, y, w, x); }
	float4 float4::get_yywy() const { return float4(y, y, w, y); }
	float4 float4::get_yywz() const { return float4(y, y, w, z); }
	float4 float4::get_yyww() const { return float4(y, y, w, w); }
	float4 float4::get_yzxx() const { return float4(y, z, x, x); }
	float4 float4::get_yzxy() const { return float4(y, z, x, y); }
	float4 float4::get_yzxz() const { return float4(y, z, x, z); }
	float4 float4::get_yzxw() const { return float4(y, z, x, w); }
	float4 float4::get_yzyx() const { return float4(y, z, y, x); }
	float4 float4::get_yzyy() const { return float4(y, z, y, y); }
	float4 float4::get_yzyz() const { return float4(y, z, y, z); }
	float4 float4::get_yzyw() const { return float4(y, z, y, w); }
	float4 float4::get_yzzx() const { return float4(y, z, z, x); }
	float4 float4::get_yzzy() const { return float4(y, z, z, y); }
	float4 float4::get_yzzz() const { return float4(y, z, z, z); }
	float4 float4::get_yzzw() const { return float4(y, z, z, w); }
	float4 float4::get_yzwx() const { return float4(y, z, w, x); }
	float4 float4::get_yzwy() const { return float4(y, z, w, y); }
	float4 float4::get_yzwz() const { return float4(y, z, w, z); }
	float4 float4::get_yzww() const { return float4(y, z, w, w); }
	float4 float4::get_ywxx() const { return float4(y, w, x, x); }
	float4 float4::get_ywxy() const { return float4(y, w, x, y); }
	float4 float4::get_ywxz() const { return float4(y, w, x, z); }
	float4 float4::get_ywxw() const { return float4(y, w, x, w); }
	float4 float4::get_ywyx() const { return float4(y, w, y, x); }
	float4 float4::get_ywyy() const { return float4(y, w, y, y); }
	float4 float4::get_ywyz() const { return float4(y, w, y, z); }
	float4 float4::get_ywyw() const { return float4(y, w, y, w); }
	float4 float4::get_ywzx() const { return float4(y, w, z, x); }
	float4 float4::get_ywzy() const { return float4(y, w, z, y); }
	float4 float4::get_ywzz() const { return float4(y, w, z, z); }
	float4 float4::get_ywzw() const { return float4(y, w, z, w); }
	float4 float4::get_ywwx() const { return float4(y, w, w, x); }
	float4 float4::get_ywwy() const { return float4(y, w, w, y); }
	float4 float4::get_ywwz() const { return float4(y, w, w, z); }
	float4 float4::get_ywww() const { return float4(y, w, w, w); }
	void   float4::set_yxzw(const float4& o) { y = o.x; x = o.y; z = o.z; w = o.w; }
	void   float4::set_yxwz(const float4& o) { y = o.x; x = o.y; w = o.z; z = o.w; }
	void   float4::set_yzxw(const float4& o) { y = o.x; z = o.y; x = o.z; w = o.w; }
	void   float4::set_yzwx(const float4& o) { y = o.x; z = o.y; w = o.z; x = o.w; }
	void   float4::set_ywxz(const float4& o) { y = o.x; w = o.y; x = o.z; z = o.w; }
	void   float4::set_ywzx(const float4& o) { y = o.x; w = o.y; z = o.z; x = o.w; }
	// z
	float4 float4::get_zxxx() const { return float4(z, x, x, x); }
	float4 float4::get_zxxy() const { return float4(z, x, x, y); }
	float4 float4::get_zxxz() const { return float4(z, x, x, z); }
	float4 float4::get_zxxw() const { return float4(z, x, x, w); }
	float4 float4::get_zxyx() const { return float4(z, x, y, x); }
	float4 float4::get_zxyy() const { return float4(z, x, y, y); }
	float4 float4::get_zxyz() const { return float4(z, x, y, z); }
	float4 float4::get_zxyw() const { return float4(z, x, y, w); }
	float4 float4::get_zxzx() const { return float4(z, x, z, x); }
	float4 float4::get_zxzy() const { return float4(z, x, z, y); }
	float4 float4::get_zxzz() const { return float4(z, x, z, z); }
	float4 float4::get_zxzw() const { return float4(z, x, z, w); }
	float4 float4::get_zxwx() const { return float4(z, x, w, x); }
	float4 float4::get_zxwy() const { return float4(z, x, w, y); }
	float4 float4::get_zxwz() const { return float4(z, x, w, z); }
	float4 float4::get_zxww() const { return float4(z, x, w, w); }
	float4 float4::get_zyxx() const { return float4(z, y, x, x); }
	float4 float4::get_zyxy() const { return float4(z, y, x, y); }
	float4 float4::get_zyxz() const { return float4(z, y, x, z); }
	float4 float4::get_zyxw() const { return float4(z, y, x, w); }
	float4 float4::get_zyyx() const { return float4(z, y, y, x); }
	float4 float4::get_zyyy() const { return float4(z, y, y, y); }
	float4 float4::get_zyyz() const { return float4(z, y, y, z); }
	float4 float4::get_zyyw() const { return float4(z, y, y, w); }
	float4 float4::get_zyzx() const { return float4(z, y, z, x); }
	float4 float4::get_zyzy() const { return float4(z, y, z, y); }
	float4 float4::get_zyzz() const { return float4(z, y, z, z); }
	float4 float4::get_zyzw() const { return float4(z, y, z, w); }
	float4 float4::get_zywx() const { return float4(z, y, w, x); }
	float4 float4::get_zywy() const { return float4(z, y, w, y); }
	float4 float4::get_zywz() const { return float4(z, y, w, z); }
	float4 float4::get_zyww() const { return float4(z, y, w, w); }
	float4 float4::get_zzxx() const { return float4(z, z, x, x); }
	float4 float4::get_zzxy() const { return float4(z, z, x, y); }
	float4 float4::get_zzxz() const { return float4(z, z, x, z); }
	float4 float4::get_zzxw() const { return float4(z, z, x, w); }
	float4 float4::get_zzyx() const { return float4(z, z, y, x); }
	float4 float4::get_zzyy() const { return float4(z, z, y, y); }
	float4 float4::get_zzyz() const { return float4(z, z, y, z); }
	float4 float4::get_zzyw() const { return float4(z, z, y, w); }
	float4 float4::get_zzzx() const { return float4(z, z, z, x); }
	float4 float4::get_zzzy() const { return float4(z, z, z, y); }
	float4 float4::get_zzzz() const { return float4(z, z, z, z); }
	float4 float4::get_zzzw() const { return float4(z, z, z, w); }
	float4 float4::get_zzwx() const { return float4(z, z, w, x); }
	float4 float4::get_zzwy() const { return float4(z, z, w, y); }
	float4 float4::get_zzwz() const { return float4(z, z, w, z); }
	float4 float4::get_zzww() const { return float4(z, z, w, w); }
	float4 float4::get_zwxx() const { return float4(z, w, x, x); }
	float4 float4::get_zwxy() const { return float4(z, w, x, y); }
	float4 float4::get_zwxz() const { return float4(z, w, x, z); }
	float4 float4::get_zwxw() const { return float4(z, w, x, w); }
	float4 float4::get_zwyx() const { return float4(z, w, y, x); }
	float4 float4::get_zwyy() const { return float4(z, w, y, y); }
	float4 float4::get_zwyz() const { return float4(z, w, y, z); }
	float4 float4::get_zwyw() const { return float4(z, w, y, w); }
	float4 float4::get_zwzx() const { return float4(z, w, z, x); }
	float4 float4::get_zwzy() const { return float4(z, w, z, y); }
	float4 float4::get_zwzz() const { return float4(z, w, z, z); }
	float4 float4::get_zwzw() const { return float4(z, w, z, w); }
	float4 float4::get_zwwx() const { return float4(z, w, w, x); }
	float4 float4::get_zwwy() const { return float4(z, w, w, y); }
	float4 float4::get_zwwz() const { return float4(z, w, w, z); }
	float4 float4::get_zwww() const { return float4(z, w, w, w); }
	void   float4::set_zyxw(const float4& o) { z = o.x; y = o.y; x = o.z; w = o.w; }
	void   float4::set_zywx(const float4& o) { z = o.x; y = o.y; w = o.z; x = o.w; }
	void   float4::set_zxyw(const float4& o) { z = o.x; x = o.y; y = o.z; w = o.w; }
	void   float4::set_zxwy(const float4& o) { z = o.x; x = o.y; w = o.z; y = o.w; }
	void   float4::set_zwyx(const float4& o) { z = o.x; w = o.y; y = o.z; x = o.w; }
	void   float4::set_zwxy(const float4& o) { z = o.x; w = o.y; x = o.z; y = o.w; }
	// w
	float4 float4::get_wxxx() const { return float4(w, x, x, x); }
	float4 float4::get_wxxy() const { return float4(w, x, x, y); }
	float4 float4::get_wxxz() const { return float4(w, x, x, z); }
	float4 float4::get_wxxw() const { return float4(w, x, x, w); }
	float4 float4::get_wxyx() const { return float4(w, x, y, x); }
	float4 float4::get_wxyy() const { return float4(w, x, y, y); }
	float4 float4::get_wxyz() const { return float4(w, x, y, z); }
	float4 float4::get_wxyw() const { return float4(w, x, y, w); }
	float4 float4::get_wxzx() const { return float4(w, x, z, x); }
	float4 float4::get_wxzy() const { return float4(w, x, z, y); }
	float4 float4::get_wxzz() const { return float4(w, x, z, z); }
	float4 float4::get_wxzw() const { return float4(w, x, z, w); }
	float4 float4::get_wxwx() const { return float4(w, x, w, x); }
	float4 float4::get_wxwy() const { return float4(w, x, w, y); }
	float4 float4::get_wxwz() const { return float4(w, x, w, z); }
	float4 float4::get_wxww() const { return float4(w, x, w, w); }
	float4 float4::get_wyxx() const { return float4(w, y, x, x); }
	float4 float4::get_wyxy() const { return float4(w, y, x, y); }
	float4 float4::get_wyxz() const { return float4(w, y, x, z); }
	float4 float4::get_wyxw() const { return float4(w, y, x, w); }
	float4 float4::get_wyyx() const { return float4(w, y, y, x); }
	float4 float4::get_wyyy() const { return float4(w, y, y, y); }
	float4 float4::get_wyyz() const { return float4(w, y, y, z); }
	float4 float4::get_wyyw() const { return float4(w, y, y, w); }
	float4 float4::get_wyzx() const { return float4(w, y, z, x); }
	float4 float4::get_wyzy() const { return float4(w, y, z, y); }
	float4 float4::get_wyzz() const { return float4(w, y, z, z); }
	float4 float4::get_wyzw() const { return float4(w, y, z, w); }
	float4 float4::get_wywx() const { return float4(w, y, w, x); }
	float4 float4::get_wywy() const { return float4(w, y, w, y); }
	float4 float4::get_wywz() const { return float4(w, y, w, z); }
	float4 float4::get_wyww() const { return float4(w, y, w, w); }
	float4 float4::get_wzxx() const { return float4(w, z, x, x); }
	float4 float4::get_wzxy() const { return float4(w, z, x, y); }
	float4 float4::get_wzxz() const { return float4(w, z, x, z); }
	float4 float4::get_wzxw() const { return float4(w, z, x, w); }
	float4 float4::get_wzyx() const { return float4(w, z, y, x); }
	float4 float4::get_wzyy() const { return float4(w, z, y, y); }
	float4 float4::get_wzyz() const { return float4(w, z, y, z); }
	float4 float4::get_wzyw() const { return float4(w, z, y, w); }
	float4 float4::get_wzzx() const { return float4(w, z, z, x); }
	float4 float4::get_wzzy() const { return float4(w, z, z, y); }
	float4 float4::get_wzzz() const { return float4(w, z, z, z); }
	float4 float4::get_wzzw() const { return float4(w, z, z, w); }
	float4 float4::get_wzwx() const { return float4(w, z, w, x); }
	float4 float4::get_wzwy() const { return float4(w, z, w, y); }
	float4 float4::get_wzwz() const { return float4(w, z, w, z); }
	float4 float4::get_wzww() const { return float4(w, z, w, w); }
	float4 float4::get_wwxx() const { return float4(w, w, x, x); }
	float4 float4::get_wwxy() const { return float4(w, w, x, y); }
	float4 float4::get_wwxz() const { return float4(w, w, x, z); }
	float4 float4::get_wwxw() const { return float4(w, w, x, w); }
	float4 float4::get_wwyx() const { return float4(w, w, y, x); }
	float4 float4::get_wwyy() const { return float4(w, w, y, y); }
	float4 float4::get_wwyz() const { return float4(w, w, y, z); }
	float4 float4::get_wwyw() const { return float4(w, w, y, w); }
	float4 float4::get_wwzx() const { return float4(w, w, z, x); }
	float4 float4::get_wwzy() const { return float4(w, w, z, y); }
	float4 float4::get_wwzz() const { return float4(w, w, z, z); }
	float4 float4::get_wwzw() const { return float4(w, w, z, w); }
	float4 float4::get_wwwx() const { return float4(w, w, w, x); }
	float4 float4::get_wwwy() const { return float4(w, w, w, y); }
	float4 float4::get_wwwz() const { return float4(w, w, w, z); }
	float4 float4::get_wwww() const { return float4(w, w, w, w); }
	void   float4::set_wyzx(const float4& o) { w = o.x; y = o.y; z = o.z; x = o.w; }
	void   float4::set_wyxz(const float4& o) { w = o.x; y = o.y; x = o.z; z = o.w; }
	void   float4::set_wzyx(const float4& o) { w = o.x; z = o.y; y = o.z; x = o.w; }
	void   float4::set_wzxy(const float4& o) { w = o.x; z = o.y; x = o.z; y = o.w; }
	void   float4::set_wxyz(const float4& o) { w = o.x; x = o.y; y = o.z; z = o.w; }
	void   float4::set_wxzy(const float4& o) { w = o.x; x = o.y; z = o.z; y = o.w; }

	//-----------------------
	// Array operator
	//-----------------------
	f32& float4::get_opIndex(s32 index)
	{
		if (index < 0 || index >= 4)
		{
			// TODO: WARNING
			index = 0;
		}
		return m[index];
	}

	//-----------------------
	// AngelScript functions
	//-----------------------
	static void float4DefaultConstructor(float4* self) { new(self) float4(); }
	static void float4CopyConstructor(const float4& other, float4* self) { new(self) float4(other); }
	static void float4ConvConstructor(f32 x, float4* self) { new(self) float4(x, x, x, x); }
	static void float4InitConstructor(f32 x, f32 y, f32 z, f32 w, float4* self) { new(self) float4(x, y, z, w); }
	static void float4ListConstructor(f32* list, float4* self) { new(self) float4(list[0], list[1], list[2], list[3]); }

	//-----------------------
	// Intrinsics
	//-----------------------
	std::string toStringF4(const float4& v)
	{
		char tmp[1024];
		sprintf(tmp, "(%g, %g, %g, %g)", v.x, v.y, v.z, v.w);
		return std::string(tmp);
	}

	//-------------------------------------
	// Registration
	//-------------------------------------
	static s32 s_float4ObjId = -1;
	s32 getFloat4ObjectId()
	{
		return s_float4ObjId;
	}

	void registerScriptMath_float4(asIScriptEngine *engine)
	{
		s32 r;
		r = engine->RegisterObjectType("float4", sizeof(float4), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<float4>() | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
		s_float4ObjId = r;

		// Register the object properties
		r = engine->RegisterObjectProperty("float4", "float x", asOFFSET(float4, x)); assert(r >= 0);
		r = engine->RegisterObjectProperty("float4", "float y", asOFFSET(float4, y)); assert(r >= 0);
		r = engine->RegisterObjectProperty("float4", "float z", asOFFSET(float4, z)); assert(r >= 0);
		r = engine->RegisterObjectProperty("float4", "float w", asOFFSET(float4, w)); assert(r >= 0);

		// Register the constructors
		r = engine->RegisterObjectBehaviour("float4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(float4DefaultConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float4", asBEHAVE_CONSTRUCT, "void f(const float4 &in)", asFUNCTION(float4CopyConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float4", asBEHAVE_CONSTRUCT, "void f(float)", asFUNCTION(float4ConvConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float4", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(float4InitConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("float4", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in) {float, float, float, float}", asFUNCTION(float4ListConstructor), asCALL_CDECL_OBJLAST); assert(r >= 0);

		// Register the operator overloads
		r = engine->RegisterObjectMethod("float4", "float4 &opAddAssign(const float4 &in)", asMETHODPR(float4, operator+=, (const float4 &), float4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 &opSubAssign(const float4 &in)", asMETHODPR(float4, operator-=, (const float4 &), float4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 &opMulAssign(const float4 &in)", asMETHODPR(float4, operator*=, (const float4 &), float4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 &opDivAssign(const float4 &in)", asMETHODPR(float4, operator/=, (const float4 &), float4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 &opAddAssign(const float &in)", asMETHODPR(float4, operator+=, (const float &), float4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 &opSubAssign(const float &in)", asMETHODPR(float4, operator-=, (const float &), float4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 &opMulAssign(const float &in)", asMETHODPR(float4, operator*=, (const float &), float4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 &opDivAssign(const float &in)", asMETHODPR(float4, operator/=, (const float &), float4&), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "bool opEquals(const float4 &in) const", asMETHODPR(float4, operator==, (const float4 &) const, bool), asCALL_THISCALL); assert(r >= 0);

		// Indexing operator.
		r = engine->RegisterObjectMethod("float4", "float& opIndex(int)", asMETHOD(float4, get_opIndex), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterObjectMethod("float4", "float4 opAdd(const float4 &in) const", asMETHODPR(float4, operator+, (const float4 &) const, float4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 opSub(const float4 &in) const", asMETHODPR(float4, operator-, (const float4 &) const, float4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 opMul(const float4 &in) const", asMETHODPR(float4, operator*, (const float4 &) const, float4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 opDiv(const float4 &in) const", asMETHODPR(float4, operator/, (const float4 &) const, float4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 opAdd(const float &in) const", asMETHODPR(float4, operator+, (const float &) const, float4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 opSub(const float &in) const", asMETHODPR(float4, operator-, (const float &) const, float4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 opMul(const float &in) const", asMETHODPR(float4, operator*, (const float &) const, float4), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 opDiv(const float &in) const", asMETHODPR(float4, operator/, (const float &) const, float4), asCALL_THISCALL); assert(r >= 0);

		// Register the swizzle operators
		// float2
		// x
		r = engine->RegisterObjectMethod("float4", "float2 get_xx() const property", asMETHOD(float4, get_xx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_xy() const property", asMETHOD(float4, get_xy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_xz() const property", asMETHOD(float4, get_xz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_xw() const property", asMETHOD(float4, get_xw), asCALL_THISCALL); assert(r >= 0);
		// y
		r = engine->RegisterObjectMethod("float4", "float2 get_yx() const property", asMETHOD(float4, get_yx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_yy() const property", asMETHOD(float4, get_yy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_yz() const property", asMETHOD(float4, get_yz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_yw() const property", asMETHOD(float4, get_yw), asCALL_THISCALL); assert(r >= 0);
		// z
		r = engine->RegisterObjectMethod("float4", "float2 get_zx() const property", asMETHOD(float4, get_zx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_zy() const property", asMETHOD(float4, get_zy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_zz() const property", asMETHOD(float4, get_zz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_zw() const property", asMETHOD(float4, get_zw), asCALL_THISCALL); assert(r >= 0);
		// w
		r = engine->RegisterObjectMethod("float4", "float2 get_wx() const property", asMETHOD(float4, get_wx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_wy() const property", asMETHOD(float4, get_wy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_wz() const property", asMETHOD(float4, get_wz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float2 get_ww() const property", asMETHOD(float4, get_ww), asCALL_THISCALL); assert(r >= 0);
		// Swizzle operators - float3
		// x
		r = engine->RegisterObjectMethod("float4", "float3 get_xxx() const property", asMETHOD(float4, get_xxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xxy() const property", asMETHOD(float4, get_xxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xxz() const property", asMETHOD(float4, get_xxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xxw() const property", asMETHOD(float4, get_xxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xyx() const property", asMETHOD(float4, get_xyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xyy() const property", asMETHOD(float4, get_xyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xyz() const property", asMETHOD(float4, get_xyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xyw() const property", asMETHOD(float4, get_xyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xzx() const property", asMETHOD(float4, get_xzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xzy() const property", asMETHOD(float4, get_xzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xzz() const property", asMETHOD(float4, get_xzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xzw() const property", asMETHOD(float4, get_xzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xwx() const property", asMETHOD(float4, get_xwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xwy() const property", asMETHOD(float4, get_xwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xwz() const property", asMETHOD(float4, get_xwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_xww() const property", asMETHOD(float4, get_xww), asCALL_THISCALL); assert(r >= 0);
		// y
		r = engine->RegisterObjectMethod("float4", "float3 get_yxx() const property", asMETHOD(float4, get_yxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yxy() const property", asMETHOD(float4, get_yxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yxz() const property", asMETHOD(float4, get_yxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yxw() const property", asMETHOD(float4, get_yxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yyx() const property", asMETHOD(float4, get_yyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yyy() const property", asMETHOD(float4, get_yyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yyz() const property", asMETHOD(float4, get_yyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yyw() const property", asMETHOD(float4, get_yyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yzx() const property", asMETHOD(float4, get_yzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yzy() const property", asMETHOD(float4, get_yzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yzz() const property", asMETHOD(float4, get_yzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yzw() const property", asMETHOD(float4, get_yzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_ywx() const property", asMETHOD(float4, get_ywx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_ywy() const property", asMETHOD(float4, get_ywy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_ywz() const property", asMETHOD(float4, get_ywz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_yww() const property", asMETHOD(float4, get_yww), asCALL_THISCALL); assert(r >= 0);
		// z
		r = engine->RegisterObjectMethod("float4", "float3 get_zxx() const property", asMETHOD(float4, get_zxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zxy() const property", asMETHOD(float4, get_zxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zxz() const property", asMETHOD(float4, get_zxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zxw() const property", asMETHOD(float4, get_zxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zyx() const property", asMETHOD(float4, get_zyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zyy() const property", asMETHOD(float4, get_zyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zyz() const property", asMETHOD(float4, get_zyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zyw() const property", asMETHOD(float4, get_zyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zzx() const property", asMETHOD(float4, get_zzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zzy() const property", asMETHOD(float4, get_zzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zzz() const property", asMETHOD(float4, get_zzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zzw() const property", asMETHOD(float4, get_zzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zwx() const property", asMETHOD(float4, get_zwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zwy() const property", asMETHOD(float4, get_zwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zwz() const property", asMETHOD(float4, get_zwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_zww() const property", asMETHOD(float4, get_zww), asCALL_THISCALL); assert(r >= 0);
		// w
		r = engine->RegisterObjectMethod("float4", "float3 get_wxx() const property", asMETHOD(float4, get_wxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wxy() const property", asMETHOD(float4, get_wxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wxz() const property", asMETHOD(float4, get_wxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wxw() const property", asMETHOD(float4, get_wxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wyx() const property", asMETHOD(float4, get_wyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wyy() const property", asMETHOD(float4, get_wyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wyz() const property", asMETHOD(float4, get_wyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wyw() const property", asMETHOD(float4, get_wyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wzx() const property", asMETHOD(float4, get_wzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wzy() const property", asMETHOD(float4, get_wzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wzz() const property", asMETHOD(float4, get_wzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wzw() const property", asMETHOD(float4, get_wzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wwx() const property", asMETHOD(float4, get_wwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wwy() const property", asMETHOD(float4, get_wwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_wwz() const property", asMETHOD(float4, get_wwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float3 get_www() const property", asMETHOD(float4, get_www), asCALL_THISCALL); assert(r >= 0);
		// Swizzle operators - float4
		// x
		r = engine->RegisterObjectMethod("float4", "float4 get_xxxx() const property", asMETHOD(float4, get_xxxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxxy() const property", asMETHOD(float4, get_xxxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxxz() const property", asMETHOD(float4, get_xxxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxxw() const property", asMETHOD(float4, get_xxxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxyx() const property", asMETHOD(float4, get_xxyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxyy() const property", asMETHOD(float4, get_xxyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxyz() const property", asMETHOD(float4, get_xxyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxyw() const property", asMETHOD(float4, get_xxyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxzx() const property", asMETHOD(float4, get_xxzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxzy() const property", asMETHOD(float4, get_xxzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxzz() const property", asMETHOD(float4, get_xxzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxzw() const property", asMETHOD(float4, get_xxzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxwx() const property", asMETHOD(float4, get_xxwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxwy() const property", asMETHOD(float4, get_xxwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxwz() const property", asMETHOD(float4, get_xxwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xxww() const property", asMETHOD(float4, get_xxww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyxx() const property", asMETHOD(float4, get_xyxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyxy() const property", asMETHOD(float4, get_xyxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyxz() const property", asMETHOD(float4, get_xyxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyxw() const property", asMETHOD(float4, get_xyxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyyx() const property", asMETHOD(float4, get_xyyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyyy() const property", asMETHOD(float4, get_xyyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyyz() const property", asMETHOD(float4, get_xyyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyyw() const property", asMETHOD(float4, get_xyyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyzx() const property", asMETHOD(float4, get_xyzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyzy() const property", asMETHOD(float4, get_xyzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyzz() const property", asMETHOD(float4, get_xyzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyzw() const property", asMETHOD(float4, get_xyzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xywx() const property", asMETHOD(float4, get_xywx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xywy() const property", asMETHOD(float4, get_xywy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xywz() const property", asMETHOD(float4, get_xywz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xyww() const property", asMETHOD(float4, get_xyww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzxx() const property", asMETHOD(float4, get_xzxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzxy() const property", asMETHOD(float4, get_xzxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzxz() const property", asMETHOD(float4, get_xzxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzxw() const property", asMETHOD(float4, get_xzxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzyx() const property", asMETHOD(float4, get_xzyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzyy() const property", asMETHOD(float4, get_xzyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzyz() const property", asMETHOD(float4, get_xzyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzyw() const property", asMETHOD(float4, get_xzyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzzx() const property", asMETHOD(float4, get_xzzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzzy() const property", asMETHOD(float4, get_xzzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzzz() const property", asMETHOD(float4, get_xzzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzzw() const property", asMETHOD(float4, get_xzzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzwx() const property", asMETHOD(float4, get_xzwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzwy() const property", asMETHOD(float4, get_xzwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzwz() const property", asMETHOD(float4, get_xzwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xzww() const property", asMETHOD(float4, get_xzww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwxx() const property", asMETHOD(float4, get_xwxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwxy() const property", asMETHOD(float4, get_xwxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwxz() const property", asMETHOD(float4, get_xwxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwxw() const property", asMETHOD(float4, get_xwxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwyx() const property", asMETHOD(float4, get_xwyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwyy() const property", asMETHOD(float4, get_xwyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwyz() const property", asMETHOD(float4, get_xwyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwyw() const property", asMETHOD(float4, get_xwyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwzx() const property", asMETHOD(float4, get_xwzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwzy() const property", asMETHOD(float4, get_xwzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwzz() const property", asMETHOD(float4, get_xwzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwzw() const property", asMETHOD(float4, get_xwzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwwx() const property", asMETHOD(float4, get_xwwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwwy() const property", asMETHOD(float4, get_xwwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwwz() const property", asMETHOD(float4, get_xwwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_xwww() const property", asMETHOD(float4, get_xwww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_xyzw(const float4 &in) property", asMETHOD(float4, set_xyzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_xywz(const float4 &in) property", asMETHOD(float4, set_xywz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_xzyw(const float4 &in) property", asMETHOD(float4, set_xzyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_xzwy(const float4 &in) property", asMETHOD(float4, set_xzwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_xwyz(const float4 &in) property", asMETHOD(float4, set_xwyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_xwzy(const float4 &in) property", asMETHOD(float4, set_xwzy), asCALL_THISCALL); assert(r >= 0);
		// y
		r = engine->RegisterObjectMethod("float4", "float4 get_yxxx() const property", asMETHOD(float4, get_yxxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxxy() const property", asMETHOD(float4, get_yxxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxxz() const property", asMETHOD(float4, get_yxxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxxw() const property", asMETHOD(float4, get_yxxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxyx() const property", asMETHOD(float4, get_yxyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxyy() const property", asMETHOD(float4, get_yxyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxyz() const property", asMETHOD(float4, get_yxyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxyw() const property", asMETHOD(float4, get_yxyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxzx() const property", asMETHOD(float4, get_yxzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxzy() const property", asMETHOD(float4, get_yxzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxzz() const property", asMETHOD(float4, get_yxzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxzw() const property", asMETHOD(float4, get_yxzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxwx() const property", asMETHOD(float4, get_yxwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxwy() const property", asMETHOD(float4, get_yxwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxwz() const property", asMETHOD(float4, get_yxwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yxww() const property", asMETHOD(float4, get_yxww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyxx() const property", asMETHOD(float4, get_yyxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyxy() const property", asMETHOD(float4, get_yyxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyxz() const property", asMETHOD(float4, get_yyxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyxw() const property", asMETHOD(float4, get_yyxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyyx() const property", asMETHOD(float4, get_yyyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyyy() const property", asMETHOD(float4, get_yyyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyyz() const property", asMETHOD(float4, get_yyyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyyw() const property", asMETHOD(float4, get_yyyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyzx() const property", asMETHOD(float4, get_yyzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyzy() const property", asMETHOD(float4, get_yyzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyzz() const property", asMETHOD(float4, get_yyzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyzw() const property", asMETHOD(float4, get_yyzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yywx() const property", asMETHOD(float4, get_yywx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yywy() const property", asMETHOD(float4, get_yywy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yywz() const property", asMETHOD(float4, get_yywz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yyww() const property", asMETHOD(float4, get_yyww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzxx() const property", asMETHOD(float4, get_yzxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzxy() const property", asMETHOD(float4, get_yzxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzxz() const property", asMETHOD(float4, get_yzxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzxw() const property", asMETHOD(float4, get_yzxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzyx() const property", asMETHOD(float4, get_yzyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzyy() const property", asMETHOD(float4, get_yzyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzyz() const property", asMETHOD(float4, get_yzyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzyw() const property", asMETHOD(float4, get_yzyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzzx() const property", asMETHOD(float4, get_yzzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzzy() const property", asMETHOD(float4, get_yzzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzzz() const property", asMETHOD(float4, get_yzzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzzw() const property", asMETHOD(float4, get_yzzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzwx() const property", asMETHOD(float4, get_yzwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzwy() const property", asMETHOD(float4, get_yzwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzwz() const property", asMETHOD(float4, get_yzwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_yzww() const property", asMETHOD(float4, get_yzww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywxx() const property", asMETHOD(float4, get_ywxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywxy() const property", asMETHOD(float4, get_ywxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywxz() const property", asMETHOD(float4, get_ywxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywxw() const property", asMETHOD(float4, get_ywxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywyx() const property", asMETHOD(float4, get_ywyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywyy() const property", asMETHOD(float4, get_ywyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywyz() const property", asMETHOD(float4, get_ywyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywyw() const property", asMETHOD(float4, get_ywyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywzx() const property", asMETHOD(float4, get_ywzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywzy() const property", asMETHOD(float4, get_ywzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywzz() const property", asMETHOD(float4, get_ywzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywzw() const property", asMETHOD(float4, get_ywzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywwx() const property", asMETHOD(float4, get_ywwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywwy() const property", asMETHOD(float4, get_ywwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywwz() const property", asMETHOD(float4, get_ywwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_ywww() const property", asMETHOD(float4, get_ywww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_yxzw(const float4 &in) property", asMETHOD(float4, set_yxzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_yxwz(const float4 &in) property", asMETHOD(float4, set_yxwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_yzxw(const float4 &in) property", asMETHOD(float4, set_yzxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_yzwx(const float4 &in) property", asMETHOD(float4, set_yzwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_ywxz(const float4 &in) property", asMETHOD(float4, set_ywxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_ywzx(const float4 &in) property", asMETHOD(float4, set_ywzx), asCALL_THISCALL); assert(r >= 0);
		// z
		r = engine->RegisterObjectMethod("float4", "float4 get_zxxx() const property", asMETHOD(float4, get_zxxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxxy() const property", asMETHOD(float4, get_zxxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxxz() const property", asMETHOD(float4, get_zxxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxxw() const property", asMETHOD(float4, get_zxxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxyx() const property", asMETHOD(float4, get_zxyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxyy() const property", asMETHOD(float4, get_zxyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxyz() const property", asMETHOD(float4, get_zxyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxyw() const property", asMETHOD(float4, get_zxyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxzx() const property", asMETHOD(float4, get_zxzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxzy() const property", asMETHOD(float4, get_zxzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxzz() const property", asMETHOD(float4, get_zxzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxzw() const property", asMETHOD(float4, get_zxzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxwx() const property", asMETHOD(float4, get_zxwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxwy() const property", asMETHOD(float4, get_zxwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxwz() const property", asMETHOD(float4, get_zxwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zxww() const property", asMETHOD(float4, get_zxww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyxx() const property", asMETHOD(float4, get_zyxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyxy() const property", asMETHOD(float4, get_zyxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyxz() const property", asMETHOD(float4, get_zyxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyxw() const property", asMETHOD(float4, get_zyxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyyx() const property", asMETHOD(float4, get_zyyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyyy() const property", asMETHOD(float4, get_zyyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyyz() const property", asMETHOD(float4, get_zyyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyyw() const property", asMETHOD(float4, get_zyyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyzx() const property", asMETHOD(float4, get_zyzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyzy() const property", asMETHOD(float4, get_zyzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyzz() const property", asMETHOD(float4, get_zyzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyzw() const property", asMETHOD(float4, get_zyzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zywx() const property", asMETHOD(float4, get_zywx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zywy() const property", asMETHOD(float4, get_zywy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zywz() const property", asMETHOD(float4, get_zywz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zyww() const property", asMETHOD(float4, get_zyww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzxx() const property", asMETHOD(float4, get_zzxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzxy() const property", asMETHOD(float4, get_zzxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzxz() const property", asMETHOD(float4, get_zzxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzxw() const property", asMETHOD(float4, get_zzxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzyx() const property", asMETHOD(float4, get_zzyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzyy() const property", asMETHOD(float4, get_zzyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzyz() const property", asMETHOD(float4, get_zzyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzyw() const property", asMETHOD(float4, get_zzyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzzx() const property", asMETHOD(float4, get_zzzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzzy() const property", asMETHOD(float4, get_zzzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzzz() const property", asMETHOD(float4, get_zzzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzzw() const property", asMETHOD(float4, get_zzzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzwx() const property", asMETHOD(float4, get_zzwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzwy() const property", asMETHOD(float4, get_zzwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzwz() const property", asMETHOD(float4, get_zzwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zzww() const property", asMETHOD(float4, get_zzww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwxx() const property", asMETHOD(float4, get_zwxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwxy() const property", asMETHOD(float4, get_zwxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwxz() const property", asMETHOD(float4, get_zwxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwxw() const property", asMETHOD(float4, get_zwxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwyx() const property", asMETHOD(float4, get_zwyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwyy() const property", asMETHOD(float4, get_zwyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwyz() const property", asMETHOD(float4, get_zwyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwyw() const property", asMETHOD(float4, get_zwyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwzx() const property", asMETHOD(float4, get_zwzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwzy() const property", asMETHOD(float4, get_zwzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwzz() const property", asMETHOD(float4, get_zwzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwzw() const property", asMETHOD(float4, get_zwzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwwx() const property", asMETHOD(float4, get_zwwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwwy() const property", asMETHOD(float4, get_zwwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwwz() const property", asMETHOD(float4, get_zwwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_zwww() const property", asMETHOD(float4, get_zwww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_zyxw(const float4 &in) property", asMETHOD(float4, set_zyxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_zywx(const float4 &in) property", asMETHOD(float4, set_zywx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_zxyw(const float4 &in) property", asMETHOD(float4, set_zxyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_zxwy(const float4 &in) property", asMETHOD(float4, set_zxwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_zwyx(const float4 &in) property", asMETHOD(float4, set_zwyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_zwxy(const float4 &in) property", asMETHOD(float4, set_zwxy), asCALL_THISCALL); assert(r >= 0);
		// w
		r = engine->RegisterObjectMethod("float4", "float4 get_wxxx() const property", asMETHOD(float4, get_wxxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxxy() const property", asMETHOD(float4, get_wxxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxxz() const property", asMETHOD(float4, get_wxxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxxw() const property", asMETHOD(float4, get_wxxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxyx() const property", asMETHOD(float4, get_wxyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxyy() const property", asMETHOD(float4, get_wxyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxyz() const property", asMETHOD(float4, get_wxyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxyw() const property", asMETHOD(float4, get_wxyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxzx() const property", asMETHOD(float4, get_wxzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxzy() const property", asMETHOD(float4, get_wxzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxzz() const property", asMETHOD(float4, get_wxzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxzw() const property", asMETHOD(float4, get_wxzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxwx() const property", asMETHOD(float4, get_wxwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxwy() const property", asMETHOD(float4, get_wxwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxwz() const property", asMETHOD(float4, get_wxwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wxww() const property", asMETHOD(float4, get_wxww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyxx() const property", asMETHOD(float4, get_wyxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyxy() const property", asMETHOD(float4, get_wyxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyxz() const property", asMETHOD(float4, get_wyxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyxw() const property", asMETHOD(float4, get_wyxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyyx() const property", asMETHOD(float4, get_wyyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyyy() const property", asMETHOD(float4, get_wyyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyyz() const property", asMETHOD(float4, get_wyyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyyw() const property", asMETHOD(float4, get_wyyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyzx() const property", asMETHOD(float4, get_wyzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyzy() const property", asMETHOD(float4, get_wyzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyzz() const property", asMETHOD(float4, get_wyzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyzw() const property", asMETHOD(float4, get_wyzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wywx() const property", asMETHOD(float4, get_wywx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wywy() const property", asMETHOD(float4, get_wywy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wywz() const property", asMETHOD(float4, get_wywz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wyww() const property", asMETHOD(float4, get_wyww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzxx() const property", asMETHOD(float4, get_wzxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzxy() const property", asMETHOD(float4, get_wzxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzxz() const property", asMETHOD(float4, get_wzxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzxw() const property", asMETHOD(float4, get_wzxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzyx() const property", asMETHOD(float4, get_wzyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzyy() const property", asMETHOD(float4, get_wzyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzyz() const property", asMETHOD(float4, get_wzyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzyw() const property", asMETHOD(float4, get_wzyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzzx() const property", asMETHOD(float4, get_wzzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzzy() const property", asMETHOD(float4, get_wzzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzzz() const property", asMETHOD(float4, get_wzzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzzw() const property", asMETHOD(float4, get_wzzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzwx() const property", asMETHOD(float4, get_wzwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzwy() const property", asMETHOD(float4, get_wzwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzwz() const property", asMETHOD(float4, get_wzwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wzww() const property", asMETHOD(float4, get_wzww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwxx() const property", asMETHOD(float4, get_wwxx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwxy() const property", asMETHOD(float4, get_wwxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwxz() const property", asMETHOD(float4, get_wwxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwxw() const property", asMETHOD(float4, get_wwxw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwyx() const property", asMETHOD(float4, get_wwyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwyy() const property", asMETHOD(float4, get_wwyy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwyz() const property", asMETHOD(float4, get_wwyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwyw() const property", asMETHOD(float4, get_wwyw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwzx() const property", asMETHOD(float4, get_wwzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwzy() const property", asMETHOD(float4, get_wwzy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwzz() const property", asMETHOD(float4, get_wwzz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwzw() const property", asMETHOD(float4, get_wwzw), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwwx() const property", asMETHOD(float4, get_wwwx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwwy() const property", asMETHOD(float4, get_wwwy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwwz() const property", asMETHOD(float4, get_wwwz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "float4 get_wwww() const property", asMETHOD(float4, get_wwww), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_wyzx(const float4 &in) property", asMETHOD(float4, set_wyzx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_wyxz(const float4 &in) property", asMETHOD(float4, set_wyxz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_wzyx(const float4 &in) property", asMETHOD(float4, set_wzyx), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_wzxy(const float4 &in) property", asMETHOD(float4, set_wzxy), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_wxyz(const float4 &in) property", asMETHOD(float4, set_wxyz), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("float4", "void   set_wxzy(const float4 &in) property", asMETHOD(float4, set_wxzy), asCALL_THISCALL); assert(r >= 0);

		r = engine->RegisterGlobalFunction("string toString(float4 v)", asFUNCTION(toStringF4), asCALL_CDECL); assert(r >= 0);
	}
}  // TFE_ForceScript
