#include "math.h"

namespace TFE_Math
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

	void buildRotationMatrix(Vec3f angles, Vec3f* mat)
	{
		if (angles.x == 0.0f && angles.y == 0.0f && angles.z == 0.0f)
		{
			// Identity.
			mat[0] = { 1.0f, 0.0f, 0.0f };
			mat[1] = { 0.0f, 1.0f, 0.0f };
			mat[2] = { 0.0f, 0.0f, 1.0f };
		}
		else if (angles.x == 0.0f && angles.z == 0.0f)
		{
			// Yaw only.
			const f32 ca = cosf(angles.y);
			const f32 sa = sinf(angles.y);
			mat[0] = { ca, 0.0f, sa };
			mat[1] = { 0.0f, 1.0f, 0.0f };
			mat[2] = { -sa, 0.0f, ca };
		}
		else
		{
			// Full orientation.
			const f32 cZ = cosf(angles.x);
			const f32 sZ = sinf(angles.x);
			const f32 cY = cosf(angles.y);
			const f32 sY = sinf(angles.y);
			const f32 cX = cosf(angles.z);
			const f32 sX = sinf(angles.z);

			mat[0] = { cZ * cY, cZ * sY * sX - sZ * cX, cZ * sY * cX + sZ * sX };
			mat[1] = { sZ * cY, sZ * sY * sX + cZ * cX, sZ * sY * cX - cZ * sX };
			mat[2] = {-sY,                cY * sX,                cY * cX };
		}
	}
}
