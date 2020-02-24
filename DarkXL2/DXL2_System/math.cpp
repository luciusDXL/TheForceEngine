#pragma once

#include "math.h"

namespace DXL2_Math
{
	Mat3 computeViewMatrix(const Vec3f* lookDir, const Vec3f* upDir)
	{
		Vec3f rightDir = cross(lookDir, upDir);
		rightDir = normalize(&rightDir);

		Vec3f orthoUp = cross(&rightDir, lookDir);
		orthoUp = normalize(&orthoUp);

		return { rightDir, orthoUp, *lookDir };
	}

	Mat3 transpose(const Mat3& mtx)
	{
		Mat3 res;
		res.m0 = { mtx.m0.x, mtx.m1.x, mtx.m2.x };
		res.m1 = { mtx.m0.y, mtx.m1.y, mtx.m2.y };
		res.m2 = { mtx.m0.z, mtx.m1.z, mtx.m2.z };

		return res;
	}

	Mat4 computeProjMatrix(f32 fovInRadians, f32 aspectRatio, f32 zNear, f32 zFar)
	{
		// Build a projection matrix.
		const f32 yScale = 1.0f / tanf(fovInRadians * 0.5f);
		const f32 xScale = yScale / aspectRatio;
		const f32 zScale = zFar / (zNear - zFar);
		const f32 wScale = zNear * zFar / (zNear - zFar);

		Mat4 r;
		r.m0 = { xScale, 0.0f, 0.0f, 0.0f };
		r.m1 = { 0.0f, yScale, 0.0f, 0.0f };
		r.m2 = { 0.0f, 0.0f, zScale, wScale };
		r.m3 = { 0.0f, 0.0f, -1.0f, 0.0f };

		return r;
	}

	Mat4 computeInvProjMatrix(const Mat4& mtx)
	{
		const f32 xScale = mtx.m0.x;	// a
		const f32 yScale = mtx.m1.y;	// b
		const f32 zScale = mtx.m2.z;	// c
		const f32 wScale = mtx.m2.w;	// d
		const f32 w2 = mtx.m3.z;		// e

		Mat4 inv = { 0 };
		inv.m0.x = 1.0f / xScale;
		inv.m1.y = 1.0f / yScale;
		inv.m2.w = 1.0f / w2;
		inv.m3.z = 1.0f / wScale;
		inv.m3.w = -zScale / (wScale * w2);

		return inv;
	}
}
